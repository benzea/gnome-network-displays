/* screencast-sink.c
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
#include "screencast-sink.h"

typedef ScreencastSinkIface ScreencastSinkInterface;
G_DEFINE_INTERFACE (ScreencastSink, screencast_sink, G_TYPE_OBJECT);

static void
screencast_sink_default_init (ScreencastSinkIface *iface)
{
  iface->start_stream = NULL;

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("display-name",
                                                            "Display Name",
                                                            "The name of the sink to display to the user",
                                                            NULL,
                                                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("matches",
                                                           "Match strings",
                                                           "One or more strings that uniquely identify the sink. This is used for de-duplication and should never change over the lifetime of the object.",
                                                           G_TYPE_PTR_ARRAY,
                                                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_int ("priority",
                                                         "Sink Priority",
                                                         "The priority of this sink for de-duplication pruposes (a higher priority will be the prefered method).",
                                                         G_MININT,
                                                         G_MAXINT,
                                                         0,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("state",
                                                          "Sink State",
                                                          "The current state of the sink.",
                                                          SCREENCAST_TYPE_SINK_STATE,
                                                          SCREENCAST_SINK_STATE_DISCONNECTED,
                                                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/**
 * screencast_sink_start_stream
 * @sink: the #ScreencastSink
 * @portal: the #ScreencastPortal
 *
 * Start streaming to this sink. The returned sink is the one
 * that is actually streaming, and may differ from the one this
 * is called on (i.e. if this is a MetaSink grouping multiple sinks).
 *
 * The portal will be used to create the GStreamer source element.
 *
 * Returns: (transfer full):
 *   The streaming sink owned by the caller.
 */
ScreencastSink *
screencast_sink_start_stream (ScreencastSink *sink, ScreencastPortal *portal)
{
  ScreencastSinkIface *iface = SCREENCAST_SINK_GET_IFACE (sink);

  return iface->start_stream (sink, portal);
}
