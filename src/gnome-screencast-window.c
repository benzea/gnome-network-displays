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

#include "gnome-screencast-config.h"
#include "gnome-screencast-window.h"
#include "screencast-sink-list.h"
#include "screencast-sink-row.h"
#include "screencast-meta-provider.h"
#include "screencast-wfd-p2p-registry.h"
#include "screencast-dummy-provider.h"

#include <gst/gst.h>

#if HAVE_SCREENCAST_PORTAL
#include "screencast-portal.h"
#endif

struct _GnomeScreencastWindow
{
  GtkApplicationWindow      parent_instance;

  ScreencastMetaProvider   *meta_provider;
  ScreencastWFDP2PRegistry *wfd_p2p_registry;

#if HAVE_SCREENCAST_PORTAL
  ScreencastPortal *portal;
#endif

  GCancellable    *cancellable;

  /* Template widgets */
  GtkStack           *step_stack;
  ScreencastSinkList *find_sink_list;
};

G_DEFINE_TYPE (GnomeScreencastWindow, gnome_screencast_window, GTK_TYPE_APPLICATION_WINDOW)

static GstElement*
sink_create_source_cb (GnomeScreencastWindow *self, ScreencastSink *sink)
{
  GstBin *bin;
  GstElement *src, *dst, *res;

  bin = GST_BIN (gst_bin_new ("screencast source bin"));
#if HAVE_SCREENCAST_PORTAL
  src = screencast_portal_get_source (self->portal);
#else
  src = gst_element_factory_make ("ximagesrc", "X11 screencast source");
#endif
  gst_bin_add (bin, src);

  dst = gst_element_factory_make ("intervideosink", "inter video sink");
  g_object_set (dst,
                "channel", "screencast-inter-video",
                NULL);
  gst_bin_add (bin, dst);

  gst_element_link_many (src, dst, NULL);

  res = gst_element_factory_make ("intervideosrc", "screencastsrc");
  g_object_set (res,
                "do-timestamp", TRUE,
                "timeout", 10000000000,
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
find_sink_list_row_activated_cb (GnomeScreencastWindow *self, ScreencastSinkRow *row, ScreencastSinkList *sink_list)
{
  ScreencastSink *sink;
  g_autoptr(ScreencastSink) streaming_sink = NULL;

#if HAVE_SCREENCAST_PORTAL
  if (!self->portal)
    {
      g_warning ("Cannot start streaming right now as we don't have a portal!");
      return;
    }
#endif

  g_assert (SCREENCAST_IS_SINK_ROW (row));

  sink = screencast_sink_row_get_sink (row);
  streaming_sink = screencast_sink_start_stream (sink);

  if (streaming_sink)
    {
      g_signal_connect_object (streaming_sink,
                               "create-source",
                               (GCallback) sink_create_source_cb,
                               self,
                               G_CONNECT_SWAPPED);

    }

  /* XXX: leak streaming_sink intentionally for now */
  g_steal_pointer (&streaming_sink);
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
}

#if HAVE_SCREENCAST_PORTAL
static void
screencast_portal_init_async_cb (GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
  GnomeScreencastWindow *window;
  g_autoptr(GError) error = NULL;

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (source_object), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error initing screencast portal: %s", error->message);

          /* XXX: This is fatal! */
          window = GNOME_SCREENCAST_WINDOW (user_data);
          gtk_widget_destroy (GTK_WIDGET (window));
        }

      g_object_unref (source_object);
      return;
    }

  window = GNOME_SCREENCAST_WINDOW (user_data);
  window->portal = SCREENCAST_PORTAL (source_object);
}
#endif

static void
gnome_screencast_window_init (GnomeScreencastWindow *self)
{
#if HAVE_SCREENCAST_PORTAL
  ScreencastPortal *portal;
#endif
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

#if HAVE_SCREENCAST_PORTAL
  portal = screencast_portal_new ();
  g_async_initable_init_async (G_ASYNC_INITABLE (portal),
                               G_PRIORITY_LOW,
                               self->cancellable,
                               screencast_portal_init_async_cb,
                               self);
#endif
}
