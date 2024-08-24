#include "config.h"

#include "udmabuf.h"

#ifdef HAVE_DMABUF

#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/udmabuf.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include "gdk/gdkdmabuffourccprivate.h"


typedef struct
{
  int mem_fd;
  int dmabuf_fd;
  size_t size;
  gpointer data;
} UDmabuf;

static int udmabuf_fd;

gboolean
udmabuf_initialize (GError **error)
{
  if (udmabuf_fd == 0)
    {
      udmabuf_fd = open ("/dev/udmabuf", O_RDWR);
      if (udmabuf_fd == -1)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Failed to open /dev/udmabuf: %s",
                       g_strerror (errno));
        }
    }

  return udmabuf_fd != -1;
}

static void
udmabuf_free (gpointer data)
{
  UDmabuf *udmabuf = data;

  munmap (udmabuf->data, udmabuf->size);
  close (udmabuf->mem_fd);
  close (udmabuf->dmabuf_fd);

  g_free (udmabuf);
}

#define align(x,y) (((x) + (y) - 1) & ~((y) - 1))

static UDmabuf *
udmabuf_allocate (size_t   size,
                  GError **error)
{
  int mem_fd = -1;
  int dmabuf_fd = -1;
  uint64_t alignment;
  int res;
  gpointer data;
  UDmabuf *udmabuf;

  if (udmabuf_fd == 0)
    {
      if (!udmabuf_initialize (error))
        goto fail;
    }

  if (udmabuf_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "udmabuf not available");
      goto fail;
    }

  alignment = sysconf (_SC_PAGE_SIZE);

  size = align (size, alignment);

  mem_fd = memfd_create ("gtk", MFD_ALLOW_SEALING);
  if (mem_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to open /dev/udmabuf: %s",
                   g_strerror (errno));
      goto fail;
    }

  res = ftruncate (mem_fd, size);
  if (res == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "ftruncate failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  if (fcntl (mem_fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "ftruncate failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  dmabuf_fd = ioctl (udmabuf_fd,
                     UDMABUF_CREATE,
                     &(struct udmabuf_create) {
                       .memfd = mem_fd,
                       .flags = UDMABUF_FLAGS_CLOEXEC,
                       .offset = 0,
                       .size = size
                     });

  if (dmabuf_fd < 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "UDMABUF_CREATE ioctl failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  data = mmap (NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, 0);

  if (data == NULL || data == MAP_FAILED)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "mmap failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  udmabuf = g_new0 (UDmabuf, 1);

  udmabuf->mem_fd = mem_fd;
  udmabuf->dmabuf_fd = dmabuf_fd;
  udmabuf->size = size;
  udmabuf->data = data;

  return udmabuf;

fail:
  if (mem_fd != -1)
    close (mem_fd);
  if (dmabuf_fd != -1)
    close (dmabuf_fd);

  return NULL;
}


GdkTexture *
udmabuf_texture_new (gsize           width,
                     gsize           height,
                     guint           fourcc,
                     GdkColorState  *color_state,
                     gboolean        premultiplied,
                     GBytes         *bytes,
                     gsize           stride,
                     GError        **error)
{
  GdkDmabufTextureBuilder *builder;
  GdkTexture *texture;
  UDmabuf *udmabuf;
  gconstpointer data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  udmabuf = udmabuf_allocate (size, error);
  if (udmabuf == NULL)
    return NULL;

  memcpy (udmabuf->data, data, size);

  builder = gdk_dmabuf_texture_builder_new ();

  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_fourcc (builder, fourcc);
  gdk_dmabuf_texture_builder_set_modifier (builder, 0);
  gdk_dmabuf_texture_builder_set_color_state (builder, color_state);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, premultiplied);
  gdk_dmabuf_texture_builder_set_n_planes (builder, 1);
  gdk_dmabuf_texture_builder_set_fd (builder, 0, udmabuf->dmabuf_fd);
  gdk_dmabuf_texture_builder_set_stride (builder, 0, stride);
  gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);

  texture = gdk_dmabuf_texture_builder_build (builder, udmabuf_free, udmabuf, error);

  g_object_unref (builder);

  return texture;
}

#else

GdkTexture *
udmabuf_texture_new (gsize           width,
                     gsize           height,
                     guint           fourcc,
                     GdkColorState  *color_state,
                     gboolean        premultiplied,
                     GBytes         *bytes,
                     gsize           stride,
                     GError        **error)
{
  g_set_error (error,
               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "Dmabufs are not supported");
  return NULL;
}

#endif

GdkTexture *
udmabuf_texture_from_texture (GdkTexture  *texture,
                              GError     **error)
{
  guint fourcc;
  gboolean premultiplied;
  guchar *data;
  gsize width, height, stride;
  GBytes *bytes;
  GdkTexture *texture2;

  switch ((int) gdk_texture_get_format (texture))
    {
#ifdef HAVE_DMABUF
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_ARGB8888;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_BGRA8888;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_ABGR8888;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_RGBA8888;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_B8G8R8A8:
      fourcc = DRM_FORMAT_ARGB8888;
      premultiplied = FALSE;
      break;
    case GDK_MEMORY_A8R8G8B8:
      fourcc = DRM_FORMAT_BGRA8888;
      premultiplied = FALSE;
      break;
#endif
    default:
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Unsupported memory format %u", gdk_texture_get_format (texture));
      return NULL;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  stride = (width * 4 + 255) & ~255;
  data = g_malloc (stride * height);

  gdk_texture_download (texture, data, stride);
  bytes = g_bytes_new_take (data, stride * height);

  texture2 = udmabuf_texture_new (width, height,
                                  fourcc,
                                  gdk_texture_get_color_state (texture),
                                  premultiplied,
                                  bytes, stride,
                                  error);
  g_bytes_unref (bytes);

  return texture2;
}

