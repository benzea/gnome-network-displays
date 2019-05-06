/* nd-wfd-p2p-registry.h
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

#include "nd-meta-provider.h"

G_BEGIN_DECLS

#define ND_TYPE_WFD_P2P_REGISTRY (nd_wfd_p2p_registry_get_type ())
G_DECLARE_FINAL_TYPE (NdWFDP2PRegistry, nd_wfd_p2p_registry, ND, WFD_P2P_REGISTRY, GObject)

NdWFDP2PRegistry * nd_wfd_p2p_registry_new (NdMetaProvider * meta_provider);

G_END_DECLS
