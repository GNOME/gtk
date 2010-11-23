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


static cairo_surface_t *gdk_broadway_ref_cairo_surface (GdkDrawable *drawable);

static const cairo_user_data_key_t gdk_broadway_cairo_key;

G_DEFINE_TYPE (GdkDrawableImplBroadway, _gdk_drawable_impl_broadway, GDK_TYPE_DRAWABLE)

static void
_gdk_drawable_impl_broadway_class_init (GdkDrawableImplBroadwayClass *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  drawable_class->ref_cairo_surface = gdk_broadway_ref_cairo_surface;
  drawable_class->create_cairo_surface = NULL;
}

static void
_gdk_drawable_impl_broadway_init (GdkDrawableImplBroadway *impl)
{
}

/**
 * _gdk_broadway_drawable_finish:
 * @drawable: a #GdkDrawableImplBroadway.
 * 
 * Performs necessary cleanup prior to destroying a window.
 **/
void
_gdk_broadway_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplBroadway *impl = GDK_DRAWABLE_IMPL_BROADWAY (drawable);

  if (impl->ref_surface)
    {
      cairo_surface_finish (impl->ref_surface);
      cairo_surface_set_user_data (impl->ref_surface, &gdk_broadway_cairo_key,
				   NULL, NULL);
    }

  if (impl->surface)
    {
      cairo_surface_destroy (impl->surface);
      impl->surface = NULL;
      cairo_surface_destroy (impl->last_surface);
      impl->last_surface = NULL;
    }
}

/**
 * _gdk_broadway_drawable_update_size:
 * @drawable: a #GdkDrawableImplBroadway.
 *
 * Updates the state of the drawable (in particular the drawable's
 * cairo surface) when its size has changed.
 **/
void
_gdk_broadway_drawable_update_size (GdkDrawable *drawable)
{
  GdkDrawableImplBroadway *impl = GDK_DRAWABLE_IMPL_BROADWAY (drawable);
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

      cairo_surface_destroy (old);
      cairo_surface_destroy (last_old);
    }

  if (impl->ref_surface)
    {
      cairo_surface_set_user_data (impl->ref_surface, &gdk_broadway_cairo_key,
				   NULL, NULL);
      impl->ref_surface = NULL;
    }
}

/*****************************************************
 * Broadway specific implementations of generic functions *
 *****************************************************/

static void
gdk_broadway_cairo_surface_destroy (void *data)
{
  GdkDrawableImplBroadway *impl = data;

  impl->ref_surface = NULL;
}

static cairo_surface_t *
gdk_broadway_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplBroadway *impl = GDK_DRAWABLE_IMPL_BROADWAY (drawable);
  cairo_t *cr;
  int w, h;

  if (GDK_IS_WINDOW_IMPL_BROADWAY (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  w = gdk_window_get_width (impl->wrapper);
  h = gdk_window_get_height (impl->wrapper);

  /* Create actual backing store if missing */
  if (!impl->surface)
    {
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

  /* Create a destroyable surface referencing the real one */
  if (!impl->ref_surface)
    {
      impl->ref_surface =
	cairo_surface_create_for_rectangle (impl->surface,
					    0, 0,
					    w, h);
      if (impl->ref_surface)
	cairo_surface_set_user_data (impl->ref_surface, &gdk_broadway_cairo_key,
				     drawable, gdk_broadway_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->ref_surface);

  return impl->ref_surface;
}
