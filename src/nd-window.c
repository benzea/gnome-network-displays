/* gnome-nd-window.c
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
#include "gnome-network-displays-config.h"
#include "nd-window.h"
#include "nd-sink-list.h"
#include "nd-sink-row.h"
#include "nd-meta-provider.h"
#include "nd-wfd-p2p-registry.h"
#include "nd-dummy-provider.h"

#include <gst/gst.h>

#include "nd-screencast-portal.h"
#include "nd-pulseaudio.h"

struct _NdWindow
{
  GtkApplicationWindow parent_instance;

  NdMetaProvider      *meta_provider;
  NdWFDP2PRegistry    *wfd_p2p_registry;

  NdScreencastPortal  *portal;
  gboolean             use_x11;

  NdPulseaudio        *pulse;

  GCancellable        *cancellable;

  NdSink              *stream_sink;

  /* Template widgets */
  GtkStack   *has_providers_stack;
  GtkStack   *step_stack;
  NdSinkList *find_sink_list;

  GtkListBox *connect_sink_list;
  GtkLabel   *connect_state_label;
  GtkButton  *connect_cancel;

  GtkListBox *stream_sink_list;
  GtkLabel   *stream_state_label;
  GtkButton  *stream_cancel;

  GtkListBox *error_sink_list;
  GtkButton  *error_return;
};

G_DEFINE_TYPE (NdWindow, gnome_nd_window, GTK_TYPE_APPLICATION_WINDOW)

static GstElement *
sink_create_source_cb (NdWindow * self, NdSink * sink)
{
  GstBin *bin;
  GstElement *src, *dst, *res;

  bin = GST_BIN (gst_bin_new ("screencast source bin"));
  g_debug ("use x11: %d", self->use_x11);
  if (self->use_x11)
    src = gst_element_factory_make ("ximagesrc", "X11 screencast source");
  else
    src = nd_screencast_portal_get_source (self->portal);
  gst_bin_add (bin, src);

  dst = gst_element_factory_make ("intervideosink", "inter video sink");
  g_object_set (dst,
                "channel", "nd-inter-video",
                "max-lateness", (gint64) - 1,
                NULL);
  gst_bin_add (bin, dst);

  gst_element_link_many (src, dst, NULL);

  res = gst_element_factory_make ("intervideosrc", "screencastsrc");
  g_object_set (res,
                "do-timestamp", TRUE,
                "timeout", (guint64) G_MAXUINT64,
                "channel", "nd-inter-video",
                NULL);

  gst_bin_add (bin, res);

  gst_element_add_pad (GST_ELEMENT (bin),
                       gst_ghost_pad_new ("src",
                                          gst_element_get_static_pad (res,
                                                                      "src")));

  return GST_ELEMENT (bin);
}

static GstElement *
sink_create_audio_source_cb (NdWindow * self, NdSink * sink)
{
  GstElement *res;

  if (!self->pulse)
    return NULL;

  res = nd_pulseaudio_get_source (self->pulse);

  return res;
}

static void
remove_widget (GtkWidget *widget, gpointer user_data)
{
  GtkContainer *container = GTK_CONTAINER (user_data);

  gtk_container_remove (container, widget);
}

static void
sink_notify_state_cb (NdWindow *self, GParamSpec *pspec, NdSink *sink)
{
  NdSinkState state;

  g_object_get (sink, "state", &state, NULL);
  g_debug ("Got state change notification from streaming sink to state %s",
           g_enum_to_string (ND_TYPE_SINK_STATE, state));

  switch (state)
    {
    case ND_SINK_STATE_WAIT_P2P:
      gtk_label_set_text (self->connect_state_label,
                          _("Making P2P connection"));
      break;

    case ND_SINK_STATE_WAIT_SOCKET:
      gtk_label_set_text (self->connect_state_label,
                          _("Establishing connection to sink"));
      break;

    case ND_SINK_STATE_WAIT_STREAMING:
      gtk_label_set_text (self->connect_state_label,
                          _("Starting to stream"));
      break;

    case ND_SINK_STATE_STREAMING:
      gtk_container_foreach (GTK_CONTAINER (self->connect_sink_list), remove_widget, self->connect_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->stream_sink_list), remove_widget, self->stream_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->error_sink_list), remove_widget, self->error_sink_list);
      gtk_container_add (GTK_CONTAINER (self->stream_sink_list),
                         GTK_WIDGET (nd_sink_row_new (self->stream_sink)));

      gtk_stack_set_visible_child_name (self->step_stack, "stream");
      break;

    case ND_SINK_STATE_ERROR:
      gtk_container_foreach (GTK_CONTAINER (self->stream_sink_list), remove_widget, self->stream_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->connect_sink_list), remove_widget, self->connect_sink_list);
      gtk_container_foreach (GTK_CONTAINER (self->error_sink_list), remove_widget, self->error_sink_list);
      gtk_container_add (GTK_CONTAINER (self->error_sink_list),
                         GTK_WIDGET (nd_sink_row_new (self->stream_sink)));

      gtk_stack_set_visible_child_name (self->step_stack, "error");
      break;

    case ND_SINK_STATE_DISCONNECTED:
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
find_sink_list_row_activated_cb (NdWindow *self, NdSinkRow *row, NdSinkList *sink_list)
{
  NdSink *sink;

  if (!self->portal && !self->use_x11)
    {
      g_warning ("Cannot start streaming right now as we don't have a portal!");
      return;
    }

  g_assert (ND_IS_SINK_ROW (row));

  sink = nd_sink_row_get_sink (row);
  self->stream_sink = nd_sink_start_stream (sink);

  if (!self->stream_sink)
    g_warning ("NdWindow: Could not start streaming!");

  g_signal_connect_object (self->stream_sink,
                           "create-source",
                           (GCallback) sink_create_source_cb,
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->stream_sink,
                           "create-audio-source",
                           (GCallback) sink_create_audio_source_cb,
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
                     GTK_WIDGET (nd_sink_row_new (self->stream_sink)));
}

static void
gnome_nd_window_finalize (GObject *obj)
{
  NdWindow *self = ND_WINDOW (obj);

  g_clear_object (&self->meta_provider);
  g_clear_object (&self->wfd_p2p_registry);

  G_OBJECT_CLASS (gnome_nd_window_parent_class)->finalize (obj);
}

static void
gnome_nd_window_class_init (NdWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gnome_nd_window_finalize;

  ND_TYPE_SINK_LIST;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/screencast/nd-window.ui");
  gtk_widget_class_bind_template_child (widget_class, NdWindow, has_providers_stack);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, step_stack);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, find_sink_list);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, connect_sink_list);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, connect_state_label);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, connect_cancel);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, stream_sink_list);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, stream_state_label);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, stream_cancel);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, error_sink_list);
  gtk_widget_class_bind_template_child (widget_class, NdWindow, error_return);
}

static void
nd_screencast_portal_init_async_cb (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  NdWindow *window;

  g_autoptr(GError) error = NULL;

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (source_object), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error initing screencast portal: %s", error->message);

          window = ND_WINDOW (user_data);

          /* Unknown method means the portal does not exist, give a slightly
           * more specific warning then.
           */
          if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD))
            g_warning ("Nding portal is unavailable! It is required to select the monitor to stream!");

          g_warning ("Falling back to X11! You need to fix your setup to avoid issues (XDG Portals and/or mutter screencasting support)!");
          window->use_x11 = TRUE;
        }

      g_object_unref (source_object);
      return;
    }

  window = ND_WINDOW (user_data);
  window->portal = ND_SCREENCAST_PORTAL (source_object);
}

static void
nd_pulseaudio_init_async_cb (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
  NdWindow *window;

  g_autoptr(GError) error = NULL;

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (source_object), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error initializing pulse audio sink: %s", error->message);

      g_object_unref (source_object);
      return;
    }

  window = ND_WINDOW (user_data);
  window->pulse = ND_PULSEAUDIO (source_object);
}

static void
stream_stop_clicked_cb (NdWindow *self)
{
  if (!self->stream_sink)
    return;

  nd_sink_stop_stream (self->stream_sink);
}

static void
on_meta_provider_has_provider_changed_cb (NdWindow *self, NdSinkRow *row, NdSinkList *sink_list)
{
  gboolean has_providers;

  g_object_get (self->meta_provider,
                "has-providers", &has_providers,
                NULL);
  gtk_stack_set_visible_child_name (self->has_providers_stack,
                                    has_providers ? "has-providers" : "no-providers");
}

static void
gnome_nd_window_init (NdWindow *self)
{
  NdScreencastPortal *portal;
  NdPulseaudio *pulse;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->meta_provider = nd_meta_provider_new ();
  g_signal_connect_object (self->meta_provider,
                           "notify::has-providers",
                           (GCallback) on_meta_provider_has_provider_changed_cb,
                           self,
                           G_CONNECT_SWAPPED);

  self->wfd_p2p_registry = nd_wfd_p2p_registry_new (self->meta_provider);
  nd_sink_list_set_provider (self->find_sink_list, ND_PROVIDER (self->meta_provider));

  if (g_strcmp0 (g_getenv ("NETWORK_DISPLAYS_DUMMY"), "1") == 0)
    {
      g_autoptr(NdDummyProvider) dummy_provider = NULL;

      g_debug ("Adding dummy provider");
      dummy_provider = nd_dummy_provider_new ();
      nd_meta_provider_add_provider (self->meta_provider, ND_PROVIDER (dummy_provider));
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

  portal = nd_screencast_portal_new ();
  g_async_initable_init_async (G_ASYNC_INITABLE (portal),
                               G_PRIORITY_LOW,
                               self->cancellable,
                               nd_screencast_portal_init_async_cb,
                               self);

  pulse = nd_pulseaudio_new ();
  g_async_initable_init_async (G_ASYNC_INITABLE (pulse),
                               G_PRIORITY_LOW,
                               self->cancellable,
                               nd_pulseaudio_init_async_cb,
                               self);
}
