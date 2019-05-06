/* nd-sink-list.h
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
#include "nd-provider.h"

G_BEGIN_DECLS

#define ND_TYPE_SINK_LIST (nd_sink_list_get_type ())
G_DECLARE_FINAL_TYPE (NdSinkList, nd_sink_list, ND, SINK_LIST, GtkListBox)

NdSinkList * nd_sink_list_new (NdProvider * provider);

void nd_sink_list_set_provider (NdSinkList *sink_list,
                                NdProvider *provider);
NdProvider *nd_sink_list_get_provider (NdSinkList *sink_list);

G_END_DECLS
