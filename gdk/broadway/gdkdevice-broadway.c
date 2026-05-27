/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
#include <stdlib.h>

#include "gdkdevice-broadway.h"

#include "gdksurfaceprivate.h"
#include "gdkprivate-broadway.h"
#include "gdkseatprivate.h"

static void gdk_broadway_device_set_surface_cursor (GdkDevice *device,
                                                    GdkSurface *surface,
                                                    GdkCursor *cursor);
static GdkSurface * gdk_broadway_device_surface_at_position (GdkDevice       *device,
                                                             double          *win_x,
                                                             double          *win_y,
                                                             GdkModifierType *mask);


G_DEFINE_TYPE (GdkBroadwayDevice, gdk_broadway_device, GDK_TYPE_DEVICE)

static void
gdk_broadway_device_class_init (GdkBroadwayDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_broadway_device_set_surface_cursor;
  device_class->surface_at_position = gdk_broadway_device_surface_at_position;
}

static void
gdk_broadway_device_init (GdkBroadwayDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_AXIS_Y, 0, 0, 1);
}

static void
gdk_broadway_device_set_surface_cursor (GdkDevice *device,
                                        GdkSurface *surface,
                                        GdkCursor *cursor)
{
}

void
gdk_broadway_device_query_state (GdkDevice         *device,
                                 GdkSurface        *surface,
                                 double            *win_x,
                                 double            *win_y,
                                 GdkModifierType   *mask)
{
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  gint32 device_root_x, device_root_y;
  guint32 mouse_toplevel_id;
  guint32 mask32;
  int origin_x, origin_y;

  if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return;

  display = gdk_device_get_display (device);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  _gdk_broadway_server_query_mouse (broadway_display->server,
                                    &mouse_toplevel_id,
                                    &device_root_x,
                                    &device_root_y,
                                    &mask32);

  gdk_surface_get_origin (surface, &origin_x, &origin_y);

  if (win_x)
    *win_x = device_root_x - origin_x;
  if (win_y)
    *win_y = device_root_y - origin_y;
  if (mask)
    *mask = mask32;
}

void
_gdk_broadway_surface_grab_check_unmap (GdkSurface *surface,
                                        gulong     serial)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkSeat *seat;

  seat = gdk_display_get_default_seat (display);
  gdk_seat_break_grab (seat, surface);
}


void
_gdk_broadway_surface_grab_check_destroy (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkSeat *seat;

  seat = gdk_display_get_default_seat (display);
  gdk_seat_break_grab (seat, surface);
}


static GdkSurface *
gdk_broadway_device_surface_at_position (GdkDevice       *device,
                                         double          *win_x,
                                         double          *win_y,
                                         GdkModifierType *mask)
{
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  gint32 device_root_x, device_root_y;
  guint32 mouse_toplevel_id;
  guint32 mask32;

  if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return NULL;

  display = gdk_device_get_display (device);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  _gdk_broadway_server_query_mouse (broadway_display->server,
                                    &mouse_toplevel_id,
                                    &device_root_x,
                                    &device_root_y,
                                    &mask32);

  if (win_x)
    *win_x = device_root_x;
  if (win_y)
    *win_y = device_root_y;
  if (mask)
    *mask = mask32;

  return g_hash_table_lookup (broadway_display->id_ht,
                              GUINT_TO_POINTER (mouse_toplevel_id));
}
