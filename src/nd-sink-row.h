/* nd-sink-row.h
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
#include "nd-sink.h"

G_BEGIN_DECLS

#define ND_TYPE_SINK_ROW (nd_sink_row_get_type ())
G_DECLARE_FINAL_TYPE (NdSinkRow, nd_sink_row, ND, SINK_ROW, GtkListBoxRow)

NdSinkRow * nd_sink_row_new (NdSink * sink);

NdSink * nd_sink_row_get_sink (NdSinkRow *sink_row);

G_END_DECLS
