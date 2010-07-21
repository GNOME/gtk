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
#include <math.h>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdkcairo.h"
#include "gdkdrawable.h"
#include "gdkinternals.h"
#include "gdkwindow.h"
#include "gdkscreen.h"
#include "gdkpixbuf.h"


static cairo_region_t *  gdk_drawable_real_get_visible_region     (GdkDrawable  *drawable);
     

G_DEFINE_ABSTRACT_TYPE (GdkDrawable, gdk_drawable, G_TYPE_OBJECT)

static void
gdk_drawable_class_init (GdkDrawableClass *klass)
{
  /* Default implementation for clip and visible region is the same */
  klass->get_clip_region = gdk_drawable_real_get_visible_region;
  klass->get_visible_region = gdk_drawable_real_get_visible_region;
}

static void
gdk_drawable_init (GdkDrawable *drawable)
{
}

/* Manipulation of drawables
 */

/**
 * gdk_drawable_get_size:
 * @drawable: a #GdkDrawable
 * @width: (out) (allow-none): location to store drawable's width, or %NULL
 * @height: (out) (allow-none): location to store drawable's height, or %NULL
 *
 * Fills *@width and *@height with the size of @drawable.
 * @width or @height can be %NULL if you only want the other one.
 *
 * On the X11 platform, if @drawable is a #GdkWindow, the returned
 * size is the size reported in the most-recently-processed configure
 * event, rather than the current size on the X server.
 * 
 **/
void
gdk_drawable_get_size (GdkDrawable *drawable,
		       gint        *width,
		       gint        *height)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  GDK_DRAWABLE_GET_CLASS (drawable)->get_size (drawable, width, height);  
}

/**
 * gdk_drawable_get_visual:
 * @drawable: a #GdkDrawable
 * 
 * Gets the #GdkVisual describing the pixel format of @drawable.
 * 
 * Return value: a #GdkVisual
 **/
GdkVisual*
gdk_drawable_get_visual (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  
  return GDK_DRAWABLE_GET_CLASS (drawable)->get_visual (drawable);
}

/**
 * gdk_drawable_get_depth:
 * @drawable: a #GdkDrawable
 * 
 * Obtains the bit depth of the drawable, that is, the number of bits
 * that make up a pixel in the drawable's visual. Examples are 8 bits
 * per pixel, 24 bits per pixel, etc.
 * 
 * Return value: number of bits per pixel
 **/
gint
gdk_drawable_get_depth (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), 0);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_depth (drawable);
}
/**
 * gdk_drawable_get_screen:
 * @drawable: a #GdkDrawable
 * 
 * Gets the #GdkScreen associated with a #GdkDrawable.
 * 
 * Return value: the #GdkScreen associated with @drawable
 *
 * Since: 2.2
 **/
GdkScreen*
gdk_drawable_get_screen(GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_screen (drawable);
}

/**
 * gdk_drawable_get_display:
 * @drawable: a #GdkDrawable
 * 
 * Gets the #GdkDisplay associated with a #GdkDrawable.
 * 
 * Return value: the #GdkDisplay associated with @drawable
 *
 * Since: 2.2
 **/
GdkDisplay*
gdk_drawable_get_display (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  
  return gdk_screen_get_display (gdk_drawable_get_screen (drawable));
}
	
/**
 * gdk_drawable_set_colormap:
 * @drawable: a #GdkDrawable
 * @colormap: a #GdkColormap
 *
 * Sets the colormap associated with @drawable. Normally this will
 * happen automatically when the drawable is created; you only need to
 * use this function if the drawable-creating function did not have a
 * way to determine the colormap, and you then use drawable operations
 * that require a colormap. The colormap for all drawables and
 * graphics contexts you intend to use together should match. i.e.
 * when using a #GdkGC to draw to a drawable, or copying one drawable
 * to another, the colormaps should match.
 * 
 **/
void
gdk_drawable_set_colormap (GdkDrawable *drawable,
                           GdkColormap *cmap)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (cmap == NULL || gdk_drawable_get_depth (drawable)
                    == cmap->visual->depth);

  GDK_DRAWABLE_GET_CLASS (drawable)->set_colormap (drawable, cmap);
}

/**
 * gdk_drawable_get_colormap:
 * @drawable: a #GdkDrawable
 * 
 * Gets the colormap for @drawable, if one is set; returns
 * %NULL otherwise.
 * 
 * Return value: the colormap, or %NULL
 **/
GdkColormap*
gdk_drawable_get_colormap (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_colormap (drawable);
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

static cairo_region_t *
gdk_drawable_real_get_visible_region (GdkDrawable *drawable)
{
  GdkRectangle rect;

  rect.x = 0;
  rect.y = 0;

  gdk_drawable_get_size (drawable, &rect.width, &rect.height);

  return cairo_region_create_rectangle (&rect);
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

/**
 * _gdk_drawable_get_subwindow_scratch_gc:
 * @drawable: A #GdkDrawable
 * 
 * Returns a #GdkGC suitable for drawing on @drawable. The #GdkGC has
 * the standard values for @drawable, except for the graphics_exposures
 * field which is %TRUE and the subwindow mode which is %GDK_INCLUDE_INFERIORS.
 *
 * The foreground color of the returned #GdkGC is undefined. The #GdkGC
 * must not be altered in any way, except to change its foreground color.
 * 
 * Return value: A #GdkGC suitable for drawing on @drawable
 * 
 * Since: 2.18
 **/
GdkGC *
_gdk_drawable_get_subwindow_scratch_gc (GdkDrawable *drawable)
{
  GdkScreen *screen;
  gint depth;

  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  screen = gdk_drawable_get_screen (drawable);

  g_return_val_if_fail (!screen->closed, NULL);

  depth = gdk_drawable_get_depth (drawable) - 1;

  if (!screen->subwindow_gcs[depth])
    {
      GdkGCValues values;
      GdkGCValuesMask mask;
      
      values.graphics_exposures = TRUE;
      values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      mask = GDK_GC_EXPOSURES | GDK_GC_SUBWINDOW;  
      
      screen->subwindow_gcs[depth] =
	gdk_gc_new_with_values (drawable, &values, mask);
    }
  
  return screen->subwindow_gcs[depth];
}


/*
 * _gdk_drawable_get_source_drawable:
 * @drawable: a #GdkDrawable
 *
 * Returns a drawable for the passed @drawable that is guaranteed to be
 * usable to create a pixmap (e.g.: not an offscreen window).
 *
 * Since: 2.16
 */
GdkDrawable *
_gdk_drawable_get_source_drawable (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  if (GDK_DRAWABLE_GET_CLASS (drawable)->get_source_drawable)
    return GDK_DRAWABLE_GET_CLASS (drawable)->get_source_drawable (drawable);

  return drawable;
}

cairo_surface_t *
_gdk_drawable_create_cairo_surface (GdkDrawable *drawable,
				    int width,
				    int height)
{
  return GDK_DRAWABLE_GET_CLASS (drawable)->create_cairo_surface (drawable,
								  width, height);
}
