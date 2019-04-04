/* screencast-wfd-p2p-sink.c
 *
 * Copyright 2018 Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gnome-screencast-config.h"
#include "screencast-wfd-p2p-sink.h"
#include "wfd/wfd-server.h"
#include "wfd/wfd-client.h"

struct _ScreencastWFDP2PSink
{
  GObject             parent_instance;

  ScreencastSinkState state;

  GCancellable       *cancellable;

  NMClient           *nm_client;
  NMDevice           *nm_device;
  NMWifiP2PPeer      *nm_peer;
  NMActiveConnection *nm_ac;

  WfdServer          *server;
  guint               server_source_id;
};

enum {
  PROP_CLIENT = 1,
  PROP_DEVICE,
  PROP_PEER,

  PROP_DISPLAY_NAME,
  PROP_MATCHES,
  PROP_PRIORITY,
  PROP_STATE,

  PROP_LAST = PROP_DISPLAY_NAME,
};

static void screencast_wfd_p2p_sink_sink_iface_init (ScreencastSinkIface *iface);
static ScreencastSink * screencast_wfd_p2p_sink_sink_start_stream (ScreencastSink *sink);
static void screencast_wfd_p2p_sink_sink_stop_stream (ScreencastSink *sink);

static void screencast_wfd_p2p_sink_sink_stop_stream_int (ScreencastWFDP2PSink *self);


G_DEFINE_TYPE_EXTENDED (ScreencastWFDP2PSink, screencast_wfd_p2p_sink, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SCREENCAST_TYPE_SINK,
                                               screencast_wfd_p2p_sink_sink_iface_init);
                       )

static GParamSpec * props[PROP_LAST] = { NULL, };


static void
peer_notify_cb (ScreencastWFDP2PSink *self, GParamSpec *pspec, NMWifiP2PPeer *peer)
{
  /* TODO: Assumes the display name may have changed.
   *       This is obviously overly agressive, on the other hand
   *       not really an issue. */
  g_object_notify (G_OBJECT (self), "display-name");
}

static void
notify_active_connection_cb (ScreencastWFDP2PSink *self, GParamSpec *pspec, NMDevice *device)
{
  if (!self->nm_ac)
    return;

  /* Nothing to do if it is still the correct connection. */
  if (self->nm_ac == nm_device_get_active_connection (device))
    return;

  /* Our active connection is not active anymore ... */
  g_clear_object (&self->nm_ac);

  screencast_wfd_p2p_sink_sink_stop_stream_int (self);
  self->state = SCREENCAST_SINK_STATE_ERROR;
  g_object_notify (G_OBJECT (self), "state");
}

static void
screencast_wfd_p2p_sink_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ScreencastWFDP2PSink *sink = SCREENCAST_WFD_P2P_SINK (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, sink->nm_client);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, sink->nm_device);
      break;

    case PROP_PEER:
      g_value_set_object (value, sink->nm_peer);
      break;

    case PROP_DISPLAY_NAME:
      g_object_get_property (G_OBJECT (sink->nm_peer), "name", value);
      break;

    case PROP_MATCHES:
      {
        g_autoptr(GPtrArray) res = NULL;
        res = g_ptr_array_new_with_free_func (g_free);

        g_ptr_array_add (res, g_strdup (nm_wifi_p2p_peer_get_hw_address (sink->nm_peer)));

        g_value_take_boxed (value, g_steal_pointer (&res));
        break;
      }

    case PROP_PRIORITY:
      g_value_set_int (value, 100);
      break;

    case PROP_STATE:
      g_value_set_enum (value, sink->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_wfd_p2p_sink_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ScreencastWFDP2PSink *sink = SCREENCAST_WFD_P2P_SINK (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_assert (sink->nm_client == NULL);
      sink->nm_client = g_value_dup_object (value);
      break;

    case PROP_DEVICE:
      g_assert (sink->nm_device == NULL);
      sink->nm_device = g_value_dup_object (value);

      g_signal_connect_object (sink->nm_device,
                               "notify::" NM_DEVICE_ACTIVE_CONNECTION,
                               (GCallback) notify_active_connection_cb,
                               sink,
                               G_CONNECT_SWAPPED);
      break;

    case PROP_PEER:
      g_assert (sink->nm_peer == NULL);
      sink->nm_peer = g_value_dup_object (value);

      g_signal_connect_object (sink->nm_peer,
                               "notify",
                               (GCallback) peer_notify_cb,
                               sink,
                               G_CONNECT_SWAPPED);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
screencast_wfd_p2p_sink_finalize (GObject *object)
{
  ScreencastWFDP2PSink *sink = SCREENCAST_WFD_P2P_SINK (object);

  g_cancellable_cancel (sink->cancellable);
  g_clear_object (&sink->cancellable);

  screencast_wfd_p2p_sink_sink_stop_stream_int (sink);

  g_clear_object (&sink->nm_client);
  g_clear_object (&sink->nm_device);
  g_clear_object (&sink->nm_peer);

  G_OBJECT_CLASS (screencast_wfd_p2p_sink_parent_class)->finalize (object);
}

static void
screencast_wfd_p2p_sink_class_init (ScreencastWFDP2PSinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = screencast_wfd_p2p_sink_get_property;
  object_class->set_property = screencast_wfd_p2p_sink_set_property;
  object_class->finalize = screencast_wfd_p2p_sink_finalize;

  props[PROP_CLIENT] =
    g_param_spec_object ("client", "Client",
                         "The NMClient used to find the sink.",
                         NM_TYPE_CLIENT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_DEVICE] =
    g_param_spec_object ("device", "Device",
                         "The NMDevice the sink was found on.",
                         NM_TYPE_DEVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_PEER] =
    g_param_spec_object ("peer", "Peer",
                         "The NMP2PPeer for this sink.",
                         NM_TYPE_WIFI_P2P_PEER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

  g_object_class_override_property (object_class, PROP_DISPLAY_NAME, "display-name");
  g_object_class_override_property (object_class, PROP_MATCHES, "matches");
  g_object_class_override_property (object_class, PROP_PRIORITY, "priority");
  g_object_class_override_property (object_class, PROP_STATE, "state");
}

static void
screencast_wfd_p2p_sink_init (ScreencastWFDP2PSink *sink)
{
  sink->state = SCREENCAST_SINK_STATE_DISCONNECTED;
  sink->cancellable = g_cancellable_new ();
}

/******************************************************************
* ScreencastSink interface implementation
******************************************************************/

static void
screencast_wfd_p2p_sink_sink_iface_init (ScreencastSinkIface *iface)
{
  iface->start_stream = screencast_wfd_p2p_sink_sink_start_stream;
  iface->stop_stream = screencast_wfd_p2p_sink_sink_stop_stream;
}

static void
play_request_cb (ScreencastWFDP2PSink *sink, GstRTSPContext *ctx, WfdClient *client)
{
  g_debug ("ScreencastWfdP2PSink: Got play request from client");

  sink->state = SCREENCAST_SINK_STATE_STREAMING;
  g_object_notify (G_OBJECT (sink), "state");
}

static void
closed_cb (ScreencastWFDP2PSink *sink, WfdClient *client)
{
  /* Connection was closed, do a clean shutdown*/
  screencast_wfd_p2p_sink_sink_stop_stream (SCREENCAST_SINK (sink));
}

static void
client_connected_cb (ScreencastWFDP2PSink *sink, WfdClient *client, WfdServer *server)
{
  g_debug ("ScreencastWfdP2PSink: Got client connection");

  g_signal_handlers_disconnect_by_func (sink->server, client_connected_cb, sink);
  sink->state = SCREENCAST_SINK_STATE_WAIT_STREAMING;
  g_object_notify (G_OBJECT (sink), "state");

  /* XXX: connect to further events. */
  g_signal_connect_object (client,
                           "play-request",
                           (GCallback) play_request_cb,
                           sink,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (client,
                           "closed",
                           (GCallback) closed_cb,
                           sink,
                           G_CONNECT_SWAPPED);
}

static GstElement *
server_create_source_cb (ScreencastWFDP2PSink *sink, WfdServer *server)
{
  GstElement *res;

  g_signal_emit_by_name (sink, "create-source", &res);

  return res;
}

static GstElement *
server_create_audio_source_cb (ScreencastWFDP2PSink *sink, WfdServer *server)
{
  GstElement *res;

  g_signal_emit_by_name (sink, "create-audio-source", &res);

  return res;
}

static void
p2p_connected (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  ScreencastWFDP2PSink *sink = NULL;
  NMActiveConnection *ac = NULL;

  g_debug ("ScreencastWfdP2PSink: Got P2P connection");

  ac = nm_client_add_and_activate_connection2_finish (NM_CLIENT (source_object), res, NULL, &error);
  if (!ac)
    {
      /* Operation was aborted */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Error activating connection: %s", error->message);
      sink = SCREENCAST_WFD_P2P_SINK (user_data);
      screencast_wfd_p2p_sink_sink_stop_stream_int (sink);
      sink->state = SCREENCAST_SINK_STATE_ERROR;
      g_object_notify (G_OBJECT (sink), "state");
      return;
    }

  sink = SCREENCAST_WFD_P2P_SINK (user_data);
  sink->nm_ac = ac;

  g_assert (sink->server == NULL);
  sink->server = wfd_server_new ();
  /*
   * XXX: Not yet implemented, but we should only bind on the P2P device
   * wfd_server_set_interface (GST_RTSP_SERVER (sink->server), nm_device_get_ip_iface (sink->nm_device));
   */
  sink->server_source_id = gst_rtsp_server_attach (GST_RTSP_SERVER (sink->server), NULL);

  if (sink->server_source_id == 0)
    {
      sink->state = SCREENCAST_SINK_STATE_ERROR;
      g_object_notify (G_OBJECT (sink), "state");
      g_clear_object (&sink->server);

      return;
    }

  g_signal_connect_object (sink->server,
                           "client-connected",
                           (GCallback) client_connected_cb,
                           sink,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (sink->server,
                           "create-source",
                           (GCallback) server_create_source_cb,
                           sink,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (sink->server,
                           "create-audio-source",
                           (GCallback) server_create_audio_source_cb,
                           sink,
                           G_CONNECT_SWAPPED);

  sink->state = SCREENCAST_SINK_STATE_WAIT_SOCKET;
  g_object_notify (G_OBJECT (sink), "state");
}

static ScreencastSink *
screencast_wfd_p2p_sink_sink_start_stream (ScreencastSink *sink)
{
  g_autoptr(NMConnection) connection = NULL;
  g_autoptr(GVariantBuilder) builder = NULL;
  g_autoptr(GBytes) wfd_ies = NULL;
  ScreencastWFDP2PSink *self = SCREENCAST_WFD_P2P_SINK (sink);
  GVariant *options = NULL;
  NMSetting *p2p_setting;
  NMSetting *ipv4_setting;
  NMSetting *ipv6_setting;

  g_return_val_if_fail (self->state == SCREENCAST_SINK_STATE_DISCONNECTED, NULL);

  self->state = SCREENCAST_SINK_STATE_WAIT_P2P;
  g_object_notify (G_OBJECT (self), "state");

  builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (builder, "{sv}", "bind-activation", g_variant_new_string ("dbus-client"));
  g_variant_builder_add (builder, "{sv}", "persist", g_variant_new_string ("volatile"));

  options = g_variant_builder_end (builder);

  /* Static WFD IEs describing a source with the RTSP server on port 7236. */
  wfd_ies = g_bytes_new_static ("\x00\x00\x06\x00\x90\x1c\x44\x00\xc8", 9);

  connection = nm_simple_connection_new ();
  p2p_setting = nm_setting_wifi_p2p_new ();
  nm_connection_add_setting (connection, p2p_setting);
  g_object_set (p2p_setting, NM_SETTING_WIFI_P2P_WFD_IES, wfd_ies, NULL);

  /* We never want to route on IPv4 */
  ipv4_setting = nm_setting_ip4_config_new ();
  nm_connection_add_setting (connection, ipv4_setting);
  g_object_set (ipv4_setting,
                NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO,
                NM_SETTING_IP_CONFIG_NEVER_DEFAULT, TRUE,
                NULL);

  /* We do not need IPv6 */
  ipv6_setting = nm_setting_ip4_config_new ();
  nm_connection_add_setting (connection, ipv6_setting);
  g_object_set (ipv6_setting,
                NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO,
                NM_SETTING_IP_CONFIG_NEVER_DEFAULT, TRUE,
                NM_SETTING_IP_CONFIG_MAY_FAIL, TRUE,
                NULL);

  nm_client_add_and_activate_connection2 (self->nm_client,
                                          connection,
                                          self->nm_device,
                                          nm_object_get_path (NM_OBJECT (self->nm_peer)),
                                          options,
                                          self->cancellable,
                                          p2p_connected,
                                          sink);

  return g_object_ref (sink);
}

static void
screencast_wfd_p2p_sink_sink_stop_stream_int (ScreencastWFDP2PSink *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  /* Destroy the server that is streaming. */
  if (self->server_source_id)
    {
      g_source_remove (self->server_source_id);
      self->server_source_id = 0;
    }

  /* Needs to protect against recursion. */
  if (self->server)
    {
      g_autoptr(WfdServer) server = NULL;

      server = g_steal_pointer (&self->server);
      g_signal_handlers_disconnect_by_data (server, self);
      wfd_server_purge (server);
    }

  /* And disconnect our active connection.
   * nm_ac will be unset if something else destroyed the connection already */
  if (self->nm_ac)
    {
      nm_device_disconnect (self->nm_device, NULL, NULL);
      g_clear_object (&self->nm_ac);
    }
}

static void
screencast_wfd_p2p_sink_sink_stop_stream (ScreencastSink *sink)
{
  ScreencastWFDP2PSink *self = SCREENCAST_WFD_P2P_SINK (sink);

  screencast_wfd_p2p_sink_sink_stop_stream_int (self);

  self->state = SCREENCAST_SINK_STATE_DISCONNECTED;
  g_object_notify (G_OBJECT (self), "state");
}

/******************************************************************
* ScreencastWFDP2PSink public functions
******************************************************************/

/**
 * screencast_wfd_p2p_sink_get_client
 * @sink: a #ScreencastWFDP2PSink
 *
 * Retrieve the #NMClient used to find the sink.
 *
 * Returns: (transfer none): The #NMClient
 */
NMClient *
screencast_wfd_p2p_sink_get_client (ScreencastWFDP2PSink * sink)
{
  return sink->nm_client;
}

/**
 * screencast_wfd_p2p_sink_get_device
 * @sink: a #ScreencastWFDP2PSink
 *
 * Retrieve the #NMDevice the sink was found on.
 *
 * Returns: (transfer none): The #NMDevice
 */
NMDevice *
screencast_wfd_p2p_sink_get_device (ScreencastWFDP2PSink * sink)
{
  return sink->nm_device;
}

/**
 * screencast_wfd_p2p_sink_get_peer
 * @sink: a #ScreencastWFDP2PSink
 *
 * Retrieve the #NMWifiP2PPeer the sink was found on.
 *
 * Returns: (transfer none): The #NMWifiP2PPeer
 */
NMWifiP2PPeer *
screencast_wfd_p2p_sink_get_peer (ScreencastWFDP2PSink * sink)
{
  return sink->nm_peer;
}

ScreencastWFDP2PSink *
screencast_wfd_p2p_sink_new (NMClient *client, NMDevice *device, NMWifiP2PPeer * peer)
{
  return g_object_new (SCREENCAST_TYPE_WFD_P2P_SINK,
                       "client", client,
                       "device", device,
                       "peer", peer,
                       NULL);
}
