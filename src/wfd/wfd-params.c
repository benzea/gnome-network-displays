#include "wfd-params.h"

G_DEFINE_BOXED_TYPE (WfdParams, wfd_params, wfd_params_copy, wfd_params_free)

static const gchar * params_m3_mandatory[] = {
  "wfd_client_rtp_ports",
  "wfd_audio_codecs",
  "wfd_video_formats",
};

static const gchar * params_m3_optional[] = {
  "wfd_3d_video_formats",
  "wfd_display_edid",
  "wfd_coupled_sink",
  "wfd_I2C",
  "wfd_standby_resume_capability",
  "wfd_connector_type",
  "wfd_uibc_capability",
  "wfd2_audio_codecs",
  "wfd2_video_codecs",
  "wfd2_aux_stream_formats",
  "wfd2_buffer_length",
  "wfd2_audio_playback_status",
  "wfd2_video_playback_status",
  "wfd2_cta_datablock_collection",
};

static const gchar *params_m3_query_extra[] = {
  "wfd_display_edid",
};

/**
 * wfd_params_new:
 *
 * Creates a new #WfdParams.
 *
 * Returns: (transfer full): A newly created #WfdParams
 */
WfdParams *
wfd_params_new (void)
{
  WfdParams *self;
  WfdVideoCodec *basic_codec;
  g_autoptr(GList) resolutions = NULL;

  self = g_slice_new0 (WfdParams);

  self->primary_rtp_port = 16384;

  /* Add a default video codec. This will be removed when we get something
   * proper from the client.
   * Doing this is mainly useful for testing with normal RTSP clients. */
  self->video_codecs = g_ptr_array_new_with_free_func ((GDestroyNotify) wfd_video_codec_unref);
  basic_codec = wfd_video_codec_new_from_desc (0, "01 01 00000081 00000000 00000000 00 0000 0000 00 none none");
  g_ptr_array_add (self->video_codecs, basic_codec);

  /* Set a default resolution (for testing purposes) */
  self->selected_codec = wfd_video_codec_ref (basic_codec);
  self->selected_resolution = wfd_resolution_copy (basic_codec->native);

  return self;
}

/**
 * wfd_params_copy:
 * @self: a #WfdParams
 *
 * Makes a deep copy of a #WfdParams.
 *
 * Returns: (transfer full): A newly created #WfdParams with the same
 *   contents as @self
 */
WfdParams *
wfd_params_copy (WfdParams *self)
{
  WfdParams *copy;

  g_return_val_if_fail (self, NULL);

  copy = wfd_params_new ();

  copy->primary_rtp_port = self->primary_rtp_port;
  copy->secondary_rtp_port = self->secondary_rtp_port;
  if (self->edid)
    {
      copy->edid = g_byte_array_new ();
      g_byte_array_append (copy->edid, self->edid->data, self->edid->len);
    }

  /* Remove the default codec and all of the ones from the original. */
  g_ptr_array_remove (copy->video_codecs, 0);
  for (guint i = 0; i < self->video_codecs->len; i++)
    {
      WfdVideoCodec *codec = (WfdVideoCodec *) g_ptr_array_index (self->video_codecs, i);
      WfdVideoCodec *new_codec = wfd_video_codec_copy (codec);

      g_ptr_array_add (copy->video_codecs, new_codec);
    }

  if (self->selected_codec)
    copy->selected_codec = wfd_video_codec_copy (self->selected_codec);
  if (self->selected_resolution)
    copy->selected_resolution = wfd_resolution_copy (self->selected_resolution);

  return copy;
}

/**
 * wfd_params_free:
 * @self: a #WfdParams
 *
 * Frees a #WfdParams allocated using wfd_params_new()
 * or wfd_params_copy().
 */
void
wfd_params_free (WfdParams *self)
{
  g_return_if_fail (self);

  g_clear_pointer (&self->selected_codec, wfd_video_codec_unref);
  g_clear_pointer (&self->selected_resolution, wfd_resolution_free);

  g_clear_pointer (&self->video_codecs, g_ptr_array_unref);
  g_clear_pointer (&self->edid, g_byte_array_unref);
  g_clear_pointer (&self->profile, g_free);

  g_slice_free (WfdParams, self);
}

gchar *
wfd_params_m3_query_params (WfdParams *self)
{
  g_autoptr(GPtrArray) query_params = NULL;

  query_params = g_ptr_array_sized_new (G_N_ELEMENTS (params_m3_mandatory) + G_N_ELEMENTS (params_m3_query_extra) + 1);

  for (gint i = 0; i < G_N_ELEMENTS (params_m3_mandatory); i++)
    g_ptr_array_add (query_params, (gpointer) params_m3_mandatory[i]);

  for (gint i = 0; i < G_N_ELEMENTS (params_m3_query_extra); i++)
    g_ptr_array_add (query_params, (gpointer) params_m3_query_extra[i]);

  /* For a newline at the end. */
  g_ptr_array_add (query_params, (gpointer) "");
  g_ptr_array_add (query_params, NULL);

  return g_strjoinv ("\r\n", (GStrv) query_params->pdata);
}

void
wfd_params_from_sink (WfdParams *self, const guint8 *body, gsize body_size)
{
  g_auto(GStrv) lines = NULL;
  gchar **line;
  g_autofree gchar *body_str = NULL;

  /* Empty body is probably testing, just keep the current values. */
  if (body == NULL)
    return;

  body_str = g_strndup ((gchar *) body, body_size);
  lines = g_strsplit (body_str, "\n", 0);

  for (line = lines; *line; line++)
    {
      g_auto(GStrv) split_line = NULL;
      gchar *option;
      gchar *value;

      g_strstrip (*line);

      /* Ignore empty lines */
      if (**line == '\0')
        continue;

      split_line = g_strsplit (*line, ":", 2);
      if (g_strv_length (split_line) != 2)
        continue;

      option = g_strstrip (split_line[0]);
      value = g_strstrip (split_line[1]);

      /* Now, handle the different options (that we support) */
      if (g_str_equal (option, "wfd_client_rtp_ports"))
        {
          g_auto(GStrv) split_value = NULL;
          split_value = g_strsplit (value, " ", 0);
          if (g_strv_length (split_value) != 4)
            {
              g_warning ("WfdParams: Ivalid value wfd_client_rtp_ports: %s", value);
              continue;
            }

          if (!g_str_equal (split_value[0], "RTP/AVP/UDP;unicast"))
            {
              g_warning ("WfdParams: Ivalid profile: %s", split_value[0]);
              continue;
            }

          if (!g_str_equal (split_value[3], "mode=play"))
            {
              g_warning ("WfdParams: Ivalid mode: %s", split_value[3]);
              continue;
            }

          g_clear_pointer (&self->profile, g_free);
          self->profile = g_strdup (split_value[0]);

          self->primary_rtp_port = g_ascii_strtoll (split_value[1], NULL, 10);
          self->secondary_rtp_port = g_ascii_strtoll (split_value[2], NULL, 10);
        }
      else if (g_str_equal (option, "wfd_video_formats"))
        {
          g_auto(GStrv) split_value = NULL;
          g_auto(GStrv) codec_descriptors = NULL;
          char **codec_descriptor;
          guint16 native;

          /* Clear video codecs to fill them up again. */
          g_clear_pointer (&self->selected_codec, wfd_video_codec_unref);
          g_clear_pointer (&self->selected_resolution, wfd_resolution_free);
          g_ptr_array_unref (self->video_codecs);
          self->video_codecs = g_ptr_array_new_with_free_func ((GDestroyNotify) wfd_video_codec_unref);

          if (g_str_equal (value, "none"))
            continue;

          split_value = g_strsplit (value, " ", 3);

          if (g_strv_length (split_value) != 3)
            {
              g_warning ("WfdParams: wfd_video_formats is invalid: %s", value);
              continue;
            }

          native = g_ascii_strtoll (split_value[0], NULL, 16);
          /* split_value[1] is the perfered display mode (WFD 1.0 specific), we just ignore it */

          codec_descriptors = g_strsplit (split_value[2], ",", 0);
          for (codec_descriptor = codec_descriptors; *codec_descriptor; codec_descriptor++)
            {
              g_autoptr(WfdVideoCodec) codec = NULL;

              g_strstrip (*codec_descriptor);
              codec = wfd_video_codec_new_from_desc (native, *codec_descriptor);
              if (codec)
                {
                  g_debug ("Add codec to params:");
                  wfd_video_codec_dump (codec);
                  g_ptr_array_add (self->video_codecs, g_steal_pointer (&codec));
                }
              else
                {
                  g_warning ("WfdParams: Could not parse codec descriptor: %s", *codec_descriptor);
                }
            }
        }
      else if (g_str_equal (option, "wfd_audio_codecs"))
        {
          /* TODO: Implement */
          g_warning ("WfdParams: Audio codec parsing not yet implemented!");
        }
      else if (g_str_equal (option, "wfd_display_edid"))
        {
          guint length;
          g_auto(GStrv) split_value = NULL;

          g_clear_pointer (&self->edid, g_byte_array_unref);

          if (g_str_equal (value, "none"))
            continue;

          split_value = g_strsplit (value, " ", 2);
          if (g_strv_length (split_value) != 2)
            {
              g_warning ("WfdParams: Invalid EDID specifier: %s", value);
              continue;
            }

          length = g_ascii_strtoll (split_value[0], NULL, 10);

          if (128 * 2 * length != strlen (split_value[1]))
            {
              g_warning ("WfdParams: EDID hex string should be %u characters but is %lu characters",
                         128 * 2 * length, strlen (split_value[1]));
              continue;
            }

          g_clear_pointer (&self->edid, g_byte_array_unref);
          /* Convert to binary, two characters at the time from the back, NUL
           * terminating in the process. */
          self->edid = g_byte_array_sized_new (128 * length);
          for (gint i = 128 * length - 1; i >= 0; i--)
            {
              gchar byte = (gchar) g_ascii_strtoll (&split_value[1][i * 2], NULL, 16);
              split_value[1][i * 2] = '\0';
              self->edid->data[i] = byte;
            }
        }
      else
        {
          g_debug ("WfdParams: Not handling option %s", option);
        }
    }
}
