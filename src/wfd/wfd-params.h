#pragma once

#include <glib-object.h>
#include "wfd-video-codec.h"
#include "wfd-audio-codec.h"

G_BEGIN_DECLS

#define WFD_TYPE_PARAMS (wfd_params_get_type ())

struct _WfdParams
{
  /*< public >*/
  gchar         *profile;
  guint16        primary_rtp_port;
  guint16        secondary_rtp_port;
  GByteArray    *edid;

  gboolean       idr_request_capability;
  gboolean       ms_cursor_capability;
  guint16        ms_cursor_width;
  guint16        ms_cursor_height;
  guint16        ms_cursor_port;

  WfdVideoCodec *selected_codec;
  WfdResolution *selected_resolution;
  WfdAudioCodec *selected_audio_codec;

  GPtrArray     *video_codecs;
  GPtrArray     *audio_codecs;
};

typedef struct _WfdParams WfdParams;

GType          wfd_params_get_type (void) G_GNUC_CONST;
WfdParams     *wfd_params_new (void);
WfdParams     *wfd_params_copy (WfdParams *self);
void           wfd_params_free (WfdParams *self);

gchar         *wfd_params_m3_query_params (WfdParams *self);
void           wfd_params_from_sink (WfdParams    *self,
                                     const guint8 *body,
                                     gsize         body_size);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WfdParams, wfd_params_free)

G_END_DECLS
