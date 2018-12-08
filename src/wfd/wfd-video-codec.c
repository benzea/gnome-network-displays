#include <string.h>
#include "wfd-video-codec.h"

G_DEFINE_BOXED_TYPE (WfdVideoCodec, wfd_video_codec, wfd_video_codec_ref, wfd_video_codec_unref)

static const WfdResolution cea_resolutions[] = {
  { 640,  480, 60, FALSE},
  { 720,  480, 60, FALSE},
  { 720,  480, 60, TRUE},
  { 720,  576, 50, FALSE},
  { 720,  576, 50, TRUE},
  {1280,  720, 30, FALSE},
  {1280,  720, 60, FALSE},
  {1920, 1080, 30, FALSE},
  {1920, 1080, 60, FALSE},
  {1920, 1080, 60, TRUE},
  {1290,  720, 25, FALSE},
  {1280,  720, 50, FALSE},
  {1920, 1080, 25, FALSE},
  {1920, 1080, 50, FALSE},
  {1920, 1080, 50, TRUE},
  {1280,  720, 24, FALSE},
  {1920, 1080, 25, FALSE},
};

static const WfdResolution vesa_resolutions[] = {
  { 800,  600, 30, FALSE},
  { 800,  600, 60, FALSE},
  {1024,  768, 30, FALSE},
  {1024,  768, 60, FALSE},
  {1152,  864, 30, FALSE},
  {1152,  864, 60, FALSE},
  {1280,  768, 30, FALSE},
  {1280,  768, 60, FALSE},
  {1280,  800, 30, FALSE},
  {1280,  800, 60, FALSE},
  {1360,  768, 30, FALSE},
  {1360,  768, 60, FALSE},
  {1366,  768, 30, FALSE},
  {1366,  768, 60, FALSE},
  {1280, 1024, 30, FALSE},
  {1280, 1024, 60, FALSE},
  {1400, 1050, 30, FALSE},
  {1400, 1050, 60, FALSE},
  {1440,  900, 30, FALSE},
  {1440,  900, 60, FALSE},
  {1600,  900, 30, FALSE},
  {1600,  900, 60, FALSE},
  {1600, 1200, 30, FALSE},
  {1600, 1200, 60, FALSE},
  {1680, 1024, 30, FALSE},
  {1680, 1024, 30, FALSE},
  {1680, 1050, 30, FALSE},
  {1680, 1050, 60, FALSE},
  {1920, 1200, 30, FALSE},
};

static const WfdResolution hh_resolutions[] = {
  {800, 480, 30, FALSE},
  {800, 480, 60, FALSE},
  {854, 480, 30, FALSE},
  {854, 480, 60, FALSE},
  {864, 480, 30, FALSE},
  {864, 480, 60, FALSE},
  {640, 360, 30, FALSE},
  {640, 360, 60, FALSE},
  {960, 540, 30, FALSE},
  {960, 540, 60, FALSE},
  {848, 480, 30, FALSE},
  {848, 480, 60, FALSE},
};

enum {
  RESOLUTION_TABLE_CEA   = 0x00,
  RESOLUTION_TABLE_HH    = 0x01,
  RESOLUTION_TABLE_VESA  = 0x02,
};

const WfdResolution *
resolution_table_lookup (gint table, guint offset)
{
  const WfdResolution *resolutions;
  gint length;

  if (table == RESOLUTION_TABLE_CEA)
    {
      resolutions = cea_resolutions;
      length = G_N_ELEMENTS (cea_resolutions);
    }
  else if (table == RESOLUTION_TABLE_HH)
    {
      resolutions = hh_resolutions;
      length = G_N_ELEMENTS (hh_resolutions);
    }
  else if (table == RESOLUTION_TABLE_VESA)
    {
      resolutions = vesa_resolutions;
      length = G_N_ELEMENTS (vesa_resolutions);
    }
  else
    {
      g_return_val_if_reached (NULL);
    }

  if (offset < length)
    return &resolutions[offset];
  else
    return NULL;
}

guint32
sup_for_resolution (gint table, const WfdResolution *resolution)
{
  const WfdResolution *resolutions;
  gint length;

  if (table == RESOLUTION_TABLE_CEA)
    {
      resolutions = cea_resolutions;
      length = G_N_ELEMENTS (cea_resolutions);
    }
  else if (table == RESOLUTION_TABLE_HH)
    {
      resolutions = hh_resolutions;
      length = G_N_ELEMENTS (hh_resolutions);
    }
  else if (table == RESOLUTION_TABLE_VESA)
    {
      resolutions = vesa_resolutions;
      length = G_N_ELEMENTS (hh_resolutions);
    }
  else
    {
      g_return_val_if_reached (0);
    }

  for (gint i = 0; i < length; i++)
    {
      if (resolutions[i].width != resolution->width)
        continue;
      if (resolutions[i].height != resolution->height)
        continue;
      if (resolutions[i].refresh_rate != resolution->refresh_rate)
        continue;
      if (resolutions[i].interlaced != resolution->interlaced)
        continue;

      return 1 << i;
    }

  return 0;
}

/**
 * wfd_video_codec_new:
 *
 * Creates a new #WfdVideoCodec.
 *
 * Returns: (transfer full): A newly created #WfdVideoCodec
 */
WfdVideoCodec *
wfd_video_codec_new (void)
{
  WfdVideoCodec *self;

  self = g_slice_new0 (WfdVideoCodec);
  self->ref_count = 1;

  return self;
}

/**
 * wfd_video_codec_copy:
 * @self: a #WfdVideoCodec
 *
 * Makes a deep copy of a #WfdVideoCodec. The parsed information about
 * supported resolutions is *not* copied.
 *
 * Returns: (transfer full): A newly created #WfdVideoCodec with the same
 *   contents as @self
 */
WfdVideoCodec *
wfd_video_codec_copy (WfdVideoCodec *self)
{
  WfdVideoCodec *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = wfd_video_codec_new ();
  copy->profile = self->profile;
  copy->level   = self->level;
  copy->latency = self->latency;
  copy->native = wfd_resolution_copy (self->native);

  return copy;
}

static void
wfd_video_codec_free (WfdVideoCodec *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_slice_free (WfdVideoCodec, self);
}

/**
 * wfd_video_codec_ref:
 * @self: A #WfdVideoCodec
 *
 * Increments the reference count of @self by one.
 *
 * Returns: (transfer none): @self
 */
WfdVideoCodec *
wfd_video_codec_ref (WfdVideoCodec *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * wfd_video_codec_unref:
 * @self: (transfer none): A #WfdVideoCodec
 *
 * Decrements the reference count of @self by one, freeing the structure when
 * the reference count reaches zero.
 */
void
wfd_video_codec_unref (WfdVideoCodec *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    wfd_video_codec_free (self);
}

/**
 * wfd_video_codec_new_from_descr:
 * @native: Integer describing the native resolution
 * @descr: A video codec descriptor string
 *
 * Parses the given video descriptor description string.
 *
 * Returns: (transfer full): A newly allocated #WfdVideoCodec, or #NULL on failure.
 */
WfdVideoCodec *
wfd_video_codec_new_from_desc (gint native, const gchar *descr)
{
  g_autoptr(WfdVideoCodec) res;
  g_auto(GStrv) tokens = NULL;
  const WfdResolution *native_res;
  guint32 tmp;

  tokens = g_strsplit (descr, " ", 11);

  if (g_strv_length (tokens) < 9)
    return NULL;

  res = wfd_video_codec_new ();

  res->profile = (WfdH264ProfileFlags) g_ascii_strtoll (tokens[0], NULL, 16);
  if (res->profile != WFD_H264_PROFILE_BASE && res->profile != WFD_H264_PROFILE_HIGH)
    {
      g_warning ("Unknown profile 0x%x", res->profile);
      return NULL;
    }

  res->level = g_ascii_strtoll (tokens[1], NULL, 16);
  if (res->level > 255)
    {
      g_warning ("Unreasonable level 0x%x", res->level);
      return NULL;
    }

  res->cea_sup = g_ascii_strtoll (tokens[2], NULL, 16);
  res->vesa_sup = g_ascii_strtoll (tokens[3], NULL, 16);
  res->hh_sup = g_ascii_strtoll (tokens[4], NULL, 16);

  res->latency = g_ascii_strtoll (tokens[5], NULL, 16);

  res->min_slice_size = g_ascii_strtoll (tokens[6], NULL, 16);

  tmp = g_ascii_strtoll (tokens[7], NULL, 16);
  res->max_slice_num = (tmp & 0x1ff) + 1;
  res->max_slice_size_ratio = (tmp >> 10) & 0x7;

  /* frame_rate_ctrl_sup */
  tmp = g_ascii_strtoll (tokens[8], NULL, 16);
  res->frame_skipping_allowed = tmp & 0x1;

  native_res = resolution_table_lookup (native & 0x7, native >> 3);
  if (native_res)
    res->native = wfd_resolution_copy ((WfdResolution*) native_res);

  return g_steal_pointer (&res);
}

/**
 * wfd_video_codec_get_max_bitrate_kbit:
 * @self: a #WfdVideoCodec
 *
 * Returns the maximum bitrate supported by the sink in kbit/s.
 *
 * Returns: (transfer full): The maximum bitrate.
 */
guint32
wfd_video_codec_get_max_bitrate_kbit (WfdVideoCodec *self)
{
  guint32 bitrate = 14000;

  switch (self->level)
    {
    case 1:
      bitrate = 14000;
      break;

    case 1 << 1:
        bitrate = 20000;
      break;

    case 1 << 2:
        bitrate = 20000;
      break;

    case 1 << 3:
        bitrate = 50000;
      break;

    case 1 << 4:
        bitrate = 50000;

    default:
      g_warning ("WfdVideoCodec: Unknown level %i", self->level);
    }

  if (self->profile == WFD_H264_PROFILE_HIGH)
    bitrate = bitrate * 1.25;

  return bitrate;
}

/**
 * wfd_video_codec_get_resolutions:
 * @self: a #WfdVideoCodec
 *
 * Returns a list of supported resolutions
 *
 * Returns: (transfer container) (element-type WfdResolution): A newly allocated #WfdVideoCodec, or #NULL on failure.
 */
GList *
wfd_video_codec_get_resolutions (WfdVideoCodec *self)
{
  WfdResolution *resolution;

  g_autoptr(GList) res = NULL;
  guint32 bits, i;

  i = 0;
  for (bits = self->hh_sup; bits; bits = bits >> 1)
    {
      if (bits & 0x1)
        {
          resolution = (gpointer) resolution_table_lookup (RESOLUTION_TABLE_HH, i);
          if (resolution)
            res = g_list_append (res, resolution);
          else
            g_warning ("Resolution %d not found in table HH, but was in suppliments 0x%X\n", i, self->hh_sup);
        }
      i++;
    }

  i = 0;
  for (bits = self->cea_sup; bits; bits = bits >> 1)
    {
      if (bits & 0x1)
        {
          resolution = (gpointer) resolution_table_lookup (RESOLUTION_TABLE_CEA, i);
          if (resolution)
            res = g_list_append (res, resolution);
          else
            g_warning ("Resolution %d not found in table CAE, but was in suppliments 0x%X\n", i, self->cea_sup);
        }
      i++;
    }

  i = 0;
  for (bits = self->vesa_sup; bits; bits = bits >> 1)
    {
      if (bits & 0x1)
        {
          resolution = (gpointer) resolution_table_lookup (RESOLUTION_TABLE_VESA, i);
          if (resolution)
            res = g_list_append (res, resolution);
          else
            g_warning ("Resolution %d not found in table VESA, but was in suppliments 0x%X\n", i, self->vesa_sup);
        }
      i++;
    }

  return g_steal_pointer (&res);
}

/**
 * wfd_video_codec_get_descriptor_for_resolution:
 * @self: a #WfdVideoCodec
 * @resolution: the #WfdResolution
 *
 * Returns the descriptor string for the selected resolution. This can be send
 * to a WFD sink.
 *
 * Returns: (transfer full): The WFD descriptor string for the given resolution.
 */
gchar *
wfd_video_codec_get_descriptor_for_resolution (WfdVideoCodec *self, const WfdResolution *resolution)
{
  guint16 cae_sup, vesa_sup, hh_sup;
  guint32 slice_enc_params;
  guint8 frame_rate_ctrl_sup;

  /* Need to find the resolution in one of the tables. */
  cae_sup = sup_for_resolution (RESOLUTION_TABLE_CEA, resolution);
  hh_sup = sup_for_resolution (RESOLUTION_TABLE_HH, resolution);
  vesa_sup = sup_for_resolution (RESOLUTION_TABLE_VESA, resolution);

  /* Set dynamic frame rate flag? */
  frame_rate_ctrl_sup = self->frame_skipping_allowed & 0x01;
  slice_enc_params = 0;

  return g_strdup_printf ("00 00 %02X %02X %08X %08X %08X %02X %04X %04X %02x none none",
                          /* static: native, resolution and prefered display mode */
                          self->profile, self->level,
                          cae_sup, vesa_sup, hh_sup,
                          self->latency, self->min_slice_size,
                          slice_enc_params, frame_rate_ctrl_sup
                          /* static: maximum width and height */
                         );
}

/**
 * wfd_video_codec_dump:
 * @self: A #WfdVideoCodec
 *
 * Write out information about the video codec for debugging. Uses g_debug()
 * internally.
 */
void
wfd_video_codec_dump (WfdVideoCodec *self)
{
  g_autoptr(GList) resolutions = NULL;
  GList *res = NULL;

  g_debug ("WfdVideoCodec:");
  g_debug (" * profile: %d", self->profile);
  g_debug (" * level: %d", self->level);
  if (self->native)
    {
      g_debug (" * native: %dx%d %d%s",
               self->native->width,
               self->native->height,
               self->native->refresh_rate,
               self->native->interlaced ? "i" : "p");
    }
  resolutions = wfd_video_codec_get_resolutions (self);
  g_debug ("Supported resolutions:");
  for (res = resolutions; res != NULL; res = res->next)
    {
      WfdResolution *resolution = res->data;
      g_debug (" * %dx%d %d%s",
               resolution->width,
               resolution->height,
               resolution->refresh_rate,
               resolution->interlaced ? "i" : "p");
    }
}
