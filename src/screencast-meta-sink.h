/* screencast-meta-sink.h
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

#pragma once

#include "screencast-sink.h"

G_BEGIN_DECLS

#define SCREENCAST_TYPE_META_SINK (screencast_meta_sink_get_type ())
G_DECLARE_FINAL_TYPE (ScreencastMetaSink, screencast_meta_sink, SCREENCAST, META_SINK, GObject)

ScreencastMetaSink *  screencast_meta_sink_new (ScreencastSink * sink);

ScreencastSink *      screencast_meta_sink_get_sink (ScreencastMetaSink * meta_sink);
void                  screencast_meta_sink_add_sink (ScreencastMetaSink *meta_sink,
                                                     ScreencastSink     *sink);
gboolean              screencast_meta_sink_remove_sink (ScreencastMetaSink *meta_sink,
                                                        ScreencastSink     *sink);


G_END_DECLS
