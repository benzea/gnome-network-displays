/* screencast-dummy-wfd-sink.c
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
#include "screencast-dummy-wfd-sink.h"
#include "wfd/wfd-server.h"
#include "wfd/wfd-client.h"

struct _ScreencastDummyWFDSink
{
  GObject             parent_instance;

  ScreencastSinkState state;

  GCancellable       *cancellable;

  ScreencastPortal   *portal;
  WfdServer          *server;
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

static void screencast_dummy_wfd_sink_sink_iface_init (ScreencastSinkIface *iface);
static ScreencastSink * screencast_dummy_wfd_sink_sink_start_stream (ScreencastSink *sink, ScreencastPortal *portal);

G_DEFINE_TYPE_EXTENDED (ScreencastDummyWFDSink, screencast_dummy_wfd_sink, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SCREENCAST_TYPE_SINK,
                                               screencast_dummy_wfd_sink_sink_iface_init);
                       )


static void
screencast_dummy_wfd_sink_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ScreencastDummyWFDSink *sink = SCREENCAST_DUMMY_WFD_SINK (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, "Dummy WFD Sink");
      break;

    case PROP_MATCHES:
      {
        g_autoptr(GPtrArray) res = NULL;
        res = g_ptr_array_new_with_free_func (g_free);

        g_ptr_array_add (res, g_strdup ("dummy-wfd-sink"));

        g_value_take_boxed (value, g_steal_pointer (&res));
        break;
      }

    case PROP_PRIORITY:
      g_value_set_int (value, 0);
      break;

    case PROP_STATE:
      g_value_set_enum (value, sink->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
screencast_dummy_wfd_sink_finalize (GObject *object)
{
  ScreencastDummyWFDSink *sink = SCREENCAST_DUMMY_WFD_SINK (object);

  g_cancellable_cancel (sink->cancellable);
  g_clear_object (&sink->cancellable);

  g_clear_object (&sink->portal);
  g_clear_object (&sink->server);

  G_OBJECT_CLASS (screencast_dummy_wfd_sink_parent_class)->finalize (object);
}

static void
screencast_dummy_wfd_sink_class_init (ScreencastDummyWFDSinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = screencast_dummy_wfd_sink_get_property;
  object_class->finalize = screencast_dummy_wfd_sink_finalize;

  g_object_class_override_property (object_class, PROP_DISPLAY_NAME, "display-name");
  g_object_class_override_property (object_class, PROP_MATCHES, "matches");
  g_object_class_override_property (object_class, PROP_PRIORITY, "priority");
  g_object_class_override_property (object_class, PROP_STATE, "state");
}

static void
screencast_dummy_wfd_sink_init (ScreencastDummyWFDSink *sink)
{
  sink->state = SCREENCAST_SINK_STATE_DISCONNECTED;
  sink->cancellable = g_cancellable_new ();
}

/******************************************************************
* ScreencastSink interface implementation
******************************************************************/

static void
screencast_dummy_wfd_sink_sink_iface_init (ScreencastSinkIface *iface)
{
  iface->start_stream = screencast_dummy_wfd_sink_sink_start_stream;
}

static void
play_request_cb (ScreencastDummyWFDSink *sink, GstRTSPContext *ctx, WfdClient *client)
{
  g_debug ("ScreencastWfdP2PSink: Got play request from client");

  sink->state = SCREENCAST_SINK_STATE_STREAMING;
  g_object_notify (G_OBJECT (sink), "state");
}

static void
client_connected_cb (ScreencastDummyWFDSink *sink, WfdClient *client, WfdServer *server)
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
}

static GstElement*
server_source_create_cb (ScreencastDummyWFDSink *sink, WfdServer *server)
{
  return screencast_portal_get_source (sink->portal);
}

static ScreencastSink *
screencast_dummy_wfd_sink_sink_start_stream (ScreencastSink *sink, ScreencastPortal *portal)
{
  ScreencastDummyWFDSink *self = SCREENCAST_DUMMY_WFD_SINK (sink);

  if (self->server)
    g_signal_handlers_disconnect_by_data (self->server, self);
  g_clear_object (&self->server);

  self->server = wfd_server_new ();

  if (gst_rtsp_server_attach (GST_RTSP_SERVER (self->server), NULL) < 0)
    {
      self->state = SCREENCAST_SINK_STATE_ERROR;
      g_object_notify (G_OBJECT (self), "state");
      g_clear_object (&self->server);

      return NULL;
    }

  g_debug ("ScreencastDummyWFDSink: You should now be able to connect to rtsp://localhost:7236/wfd1.0");

  g_signal_connect_object (self->server,
                           "client-connected",
                           (GCallback) client_connected_cb,
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->server,
                           "create-source",
                           (GCallback) server_source_create_cb,
                           self,
                           G_CONNECT_SWAPPED);

  self->state = SCREENCAST_SINK_STATE_WAIT_SOCKET;
  g_object_notify (G_OBJECT (self), "state");

  return g_object_ref (sink);
}

ScreencastDummyWFDSink *
screencast_dummy_wfd_sink_new (void)
{
  return g_object_new (SCREENCAST_TYPE_DUMMY_WFD_SINK,
                       NULL);
}
