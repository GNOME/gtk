/* gdkdrmsurface.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkdebugprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplay.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockprivate.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"

#include "gdkdrmdevice.h"
#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmpopupsurface-private.h"
#include "gdkdrmsurface-private.h"

G_DEFINE_ABSTRACT_TYPE (GdkDrmSurface, gdk_drm_surface, GDK_TYPE_SURFACE)

static void
gdk_drm_surface_set_input_region (GdkSurface     *surface,
                                  cairo_region_t *region)
{
  g_assert (GDK_IS_DRM_SURFACE (surface));
}

static void
gdk_drm_surface_set_opaque_region (GdkSurface     *surface,
                                   cairo_region_t *region)
{
  g_assert (GDK_IS_DRM_SURFACE (surface));
}

static void
gdk_drm_surface_hide (GdkSurface *surface)
{
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_surface_get_display (surface));

  g_assert (GDK_IS_DRM_SURFACE (surface));

  _gdk_drm_display_remove_surface (display, GDK_DRM_SURFACE (surface));
  gdk_surface_set_is_mapped (surface, FALSE);
}

static double
gdk_drm_surface_get_scale (GdkSurface *surface)
{
  g_assert (GDK_IS_DRM_SURFACE (surface));

  return 1;
}

static void
gdk_drm_surface_get_root_coords (GdkSurface *surface,
                                 int         x,
                                 int         y,
                                 int        *root_x,
                                 int        *root_y)
{
  GdkDrmSurface *self = (GdkDrmSurface *)surface;

  g_assert (GDK_IS_DRM_SURFACE (self));

  if (root_x)
    *root_x = self->root_x + x;

  if (root_y)
    *root_y = self->root_y + y;
}

static gboolean
gdk_drm_surface_get_device_state (GdkSurface      *surface,
                                  GdkDevice       *device,
                                  double          *x,
                                  double          *y,
                                  GdkModifierType *mask)
{
  GdkDrmSurface *self = GDK_DRM_SURFACE (surface);
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_surface_get_display (surface));

  g_assert (GDK_IS_DRM_SURFACE (surface));
  g_assert (GDK_IS_DRM_DEVICE (device));
  g_assert (x != NULL);
  g_assert (y != NULL);
  g_assert (mask != NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  *x = display->pointer_x - self->root_x;
  *y = display->pointer_y - self->root_y;
  *mask = _gdk_drm_display_get_current_keyboard_modifiers (display) |
          _gdk_drm_display_get_current_mouse_modifiers (display);

  return TRUE;
}

static void
gdk_drm_surface_get_geometry (GdkSurface *surface,
                              int        *x,
                              int        *y,
                              int        *width,
                              int        *height)
{
  g_assert (GDK_IS_DRM_SURFACE (surface));

  if (x != NULL)
    *x = surface->x;

  if (y != NULL)
    *y = surface->y;

  if (width != NULL)
    *width = surface->width;

  if (height != NULL)
    *height = surface->height;
}

static GdkDrag *
gdk_drm_surface_drag_begin (GdkSurface         *surface,
                            GdkDevice          *device,
                            GdkContentProvider *content,
                            GdkDragAction       actions,
                            double              dx,
                            double              dy)
{
  g_assert (GDK_IS_DRM_SURFACE (surface));
  g_assert (GDK_IS_DRM_DEVICE (device));
  g_assert (GDK_IS_CONTENT_PROVIDER (content));

  return NULL;
}

static void
gdk_drm_surface_destroy (GdkSurface *surface,
                         gboolean    foreign_destroy)
{
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_surface_get_display (surface));

  _gdk_drm_display_remove_surface (display, GDK_DRM_SURFACE (surface));
}

static void
gdk_drm_surface_class_init (GdkDrmSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  surface_class->destroy = gdk_drm_surface_destroy;
  surface_class->drag_begin = gdk_drm_surface_drag_begin;
  surface_class->get_device_state = gdk_drm_surface_get_device_state;
  surface_class->get_geometry = gdk_drm_surface_get_geometry;
  surface_class->get_root_coords = gdk_drm_surface_get_root_coords;
  surface_class->get_scale = gdk_drm_surface_get_scale;
  surface_class->hide = gdk_drm_surface_hide;
  surface_class->set_input_region = gdk_drm_surface_set_input_region;
  surface_class->set_opaque_region = gdk_drm_surface_set_opaque_region;
}

static void
gdk_drm_surface_init (GdkDrmSurface *self)
{
}

void
_gdk_drm_surface_move (GdkDrmSurface *surface,
                       float          x,
                       float          y)
{
  _gdk_drm_surface_move_resize (surface, x, y, -1, -1);
}

void
_gdk_drm_surface_move_resize (GdkDrmSurface *surface,
                              float          x,
                              float          y,
                              float          width,
                              float          height)
{
  GdkSurface *s = GDK_SURFACE (surface);

  surface->root_x = (int) x;
  surface->root_y = (int) y;
  s->x = (int) x;
  s->y = (int) y;
  if (width >= 0)
    s->width = (int) width;
  if (height >= 0)
    s->height = (int) height;
}

void
_gdk_drm_surface_show (GdkDrmSurface *surface)
{
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_surface_get_display (GDK_SURFACE (surface)));

  _gdk_drm_display_add_surface (display, surface);
  gdk_surface_set_is_mapped (GDK_SURFACE (surface), TRUE);
}

void
gdk_drm_surface_set_position (GdkDrmSurface *surface,
                              int            x,
                              int            y)
{
  g_return_if_fail (GDK_IS_DRM_SURFACE (surface));

  _gdk_drm_surface_move (surface, (float) x, (float) y);
}

void
_gdk_drm_surface_get_root_coords (GdkDrmSurface *surface,
                                  int           *root_x,
                                  int           *root_y)
{
  *root_x = surface->root_x;
  *root_y = surface->root_y;
}

GdkMonitor *
_gdk_drm_surface_get_best_monitor (GdkDrmSurface *surface)
{
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_surface_get_display (GDK_SURFACE (surface)));

  return gdk_display_get_monitor_at_surface (GDK_DISPLAY (display), GDK_SURFACE (surface));
}
