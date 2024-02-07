#include "config.h"

#include "gdkwaylanddmabufformats.h"

#include "gdkdebugprivate.h"
#include "gdkdmabufformatsprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdmabufformatsprivate.h"

#include "linux-dmabuf-unstable-v1-client-protocol.h"

/**
 * gdk_wayland_dmabuf_formats_get_main_device:
 * @formats: a `GdkDmabufFormats`
 *
 * Returns the DRM device that the compositor uses for compositing.
 *
 * If this information isn't available (e.g. because @formats wasn't
 * obtained form the compositor), then 0 is returned.
 *
 * Returns: the main DRM device that the compositor prefers
 *
 * Since: 4.14
 */
dev_t
gdk_wayland_dmabuf_formats_get_main_device (GdkDmabufFormats *formats)
{
  return formats->device;
}

/**
 * gdk_wayland_dmabuf_formats_get_target_device:
 * @formats: a `GdkDmabufFormats`
 * @idx: the index of the format to return
 *
 * Returns the target DRM device that should be used for creating buffers
 * with this format.
 *
 * If this information isn't available (e.g. because @formats wasn't
 * obtained form the compositor), then 0 is returned.
 *
 * Returns: the target DRM device for this format
 *
 * Since: 4.14
 */
dev_t
gdk_wayland_dmabuf_formats_get_target_device (GdkDmabufFormats *formats,
                                              gsize             idx)
{
  GdkDmabufFormat *format;

  g_return_val_if_fail (idx < formats->n_formats, 0);

  format = &formats->formats[idx];

  return format->device;
}

/**
 * gdk_wayland_dmabuf_formats_is_scanout:
 * @formats: a `GdkDmabufFormats`
 * @idx: the index of the format to return
 *
 * Returns whether the compositor intents to use buffers with this
 * format for scanout.
 *
 * If this information isn't available (e.g. because @formats wasn't
 * obtained form the compositor), then 0 is returned.
 *
 * Returns: whether the format will be used for scanout
 *
 * Since: 4.14
 */
gboolean
gdk_wayland_dmabuf_formats_is_scanout (GdkDmabufFormats *formats,
                                       gsize             idx)
{
  GdkDmabufFormat *format;

  g_return_val_if_fail (idx < formats->n_formats, 0);

  format = &formats->formats[idx];

  return format->flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT;
}
