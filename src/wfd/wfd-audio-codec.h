#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define WFD_TYPE_AUDIO_CODEC (wfd_audio_codec_get_type ())

typedef enum {
  WFD_AUDIO_LPCM,
  WFD_AUDIO_AAC,
  WFD_AUDIO_AC3,
} WfdAudioCodecType;

typedef struct _WfdAudioCodec WfdAudioCodec;

struct _WfdAudioCodec
{
  WfdAudioCodecType type;

  guint32           modes;
  guint             latency_ms;

  /*< private >*/
  guint ref_count;
};

GType              wfd_audio_codec_get_type (void) G_GNUC_CONST;
WfdAudioCodec     *wfd_audio_codec_new (void);
WfdAudioCodec     *wfd_audio_codec_copy (WfdAudioCodec *self);
WfdAudioCodec     *wfd_audio_codec_ref (WfdAudioCodec *self);
void               wfd_audio_codec_unref (WfdAudioCodec *self);

WfdAudioCodec     *wfd_audio_codec_new_from_desc (const gchar *descr);
gchar             *wfd_audio_get_descriptor (WfdAudioCodec *self);
void               wfd_audio_codec_dump (WfdAudioCodec *self);


G_DEFINE_AUTOPTR_CLEANUP_FUNC (WfdAudioCodec, wfd_audio_codec_unref)

G_END_DECLS
