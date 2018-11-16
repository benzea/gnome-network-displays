/* screencast-sink-row.h
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

#include <gtk/gtk.h>
#include "screencast-sink.h"

G_BEGIN_DECLS

#define SCREENCAST_TYPE_SINK_ROW (screencast_sink_row_get_type ())
G_DECLARE_FINAL_TYPE (ScreencastSinkRow, screencast_sink_row, SCREENCAST, SINK_ROW, GtkListBoxRow)

ScreencastSinkRow * screencast_sink_row_new (ScreencastSink * sink);

ScreencastSink * screencast_sink_row_get_sink (ScreencastSinkRow *sink_row);

G_END_DECLS
