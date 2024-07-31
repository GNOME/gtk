#include "config.h"

#ifdef HAVE_DMABUF

#include "udmabuf.h"

#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/udmabuf.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <gio/gio.h>

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

void
udmabuf_free (gpointer data)
{
  UDmabuf *udmabuf = data;

  munmap (udmabuf->data, udmabuf->size);
  close (udmabuf->mem_fd);
  close (udmabuf->dmabuf_fd);

  g_free (udmabuf);
}

#define align(x,y) (((x) + (y) - 1) & ~((y) - 1))

UDmabuf *
udmabuf_allocate (size_t   size,
                  GError **error)
{
  int mem_fd = -1;
  int dmabuf_fd = -1;
  uint64_t alignment;
  int res;
  gpointer data;
  UDmabuf *udmabuf;

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

  if (!data)
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

#endif
