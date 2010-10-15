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

#include "gdkdrawable.h"

#include "gdkcairo.h"
#include "gdkinternals.h"
#include "gdkwindow.h"
#include "gdkscreen.h"
#include "gdkpixbuf.h"

#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>

G_DEFINE_ABSTRACT_TYPE (GdkDrawable, gdk_drawable, G_TYPE_OBJECT)

static void
gdk_drawable_class_init (GdkDrawableClass *klass)
{
}

static void
gdk_drawable_init (GdkDrawable *drawable)
{
}

/**
 * gdk_drawable_get_clip_region:
 * @drawable: a #GdkDrawable
 * 
 * Computes the region of a drawable that potentially can be written
 * to by drawing primitives. This region will not take into account
 * the clip region for the GC, and may also not take into account
 * other factors such as if the window is obscured by other windows,
 * but no area outside of this region will be affected by drawing
 * primitives.
 * 
 * Returns: a #cairo_region_t. This must be freed with cairo_region_destroy()
 *          when you are done.
 **/
cairo_region_t *
gdk_drawable_get_clip_region (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_clip_region (drawable);
}

/**
 * gdk_drawable_get_visible_region:
 * @drawable: a #GdkDrawable
 * 
 * Computes the region of a drawable that is potentially visible.
 * This does not necessarily take into account if the window is
 * obscured by other windows, but no area outside of this region
 * is visible.
 * 
 * Returns: a #cairo_region_t. This must be freed with cairo_region_destroy()
 *          when you are done.
 **/
cairo_region_t *
gdk_drawable_get_visible_region (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_visible_region (drawable);
}

/**
 * _gdk_drawable_ref_cairo_surface:
 * @drawable: a #GdkDrawable
 * 
 * Obtains a #cairo_surface_t for the given drawable. If a
 * #cairo_surface_t for the drawable already exists, it will be
 * referenced, otherwise a new surface will be created.
 * 
 * Return value: a newly referenced #cairo_surface_t that points
 *  to @drawable. Unref with cairo_surface_destroy()
 **/
cairo_surface_t *
_gdk_drawable_ref_cairo_surface (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->ref_cairo_surface (drawable);
}

/************************************************************************/

cairo_surface_t *
_gdk_drawable_create_cairo_surface (GdkDrawable *drawable,
				    int width,
				    int height)
{
  return GDK_DRAWABLE_GET_CLASS (drawable)->create_cairo_surface (drawable,
								  width, height);
}
