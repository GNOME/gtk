/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <libinput.h>
#include <libudev.h>

#ifdef HAVE_EGL
#include <epoxy/egl.h>
#endif

#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"

#include "gdkdrmcairocontext-private.h"
#include "gdkdrmclipboard-private.h"
#include "gdkdrmdisplay-private.h"
#include "gdkdrmglcontext-private.h"
#include "gdkdrmkeymap-private.h"
#include "gdkdrminput-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmpopupsurface-private.h"
#include "gdkdrmseat-private.h"
#include "gdkdrmsurface-private.h"
#include "gdkdrmtoplevelsurface-private.h"

#include <glib/gstdio.h>

G_DEFINE_FINAL_TYPE (GdkDrmDisplay, gdk_drm_display, GDK_TYPE_DISPLAY)

/* Check if a DRM fd has at least one connected connector (can drive a display).
 * Used to prefer a card that has outputs when multiple cards exist. */
static gboolean
drm_device_has_connected_connector (int fd)
{
  drmModeRes *resources;
  int i;
  gboolean found = FALSE;

  resources = drmModeGetResources (fd);
  if (!resources)
    return FALSE;

  for (i = 0; i < resources->count_connectors && !found; i++)
    {
      drmModeConnector *connector = drmModeGetConnector (fd, resources->connectors[i]);
      if (connector && connector->connection == DRM_MODE_CONNECTED)
        found = TRUE;
      if (connector)
        drmModeFreeConnector (connector);
    }
  drmModeFreeResources (resources);
  return found;
}

/* Priority for device selection, matching mutter's choose_primary_gpu order:
 * 1) udev tag mutter-device-preferred-primary, 2) platform (integrated), 3) boot VGA, 4) any. */
typedef struct { char *path; int priority; } DrmCandidate;

static void
drm_candidate_free (DrmCandidate *c)
{
  g_free (c->path);
}

static int
drm_candidate_compare (const DrmCandidate *a,
                       const DrmCandidate *b)
{
  int cmp = b->priority - a->priority;
  return cmp ? cmp : g_strcmp0 (a->path, b->path);
}

static gboolean
udev_device_is_platform (struct udev_device *device)
{
  struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype (
    device, "platform", NULL);
  return parent != NULL;
}

static gboolean
udev_device_is_boot_vga (struct udev_device *device)
{
  struct udev_device *pci;
  const char *vga_str;

  pci = udev_device_get_parent_with_subsystem_devtype (device, "pci", NULL);
  if (!pci)
    return FALSE;
  vga_str = udev_device_get_sysattr_value (pci, "boot_vga");
  return vga_str && vga_str[0] == '1' && vga_str[1] == '\0';
}

/* Enumerate DRM cards via udev (like mutter), filter by seat and type, and sort
 * by preferred-primary > platform > boot_vga > other. Returns a GArray of
 * DrmCandidate; caller frees array and path in each element. */
static GArray *
drm_discover_via_udev (struct udev *udev,
                       const char  *seat_id)
{
  struct udev_enumerate *enumerate;
  struct udev_list_entry *entry;
  GArray *candidates;
  const char *syspath;

  enumerate = udev_enumerate_new (udev);
  if (!enumerate)
    return NULL;

  udev_enumerate_add_match_subsystem (enumerate, "drm");
  udev_enumerate_scan_devices (enumerate);

  candidates = g_array_new (TRUE, TRUE, sizeof (DrmCandidate));

  udev_list_entry_foreach (entry, udev_enumerate_get_list_entry (enumerate))
    {
      struct udev_device *device;
      const char *devnode, *devtype, *device_seat, *sysname;
      int priority = 0;
      DrmCandidate c = { 0 };

      syspath = udev_list_entry_get_name (entry);
      device = udev_device_new_from_syspath (udev, syspath);
      if (!device)
        continue;

      devtype = udev_device_get_property_value (device, "DEVTYPE");
      if (!g_str_equal (devtype, "drm_minor"))
        {
          udev_device_unref (device);
          continue;
        }

      sysname = udev_device_get_sysname (device);
      if (!sysname || !g_str_has_prefix (sysname, "card"))
        {
          udev_device_unref (device);
          continue;
        }
      if (sysname[4] < '0' || sysname[4] > '9')
        {
          udev_device_unref (device);
          continue;
        }

      device_seat = udev_device_get_property_value (device, "ID_SEAT");
      if (!device_seat)
        device_seat = "seat0";
      if (!g_str_equal (device_seat, seat_id))
        {
          udev_device_unref (device);
          continue;
        }

      devnode = udev_device_get_devnode (device);
      if (!devnode)
        {
          udev_device_unref (device);
          continue;
        }

      if (udev_device_has_tag (device, "mutter-device-preferred-primary") != 0)
        priority = 4;
      else if (udev_device_is_platform (device))
        priority = 3;
      else if (udev_device_is_boot_vga (device))
        priority = 2;
      else
        priority = 1;

      c.path = g_strdup (devnode);
      c.priority = priority;
      g_array_append_val (candidates, c);
      udev_device_unref (device);
    }

  udev_enumerate_unref (enumerate);

  if (candidates->len == 0)
    {
      g_array_free (candidates, TRUE);
      return NULL;
    }

  g_array_sort (candidates, (GCompareFunc) drm_candidate_compare);
  return candidates;
}

/* Fallback: build a sorted list of DRM card names from /dev/dri/ when udev
 * is unavailable or returns no devices. Returns GList of newly allocated
 * strings; caller frees list and elements. */
static GList *
drm_discover_card_names_fallback (void)
{
  GDir *dir;
  const char *ent;
  GList *names = NULL;

  dir = g_dir_open ("/dev/dri", 0, NULL);
  if (!dir)
    return NULL;

  while ((ent = g_dir_read_name (dir)) != NULL)
    {
      if (g_str_has_prefix (ent, "card") && ent[4] != '\0')
        {
          const char *p = ent + 4;
          while (*p >= '0' && *p <= '9')
            p++;
          if (*p == '\0')
            names = g_list_append (names, g_strdup (ent));
        }
    }
  g_dir_close (dir);

  if (!names)
    return NULL;

  names = g_list_sort (names, (GCompareFunc) g_strcmp0);
  return names;
}

/* Try to open and take master on a single DRM device. If successful, sets
 * self->drm_fd and returns TRUE. If prefer_connected is TRUE and the device
 * has no connected connectors, leaves fd closed and returns FALSE so the
 * caller can try the next card. */
static gboolean
try_open_drm_device (GdkDrmDisplay *self,
                    const char    *path,
                    gboolean       prefer_connected,
                    GError       **error)
{
  int fd;

  fd = open (path, O_RDWR | O_CLOEXEC);
  if (fd < 0)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                   "Failed to open DRM device %s: %s", path, strerror (errno));
      return FALSE;
    }

  if (drmSetMaster (fd) != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to become DRM master on %s: %s", path, strerror (errno));
      close (fd);
      return FALSE;
    }

  if (prefer_connected && !drm_device_has_connected_connector (fd))
    {
      drmDropMaster (fd);
      close (fd);
      g_clear_error (error);
      return FALSE;
    }

  self->drm_fd = fd;
  return TRUE;
}

typedef struct
{
  GSource parent_instance;
  GdkDrmDisplay *display;
  GPollFD poll_fd;
} GdkDrmDrmSource;

static gboolean
drm_source_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)
{
  GdkDrmDrmSource *drm_source = (GdkDrmDrmSource *) source;
  _gdk_drm_display_process_events (drm_source->display);
  return G_SOURCE_CONTINUE;
}

static void
drm_source_finalize (GSource *source)
{
  GdkDrmDrmSource *drm_source = (GdkDrmDrmSource *) source;
  if (drm_source->display)
    drm_source->display->drm_source = NULL;
}

static GSourceFuncs drm_source_funcs = {
  NULL,
  NULL,
  drm_source_dispatch,
  drm_source_finalize
};

void
_gdk_drm_display_process_events (GdkDrmDisplay *display)
{
  char buf[1024];
  ssize_t n;

  if (display->drm_fd < 0)
    return;

  while ((n = read (display->drm_fd, buf, sizeof (buf))) > 0)
    {
      char *p = buf;
      while (p < buf + n)
        {
          struct drm_event *ev = (struct drm_event *) p;
          if (p + ev->length > buf + n)
            break;
          if (ev->type == DRM_EVENT_FLIP_COMPLETE)
            {
              struct drm_event_vblank *vbl = (struct drm_event_vblank *) ev;
              guint32 crtc_id = (guint32) (uintptr_t) vbl->user_data;
              if (crtc_id != 0)
                g_hash_table_remove (display->page_flip_pending, GUINT_TO_POINTER (crtc_id));
            }
          p += ev->length;
        }
    }
}

void
_gdk_drm_display_wait_page_flip (GdkDrmDisplay *display,
                                 guint32         crtc_id)
{
  const int timeout_ms = 100;
  int elapsed = 0;

  while (_gdk_drm_display_is_page_flip_pending (display, crtc_id) && elapsed < 5000)
    {
      _gdk_drm_display_process_events (display);
      if (_gdk_drm_display_is_page_flip_pending (display, crtc_id))
        {
          struct pollfd pfd = { display->drm_fd, POLLIN, 0 };
          int r = poll (&pfd, 1, timeout_ms);
          if (r > 0)
            _gdk_drm_display_process_events (display);
          elapsed += timeout_ms;
        }
    }
}

void
_gdk_drm_display_mark_page_flip_pending (GdkDrmDisplay *display,
                                         guint32        crtc_id)
{
  g_hash_table_insert (display->page_flip_pending,
                       GUINT_TO_POINTER (crtc_id),
                       GINT_TO_POINTER (1));
}

gboolean
_gdk_drm_display_is_page_flip_pending (GdkDrmDisplay *display,
                                       guint32        crtc_id)
{
  return g_hash_table_contains (display->page_flip_pending,
                                GUINT_TO_POINTER (crtc_id));
}

static GdkDrmMonitor *
get_monitor_unowned (GdkDrmDisplay *self,
                     guint          position)
{
  GdkDrmMonitor *monitor;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), position);
  if (monitor != NULL)
    g_object_unref (monitor);

  return monitor;
}

static gboolean
open_drm_device (GdkDrmDisplay *self,
                 const char    *display_name,
                 GError       **error)
{
  gboolean explicit_name = display_name && display_name[0];
  char *path = NULL;
  const char *name;

  if (explicit_name)
    {
      name = display_name;
      if (g_str_has_prefix (name, "drm:"))
        path = g_strdup_printf ("/dev/dri/%s", name + 4);
      else if (g_str_has_prefix (name, "/dev/"))
        path = g_strdup (name);
      else
        path = g_strdup_printf ("/dev/dri/%s", name);

      if (try_open_drm_device (self, path, FALSE, error))
        {
          g_free (path);
          return TRUE;
        }
      g_free (path);
      return FALSE;
    }

  /* No display name: discover DRM cards (mutter-style udev, then fallback).
   * Order: udev list (preferred-primary > platform > boot_vga > other), then
   * /dev/dri scan, then card0. Prefer a card with a connected connector. */
  {
    struct udev *udev = udev_new ();
    GArray *udev_candidates = udev ? drm_discover_via_udev (udev, "seat0") : NULL;
    if (udev)
      udev_unref (udev);

    if (udev_candidates && udev_candidates->len > 0)
      {
        guint i;
        gboolean opened = FALSE;

        for (i = 0; i < udev_candidates->len; i++)
          {
            DrmCandidate *c = &g_array_index (udev_candidates, DrmCandidate, i);
            if (try_open_drm_device (self, c->path, TRUE, error))
              {
                g_free (self->name);
                self->name = g_path_get_basename (c->path);
                opened = TRUE;
                break;
              }
          }
        if (!opened)
          {
            g_clear_error (error);
            for (i = 0; i < udev_candidates->len; i++)
              {
                DrmCandidate *c = &g_array_index (udev_candidates, DrmCandidate, i);
                if (try_open_drm_device (self, c->path, FALSE, error))
                  {
                    g_free (self->name);
                    self->name = g_path_get_basename (c->path);
                    opened = TRUE;
                    break;
                  }
                g_clear_error (error);
              }
          }
        for (i = 0; i < udev_candidates->len; i++)
          drm_candidate_free (&g_array_index (udev_candidates, DrmCandidate, i));
        g_array_free (udev_candidates, TRUE);
        if (opened)
          return TRUE;
      }
  }

  /* Fallback: scan /dev/dri/ for card* (no udev or no udev seats) */
  {
    GList *cards = drm_discover_card_names_fallback ();
    if (cards)
      {
        GList *l;
        for (l = cards; l != NULL; l = l->next)
          {
            const char *card_name = l->data;
            char *card_path = g_strdup_printf ("/dev/dri/%s", card_name);
            if (try_open_drm_device (self, card_path, TRUE, error))
              {
                g_free (self->name);
                self->name = g_strdup (card_name);
                g_free (card_path);
                g_list_free_full (cards, g_free);
                return TRUE;
              }
            g_free (card_path);
          }
        g_clear_error (error);
        for (l = cards; l != NULL; l = l->next)
          {
            const char *card_name = l->data;
            char *card_path = g_strdup_printf ("/dev/dri/%s", card_name);
            if (try_open_drm_device (self, card_path, FALSE, error))
              {
                g_free (self->name);
                self->name = g_strdup (card_name);
                g_free (card_path);
                g_list_free_full (cards, g_free);
                return TRUE;
              }
            g_clear_error (error);
            g_free (card_path);
          }
        g_list_free_full (cards, g_free);
      }
  }

  /* Last resort: try legacy "card0" */
  if (try_open_drm_device (self, "/dev/dri/card0", FALSE, error))
    {
      g_free (self->name);
      self->name = g_strdup ("card0");
      return TRUE;
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "No usable DRM device found. Check /dev/dri/ and DRM permissions.");
  return FALSE;
}

static void
load_monitors (GdkDrmDisplay *self)
{
  drmModeRes *resources;
  int i;
  int x_offset = 0;

  resources = drmModeGetResources (self->drm_fd);
  if (!resources)
    return;

  for (i = 0; i < resources->count_connectors; i++)
    {
      drmModeConnector *connector;
      drmModeCrtc *crtc = NULL;
      GdkDrmMonitor *monitor;
      GdkRectangle geometry;

      connector = drmModeGetConnector (self->drm_fd, resources->connectors[i]);
      if (!connector || connector->connection != DRM_MODE_CONNECTED)
        {
          if (connector)
            drmModeFreeConnector (connector);
          continue;
        }

      if (connector->encoder_id)
        {
          drmModeEncoder *enc = drmModeGetEncoder (self->drm_fd, connector->encoder_id);
          if (enc && enc->crtc_id)
            crtc = drmModeGetCrtc (self->drm_fd, enc->crtc_id);
          if (enc)
            drmModeFreeEncoder (enc);
        }

      if (!connector->count_modes)
        {
          drmModeFreeConnector (connector);
          if (crtc)
            drmModeFreeCrtc (crtc);
          continue;
        }

      geometry.x = x_offset;
      geometry.y = 0;
      geometry.width = connector->modes[0].hdisplay;
      geometry.height = connector->modes[0].vdisplay;
      x_offset += geometry.width;

      monitor = _gdk_drm_monitor_new (self, &geometry,
                                      connector->connector_id,
                                      crtc ? crtc->crtc_id : 0,
                                      connector->count_modes > 0 ? &connector->modes[0] : NULL);
      g_list_store_append (self->monitors, monitor);
      g_object_unref (monitor);

      self->layout_bounds.width = x_offset;
      self->layout_bounds.height = MAX (self->layout_bounds.height, geometry.height);
      self->layout_bounds.x = 0;
      self->layout_bounds.y = 0;

      drmModeFreeConnector (connector);
      if (crtc)
        drmModeFreeCrtc (crtc);
    }

  drmModeFreeResources (resources);
}

static int
libinput_open_restricted (const char *path,
                         int         flags,
                         void       *user_data)
{
  int fd = open (path, flags | O_CLOEXEC);
  if (fd < 0)
    return -errno;
  return fd;
}

static void
libinput_close_restricted (int   fd,
                           void *user_data)
{
  close (fd);
}

static const struct libinput_interface libinput_interface = {
  .open_restricted = libinput_open_restricted,
  .close_restricted = libinput_close_restricted,
};

static gboolean
load_libinput (GdkDrmDisplay *self,
               GError       **error)
{
  struct udev *udev = udev_new ();
  if (!udev)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to create udev context");
      return FALSE;
    }

  self->libinput = libinput_udev_create_context (&libinput_interface, NULL, udev);
  udev_unref (udev);
  if (!self->libinput)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to create libinput context");
      return FALSE;
    }

  if (libinput_udev_assign_seat (self->libinput, "seat0") != 0)
    {
      libinput_unref (self->libinput);
      self->libinput = NULL;
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to assign seat");
      return FALSE;
    }

  return TRUE;
}

static gboolean
gdk_drm_display_get_setting (GdkDisplay *display,
                             const char *setting,
                             GValue     *value)
{
  return FALSE;
}

static GListModel *
gdk_drm_display_get_monitors (GdkDisplay *display)
{
  return G_LIST_MODEL (GDK_DRM_DISPLAY (display)->monitors);
}

static GdkMonitor *
gdk_drm_display_get_monitor_at_surface (GdkDisplay *display,
                                        GdkSurface *surface)
{
  GdkDrmDisplay *self = (GdkDrmDisplay *)display;
  GdkDrmSurface *drm_surface = GDK_DRM_SURFACE (surface);
  guint n_monitors;
  int cx, cy;

  g_assert (GDK_IS_DRM_DISPLAY (self));
  g_assert (GDK_IS_DRM_SURFACE (surface));

  cx = drm_surface->root_x;
  cy = drm_surface->root_y;
  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));

  for (guint i = 0; i < n_monitors; i++)
    {
      GdkDrmMonitor *monitor = get_monitor_unowned (self, i);
      const GdkRectangle *geom = &GDK_MONITOR (monitor)->geometry;

      if (cx >= geom->x && cy >= geom->y &&
          cx < geom->x + geom->width && cy < geom->y + geom->height)
        return GDK_MONITOR (monitor);
    }

  return GDK_MONITOR (get_monitor_unowned (self, 0));
}

static void
gdk_drm_display_load_seat (GdkDrmDisplay *self)
{
  GdkSeat *seat;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  seat = _gdk_drm_seat_new (self);
  gdk_display_add_seat (GDK_DISPLAY (self), seat);
  g_object_unref (seat);
}

GdkDrmSurface *
_gdk_drm_display_get_surface_at_display_coords (GdkDrmDisplay *self,
                                                int            x,
                                                int            y,
                                                int           *out_surface_x,
                                                int           *out_surface_y)
{
  GList *l;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  for (l = self->surfaces; l != NULL; l = l->next)
    {
      GdkDrmSurface *surface = l->data;
      int w, h;

      gdk_surface_get_geometry (GDK_SURFACE (surface), NULL, NULL, &w, &h);
      if (x >= surface->root_x && y >= surface->root_y &&
          x < surface->root_x + w && y < surface->root_y + h)
        {
          if (out_surface_x)
            *out_surface_x = x - surface->root_x;
          if (out_surface_y)
            *out_surface_y = y - surface->root_y;
          return surface;
        }
    }

  return NULL;
}

void
_gdk_drm_display_add_surface (GdkDrmDisplay *self,
                              GdkDrmSurface *surface)
{
  g_assert (GDK_IS_DRM_DISPLAY (self));
  g_assert (GDK_IS_DRM_SURFACE (surface));

  self->surfaces = g_list_prepend (self->surfaces, surface);
}

void
_gdk_drm_display_remove_surface (GdkDrmDisplay *self,
                                GdkDrmSurface *surface)
{
  g_assert (GDK_IS_DRM_DISPLAY (self));

  self->surfaces = g_list_remove (self->surfaces, surface);
}

void
_gdk_drm_display_set_pointer_position (GdkDrmDisplay *self,
                                       int            x,
                                       int            y)
{
  g_assert (GDK_IS_DRM_DISPLAY (self));

  self->pointer_x = CLAMP (x, self->layout_bounds.x,
                          self->layout_bounds.x + self->layout_bounds.width - 1);
  self->pointer_y = CLAMP (y, self->layout_bounds.y,
                          self->layout_bounds.y + self->layout_bounds.height - 1);
}

void
gdk_drm_display_set_monitor_layout (GdkDrmDisplay      *display,
                                    const GdkRectangle *geometries,
                                    guint               n_geometries)
{
  guint n_monitors;
  int min_x, min_y, max_x, max_y;
  guint i;

  g_return_if_fail (GDK_IS_DRM_DISPLAY (display));
  g_return_if_fail (geometries != NULL || n_geometries == 0);

  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (display->monitors));
  if (n_geometries != n_monitors)
    return;

  min_x = min_y = G_MAXINT;
  max_x = max_y = G_MININT;

  for (i = 0; i < n_geometries; i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (display->monitors), i);
      const GdkRectangle *r = &geometries[i];

      gdk_monitor_set_geometry (monitor, r);
      g_object_unref (monitor);

      min_x = MIN (min_x, r->x);
      min_y = MIN (min_y, r->y);
      max_x = MAX (max_x, r->x + r->width);
      max_y = MAX (max_y, r->y + r->height);
    }

  display->layout_bounds.x = min_x;
  display->layout_bounds.y = min_y;
  display->layout_bounds.width = max_x - min_x;
  display->layout_bounds.height = max_y - min_y;
}

void
gdk_drm_display_get_layout_bounds (GdkDrmDisplay *display,
                                   GdkRectangle  *bounds)
{
  g_return_if_fail (GDK_IS_DRM_DISPLAY (display));
  g_return_if_fail (bounds != NULL);

  *bounds = display->layout_bounds;
}

static const char *
gdk_drm_display_get_name (GdkDisplay *display)
{
  return GDK_DRM_DISPLAY (display)->name;
}

static void
gdk_drm_display_beep (GdkDisplay *display)
{
  /* Not Supported */
}

static void
gdk_drm_display_flush (GdkDisplay *display)
{
  /* Not Supported */
}

static void
gdk_drm_display_sync (GdkDisplay *display)
{
  /* Not Supported */
}

static gulong
gdk_drm_display_get_next_serial (GdkDisplay *display)
{
  static gulong serial = 0;
  return ++serial;
}

static void
gdk_drm_display_notify_startup_complete (GdkDisplay  *display,
                                           const char *startup_notification_id)
{
  /* Not Supported */
}

static GdkKeymap *
gdk_drm_display_get_keymap (GdkDisplay *display)
{
  GdkDrmDisplay *self = (GdkDrmDisplay *)display;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  return GDK_KEYMAP (self->keymap);
}

static void
gdk_drm_display_load_clipboard (GdkDrmDisplay *self)
{
  g_assert (GDK_IS_DRM_DISPLAY (self));

  GDK_DISPLAY (self)->clipboard = _gdk_drm_clipboard_new (self);
}

static GdkGLContext *
gdk_drm_display_init_gl (GdkDisplay  *display,
                         GError     **error)
{
  GdkDrmDisplay *self = GDK_DRM_DISPLAY (display);

  if (self->gbm_device && !_gdk_display_peek_egl_display (display))
    {
#ifdef HAVE_EGL
      if (!gdk_display_init_egl (display, 0, self->gbm_device, TRUE, error))
        return NULL;
#else
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           "EGL support not compiled in");
      return NULL;
#endif
    }

  return _gdk_drm_gl_context_new (GDK_DRM_DISPLAY (display), error);
}

static void
gdk_drm_display_queue_events (GdkDisplay *display)
{
  g_assert (GDK_IS_DRM_DISPLAY (display));

}

static void
gdk_drm_display_finalize (GObject *object)
{
  GdkDrmDisplay *self = (GdkDrmDisplay *)object;

  if (self->drm_source)
    {
      g_source_destroy (self->drm_source);
      g_clear_pointer (&self->drm_source, g_source_unref);
    }
  g_clear_pointer (&self->page_flip_pending, g_hash_table_unref);
  g_clear_pointer (&self->crtc_initialized, g_hash_table_unref);
  if (self->libinput_source)
    {
      g_source_destroy (self->libinput_source);
      g_clear_pointer (&self->libinput_source, g_source_unref);
    }
  if (self->libinput)
    {
      libinput_unref (self->libinput);
      self->libinput = NULL;
    }
  if (self->gbm_device)
    {
      gbm_device_destroy (self->gbm_device);
      self->gbm_device = NULL;
    }
  if (self->drm_fd >= 0)
    {
      drmDropMaster (self->drm_fd);
      close (self->drm_fd);
      self->drm_fd = -1;
    }

  g_clear_object (&GDK_DISPLAY (self)->clipboard);
  g_clear_object (&self->monitors);
  g_clear_object (&self->keymap);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gdk_drm_display_parent_class)->finalize (object);
}

static void
gdk_drm_display_class_init (GdkDrmDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->finalize = gdk_drm_display_finalize;

  display_class->toplevel_type = GDK_TYPE_DRM_TOPLEVEL_SURFACE;
  display_class->popup_type = GDK_TYPE_DRM_POPUP_SURFACE;
  display_class->cairo_context_type = GDK_TYPE_DRM_CAIRO_CONTEXT;

  display_class->beep = gdk_drm_display_beep;
  display_class->flush = gdk_drm_display_flush;
  display_class->get_keymap = gdk_drm_display_get_keymap;
  display_class->get_monitors = gdk_drm_display_get_monitors;
  display_class->get_monitor_at_surface = gdk_drm_display_get_monitor_at_surface;
  display_class->get_next_serial = gdk_drm_display_get_next_serial;
  display_class->get_name = gdk_drm_display_get_name;
  display_class->get_setting = gdk_drm_display_get_setting;
  display_class->init_gl = gdk_drm_display_init_gl;
  display_class->notify_startup_complete = gdk_drm_display_notify_startup_complete;
  display_class->queue_events = gdk_drm_display_queue_events;
  display_class->sync = gdk_drm_display_sync;
}

static void
gdk_drm_display_init (GdkDrmDisplay *self)
{
  self->monitors = g_list_store_new (GDK_TYPE_MONITOR);

  gdk_display_set_composited (GDK_DISPLAY (self), FALSE);
  gdk_display_set_input_shapes (GDK_DISPLAY (self), FALSE);
  gdk_display_set_rgba (GDK_DISPLAY (self), FALSE);
  gdk_display_set_shadow_width (GDK_DISPLAY (self), FALSE);
}

GdkDisplay *
_gdk_drm_display_open (const char *display_name)
{
  GdkDrmDisplay *self;
  GError *error = NULL;

  display_name = display_name ? display_name : "";
  GDK_DEBUG (MISC, "opening display %s", display_name);

  self = g_object_new (GDK_TYPE_DRM_DISPLAY, NULL);
  self->name = g_strdup (display_name);
  self->drm_fd = -1;

  if (!open_drm_device (self, display_name, &error))
    {
      g_warning ("Failed to open DRM device: %s", error->message);
      g_clear_error (&error);
      g_object_unref (self);
      return NULL;
    }

  self->gbm_device = gbm_create_device (self->drm_fd);

  load_monitors (self);

  if (self->layout_bounds.width > 0 && self->layout_bounds.height > 0)
    {
      self->pointer_x = self->layout_bounds.x + self->layout_bounds.width / 2;
      self->pointer_y = self->layout_bounds.y + self->layout_bounds.height / 2;
    }

  if (!load_libinput (self, &error))
    {
      g_warning ("Failed to init libinput: %s", error->message);
      g_clear_error (&error);
    }

  self->keymap = GDK_KEYMAP (_gdk_drm_keymap_new (self));
  gdk_drm_display_load_seat (self);
  gdk_drm_display_load_clipboard (self);

  if (self->libinput)
    {
      self->libinput_source = _gdk_drm_input_source_new (self);
      if (self->libinput_source)
        g_source_attach (self->libinput_source, NULL);
    }

  self->page_flip_pending = g_hash_table_new (NULL, NULL);
  self->crtc_initialized = g_hash_table_new (NULL, NULL);
  if (self->drm_fd >= 0)
    {
      GdkDrmDrmSource *drm_src = (GdkDrmDrmSource *) g_source_new (&drm_source_funcs, sizeof (GdkDrmDrmSource));
      drm_src->display = self;
      drm_src->poll_fd.fd = self->drm_fd;
      drm_src->poll_fd.events = G_IO_IN;
      g_source_add_poll ((GSource *) drm_src, &drm_src->poll_fd);
      g_source_set_priority ((GSource *) drm_src, G_PRIORITY_DEFAULT);
      self->drm_source = (GSource *) drm_src;
      g_source_attach ((GSource *) drm_src, NULL);
      g_source_set_static_name ((GSource *) drm_src, "[gtk] gdk-drm-display");
    }

  g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&self);
  gdk_display_emit_opened (GDK_DISPLAY (self));

  return GDK_DISPLAY (self);
}

GdkModifierType
_gdk_drm_display_get_current_keyboard_modifiers (GdkDrmDisplay *display)
{
  return display->keyboard_modifiers;
}

GdkModifierType
_gdk_drm_display_get_current_mouse_modifiers (GdkDrmDisplay *display)
{
  return display->mouse_modifiers;
}

void
_gdk_drm_display_from_display_coords (GdkDrmDisplay *self,
                                      int            x,
                                      int            y,
                                      int           *out_x,
                                      int           *out_y)
{
  *out_x = x;
  *out_y = y;
}
