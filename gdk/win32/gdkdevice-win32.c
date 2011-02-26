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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gdk/gdkwindow.h>

#include <windowsx.h>
#include <objbase.h>

#include "gdkdisplayprivate.h"
#include "gdkdevice-win32.h"
#include "gdkwin32.h"

static gboolean gdk_device_win32_get_history (GdkDevice      *device,
                                              GdkWindow      *window,
                                              guint32         start,
                                              guint32         stop,
                                              GdkTimeCoord ***events,
                                              gint           *n_events);
static void gdk_device_win32_get_state (GdkDevice       *device,
                                        GdkWindow       *window,
                                        gdouble         *axes,
                                        GdkModifierType *mask);
static void gdk_device_win32_set_window_cursor (GdkDevice *device,
                                                GdkWindow *window,
                                                GdkCursor *cursor);
static void gdk_device_win32_warp (GdkDevice *device,
                                   GdkScreen *screen,
                                   gint       x,
                                   gint       y);
static gboolean gdk_device_win32_query_state (GdkDevice        *device,
                                              GdkWindow        *window,
                                              GdkWindow       **root_window,
                                              GdkWindow       **child_window,
                                              gint             *root_x,
                                              gint             *root_y,
                                              gint             *win_x,
                                              gint             *win_y,
                                              GdkModifierType  *mask);
static GdkGrabStatus gdk_device_win32_grab   (GdkDevice     *device,
                                              GdkWindow     *window,
                                              gboolean       owner_events,
                                              GdkEventMask   event_mask,
                                              GdkWindow     *confine_to,
                                              GdkCursor     *cursor,
                                              guint32        time_);
static void          gdk_device_win32_ungrab (GdkDevice     *device,
                                              guint32        time_);
static GdkWindow * gdk_device_win32_window_at_position (GdkDevice       *device,
                                                        gint            *win_x,
                                                        gint            *win_y,
                                                        GdkModifierType *mask,
                                                        gboolean         get_toplevel);
static void      gdk_device_win32_select_window_events (GdkDevice       *device,
                                                        GdkWindow       *window,
                                                        GdkEventMask     event_mask);


G_DEFINE_TYPE (GdkDeviceWin32, gdk_device_win32, GDK_TYPE_DEVICE)

static void
gdk_device_win32_class_init (GdkDeviceWin32Class *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_device_win32_get_history;
  device_class->get_state = gdk_device_win32_get_state;
  device_class->set_window_cursor = gdk_device_win32_set_window_cursor;
  device_class->warp = gdk_device_win32_warp;
  device_class->query_state = gdk_device_win32_query_state;
  device_class->grab = gdk_device_win32_grab;
  device_class->ungrab = gdk_device_win32_ungrab;
  device_class->window_at_position = gdk_device_win32_window_at_position;
  device_class->select_window_events = gdk_device_win32_select_window_events;
}

static void
gdk_device_win32_init (GdkDeviceWin32 *device_win32)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_win32);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

static gboolean
gdk_device_win32_get_history (GdkDevice      *device,
                              GdkWindow      *window,
                              guint32         start,
                              guint32         stop,
                              GdkTimeCoord ***events,
                              gint           *n_events)
{
  return FALSE;
}

static void
gdk_device_win32_get_state (GdkDevice       *device,
                            GdkWindow       *window,
                            gdouble         *axes,
                            GdkModifierType *mask)
{
  gint x_int, y_int;

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

static void
gdk_device_win32_set_window_cursor (GdkDevice *device,
                                    GdkWindow *window,
                                    GdkCursor *cursor)
{
  GdkWin32Cursor *cursor_private;
  GdkWindow *parent_window;
  GdkWindowImplWin32 *impl;
  HCURSOR hcursor;
  HCURSOR hprevcursor;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  cursor_private = (GdkWin32Cursor*) cursor;

  hprevcursor = impl->hcursor;

  if (!cursor)
    hcursor = NULL;
  else
    hcursor = cursor_private->hcursor;

  if (hcursor != NULL)
    {
      /* If the pointer is over our window, set new cursor */
      GdkWindow *curr_window = gdk_window_get_pointer (window, NULL, NULL, NULL);

      if (curr_window == window ||
          (curr_window && window == gdk_window_get_toplevel (curr_window)))
        SetCursor (hcursor);
      else
        {
          /* Climb up the tree and find whether our window is the
           * first ancestor that has cursor defined, and if so, set
           * new cursor.
           */
          while (curr_window && curr_window->impl &&
                 !GDK_WINDOW_IMPL_WIN32 (curr_window->impl)->hcursor)
            {
              curr_window = curr_window->parent;
              if (curr_window == GDK_WINDOW (window))
                {
                  SetCursor (hcursor);
                  break;
                }
            }
        }
    }

  /* Unset the previous cursor: Need to make sure it's no longer in
   * use before we destroy it, in case we're not over our window but
   * the cursor is still set to our old one.
   */
  if (hprevcursor != NULL &&
      GetCursor () == hprevcursor)
    {
      /* Look for a suitable cursor to use instead */
      hcursor = NULL;
      parent_window = GDK_WINDOW (window)->parent;

      while (hcursor == NULL)
        {
          if (parent_window)
            {
              impl = GDK_WINDOW_IMPL_WIN32 (parent_window->impl);
              hcursor = impl->hcursor;
              parent_window = parent_window->parent;
            }
          else
            hcursor = LoadCursor (NULL, IDC_ARROW);
        }

      SetCursor (hcursor);
    }
}

static void
gdk_device_win32_warp (GdkDevice *device,
                       GdkScreen *screen,
                       gint       x,
                       gint       y)
{
  SetCursorPos (x - _gdk_offset_x, y - _gdk_offset_y);
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

static gboolean
gdk_device_win32_query_state (GdkDevice        *device,
                              GdkWindow        *window,
                              GdkWindow       **root_window,
                              GdkWindow       **child_window,
                              gint             *root_x,
                              gint             *root_y,
                              gint             *win_x,
                              gint             *win_y,
                              GdkModifierType  *mask)
{
  gboolean return_val;
  POINT point;
  HWND hwnd, hwndc;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);
  
  return_val = TRUE;

  hwnd = GDK_WINDOW_HWND (window);
  GetCursorPos (&point);

  if (root_x)
    *root_x = point.x;

  if (root_y)
    *root_y = point.y;

  ScreenToClient (hwnd, &point);

  if (win_x)
    *win_x = point.x;

  if (win_y)
    *win_y = point.y;

  if (window == _gdk_root)
    {
      if (win_x)
        *win_x += _gdk_offset_x;

      if (win_y)
        *win_y += _gdk_offset_y;
    }

  if (child_window)
    {
      hwndc = ChildWindowFromPoint (hwnd, point);

      if (hwndc && hwndc != hwnd)
        *child_window = gdk_win32_handle_table_lookup (hwndc);
      else
        *child_window = NULL; /* Direct child unknown to gdk */
    }

  if (root_window)
    {
      GdkScreen *screen;

      screen = gdk_window_get_screen (window);
      *root_window = gdk_screen_get_root_window (screen);
    }

  if (mask)
    *mask = get_current_mask ();

  return TRUE;
}

static GdkGrabStatus
gdk_device_win32_grab (GdkDevice    *device,
                       GdkWindow    *window,
                       gboolean      owner_events,
                       GdkEventMask  event_mask,
                       GdkWindow    *confine_to,
                       GdkCursor    *cursor,
                       guint32       time_)
{
  if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    SetCapture (GDK_WINDOW_HWND (window));

  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_win32_ungrab (GdkDevice *device,
                         guint32    time_)
{
  GdkDeviceGrabInfo *info;
  GdkDisplay *display;

  display = gdk_device_get_display (device);
  info = _gdk_display_get_last_device_grab (display, device);

  if (info)
    info->serial_end = 0;

  if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    ReleaseCapture ();

  _gdk_display_device_grab_update (display, device, NULL, 0);
}

static GdkWindow *
gdk_device_win32_window_at_position (GdkDevice       *device,
                                     gint            *win_x,
                                     gint            *win_y,
                                     GdkModifierType *mask,
                                     gboolean         get_toplevel)
{
  GdkWindow *window;
  POINT point, pointc;
  HWND hwnd, hwndc;
  RECT rect;

  GetCursorPos (&pointc);
  point = pointc;
  hwnd = WindowFromPoint (point);

  if (hwnd == NULL)
    {
      window = _gdk_root;
      *win_x = pointc.x + _gdk_offset_x;
      *win_y = pointc.y + _gdk_offset_y;
      return window;
    }

  ScreenToClient (hwnd, &point);

  do
    {
      if (get_toplevel &&
          (window = gdk_win32_handle_table_lookup (hwnd)) != NULL &&
          GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
        break;

      hwndc = ChildWindowFromPoint (hwnd, point);
      ClientToScreen (hwnd, &point);
      ScreenToClient (hwndc, &point);
    }
  while (hwndc != hwnd && (hwnd = hwndc, 1));

  window = gdk_win32_handle_table_lookup (hwnd);

  if (window && (win_x || win_y))
    {
      GetClientRect (hwnd, &rect);
      *win_x = point.x - rect.left;
      *win_y = point.y - rect.top;
    }

  return window;
}

static void
gdk_device_win32_select_window_events (GdkDevice    *device,
                                       GdkWindow    *window,
                                       GdkEventMask  event_mask)
{
}
