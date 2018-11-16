#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define WFD_TYPE_RESOLUTION (wfd_resolution_get_type ())

typedef struct _WfdResolution WfdResolution;

GType              wfd_resolution_get_type (void) G_GNUC_CONST;
WfdResolution     *wfd_resolution_new (void);
WfdResolution     *wfd_resolution_copy (WfdResolution *self);
void               wfd_resolution_free (WfdResolution *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WfdResolution, wfd_resolution_free)

struct _WfdResolution
{
  gint     width;
  gint     height;
  gint     refresh_rate;
  gboolean interlaced;
};

G_END_DECLS
