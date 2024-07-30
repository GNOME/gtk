#include "config.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/udmabuf.h>
#include <drm_fourcc.h>
#include <errno.h>

#include <gtk/gtk.h>

static int udmabuf_fd;

static gboolean
initialize_udmabuf (GError **error)
{
  if (udmabuf_fd == 0)
    udmabuf_fd = open ("/dev/udmabuf", O_RDWR);

  if (udmabuf_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Opening /dev/udmabuf failed: %s", strerror (errno));
      return FALSE;
    }

  return TRUE;
}

#define align(x,y) (((x) + (y) - 1) & ~((y) - 1))

typedef struct
{
  int mem_fd;
  int dmabuf_fd;
  size_t size;
  gpointer data;
} UDmabuf;

static void
udmabuf_free (gpointer data)
{
  UDmabuf *udmabuf = data;

  munmap (udmabuf->data, udmabuf->size);
  close (udmabuf->mem_fd);
  close (udmabuf->dmabuf_fd);

  g_free (udmabuf);
}

static UDmabuf *
allocate_udmabuf (size_t   size,
                  GError **error)
{
  int mem_fd = -1;
  int dmabuf_fd = -1;
  uint64_t alignment;
  int res;
  gpointer data;
  UDmabuf *udmabuf;

  if (udmabuf_fd == -1)
    goto fail;

  alignment = sysconf (_SC_PAGE_SIZE);

  size = align(size, alignment);

  mem_fd = memfd_create ("gtk", MFD_ALLOW_SEALING);
  if (mem_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "memfd_create failed: %s", strerror (errno));
      goto fail;
    }

  res = ftruncate (mem_fd, size);
  if (res == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "ftruncate failed: %s", strerror (errno));
       goto fail;
    }

  if (fcntl (mem_fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "fcntl failed: %s", strerror (errno));
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
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "UDMABUF_CREATE ioctl failed");
      goto fail;
    }

  data = mmap (NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, 0);

  if (!data)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "mmap failed: %s", strerror (errno));
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

static void
test_dmabuf_no_gpu (void)
{
  guchar *buffer;
  UDmabuf *udmabuf;
  GdkDmabufTextureBuilder *builder;
  GdkTexture *texture;
  GError *error = NULL;
  GdkTextureDownloader *downloader;
  gsize stride;
  GBytes *bytes;
  const guchar *data;

  if (!initialize_udmabuf (&error))
    {
      g_test_skip (error->message);
      g_error_free (error);
      return;
    }

  udmabuf = allocate_udmabuf (32, &error);
  if (!udmabuf)
    {
      g_test_fail_printf ("%s", error->message);
      g_error_free (error);
      return;
    }

  buffer = udmabuf->data;

  buffer[0] = 255;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 255;

  builder = gdk_dmabuf_texture_builder_new ();
  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());

  gdk_dmabuf_texture_builder_set_width (builder, 1);
  gdk_dmabuf_texture_builder_set_height (builder, 1);
  gdk_dmabuf_texture_builder_set_fourcc (builder, DRM_FORMAT_RGBA8888);
  gdk_dmabuf_texture_builder_set_modifier (builder, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, FALSE);
  gdk_dmabuf_texture_builder_set_n_planes (builder, 1);
  gdk_dmabuf_texture_builder_set_stride (builder, 1, 4);
  gdk_dmabuf_texture_builder_set_fd (builder, 0, udmabuf->dmabuf_fd);
  gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);
  gdk_dmabuf_texture_builder_set_color_state (builder, gdk_color_state_get_srgb ());

  texture = gdk_dmabuf_texture_builder_build (builder, udmabuf_free, udmabuf, &error);
  g_assert_no_error (error);

  g_object_unref (builder);

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, gdk_texture_get_format (texture));
  gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));

  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);

  gdk_texture_downloader_free (downloader);

  data = g_bytes_get_data (bytes, NULL);
  g_assert_true (memcmp (data, buffer, 4) == 0);

  g_bytes_unref (bytes);

  g_object_unref (texture);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dmabuf/no-gpu", test_dmabuf_no_gpu);

  return g_test_run ();
}
