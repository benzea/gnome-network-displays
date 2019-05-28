#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ND_TYPE_CODEC_INSTALL (nd_codec_install_get_type())

G_DECLARE_FINAL_TYPE (NdCodecInstall, nd_codec_install, ND, CODEC_INSTALL, GtkRevealer)

NdCodecInstall *nd_codec_install_new (void);

G_END_DECLS
