/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/* gdkgeometry-win32.c: emulation of 32 bit coordinates within the
 * limits of Win32 GDI. The idea of big window emulation is more or less
 * a copy of the X11 version, and the equvalent of guffaw scrolling
 * is ScrollWindowEx(). While we determine the invalidated region
 * ourself during scrolling, we do not pass SW_INVALIDATE to
 * ScrollWindowEx() to avoid a unnecessary WM_PAINT.
 *
 * Bits are always scrolled correctly by ScrollWindowEx(), but
 * some big children may hit the coordinate boundary (i.e.
 * win32_x/win32_y < -16383) after scrolling. They needed to be moved
 * back to the real position determined by gdk_window_compute_position().
 * This is handled in gdk_window_postmove().
 *
 * The X11 version by Owen Taylor <otaylor@redhat.com>
 * Copyright Red Hat, Inc. 2000
 * Win32 hack by Tor Lillqvist <tml@iki.fi>
 * and Hans Breuer <hans@breuer.org>
 * Modified by Ivan, Wong Yat Cheung <email@ivanwong.info>
 * so that big window emulation finally works.
 */

#include "config.h"
#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkwin32.h"

#define SIZE_LIMIT 32767

typedef struct _GdkWindowParentPos GdkWindowParentPos;

static void tmp_unset_bg (GdkWindow *window);
static void tmp_reset_bg (GdkWindow *window);

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (MISC, g_print ("_gdk_window_move_resize_child: %s@%+d%+d %dx%d@%+d%+d\n",
			   _gdk_win32_window_description (window),
			   window->x, window->y, width, height, x, y));

  if (width > 65535 || height > 65535)
  {
    g_warning ("Native children wider or taller than 65535 pixels are not supported.");

    if (width > 65535)
      width = 65535;
    if (height > 65535)
      height = 65535;
  }

  window->x = x;
  window->y = y;
  window->width = width;
  window->height = height;

  _gdk_win32_window_tmp_unset_parent_bg (window);
  _gdk_win32_window_tmp_unset_bg (window, TRUE);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%d,%d,"
			   "NOACTIVATE|NOZORDER)\n",
			   GDK_WINDOW_HWND (window),
			   window->x + window->parent->abs_x, window->y + window->parent->abs_y,
			   width, height));

  API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			   window->x + window->parent->abs_x, window->y + window->parent->abs_y,
			   width, height,
			   SWP_NOACTIVATE | SWP_NOZORDER));

  _gdk_win32_window_tmp_reset_bg (window, TRUE);
}

void
_gdk_win32_window_tmp_unset_bg (GdkWindow *window,
				gboolean recurse)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->input_only || window->destroyed ||
      (window->window_type != GDK_WINDOW_ROOT &&
       !GDK_WINDOW_IS_MAPPED (window)))
    return;

  if (_gdk_window_has_impl (window) &&
      GDK_WINDOW_IS_WIN32 (window) &&
      window->window_type != GDK_WINDOW_ROOT &&
      window->window_type != GDK_WINDOW_FOREIGN)
    tmp_unset_bg (window);

  if (recurse)
    {
      GList *l;

      for (l = window->children; l != NULL; l = l->next)
	_gdk_win32_window_tmp_unset_bg (l->data, TRUE);
    }
}

static void
tmp_unset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  impl->no_bg = TRUE;
}

static void
tmp_reset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  impl->no_bg = FALSE;
}

void
_gdk_win32_window_tmp_unset_parent_bg (GdkWindow *window)
{
  if (GDK_WINDOW_TYPE (window->parent) == GDK_WINDOW_ROOT)
    return;

  window = _gdk_window_get_impl_window (window->parent);
  _gdk_win32_window_tmp_unset_bg (window, FALSE);
}

void
_gdk_win32_window_tmp_reset_bg (GdkWindow *window,
				gboolean   recurse)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->input_only || window->destroyed ||
      (window->window_type != GDK_WINDOW_ROOT && !GDK_WINDOW_IS_MAPPED (window)))
    return;

  if (_gdk_window_has_impl (window) &&
      GDK_WINDOW_IS_WIN32 (window) &&
      window->window_type != GDK_WINDOW_ROOT &&
      window->window_type != GDK_WINDOW_FOREIGN)
    {
      tmp_reset_bg (window);
    }

  if (recurse)
    {
      GList *l;

      for (l = window->children; l != NULL; l = l->next)
	_gdk_win32_window_tmp_reset_bg (l->data, TRUE);
    }
}
