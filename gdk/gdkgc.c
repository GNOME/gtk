/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <string.h>

#include "gdkgc.h"
#include "gdkrgb.h"
#include "gdkprivate.h"

static void gdk_gc_class_init (GObjectClass *class);
static void gdk_gc_finalize   (GObject      *object);

GObjectClass *parent_class;

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
                                            &object_info);
    }
  
  return object_type;
}

static void
gdk_gc_class_init (GObjectClass *class)
{
  parent_class = g_type_class_peek_parent (class);
  
  class->finalize = gdk_gc_finalize;
}

GdkGC*
gdk_gc_new (GdkDrawable *drawable)
{
  g_return_val_if_fail (drawable != NULL, NULL);

  return gdk_gc_new_with_values (drawable, NULL, 0);
}

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

  gc->colormap = gdk_drawable_get_colormap (drawable);
  if (gc->colormap)
    g_object_ref (G_OBJECT (gc->colormap));
  
  return gc;
}

static void
gdk_gc_finalize (GObject *object)
{
  GdkGC *gc = GDK_GC (object);
  
  if (gc->colormap)
    g_object_unref (G_OBJECT (gc->colormap));

  parent_class->finalize (object);
}

GdkGC *
gdk_gc_ref (GdkGC *gc)
{
  return (GdkGC *) g_object_ref (G_OBJECT (gc));
}

void
gdk_gc_unref (GdkGC *gc)
{
  g_object_unref (G_OBJECT (gc));
}

void
gdk_gc_get_values (GdkGC       *gc,
		   GdkGCValues *values)
{
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (values != NULL);

  GDK_GC_GET_CLASS (gc)->get_values (gc, values);
}

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

void
gdk_gc_set_foreground (GdkGC	*gc,
		       GdkColor *color)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (color != NULL);

  values.foreground = *color;
  gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
}

void
gdk_gc_set_background (GdkGC	*gc,
		       GdkColor *color)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (color != NULL);

  values.background = *color;
  gdk_gc_set_values (gc, &values, GDK_GC_BACKGROUND);
}

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

void
gdk_gc_set_function (GdkGC	 *gc,
		     GdkFunction  function)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.function = function;
  gdk_gc_set_values (gc, &values, GDK_GC_FUNCTION);
}

void
gdk_gc_set_fill (GdkGC	 *gc,
		 GdkFill  fill)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.fill = fill;
  gdk_gc_set_values (gc, &values, GDK_GC_FILL);
}

void
gdk_gc_set_tile (GdkGC	   *gc,
		 GdkPixmap *tile)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.tile = tile;
  gdk_gc_set_values (gc, &values, GDK_GC_TILE);
}

void
gdk_gc_set_stipple (GdkGC     *gc,
		    GdkPixmap *stipple)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.stipple = stipple;
  gdk_gc_set_values (gc, &values, GDK_GC_STIPPLE);
}

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

void
gdk_gc_set_clip_mask (GdkGC	*gc,
		      GdkBitmap *mask)
{
  GdkGCValues values;
  
  g_return_if_fail (GDK_IS_GC (gc));
  
  values.clip_mask = mask;
  gdk_gc_set_values (gc, &values, GDK_GC_CLIP_MASK);
}


void
gdk_gc_set_subwindow (GdkGC	       *gc,
		      GdkSubwindowMode	mode)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.subwindow_mode = mode;
  gdk_gc_set_values (gc, &values, GDK_GC_SUBWINDOW);
}

void
gdk_gc_set_exposures (GdkGC     *gc,
		      gboolean   exposures)
{
  GdkGCValues values;

  g_return_if_fail (GDK_IS_GC (gc));

  values.graphics_exposures = exposures;
  gdk_gc_set_values (gc, &values, GDK_GC_EXPOSURES);
}

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
	g_object_unref (G_OBJECT (gc->colormap));

      gc->colormap = colormap;
      g_object_ref (G_OBJECT (gc->colormap));
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
 * Return value: 
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
gdk_gc_set_rgb_fg_color (GdkGC *gc, GdkColor *color)
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
gdk_gc_set_rgb_bg_color (GdkGC *gc, GdkColor *color)
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
