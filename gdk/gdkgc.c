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

#include "gdkalias.h"
#include "gdkgc.h"
#include "gdkrgb.h"
#include "gdkprivate.h"

static void gdk_gc_class_init (GObjectClass *class);
static void gdk_gc_finalize   (GObject      *object);

static GObjectClass *parent_class;

GType
gdk_gc_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkGCClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_gc_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkGC),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkGC",
                                            &object_info, 
					    G_TYPE_FLAG_ABSTRACT);
    }
  
  return object_type;
}

static void
gdk_gc_class_init (GObjectClass *class)
{
  parent_class = g_type_class_peek_parent (class);
  
  class->finalize = gdk_gc_finalize;
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
  GdkGC *gc;

  g_return_val_if_fail (drawable != NULL, NULL);

  gc = GDK_DRAWABLE_GET_CLASS (drawable)->create_gc (drawable,
                                                     values,
                                                     values_mask);

  if (gc == NULL) /* This would mean the drawable was destroyed. */
    return NULL;
  
  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    gc->clip_x_origin = values->clip_x_origin;
  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    gc->clip_y_origin = values->clip_y_origin;
  if (values_mask & GDK_GC_TS_X_ORIGIN)
    gc->ts_x_origin = values->ts_x_origin;
  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    gc->ts_y_origin = values->ts_y_origin;

  /* gc->colormap will already be set if gdk_gc_new_with_values()
   * recurses - as in GdkPixmap => impl object.
   */
  if (!gc->colormap)
    {
      gc->colormap = gdk_drawable_get_colormap (drawable);
      if (gc->colormap)
	g_object_ref (gc->colormap);
    }
  
  return gc;
}

static void
gdk_gc_finalize (GObject *object)
{
  GdkGC *gc = GDK_GC (object);
  
  if (gc->colormap)
    g_object_unref (gc->colormap);

  parent_class->finalize (object);
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
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (values != NULL);

  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    gc->clip_x_origin = values->clip_x_origin;
  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    gc->clip_y_origin = values->clip_y_origin;
  if (values_mask & GDK_GC_TS_X_ORIGIN)
    gc->ts_x_origin = values->ts_x_origin;
  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    gc->ts_y_origin = values->ts_y_origin;
  
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
