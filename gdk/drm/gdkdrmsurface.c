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
  g_assert (GDK_IS_DRM_SURFACE (surface));

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
    *root_x = 0;

  if (root_y)
    *root_y = 0;
}

static gboolean
gdk_drm_surface_get_device_state (GdkSurface      *surface,
                                  GdkDevice       *device,
                                  double          *x,
                                  double          *y,
                                  GdkModifierType *mask)
{
  g_assert (GDK_IS_DRM_SURFACE (surface));
  g_assert (GDK_IS_DRM_DEVICE (device));
  g_assert (x != NULL);
  g_assert (y != NULL);
  g_assert (mask != NULL);

  *x = 0;
  *y = 0;
  *mask = 0;

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  return FALSE;
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
    *x = 0;

  if (y != NULL)
    *y = 0;

  if (width != NULL)
    *width = 0;

  if (height != NULL)
    *height = 0;
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
  return NULL;
}
