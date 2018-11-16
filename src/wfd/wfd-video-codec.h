#pragma once

#include <glib-object.h>
#include "wfd-resolution.h"

G_BEGIN_DECLS

#define WFD_TYPE_VIDEO_CODEC (wfd_video_codec_get_type ())

typedef struct _WfdVideoCodec WfdVideoCodec;

typedef enum {
  WFD_H264_PROFILE_BASE = 0x01,
  WFD_H264_PROFILE_HIGH = 0x02,
} WfdH264ProfileFlags;


struct _WfdVideoCodec
{
  /*< public >*/
  WfdH264ProfileFlags profile;
  guint8              level;
  guint8              latency;
  gint                frame_skipping_allowed;

  /*< private >*/
  WfdResolution *native;

  guint32        cea_sup;
  guint32        vesa_sup;
  guint32        hh_sup;

  guint32        min_slice_size;
  gint           max_slice_num        : 10;
  guint16        max_slice_size_ratio : 3;

  guint          ref_count;
};

GType              wfd_video_codec_get_type (void) G_GNUC_CONST;
WfdVideoCodec     *wfd_video_codec_new (void);
WfdVideoCodec     *wfd_video_codec_copy (WfdVideoCodec *self);
WfdVideoCodec     *wfd_video_codec_ref (WfdVideoCodec *self);
void               wfd_video_codec_unref (WfdVideoCodec *self);

WfdVideoCodec     *wfd_video_codec_new_from_desc (gint         native,
                                                  const gchar *descr);

guint32            wfd_video_codec_get_max_bitrate_kbit (WfdVideoCodec *self);

GList             *wfd_video_codec_get_resolutions (WfdVideoCodec *self);
gchar             *wfd_video_codec_get_descriptor_for_resolution (WfdVideoCodec       *self,
                                                                  const WfdResolution *resolution);

void               wfd_video_codec_dump (WfdVideoCodec *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WfdVideoCodec, wfd_video_codec_unref)

G_END_DECLS
