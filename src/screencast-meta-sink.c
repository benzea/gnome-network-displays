/* screencast-meta-sink.c
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
#include "screencast-meta-sink.h"

struct _ScreencastMetaSink
{
  GObject         parent_instance;

  ScreencastSink *current_sink;
  GPtrArray      *sinks;
};

enum {
  PROP_SINK = 1,
  PROP_SINKS,

  PROP_DISPLAY_NAME,
  PROP_MATCHES,
  PROP_PRIORITY,
  PROP_STATE,

  PROP_LAST = PROP_DISPLAY_NAME,
};

static void screencast_meta_sink_sink_iface_init (ScreencastSinkIface *iface);
static ScreencastSink * screencast_meta_sink_sink_start_stream (ScreencastSink *sink);
static void screencast_meta_sink_sink_stop_stream (ScreencastSink *sink);

G_DEFINE_TYPE_EXTENDED (ScreencastMetaSink, screencast_meta_sink, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SCREENCAST_TYPE_SINK,
                                               screencast_meta_sink_sink_iface_init);
                       )

static GParamSpec * props[PROP_LAST] = { NULL, };

static void
screencast_meta_sink_notify_sink_cb (ScreencastMetaSink *meta_sink, GParamSpec *pspec, ScreencastSink *sink)
{
  /* Propage the cb if this MetaSink also has a property of the same name */
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (meta_sink), pspec->name))
    g_object_notify (G_OBJECT (meta_sink), pspec->name);
}

static void
screencast_meta_sink_update (ScreencastMetaSink *meta_sink)
{
  gint i;
  gint best_priority = G_MININT;
  ScreencastSink *best_sink = NULL;

  for (i = 0; i < meta_sink->sinks->len; i++)
    {
      ScreencastSink *sink;
      gint priority;
      sink = g_ptr_array_index (meta_sink->sinks, i);

      g_object_get (sink, "priority", &priority, NULL);
      if (priority == best_priority)
        g_debug ("MetaSink: Found two sinks with identical priority! Prefered order is undefined.\n");

      if (priority > best_priority)
        best_sink = sink;
    }

  /* Nothing has changed */
  if (best_sink == meta_sink->current_sink)
    return;

  if (meta_sink->current_sink)
    {
      g_signal_handlers_disconnect_by_data (meta_sink->current_sink, meta_sink);
      g_clear_object (&meta_sink->current_sink);
    }

  if (best_sink)
    {
      meta_sink->current_sink = g_object_ref (best_sink);
      g_signal_connect_object (meta_sink->current_sink,
                               "notify", (GCallback) screencast_meta_sink_notify_sink_cb,
                               meta_sink, G_CONNECT_SWAPPED);
    }
  else
    {
      g_debug ("MetaSink: No usable sink is left, object has become invalid.");
    }

  /* Notify the pass-through properties */
  g_object_notify (G_OBJECT (meta_sink), "display-name");
  g_object_notify (G_OBJECT (meta_sink), "priority");
  g_object_notify (G_OBJECT (meta_sink), "state");
}

static void
screencast_meta_sink_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ScreencastMetaSink *meta_sink = SCREENCAST_META_SINK (object);

  switch (prop_id)
    {
    case PROP_SINK:
      g_value_set_object (value, meta_sink->current_sink);
      break;

    case PROP_SINKS:
      g_value_set_boxed (value, meta_sink->sinks);
      break;

    case PROP_DISPLAY_NAME:
      if (meta_sink->current_sink)
        g_object_get_property (G_OBJECT (meta_sink->current_sink), pspec->name, value);
      else
        g_value_set_static_string (value, NULL);
      break;

    case PROP_MATCHES:
      {
        g_autoptr(GPtrArray) res = NULL;
        res = g_ptr_array_new_with_free_func (g_free);

        for (gint i = 0; i < meta_sink->sinks->len; i++)
          {
            g_autoptr(GPtrArray) sub_matches = NULL;

            g_object_get (g_ptr_array_index (meta_sink->sinks, i),
                          "matches", &sub_matches,
                          NULL);

            for (gint j = 0; j < sub_matches->len; j++)
              if (!g_ptr_array_find_with_equal_func (res, g_ptr_array_index (sub_matches, j), g_str_equal, NULL))
                g_ptr_array_add (res, g_strdup (g_ptr_array_index (sub_matches, j)));
          }

        g_value_take_boxed (value, g_steal_pointer (&res));
        break;
      }

    case PROP_PRIORITY:
      if (meta_sink->current_sink)
        g_object_get_property (G_OBJECT (meta_sink->current_sink), pspec->name, value);
      else
        g_value_set_int (value, 0);
      break;

    case PROP_STATE:
      if (meta_sink->current_sink)
        g_object_get_property (G_OBJECT (meta_sink->current_sink), pspec->name, value);
      else
        g_value_set_enum (value, SCREENCAST_SINK_STATE_DISCONNECTED);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_meta_sink_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ScreencastMetaSink *meta_sink = SCREENCAST_META_SINK (object);

  switch (prop_id)
    {
    case PROP_SINK:
      screencast_meta_sink_add_sink (meta_sink, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
screencast_meta_sink_finalize (GObject *object)
{
  ScreencastMetaSink *meta_sink = SCREENCAST_META_SINK (object);

  g_ptr_array_free (meta_sink->sinks, TRUE);
  meta_sink->sinks = NULL;

  g_signal_handlers_disconnect_by_data (meta_sink->current_sink, meta_sink);
  g_clear_object (&meta_sink->current_sink);

  G_OBJECT_CLASS (screencast_meta_sink_parent_class)->finalize (object);
}

static void
screencast_meta_sink_class_init (ScreencastMetaSinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = screencast_meta_sink_get_property;
  object_class->set_property = screencast_meta_sink_set_property;
  object_class->finalize = screencast_meta_sink_finalize;

  props[PROP_SINK] =
    g_param_spec_object ("sink", "sink",
                         "The currently selected sink. Writing the property results in the sink to be added (but not necessarily selected).",
                         SCREENCAST_TYPE_SINK,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_SINKS] =
    g_param_spec_boxed ("sinks", "sinks",
                        "All sinks that are grouped into the meta sink.",
                        G_TYPE_PTR_ARRAY,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

  g_object_class_override_property (object_class, PROP_DISPLAY_NAME, "display-name");
  g_object_class_override_property (object_class, PROP_MATCHES, "matches");
  g_object_class_override_property (object_class, PROP_PRIORITY, "priority");
  g_object_class_override_property (object_class, PROP_STATE, "state");
}

static void
screencast_meta_sink_init (ScreencastMetaSink *meta_sink)
{
  meta_sink->sinks = g_ptr_array_new_with_free_func (g_object_unref);
}

/******************************************************************
* ScreencastSink interface implementation
******************************************************************/

static void
screencast_meta_sink_sink_iface_init (ScreencastSinkIface *iface)
{
  iface->start_stream = screencast_meta_sink_sink_start_stream;
  iface->stop_stream = screencast_meta_sink_sink_stop_stream;
}

static ScreencastSink *
screencast_meta_sink_sink_start_stream (ScreencastSink *sink)
{
  ScreencastMetaSink *meta_sink = SCREENCAST_META_SINK (sink);

  g_assert (meta_sink->current_sink);

  return screencast_sink_start_stream (meta_sink->current_sink);
}

static void
screencast_meta_sink_sink_stop_stream (ScreencastSink *sink)
{
  /* This must not happen. */
  g_assert_not_reached ();
}

/******************************************************************
* ScreencastMetaSink public functions
******************************************************************/

/**
 * screencast_meta_sink_get_sink
 * @meta_sink: a #ScreencastMetaSink
 *
 * Retrieve the currently selected sink for this meta sink.
 *
 * Returns: (transfer none): The selected sink
 */
ScreencastSink *
screencast_meta_sink_get_sink (ScreencastMetaSink *meta_sink)
{
  return meta_sink->current_sink;
}

/**
 * screencast_meta_sink_add_sink
 * @meta_sink: a #ScreencastMetaSink
 * @sink: a #ScreencastSink
 *
 * Adds the sink to the list of known sinks.
 */
void
screencast_meta_sink_add_sink (ScreencastMetaSink *meta_sink,
                               ScreencastSink     *sink)
{
  g_assert (!g_ptr_array_find (meta_sink->sinks, sink, NULL));

  g_ptr_array_add (meta_sink->sinks, g_object_ref (sink));

  screencast_meta_sink_update (meta_sink);

  g_object_notify_by_pspec (G_OBJECT (meta_sink), props[PROP_SINKS]);
  g_object_notify (G_OBJECT (meta_sink), "matches");
}

/**
 * screencast_meta_sink_remove_sink
 * @meta_sink: a #ScreencastMetaSink
 * @sink: a #ScreencastSink
 *
 * Remove the given sink from the meta sink. If the last child
 * sink has been removed, then the metasink becomes invalid.
 *
 * Returns:
 *   #TRUE if no sinks are left and the #ScreencastMetaSink has become invalid.
 */
gboolean
screencast_meta_sink_remove_sink (ScreencastMetaSink *meta_sink,
                                  ScreencastSink     *sink)
{
  g_assert (g_ptr_array_remove (meta_sink->sinks, sink));

  screencast_meta_sink_update (meta_sink);

  g_object_notify_by_pspec (G_OBJECT (meta_sink), props[PROP_SINKS]);
  g_object_notify (G_OBJECT (meta_sink), "matches");

  return meta_sink->sinks->len <= 0;
}

ScreencastMetaSink *
screencast_meta_sink_new (ScreencastSink *sink)
{
  return g_object_new (SCREENCAST_TYPE_META_SINK,
                       "sink", sink,
                       NULL);
}
