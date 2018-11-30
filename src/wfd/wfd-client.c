#include <glib-object.h>
#include <gst/rtsp/gstrtspmessage.h>
#include "wfd-client.h"
#include "wfd-media-factory.h"
#include "wfd-media.h"
#include "wfd-params.h"

typedef enum {
  INIT_STATE_M0_INVALID = 0,
  INIT_STATE_M1_SOURCE_QUERY_OPTIONS = 1,
  INIT_STATE_M2_SINK_QUERY_OPTIONS = 2,
  INIT_STATE_M3_SOURCE_GET_PARAMS = 3,
  INIT_STATE_M4_SOURCE_SET_PARAMS = 4,
  INIT_STATE_M5_SOURCE_TRIGGER_SETUP = 5,

  INIT_STATE_DONE = 9999,
} WfdClientInitState;

struct _WfdClient
{
  GstRTSPClient      parent_instance;

  guint              keep_alive_source_id;
  guint              idle_source_id;

  WfdClientInitState init_state;
  WfdParams         *params;

  WfdMedia          *media;
};

G_DEFINE_TYPE (WfdClient, wfd_client, GST_TYPE_RTSP_CLIENT)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static const gchar * supported_rtsp_features[] = {
  "org.wfa.wfd1.0",
  "OPTIONS",
  "DESCRIBE",
  "GET_PARAMETER",
  "PAUSE",
  "PLAY",
  "SETUP",
  "SET_PARAMETER",
  "TEARDOWN",
  NULL
};

void
idle_handler_destroy (gpointer user_data)
{
  WfdClient *self = WFD_CLIENT (user_data);

  self->idle_source_id = 0;
}

static void
register_idle_handler (WfdClient *self, GSourceFunc callback)
{
  if (self->idle_source_id)
    {
      g_warning ("WfdClient: Already have one idle handler scheduled, removing it");
      g_source_remove (self->idle_source_id);
    }
  self->idle_source_id = g_idle_add_full (G_PRIORITY_DEFAULT,
                                            callback,
                                            self,
                                            idle_handler_destroy);
}

WfdClient *
wfd_client_new (void)
{
  return g_object_new (WFD_TYPE_CLIENT, NULL);
}

static void
wfd_client_finalize (GObject *object)
{
  WfdClient *self = (WfdClient *) object;

  g_clear_pointer (&self->params, wfd_params_free);

  if (self->keep_alive_source_id)
    g_source_remove (self->keep_alive_source_id);
  self->keep_alive_source_id = 0;

  if (self->idle_source_id)
    g_source_remove (self->idle_source_id);
  self->idle_source_id = 0;

  G_OBJECT_CLASS (wfd_client_parent_class)->finalize (object);
}

static void
wfd_client_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  WfdClient *self = WFD_CLIENT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
wfd_client_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  WfdClient *self = WFD_CLIENT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

gchar *
wfd_client_check_requirements (GstRTSPClient *client, GstRTSPContext *ctx, gchar ** arr)
{
  g_autoptr(GPtrArray) unsupported = NULL;
  gchar **req;
  char *res = NULL;

  for (req = arr; *req; req++)
    {
      if (!g_strv_contains (supported_rtsp_features, *req))
        {
          if (unsupported == NULL)
            unsupported = g_ptr_array_new ();
          g_ptr_array_add (unsupported, *req);
        }
    }

  if (!unsupported)
    return g_strdup ("");

  res = g_strjoinv (", ", (GStrv) unsupported->pdata);
  g_warning ("WfdClient: Cannot support the following requested features: %s", res);

  return res;
}

gint
compare_resolutions (gconstpointer a, gconstpointer b)
{
  WfdResolution *res_a = (WfdResolution *) a;
  WfdResolution *res_b = (WfdResolution *) b;
  gint a_weight;
  gint b_weight;

  a_weight = res_a->width * res_a->height * 100 + res_a->refresh_rate / (res_a->interlaced ? 2 : 1) - res_a->interlaced;
  b_weight = res_b->width * res_b->height * 100 + res_b->refresh_rate / (res_b->interlaced ? 2 : 1) - res_b->interlaced;
  return a_weight - b_weight;
}

void
wfd_client_select_codec_and_resolution (WfdClient *self, WfdH264ProfileFlags profile)
{
  gint i;
  WfdVideoCodec *codec = NULL;

  for (i = 0; i < self->params->video_codecs->len; i++)
    {
      WfdVideoCodec *item = g_ptr_array_index (self->params->video_codecs, i);

      /* Use the first codec we can find. */
      if (!codec)
        codec = item;

      if (codec->profile != item->profile && item->profile == profile)
        codec = item;

      if (codec->profile == item->profile && item->level > codec->level)
        codec = item;
    }

  self->params->selected_codec = wfd_video_codec_ref (codec);

  if (codec->native)
    {
      self->params->selected_resolution = wfd_resolution_copy (codec->native);
    }
  else
    {
      /* Find a good resolution. */
      g_autoptr(GList) resolutions = NULL;
      GList *last;

      resolutions = wfd_video_codec_get_resolutions (codec);
      resolutions = g_list_sort (resolutions, compare_resolutions);
      last = g_list_last (resolutions);
      if (last)
        {
          self->params->selected_resolution = wfd_resolution_copy ((WfdResolution *) last->data);
        }
      else
        {
          /* Create a standard full HD resolution if everything fails. */
          g_warning ("WfdClient: No resolution found, falling back to standard FullHD resolution.");
          self->params->selected_resolution = wfd_resolution_new ();
          self->params->selected_resolution->width = 1920;
          self->params->selected_resolution->height = 1080;
          self->params->selected_resolution->refresh_rate = 30;
          self->params->selected_resolution->interlaced = FALSE;
        }
    }
}

gboolean
wfd_client_configure_client_media (GstRTSPClient * client,
                                   GstRTSPMedia * media, GstRTSPStream * stream,
                                   GstRTSPContext * ctx)
{
  WfdClient *self = WFD_CLIENT (client);
  g_autoptr(GstElement) element;
  gboolean res;

  g_return_val_if_fail (self->params->selected_codec, FALSE);
  g_return_val_if_fail (self->params->selected_resolution, FALSE);

  self->media = WFD_MEDIA (media);

  element = gst_rtsp_media_get_element (media);
  wfd_configure_media_element (GST_BIN (element), self->params->selected_codec, self->params->selected_resolution);

  res = GST_RTSP_CLIENT_CLASS (wfd_client_parent_class)->configure_client_media (client, media, stream, ctx);

  gst_rtsp_media_prepare (media, gst_rtsp_thread_pool_get_thread (gst_rtsp_client_get_thread_pool (client), GST_RTSP_THREAD_TYPE_MEDIA, ctx));

  return res;
}

static gboolean
wfd_client_trigger_setup_idle (gpointer user_data)
{
  wfd_client_trigger_method (WFD_CLIENT (user_data), "SETUP");

  return G_SOURCE_REMOVE;
}

static gchar *
wfd_client_get_presentation_uri (WfdClient *self)
{
  GstRTSPClient *client = GST_RTSP_CLIENT (self);
  GstRTSPConnection *connection;
  GSocket *socket;

  g_autoptr(GSocketAddress) sock_addr;
  GInetAddress *inet_addr;
  g_autofree gchar *addr = NULL;
  gint port;

  connection = gst_rtsp_client_get_connection (client);
  socket = gst_rtsp_connection_get_read_socket (connection);
  sock_addr = g_socket_get_local_address (socket, NULL);
  inet_addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (sock_addr));
  addr = g_inet_address_to_string (inet_addr);
  port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (sock_addr));

  return g_strdup_printf ("rtsp://%s:%d/wfd1.0/streamid=0", addr, port);
}

static void
wfd_client_set_params (WfdClient *self)
{
  GstRTSPMessage msg = { 0 };
  g_autofree gchar * body = NULL;
  g_autofree gchar * presentation_uri = NULL;
  g_autofree gchar * resolution_descr = NULL;

  self->init_state = INIT_STATE_M4_SOURCE_SET_PARAMS;

  gst_rtsp_message_init_request (&msg, GST_RTSP_SET_PARAMETER, "rtsp://localhost/wfd1.0");

  presentation_uri = wfd_client_get_presentation_uri (self);
  resolution_descr = wfd_video_codec_get_descriptor_for_resolution (self->params->selected_codec, self->params->selected_resolution);

  body = g_strdup_printf (
    "wfd_video_formats: %s\r\n"
    "wfd_audio_codecs: none\r\n"
    "wfd_presentation_URL: %s none\r\n"
    "wfd_client_rtp_ports: RTP/AVP/UDP;unicast %u %u mode=play\r\n",
    resolution_descr,
    presentation_uri,
    self->params->primary_rtp_port, self->params->secondary_rtp_port);

  gst_rtsp_message_add_header_by_name (&msg, "Content-Type", "text/parameters");
  gst_rtsp_message_set_body (&msg, (guint8 *) body, strlen (body));

  gst_rtsp_client_send_message (GST_RTSP_CLIENT (self), NULL, &msg);

  gst_rtsp_message_unset (&msg);
}

static gboolean
wfd_client_idle_set_params (gpointer user_data)
{
  wfd_client_set_params (WFD_CLIENT (user_data));

  return G_SOURCE_REMOVE;
}

void
wfd_client_handle_response (GstRTSPClient * client, GstRTSPContext *ctx)
{
  WfdClient *self = WFD_CLIENT (client);

  /* Track the initialization process and possibly trigger the
   * next state of the connection establishment. */
  switch (self->init_state)
    {
    case INIT_STATE_M1_SOURCE_QUERY_OPTIONS:
      g_debug ("WfdClient: OPTIONS querying done");
      /* The standard says to disconnect. However, if we do this,
       * then it is possible to connect a normal RTSP client for testing.
       * e.g. VLC will play back the stream correctly. */
      self->init_state = INIT_STATE_M2_SINK_QUERY_OPTIONS;
      break;

    case INIT_STATE_M3_SOURCE_GET_PARAMS:
      g_debug ("WfdClient: GET_PARAMS done");
      wfd_params_from_sink (self->params, ctx->response->body, ctx->response->body_size);

      /* XXX: Pick the better profile if we have an encoder that supports it! */
      wfd_client_select_codec_and_resolution (self, WFD_H264_PROFILE_BASE);

      register_idle_handler (self, wfd_client_idle_set_params);
      break;

    case INIT_STATE_M4_SOURCE_SET_PARAMS:
      g_debug ("WfdClient: SET_PARAMS done");
      register_idle_handler (self, wfd_client_trigger_setup_idle);
      break;

    case INIT_STATE_M5_SOURCE_TRIGGER_SETUP:
      self->init_state = INIT_STATE_DONE;
      g_debug ("WfdClient: Initialization done!");
      break;

    default:
      /* Nothing to be done in the other states. */
      break;
    }
}

static gchar *
wfd_client_make_path_from_uri (GstRTSPClient * client, const GstRTSPUrl * uri)
{
  GstRTSPContext *ctx = gst_rtsp_context_get_current ();

  /* Strip /streamid=0.
   * This is a bad hack, because gstreamer does not support playing/pausing
   * a specific stream. We can do so safely because we only have one stream.
   */
  if (ctx->request &&
      (ctx->request->type != GST_RTSP_PLAY ||
       ctx->request->type != GST_RTSP_PAUSE))
    {
      if (g_str_has_suffix (uri->abspath, "/streamid=0"))
        return g_strndup (uri->abspath, strlen (uri->abspath) - 11);
      else
        return g_strdup (uri->abspath);
    }
  else
    {
      return GST_RTSP_CLIENT_CLASS (wfd_client_parent_class)->make_path_from_uri (client, uri);
    }
}

GstRTSPFilterResult
wfd_client_timeout_session_filter_func (GstRTSPClient  *client,
                                        GstRTSPSession *sess,
                                        gpointer        user_data)
{
  GstRTSPMessage msg = { 0 };

  g_debug ("WfdClient: Doing keep-alive");

  gst_rtsp_message_init_request (&msg, GST_RTSP_GET_PARAMETER, "rtsp://localhost/wfd1.0/streamid=0");

  gst_rtsp_client_send_message (client, sess, &msg);

  gst_rtsp_message_unset (&msg);

  return GST_RTSP_FILTER_KEEP;
}

static gboolean
wfd_client_keep_alive_timeout (gpointer user_data)
{
  GstRTSPClient *client = GST_RTSP_CLIENT (user_data);

  gst_rtsp_client_session_filter (client, wfd_client_timeout_session_filter_func, NULL);

  return G_SOURCE_CONTINUE;
}

static void
wfd_client_new_session (GstRTSPClient *client, GstRTSPSession *session)
{
  WfdClient *self = WFD_CLIENT (client);

  /* The WFD standard suggests a timeout of 30 seconds */
  gst_rtsp_session_set_timeout (session, 30);
  g_object_set (session, "timeout-always-visible", FALSE, NULL);

  if (self->keep_alive_source_id == 0)
    self->keep_alive_source_id = g_timeout_add_seconds (25, wfd_client_keep_alive_timeout, client);
}

static gboolean
wfd_client_idle_wfd_query_params (gpointer user_data)
{
  WfdClient *self = WFD_CLIENT (user_data);
  GstRTSPMessage msg = { 0 };

  g_autoptr(WfdClient) client = WFD_CLIENT (user_data);
  g_autofree gchar * query_params = NULL;

  g_debug ("WFD query params");

  self->init_state = INIT_STATE_M3_SOURCE_GET_PARAMS;

  gst_rtsp_message_init_request (&msg, GST_RTSP_GET_PARAMETER, "rtsp://localhost/wfd1.0");
  gst_rtsp_message_add_header_by_name (&msg, "Content-Type", "text/parameters");
  query_params = wfd_params_m3_query_params (self->params);
  gst_rtsp_message_set_body (&msg, (guint8 *) query_params, strlen (query_params));

  gst_rtsp_client_send_message (GST_RTSP_CLIENT (client), NULL, &msg);

  gst_rtsp_message_unset (&msg);

  return G_SOURCE_REMOVE;
}

GstRTSPStatusCode
wfd_client_pre_options_request (GstRTSPClient *client, GstRTSPContext *ctx)
{
  WfdClient *self = WFD_CLIENT (client);

  if (self->init_state <= INIT_STATE_M2_SINK_QUERY_OPTIONS)
    {
      if (self->init_state == INIT_STATE_M1_SOURCE_QUERY_OPTIONS)
        g_warning ("WfdClient: Got OPTIONS before getting reply querying WFD support; continuing anyway.");

      g_idle_add_full (G_PRIORITY_LOW, wfd_client_idle_wfd_query_params, g_object_ref (client), NULL);
    }

  return GST_RTSP_STS_OK;
}


static void
wfd_client_send_message (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPMessage *msg)
{
  gchar *hdr = NULL;

  /* Hook for sending a message. /*

     /* Modify the "Public" header to advertise support for WFD 1.0 */
  gst_rtsp_message_get_header (msg, GST_RTSP_HDR_PUBLIC, &hdr, 0);
  if (hdr)
    {
      g_autofree gchar * new_hdr = NULL;

      new_hdr = g_strconcat ("org.wfa.wfd1.0, ", hdr, NULL);

      gst_rtsp_message_remove_header (msg, GST_RTSP_HDR_PUBLIC, -1);
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_PUBLIC, new_hdr);
    }

  /* Strip away any ;timeout=30 from outgoing messages. This seems to be
   *  confuse some clients. */
  gst_rtsp_message_get_header (msg, GST_RTSP_HDR_SESSION, &hdr, 0);
  if (msg->type == GST_RTSP_MESSAGE_REQUEST && hdr)
    {
      if (g_str_has_suffix (hdr, ";timeout=30"))
        {
          g_autofree gchar * new_hdr = NULL;

          new_hdr = g_strndup (hdr, strlen (hdr) - 11);
          gst_rtsp_message_remove_header (msg, GST_RTSP_HDR_SESSION, -1);
          gst_rtsp_message_add_header (msg, GST_RTSP_HDR_SESSION, new_hdr);
        }
    }
}

static void
wfd_client_class_init (WfdClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstRTSPClientClass *client_class = GST_RTSP_CLIENT_CLASS (klass);

  object_class->finalize = wfd_client_finalize;
  object_class->get_property = wfd_client_get_property;
  object_class->set_property = wfd_client_set_property;

  client_class->check_requirements = wfd_client_check_requirements;
  client_class->configure_client_media = wfd_client_configure_client_media;
  client_class->handle_response = wfd_client_handle_response;
  client_class->make_path_from_uri = wfd_client_make_path_from_uri;
  client_class->new_session = wfd_client_new_session;
  /*client_class->params_set = wfd_client_params_set;*/
  client_class->pre_options_request = wfd_client_pre_options_request;
  client_class->send_message = wfd_client_send_message;
}

static void
wfd_client_init (WfdClient *self)
{
  self->init_state = INIT_STATE_M0_INVALID;
  self->params = wfd_params_new ();
}

void
wfd_client_query_support (WfdClient *self)
{
  GstRTSPMessage msg = { 0 };

  self->init_state = INIT_STATE_M1_SOURCE_QUERY_OPTIONS;
  gst_rtsp_message_init_request (&msg, GST_RTSP_OPTIONS, "*");
  gst_rtsp_message_add_header_by_name (&msg, "Require", "org.wfa.wfd1.0");

  gst_rtsp_client_send_message (GST_RTSP_CLIENT (self), NULL, &msg);

  gst_rtsp_message_unset (&msg);
}

void
wfd_client_trigger_method (WfdClient *self, const gchar *method)
{
  GstRTSPMessage msg = { 0 };
  g_autofree gchar *body = NULL;

  if (g_str_equal (method, "SETUP") && self->init_state == INIT_STATE_M4_SOURCE_SET_PARAMS)
    self->init_state = INIT_STATE_M5_SOURCE_TRIGGER_SETUP;

  gst_rtsp_message_init_request (&msg, GST_RTSP_SET_PARAMETER, "rtsp://localhost/wfd1.0");
  body = g_strdup_printf ("wfd_trigger_method: %s\r\n", method);
  gst_rtsp_message_set_body (&msg, (guint8 *) body, strlen (body));
  gst_rtsp_message_add_header_by_name (&msg, "Content-Type", "text/parameters");

  gst_rtsp_client_send_message (GST_RTSP_CLIENT (self), NULL, &msg);

  gst_rtsp_message_unset (&msg);
}
