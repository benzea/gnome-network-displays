#pragma once

#include <gio/gio.h>
#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SCREENCAST_TYPE_PULSEAUDIO (screencast_pulseaudio_get_type ())

G_DECLARE_FINAL_TYPE (ScreencastPulseaudio, screencast_pulseaudio, SCREENCAST, PULSEAUDIO, GObject)

ScreencastPulseaudio *screencast_pulseaudio_new (void);

GstElement           *screencast_pulseaudio_get_source (ScreencastPulseaudio *self);

G_END_DECLS
