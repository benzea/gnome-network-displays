/* nd-wfd-p2p-sink.h
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
#include "nd-sink.h"

G_BEGIN_DECLS

#define ND_TYPE_WFD_P2P_SINK (nd_wfd_p2p_sink_get_type ())
G_DECLARE_FINAL_TYPE (NdWFDP2PSink, nd_wfd_p2p_sink, ND, WFD_P2P_SINK, GObject)

NdWFDP2PSink * nd_wfd_p2p_sink_new (NMClient * client,
                                    NMDevice * device,
                                    NMWifiP2PPeer * peer);

NMClient *             nd_wfd_p2p_sink_get_client (NdWFDP2PSink * sink);
NMDevice *             nd_wfd_p2p_sink_get_device (NdWFDP2PSink * sink);
NMWifiP2PPeer *        nd_wfd_p2p_sink_get_peer (NdWFDP2PSink * sink);


G_END_DECLS
