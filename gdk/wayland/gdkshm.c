/*
 * Copyright © 2025 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "gdkshm-private.h"

/* {{{ Shm buffer handling */

static const cairo_user_data_key_t gdk_wayland_shm_surface_cairo_key;

typedef struct _GdkWaylandCairoSurfaceData {
  gpointer buf;
  size_t buf_length;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  GdkWaylandDisplay *display;
} GdkWaylandCairoSurfaceData;

static int
open_shared_memory (void)
{
  static gboolean force_shm_open = FALSE;
  int ret = -1;

#if !defined (HAVE_MEMFD_CREATE)
  force_shm_open = TRUE;
#endif

  do
    {
#if defined (HAVE_MEMFD_CREATE)
      if (!force_shm_open)
        {
          int options = MFD_CLOEXEC;
#if defined (MFD_ALLOW_SEALING)
          options |= MFD_ALLOW_SEALING;
#endif
          ret = memfd_create ("gdk-wayland", options);

          /* fall back to shm_open until debian stops shipping 3.16 kernel
           * See bug 766341
           */
          if (ret < 0 && errno == ENOSYS)
            force_shm_open = TRUE;
#if defined (F_ADD_SEALS) && defined (F_SEAL_SHRINK)
          if (ret >= 0)
            fcntl (ret, F_ADD_SEALS, F_SEAL_SHRINK);
#endif
        }
#endif

      if (force_shm_open)
        {
#if defined (__FreeBSD__)
          ret = shm_open (SHM_ANON, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
#else
          char name[NAME_MAX - 1] = "";

          sprintf (name, "/gdk-wayland-%x", g_random_int ());

          ret = shm_open (name, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);

          if (ret >= 0)
            shm_unlink (name);
          else if (errno == EEXIST)
            continue;
#endif
        }
    }
  while (ret < 0 && errno == EINTR);

  if (ret < 0)
    g_critical (G_STRLOC ": creating shared memory file (using %s) failed: %m",
                force_shm_open? "shm_open" : "memfd_create");

  return ret;
}

static struct wl_shm_pool *
create_shm_pool (struct wl_shm  *shm,
                 int             size,
                 size_t         *buf_length,
                 void          **data_out)
{
  struct wl_shm_pool *pool;
  int fd;
  void *data;

  fd = open_shared_memory ();

  if (fd < 0)
    goto fail;

  if (ftruncate (fd, size) < 0)
    {
      g_critical (G_STRLOC ": Truncating shared memory file failed: %m");
      close (fd);
      goto fail;
    }

  data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (data == MAP_FAILED)
    {
      g_critical (G_STRLOC ": mmap'ping shared memory file failed: %m");
      close (fd);
      goto fail;
    }

  pool = wl_shm_create_pool (shm, fd, size);

  close (fd);

  *data_out = data;
  *buf_length = size;

  return pool;

fail:
  *data_out = NULL;
  *buf_length = 0;
  return NULL;
}

static void
gdk_wayland_cairo_surface_destroy (void *p)
{
  GdkWaylandCairoSurfaceData *data = p;

  if (data->buffer)
    wl_buffer_destroy (data->buffer);

  if (data->pool)
    wl_shm_pool_destroy (data->pool);

  munmap (data->buf, data->buf_length);
  g_free (data);
}

cairo_surface_t *
gdk_wayland_display_create_shm_surface (GdkWaylandDisplay *display,
                                        uint32_t           width,
                                        uint32_t           height)
{
  GdkWaylandCairoSurfaceData *data;
  cairo_surface_t *surface = NULL;
  cairo_status_t status;
  int stride;

  data = g_new (GdkWaylandCairoSurfaceData, 1);
  data->display = display;
  data->buffer = NULL;

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);

  data->pool = create_shm_pool (display->shm,
                                height * stride,
                                &data->buf_length,
                                &data->buf);
  if (G_UNLIKELY (data->pool == NULL))
    g_error ("Unable to create shared memory pool");

  surface = cairo_image_surface_create_for_data (data->buf,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width,
                                                 height,
                                                 stride);

  data->buffer = wl_shm_pool_create_buffer (data->pool, 0,
                                            width, height,
                                            stride, WL_SHM_FORMAT_ARGB8888);

  cairo_surface_set_user_data (surface, &gdk_wayland_shm_surface_cairo_key,
                               data, gdk_wayland_cairo_surface_destroy);

  status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_critical (G_STRLOC ": Unable to create Cairo image surface: %s",
                  cairo_status_to_string (status));
    }

  return surface;
}

struct wl_buffer *
_gdk_wayland_shm_surface_get_wl_buffer (cairo_surface_t *surface)
{
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_shm_surface_cairo_key);
  return data->buffer;
}

gboolean
_gdk_wayland_is_shm_surface (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &gdk_wayland_shm_surface_cairo_key) != NULL;
}

/* {{{2 wl_shm_buffer listener */

static void
shm_buffer_release (void             *data,
                    struct wl_buffer *buffer)
{
  cairo_surface_t *surface = data;

  /* Note: the wl_buffer is destroyed as cairo user data */
  cairo_surface_destroy (surface);
}

static const struct wl_buffer_listener shm_buffer_listener = {
  shm_buffer_release,
};

/* }}} */

struct wl_buffer *
_gdk_wayland_shm_texture_get_wl_buffer (GdkWaylandDisplay *display,
                                        GdkTexture        *texture)
{
  int width, height;
  cairo_surface_t *surface;
  GdkTextureDownloader *downloader;
  struct wl_buffer *buffer;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  surface = gdk_wayland_display_create_shm_surface (display, width, height);

  downloader = gdk_texture_downloader_new (texture);

  gdk_texture_downloader_download_into (downloader,
                                        cairo_image_surface_get_data (surface),
                                        cairo_image_surface_get_stride (surface));

  gdk_texture_downloader_free (downloader);

  buffer = _gdk_wayland_shm_surface_get_wl_buffer (surface);
  wl_buffer_add_listener (buffer, &shm_buffer_listener, surface);

  return buffer;
}

/* }}} */

/* vim:set foldmethod=marker: */
