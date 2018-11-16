#include "wfd-resolution.h"

G_DEFINE_BOXED_TYPE (WfdResolution, wfd_resolution, wfd_resolution_copy, wfd_resolution_free)

/**
 * wfd_resolution_new:
 *
 * Creates a new #WfdResolution.
 *
 * Returns: (transfer full): A newly created #WfdResolution
 */
WfdResolution *
wfd_resolution_new (void)
{
  WfdResolution *self;

  self = g_slice_new0 (WfdResolution);

  return self;
}

/**
 * wfd_resolution_copy:
 * @self: a #WfdResolution
 *
 * Makes a deep copy of a #WfdResolution.
 *
 * Returns: (transfer full): A newly created #WfdResolution with the same
 *   contents as @self
 */
WfdResolution *
wfd_resolution_copy (WfdResolution *self)
{
  WfdResolution *copy;

  g_return_val_if_fail (self, NULL);

  copy = wfd_resolution_new ();
  copy->width = self->width;
  copy->height = self->height;
  copy->refresh_rate = self->refresh_rate;
  copy->interlaced = self->interlaced;

  return copy;
}

/**
 * wfd_resolution_free:
 * @self: a #WfdResolution
 *
 * Frees a #WfdResolution allocated using wfd_resolution_new()
 * or wfd_resolution_copy().
 */
void
wfd_resolution_free (WfdResolution *self)
{
  g_return_if_fail (self);

  g_slice_free (WfdResolution, self);
}
