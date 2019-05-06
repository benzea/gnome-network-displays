/* nd-sink.h
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

#include <glib-object.h>
#include "nd-enum-types.h"

G_BEGIN_DECLS

#define ND_TYPE_SINK (nd_sink_get_type ())
#define ND_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ND_TYPE_SINK, NdSink))
#define ND_IS_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ND_TYPE_SINK))
#define ND_SINK_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ND_TYPE_SINK, NdSinkIface))

typedef struct _NdSink      NdSink;      /* dummy typedef */
typedef struct _NdSinkIface NdSinkIface;

typedef enum {
  ND_SINK_STATE_DISCONNECTED       =      0x0,
  ND_SINK_STATE_WAIT_P2P           =    0x100,
  ND_SINK_STATE_WAIT_SOCKET        =    0x110,
  ND_SINK_STATE_WAIT_STREAMING     =    0x120,
  ND_SINK_STATE_STREAMING          =   0x1000,
  ND_SINK_STATE_ERROR              =  0x10000,
} NdSinkState;

struct _NdSinkIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  NdSink * (* start_stream) (NdSink *sink);
  void     (* stop_stream)  (NdSink *sink);
};

GType nd_sink_get_type (void) G_GNUC_CONST;

NdSink *nd_sink_start_stream (NdSink *sink);
void            nd_sink_stop_stream (NdSink *sink);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NdSink, g_object_unref)

G_END_DECLS
