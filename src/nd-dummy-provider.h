#pragma once

#include <nd-provider.h>

G_BEGIN_DECLS

#define ND_TYPE_DUMMY_PROVIDER (nd_dummy_provider_get_type ())

G_DECLARE_FINAL_TYPE (NdDummyProvider, nd_dummy_provider, ND, DUMMY_PROVIDER, GObject)

NdDummyProvider * nd_dummy_provider_new (void);

G_END_DECLS
