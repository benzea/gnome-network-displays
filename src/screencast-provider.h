/* screencast-provider.h
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
#include "screencast-sink.h"

G_BEGIN_DECLS

#define SCREENCAST_TYPE_PROVIDER (screencast_provider_get_type ())
#define SCREENCAST_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCREENCAST_TYPE_PROVIDER, ScreencastProvider))
#define SCREENCAST_IS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCREENCAST_TYPE_PROVIDER))
#define SCREENCAST_PROVIDER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SCREENCAST_TYPE_PROVIDER, ScreencastProviderIface))

typedef struct _ScreencastProvider      ScreencastProvider;      /* dummy typedef */
typedef struct _ScreencastProviderIface ScreencastProviderIface;

struct _ScreencastProviderIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  /* basics */
  GList * (*get_sinks) (ScreencastProvider *provider);


  /* signals */
  void (* sink_added) (ScreencastProvider *provider,
                       ScreencastSink     *sink);
  void (* sink_removed) (ScreencastProvider *provider,
                         ScreencastSink     *sink);
};

GType screencast_provider_get_type (void) G_GNUC_CONST;

GList * screencast_provider_get_sinks (ScreencastProvider *provider);

G_END_DECLS
