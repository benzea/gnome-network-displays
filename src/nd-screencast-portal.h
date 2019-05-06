#pragma once

#include <gio/gio.h>
#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define ND_TYPE_SCREENCAST_PORTAL (nd_screencast_portal_get_type ())

G_DECLARE_FINAL_TYPE (NdScreencastPortal, nd_screencast_portal, ND, SCREENCAST_PORTAL, GObject)

NdScreencastPortal * nd_screencast_portal_new (void);

GstElement *nd_screencast_portal_get_source (NdScreencastPortal *self);

G_END_DECLS
