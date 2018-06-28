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
 * back to the real position determined by gdk_surface_compute_position().
 * This is handled in gdk_surface_postmove().
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

typedef struct _GdkSurfaceParentPos GdkSurfaceParentPos;

static void
tmp_unset_bg (GdkSurface *window)
{
  GdkSurfaceImplWin32 *impl;

  impl = GDK_SURFACE_IMPL_WIN32 (window->impl);

  impl->no_bg = TRUE;
}

static void
tmp_reset_bg (GdkSurface *window)
{
  GdkSurfaceImplWin32 *impl;

  impl = GDK_SURFACE_IMPL_WIN32 (window->impl);

  impl->no_bg = FALSE;
}

void
_gdk_surface_move_resize_child (GdkSurface *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkSurfaceImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_WIN32 (window->impl);
  GDK_NOTE (MISC, g_print ("_gdk_surface_move_resize_child: %s@%+d%+d %dx%d@%+d%+d\n",
			   _gdk_win32_surface_description (window),
			   window->x, window->y, width, height, x, y));

  if (width * impl->surface_scale > 65535 || height * impl->surface_scale > 65535)
    {
      g_warning ("Native children wider or taller than 65535 pixels are not supported.");

      if (width * impl->surface_scale > 65535)
        width = 65535 / impl->surface_scale;
      if (height * impl->surface_scale > 65535)
        height = 65535 /impl->surface_scale;
    }

  window->x = x;
  window->y = y;
  window->width = width;
  window->height = height;
  impl->unscaled_width = width * impl->surface_scale;
  impl->unscaled_height = height * impl->surface_scale;

  _gdk_win32_surface_tmp_unset_parent_bg (window);
  _gdk_win32_surface_tmp_unset_bg (window, TRUE);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%d,%d,"
			   "NOACTIVATE|NOZORDER)\n",
			   GDK_SURFACE_HWND (window),
			   (window->x + window->parent->abs_x) * impl->surface_scale,
			   (window->y + window->parent->abs_y) * impl->surface_scale,
			   impl->unscaled_width,
			   impl->unscaled_height));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), NULL,
			   (window->x + window->parent->abs_x) * impl->surface_scale,
			   (window->y + window->parent->abs_y) * impl->surface_scale,
			   impl->unscaled_width,
			   impl->unscaled_height,
			   SWP_NOACTIVATE | SWP_NOZORDER));

  _gdk_win32_surface_tmp_reset_bg (window, TRUE);
}

void
_gdk_win32_surface_tmp_unset_bg (GdkSurface *window,
				gboolean recurse)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (window->input_only || window->destroyed || !GDK_SURFACE_IS_MAPPED (window))
    return;

  if (_gdk_surface_has_impl (window) &&
      GDK_SURFACE_IS_WIN32 (window))
    tmp_unset_bg (window);

  if (recurse)
    {
      GList *l;

      for (l = window->children; l != NULL; l = l->next)
	_gdk_win32_surface_tmp_unset_bg (l->data, TRUE);
    }
}

void
_gdk_win32_surface_tmp_unset_parent_bg (GdkSurface *window)
{
  if (window->parent == NULL)
    return;

  window = _gdk_surface_get_impl_surface (window->parent);
  _gdk_win32_surface_tmp_unset_bg (window, FALSE);
}

void
_gdk_win32_surface_tmp_reset_bg (GdkSurface *window,
				gboolean   recurse)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (window->input_only || window->destroyed || !GDK_SURFACE_IS_MAPPED (window))
    return;

  if (_gdk_surface_has_impl (window) &&
      GDK_SURFACE_IS_WIN32 (window))
    {
      tmp_reset_bg (window);
    }

  if (recurse)
    {
      GList *l;

      for (l = window->children; l != NULL; l = l->next)
	_gdk_win32_surface_tmp_reset_bg (l->data, TRUE);
    }
}
