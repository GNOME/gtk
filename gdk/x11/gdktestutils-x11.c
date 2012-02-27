/* Gtk+ testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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

#include "gdktestutils.h"

#include "gdkkeysyms.h"
#include "gdkprivate-x11.h"

#include <X11/Xlib.h>

void
_gdk_x11_window_sync_rendering (GdkWindow *window)
{
  Display *display = GDK_WINDOW_XDISPLAY (window);
  XImage *ximage;

  /* syncronize to X drawing queue, see:
   * http://mail.gnome.org/archives/gtk-devel-list/2006-October/msg00103.html
   */
  ximage = XGetImage (display, DefaultRootWindow (display),
	     	      0, 0, 1, 1, AllPlanes, ZPixmap);
  if (ximage != NULL)
    XDestroyImage (ximage);
}

gboolean
_gdk_x11_window_simulate_key (GdkWindow      *window,
                              gint            x,
                              gint            y,
                              guint           keyval,
                              GdkModifierType modifiers,
                              GdkEventType    key_pressrelease)
{
  GdkScreen *screen;
  GdkKeymapKey *keys = NULL;
  gboolean success;
  gint n_keys = 0;
  XKeyEvent xev = {
    0,  /* type */
    0,  /* serial */
    1,  /* send_event */
  };
  g_return_val_if_fail (key_pressrelease == GDK_KEY_PRESS || key_pressrelease == GDK_KEY_RELEASE, FALSE);
  g_return_val_if_fail (window != NULL, FALSE);
  if (!GDK_WINDOW_IS_MAPPED (window))
    return FALSE;

  screen = gdk_window_get_screen (window);

  if (x < 0 && y < 0)
    {
      x = window->width / 2;
      y = window->height / 2;
    }

  /* Convert to impl coordinates */
  x = x + window->abs_x;
  y = y + window->abs_y;

  xev.type = key_pressrelease == GDK_KEY_PRESS ? KeyPress : KeyRelease;
  xev.display = GDK_WINDOW_XDISPLAY (window);
  xev.window = GDK_WINDOW_XID (window);
  xev.root = RootWindow (xev.display, GDK_X11_SCREEN (screen)->screen_num);
  xev.subwindow = 0;
  xev.time = 0;
  xev.x = MAX (x, 0);
  xev.y = MAX (y, 0);
  xev.x_root = 0;
  xev.y_root = 0;
  xev.state = modifiers;
  xev.keycode = 0;
  success = gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (gdk_window_get_display (window)), keyval, &keys, &n_keys);
  success &= n_keys > 0;
  if (success)
    {
      gint i;
      for (i = 0; i < n_keys; i++)
        if (keys[i].group == 0 && (keys[i].level == 0 || keys[i].level == 1))
          {
            xev.keycode = keys[i].keycode;
            if (keys[i].level == 1)
              {
                /* Assume shift takes us to level 1 */
                xev.state |= GDK_SHIFT_MASK;
              }
            break;
          }
      if (i >= n_keys) /* no match for group==0 and level==0 or 1 */
        xev.keycode = keys[0].keycode;
    }
  g_free (keys);
  if (!success)
    return FALSE;
  gdk_x11_display_error_trap_push (GDK_WINDOW_DISPLAY (window));
  xev.same_screen = XTranslateCoordinates (xev.display, xev.window, xev.root,
                                           xev.x, xev.y, &xev.x_root, &xev.y_root,
                                           &xev.subwindow);
  if (!xev.subwindow)
    xev.subwindow = xev.window;
  success &= xev.same_screen;
  if (x >= 0 && y >= 0)
    success &= 0 != XWarpPointer (xev.display, None, xev.window, 0, 0, 0, 0, xev.x, xev.y);
  success &= 0 != XSendEvent (xev.display, xev.window, True, key_pressrelease == GDK_KEY_PRESS ? KeyPressMask : KeyReleaseMask, (XEvent*) &xev);
  XSync (xev.display, False);
  success &= 0 == gdk_x11_display_error_trap_pop (GDK_WINDOW_DISPLAY (window));
  return success;
}

gboolean
_gdk_x11_window_simulate_button (GdkWindow      *window,
                                 gint            x,
                                 gint            y,
                                 guint           button, /*1..3*/
                                 GdkModifierType modifiers,
                                 GdkEventType    button_pressrelease)
{
  GdkScreen *screen;
  XButtonEvent xev = {
    0,  /* type */
    0,  /* serial */
    1,  /* send_event */
  };
  gboolean success;

  g_return_val_if_fail (button_pressrelease == GDK_BUTTON_PRESS || button_pressrelease == GDK_BUTTON_RELEASE, FALSE);
  g_return_val_if_fail (window != NULL, FALSE);

  if (!GDK_WINDOW_IS_MAPPED (window))
    return FALSE;

  screen = gdk_window_get_screen (window);

  if (x < 0 && y < 0)
    {
      x = window->width / 2;
      y = window->height / 2;
    }

  /* Convert to impl coordinates */
  x = x + window->abs_x;
  y = y + window->abs_y;

  xev.type = button_pressrelease == GDK_BUTTON_PRESS ? ButtonPress : ButtonRelease;
  xev.display = GDK_WINDOW_XDISPLAY (window);
  xev.window = GDK_WINDOW_XID (window);
  xev.root = RootWindow (xev.display, GDK_X11_SCREEN (screen)->screen_num);
  xev.subwindow = 0;
  xev.time = 0;
  xev.x = x;
  xev.y = y;
  xev.x_root = 0;
  xev.y_root = 0;
  xev.state = modifiers;
  xev.button = button;
  gdk_x11_display_error_trap_push (GDK_WINDOW_DISPLAY (window));
  xev.same_screen = XTranslateCoordinates (xev.display, xev.window, xev.root,
                                           xev.x, xev.y, &xev.x_root, &xev.y_root,
                                           &xev.subwindow);
  if (!xev.subwindow)
    xev.subwindow = xev.window;
  success = xev.same_screen;
  success &= 0 != XWarpPointer (xev.display, None, xev.window, 0, 0, 0, 0, xev.x, xev.y);
  success &= 0 != XSendEvent (xev.display, xev.window, True, button_pressrelease == GDK_BUTTON_PRESS ? ButtonPressMask : ButtonReleaseMask, (XEvent*) &xev);
  XSync (xev.display, False);
  success &= 0 == gdk_x11_display_error_trap_pop(GDK_WINDOW_DISPLAY (window));
  return success;
}
