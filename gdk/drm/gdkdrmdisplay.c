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
#include <string.h>
#include <unistd.h>

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

G_DEFINE_FINAL_TYPE (GdkDrmDisplay, gdk_drm_display, GDK_TYPE_DISPLAY)

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
  char *path = NULL;
  const char *name = display_name && display_name[0] ? display_name : "card0";

  if (g_str_has_prefix (name, "drm:"))
    path = g_strdup_printf ("/dev/dri/%s", name + 4);
  else if (g_str_has_prefix (name, "/dev/"))
    path = g_strdup (name);
  else
    path = g_strdup_printf ("/dev/dri/%s", name);

  self->drm_fd = open (path, O_RDWR | O_CLOEXEC);
  g_free (path);

  if (self->drm_fd < 0)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                   "Failed to open DRM device: %s", strerror (errno));
      return FALSE;
    }

  if (drmSetMaster (self->drm_fd) != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to become DRM master: %s", strerror (errno));
      close (self->drm_fd);
      self->drm_fd = -1;
      return FALSE;
    }

  return TRUE;
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

      monitor = _gdk_drm_monitor_new (self, &geometry, connector->connector_id);
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

  self->libinput = libinput_udev_create_context (&(struct libinput_interface){ 0 }, NULL, udev);
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
#ifdef HAVE_EGL
  if (self->egl_display)
    {
      eglTerminate (self->egl_display);
      self->egl_display = NULL;
    }
#endif
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
  if (self->gbm_device)
    {
#ifdef HAVE_EGL
      self->egl_display = eglGetDisplay ((EGLNativeDisplayType) self->gbm_device);
      if (self->egl_display != EGL_NO_DISPLAY)
        eglInitialize (self->egl_display, NULL, NULL);
#endif
    }

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
