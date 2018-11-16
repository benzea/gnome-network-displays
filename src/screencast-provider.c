/* screencast-provider.c
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
#include "screencast-provider.h"

typedef ScreencastProviderIface ScreencastProviderInterface;
G_DEFINE_INTERFACE (ScreencastProvider, screencast_provider, G_TYPE_OBJECT);

static void
screencast_provider_default_init (ScreencastProviderIface *iface)
{
  /**
   * ScreencastProvider::sink-added:
   * @provider: the #ScreencastProvider on which the signal is emitted
   * @sink: the #ScreencastSink that was added
   *
   * The ::sink-added signal is emitted whenever a new sink is added
   * to the provider.
   */
  g_signal_new ("sink-added",
                SCREENCAST_TYPE_PROVIDER,
                0,
                G_STRUCT_OFFSET (ScreencastProviderIface, sink_added),
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1, SCREENCAST_TYPE_SINK);

  /**
   * ScreencastProvider::sink-removed:
   * @provider: the #ScreencastProvider on which the signal is emitted
   * @sink: the #ScreencastSink that was removed
   *
   * The ::sink-added signal is emitted whenever a sink is removed
   * from the provider.
   */
  g_signal_new ("sink-removed",
                SCREENCAST_TYPE_PROVIDER,
                0,
                G_STRUCT_OFFSET (ScreencastProviderIface, sink_removed),
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1, SCREENCAST_TYPE_SINK);
}

/**
 * screencast_provider_get_sinks
 * @provider: the #ScreencastProvider
 *
 * Retrieve a list of all sinks known to the provider.
 *
 * Returns: (element-type ScreencastSink) (transfer container):
 *   A newly allocated list of sinks. Free with g_list_free().
 */
GList *
screencast_provider_get_sinks (ScreencastProvider *provider)
{
  ScreencastProviderIface *iface = SCREENCAST_PROVIDER_GET_IFACE (provider);

  return iface->get_sinks (provider);
}
