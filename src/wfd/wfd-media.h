#pragma once

#include <gst/rtsp-server/rtsp-media.h>

G_BEGIN_DECLS

#define WFD_TYPE_MEDIA (wfd_media_get_type ())

G_DECLARE_FINAL_TYPE (WfdMedia, wfd_media, WFD, MEDIA, GstRTSPMedia)

WfdMedia * wfd_media_new (void);

G_END_DECLS
