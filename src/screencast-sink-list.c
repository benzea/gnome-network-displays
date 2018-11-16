/* screencast-sink-list.c
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
#include "screencast-sink-list.h"
#include "screencast-sink.h"
#include "screencast-sink-row.h"

struct _ScreencastSinkList
{
  GtkListBox          parent_instance;

  ScreencastProvider *provider;
};

enum {
  PROP_PROVIDER = 1,
  PROP_LAST,
};

G_DEFINE_TYPE (ScreencastSinkList, screencast_sink_list, GTK_TYPE_LIST_BOX)

static GParamSpec * props[PROP_LAST] = { NULL, };


static void
sink_added_cb (ScreencastSinkList *sink_list,
               ScreencastSink     *sink,
               ScreencastProvider *provider)
{
  ScreencastSinkRow *sink_row;

  g_debug ("SinkList: Adding a sink");
  sink_row = screencast_sink_row_new (sink);
  gtk_container_add (GTK_CONTAINER (sink_list), GTK_WIDGET (sink_row));
  gtk_widget_show (GTK_WIDGET (sink_row));
}

static void
foreach_remove_matching_sink (ScreencastSinkRow *sink_row,
                              ScreencastSink    *sink)
{
  if (screencast_sink_row_get_sink (sink_row) == sink)
    gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (sink_row))),
                          GTK_WIDGET (sink_row));
}

static void
sink_removed_cb (ScreencastSinkList *sink_list,
                 ScreencastSink     *sink,
                 ScreencastProvider *provider)
{
  g_debug ("SinkList: Removing a sink");
  gtk_container_foreach (GTK_CONTAINER (sink_list),
                         (GtkCallback) foreach_remove_matching_sink,
                         sink);
}

static void
screencast_sink_list_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ScreencastSinkList *sink_list = SCREENCAST_SINK_LIST (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_object (value, sink_list->provider);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_sink_list_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ScreencastSinkList *sink_list = SCREENCAST_SINK_LIST (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      screencast_sink_list_set_provider (sink_list, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_sink_list_finalize (GObject *object)
{
  ScreencastSinkList *sink_list = SCREENCAST_SINK_LIST (object);

  screencast_sink_list_set_provider (sink_list, NULL);

  G_OBJECT_CLASS (screencast_sink_list_parent_class)->finalize (object);
}

static void
screencast_sink_list_class_init (ScreencastSinkListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = screencast_sink_list_get_property;
  object_class->set_property = screencast_sink_list_set_property;
  object_class->finalize = screencast_sink_list_finalize;

  props[PROP_PROVIDER] =
    g_param_spec_object ("provider", "The sink provider",
                         "The sink provider (usually a MetaProvider) that finds the available sinks.",
                         SCREENCAST_TYPE_PROVIDER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);
}

static void
screencast_sink_list_init (ScreencastSinkList *self)
{
}

/**
 * screencast_sink_list_get_provider
 * @sink_list: a #ScreencastSinkList
 *
 * Retrieve the sink provider that is used to populate the sink list.
 *
 * Returns: (transfer none): The sink provider
 */
ScreencastProvider *
screencast_sink_list_get_provider (ScreencastSinkList *sink_list)
{
  return sink_list->provider;
}

/**
 * screencast_sink_list_set_provider
 * @sink_list: a #ScreencastSinkList
 *
 * Retrieve the sink provider that is used to populate the sink list.
 */
void
screencast_sink_list_set_provider (ScreencastSinkList *sink_list,
                                   ScreencastProvider *provider)
{
  if (sink_list->provider)
    {
      g_signal_handlers_disconnect_by_data (sink_list->provider, sink_list);
      g_clear_object (&sink_list->provider);
    }

  if (provider)
    {
      sink_list->provider = g_object_ref (provider);

      g_signal_connect_object (sink_list->provider,
                               "sink-added",
                               (GCallback) sink_added_cb,
                               sink_list,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (sink_list->provider,
                               "sink-removed",
                               (GCallback) sink_removed_cb,
                               sink_list,
                               G_CONNECT_SWAPPED);

    }
}

ScreencastSinkList *
screencast_sink_list_new (ScreencastProvider *provider)
{
  return g_object_new (SCREENCAST_TYPE_SINK_LIST,
                       "provider", provider,
                       NULL);
}
