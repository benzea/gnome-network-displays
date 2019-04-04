#pragma once

#include <gst/rtsp-server/rtsp-media-factory.h>
#include "wfd-params.h"

G_BEGIN_DECLS

#define WFD_TYPE_MEDIA_FACTORY (wfd_media_factory_get_type ())
G_DECLARE_FINAL_TYPE (WfdMediaFactory, wfd_media_factory, WFD, MEDIA_FACTORY, GstRTSPMediaFactory)

typedef enum {
  WFD_QUIRK_NO_IDR = 0x01,
} WfdMediaQuirks;

WfdMediaFactory * wfd_media_factory_new (void);

/* Just because it is convenient to have next to the pipeline creation code */
WfdMediaQuirks wfd_configure_media_element (GstBin    *bin,
                                            WfdParams *params);

G_END_DECLS
