#include "wfd-audio-codec.h"

G_DEFINE_BOXED_TYPE (WfdAudioCodec, wfd_audio_codec, wfd_audio_codec_ref, wfd_audio_codec_unref)

/**
 * wfd_audio_codec_new:
 *
 * Creates a new #WfdAudioCodec.
 *
 * Returns: (transfer full): A newly created #WfdAudioCodec
 */
WfdAudioCodec *
wfd_audio_codec_new (void)
{
  WfdAudioCodec *self;

  self = g_slice_new0 (WfdAudioCodec);
  self->ref_count = 1;

  return self;
}

/**
 * wfd_audio_codec_copy:
 * @self: a #WfdAudioCodec
 *
 * Makes a deep copy of a #WfdAudioCodec.
 *
 * Returns: (transfer full): A newly created #WfdAudioCodec with the same
 *   contents as @self
 */
WfdAudioCodec *
wfd_audio_codec_copy (WfdAudioCodec *self)
{
  WfdAudioCodec *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = wfd_audio_codec_new ();

  return copy;
}

static void
wfd_audio_codec_free (WfdAudioCodec *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_slice_free (WfdAudioCodec, self);
}

/**
 * wfd_audio_codec_ref:
 * @self: A #WfdAudioCodec
 *
 * Increments the reference count of @self by one.
 *
 * Returns: (transfer none): @self
 */
WfdAudioCodec *
wfd_audio_codec_ref (WfdAudioCodec *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * wfd_audio_codec_unref:
 * @self: (transfer none): A #WfdAudioCodec
 *
 * Decrements the reference count of @self by one, freeing the structure when
 * the reference count reaches zero.
 */
void
wfd_audio_codec_unref (WfdAudioCodec *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    wfd_audio_codec_free (self);
}

WfdAudioCodec *
wfd_audio_codec_new_from_desc (const gchar *descr)
{
  g_autoptr(WfdAudioCodec) res = NULL;
  g_auto(GStrv) tokens = NULL;

  tokens = g_strsplit (descr, " ", 3);

  if (g_strv_length (tokens) < 3)
    return NULL;

  res = wfd_audio_codec_new ();

  if (g_str_equal (tokens[0], "LPCM"))
    res->type = WFD_AUDIO_LPCM;
  else if (g_str_equal (tokens[0], "AAC"))
    res->type = WFD_AUDIO_AAC;
  else if (g_str_equal (tokens[0], "AC3"))
    res->type = WFD_AUDIO_AC3;
  else
    return NULL;

  res->modes = g_ascii_strtoll (tokens[1], NULL, 16);

  res->latency_ms = g_ascii_strtoull (tokens[2], NULL, 16) * 5;

  return g_steal_pointer (&res);
}

gchar *
wfd_audio_get_descriptor (WfdAudioCodec *self)
{
  const gchar *type;

  if (self == NULL)
    return g_strdup ("none");

  switch (self->type)
    {
    case WFD_AUDIO_LPCM:
      type = "LPCM";
      break;

    case WFD_AUDIO_AAC:
      type = "AAC";
      break;

    case WFD_AUDIO_AC3:
      type = "AC3";
      break;

    default:
      g_assert_not_reached ();
      return g_strdup ("none");
    }

  return g_strdup_printf ("%s %08X %02X", type, self->modes, 0);
}


void
wfd_audio_codec_dump (WfdAudioCodec *self)
{
  const gchar *type = "invalid";

  switch (self->type)
    {
    case WFD_AUDIO_LPCM:
      type = "LPCM";
      break;

    case WFD_AUDIO_AAC:
      type = "AAC";
      break;

    case WFD_AUDIO_AC3:
      type = "AC3";
      break;

    default:
      g_assert_not_reached ();
    }

  g_debug ("WfdAudioCodec: %s, %" G_GUINT32_FORMAT ", latency: %d", type, self->modes, self->latency_ms);
}
