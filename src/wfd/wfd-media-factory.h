#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <gst/rtsp-server/rtsp-media-factory.h>
#pragma GCC diagnostic pop
#include "wfd-params.h"

G_BEGIN_DECLS

#define WFD_TYPE_MEDIA_FACTORY (wfd_media_factory_get_type ())
G_DECLARE_FINAL_TYPE (WfdMediaFactory, wfd_media_factory, WFD, MEDIA_FACTORY, GstRTSPMediaFactory)

typedef enum {
  WFD_QUIRK_NO_IDR = 0x01,
} WfdMediaQuirks;

WfdMediaFactory * wfd_media_factory_new (void);

gboolean          wfd_get_missing_codecs (GStrv *video,
                                          GStrv *audio);

/* Just because it is convenient to have next to the pipeline creation code */
WfdMediaQuirks wfd_configure_media_element (GstBin    *bin,
                                            WfdParams *params);

G_END_DECLS
