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

#include "gdkdrawable.h"
#include "gdkinternals.h"
#include "gdkwindow.h"

static GdkDrawable* gdk_drawable_real_get_composite_drawable (GdkDrawable *drawable,
							      gint         x,
							      gint         y,
							      gint         width,
							      gint         height,
							      gint        *composite_x_offset,
							      gint        *composite_y_offset);
static GdkRegion *  gdk_drawable_real_get_visible_region     (GdkDrawable *drawable);

static void gdk_drawable_class_init (GdkDrawableClass *klass);

GType
gdk_drawable_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDrawableClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawable),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDrawable",
                                            &object_info, 0);
    }  

  return object_type;
}

static void
gdk_drawable_class_init (GdkDrawableClass *klass)
{
  klass->get_composite_drawable = gdk_drawable_real_get_composite_drawable;
  /* Default implementation for clip and visible region is the same */
  klass->get_clip_region = gdk_drawable_real_get_visible_region;
  klass->get_visible_region = gdk_drawable_real_get_visible_region;
}

/* Manipulation of drawables
 */

void          
gdk_drawable_set_data (GdkDrawable   *drawable,
		       const gchar   *key,
		       gpointer	      data,
		       GDestroyNotify destroy_func)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  
  g_object_set_qdata_full (G_OBJECT (drawable),
                           g_quark_from_string (key),
                           data,
                           destroy_func);
}

gpointer
gdk_drawable_get_data (GdkDrawable   *drawable,
		       const gchar   *key)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  
  return g_object_get_qdata (G_OBJECT (drawable),
                             g_quark_try_string (key));
}

void
gdk_drawable_get_size (GdkDrawable *drawable,
		       gint        *width,
		       gint        *height)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  GDK_DRAWABLE_GET_CLASS (drawable)->get_size (drawable, width, height);  
}

GdkVisual*
gdk_drawable_get_visual (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  
  return GDK_DRAWABLE_GET_CLASS (drawable)->get_visual (drawable);
}

gint
gdk_drawable_get_depth (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), 0);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_depth (drawable);
}

void
gdk_drawable_set_colormap (GdkDrawable *drawable,
                           GdkColormap *cmap)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  GDK_DRAWABLE_GET_CLASS (drawable)->set_colormap (drawable, cmap);
}

GdkColormap*
gdk_drawable_get_colormap (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_colormap (drawable);
}

GdkDrawable*
gdk_drawable_ref (GdkDrawable *drawable)
{
  return (GdkDrawable *) g_object_ref (G_OBJECT (drawable));
}

void
gdk_drawable_unref (GdkDrawable *drawable)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  g_object_unref (G_OBJECT (drawable));
}

/* Drawing
 */
void
gdk_draw_point (GdkDrawable *drawable,
                GdkGC       *gc,
                gint         x,
                gint         y)
{
  GdkPoint point;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  point.x = x;
  point.y = y;
  
  GDK_DRAWABLE_GET_CLASS (drawable)->draw_points (drawable, gc, &point, 1);
}

void
gdk_draw_line (GdkDrawable *drawable,
	       GdkGC       *gc,
	       gint         x1,
	       gint         y1,
	       gint         x2,
	       gint         y2)
{
  GdkSegment segment;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  segment.x1 = x1;
  segment.y1 = y1;
  segment.x2 = x2;
  segment.y2 = y2;
  GDK_DRAWABLE_GET_CLASS (drawable)->draw_segments (drawable, gc, &segment, 1);
}

void
gdk_draw_rectangle (GdkDrawable *drawable,
		    GdkGC       *gc,
		    gint         filled,
		    gint         x,
		    gint         y,
		    gint         width,
		    gint         height)
{  
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  if (width < 0 || height < 0)
    {
      gint real_width;
      gint real_height;
      
      gdk_drawable_get_size (drawable, &real_width, &real_height);

      if (width < 0)
        width = real_width;
      if (height < 0)
        height = real_height;
    }

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_rectangle (drawable, gc, filled, x, y,
                                                     width, height);
}

void
gdk_draw_arc (GdkDrawable *drawable,
	      GdkGC       *gc,
	      gint         filled,
	      gint         x,
	      gint         y,
	      gint         width,
	      gint         height,
	      gint         angle1,
	      gint         angle2)
{  
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  if (width < 0 || height < 0)
    {
      gint real_width;
      gint real_height;
      
      gdk_drawable_get_size (drawable, &real_width, &real_height);

      if (width < 0)
        width = real_width;
      if (height < 0)
        height = real_height;
    }

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_arc (drawable, gc, filled,
                                               x, y, width, height, angle1, angle2);
}

void
gdk_draw_polygon (GdkDrawable *drawable,
		  GdkGC       *gc,
		  gint         filled,
		  GdkPoint    *points,
		  gint         npoints)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_polygon (drawable, gc, filled,
                                                   points, npoints);
}

/* gdk_draw_string
 *
 * Modified by Li-Da Lho to draw 16 bits and Multibyte strings
 *
 * Interface changed: add "GdkFont *font" to specify font or fontset explicitely
 */
void
gdk_draw_string (GdkDrawable *drawable,
		 GdkFont     *font,
		 GdkGC       *gc,
		 gint         x,
		 gint         y,
		 const gchar *string)
{
  gdk_draw_text (drawable, font, gc, x, y, string, _gdk_font_strlen (font, string));
}

/* gdk_draw_text
 *
 * Modified by Li-Da Lho to draw 16 bits and Multibyte strings
 *
 * Interface changed: add "GdkFont *font" to specify font or fontset explicitely
 */
void
gdk_draw_text (GdkDrawable *drawable,
	       GdkFont     *font,
	       GdkGC       *gc,
	       gint         x,
	       gint         y,
	       const gchar *text,
	       gint         text_length)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (font != NULL);
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (text != NULL);

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_text (drawable, font, gc, x, y, text, text_length);
}

void
gdk_draw_text_wc (GdkDrawable	 *drawable,
		  GdkFont	 *font,
		  GdkGC		 *gc,
		  gint		  x,
		  gint		  y,
		  const GdkWChar *text,
		  gint		  text_length)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (font != NULL);
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (text != NULL);

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_text_wc (drawable, font, gc, x, y, text, text_length);
}

void
gdk_draw_drawable (GdkDrawable *drawable,
		   GdkGC       *gc,
		   GdkDrawable *src,
		   gint         xsrc,
		   gint         ysrc,
		   gint         xdest,
		   gint         ydest,
		   gint         width,
		   gint         height)
{
  GdkDrawable *composite;
  gint composite_x_offset = 0;
  gint composite_y_offset = 0;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (src != NULL);
  g_return_if_fail (GDK_IS_GC (gc));

  if (width < 0 || height < 0)
    {
      gint real_width;
      gint real_height;
      
      gdk_drawable_get_size (src, &real_width, &real_height);

      if (width < 0)
        width = real_width;
      if (height < 0)
        height = real_height;
    }


  composite =
    GDK_DRAWABLE_GET_CLASS (src)->get_composite_drawable (src,
                                                          xsrc, ysrc,
                                                          width, height,
                                                          &composite_x_offset,
                                                          &composite_y_offset);

  
  GDK_DRAWABLE_GET_CLASS (drawable)->draw_drawable (drawable, gc, composite,
                                                    xsrc - composite_x_offset,
                                                    ysrc - composite_y_offset,
                                                    xdest, ydest,
                                                    width, height);
  
  g_object_unref (G_OBJECT (composite));
}

void
gdk_draw_image (GdkDrawable *drawable,
		GdkGC       *gc,
		GdkImage    *image,
		gint         xsrc,
		gint         ysrc,
		gint         xdest,
		gint         ydest,
		gint         width,
		gint         height)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (image != NULL);
  g_return_if_fail (GDK_IS_GC (gc));

  if (width == -1)
    width = image->width;
  if (height == -1)
    height = image->height;

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_image (drawable, gc, image, xsrc, ysrc,
                                                 xdest, ydest, width, height);
}

void
gdk_draw_points (GdkDrawable *drawable,
		 GdkGC       *gc,
		 GdkPoint    *points,
		 gint         npoints)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail ((points != NULL) && (npoints > 0));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (npoints >= 0);

  if (npoints == 0)
    return;

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_points (drawable, gc, points, npoints);
}

void
gdk_draw_segments (GdkDrawable *drawable,
		   GdkGC       *gc,
		   GdkSegment  *segs,
		   gint         nsegs)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  if (nsegs == 0)
    return;

  g_return_if_fail (segs != NULL);
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (nsegs >= 0);

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_segments (drawable, gc, segs, nsegs);
}

void
gdk_draw_lines (GdkDrawable *drawable,
		GdkGC       *gc,
		GdkPoint    *points,
		gint         npoints)
{

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (points != NULL);
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (npoints >= 0);

  if (npoints == 0)
    return;

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_lines (drawable, gc, points, npoints);
}

void
gdk_draw_glyphs (GdkDrawable      *drawable,
		 GdkGC            *gc,
		 PangoFont        *font,
		 gint              x,
		 gint              y,
		 PangoGlyphString *glyphs)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));


  GDK_DRAWABLE_GET_CLASS (drawable)->draw_glyphs (drawable, gc, font, x, y, glyphs);
}


GdkImage*
gdk_drawable_get_image (GdkDrawable *drawable,
                        gint         x,
                        gint         y,
                        gint         width,
                        gint         height)
{
  GdkDrawable *composite;
  gint composite_x_offset = 0;
  gint composite_y_offset = 0;
  GdkImage *retval;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (x >= 0, NULL);
  g_return_val_if_fail (y >= 0, NULL);

  if (width < 0 || height < 0)
    gdk_drawable_get_size (drawable,
                           width < 0 ? &width : NULL,
                           height < 0 ? &height : NULL);
  
  composite =
    GDK_DRAWABLE_GET_CLASS (drawable)->get_composite_drawable (drawable,
                                                               x, y,
                                                               width, height,
                                                               &composite_x_offset,
                                                               &composite_y_offset); 
  
  retval = GDK_DRAWABLE_GET_CLASS (composite)->get_image (composite,
                                                          x - composite_x_offset,
                                                          y - composite_y_offset,
                                                          width, height);

  g_object_unref (G_OBJECT (composite));

  return retval;
}

static GdkDrawable*
gdk_drawable_real_get_composite_drawable (GdkDrawable *drawable,
                                          gint         x,
                                          gint         y,
                                          gint         width,
                                          gint         height,
                                          gint        *composite_x_offset,
                                          gint        *composite_y_offset)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  *composite_x_offset = 0;
  *composite_y_offset = 0;
  
  return GDK_DRAWABLE (g_object_ref (G_OBJECT (drawable)));
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
 * Return value: a #GdkRegion. This must be freed with gdk_region_destroy()
 *               when you are done.
 **/
GdkRegion *
gdk_drawable_get_clip_region (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_clip_region (drawable);
}

/**
 * gdk_drawable_get_visible_region:
 * @drawable: 
 * 
 * Computes the region of a drawable that is potentially visible.
 * This does not necessarily take into account if the window is
 * obscured by other windows, but no area outside of this region
 * is visible.
 * 
 * Return value: a #GdkRegion. This must be freed with gdk_region_destroy()
 *               when you are done.
 **/
GdkRegion *
gdk_drawable_get_visible_region (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  return GDK_DRAWABLE_GET_CLASS (drawable)->get_visible_region (drawable);
}

static GdkRegion *
gdk_drawable_real_get_visible_region (GdkDrawable *drawable)
{
  GdkRectangle rect;

  rect.x = 0;
  rect.y = 0;

  gdk_drawable_get_size (drawable, &rect.width, &rect.height);

  return gdk_region_rectangle (&rect);
}
