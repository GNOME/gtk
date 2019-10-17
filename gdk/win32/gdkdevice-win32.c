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

#include <gdk/gdksurface.h>

#include <windowsx.h>
#include <objbase.h>
#include <math.h>

#include "gdkdevice-win32.h"
#include "gdkwin32.h"
#include "gdkdisplay-win32.h"

G_DEFINE_TYPE (GdkDeviceWin32, gdk_device_win32, GDK_TYPE_DEVICE)

static gboolean
gdk_device_win32_get_history (GdkDevice      *device,
                              GdkSurface      *window,
                              guint32         start,
                              guint32         stop,
                              GdkTimeCoord ***events,
                              gint           *n_events)
{
  return FALSE;
}

static void
gdk_device_win32_get_state (GdkDevice       *device,
                            GdkSurface       *window,
                            gdouble         *axes,
                            GdkModifierType *mask)
{
  double x, y;

  gdk_surface_get_device_position (window, device, &x, &y, mask);

  if (axes)
    {
      axes[0] = round (x);
      axes[1] = round (y);
    }
}

static void
gdk_device_win32_set_surface_cursor (GdkDevice *device,
                                    GdkSurface *window,
                                    GdkCursor *cursor)
{
}

static GdkModifierType
get_current_mask (void)
{
  GdkModifierType mask;
  BYTE kbd[256];

  GetKeyboardState (kbd);
  mask = 0;
  if (kbd[VK_SHIFT] & 0x80)
    mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    mask |= GDK_MOD1_MASK;
  if (kbd[VK_LBUTTON] & 0x80)
    mask |= GDK_BUTTON1_MASK;
  if (kbd[VK_MBUTTON] & 0x80)
    mask |= GDK_BUTTON2_MASK;
  if (kbd[VK_RBUTTON] & 0x80)
    mask |= GDK_BUTTON3_MASK;

  return mask;
}

static void
gdk_device_win32_query_state (GdkDevice        *device,
                              GdkSurface        *window,
                              GdkSurface       **child_window,
                              gdouble          *root_x,
                              gdouble          *root_y,
                              gdouble          *win_x,
                              gdouble          *win_y,
                              GdkModifierType  *mask)
{
  POINT point;
  HWND hwnd, hwndc;
  gint scale;

  if (window)
    {
      scale = GDK_WIN32_SURFACE (window)->surface_scale;
      hwnd = GDK_SURFACE_HWND (window);
    }
  else
    {
      GdkDisplay *display = gdk_device_get_display (device);

      scale = GDK_WIN32_DISPLAY (display)->surface_scale;
      hwnd = NULL;
    }

  GetCursorPos (&point);

  if (root_x)
    *root_x = point.x / scale;

  if (root_y)
    *root_y = point.y / scale;

  if (hwnd)
    ScreenToClient (hwnd, &point);

  if (win_x)
    *win_x = point.x / scale;

  if (win_y)
    *win_y = point.y / scale;

  if (window)
    {
      if (win_x)
        *win_x += _gdk_offset_x;

      if (win_y)
        *win_y += _gdk_offset_y;

      if (root_x)
        *root_x += _gdk_offset_x;

      if (root_y)
        *root_y += _gdk_offset_y;
    }

  if (hwnd && child_window)
    {
      hwndc = ChildWindowFromPoint (hwnd, point);

      if (hwndc && hwndc != hwnd)
        *child_window = gdk_win32_handle_table_lookup (hwndc);
      else
        *child_window = NULL; /* Direct child unknown to gdk */
    }

  if (mask)
    *mask = get_current_mask ();
}

static GdkGrabStatus
gdk_device_win32_grab (GdkDevice    *device,
                       GdkSurface    *window,
                       gboolean      owner_events,
                       GdkEventMask  event_mask,
                       GdkSurface    *confine_to,
                       GdkCursor    *cursor,
                       guint32       time_)
{
  /* No support for grabbing the slave atm */
  return GDK_GRAB_NOT_VIEWABLE;
}

static void
gdk_device_win32_ungrab (GdkDevice *device,
                         guint32    time_)
{
}

static void
screen_to_client (HWND hwnd, POINT screen_pt, POINT *client_pt)
{
  *client_pt = screen_pt;
  ScreenToClient (hwnd, client_pt);
}

GdkSurface *
_gdk_device_win32_surface_at_position (GdkDevice       *device,
                                      gdouble         *win_x,
                                      gdouble         *win_y,
                                      GdkModifierType *mask,
                                      gboolean         get_toplevel)
{
  GdkSurface *window = NULL;
  GdkWin32Surface *impl = NULL;
  POINT screen_pt, client_pt;
  HWND hwnd, hwndc;
  RECT rect;

  GetCursorPos (&screen_pt);

  if (get_toplevel)
    {
      /* Only consider visible children of the desktop to avoid the various
       * non-visible windows you often find on a running Windows box. These
       * might overlap our windows and cause our walk to fail. As we assume
       * WindowFromPoint() can find our windows, we follow similar logic
       * here, and ignore invisible and disabled windows.
       */
      hwnd = GetDesktopWindow ();
      do {
        window = gdk_win32_handle_table_lookup (hwnd);

        if (window != NULL)
          break;

        screen_to_client (hwnd, screen_pt, &client_pt);
        hwndc = ChildWindowFromPointEx (hwnd, client_pt, CWP_SKIPDISABLED  |
                                                         CWP_SKIPINVISIBLE);

	/* Verify that we're really inside the client area of the window */
	if (hwndc != hwnd)
	  {
	    GetClientRect (hwndc, &rect);
	    screen_to_client (hwndc, screen_pt, &client_pt);
	    if (!PtInRect (&rect, client_pt))
	      hwndc = hwnd;
	  }

      } while (hwndc != hwnd && (hwnd = hwndc, 1));

    }
  else
    {
      hwnd = WindowFromPoint (screen_pt);

      /* Verify that we're really inside the client area of the window */
      GetClientRect (hwnd, &rect);
      screen_to_client (hwnd, screen_pt, &client_pt);
      if (!PtInRect (&rect, client_pt))
	hwnd = NULL;

      /* If we didn't hit any window at that point, return the desktop */
      if (hwnd == NULL)
        {
          if (win_x)
            *win_x = screen_pt.x + _gdk_offset_x;
          if (win_y)
            *win_y = screen_pt.y + _gdk_offset_y;

          return NULL;
        }

      window = gdk_win32_handle_table_lookup (hwnd);
    }

  if (window && (win_x || win_y))
    {
      impl = GDK_WIN32_SURFACE (window);

      if (win_x)
        *win_x = client_pt.x / impl->surface_scale;
      if (win_y)
        *win_y = client_pt.y / impl->surface_scale;
    }

  return window;
}

static void
gdk_device_win32_class_init (GdkDeviceWin32Class *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_device_win32_get_history;
  device_class->get_state = gdk_device_win32_get_state;
  device_class->set_surface_cursor = gdk_device_win32_set_surface_cursor;
  device_class->query_state = gdk_device_win32_query_state;
  device_class->grab = gdk_device_win32_grab;
  device_class->ungrab = gdk_device_win32_ungrab;
  device_class->surface_at_position = _gdk_device_win32_surface_at_position;
}

static void
gdk_device_win32_init (GdkDeviceWin32 *device_win32)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_win32);

  _gdk_device_add_axis (device, NULL, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, NULL, GDK_AXIS_Y, 0, 0, 1);
}
