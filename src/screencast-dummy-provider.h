#pragma once

#include <screencast-provider.h>

G_BEGIN_DECLS

#define SCREENCAST_TYPE_DUMMY_PROVIDER (screencast_dummy_provider_get_type())

G_DECLARE_FINAL_TYPE (ScreencastDummyProvider, screencast_dummy_provider, SCREENCAST, DUMMY_PROVIDER, GObject)

ScreencastDummyProvider *screencast_dummy_provider_new (void);

G_END_DECLS
