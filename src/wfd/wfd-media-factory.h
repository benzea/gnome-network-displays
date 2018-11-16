#pragma once

#include <gst/rtsp-server/rtsp-media-factory.h>
#include "wfd-video-codec.h"

G_BEGIN_DECLS

#define WFD_TYPE_MEDIA_FACTORY (wfd_media_factory_get_type ())

G_DECLARE_FINAL_TYPE (WfdMediaFactory, wfd_media_factory, WFD, MEDIA_FACTORY, GstRTSPMediaFactory)

WfdMediaFactory * wfd_media_factory_new (void);

/* Just because it is convenient to have next to the pipeline creation code */
void wfd_configure_media_element (GstBin        *bin,
                                  WfdVideoCodec *codec,
                                  WfdResolution *resolution);

G_END_DECLS
