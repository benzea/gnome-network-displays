#include "gnome-screencast-config.h"
#include "wfd-media-factory.h"
#include "wfd-media.h"


typedef enum {
  ENCODER_OPENH264 = 0,
  ENCODER_X264,
} WfdH264Encoder;

typedef enum {
  ENCODER_AAC_NONE = 0,
  ENCODER_AAC_FDK,
  ENCODER_AAC_AVENC,
  ENCODER_AAC_FAAC,
} WfdAACEncoder;

struct _WfdMediaFactory
{
  GstRTSPMediaFactory parent_instance;

  WfdH264Encoder      encoder;
  WfdAACEncoder       aac_encoder;
};

G_DEFINE_TYPE (WfdMediaFactory, wfd_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY)

typedef struct {
  GstSegment *segment;
} QOSData;

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum {
  SIGNAL_CREATE_SOURCE,
  SIGNAL_CREATE_AUDIO_SOURCE,
  NR_SIGNALS
};

static guint signals[NR_SIGNALS];

static void
encoding_perf_handoff_cb (GstElement *elem, GstBuffer *buf, gpointer user_data)
{
  QOSData *qos_data = user_data;
  g_autoptr(GstClock) clock = NULL;
  GstClockTime now;

  clock = gst_element_get_clock (elem);
  if (!clock)
    return;

  now = MAX (0, gst_clock_get_time (clock) - gst_element_get_base_time (elem));
  if (buf->pts != GST_CLOCK_TIME_NONE)
    {
      GstEvent *qos_event;
      gdouble proportion;
      GstClockTimeDiff pts;
      GstClockTimeDiff late;

      if (qos_data->segment)
        pts = gst_segment_to_running_time (qos_data->segment, GST_FORMAT_TIME, buf->pts);
      else
        pts = buf->pts;

      late = MAX (0, now - pts);

      /* Ignore the first few frames. */
      if (pts > 100 * GST_MSECOND)
        {
          /* We stop accepting things at more than 50ms delay;
           * Just use late / 50ms for the long term proportion. */
          proportion = late / (gdouble) (50 * GST_MSECOND);

          /* g_debug ("Sending QOS event with proportion %.2f", proportion); */
          qos_event = gst_event_new_qos (GST_QOS_TYPE_UNDERFLOW,
                                         proportion,
                                         late - 50 * GST_MSECOND,
                                         pts);

          gst_element_send_event (elem, qos_event);
        }
    }
}

GstPadProbeReturn
encoding_perf_probe_cb (GstPad *pad,
                        GstPadProbeInfo *info,
                        gpointer user_data)
{
  const GstSegment *segment;
  QOSData *qos_data = user_data;
  GstEvent *event;

  if (info->type != GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
    return GST_PAD_PROBE_OK;

  event = gst_pad_probe_info_get_event (info);

  if (GST_EVENT_TYPE (event) != GST_EVENT_SEGMENT)
    return;

  gst_event_parse_segment (event, &segment);

  g_clear_pointer (&qos_data->segment, gst_segment_free);
  qos_data->segment = gst_segment_copy (segment);

  return GST_PAD_PROBE_OK;
}

static void
free_qos_data (QOSData *qos_data)
{
  g_clear_pointer (&qos_data->segment, gst_segment_free);
  g_free (qos_data);
}

GstElement *
wfd_media_factory_create_element (GstRTSPMediaFactory *factory, const GstRTSPUrl *url)
{
  WfdMediaFactory *self = WFD_MEDIA_FACTORY (factory);
  QOSData *qos_data;
  g_autoptr(GstBin) bin = NULL;
  g_autoptr(GstCaps) caps = NULL;
  g_autoptr(GstBin) audio_pipeline = NULL;
  g_autoptr(GstPad) encoding_perf_sink = NULL;
  GstElement *source = NULL;
  GstElement *audio_source = NULL;
  GstElement *scale;
  GstElement *sizefilter;
  GstElement *convert;
  GstElement *tee_interlace;
  GstElement *interlace;
  GstElement *selector_interlace;
  GstElement *queue_pre_encoder;
  GstElement *encoder;
  GstElement *encoding_perf;
  GstElement *parse;
  GstElement *codecfilter;
  GstElement *queue_mpegmux_video;
  GstElement *mpegmux;
  GstElement *queue_pre_payloader;
  GstElement *payloader;
  gboolean success = TRUE;

  bin = GST_BIN (gst_bin_new ("wfd-encoder-bin"));

  /* Test input, will be replaced by real source */
  g_signal_emit (self, signals[SIGNAL_CREATE_SOURCE], 0, &source);
  g_assert (source);
  success &= gst_bin_add (bin, source);

  scale = gst_element_factory_make ("videoscale", "wfd-scale");
  success &= gst_bin_add (bin, scale);

  caps = gst_caps_new_simple ("video/x-raw",
                              "framerate", GST_TYPE_FRACTION, 30, 1,
                              "width", G_TYPE_INT, 1920,
                              "height", G_TYPE_INT, 1080,
                              NULL);
  sizefilter = gst_element_factory_make ("capsfilter", "wfd-sizefilter");
  success &= gst_bin_add (bin, sizefilter);
  g_object_set (sizefilter,
                "caps", caps,
                NULL);
  g_clear_pointer (&caps, gst_mini_object_unref);

  convert = gst_element_factory_make ("videoconvert", "wfd-videoconvert");
  success &= gst_bin_add (bin, convert);

  tee_interlace = gst_element_factory_make ("tee", "wfd-tee-interlace");
  success &= gst_bin_add (bin, tee_interlace);

  interlace = gst_element_factory_make ("interlace", "wfd-interlace");
  success &= gst_bin_add (bin, interlace);
  /* 1:1 is 0, so convert 60 -> 60i */
  g_object_set (interlace,
                "field-pattern", 0,
                NULL);

  selector_interlace = gst_element_factory_make ("input-selector", "wfd-selector-interlace");
  /* No need to sync the streams. */
  g_object_set (selector_interlace,
                "sync-streams", FALSE,
                NULL);
  success &= gst_bin_add (bin, selector_interlace);

  queue_pre_encoder = gst_element_factory_make ("queue", "wfd-pre-encoder-queue");
  g_object_set (queue_pre_encoder,
                "max-size-buffers", (guint) 1,
                "leaky", 0,
                NULL);
  success &= gst_bin_add (bin, queue_pre_encoder);

  switch (self->encoder)
    {
    case ENCODER_OPENH264:
      encoder = gst_element_factory_make ("openh264enc", "wfd-encoder");
      success &= gst_bin_add (bin, encoder);
      g_object_set (encoder,
                    "qos", TRUE,
                    "multi-thread", 1,
                    "usage-type", 1, /* screen */
                    "slice-mode", 1, /* n-slices */
                    "num-slices", 1,
                    "rate-control", 1, /* bitrate */
                    "gop-size", 30,
                    /* If frame skipping is too aggressive, then audio will
                     * drop out. So don't enable it. */
                    "enable-frame-skip", FALSE,
                    "scene-change-detection", TRUE,
                    "background-detection", TRUE,
                    /*"adaptive-quantization", FALSE,*/
                    /*"max-slice-size", 5000,*/
                    "complexity", 0,
                    /*"deblocking", 2,*/
                    NULL);

      /* Maybe try:
       *  - rate-control: 2, buffer*/
      break;

    case ENCODER_X264:
      encoder = gst_element_factory_make ("x264enc", "wfd-encoder");
      success &= gst_bin_add (bin, encoder);

      gst_preset_load_preset (GST_PRESET (encoder), "Profile High");
      break;

    default:
      g_assert_not_reached ();
    }
  g_object_set_data (G_OBJECT (encoder), "wfd-encoder-impl", GINT_TO_POINTER (self->encoder));

  encoding_perf = gst_element_factory_make ("identity", "wfd-measure-encoder-realtime");
  success &= gst_bin_add (bin, encoding_perf);
  qos_data = g_new0 (QOSData, 1);
  g_object_set_data_full (G_OBJECT (encoding_perf), "wfd-qos-data", qos_data, (GDestroyNotify) free_qos_data);
  g_signal_connect (encoding_perf, "handoff", G_CALLBACK (encoding_perf_handoff_cb), qos_data);
  encoding_perf_sink = gst_element_get_static_pad (encoding_perf, "sink");
  gst_pad_add_probe (encoding_perf_sink,
                     GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                     encoding_perf_probe_cb,
                     qos_data,
                     NULL);

  /* Repack the H264 stream */
  parse = gst_element_factory_make ("h264parse", "wfd-h264parse");
  success &= gst_bin_add (bin, parse);
  g_object_set (parse,
                "config-interval", (gint) - 1,
                NULL);

  caps = gst_caps_from_string ("video/x-h264,alignment=nal,stream-format=byte-stream,profile=baseline");
  codecfilter = gst_element_factory_make ("capsfilter", "wfd-codecfilter");
  g_object_set (codecfilter,
                "caps", caps,
                NULL);
  g_clear_pointer (&caps, gst_mini_object_unref);
  success &= gst_bin_add (bin, codecfilter);

  queue_mpegmux_video = gst_element_factory_make ("queue", "wfd-mpegmux-video-queue");
  success &= gst_bin_add (bin, queue_mpegmux_video);
  g_object_set (queue_mpegmux_video,
                "max-size-buffers", (guint) 1000,
                "max-size-time", 500 * GST_MSECOND,
                NULL);

  mpegmux = gst_element_factory_make ("mpegtsmux", "wfd-mpegtsmux");
  success &= gst_bin_add (bin, mpegmux);
  g_object_set (mpegmux,
                "alignment", (gint) 7, /* Force the correct alignment for UDP */
                NULL);


  queue_pre_payloader = gst_element_factory_make ("queue", "wfd-pre-payloader-queue");
  success &= gst_bin_add (bin, queue_pre_payloader);
  g_object_set (queue_pre_payloader,
                "max-size-buffers", (guint) 1,
                "leaky", 0,
                NULL);

  payloader = gst_element_factory_make ("rtpmp2tpay", "pay0");
  success &= gst_bin_add (bin, payloader);
  g_object_set (payloader,
                "ssrc", 1,
                /* Perfect is in relation to the input buffers, but we want the
                 * proper clock from when the packet was sent. */
                "perfect-rtptime", FALSE,
                "timestamp-offset", (guint) 0,
                "seqnum-offset", (gint) 0,
                NULL);

  success &= gst_element_link_many (source,
                                    scale,
                                    sizefilter,
                                    convert,
                                    tee_interlace,
                                    selector_interlace,
                                    queue_pre_encoder,
                                    encoder,
                                    encoding_perf,
                                    parse,
                                    codecfilter,
                                    queue_mpegmux_video,
                                    NULL);

  success &= gst_element_link_many (tee_interlace,
                                    interlace,
                                    selector_interlace,
                                    NULL);

  /* The WFD specification says we should use stream ID 0x1011. */
  success &= gst_element_link_pads (queue_mpegmux_video, "src", mpegmux, "sink_4113");
  success &= gst_element_link_many (mpegmux,
                                    queue_pre_payloader,
                                    payloader,
                                    NULL);


  /* Add audio elements */
  if (self->aac_encoder != ENCODER_AAC_NONE)
    g_signal_emit (self, signals[SIGNAL_CREATE_AUDIO_SOURCE], 0, &audio_source);

  if (audio_source)
    {
      GstElement *audioencoder;
      GstElement *audioresample;
      GstElement *audioconvert;
      GstElement *queue_mpegmux_audio;

      audio_pipeline = GST_BIN (gst_bin_new ("wfd-audio"));
      success &= gst_bin_add (bin, GST_ELEMENT (g_object_ref (audio_pipeline)));
      /* The audio pipeline is disabled by default, we hook it up and
       * enable it during configuration. */
      gst_element_set_locked_state (GST_ELEMENT (audio_pipeline), TRUE);

      success &= gst_bin_add (audio_pipeline, audio_source);

      audioresample = gst_element_factory_make ("audioresample", "wfd-audio-resample");
      success &= gst_bin_add (audio_pipeline, audioresample);

      audioconvert = gst_element_factory_make ("audioconvert", "wfd-audio-convert");
      success &= gst_bin_add (audio_pipeline, audioconvert);

      switch (self->aac_encoder)
        {
        case ENCODER_AAC_FDK:
          audioencoder = gst_element_factory_make ("fdkaacenc", "wfd-audio-aac-enc");
          break;

        case ENCODER_AAC_FAAC:
          audioencoder = gst_element_factory_make ("faac", "wfd-audio-aac-enc");
          break;

        case ENCODER_AAC_AVENC:
          audioencoder = gst_element_factory_make ("avenc_aac", "wfd-audio-aac-enc");
          break;

        default:
          g_assert_not_reached ();
        }
      success &= gst_bin_add (audio_pipeline, audioencoder);

      queue_mpegmux_audio = gst_element_factory_make ("queue", "wfd-mpegmux-audio-queue");
      g_object_set (queue_mpegmux_audio,
                    "max-size-buffers", (guint) 100000,
                    "max-size-time", 500 * GST_MSECOND,
                    "leaky", 0,
                    NULL);
      success &= gst_bin_add (audio_pipeline, queue_mpegmux_audio);

      caps = gst_caps_new_simple ("audio/mpeg",
                                  "channels", G_TYPE_INT, 2,
                                  "rate", G_TYPE_INT, 48000,
                                  NULL);

      success &= gst_element_link_many (audio_source, audioresample, audioconvert, NULL);
      success &= gst_element_link (audioconvert, audioencoder);
      success &= gst_element_link_filtered (audioencoder, queue_mpegmux_audio, caps);
      g_clear_pointer (&caps, gst_mini_object_unref);

      gst_element_add_pad (GST_ELEMENT (audio_pipeline),
                           gst_ghost_pad_new ("src",
                                              gst_element_get_static_pad (queue_mpegmux_audio,
                                                                          "src")));
    }

  GST_DEBUG_BIN_TO_DOT_FILE (bin,
                             GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE,
                             "wfd-encoder-bin");
  if (!success)
    {
      g_error ("WfdMediaFactory: Error creating encoding pipeline. If gstreamer is compiled with debugging and GST_DEBUG_DUMP_DOT_DIR is set, then the pipeline will have been dumped.");
      g_clear_object (&bin);
    }

  return (GstElement *) g_steal_pointer (&bin);
}

static gboolean
pipeline_bus_watch_cb (GstBus     *bus,
                       GstMessage *message,
                       gpointer    data)
{
  switch (GST_MESSAGE_TYPE (message))
    {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;

        gst_message_parse_error (message, &err, &debug);
        g_print ("Error from %s: %s\n", GST_MESSAGE_SRC_NAME (message), err->message);
        g_error_free (err);
        g_free (debug);

        break;
      }

    case GST_MESSAGE_WARNING: {
        GError *err;
        gchar *debug;

        gst_message_parse_warning (message, &err, &debug);
        g_print ("Warning from %s: %s\n", GST_MESSAGE_SRC_NAME (message), err->message);
        g_error_free (err);
        g_free (debug);

        break;
      }

    case GST_MESSAGE_QOS: {
        gboolean live;
        guint64 running_time, stream_time, timestamp, duration;
        guint64 processed, dropped;
        gint64 jitter;
        gdouble proportion;
        gint quality;

        gst_message_parse_qos (message, &live, &running_time, &stream_time, &timestamp, &duration);
        gst_message_parse_qos_stats (message, NULL, &processed, &dropped);
        gst_message_parse_qos_values (message, &jitter, &proportion, &quality);

        g_debug ("QOS: proportion: %.3f, processed: %" G_GUINT64_FORMAT ", dropped: %" G_GUINT64_FORMAT "",
                 proportion, processed, dropped);
        break;
      }

    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_debug ("reached EOS");
      break;

    default:
      /* unhandled message */
      break;
    }

  return TRUE;
}

static GstElement *
wfd_media_factory_create_pipeline (GstRTSPMediaFactory *factory, GstRTSPMedia *media)
{
  GstElement *pipeline;

  g_autoptr(GstBus) bus = NULL;

  pipeline = GST_RTSP_MEDIA_FACTORY_CLASS (wfd_media_factory_parent_class)->create_pipeline (factory, media);

  /* We need a high latency for the openh264 encoder at least when the
   * usage-type is set to "screen". After e.g. scene changes the latency will
   * be very high for short periods of time, and this prevents further issues. */
  gst_pipeline_set_latency (GST_PIPELINE (pipeline), 500 * GST_MSECOND);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, pipeline_bus_watch_cb, NULL);

  return pipeline;
}

void
wfd_configure_media_element (GstBin *bin, WfdParams *params)
{
  g_autoptr(GstCaps) caps_sizefilter = NULL;
  g_autoptr(GstElement) sizefilter = NULL;
  g_autoptr(GstCaps) caps_codecfilter = NULL;
  g_autoptr(GstElement) codecfilter = NULL;
  g_autoptr(GstElement) encoder = NULL;
  g_autoptr(GstElement) selector_interlace = NULL;
  g_autoptr(GstPad) selector_interlace_pad = NULL;
  g_autoptr(GstElement) audio_pipeline = NULL;
  g_autoptr(GstElement) mpegmux = NULL;
  WfdVideoCodec *codec = params->selected_codec;
  WfdResolution *resolution = params->selected_resolution;
  WfdH264Encoder encoder_impl;
  WfdH264ProfileFlags profile;
  guint gop_size = resolution->refresh_rate;
  guint bitrate_kbit = wfd_video_codec_get_max_bitrate_kbit (codec);

  /* Limit initial video bitrate to 512kBit/s to ensure we don't
   * saturate the wifi link.
   * This is a rather bad method, but it kind of works. */
  bitrate_kbit = MIN (bitrate_kbit, 512 * 8);

  /* Decrease the number of keyframes if the device is able to request
   * IDRs by itself. */
  if (params->idr_request_capability)
    gop_size = 10 * resolution->refresh_rate;

  caps_sizefilter = gst_caps_new_simple ("video/x-raw",
                                         "framerate", GST_TYPE_FRACTION, resolution->refresh_rate, 1,
                                         "width", G_TYPE_INT, resolution->width,
                                         "height", G_TYPE_INT, resolution->height,
                                         NULL);

  sizefilter = gst_bin_get_by_name (bin, "wfd-sizefilter");
  g_object_set (sizefilter,
                "caps", caps_sizefilter,
                NULL);

  encoder = gst_bin_get_by_name (bin, "wfd-encoder");
  encoder_impl = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (encoder), "wfd-encoder-impl"));
  switch (encoder_impl)
    {
    case ENCODER_OPENH264:
      /* We could set multi-thread/num-slices to codec->max_slice_num; but not sure
       * if that works realiably, and simply using one slice is on the safe side
       */
      profile = WFD_H264_PROFILE_BASE;
      g_object_set (encoder,
                    "max-bitrate", (guint) bitrate_kbit * 1024,
                    "bitrate", (guint) bitrate_kbit * 1024,
                    "gop-size", gop_size,
                    NULL);
      break;

    case ENCODER_X264:
      if (codec->profile == WFD_H264_PROFILE_HIGH)
        gst_preset_load_preset (GST_PRESET (encoder), "Profile High");
      else
        gst_preset_load_preset (GST_PRESET (encoder), "Profile Base");

      g_object_set (encoder,
                    /* "pass", "cbr", */
                    "b-adapt", FALSE,
                    "bframes", 0,
                    "key-int-max", gop_size,
                    "interlaced", resolution->interlaced,
                    "bitrate",  bitrate_kbit,
                    "max-rc-lookahead", 0,
                    "qos", TRUE,
                    NULL);
      break;

    default:
      g_assert_not_reached ();
    }

  if (profile == WFD_H264_PROFILE_HIGH)
    caps_codecfilter = gst_caps_from_string ("video/x-h264,alignment=nal,stream-format=byte-stream,profile=high");
  else
    caps_codecfilter = gst_caps_from_string ("video/x-h264,alignment=nal,stream-format=byte-stream,profile=baseline");

  codecfilter = gst_bin_get_by_name (bin, "wfd-codecfilter");
  g_object_set (codecfilter,
                "caps", caps_codecfilter,
                NULL);


  /* Unlink the interlacer */
  selector_interlace = gst_bin_get_by_name (bin, "wfd-selector-interlace");
  selector_interlace_pad = gst_element_get_static_pad (selector_interlace, resolution->interlaced ? "sink_1" : "sink_0");
  g_object_set (selector_interlace,
                "active-pad", selector_interlace_pad,
                NULL);

  g_debug ("An audiocodec has been selected: %s", params->selected_audio_codec ? "yes" : "no");
  audio_pipeline = gst_bin_get_by_name (bin, "wfd-audio");
  mpegmux = gst_bin_get_by_name (bin, "wfd-mpegtsmux");
  if (audio_pipeline)
    {
      gst_element_unlink (audio_pipeline, mpegmux);

      if (params->selected_audio_codec)
        {
          /* We currently only handle AAC with 2 channels and 48kHz */
          g_assert (params->selected_audio_codec->type == WFD_AUDIO_AAC);
          g_assert (params->selected_audio_codec->modes == 0x1);

          gst_element_set_locked_state (GST_ELEMENT (audio_pipeline), FALSE);

          /* Hook up the audio channel */
          gst_element_link_pads (audio_pipeline, "src", mpegmux, "sink_4352");
        }
      else
        {
          gst_element_set_locked_state (GST_ELEMENT (audio_pipeline), TRUE);
          gst_element_set_state (GST_ELEMENT (audio_pipeline), GST_STATE_NULL);
        }
    }

  GST_DEBUG_BIN_TO_DOT_FILE (bin,
                             GST_DEBUG_GRAPH_SHOW_ALL,
                             "wfd-encoder-bin-configured");
}

GstRTSPMedia *
wfd_media_factory_construct (GstRTSPMediaFactory *factory, const GstRTSPUrl *url)
{
  GstRTSPContext *ctx;
  GstRTSPMedia *res;
  GstRTSPClient *client;
  GstRTSPStream *stream;

  res = GST_RTSP_MEDIA_FACTORY_CLASS (wfd_media_factory_parent_class)->construct (factory, url);

  /* XXX: Can this be done in a better location?*/
  ctx = gst_rtsp_context_get_current ();
  client = ctx->client;

  stream = gst_rtsp_media_get_stream (res, 0);
  gst_rtsp_stream_set_control (stream, "streamid=0");
  g_debug ("WfdMedia init: Got %d streams", gst_rtsp_media_n_streams (res));

  return res;
}

WfdMediaFactory *
wfd_media_factory_new (void)
{
  return g_object_new (WFD_TYPE_MEDIA_FACTORY, NULL);
}

static void
wfd_media_factory_finalize (GObject *object)
{
  WfdMediaFactory *self = (WfdMediaFactory *) object;

  G_OBJECT_CLASS (wfd_media_factory_parent_class)->finalize (object);
}

static void
wfd_media_factory_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  WfdMediaFactory *self = WFD_MEDIA_FACTORY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
wfd_media_factory_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  WfdMediaFactory *self = WFD_MEDIA_FACTORY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
wfd_media_factory_class_init (WfdMediaFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstRTSPMediaFactoryClass *media_factory_class = GST_RTSP_MEDIA_FACTORY_CLASS (klass);

  object_class->finalize = wfd_media_factory_finalize;
  object_class->get_property = wfd_media_factory_get_property;
  object_class->set_property = wfd_media_factory_set_property;

  media_factory_class->create_element = wfd_media_factory_create_element;
  media_factory_class->create_pipeline = wfd_media_factory_create_pipeline;
  media_factory_class->construct = wfd_media_factory_construct;

  signals[SIGNAL_CREATE_SOURCE] =
    g_signal_new ("create-source", WFD_TYPE_MEDIA_FACTORY, G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL,
                  NULL,
                  GST_TYPE_ELEMENT, 0);

  signals[SIGNAL_CREATE_AUDIO_SOURCE] =
    g_signal_new ("create-audio-source", WFD_TYPE_MEDIA_FACTORY, G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL,
                  NULL,
                  GST_TYPE_ELEMENT, 0);
}

static void
wfd_media_factory_init (WfdMediaFactory *self)
{
  GstRTSPMediaFactory *media_factory = GST_RTSP_MEDIA_FACTORY (self);

  g_autoptr(GstElementFactory) x264enc_factory = NULL;
  g_autoptr(GstElementFactory) aac_factory = NULL;

  /* Default to openh264 and assume it is usable, prefer x264enc when available. */
  self->encoder = ENCODER_OPENH264;
  x264enc_factory = gst_element_factory_find ("x264enc");
  if (x264enc_factory)
    {
      g_debug ("Using x264enc for video encoding.");
      self->encoder = ENCODER_X264;
    }

  /* Default to openh264 and assume it is usable, prefer x264enc when available. */
  self->aac_encoder = ENCODER_AAC_NONE;
  aac_factory = gst_element_factory_find ("fdkaac");
  if (aac_factory)
    {
      self->aac_encoder = ENCODER_AAC_FDK;
      g_clear_object (&aac_factory);
    }

  aac_factory = gst_element_factory_find ("faac");
  if (aac_factory)
    {
      self->aac_encoder = ENCODER_AAC_FAAC;
      g_clear_object (&aac_factory);
    }

  aac_factory = gst_element_factory_find ("avenc_aac");
  if (aac_factory)
    {
      self->aac_encoder = ENCODER_AAC_AVENC;
      g_clear_object (&aac_factory);
    }

  gst_rtsp_media_factory_set_media_gtype (media_factory, WFD_TYPE_MEDIA);
  gst_rtsp_media_factory_set_suspend_mode (media_factory, GST_RTSP_SUSPEND_MODE_RESET);
  gst_rtsp_media_factory_set_buffer_size (media_factory, 65536);
}
