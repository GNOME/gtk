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

#include <gdk/gdkwindow.h>

#include <windowsx.h>
#include <objbase.h>

#include "gdkwin32.h"
#include "gdkdevice-wintab.h"

static gboolean gdk_device_wintab_get_history (GdkDevice      *device,
                                               GdkWindow      *window,
                                               guint32         start,
                                               guint32         stop,
                                               GdkTimeCoord ***events,
                                               gint           *n_events);
static void gdk_device_wintab_get_state (GdkDevice       *device,
                                         GdkWindow       *window,
                                         gdouble         *axes,
                                         GdkModifierType *mask);
static void gdk_device_wintab_set_window_cursor (GdkDevice *device,
                                                 GdkWindow *window,
                                                 GdkCursor *cursor);
static void gdk_device_wintab_warp (GdkDevice *device,
                                    GdkScreen *screen,
                                    gdouble   x,
                                    gdouble   y);
static void gdk_device_wintab_query_state (GdkDevice        *device,
                                           GdkWindow        *window,
                                           GdkWindow       **root_window,
                                           GdkWindow       **child_window,
                                           gdouble          *root_x,
                                           gdouble          *root_y,
                                           gdouble          *win_x,
                                           gdouble          *win_y,
                                           GdkModifierType  *mask);
static GdkGrabStatus gdk_device_wintab_grab   (GdkDevice     *device,
                                               GdkWindow     *window,
                                               gboolean       owner_events,
                                               GdkEventMask   event_mask,
                                               GdkWindow     *confine_to,
                                               GdkCursor     *cursor,
                                               guint32        time_);
static void          gdk_device_wintab_ungrab (GdkDevice     *device,
                                               guint32        time_);
static GdkWindow * gdk_device_wintab_window_at_position (GdkDevice       *device,
                                                         gdouble         *win_x,
                                                         gdouble         *win_y,
                                                         GdkModifierType *mask,
                                                         gboolean         get_toplevel);
static void      gdk_device_wintab_select_window_events (GdkDevice       *device,
                                                         GdkWindow       *window,
                                                         GdkEventMask     event_mask);


G_DEFINE_TYPE (GdkDeviceWintab, gdk_device_wintab, GDK_TYPE_DEVICE)

static void
gdk_device_wintab_class_init (GdkDeviceWintabClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_device_wintab_get_history;
  device_class->get_state = gdk_device_wintab_get_state;
  device_class->set_window_cursor = gdk_device_wintab_set_window_cursor;
  device_class->warp = gdk_device_wintab_warp;
  device_class->query_state = gdk_device_wintab_query_state;
  device_class->grab = gdk_device_wintab_grab;
  device_class->ungrab = gdk_device_wintab_ungrab;
  device_class->window_at_position = gdk_device_wintab_window_at_position;
  device_class->select_window_events = gdk_device_wintab_select_window_events;
}

static void
gdk_device_wintab_init (GdkDeviceWintab *device_wintab)
{
}

static gboolean
gdk_device_wintab_get_history (GdkDevice      *device,
                               GdkWindow      *window,
                               guint32         start,
                               guint32         stop,
                               GdkTimeCoord ***events,
                               gint           *n_events)
{
  return FALSE;
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
gdk_device_wintab_get_state (GdkDevice       *device,
                             GdkWindow       *window,
                             gdouble         *axes,
                             GdkModifierType *mask)
{
  GdkDeviceWintab *device_wintab;

  device_wintab = GDK_DEVICE_WINTAB (device);

  /* For now just use the last known button and axis state of the device.
   * Since graphical tablets send an insane amount of motion events each
   * second, the info should be fairly up to date */
  if (mask)
    {
      *mask = get_current_mask ();
      *mask &= 0xFF; /* Mask away core pointer buttons */
      *mask |= ((device_wintab->button_state << 8)
                & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
                   | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
                   | GDK_BUTTON5_MASK));
    }

  if (axes && device_wintab->last_axis_data)
    _gdk_device_wintab_translate_axes (device_wintab, window, axes, NULL, NULL);
}

static void
gdk_device_wintab_set_window_cursor (GdkDevice *device,
                                     GdkWindow *window,
                                     GdkCursor *cursor)
{
}

static void
gdk_device_wintab_warp (GdkDevice *device,
                        GdkScreen *screen,
                        gdouble   x,
                        gdouble   y)
{
}

static void
gdk_device_wintab_query_state (GdkDevice        *device,
                               GdkWindow        *window,
                               GdkWindow       **root_window,
                               GdkWindow       **child_window,
                               gdouble          *root_x,
                               gdouble          *root_y,
                               gdouble          *win_x,
                               gdouble          *win_y,
                               GdkModifierType  *mask)
{
  GdkDeviceWintab *device_wintab;
  POINT point;
  HWND hwnd, hwndc;

  device_wintab = GDK_DEVICE_WINTAB (device);

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
    {
      *mask = get_current_mask ();
      *mask &= 0xFF; /* Mask away core pointer buttons */
      *mask |= ((device_wintab->button_state << 8)
                & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
                   | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
                   | GDK_BUTTON5_MASK));

    }
}

static GdkGrabStatus
gdk_device_wintab_grab (GdkDevice    *device,
                        GdkWindow    *window,
                        gboolean      owner_events,
                        GdkEventMask  event_mask,
                        GdkWindow    *confine_to,
                        GdkCursor    *cursor,
                        guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_wintab_ungrab (GdkDevice *device,
                          guint32    time_)
{
}

static GdkWindow *
gdk_device_wintab_window_at_position (GdkDevice       *device,
                                      gdouble         *win_x,
                                      gdouble         *win_y,
                                      GdkModifierType *mask,
                                      gboolean         get_toplevel)
{
  return NULL;
}

static void
gdk_device_wintab_select_window_events (GdkDevice    *device,
                                        GdkWindow    *window,
                                        GdkEventMask  event_mask)
{
}

void
_gdk_device_wintab_translate_axes (GdkDeviceWintab *device_wintab,
                                   GdkWindow       *window,
                                   gdouble         *axes,
                                   gdouble         *x,
                                   gdouble         *y)
{
  GdkDevice *device;
  GdkWindow *impl_window;
  gint root_x, root_y;
  gdouble temp_x, temp_y;
  gint i;

  device = GDK_DEVICE (device_wintab);
  impl_window = _gdk_window_get_impl_window (window);
  temp_x = temp_y = 0;

  gdk_window_get_origin (impl_window, &root_x, &root_y);

  for (i = 0; i < gdk_device_get_n_axes (device); i++)
    {
      GdkAxisUse use;

      use = gdk_device_get_axis_use (device, i);

      switch (use)
        {
        case GDK_AXIS_X:
        case GDK_AXIS_Y:
          if (gdk_device_get_mode (device) == GDK_MODE_WINDOW)
            _gdk_device_translate_window_coord (device, window, i,
                                                device_wintab->last_axis_data[i],
                                                &axes[i]);
          else
            _gdk_device_translate_screen_coord (device, window,
                                                root_x, root_y, i,
                                                device_wintab->last_axis_data[i],
                                                &axes[i]);
          if (use == GDK_AXIS_X)
            temp_x = axes[i];
          else if (use == GDK_AXIS_Y)
            temp_y = axes[i];

          break;
        default:
          _gdk_device_translate_axis (device, i,
                                      device_wintab->last_axis_data[i],
                                      &axes[i]);
          break;
        }
    }

  if (x)
    *x = temp_x;

  if (y)
    *y = temp_y;
}
