#include "gnome-screencast-config.h"
#include "wfd-media-factory.h"
#include "wfd-media.h"


typedef enum {
  ENCODER_OPENH264 = 0,
  ENCODER_X264,
} WfdH264Encoder;

struct _WfdMediaFactory
{
  GstRTSPMediaFactory parent_instance;

  WfdH264Encoder      encoder;
};

G_DEFINE_TYPE (WfdMediaFactory, wfd_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum {
  SIGNAL_CREATE_SOURCE,
  NR_SIGNALS
};

static guint signals[NR_SIGNALS];

GstElement *
wfd_media_factory_create_element (GstRTSPMediaFactory *factory, const GstRTSPUrl *url)
{
  WfdMediaFactory *self = WFD_MEDIA_FACTORY (factory);

  g_autoptr(GstBin) bin = NULL;
  g_autoptr(GstCaps) caps = NULL;
  GstElement *source = NULL;
  GstElement *scale;
  GstElement *sizefilter;
  GstElement *convert;
  GstElement *tee_interlace;
  GstElement *interlace;
  GstElement *selector_interlace;
  GstElement *queue_pre_encoder;
  GstElement *encoder;
  GstElement *parse;
  GstElement *codecfilter;
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
                "caps", g_steal_pointer (&caps),
                NULL);


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
                    "enable-frame-skip", FALSE,
                    /*"background-detection", FALSE,*/
                    /*"adaptive-quantization", FALSE,*/
                    /*"max-slice-size", 5000,*/
                    /*"complexity", 0,*/
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

  /* Repack the H264 stream */
  parse = gst_element_factory_make ("h264parse", "wfd-h264parse");
  success &= gst_bin_add (bin, parse);
  g_object_set (parse,
                "config-interval", (gint) - 1,
                NULL);

  caps = gst_caps_from_string ("video/x-h264,alignment=nal,stream-format=byte-stream");
  codecfilter = gst_element_factory_make ("capsfilter", "wfd-codecfilter");
  g_object_set (codecfilter,
                "caps", g_steal_pointer (&caps),
                NULL);
  success &= gst_bin_add (bin, codecfilter);

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
                                    parse,
                                    codecfilter,
                                    NULL);

  success &= gst_element_link_many (tee_interlace,
                                    interlace,
                                    selector_interlace,
                                    NULL);

  /* The WFD specification says we should use stream ID 0x1011. */
  success &= gst_element_link_pads (codecfilter, "src", mpegmux, "sink_4113");
  success &= gst_element_link_many (mpegmux,
                                    queue_pre_payloader,
                                    payloader,
                                    NULL);

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

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, pipeline_bus_watch_cb, NULL);

  return pipeline;
}

void
wfd_configure_media_element (GstBin *bin, WfdParams *params)
{
  g_autoptr(GstCaps) caps = NULL;
  g_autoptr(GstElement) sizefilter = NULL;
  g_autoptr(GstElement) encoder = NULL;
  g_autoptr(GstElement) selector_interlace = NULL;
  g_autoptr(GstPad) selector_interlace_pad = NULL;
  WfdVideoCodec *codec = params->selected_codec;
  WfdResolution *resolution = params->selected_resolution;
  WfdH264Encoder encoder_impl;
  guint gop_size = resolution->refresh_rate;

  /* Decrease the number of keyframes if the device is able to request
   * IDRs by itself. */
  if (params->idr_request_capability)
    gop_size = 10 * resolution->refresh_rate;

  caps = gst_caps_new_simple ("video/x-raw",
                              "framerate", GST_TYPE_FRACTION, resolution->refresh_rate, 1,
                              "width", G_TYPE_INT, resolution->width,
                              "height", G_TYPE_INT, resolution->height,
                              NULL);

  sizefilter = gst_bin_get_by_name (bin, "wfd-sizefilter");
  g_object_set (sizefilter,
                "caps", caps,
                NULL);

  encoder = gst_bin_get_by_name (bin, "wfd-encoder");
  encoder_impl = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (encoder), "wfd-encoder-impl"));
  switch (encoder_impl)
    {
    case ENCODER_OPENH264:
      /* We could set multi-thread/num-slices to codec->max_slice_num; but not sure
       * if that works realiably, and simply using one slice is on the safe side
       */
      g_object_set (encoder,
                    "enable-frame-skip", codec->frame_skipping_allowed,
                    "max-bitrate", wfd_video_codec_get_max_bitrate_kbit (codec) * 1024,
                    "bitrate", wfd_video_codec_get_max_bitrate_kbit (codec) * 1024,
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
                    "bitrate",  wfd_video_codec_get_max_bitrate_kbit (codec),
                    "max-rc-lookahead", 0,
                    "qos", TRUE,
                    NULL);
      break;

    default:
      g_assert_not_reached ();
    }

  /* Unlink the interlacer */
  selector_interlace = gst_bin_get_by_name (bin, "wfd-selector-interlace");
  selector_interlace_pad = gst_element_get_static_pad (selector_interlace, resolution->interlaced ? "sink_1" : "sink_0");
  g_object_set (selector_interlace,
                "active-pad", selector_interlace_pad,
                NULL);

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
}

static void
wfd_media_factory_init (WfdMediaFactory *self)
{
  GstRTSPMediaFactory *media_factory = GST_RTSP_MEDIA_FACTORY (self);

  g_autoptr(GstElementFactory) x264enc_factory = NULL;

  /* Default to openh264 and assume it is usable, prefer x264enc when available. */
  self->encoder = ENCODER_OPENH264;
  x264enc_factory = gst_element_factory_find ("x264enc");
  if (x264enc_factory)
    {
      g_debug ("Using x264enc for video encoding.");
      self->encoder = ENCODER_X264;
    }

  gst_rtsp_media_factory_set_media_gtype (media_factory, WFD_TYPE_MEDIA);
  gst_rtsp_media_factory_set_suspend_mode (media_factory, GST_RTSP_SUSPEND_MODE_RESET);
}
