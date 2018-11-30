#include "wfd-server.h"
#include "wfd-client.h"
#include "wfd-media-factory.h"

struct _WfdServer
{
  GstRTSPServer parent_instance;

  guint         clean_pool_source_id;
};

G_DEFINE_TYPE (WfdServer, wfd_server, GST_TYPE_RTSP_SERVER)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum
{
  SIGNAL_CREATE_SOURCE,
  NR_SIGNALS
};

static guint signals[NR_SIGNALS];


WfdServer *
wfd_server_new (void)
{
  return g_object_new (WFD_TYPE_SERVER, NULL);
}

static void
wfd_server_finalize (GObject *object)
{
  WfdServer *self = (WfdServer *) object;

  g_source_remove (self->clean_pool_source_id);
  self->clean_pool_source_id = 0;

  G_OBJECT_CLASS (wfd_server_parent_class)->finalize (object);
}

static void
wfd_server_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  WfdServer *self = WFD_SERVER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
wfd_server_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  WfdServer *self = WFD_SERVER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GstRTSPClient *
wfd_server_create_client (GstRTSPServer *server)
{
  g_autoptr(WfdClient) client;
  GstRTSPClient *rtsp_client;

  client = wfd_client_new ();
  rtsp_client = GST_RTSP_CLIENT (client);

  gst_rtsp_client_set_session_pool (rtsp_client, gst_rtsp_server_get_session_pool (server));
  gst_rtsp_client_set_mount_points (rtsp_client, gst_rtsp_server_get_mount_points (server));
  gst_rtsp_client_set_auth (rtsp_client, gst_rtsp_server_get_auth (server));
  gst_rtsp_client_set_thread_pool (rtsp_client, gst_rtsp_server_get_thread_pool (server));

  return GST_RTSP_CLIENT (g_steal_pointer (&client));
}

static gboolean
timeout_query_wfd_support (gpointer user_data)
{
  WfdClient *client = WFD_CLIENT (user_data);

  g_object_set_data (G_OBJECT (client),
                     "wfd-query-support-timeout",
                     NULL);

  wfd_client_query_support (client);

  return G_SOURCE_REMOVE;
}

static void
wfd_server_client_closed (GstRTSPServer *server, GstRTSPClient *client)
{
  guint query_support_id;

  query_support_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (client),
                                                          "wfd-query-support-timeout"));

  if (query_support_id)
    g_source_remove (query_support_id);
}

static void
wfd_server_client_connected (GstRTSPServer *server, GstRTSPClient *client)
{
  guint query_support_id;

  query_support_id = g_timeout_add (500, timeout_query_wfd_support, client);

  g_object_set_data (G_OBJECT (client),
                     "wfd-query-support-timeout",
                     GUINT_TO_POINTER (query_support_id));

  g_signal_connect_object (client,
                           "closed",
                           (GCallback) wfd_server_client_closed,
                           server,
                           G_CONNECT_SWAPPED);
}

static void
wfd_server_class_init (WfdServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstRTSPServerClass *server_class = GST_RTSP_SERVER_CLASS (klass);

  object_class->finalize = wfd_server_finalize;
  object_class->get_property = wfd_server_get_property;
  object_class->set_property = wfd_server_set_property;

  server_class->create_client = wfd_server_create_client;
  server_class->client_connected = wfd_server_client_connected;

  signals[SIGNAL_CREATE_SOURCE] =
    g_signal_new ("create-source", WFD_TYPE_SERVER, G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL,
                  NULL,
                  GST_TYPE_ELEMENT, 0);
}

static gboolean
clean_pool (gpointer user_data)
{
  GstRTSPServer *server = GST_RTSP_SERVER (user_data);
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);

  return G_SOURCE_CONTINUE;
}

static GstElement*
factory_source_create_cb (WfdMediaFactory *factory, WfdServer *self)
{
  GstElement *res;

  g_signal_emit (self, signals[SIGNAL_CREATE_SOURCE], 0, &res);

  return res;
}

static void
wfd_server_init (WfdServer *self)
{
  g_autoptr(WfdMediaFactory) factory = NULL;
  GstRTSPMountPoints *mount_points;
  /* We need to clean up the pool regularly as it does not happen
   * automatically. */
  self->clean_pool_source_id = g_timeout_add_seconds (2, clean_pool, self);

  factory = wfd_media_factory_new ();
  g_signal_connect_object (factory,
                           "create-source",
                           (GCallback) factory_source_create_cb,
                           self,
                           0);

  mount_points = gst_rtsp_server_get_mount_points (GST_RTSP_SERVER (self));
  gst_rtsp_mount_points_add_factory (mount_points, "/wfd1.0", GST_RTSP_MEDIA_FACTORY (g_steal_pointer (&factory)));

  gst_rtsp_server_set_address (GST_RTSP_SERVER (self), "0.0.0.0");
  gst_rtsp_server_set_service (GST_RTSP_SERVER (self), "7236");
}
