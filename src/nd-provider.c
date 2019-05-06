/* nd-provider.c
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

#include "gnome-network-displays-config.h"
#include "nd-provider.h"

typedef NdProviderIface NdProviderInterface;
G_DEFINE_INTERFACE (NdProvider, nd_provider, G_TYPE_OBJECT);

static void
nd_provider_default_init (NdProviderIface *iface)
{
  /**
   * NdProvider::sink-added:
   * @provider: the #NdProvider on which the signal is emitted
   * @sink: the #NdSink that was added
   *
   * The ::sink-added signal is emitted whenever a new sink is added
   * to the provider.
   */
  g_signal_new ("sink-added",
                ND_TYPE_PROVIDER,
                0,
                G_STRUCT_OFFSET (NdProviderIface, sink_added),
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1, ND_TYPE_SINK);

  /**
   * NdProvider::sink-removed:
   * @provider: the #NdProvider on which the signal is emitted
   * @sink: the #NdSink that was removed
   *
   * The ::sink-added signal is emitted whenever a sink is removed
   * from the provider.
   */
  g_signal_new ("sink-removed",
                ND_TYPE_PROVIDER,
                0,
                G_STRUCT_OFFSET (NdProviderIface, sink_removed),
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1, ND_TYPE_SINK);

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("discover",
                                                             "Discover",
                                                             "Whether discovery is turned on",
                                                             TRUE,
                                                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * nd_provider_get_sinks
 * @provider: the #NdProvider
 *
 * Retrieve a list of all sinks known to the provider.
 *
 * Returns: (element-type NdSink) (transfer container):
 *   A newly allocated list of sinks. Free with g_list_free().
 */
GList *
nd_provider_get_sinks (NdProvider *provider)
{
  NdProviderIface *iface = ND_PROVIDER_GET_IFACE (provider);

  return iface->get_sinks (provider);
}
