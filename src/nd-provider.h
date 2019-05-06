/* nd-provider.h
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
#include "nd-sink.h"

G_BEGIN_DECLS

#define ND_TYPE_PROVIDER (nd_provider_get_type ())
#define ND_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ND_TYPE_PROVIDER, NdProvider))
#define ND_IS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ND_TYPE_PROVIDER))
#define ND_PROVIDER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ND_TYPE_PROVIDER, NdProviderIface))

typedef struct _NdProvider      NdProvider;      /* dummy typedef */
typedef struct _NdProviderIface NdProviderIface;

struct _NdProviderIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  /* basics */
  GList * (*get_sinks) (NdProvider *provider);


  /* signals */
  void (* sink_added) (NdProvider *provider,
                       NdSink     *sink);
  void (* sink_removed) (NdProvider *provider,
                         NdSink     *sink);
};

GType nd_provider_get_type (void) G_GNUC_CONST;

GList * nd_provider_get_sinks (NdProvider *provider);

G_END_DECLS
