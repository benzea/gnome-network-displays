#pragma once

#include <gio/gio.h>
#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SCREENCAST_TYPE_PORTAL (screencast_portal_get_type ())

G_DECLARE_FINAL_TYPE (ScreencastPortal, screencast_portal, SCREENCAST, PORTAL, GObject)

ScreencastPortal * screencast_portal_new (void);

GstElement *screencast_portal_get_source (ScreencastPortal *self);

G_END_DECLS
