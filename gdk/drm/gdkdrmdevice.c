/* gdkdrmdevice.c
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

#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"

#include "gdkdrmdevice.h"
#include "gdkdrmdevice-private.h"
#include "gdkdrmdisplay-private.h"
#include "gdkdrmsurface-private.h"

struct _GdkDrmDevice
{
  GdkDevice parent_instance;
};

struct _GdkDrmDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkDrmDevice, gdk_drm_device, GDK_TYPE_DEVICE)

static void
gdk_drm_device_set_surface_cursor (GdkDevice  *device,
                                   GdkSurface *surface,
                                   GdkCursor  *cursor)
{
  g_assert (GDK_IS_DRM_DEVICE (device));
  g_assert (GDK_IS_DRM_SURFACE (surface));
  g_assert (!cursor || GDK_IS_CURSOR (cursor));

}

static GdkSurface *
gdk_drm_device_surface_at_position (GdkDevice       *device,
                                    double          *win_x,
                                    double          *win_y,
                                    GdkModifierType *state)
{
  GdkDrmDisplay *display;
  GdkDrmSurface *surface;
  graphene_point_t point;
  int x;
  int y;

  g_assert (GDK_IS_DRM_DEVICE (device));
  g_assert (win_x != NULL);
  g_assert (win_y != NULL);

  display = GDK_DRM_DISPLAY (gdk_device_get_display (device));

  if (state != NULL)
    *state = (_gdk_drm_display_get_current_keyboard_modifiers (display) |
              _gdk_drm_display_get_current_mouse_modifiers (display));

#if 1
  point = GRAPHENE_POINT_INIT (0, 0);
#endif

#if 0
  surface = _gdk_drm_display_get_surface_at_display_coords (display, point.x, point.y, &x, &y);
#else
  surface = NULL;
#endif

  *win_x = x;
  *win_y = y;

  return GDK_SURFACE (surface);
}

static GdkGrabStatus
gdk_drm_device_grab (GdkDevice    *device,
                     GdkSurface   *window,
                     gboolean      owner_events,
                     GdkEventMask  event_mask,
                     GdkSurface   *confine_to,
                     GdkCursor    *cursor,
                     guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_drm_device_ungrab (GdkDevice *device,
                       guint32    time_)
{
  GdkDrmDevice *self = (GdkDrmDevice *)device;
  GdkDeviceGrabInfo *grab;
  GdkDisplay *display;

  g_assert (GDK_IS_DRM_DEVICE (self));

  display = gdk_device_get_display (device);
  grab = _gdk_display_get_last_device_grab (display, device);

  if (grab != NULL)
    grab->serial_end = grab->serial_start;

  _gdk_display_device_grab_update (display, device, 0);
}

void
gdk_drm_device_query_state (GdkDevice        *device,
                            GdkSurface       *surface,
                            GdkSurface      **child_surface,
                            double           *win_x,
                            double           *win_y,
                            GdkModifierType  *mask)
{
  GdkDisplay *display;
  graphene_point_t point;
  int sx = 0;
  int sy = 0;
  int x_tmp;
  int y_tmp;

  g_assert (GDK_IS_DRM_DEVICE (device));
  g_assert (!surface || GDK_IS_DRM_SURFACE (surface));

  if (child_surface)
    *child_surface = surface;

  display = gdk_device_get_display (device);

#if 0
  point = [NSEvent mouseLocation];
#else
  point = GRAPHENE_POINT_INIT (0, 0);
#endif

  _gdk_drm_display_from_display_coords (GDK_DRM_DISPLAY (display),
                                        point.x, point.y,
                                        &x_tmp, &y_tmp);

  if (surface)
    _gdk_drm_surface_get_root_coords (GDK_DRM_SURFACE (surface), &sx, &sy);

  if (win_x)
    *win_x = x_tmp - sx;

  if (win_y)
    *win_y = y_tmp - sy;

  if (mask)
    *mask = _gdk_drm_display_get_current_keyboard_modifiers (GDK_DRM_DISPLAY (display)) |
            _gdk_drm_display_get_current_mouse_modifiers (GDK_DRM_DISPLAY (display));
}

static void
gdk_drm_device_class_init (GdkDrmDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->grab = gdk_drm_device_grab;
  device_class->set_surface_cursor = gdk_drm_device_set_surface_cursor;
  device_class->surface_at_position = gdk_drm_device_surface_at_position;
  device_class->ungrab = gdk_drm_device_ungrab;
}

static void
gdk_drm_device_init (GdkDrmDevice *self)
{
  _gdk_device_add_axis (GDK_DEVICE (self), GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (GDK_DEVICE (self), GDK_AXIS_Y, 0, 0, 1);
}
