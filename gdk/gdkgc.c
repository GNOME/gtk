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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <config.h>
#include <string.h>

#include "gdkcairo.h"
#include "gdkgc.h"
#include "gdkinternals.h"
#include "gdkpixmap.h"
#include "gdkrgb.h"
#include "gdkprivate.h"
#include "gdkalias.h"

static void gdk_gc_finalize   (GObject      *object);

typedef struct _GdkGCPrivate GdkGCPrivate;

struct _GdkGCPrivate
{
  GdkRegion *clip_region;

  GdkFill fill;
  GdkBitmap *stipple;
  GdkPixmap *tile;
  
  guint32 fg_pixel;
  guint32 bg_pixel;
};

#define GDK_GC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDK_TYPE_GC, GdkGCPrivate))

G_DEFINE_TYPE (GdkGC, gdk_gc, G_TYPE_OBJECT)

static void
gdk_gc_class_init (GdkGCClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  object_class->finalize = gdk_gc_finalize;

  g_type_class_add_private (object_class, sizeof (GdkGCPrivate));
}

static void
gdk_gc_init (GdkGC *gc)
{
  GdkGCPrivate *priv = GDK_GC_GET_PRIVATE (gc);

  priv->fill = GDK_SOLID;

  /* These are the default X11 value, which we match. They are clearly
   * wrong for TrueColor displays, so apps have to change them.
   */
  priv->fg_pixel = 0;
  priv->bg_pixel = 1;
}

/**
 * gdk_gc_new:
 * @drawable: a #GdkDrawable. The created GC must always be used
 *   with drawables of the same depth as this one.
 *
 * Create a new graphics context with default values. 
 *
 * Returns: the new graphics context.
 **/
GdkGC*
gdk_gc_new (GdkDrawable *drawable)
{
  g_return_val_if_fail (drawable != NULL, NULL);

  return gdk_gc_new_with_values (drawable, NULL, 0);
}

/**
 * gdk_gc_new_with_values:
 * @drawable: a #GdkDrawable. The created GC must always be used
 *   with drawables of the same depth as this one.
 * @values: a structure containing initial values for the GC.
 * @values_mask: a bit mask indicating which fields in @values
 *   are set.
 * 
 * Create a new GC with the given initial values.
 * 
 * Return value: the new graphics context.
 **/
GdkGC*
gdk_gc_new_with_values (GdkDrawable	*drawable,
			GdkGCValues	*values,
			GdkGCValuesMask	 values_mask)
{
  g_return_val_if_fail (drawable != NULL, NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->create_gc (drawable,
						       values,
						       values_mask);
}

/**
 * _gdk_gc_init:
 * @gc: a #GdkGC
 * @drawable: a #GdkDrawable.
 * @values: a structure containing initial values for the GC.
 * @values_mask: a bit mask indicating which fields in @values
 *   are set.
 * 
 * Does initialization of the generic portions of a #GdkGC
 * created with the specified values and values_mask. This
 * should be called out of the implementation of
 * GdkDrawable.create_gc() immediately after creating the
 * #GdkGC object.
 **/
void
_gdk_gc_init (GdkGC           *gc,
	      GdkDrawable     *drawable,
	      GdkGCValues     *values,
	      GdkGCValuesMask  values_mask)
{
  GdkGCPrivate *priv;

  g_return_if_fail (GDK_IS_GC (gc));

  priv = GDK_GC_GET_PRIVATE (gc);

  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    gc->clip_x_origin = values->clip_x_origin;
  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    gc->clip_y_origin = values->clip_y_origin;
  if (values_mask & GDK_GC_TS_X_ORIGIN)
    gc->ts_x_origin = values->ts_x_origin;
  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    gc->ts_y_origin = values->ts_y_origin;
  if (values_mask & GDK_GC_FILL)
    priv->fill = values->fill;
  if (values_mask & GDK_GC_STIPPLE)
    {
      priv->stipple = values->stipple;
      if (priv->stipple)
	g_object_ref (priv->stipple);
    }
  if (values_mask & GDK_GC_TILE)
    {
      priv->tile = values->tile;
      if (priv->tile)
	g_object_ref (priv->tile);
    }
  if (values_mask & GDK_GC_FOREGROUND)
    priv->fg_pixel = values->foreground.pixel;
  if (values_mask & GDK_GC_BACKGROUND)
    priv->bg_pixel = values->background.pixel;

  gc->colormap = gdk_drawable_get_colormap (drawable);
  if (gc->colormap)
    g_object_ref (gc->colormap);
}

static void
gdk_gc_finalize (GObject *object)
{
  GdkGC *gc = GDK_GC (object);
  GdkGCPrivate *priv = GDK_GC_GET_PRIVATE (gc);
  
  if (priv->clip_region)
    gdk_region_destroy (priv->clip_region);
  if (gc->colormap)
    g_object_unref (gc->colormap);
  if (priv->tile)
    g_object_unref (priv->tile);
  if (priv->stipple)
    g_object_unref (priv->stipple);

  G_OBJECT_CLASS (gdk_gc_parent_class)->finalize (object);
}

/**
 * gdk_gc_ref:
 * @gc: a #GdkGC
 *
 * Deprecated function; use g_object_ref() instead.
 *
 * Return value: the gc.
 **/
GdkGC *
gdk_gc_ref (GdkGC *gc)
{
  return (GdkGC *) g_object_ref (gc);
}

/**
 * gdk_gc_unref:
 * @gc: a #GdkGC
 *
 * Decrement the reference count of @gc.
 *
 * Deprecated: Use g_object_unref() instead.
 **/
void
gdk_gc_unref (GdkGC *gc)
{
  g_object_unref (gc);
}

/**
 * gdk_gc_get_values:
 * @gc:  a #GdkGC.
 * @values: the #GdkGCValues structure in which to store the results.
 * 
 * Retrieves the current values from a graphics context. Note that 
 * only the pixel values of the @values->foreground and @values->background
 * are filled, use gdk_colormap_query_color() to obtain the rgb values
 * if you need them.
 **/
void
gdk_gc_get_values (GdkGC       *gc,
		   GdkGCValues *values)
{
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (values != NULL);

  GDK_GC_GET_CLASS (gc)->get_values (gc, values);
}

/**
 * gdk_gc_set_values:
 * @gc: a #GdkGC
 * @values: struct containing the new values
 * @values_mask: mask indicating which struct fields are to be used
 *
 * Sets attributes of a graphics context in bulk. For each flag set in
 * @values_mask, the corresponding field will be read from @values and
 * set as the new value for @gc. If you're only setting a few values
 * on @gc, calling individual "setter" functions is likely more
 * convenient.
 * 
 **/
void
gdk_gc_set_values (GdkGC           *gc,
		   GdkGCValues	   *values,
		   GdkGCValuesMask  values_mask)
{
  GdkGCPrivate *priv;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (values != NULL);

  priv = GDK_GC_GET_PRIVATE (gc);

  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    gc->clip_x_origin = values->clip_x_origin;
  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    gc->clip_y_origin = values->clip_y_origin;
  if (values_mask & GDK_GC_TS_X_ORIGIN)
    gc->ts_x_origin = values->ts_x_origin;
  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    gc->ts_y_origin = values->ts_y_origin;
  if (values_mask & GDK_GC_CLIP_MASK)
    {
      GdkGCPrivate *priv = GDK_GC_GET_PRIVATE (gc);
      if (priv->clip_region)
	{
	  gdk_region_destroy (priv->clip_region);
	  priv->clip_region = NULL;
	}
    }
  if (values_mask & GDK_GC_FILL)
    priv->fill = values->fill;
  if (values_mask & GDK_GC_STIPPLE)
    {
      if (priv->stipple != values->stipple)
	{
	  if (priv->stipple)
	    g_object_unref (priv->stipple);
	  priv->stipple = values->stipple;
	  if (priv->stipple)
	    g_object_ref (priv->stipple);
	}
    }
  if (values_mask & GDK_GC_TILE)
    {
      if (priv->tile != values->tile)
	{
	  if (priv->tile)
	    g_object_unref (priv->tile);
	  priv->tile = values->tile;
	  if (priv->tile)
	    g_object_ref (priv->tile);
	}
    }
  if (values_mask & GDK_GC_FOREGROUND)
    priv->fg_pixel = values->foreground.pixel;
  if (values_mask & GDK_GC_BACKGROUND)
    priv->bg_pixel = values->background.pixel;
  
  GDK_GC_GET_CLASS (gc)->set_values (gc, values, values_mask);
}

/**
 * gdk_gc_set_foreground:
 * @gc: a #GdkGC.
 * @color: the new foreground color.
 * 
 * Sets the foreground color for a graphics context.
 * Note that this function uses @color->pixel, use 
 * gdk_gc_set_rgb_fg_color() to specify the foreground 
 * color as red, green, blue components.
 **/
void
gdk_gc_set_foreground (GdkGC	      *gc,
		       const GdkColor *color)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (color != NULL);

  values.foreground = *color;
  gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
}

/**
 * gdk_gc_set_background:
 * @gc: a #GdkGC.
 * @color: the new background color.
 * 
 * Sets the background color for a graphics context.
 * Note that this function uses @color->pixel, use 
 * gdk_gc_set_rgb_bg_color() to specify the background 
 * color as red, green, blue components.
 **/
void
gdk_gc_set_background (GdkGC	      *gc,
		       const GdkColor *color)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (color != NULL);

  values.background = *color;
  gdk_gc_set_values (gc, &values, GDK_GC_BACKGROUND);
}

/**
 * gdk_gc_set_font:
 * @gc: a #GdkGC.
 * @font: the new font. 
 * 
 * Sets the font for a graphics context. (Note that
 * all text-drawing functions in GDK take a @font
 * argument; the value set here is used when that
 * argument is %NULL.)
 **/
void
gdk_gc_set_font (GdkGC	 *gc,
		 GdkFont *font)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (font != NULL);

  values.font = font;
  gdk_gc_set_values (gc, &values, GDK_GC_FONT);
}

/**
 * gdk_gc_set_function:
 * @gc: a #GdkGC.
 * @function: the #GdkFunction to use
 * 
 * Determines how the current pixel values and the
 * pixel values being drawn are combined to produce
 * the final pixel values.
 **/
void
gdk_gc_set_function (GdkGC	 *gc,
		     GdkFunction  function)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.function = function;
  gdk_gc_set_values (gc, &values, GDK_GC_FUNCTION);
}

/**
 * gdk_gc_set_fill:
 * @gc: a #GdkGC.
 * @fill: the new fill mode.
 * 
 * Set the fill mode for a graphics context.
 **/
void
gdk_gc_set_fill (GdkGC	 *gc,
		 GdkFill  fill)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.fill = fill;
  gdk_gc_set_values (gc, &values, GDK_GC_FILL);
}

/**
 * gdk_gc_set_tile:
 * @gc:  a #GdkGC.
 * @tile:  the new tile pixmap.
 * 
 * Set a tile pixmap for a graphics context.
 * This will only be used if the fill mode
 * is %GDK_TILED.
 **/
void
gdk_gc_set_tile (GdkGC	   *gc,
		 GdkPixmap *tile)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.tile = tile;
  gdk_gc_set_values (gc, &values, GDK_GC_TILE);
}

/**
 * gdk_gc_set_stipple:
 * @gc: a #GdkGC.
 * @stipple: the new stipple bitmap.
 * 
 * Set the stipple bitmap for a graphics context. The
 * stipple will only be used if the fill mode is
 * %GDK_STIPPLED or %GDK_OPAQUE_STIPPLED.
 **/
void
gdk_gc_set_stipple (GdkGC     *gc,
		    GdkPixmap *stipple)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.stipple = stipple;
  gdk_gc_set_values (gc, &values, GDK_GC_STIPPLE);
}

/**
 * gdk_gc_set_ts_origin:
 * @gc:  a #GdkGC.
 * @x: the x-coordinate of the origin.
 * @y: the y-coordinate of the origin.
 * 
 * Set the origin when using tiles or stipples with
 * the GC. The tile or stipple will be aligned such
 * that the upper left corner of the tile or stipple
 * will coincide with this point.
 **/
void
gdk_gc_set_ts_origin (GdkGC *gc,
		      gint   x,
		      gint   y)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.ts_x_origin = x;
  values.ts_y_origin = y;
  
  gdk_gc_set_values (gc, &values,
		     GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
}

/**
 * gdk_gc_set_clip_origin:
 * @gc: a #GdkGC.
 * @x: the x-coordinate of the origin.
 * @y: the y-coordinate of the origin.
 * 
 * Sets the origin of the clip mask. The coordinates are
 * interpreted relative to the upper-left corner of
 * the destination drawable of the current operation.
 **/
void
gdk_gc_set_clip_origin (GdkGC *gc,
			gint   x,
			gint   y)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.clip_x_origin = x;
  values.clip_y_origin = y;
  
  gdk_gc_set_values (gc, &values,
		     GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
}

/**
 * gdk_gc_set_clip_mask:
 * @gc: the #GdkGC.
 * @mask: a bitmap.
 * 
 * Sets the clip mask for a graphics context from a bitmap.
 * The clip mask is interpreted relative to the clip
 * origin. (See gdk_gc_set_clip_origin()).
 **/
void
gdk_gc_set_clip_mask (GdkGC	*gc,
		      GdkBitmap *mask)
{
  GdkGCValues values;
  
  g_return_if_fail (GDK_IS_GC (gc));
  
  values.clip_mask = mask;
  gdk_gc_set_values (gc, &values, GDK_GC_CLIP_MASK);
}

static void
_gdk_gc_set_clip_region_internal (GdkGC     *gc,
				  GdkRegion *region)
{
  GdkGCPrivate *priv = GDK_GC_GET_PRIVATE (gc);

  if (priv->clip_region)
    gdk_region_destroy (priv->clip_region);

  priv->clip_region = region;

  _gdk_windowing_gc_set_clip_region (gc, region);
}

/**
 * gdk_gc_set_clip_rectangle:
 * @gc: a #GdkGC.
 * @rectangle: the rectangle to clip to.
 * 
 * Sets the clip mask for a graphics context from a
 * rectangle. The clip mask is interpreted relative to the clip
 * origin. (See gdk_gc_set_clip_origin()).
 **/
void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkRegion *region;
  
  g_return_if_fail (GDK_IS_GC (gc));

  if (rectangle)
    region = gdk_region_rectangle (rectangle);
  else
    region = NULL;

  _gdk_gc_set_clip_region_internal (gc, region);
}

/**
 * gdk_gc_set_clip_region:
 * @gc: a #GdkGC.
 * @region: the #GdkRegion. 
 * 
 * Sets the clip mask for a graphics context from a region structure.
 * The clip mask is interpreted relative to the clip origin. (See
 * gdk_gc_set_clip_origin()).
 **/
void
gdk_gc_set_clip_region (GdkGC	  *gc,
			GdkRegion *region)
{
  g_return_if_fail (GDK_IS_GC (gc));

  if (region)
    region = gdk_region_copy (region);
  
  _gdk_gc_set_clip_region_internal (gc, region);
}

/**
 * _gdk_gc_get_clip_region:
 * @gc: a #GdkGC
 * 
 * Gets the current clip region for @gc, if any.
 * 
 * Return value: the clip region for the GC, or %NULL.
 *   (if a clip mask is set, the return will be %NULL)
 *   This value is owned by the GC and must not be freed.
 **/
GdkRegion *
_gdk_gc_get_clip_region (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), NULL);

  return GDK_GC_GET_PRIVATE (gc)->clip_region;
}

/**
 * _gdk_gc_get_fill:
 * @gc: a #GdkGC
 * 
 * Gets the current file style for the GC
 * 
 * Return value: the file style for the GC
 **/
GdkFill
_gdk_gc_get_fill (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), GDK_SOLID);

  return GDK_GC_GET_PRIVATE (gc)->fill;
}

/**
 * _gdk_gc_get_tile:
 * @gc: a #GdkGC
 * 
 * Gets the tile pixmap for @gc, if any
 * 
 * Return value: the tile set on the GC, or %NULL. The
 *   value is owned by the GC and must not be freed.
 **/
GdkPixmap *
_gdk_gc_get_tile (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), NULL);

  return GDK_GC_GET_PRIVATE (gc)->tile;
}

/**
 * _gdk_gc_get_stipple:
 * @gc: a #GdkGC
 * 
 * Gets the stipple pixmap for @gc, if any
 * 
 * Return value: the stipple set on the GC, or %NULL. The
 *   value is owned by the GC and must not be freed.
 **/
GdkBitmap *
_gdk_gc_get_stipple (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), NULL);

  return GDK_GC_GET_PRIVATE (gc)->stipple;
}

/**
 * _gdk_gc_get_fg_pixel:
 * @gc: a #GdkGC
 * 
 * Gets the foreground pixel value for @gc. If the
 * foreground pixel has never been set, returns the
 * default value 0.
 * 
 * Return value: the foreground pixel value of the GC
 **/
guint32
_gdk_gc_get_fg_pixel (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), 0);
  
  return GDK_GC_GET_PRIVATE (gc)->fg_pixel;
}

/**
 * _gdk_gc_get_bg_pixel:
 * @gc: a #GdkGC
 * 
 * Gets the background pixel value for @gc.If the
 * foreground pixel has never been set, returns the
 * default value 1.
 * 
 * Return value: the foreground pixel value of the GC
 **/
guint32
_gdk_gc_get_bg_pixel (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), 0);
  
  return GDK_GC_GET_PRIVATE (gc)->bg_pixel;
}

/**
 * gdk_gc_set_subwindow:
 * @gc: a #GdkGC.
 * @mode: the subwindow mode.
 * 
 * Sets how drawing with this GC on a window will affect child
 * windows of that window. 
 **/
void
gdk_gc_set_subwindow (GdkGC	       *gc,
		      GdkSubwindowMode	mode)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.subwindow_mode = mode;
  gdk_gc_set_values (gc, &values, GDK_GC_SUBWINDOW);
}

/**
 * gdk_gc_set_exposures:
 * @gc: a #GdkGC.
 * @exposures: if %TRUE, exposure events will be generated.
 * 
 * Sets whether copying non-visible portions of a drawable
 * using this graphics context generate exposure events
 * for the corresponding regions of the destination
 * drawable. (See gdk_draw_drawable()).
 **/
void
gdk_gc_set_exposures (GdkGC     *gc,
		      gboolean   exposures)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.graphics_exposures = exposures;
  gdk_gc_set_values (gc, &values, GDK_GC_EXPOSURES);
}

/**
 * gdk_gc_set_line_attributes:
 * @gc: a #GdkGC.
 * @line_width: the width of lines.
 * @line_style: the dash-style for lines.
 * @cap_style: the manner in which the ends of lines are drawn.
 * @join_style: the in which lines are joined together.
 * 
 * Sets various attributes of how lines are drawn. See
 * the corresponding members of #GdkGCValues for full
 * explanations of the arguments.
 **/
void
gdk_gc_set_line_attributes (GdkGC	*gc,
			    gint	 line_width,
			    GdkLineStyle line_style,
			    GdkCapStyle	 cap_style,
			    GdkJoinStyle join_style)
{
  GdkGCValues values;

  values.line_width = line_width;
  values.line_style = line_style;
  values.cap_style = cap_style;
  values.join_style = join_style;

  gdk_gc_set_values (gc, &values,
		     GDK_GC_LINE_WIDTH |
		     GDK_GC_LINE_STYLE |
		     GDK_GC_CAP_STYLE |
		     GDK_GC_JOIN_STYLE);
}

/**
 * gdk_gc_set_dashes:
 * @gc: a #GdkGC.
 * @dash_offset: the phase of the dash pattern.
 * @dash_list: an array of dash lengths.
 * @n: the number of elements in @dash_list.
 * 
 * Sets the way dashed-lines are drawn. Lines will be
 * drawn with alternating on and off segments of the
 * lengths specified in @dash_list. The manner in
 * which the on and off segments are drawn is determined
 * by the @line_style value of the GC. (This can
 * be changed with gdk_gc_set_line_attributes().)
 *
 * The @dash_offset defines the phase of the pattern, 
 * specifying how many pixels into the dash-list the pattern 
 * should actually begin.
 **/
void
gdk_gc_set_dashes (GdkGC *gc,
		   gint	  dash_offset,
		   gint8  dash_list[],
		   gint   n)
{
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (dash_list != NULL);

  GDK_GC_GET_CLASS (gc)->set_dashes (gc, dash_offset, dash_list, n);
}

/**
 * gdk_gc_offset:
 * @gc: a #GdkGC
 * @x_offset: amount by which to offset the GC in the X direction
 * @y_offset: amount by which to offset the GC in the Y direction
 * 
 * Offset attributes such as the clip and tile-stipple origins
 * of the GC so that drawing at x - x_offset, y - y_offset with
 * the offset GC  has the same effect as drawing at x, y with the original
 * GC.
 **/
void
gdk_gc_offset (GdkGC *gc,
	       gint   x_offset,
	       gint   y_offset)
{
  if (x_offset != 0 || y_offset != 0)
    {
      GdkGCValues values;

      values.clip_x_origin = gc->clip_x_origin - x_offset;
      values.clip_y_origin = gc->clip_y_origin - y_offset;
      values.ts_x_origin = gc->ts_x_origin - x_offset;
      values.ts_y_origin = gc->ts_y_origin - y_offset;
      
      gdk_gc_set_values (gc, &values,
			 GDK_GC_CLIP_X_ORIGIN |
			 GDK_GC_CLIP_Y_ORIGIN |
			 GDK_GC_TS_X_ORIGIN |
			 GDK_GC_TS_Y_ORIGIN);
    }
}

/**
 * gdk_gc_copy:
 * @dst_gc: the destination graphics context.
 * @src_gc: the source graphics context.
 * 
 * Copy the set of values from one graphics context
 * onto another graphics context.
 **/
void
gdk_gc_copy (GdkGC *dst_gc,
	     GdkGC *src_gc)
{
  GdkGCPrivate *dst_priv, *src_priv;
  
  g_return_if_fail (GDK_IS_GC (dst_gc));
  g_return_if_fail (GDK_IS_GC (src_gc));

  dst_priv = GDK_GC_GET_PRIVATE (dst_gc);
  src_priv = GDK_GC_GET_PRIVATE (src_gc);

  _gdk_windowing_gc_copy (dst_gc, src_gc);

  dst_gc->clip_x_origin = src_gc->clip_x_origin;
  dst_gc->clip_y_origin = src_gc->clip_y_origin;
  dst_gc->ts_x_origin = src_gc->ts_x_origin;
  dst_gc->ts_y_origin = src_gc->ts_y_origin;

  if (src_gc->colormap)
    g_object_ref (src_gc->colormap);

  if (dst_gc->colormap)
    g_object_unref (dst_gc->colormap);

  dst_gc->colormap = src_gc->colormap;

  if (dst_priv->clip_region)
    gdk_region_destroy (dst_priv->clip_region);

  if (src_priv->clip_region)
    dst_priv->clip_region = gdk_region_copy (src_priv->clip_region);
  else
    dst_priv->clip_region = NULL;
  
  dst_priv->fill = src_priv->fill;
  
  if (dst_priv->stipple)
    g_object_unref (dst_priv->stipple);
  dst_priv->stipple = src_priv->stipple;
  if (dst_priv->stipple)
    g_object_ref (dst_priv->stipple);
  
  if (dst_priv->tile)
    g_object_unref (dst_priv->tile);
  dst_priv->tile = src_priv->tile;
  if (dst_priv->tile)
    g_object_ref (dst_priv->tile);

  dst_priv->fg_pixel = src_priv->fg_pixel;
  dst_priv->bg_pixel = src_priv->bg_pixel;
}

/**
 * gdk_gc_set_colormap:
 * @gc: a #GdkGC
 * @colormap: a #GdkColormap
 * 
 * Sets the colormap for the GC to the given colormap. The depth
 * of the colormap's visual must match the depth of the drawable
 * for which the GC was created.
 **/
void
gdk_gc_set_colormap (GdkGC       *gc,
		     GdkColormap *colormap)
{
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  if (gc->colormap != colormap)
    {
      if (gc->colormap)
	g_object_unref (gc->colormap);

      gc->colormap = colormap;
      g_object_ref (gc->colormap);
    }
    
}

/**
 * gdk_gc_get_colormap:
 * @gc: a #GdkGC
 * 
 * Retrieves the colormap for a given GC, if it exists.
 * A GC will have a colormap if the drawable for which it was created
 * has a colormap, or if a colormap was set explicitely with
 * gdk_gc_set_colormap.
 * 
 * Return value: the colormap of @gc, or %NULL if @gc doesn't have one.
 **/
GdkColormap *
gdk_gc_get_colormap (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC (gc), NULL);

  return gc->colormap;
}

static GdkColormap *
gdk_gc_get_colormap_warn (GdkGC *gc)
{
  GdkColormap *colormap = gdk_gc_get_colormap (gc);
  if (!colormap)
    {
      g_warning ("gdk_gc_set_rgb_fg_color() and gdk_gc_set_rgb_bg_color() can\n"
		 "only be used on GC's with a colormap. A GC will have a colormap\n"
		 "if it is created for a drawable with a colormap, or if a\n"
		 "colormap has been set explicitly with gdk_gc_set_colormap.\n");
      return NULL;
    }

  return colormap;
}

/**
 * gdk_gc_set_rgb_fg_color:
 * @gc: a #GdkGC
 * @color: an unallocated #GdkColor.
 * 
 * Set the foreground color of a GC using an unallocated color. The
 * pixel value for the color will be determined using GdkRGB. If the
 * colormap for the GC has not previously been initialized for GdkRGB,
 * then for pseudo-color colormaps (colormaps with a small modifiable
 * number of colors), a colorcube will be allocated in the colormap.
 * 
 * Calling this function for a GC without a colormap is an error.
 **/
void
gdk_gc_set_rgb_fg_color (GdkGC          *gc,
			 const GdkColor *color)
{
  GdkColormap *cmap;
  GdkColor tmp_color;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (color != NULL);

  cmap = gdk_gc_get_colormap_warn (gc);
  if (!cmap)
    return;

  tmp_color = *color;
  gdk_rgb_find_color (cmap, &tmp_color);
  gdk_gc_set_foreground (gc, &tmp_color);
}

/**
 * gdk_gc_set_rgb_bg_color:
 * @gc: a #GdkGC
 * @color: an unallocated #GdkColor.
 * 
 * Set the background color of a GC using an unallocated color. The
 * pixel value for the color will be determined using GdkRGB. If the
 * colormap for the GC has not previously been initialized for GdkRGB,
 * then for pseudo-color colormaps (colormaps with a small modifiable
 * number of colors), a colorcube will be allocated in the colormap.
 * 
 * Calling this function for a GC without a colormap is an error.
 **/
void
gdk_gc_set_rgb_bg_color (GdkGC          *gc,
			 const GdkColor *color)
{
  GdkColormap *cmap;
  GdkColor tmp_color;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (color != NULL);

  cmap = gdk_gc_get_colormap_warn (gc);
  if (!cmap)
    return;

  tmp_color = *color;
  gdk_rgb_find_color (cmap, &tmp_color);
  gdk_gc_set_background (gc, &tmp_color);
}

static cairo_surface_t *
make_stipple_tile_surface (cairo_t   *cr,
			   GdkBitmap *stipple,
			   GdkColor  *foreground,
			   GdkColor  *background)
{
  cairo_t *tmp_cr;
  cairo_surface_t *surface; 
  cairo_surface_t *alpha_surface;
  gint width, height;

  gdk_drawable_get_size (stipple,
			 &width, &height);
  
  alpha_surface = _gdk_drawable_ref_cairo_surface (stipple);
  
  surface = cairo_surface_create_similar (cairo_get_target (cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  width, height);

  tmp_cr = cairo_create (surface);
  
  cairo_set_operator (tmp_cr, CAIRO_OPERATOR_SOURCE);
 
  if (background)
      gdk_cairo_set_source_color (tmp_cr, background);
  else
      cairo_set_source_rgba (tmp_cr, 0, 0, 0 ,0);

  cairo_paint (tmp_cr);

  cairo_set_operator (tmp_cr, CAIRO_OPERATOR_OVER);

  gdk_cairo_set_source_color (tmp_cr, foreground);
  cairo_mask_surface (tmp_cr, alpha_surface, 0, 0);
  
  cairo_destroy (tmp_cr);
  cairo_surface_destroy (alpha_surface);

  return surface;
}

static void
gc_get_foreground (GdkGC    *gc,
		   GdkColor *color)
{
  GdkGCPrivate *priv = GDK_GC_GET_PRIVATE (gc);
  
  color->pixel = priv->bg_pixel;

  if (gc->colormap)
    gdk_colormap_query_color (gc->colormap, priv->fg_pixel, color);
  else
    g_warning ("No colormap in gc_get_foreground");
}

static void
gc_get_background (GdkGC    *gc,
		   GdkColor *color)
{
  GdkGCPrivate *priv = GDK_GC_GET_PRIVATE (gc);
  
  color->pixel = priv->bg_pixel;

  if (gc->colormap)
    gdk_colormap_query_color (gc->colormap, priv->bg_pixel, color);
  else
    g_warning ("No colormap in gc_get_background");
}

/**
 * _gdk_gc_update_context:
 * @gc: a #GdkGC
 * @cr: a #cairo_t
 * @override_foreground: a foreground color to use to override the
 *   foreground color of the GC
 * @override_stipple: a stipple pattern to use to override the
 *   stipple from the GC. If this is present and the fill mode
 *   of the GC isn't %GDK_STIPPLED or %GDK_OPAQUE_STIPPLED
 *   the fill mode will be forced to %GDK_STIPPLED
 * @gc_changed: pass %FALSE if the @gc has not changed since the
 *     last call to this function
 * 
 * Set the attributes of a cairo context to match those of a #GdkGC
 * as far as possible. Some aspects of a #GdkGC, such as clip masks
 * and functions other than %GDK_COPY are not currently handled.
 **/
void
_gdk_gc_update_context (GdkGC     *gc,
			cairo_t   *cr,
			GdkColor  *override_foreground,
			GdkBitmap *override_stipple,
			gboolean   gc_changed)
{
  GdkGCPrivate *priv;
  GdkFill fill;
  GdkColor foreground;
  GdkColor background;
  cairo_surface_t *tile_surface = NULL;
  GdkBitmap *stipple = NULL;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (override_stipple == NULL || GDK_IS_PIXMAP (override_stipple));

  priv = GDK_GC_GET_PRIVATE (gc);

  fill = priv->fill;
  if (override_stipple && fill != GDK_OPAQUE_STIPPLED)
    fill = GDK_STIPPLED;

  if (fill != GDK_TILED)
    {
      if (override_foreground)
	foreground = *override_foreground;
      else
	gc_get_foreground (gc, &foreground);
    }

  if (fill == GDK_OPAQUE_STIPPLED)
    gc_get_background (gc, &background);


  switch (fill)
    {
    case GDK_SOLID:
      break;
    case GDK_TILED:
      if (!priv->tile)
	fill = GDK_SOLID;
      break;
    case GDK_STIPPLED:
    case GDK_OPAQUE_STIPPLED:
      if (override_stipple)
	stipple = override_stipple;
      else
	stipple = priv->stipple;
      
      if (!stipple)
	fill = GDK_SOLID;
      break;
    }
  
  switch (fill)
    {
    case GDK_SOLID:
      gdk_cairo_set_source_color (cr, &foreground);
      break;
    case GDK_TILED:
      tile_surface = _gdk_drawable_ref_cairo_surface (priv->tile);
      break;
    case GDK_STIPPLED:
      tile_surface = make_stipple_tile_surface (cr, stipple, &foreground, NULL);
      break;
    case GDK_OPAQUE_STIPPLED:
      tile_surface = make_stipple_tile_surface (cr, stipple, &foreground, &background);
      break;
    }

  /* Tiles, stipples, and clip regions are all specified in device space,
   * not user space. For the clip region, we can simply change the matrix,
   * clip, then clip back, but for the source pattern, we need to
   * compute the right matrix.
   *
   * What we want is:
   *
   *     CTM_inverse * Pattern_matrix = Translate(- ts_x, - ts_y)
   *
   * (So that ts_x, ts_y in device space is taken to 0,0 in pattern
   * space). So, pattern_matrix = CTM * Translate(- ts_x, - tx_y);
   */

  if (tile_surface)
    {
      cairo_pattern_t *pattern = cairo_pattern_create_for_surface (tile_surface);
      cairo_matrix_t user_to_device;
      cairo_matrix_t user_to_pattern;
      cairo_matrix_t device_to_pattern;

      cairo_get_matrix (cr, &user_to_device);
      cairo_matrix_init_translate (&device_to_pattern,
				   - gc->ts_x_origin, - gc->ts_y_origin);
      cairo_matrix_multiply (&user_to_pattern,
			     &user_to_device, &device_to_pattern);
      
      cairo_pattern_set_matrix (pattern, &user_to_pattern);
      cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
      cairo_set_source (cr, pattern);
      
      cairo_surface_destroy (tile_surface);
      cairo_pattern_destroy (pattern);
    }

  if (!gc_changed)
    return;

  cairo_reset_clip (cr);
  if (priv->clip_region)
    {
      cairo_save (cr);

      cairo_identity_matrix (cr);
      cairo_translate (cr, gc->clip_x_origin, gc->clip_y_origin);

      cairo_new_path (cr);
      gdk_cairo_region (cr, priv->clip_region);

      cairo_restore (cr);

      cairo_clip (cr);
    }
}


#define __GDK_GC_C__
#include "gdkaliasdef.c"
