/* GIMP Drawing Kit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkdrawable-x11.h"

#include "gdkx.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

#include <cairo-xlib.h>

#include <stdlib.h>
#include <string.h>


static cairo_surface_t *gdk_x11_ref_cairo_surface (GdkDrawable *drawable);
static cairo_surface_t *gdk_x11_create_cairo_surface (GdkDrawable *drawable,
                                                      int          width,
                                                      int          height);
     
static const cairo_user_data_key_t gdk_x11_cairo_key;

G_DEFINE_TYPE (GdkDrawableImplX11, _gdk_drawable_impl_x11, GDK_TYPE_DRAWABLE)

static void
_gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  drawable_class->ref_cairo_surface = gdk_x11_ref_cairo_surface;
  drawable_class->create_cairo_surface = gdk_x11_create_cairo_surface;
}

static void
_gdk_drawable_impl_x11_init (GdkDrawableImplX11 *impl)
{
}

/**
 * _gdk_x11_drawable_finish:
 * @drawable: a #GdkDrawableImplX11.
 * 
 * Performs necessary cleanup prior to destroying a window.
 **/
void
_gdk_x11_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				   NULL, NULL);
    }
}

/**
 * _gdk_x11_drawable_update_size:
 * @drawable: a #GdkDrawableImplX11.
 * 
 * Updates the state of the drawable (in particular the drawable's
 * cairo surface) when its size has changed.
 **/
void
_gdk_x11_drawable_update_size (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (impl->cairo_surface)
    {
      cairo_xlib_surface_set_size (impl->cairo_surface,
                                   gdk_window_get_width (impl->wrapper),
                                   gdk_window_get_height (impl->wrapper));
    }
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static GdkDrawable *
get_impl_drawable (GdkDrawable *drawable)
{
  if (GDK_IS_WINDOW (drawable))
    return ((GdkWindowObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a window");
      return NULL;
    }
}

/**
 * gdk_x11_drawable_get_xdisplay:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the display of a #GdkDrawable.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_drawable_get_xdisplay (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (drawable)->screen);
  else
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen);
}

/**
 * gdk_x11_drawable_get_xid:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the X resource (window) belonging to a #GdkDrawable.
 * 
 * Return value: the ID of @drawable's X resource.
 **/
XID
gdk_x11_drawable_get_xid (GdkDrawable *drawable)
{
  GdkDrawable *impl;
  
  if (GDK_IS_WINDOW (drawable))
    {
      GdkWindow *window = (GdkWindow *)drawable;
      
      /* Try to ensure the window has a native window */
      if (!_gdk_window_has_impl (window))
	{
	  gdk_window_ensure_native (window);

	  /* We sync here to ensure the window is created in the Xserver when
	   * this function returns. This is required because the returned XID
	   * for this window must be valid immediately, even with another
	   * connection to the Xserver */
	  gdk_display_sync (gdk_window_get_display (window));
	}
      
      if (!GDK_WINDOW_IS_X11 (window))
        {
          g_warning (G_STRLOC " drawable is not a native X11 window");
          return None;
        }
      
      impl = ((GdkWindowObject *)drawable)->impl;
    }
  else
    {
      g_warning (G_STRLOC " drawable is not a window");
      return None;
    }

  return ((GdkDrawableImplX11 *)impl)->xid;
}

GdkDrawable *
gdk_x11_window_get_drawable_impl (GdkWindow *window)
{
  return ((GdkWindowObject *)window)->impl;
}

static void
gdk_x11_cairo_surface_destroy (void *data)
{
  GdkDrawableImplX11 *impl = data;

  impl->cairo_surface = NULL;
}

static cairo_surface_t *
gdk_x11_create_cairo_surface (GdkDrawable *drawable,
			      int width,
			      int height)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  GdkVisual *visual;
    
  visual = gdk_window_get_visual (impl->wrapper);
  return cairo_xlib_surface_create (GDK_SCREEN_XDISPLAY (impl->screen),
                                    impl->xid,
                                    GDK_VISUAL_XVISUAL (visual),
                                    width, height);
}

static cairo_surface_t *
gdk_x11_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (GDK_IS_WINDOW_IMPL_X11 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      impl->cairo_surface = gdk_x11_create_cairo_surface (drawable,
                                                          gdk_window_get_width (impl->wrapper),
                                                          gdk_window_get_height (impl->wrapper));
      
      if (impl->cairo_surface)
	cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				     drawable, gdk_x11_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}
