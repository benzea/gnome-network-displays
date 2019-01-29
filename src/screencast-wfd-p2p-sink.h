/* screencast-wfd-p2p-sink.h
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
#include <NetworkManager.h>
#include "screencast-sink.h"

G_BEGIN_DECLS

#define SCREENCAST_TYPE_WFD_P2P_SINK (screencast_wfd_p2p_sink_get_type ())
G_DECLARE_FINAL_TYPE (ScreencastWFDP2PSink, screencast_wfd_p2p_sink, SCREENCAST, WFD_P2P_SINK, GObject)

ScreencastWFDP2PSink * screencast_wfd_p2p_sink_new (NMClient * client,
                                                    NMDevice * device,
                                                    NMWifiP2PPeer * peer);

NMClient *             screencast_wfd_p2p_sink_get_client (ScreencastWFDP2PSink * sink);
NMDevice *             screencast_wfd_p2p_sink_get_device (ScreencastWFDP2PSink * sink);
NMWifiP2PPeer *        screencast_wfd_p2p_sink_get_peer (ScreencastWFDP2PSink * sink);


G_END_DECLS
