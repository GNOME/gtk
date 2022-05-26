/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
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
#include <gtk/gtk.h>
#include <gdk/wayland/gdkwayland.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static struct wl_compositor *wl_compositor;
static struct wl_subcompositor *wl_subcompositor;
static struct wl_shm *wl_shm;

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  //g_print ("global: %s %u, id %u\n", interface, version, id);
  if (strcmp (interface, "wl_compositor") == 0)
    wl_compositor = wl_registry_bind (registry, id, &wl_compositor_interface, version);

  if (strcmp (interface, "wl_subcompositor") == 0)
    wl_subcompositor = wl_registry_bind (registry, id, &wl_subcompositor_interface, version);

  if (strcmp (interface, "wl_shm") == 0)
    wl_shm = wl_registry_bind (registry, id, &wl_shm_interface, version);
}

static void
gdk_registry_handle_global_remove (void               *data,
                                   struct wl_registry *registry,
                                   uint32_t            id)
{
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

static void
set_up_registry (struct wl_display *wl_display)
{
  struct wl_registry *wl_registry;

  wl_registry = wl_display_get_registry (wl_display);
  wl_registry_add_listener (wl_registry, &registry_listener, NULL);
  if (wl_display_roundtrip (wl_display) < 0)
    exit (1);

  g_assert (wl_compositor != NULL);
  g_assert (wl_subcompositor != NULL);
  g_assert (wl_shm != NULL);
}

static void
create_subsurface (struct wl_surface     *parent,
                   struct wl_surface    **child,
                   struct wl_subsurface **subsurface)
{
  *child = wl_compositor_create_surface (wl_compositor);
  *subsurface = wl_subcompositor_get_subsurface (wl_subcompositor, *child, parent);
  wl_subsurface_set_desync (*subsurface);
}

static void
surface_fill (struct wl_surface *surface,
              int                width,
              int                height)
{
  size_t size;
  const char *xdg_runtime_dir;
  int fd;
  guint32 *data;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;

  size = width * height * 4;
  xdg_runtime_dir = getenv ("XDG_RUNTIME_DIR");
  fd = open (xdg_runtime_dir, O_TMPFILE|O_RDWR|O_EXCL, 0600);
  ftruncate (fd, size);
  data = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  for (int i = 0; i < height; i++)
    for (int j = 0; j < width; j++)
      data[i * width + j] = 0xff00ff00;

  pool = wl_shm_create_pool (wl_shm, fd, size);
  buffer = wl_shm_pool_create_buffer (pool, 0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy (pool);

  close (fd);

  wl_surface_attach (surface, buffer, 0, 0);
  wl_surface_commit (surface);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GdkSurface *surface;
  struct wl_display *wl_display;
  struct wl_surface *parent;
  struct wl_surface *child;
  struct wl_subsurface *subsurface;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "hello world");

  gtk_widget_show (window);

  surface = gtk_native_get_surface (GTK_NATIVE (window));

  wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
  parent = gdk_wayland_surface_get_wl_surface (surface);

  set_up_registry (wl_display);
  create_subsurface (parent, &child, &subsurface);
  wl_subsurface_set_position (subsurface, 100, 100);
  surface_fill (child, 100, 100);

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
