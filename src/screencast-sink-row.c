/* screencast-sink-row.c
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
#include "screencast-sink-row.h"

struct _ScreencastSinkRow
{
  GtkListBoxRow   parent_instance;

  ScreencastSink *sink;

  /* Template widgets */
  GtkLabel *name_label;
};

enum {
  PROP_SINK = 1,
  PROP_LAST,
};

G_DEFINE_TYPE (ScreencastSinkRow, screencast_sink_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec * props[PROP_LAST] = { NULL, };


static void
screencast_sink_row_sync (ScreencastSinkRow *sink_row)
{
  g_autofree gchar *display_name = NULL;

  g_object_get (sink_row->sink, "display-name", &display_name, NULL);

  gtk_label_set_text (sink_row->name_label, display_name);
}

static void
screencast_sink_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ScreencastSinkRow *sink_row = SCREENCAST_SINK_ROW (object);

  switch (prop_id)
    {
    case PROP_SINK:
      g_value_set_object (value, sink_row->sink);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_sink_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ScreencastSinkRow *sink_row = SCREENCAST_SINK_ROW (object);

  switch (prop_id)
    {
    case PROP_SINK:
      g_assert (sink_row->sink == NULL);
      sink_row->sink = g_value_dup_object (value);

      g_assert (sink_row->sink != NULL);

      g_signal_connect_object (sink_row->sink,
                               "notify",
                               (GCallback) screencast_sink_row_sync,
                               object,
                               G_CONNECT_SWAPPED);
      screencast_sink_row_sync (sink_row);

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_sink_row_finalize (GObject *object)
{
  ScreencastSinkRow *sink_row = SCREENCAST_SINK_ROW (object);

  g_signal_handlers_disconnect_by_data (sink_row->sink, object);
  g_clear_object (&sink_row->sink);

  G_OBJECT_CLASS (screencast_sink_row_parent_class)->finalize (object);
}

static void
screencast_sink_row_class_init (ScreencastSinkRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/screencast/screencast-sink-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ScreencastSinkRow, name_label);

  object_class->get_property = screencast_sink_row_get_property;
  object_class->set_property = screencast_sink_row_set_property;
  object_class->finalize = screencast_sink_row_finalize;

  props[PROP_SINK] =
    g_param_spec_object ("sink", "Sink",
                         "The sink that the row is representing.",
                         SCREENCAST_TYPE_SINK,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);
}

static void
screencast_sink_row_init (ScreencastSinkRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * screencast_sink_row_new
 * @sink: a #ScreencastSink
 *
 * Creates a new #ScreencastSinkRow for the given #ScreencastSink.
 *
 * Returns:
 *   a newly created #ScreencastSinkRow
 */
ScreencastSinkRow *
screencast_sink_row_new (ScreencastSink *sink)
{
  return g_object_new (SCREENCAST_TYPE_SINK_ROW,
                       "sink", sink,
                       NULL);
}

/**
 * screencast_sink_row_get_sink
 * @sink_row: a #ScreencastSinkRow
 *
 * Retrive the #ScreencastSink for this #ScreencastSinkRow.
 *
 * Returns: (transfer none):
 *   the sink for this row
 */
ScreencastSink *
screencast_sink_row_get_sink (ScreencastSinkRow *sink_row)
{
  return sink_row->sink;
}
