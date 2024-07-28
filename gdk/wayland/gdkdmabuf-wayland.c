#include "config.h"

#include "gdkdmabuf-wayland-private.h"

#include "gdkdebugprivate.h"
#include "gdkdmabufformatsprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdmabufformatsprivate.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>

#include "linux-dmabuf-unstable-v1-client-protocol.h"


static DmabufTranche *
dmabuf_tranche_new (void)
{
  return g_new0 (DmabufTranche, 1);
}

static void
dmabuf_tranche_free (DmabufTranche *tranche)
{
  g_free (tranche->formats);
  g_free (tranche);
}

static DmabufFormats *
dmabuf_formats_new (void)
{
  DmabufFormats *formats;

  formats = g_new0 (DmabufFormats, 1);
  formats->tranches = g_ptr_array_new_with_free_func ((GDestroyNotify) dmabuf_tranche_free);

  return formats;
}

static void
dmabuf_formats_free (DmabufFormats *formats)
{
  g_ptr_array_unref (formats->tranches);
  g_free (formats);
}

static void
update_dmabuf_formats (DmabufFormatsInfo *info)
{
  DmabufFormats *formats = info->dmabuf_formats;

  GDK_DISPLAY_DEBUG (info->display, MISC,
                     "dmabuf format table (%" G_GSIZE_FORMAT " entries)", info->n_dmabuf_formats);
  GDK_DISPLAY_DEBUG (info->display, MISC,
                     "dmabuf main device: %u %u",
                     major (formats->main_device),
                     minor (formats->main_device));

  for (gsize i = 0; i < formats->tranches->len; i++)
    {
      DmabufTranche *tranche = g_ptr_array_index (formats->tranches, i);

      GDK_DISPLAY_DEBUG (info->display, MISC,
                         "dmabuf tranche target device: %u %u",
                         major (tranche->target_device),
                         minor (tranche->target_device));

      GDK_DISPLAY_DEBUG (info->display, MISC,
                         "dmabuf%s tranche (%" G_GSIZE_FORMAT " entries):",
                         tranche->flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT ? " scanout" : "",
                         tranche->n_formats);

      for (gsize j = 0; j < tranche->n_formats; j++)
        {
          GDK_DISPLAY_DEBUG (info->display, MISC,
                             "  %.4s:%#" G_GINT64_MODIFIER "x",
                             (char *) &(tranche->formats[j].fourcc),
                             tranche->formats[j].modifier);
        }
    }
}

static void
linux_dmabuf_done (void *data,
                   struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
  DmabufFormatsInfo *info = data;

  g_clear_pointer (&info->dmabuf_formats, dmabuf_formats_free);

  info->dmabuf_formats = info->pending_dmabuf_formats;
  info->pending_dmabuf_formats = NULL;

  update_dmabuf_formats (info);
}

static void
linux_dmabuf_format_table (void *data,
                           struct zwp_linux_dmabuf_feedback_v1 *feedback,
                           int32_t fd,
                           uint32_t size)
{
  DmabufFormatsInfo *info = data;

  if (info->dmabuf_formats)
    munmap (info->dmabuf_formats, sizeof (DmabufFormat) * info->n_dmabuf_formats);

  info->n_dmabuf_formats = size / 16;
  info->dmabuf_format_table = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
}

static void
linux_dmabuf_main_device (void *data,
                          struct zwp_linux_dmabuf_feedback_v1 *feedback,
                          struct wl_array *device)
{
  DmabufFormatsInfo *info = data;
  dev_t dev;

  memcpy (&dev, device->data, sizeof (dev_t));

  g_assert (info->pending_dmabuf_formats == NULL);

  info->pending_dmabuf_formats = dmabuf_formats_new ();
  info->pending_dmabuf_formats->main_device = dev;
}

static void
linux_dmabuf_tranche_done (void *data,
                           struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
  DmabufFormatsInfo *info = data;

  g_ptr_array_add (info->pending_dmabuf_formats->tranches,
                   info->pending_tranche);

  info->pending_tranche = NULL;
}

static void
linux_dmabuf_tranche_target_device (void *data,
                                    struct zwp_linux_dmabuf_feedback_v1 *feedback,
                                    struct wl_array *device)
{
  DmabufFormatsInfo *info = data;
  dev_t dev;
  DmabufTranche *tranche;

  memcpy (&dev, device->data, sizeof (dev_t));

  g_assert (info->pending_tranche == NULL);

  tranche = dmabuf_tranche_new ();
  tranche->target_device = dev;

  info->pending_tranche = tranche;
}

static void
linux_dmabuf_tranche_formats (void *data,
                              struct zwp_linux_dmabuf_feedback_v1 *feedback,
                              struct wl_array *indices)
{
  DmabufFormatsInfo *info = data;
  DmabufTranche *tranche;
  int i;
  guint16 *pos;

  g_assert (info->pending_tranche != NULL);
  tranche = info->pending_tranche;

  tranche->n_formats = indices->size / sizeof (guint16);
  tranche->formats = g_new (DmabufFormat, tranche->n_formats);

  i = 0;
  wl_array_for_each (pos, indices)
    {
      tranche->formats[i++] = info->dmabuf_format_table[*pos];
    }
}

static void
linux_dmabuf_tranche_flags (void *data,
                            struct zwp_linux_dmabuf_feedback_v1 *feedback,
                            uint32_t flags)
{
  DmabufFormatsInfo *info = data;
  DmabufTranche *tranche;

  g_assert (info->pending_tranche != NULL);
  tranche = info->pending_tranche;
  tranche->flags = flags;
}

static const struct zwp_linux_dmabuf_feedback_v1_listener feedback_listener = {
  linux_dmabuf_done,
  linux_dmabuf_format_table,
  linux_dmabuf_main_device,
  linux_dmabuf_tranche_done,
  linux_dmabuf_tranche_target_device,
  linux_dmabuf_tranche_formats,
  linux_dmabuf_tranche_flags,
};

DmabufFormatsInfo *
dmabuf_formats_info_new (GdkDisplay                          *display,
                         const char                          *name,
                         struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
  DmabufFormatsInfo *info;

  info = g_new0 (DmabufFormatsInfo, 1);

  info->display = display;
  info->name = g_strdup (name);
  info->feedback = feedback;

  if (info->feedback)
    zwp_linux_dmabuf_feedback_v1_add_listener (info->feedback,
                                               &feedback_listener, info);

  return info;
}

void
dmabuf_formats_info_free (DmabufFormatsInfo *info)
{
  g_free (info->name);
  g_clear_pointer (&info->feedback, zwp_linux_dmabuf_feedback_v1_destroy);
  if (info->dmabuf_format_table)
    {
      munmap (info->dmabuf_format_table, info->n_dmabuf_formats * 16);
      info->dmabuf_format_table = NULL;
    }
  g_clear_pointer (&info->dmabuf_formats, dmabuf_formats_free);
  g_clear_pointer (&info->pending_dmabuf_formats, dmabuf_formats_free);
  g_clear_pointer (&info->pending_tranche, dmabuf_tranche_free);

  g_free (info);
}
