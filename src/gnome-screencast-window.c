/* gnome-screencast-window.c
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

#include <glib/gi18n.h>
#include "gnome-screencast-config.h"
#include "gnome-screencast-window.h"
#include "screencast-sink-list.h"
#include "screencast-sink-row.h"
#include "screencast-meta-provider.h"
#include "screencast-wfd-p2p-registry.h"
#include "screencast-dummy-provider.h"

#include <gst/gst.h>

#include "screencast-portal.h"

struct _GnomeScreencastWindow
{
  GtkApplicationWindow      parent_instance;

  ScreencastMetaProvider   *meta_provider;
  ScreencastWFDP2PRegistry *wfd_p2p_registry;

  ScreencastPortal *portal;
  gboolean          use_x11;

  GCancellable   *cancellable;

  ScreencastSink *stream_sink;

  /* Template widgets */
  GtkStack           *step_stack;
  ScreencastSinkList *find_sink_list;

  GtkListBox         *connect_sink_list;
  GtkLabel           *connect_state_label;
  GtkButton          *connect_cancel;

  GtkListBox         *stream_sink_list;
  GtkLabel           *stream_state_label;
  GtkButton          *stream_cancel;

  GtkListBox         *error_sink_list;
  GtkButton          *error_return;
};

G_DEFINE_TYPE (GnomeScreencastWindow, gnome_screencast_window, GTK_TYPE_APPLICATION_WINDOW)

static GstElement *
sink_create_source_cb (GnomeScreencastWindow * self, ScreencastSink * sink)
{
  GstBin *bin;
  GstElement *src, *dst, *res;

  bin = GST_BIN (gst_bin_new ("screencast source bin"));
  g_debug ("use x11: %d", self->use_x11);
  if (self->use_x11)
    src = gst_element_factory_make ("ximagesrc", "X11 screencast source");
  else
    src = screencast_portal_get_source (self->portal);
  gst_bin_add (bin, src);

  dst = gst_element_factory_make ("intervideosink", "inter video sink");
  g_object_set (dst,
                "channel", "screencast-inter-video",
                "max-lateness", (gint64) - 1,
                NULL);
  gst_bin_add (bin, dst);

  gst_element_link_many (src, dst, NULL);

  res = gst_element_factory_make ("intervideosrc", "screencastsrc");
  g_object_set (res,
                "do-timestamp", TRUE,
                "timeout", (guint64) 10000000000,
                "channel", "screencast-inter-video",
                NULL);

  gst_bin_add (bin, res);

  gst_element_add_pad (GST_ELEMENT (bin),
                       gst_ghost_pad_new ("src",
                                          gst_element_get_static_pad (res,
                                                                      "src")));

  return GST_ELEMENT (bin);
}

static void
remove_widget (GtkWidget *widget, gpointer user_data)
{
  GtkContainer *container = GTK_CONTAINER (user_data);

  gtk_container_remove (container, widget);
}

static void
sink_notify_state_cb (GnomeScreencastWindow *self, GParamSpec *pspec, ScreencastSink *sink)
{
  ScreencastSinkState state;

  g_object_get (sink, "state", &state, NULL);
  g_debug ("Got state change notification from streaming sink to state %s",
           g_enum_to_string (SCREENCAST_TYPE_SINK_STATE, state));

  switch (state)
    {
    case SCREENCAST_SINK_STATE_WAIT_P2P:
      gtk_label_set_text (self->connect_state_label,
                          _("Making P2P connection"));
      break;

    case SCREENCAST_SINK_STATE_WAIT_SOCKET:
      gtk_label_set_text (self->connect_state_label,
                          _("Establishing connection to sink"));
      break;

    case SCREENCAST_SINK_STATE_WAIT_STREAMING:
      gtk_label_set_text (self->connect_state_label,
                          _("Starting to stream"));
      break;

    case SCREENCAST_SINK_STATE_STREAMING:
      gtk_container_foreach (GTK_CONTAINER (self->connect_sink_list), remove_widget, self->connect_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->stream_sink_list), remove_widget, self->stream_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->error_sink_list), remove_widget, self->error_sink_list);
      gtk_container_add (GTK_CONTAINER (self->stream_sink_list),
                         GTK_WIDGET (screencast_sink_row_new (self->stream_sink)));

      gtk_stack_set_visible_child_name (self->step_stack, "stream");
      break;

    case SCREENCAST_SINK_STATE_ERROR:
      gtk_container_foreach (GTK_CONTAINER (self->stream_sink_list), remove_widget, self->stream_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->connect_sink_list), remove_widget, self->connect_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->error_sink_list), remove_widget, self->error_sink_list);
      gtk_container_add (GTK_CONTAINER (self->error_sink_list),
                         GTK_WIDGET (screencast_sink_row_new (self->stream_sink)));

      gtk_stack_set_visible_child_name (self->step_stack, "error");

    case SCREENCAST_SINK_STATE_DISCONNECTED:
      gtk_container_foreach (GTK_CONTAINER (self->stream_sink_list), remove_widget, self->stream_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->connect_sink_list), remove_widget, self->connect_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->error_sink_list), remove_widget, self->error_sink_list);

      gtk_stack_set_visible_child_name (self->step_stack, "find");
      g_object_set (self->meta_provider, "discover", TRUE, NULL);

      g_signal_handlers_disconnect_by_data (self->stream_sink, self);
      g_clear_object (&self->stream_sink);
      break;
    }
}

static void
find_sink_list_row_activated_cb (GnomeScreencastWindow *self, ScreencastSinkRow *row, ScreencastSinkList *sink_list)
{
  ScreencastSink *sink;

  if (!self->portal && ! self->use_x11)
    {
      g_warning ("Cannot start streaming right now as we don't have a portal!");
      return;
    }

  g_assert (SCREENCAST_IS_SINK_ROW (row));

  sink = screencast_sink_row_get_sink (row);
  self->stream_sink = screencast_sink_start_stream (sink);

  if (!self->stream_sink)
    g_warning ("ScreencastWindow: Could not start streaming!");

  g_signal_connect_object (self->stream_sink,
                           "create-source",
                           (GCallback) sink_create_source_cb,
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->stream_sink,
                           "notify::state",
                           (GCallback) sink_notify_state_cb,
                           self,
                           G_CONNECT_SWAPPED);

  g_object_set (self->meta_provider, "discover", FALSE, NULL);
  gtk_stack_set_visible_child_name (self->step_stack, "connect");
  gtk_container_add (GTK_CONTAINER (self->connect_sink_list),
                     GTK_WIDGET (screencast_sink_row_new (self->stream_sink)));
}

static void
gnome_screencast_window_finalize (GObject *obj)
{
  GnomeScreencastWindow *self = GNOME_SCREENCAST_WINDOW (obj);

  g_clear_object (&self->meta_provider);
  g_clear_object (&self->wfd_p2p_registry);

  G_OBJECT_CLASS (gnome_screencast_window_parent_class)->finalize (obj);
}

static void
gnome_screencast_window_class_init (GnomeScreencastWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gnome_screencast_window_finalize;

  SCREENCAST_TYPE_SINK_LIST;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/screencast/gnome-screencast-window.ui");
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, step_stack);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, find_sink_list);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, connect_sink_list);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, connect_state_label);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, connect_cancel);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, stream_sink_list);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, stream_state_label);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, stream_cancel);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, error_sink_list);
  gtk_widget_class_bind_template_child (widget_class, GnomeScreencastWindow, error_return);
}

static void
screencast_portal_init_async_cb (GObject      *source_object,
                                 GAsyncResult *res,
                                 gpointer      user_data)
{
  GnomeScreencastWindow *window;

  g_autoptr(GError) error = NULL;

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (source_object), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error initing screencast portal: %s", error->message);

          window = GNOME_SCREENCAST_WINDOW (user_data);

          /* If this the error was for an unknown method, then we likely
           * don't have a gnome-shell or it does not support the screen casting
           * portal.
           * In this case we assume that an xvimagesrc is usable, in all other
           * cases it is a fatal error. */
          if (!g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD))
            gtk_widget_destroy (GTK_WIDGET (window));
          else
            window->use_x11 = TRUE;
        }

      g_object_unref (source_object);
      return;
    }

  window = GNOME_SCREENCAST_WINDOW (user_data);
  window->portal = SCREENCAST_PORTAL (source_object);
}

static void
stream_stop_clicked_cb (GnomeScreencastWindow *self)
{
  if (!self->stream_sink)
    return;

  screencast_sink_stop_stream (self->stream_sink);
}

static void
gnome_screencast_window_init (GnomeScreencastWindow *self)
{
  ScreencastPortal *portal;
  gtk_widget_init_template (GTK_WIDGET (self));

  self->meta_provider = screencast_meta_provider_new ();
  self->wfd_p2p_registry = screencast_wfd_p2p_registry_new (self->meta_provider);
  screencast_sink_list_set_provider (self->find_sink_list, SCREENCAST_PROVIDER (self->meta_provider));

  if (g_strcmp0 (g_getenv ("SCREENCAST_DUMMY"), "1") == 0)
    {
      g_autoptr(ScreencastDummyProvider) dummy_provider = NULL;

      g_debug ("Adding dummy provider");
      dummy_provider = screencast_dummy_provider_new ();
      screencast_meta_provider_add_provider (self->meta_provider, SCREENCAST_PROVIDER (dummy_provider));
    }

  g_signal_connect_object (self->find_sink_list,
                           "row-activated",
                           (GCallback) find_sink_list_row_activated_cb,
                           self,
                           G_CONNECT_SWAPPED);

  self->cancellable = g_cancellable_new ();

  /* All of these buttons just stop the stream, which will return us
   * to the DISCONNECTED state and the selection page. */
  g_signal_connect_object (self->connect_cancel,
                           "clicked",
                           (GCallback) stream_stop_clicked_cb,
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->stream_cancel,
                           "clicked",
                           (GCallback) stream_stop_clicked_cb,
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->error_return,
                           "clicked",
                           (GCallback) stream_stop_clicked_cb,
                           self,
                           G_CONNECT_SWAPPED);

  portal = screencast_portal_new ();
  g_async_initable_init_async (G_ASYNC_INITABLE (portal),
                               G_PRIORITY_LOW,
                               self->cancellable,
                               screencast_portal_init_async_cb,
                               self);
}
