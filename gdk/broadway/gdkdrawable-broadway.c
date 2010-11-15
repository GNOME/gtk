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

#include "gdkdrawable-broadway.h"

#include "gdkprivate-broadway.h"
#include "gdkscreen-broadway.h"
#include "gdkdisplay-broadway.h"

#include <stdlib.h>
#include <string.h>


static cairo_surface_t *gdk_x11_ref_cairo_surface (GdkDrawable *drawable);

static const cairo_user_data_key_t gdk_x11_cairo_key;

G_DEFINE_TYPE (GdkDrawableImplX11, _gdk_drawable_impl_x11, GDK_TYPE_DRAWABLE)

static void
_gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  drawable_class->ref_cairo_surface = gdk_x11_ref_cairo_surface;
  drawable_class->create_cairo_surface = NULL;
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

  if (impl->surface)
    {
      cairo_surface_finish (impl->surface);
      impl->surface = NULL;
      cairo_surface_finish (impl->last_surface);
      impl->last_surface = NULL;
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
  cairo_surface_t *old, *last_old;

  if (impl->surface)
    {
      old = impl->surface;
      last_old = impl->last_surface;

      impl->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
						  gdk_window_get_width (impl->wrapper),
						  gdk_window_get_height (impl->wrapper));
      impl->last_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
						       gdk_window_get_width (impl->wrapper),
						       gdk_window_get_height (impl->wrapper));

      /* TODO: copy old contents */

      cairo_surface_finish (old);
      cairo_surface_finish (last_old);
    }
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static cairo_surface_t *
gdk_x11_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  cairo_t *cr;
  int w, h;

  if (GDK_IS_WINDOW_IMPL_X11 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->surface)
    {
      w = gdk_window_get_width (impl->wrapper);
      h = gdk_window_get_height (impl->wrapper);
      impl->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
      impl->last_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);

      cr = cairo_create (impl->surface);
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_rectangle (cr, 0, 0, w, h);
      cairo_fill (cr);
      cairo_destroy (cr);

      cr = cairo_create (impl->last_surface);
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_rectangle (cr, 0, 0, w, h);
      cairo_fill (cr);
      cairo_destroy (cr);
    }

  return cairo_surface_reference (impl->surface);
}
