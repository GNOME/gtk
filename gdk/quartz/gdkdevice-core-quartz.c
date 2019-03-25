/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2010 Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdkdeviceprivate.h>
#include <gdk/gdkdisplayprivate.h>

#import "GdkQuartzView.h"
#include "gdkquartzsurface.h"
#include "gdkquartzcursor.h"
#include "gdkprivate-quartz.h"
#include "gdkquartzdevice-core.h"

struct _GdkQuartzDeviceCore
{
  GdkDevice parent_instance;
};

struct _GdkQuartzDeviceCoreClass
{
  GdkDeviceClass parent_class;
};

static gboolean gdk_quartz_device_core_get_history (GdkDevice      *device,
                                                    GdkSurface      *window,
                                                    guint32         start,
                                                    guint32         stop,
                                                    GdkTimeCoord ***events,
                                                    gint           *n_events);
static void gdk_quartz_device_core_get_state (GdkDevice       *device,
                                              GdkSurface       *window,
                                              gdouble         *axes,
                                              GdkModifierType *mask);
static void gdk_quartz_device_core_set_surface_cursor (GdkDevice *device,
                                                      GdkSurface *window,
                                                      GdkCursor *cursor);
static void gdk_quartz_device_core_warp (GdkDevice *device,
                                         gdouble    x,
                                         gdouble    y);
static void gdk_quartz_device_core_query_state (GdkDevice        *device,
                                                GdkSurface        *window,
                                                GdkSurface       **root_window,
                                                GdkSurface       **child_window,
                                                gdouble          *root_x,
                                                gdouble          *root_y,
                                                gdouble          *win_x,
                                                gdouble          *win_y,
                                                GdkModifierType  *mask);
static GdkGrabStatus gdk_quartz_device_core_grab   (GdkDevice     *device,
                                                    GdkSurface     *window,
                                                    gboolean       owner_events,
                                                    GdkEventMask   event_mask,
                                                    GdkSurface     *confine_to,
                                                    GdkCursor     *cursor,
                                                    guint32        time_);
static void          gdk_quartz_device_core_ungrab (GdkDevice     *device,
                                                    guint32        time_);
static GdkSurface * gdk_quartz_device_core_surface_at_position (GdkDevice       *device,
                                                              gdouble         *win_x,
                                                              gdouble         *win_y,
                                                              GdkModifierType *mask,
                                                              gboolean         get_toplevel);


G_DEFINE_TYPE (GdkQuartzDeviceCore, gdk_quartz_device_core, GDK_TYPE_DEVICE)

static void
gdk_quartz_device_core_class_init (GdkQuartzDeviceCoreClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_quartz_device_core_get_history;
  device_class->get_state = gdk_quartz_device_core_get_state;
  device_class->set_surface_cursor = gdk_quartz_device_core_set_surface_cursor;
  device_class->warp = gdk_quartz_device_core_warp;
  device_class->query_state = gdk_quartz_device_core_query_state;
  device_class->grab = gdk_quartz_device_core_grab;
  device_class->ungrab = gdk_quartz_device_core_ungrab;
  device_class->surface_at_position = gdk_quartz_device_core_surface_at_position;
}

static void
gdk_quartz_device_core_init (GdkQuartzDeviceCore *quartz_device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (quartz_device_core);

  _gdk_device_add_axis (device, NULL, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, NULL, GDK_AXIS_Y, 0, 0, 1);
}

static gboolean
gdk_quartz_device_core_get_history (GdkDevice      *device,
                                    GdkSurface      *window,
                                    guint32         start,
                                    guint32         stop,
                                    GdkTimeCoord ***events,
                                    gint           *n_events)
{
  return FALSE;
}

static void
gdk_quartz_device_core_get_state (GdkDevice       *device,
                                  GdkSurface       *window,
                                  gdouble         *axes,
                                  GdkModifierType *mask)
{
  gdouble x_pos, y_pos;

  gdk_surface_get_device_position (window, device, &x_pos, &y_pos, mask);

  if (axes)
    {
      axes[0] = x_pos;
      axes[1] = y_pos;
    }
}

static void
translate_coords_to_child_coords (GdkSurface *parent,
                                  GdkSurface *child,
                                  gint      *x,
                                  gint      *y)
{
  GdkSurface *current = child;

  if (child == parent)
    return;

  while (current != parent)
    {
      gint tmp_x, tmp_y;

      gdk_surface_get_origin (current, &tmp_x, &tmp_y);

      *x -= tmp_x;
      *y -= tmp_y;

      current = gdk_surface_get_parent (current);
    }
}

static void
gdk_quartz_device_core_set_surface_cursor (GdkDevice *device,
                                          GdkSurface *window,
                                          GdkCursor *cursor)
{
  NSCursor *nscursor;

  if (GDK_SURFACE_DESTROYED (window))
    return;

  nscursor = _gdk_quartz_cursor_get_ns_cursor (cursor);

  [nscursor set];
}

static void
gdk_quartz_device_core_warp (GdkDevice *device,
                             gdouble    x,
                             gdouble    y)
{
  CGDisplayMoveCursorToPoint (CGMainDisplayID (), CGPointMake (x, y));
}

static GdkSurface *
gdk_quartz_device_core_query_state_helper (GdkSurface       *window,
                                           GdkDevice       *device,
                                           gdouble         *x,
                                           gdouble         *y,
                                           GdkModifierType *mask)
{
  GdkSurface *toplevel;
  NSPoint point;
  gint x_tmp, y_tmp;
  GdkSurface *found_window;

  g_return_val_if_fail (window == NULL || GDK_IS_SURFACE (window), NULL);

  if (GDK_SURFACE_DESTROYED (window))
    {
      *x = 0;
      *y = 0;
      *mask = 0;
      return NULL;
    }

  toplevel = gdk_surface_get_toplevel (window);

  if (mask)
    *mask = _gdk_quartz_events_get_current_keyboard_modifiers () |
        _gdk_quartz_events_get_current_mouse_modifiers ();

  /* Get the y coordinate, needs to be flipped. */
  if (window == _gdk_root)
    {
      point = [NSEvent mouseLocation];
      _gdk_quartz_surface_nspoint_to_gdk_xy (point, &x_tmp, &y_tmp);
    }
  else
    {
      GdkSurfaceImplQuartz *impl;
      NSWindow *nswindow;

      impl = GDK_SURFACE_IMPL_QUARTZ (toplevel->impl);
      nswindow = impl->toplevel;

      point = [nswindow mouseLocationOutsideOfEventStream];

      x_tmp = point.x;
      y_tmp = toplevel->height - point.y;

      window = toplevel;
    }

  found_window = _gdk_quartz_surface_find_child (window, x_tmp, y_tmp,
                                                FALSE);

  if (found_window == _gdk_root)
    found_window = NULL;
  else if (found_window)
    translate_coords_to_child_coords (window, found_window,
                                      &x_tmp, &y_tmp);

  if (x)
    *x = x_tmp;

  if (y)
    *y = y_tmp;

  return found_window;
}

static void
gdk_quartz_device_core_query_state (GdkDevice        *device,
                                    GdkSurface        *window,
                                    GdkSurface       **child_window,
                                    gdouble          *root_x,
                                    gdouble          *root_y,
                                    gdouble          *win_x,
                                    gdouble          *win_y,
                                    GdkModifierType  *mask)
{
  GdkSurface *found_window;
  NSPoint point;
  gint x_tmp, y_tmp;

  if (window == NULL)
    window = _gdk_root;

  found_window = gdk_quartz_device_core_query_state_helper (window, device,
                                                            win_x, win_y,
                                                            mask);

  if (root_window)
    *root_window = _gdk_root;

  if (child_window)
    *child_window = found_window;

  point = [NSEvent mouseLocation];
  _gdk_quartz_surface_nspoint_to_gdk_xy (point, &x_tmp, &y_tmp);

  if (root_x)
    *root_x = x_tmp;

  if (root_y)
    *root_y = y_tmp;
}

static GdkGrabStatus
gdk_quartz_device_core_grab (GdkDevice    *device,
                             GdkSurface    *window,
                             gboolean      owner_events,
                             GdkEventMask  event_mask,
                             GdkSurface    *confine_to,
                             GdkCursor    *cursor,
                             guint32       time_)
{
  /* Should remain empty */
  return GDK_GRAB_SUCCESS;
}

static void
gdk_quartz_device_core_ungrab (GdkDevice *device,
                               guint32    time_)
{
  GdkDeviceGrabInfo *grab;

  grab = _gdk_display_get_last_device_grab (_gdk_display, device);
  if (grab)
    grab->serial_end = 0;

  _gdk_display_device_grab_update (_gdk_display, device, NULL, 0);
}

static GdkSurface *
gdk_quartz_device_core_surface_at_position (GdkDevice       *device,
                                           gdouble         *win_x,
                                           gdouble         *win_y,
                                           GdkModifierType *mask,
                                           gboolean         get_toplevel)
{
  GdkSurface *found_window;
  NSPoint point;
  gint x_tmp, y_tmp;

  /* Get mouse coordinates, find window under the mouse pointer */
  point = [NSEvent mouseLocation];
  _gdk_quartz_surface_nspoint_to_gdk_xy (point, &x_tmp, &y_tmp);

  found_window = _gdk_quartz_surface_find_child (_gdk_root, x_tmp, y_tmp, get_toplevel);

  if (found_window)
    translate_coords_to_child_coords (_gdk_root, found_window, &x_tmp, &y_tmp);

  if (win_x)
    *win_x = found_window ? x_tmp : -1;

  if (win_y)
    *win_y = found_window ? y_tmp : -1;

  if (mask)
    *mask = _gdk_quartz_events_get_current_keyboard_modifiers () |
        _gdk_quartz_events_get_current_mouse_modifiers ();

  return found_window;
}

