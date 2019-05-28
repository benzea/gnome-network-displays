#include "nd-codec-install.h"
#include <glib/gi18n.h>
#include <gst/pbutils/pbutils.h>

struct _NdCodecInstall
{
  GtkRevealer parent_instance;

  GStrv       codecs;

  GtkListBox *listbox;
  GtkLabel   *header;
};

G_DEFINE_TYPE (NdCodecInstall, nd_codec_install, GTK_TYPE_REVEALER)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_CODECS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

NdCodecInstall *
nd_codec_install_new (void)
{
  return g_object_new (ND_TYPE_CODEC_INSTALL, NULL);
}

static void
nd_codec_install_finalize (GObject *object)
{
  NdCodecInstall *self = (NdCodecInstall *) object;

  g_clear_pointer (&self->codecs, g_strfreev);

  G_OBJECT_CLASS (nd_codec_install_parent_class)->finalize (object);
}

static void
remove_widget (GtkWidget *widget, gpointer user_data)
{
  GtkContainer *container = GTK_CONTAINER (user_data);

  gtk_container_remove (container, widget);
}

static gchar *
get_description (const gchar *codec)
{
  if (g_strcmp0 (codec, "openh264enc") == 0)
    return g_strdup_printf (_("GStreamer OpenH264 video encoder (%s)"), codec);
  else if (g_strcmp0 (codec, "x264enc") == 0)
    return g_strdup_printf (_("GStreamer x264 video encoder (%s)"), codec);
  else if (g_strcmp0 (codec, "vaapih264enc") == 0)
    return g_strdup_printf (_("GStreamer VA-API H264 video encoder (%s)"), codec);
  else if (g_strcmp0 (codec, "fdkaacenc") == 0)
    return g_strdup_printf (_("GStreamer FDK AAC audio encoder (%s)"), codec);
  else if (g_strcmp0 (codec, "avenc_aac") == 0)
    return g_strdup_printf (_("GStreamer libav AAC audio encoder (%s)"), codec);
  else if (g_strcmp0 (codec, "faac") == 0)
    return g_strdup_printf (_("GStreamer Free AAC audio encoder (%s)"), codec);

  return g_strdup_printf (_("GStreamer Element “%s”"), codec);
}

static void
nd_codec_install_update (NdCodecInstall *self)
{
  gchar **codec;

  gtk_container_foreach (GTK_CONTAINER (self->listbox), remove_widget, self->listbox);

  if (self->codecs == NULL)
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);
      return;
    }

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), self->codecs[0] != NULL);

  for (codec = self->codecs; *codec; codec += 1)
    {
      GtkWidget *row;
      GtkWidget *label;
      g_autofree gchar *description = NULL;

      row = gtk_list_box_row_new ();

      description = get_description (*codec);
      label = gtk_label_new (description);
      gtk_container_add (GTK_CONTAINER (row), label);
      gtk_widget_show (label);
      g_object_set (label,
                    "margin", 6,
                    "xalign", 0.0,
                    NULL);

      g_object_set_data_full (G_OBJECT (row), "codec", g_strdup (*codec), g_free);
      g_object_set_data_full (G_OBJECT (row), "desc", g_steal_pointer (&description), g_free);

      gtk_list_box_insert (self->listbox, row, -1);
      gtk_widget_show (row);
    }
}

static void
nd_codec_install_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  NdCodecInstall *self = ND_CODEC_INSTALL (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_boxed (value, self->codecs);
      break;

    case PROP_CODECS:
      g_value_set_string (value, gtk_label_get_text (self->header));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nd_codec_install_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  NdCodecInstall *self = ND_CODEC_INSTALL (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_text (self->header, g_value_get_string (value));
      break;

    case PROP_CODECS:
      g_clear_pointer (&self->codecs, g_strfreev);
      self->codecs = g_value_dup_boxed (value);

      nd_codec_install_update (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nd_codec_install_class_init (NdCodecInstallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nd_codec_install_finalize;
  object_class->get_property = nd_codec_install_get_property;
  object_class->set_property = nd_codec_install_set_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", "Title",
                         "String for the title of the list",
                         _("Please install one of the following GStreamer plugins by clicking below"),
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CODECS] =
    g_param_spec_boxed ("codecs", "Codecs",
                        "List of required codecs",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
on_install_plugins_done_cb (GstInstallPluginsReturn result,
                            gpointer                user_data)
{
  g_debug ("gst_install_plugins_async operation finished");
}

static void
on_row_activated_cb (NdCodecInstall *self, GtkListBoxRow *row)
{
  GstInstallPluginsContext *ctx = NULL;
  const gchar *codecs[2] = { NULL };
  GstInstallPluginsReturn res;
  GstElement *elem;
  GstMessage *msg;

  ctx = gst_install_plugins_context_new ();
  gst_install_plugins_context_set_desktop_id (ctx, "org.gnome.NetworkDisplays");

  elem = gst_pipeline_new ("dummy");
  msg = gst_missing_element_message_new (elem, g_object_get_data (G_OBJECT (row), "codec"));
  codecs[0] = gst_missing_plugin_message_get_installer_detail (msg);

  res = gst_install_plugins_async (codecs, ctx, on_install_plugins_done_cb, NULL);

  /* Try to fall back to calling into gnome-software directly in case the
   * GStreamer plugin helper is not installed.
   * Not sure if this is sane to do, but it may be worth a shot. */
  if (res == GST_INSTALL_PLUGINS_HELPER_MISSING)
    {
      GDBusConnection *con = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
      GVariantBuilder params;
      g_autofree gchar *desc = NULL;

      desc = g_strdup_printf ("%s|gstreamer1(element-%s)()(%dbit)",
                              (const gchar *) g_object_get_data (G_OBJECT (row), "desc"),
                              (const gchar *) g_object_get_data (G_OBJECT (row), "codec"),
                              (gint) sizeof (gpointer) * 8);

      g_variant_builder_init (&params, G_VARIANT_TYPE ("(asssa{sv})"));
      g_variant_builder_open (&params, G_VARIANT_TYPE ("as"));
      g_variant_builder_add (&params, "s", desc);
      g_variant_builder_close (&params);
      g_variant_builder_add (&params, "s", "");
      g_variant_builder_add (&params, "s", "org.gnome.NetworkDisplays");
      g_variant_builder_open (&params, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_close (&params);

      g_dbus_connection_call (con,
                              "org.gnome.Software",
                              "/org/freedesktop/PackageKit",
                              "org.freedesktop.PackageKit.Modify2",
                              "InstallGStreamerResources",
                              g_variant_builder_end (&params),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL,
                              NULL,
                              NULL);
    }
}

static void
nd_codec_install_init (NdCodecInstall *self)
{
  GtkWidget *box;
  GtkWidget *frame;

  g_object_set (self,
                "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
                NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  g_object_set (box, "margin", 6, NULL);
  gtk_container_add (GTK_CONTAINER (self), box);
  gtk_widget_show (box);

  self->header = GTK_LABEL (gtk_label_new (g_value_get_string (g_param_spec_get_default_value (properties[PROP_TITLE]))));
  g_object_set (self->header,
                "wrap", TRUE,
                "wrap-mode", GTK_WRAP_WORD,
                "xalign", 0.0,
                NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->header));
  gtk_widget_show (GTK_WIDGET (self->header));

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (box), frame);
  gtk_widget_show (frame);

  self->listbox = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (self->listbox));
  gtk_widget_show (GTK_WIDGET (self->listbox));

  g_signal_connect_object (self->listbox,
                           "row-activated",
                           G_CALLBACK (on_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
