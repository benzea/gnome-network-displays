/* nd-meta-provider.h
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

#include "nd-provider.h"

G_BEGIN_DECLS

#define ND_TYPE_META_PROVIDER (nd_meta_provider_get_type ())
G_DECLARE_FINAL_TYPE (NdMetaProvider, nd_meta_provider, ND, META_PROVIDER, GObject)


NdMetaProvider * nd_meta_provider_new (void);

GList *          nd_meta_provider_get_providers (NdMetaProvider *meta_provider);

void             nd_meta_provider_add_provider (NdMetaProvider *meta_provider,
                                                NdProvider     *provider);
void             nd_meta_provider_remove_provider (NdMetaProvider *meta_provider,
                                                   NdProvider     *provider);


G_END_DECLS
