/* screencast-sink.h
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
#include "screencast-enum-types.h"
#include "screencast-portal.h"

G_BEGIN_DECLS

#define SCREENCAST_TYPE_SINK (screencast_sink_get_type ())
#define SCREENCAST_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCREENCAST_TYPE_SINK, ScreencastSink))
#define SCREENCAST_IS_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCREENCAST_TYPE_SINK))
#define SCREENCAST_SINK_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SCREENCAST_TYPE_SINK, ScreencastSinkIface))

typedef struct _ScreencastSink      ScreencastSink;      /* dummy typedef */
typedef struct _ScreencastSinkIface ScreencastSinkIface;

typedef enum {
  SCREENCAST_SINK_STATE_DISCONNECTED       =      0x0,
  SCREENCAST_SINK_STATE_WAIT_P2P           =    0x100,
  SCREENCAST_SINK_STATE_WAIT_SOCKET        =    0x110,
  SCREENCAST_SINK_STATE_WAIT_STREAMING     =    0x120,
  SCREENCAST_SINK_STATE_STREAMING          =   0x1000,
  SCREENCAST_SINK_STATE_ERROR              =  0x10000,
} ScreencastSinkState;

struct _ScreencastSinkIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  ScreencastSink * (* start_stream) (ScreencastSink *sink, ScreencastPortal *portal);
};

GType screencast_sink_get_type (void) G_GNUC_CONST;

ScreencastSink *screencast_sink_start_stream (ScreencastSink *sink, ScreencastPortal *portal);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ScreencastSink, g_object_unref)

G_END_DECLS
