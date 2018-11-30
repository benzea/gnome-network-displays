#pragma once

#include "screencast-sink.h"

G_BEGIN_DECLS

#define SCREENCAST_TYPE_DUMMY_WFD_SINK (screencast_dummy_wfd_sink_get_type())

G_DECLARE_FINAL_TYPE (ScreencastDummyWFDSink, screencast_dummy_wfd_sink, SCREENCAST, DUMMY_WFD_SINK, GObject)

ScreencastDummyWFDSink *screencast_dummy_wfd_sink_new (void);

G_END_DECLS
