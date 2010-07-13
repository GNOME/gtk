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


static GdkDrawable* gdk_drawable_real_get_composite_drawable (GdkDrawable  *drawable,
							      gint          x,
							      gint          y,
							      gint          width,
							      gint          height,
							      gint         *composite_x_offset,
							      gint         *composite_y_offset);
static cairo_region_t *  gdk_drawable_real_get_visible_region     (GdkDrawable  *drawable);
static void         gdk_drawable_real_draw_drawable          (GdkDrawable  *drawable,
							      GdkGC	   *gc,
							      GdkDrawable  *src,
							      gint          xsrc,
							      gint	    ysrc,
							      gint	    xdest,
							      gint	    ydest,
							      gint	    width,
							      gint	    height);
     

G_DEFINE_ABSTRACT_TYPE (GdkDrawable, gdk_drawable, G_TYPE_OBJECT)

static void
gdk_drawable_class_init (GdkDrawableClass *klass)
{
  klass->get_composite_drawable = gdk_drawable_real_get_composite_drawable;
  /* Default implementation for clip and visible region is the same */
  klass->get_clip_region = gdk_drawable_real_get_visible_region;
  klass->get_visible_region = gdk_drawable_real_get_visible_region;
  klass->draw_drawable = gdk_drawable_real_draw_drawable;
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

/* Drawing
 */

/**
 * gdk_draw_point:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @x: the x coordinate of the point.
 * @y: the y coordinate of the point.
 * 
 * Draws a point, using the foreground color and other attributes of 
 * the #GdkGC.
 **/
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

/**
 * gdk_draw_line:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap). 
 * @gc: a #GdkGC.
 * @x1_: the x coordinate of the start point.
 * @y1_: the y coordinate of the start point.
 * @x2_: the x coordinate of the end point.
 * @y2_: the y coordinate of the end point.
 * 
 * Draws a line, using the foreground color and other attributes of 
 * the #GdkGC.
 **/
void
gdk_draw_line (GdkDrawable *drawable,
	       GdkGC       *gc,
	       gint         x1,
	       gint         y1,
	       gint         x2,
	       gint         y2)
{
  GdkSegment segment;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  segment.x1 = x1;
  segment.y1 = y1;
  segment.x2 = x2;
  segment.y2 = y2;
  GDK_DRAWABLE_GET_CLASS (drawable)->draw_segments (drawable, gc, &segment, 1);
}

/**
 * gdk_draw_rectangle:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @filled: %TRUE if the rectangle should be filled.
 * @x: the x coordinate of the left edge of the rectangle.
 * @y: the y coordinate of the top edge of the rectangle.
 * @width: the width of the rectangle.
 * @height: the height of the rectangle.
 * 
 * Draws a rectangular outline or filled rectangle, using the foreground color
 * and other attributes of the #GdkGC.
 *
 * A rectangle drawn filled is 1 pixel smaller in both dimensions than a 
 * rectangle outlined. Calling 
 * <literal>gdk_draw_rectangle (window, gc, TRUE, 0, 0, 20, 20)</literal> 
 * results in a filled rectangle 20 pixels wide and 20 pixels high. Calling
 * <literal>gdk_draw_rectangle (window, gc, FALSE, 0, 0, 20, 20)</literal> 
 * results in an outlined rectangle with corners at (0, 0), (0, 20), (20, 20),
 * and (20, 0), which makes it 21 pixels wide and 21 pixels high.
 **/
void
gdk_draw_rectangle (GdkDrawable *drawable,
		    GdkGC       *gc,
		    gboolean     filled,
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

/**
 * gdk_draw_arc:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @filled: %TRUE if the arc should be filled, producing a 'pie slice'.
 * @x: the x coordinate of the left edge of the bounding rectangle.
 * @y: the y coordinate of the top edge of the bounding rectangle.
 * @width: the width of the bounding rectangle.
 * @height: the height of the bounding rectangle.
 * @angle1: the start angle of the arc, relative to the 3 o'clock position,
 *     counter-clockwise, in 1/64ths of a degree.
 * @angle2: the end angle of the arc, relative to @angle1, in 1/64ths 
 *     of a degree.
 * 
 * Draws an arc or a filled 'pie slice'. The arc is defined by the bounding
 * rectangle of the entire ellipse, and the start and end angles of the part 
 * of the ellipse to be drawn.
 **/
void
gdk_draw_arc (GdkDrawable *drawable,
	      GdkGC       *gc,
	      gboolean     filled,
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

/**
 * gdk_draw_polygon:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @filled: %TRUE if the polygon should be filled. The polygon is closed
 *     automatically, connecting the last point to the first point if 
 *     necessary.
 * @points: an array of #GdkPoint structures specifying the points making 
 *     up the polygon.
 * @n_points: the number of points.
 * 
 * Draws an outlined or filled polygon.
 **/
void
gdk_draw_polygon (GdkDrawable    *drawable,
		  GdkGC          *gc,
		  gboolean        filled,
		  const GdkPoint *points,
		  gint            n_points)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_polygon (drawable, gc, filled,
                                                   (GdkPoint *) points,
                                                   n_points);
}

/**
 * gdk_draw_drawable:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC sharing the drawable's visual and colormap
 * @src: the source #GdkDrawable, which may be the same as @drawable
 * @xsrc: X position in @src of rectangle to draw
 * @ysrc: Y position in @src of rectangle to draw
 * @xdest: X position in @drawable where the rectangle should be drawn
 * @ydest: Y position in @drawable where the rectangle should be drawn
 * @width: width of rectangle to draw, or -1 for entire @src width
 * @height: height of rectangle to draw, or -1 for entire @src height
 *
 * Copies the @width x @height region of @src at coordinates (@xsrc,
 * @ysrc) to coordinates (@xdest, @ydest) in @drawable.
 * @width and/or @height may be given as -1, in which case the entire
 * @src drawable will be copied.
 *
 * Most fields in @gc are not used for this operation, but notably the
 * clip mask or clip region will be honored.
 *
 * The source and destination drawables must have the same visual and
 * colormap, or errors will result. (On X11, failure to match
 * visual/colormap results in a BadMatch error from the X server.)
 * A common cause of this problem is an attempt to draw a bitmap to
 * a color drawable. The way to draw a bitmap is to set the bitmap as 
 * the stipple on the #GdkGC, set the fill mode to %GDK_STIPPLED, and 
 * then draw the rectangle.
 **/
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
  g_return_if_fail (GDK_IS_DRAWABLE (src));
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

  /* TODO: For non-native windows this may copy stuff from other overlapping
     windows. We should clip that and (for windows with bg != None) clear that
     area in the destination instead. */

  if (GDK_DRAWABLE_GET_CLASS (drawable)->draw_drawable_with_src)
    GDK_DRAWABLE_GET_CLASS (drawable)->draw_drawable_with_src (drawable, gc,
							       composite,
							       xsrc - composite_x_offset,
							       ysrc - composite_y_offset,
							       xdest, ydest,
							       width, height,
							       src);
  else /* backwards compat for old out-of-tree implementations of GdkDrawable (are there any?) */
    GDK_DRAWABLE_GET_CLASS (drawable)->draw_drawable (drawable, gc,
						      composite,
						      xsrc - composite_x_offset,
						      ysrc - composite_y_offset,
						      xdest, ydest,
						      width, height);

  g_object_unref (composite);
}

/**
 * gdk_draw_points:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @points: an array of #GdkPoint structures.
 * @n_points: the number of points to be drawn.
 * 
 * Draws a number of points, using the foreground color and other 
 * attributes of the #GdkGC.
 **/
void
gdk_draw_points (GdkDrawable    *drawable,
		 GdkGC          *gc,
		 const GdkPoint *points,
		 gint            n_points)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail ((points != NULL) && (n_points > 0));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (n_points >= 0);

  if (n_points == 0)
    return;

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_points (drawable, gc,
                                                  (GdkPoint *) points, n_points);
}

/**
 * gdk_draw_segments:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @segs: an array of #GdkSegment structures specifying the start and 
 *   end points of the lines to be drawn.
 * @n_segs: the number of line segments to draw, i.e. the size of the 
 *   @segs array.
 * 
 * Draws a number of unconnected lines.
 **/
void
gdk_draw_segments (GdkDrawable      *drawable,
		   GdkGC            *gc,
		   const GdkSegment *segs,
		   gint              n_segs)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  if (n_segs == 0)
    return;

  g_return_if_fail (segs != NULL);
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (n_segs >= 0);

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_segments (drawable, gc,
                                                    (GdkSegment *) segs, n_segs);
}

/**
 * gdk_draw_lines:
 * @drawable: a #GdkDrawable (a #GdkWindow or a #GdkPixmap).
 * @gc: a #GdkGC.
 * @points: an array of #GdkPoint structures specifying the endpoints of the
 * @n_points: the size of the @points array.
 * 
 * Draws a series of lines connecting the given points.
 * The way in which joins between lines are draw is determined by the
 * #GdkCapStyle value in the #GdkGC. This can be set with
 * gdk_gc_set_line_attributes().
 **/
void
gdk_draw_lines (GdkDrawable    *drawable,
		GdkGC          *gc,
		const GdkPoint *points,
		gint            n_points)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (points != NULL);
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (n_points >= 0);

  if (n_points == 0)
    return;

  GDK_DRAWABLE_GET_CLASS (drawable)->draw_lines (drawable, gc,
                                                 (GdkPoint *) points, n_points);
}

static void
real_draw_glyphs (GdkDrawable       *drawable,
		  GdkGC	            *gc,
		  const PangoMatrix *matrix,
		  PangoFont         *font,
		  gdouble            x,
		  gdouble            y,
		  PangoGlyphString  *glyphs)
{
  cairo_t *cr;

  cr = gdk_cairo_create (drawable);
  _gdk_gc_update_context (gc, cr, NULL, NULL, TRUE, drawable);

  if (matrix)
    {
      cairo_matrix_t cairo_matrix;

      cairo_matrix.xx = matrix->xx;
      cairo_matrix.yx = matrix->yx;
      cairo_matrix.xy = matrix->xy;
      cairo_matrix.yy = matrix->yy;
      cairo_matrix.x0 = matrix->x0;
      cairo_matrix.y0 = matrix->y0;
      
      cairo_set_matrix (cr, &cairo_matrix);
    }

  cairo_move_to (cr, x, y);
  pango_cairo_show_glyph_string (cr, font, glyphs);

  cairo_destroy (cr);
}

/**
 * gdk_draw_glyphs:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC
 * @font: font to be used
 * @x: X coordinate of baseline origin
 * @y: Y coordinate of baseline origin
 * @glyphs: the glyph string to draw
 *
 * This is a low-level function; 99% of text rendering should be done
 * using gdk_draw_layout() instead.
 *
 * A glyph is a single image in a font. This function draws a sequence of
 * glyphs.  To obtain a sequence of glyphs you have to understand a
 * lot about internationalized text handling, which you don't want to
 * understand; thus, use gdk_draw_layout() instead of this function,
 * gdk_draw_layout() handles the details.
 * 
 **/
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
  
  real_draw_glyphs (drawable, gc, NULL, font,
		    x, y, glyphs);
}

/**
 * gdk_draw_glyphs_transformed:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC
 * @matrix: (allow-none): a #PangoMatrix, or %NULL to use an identity transformation
 * @font: the font in which to draw the string
 * @x:       the x position of the start of the string (in Pango
 *           units in user space coordinates)
 * @y:       the y position of the baseline (in Pango units
 *           in user space coordinates)
 * @glyphs:  the glyph string to draw
 * 
 * Renders a #PangoGlyphString onto a drawable, possibly
 * transforming the layed-out coordinates through a transformation
 * matrix. Note that the transformation matrix for @font is not
 * changed, so to produce correct rendering results, the @font
 * must have been loaded using a #PangoContext with an identical
 * transformation matrix to that passed in to this function.
 *
 * See also gdk_draw_glyphs(), gdk_draw_layout().
 *
 * Since: 2.6
 **/
void
gdk_draw_glyphs_transformed (GdkDrawable       *drawable,
			     GdkGC	       *gc,
			     const PangoMatrix *matrix,
			     PangoFont         *font,
			     gint               x,
			     gint               y,
			     PangoGlyphString  *glyphs)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));

  real_draw_glyphs (drawable, gc, matrix, font,
		    x / PANGO_SCALE, y / PANGO_SCALE, glyphs);
}

/**
 * gdk_draw_trapezoids:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC
 * @trapezoids: an array of #GdkTrapezoid structures
 * @n_trapezoids: the number of trapezoids to draw
 * 
 * Draws a set of anti-aliased trapezoids. The trapezoids are
 * combined using saturation addition, then drawn over the background
 * as a set. This is low level functionality used internally to implement
 * rotated underlines and backgrouds when rendering a PangoLayout and is
 * likely not useful for applications.
 *
 * Since: 2.6
 **/
void
gdk_draw_trapezoids (GdkDrawable        *drawable,
		     GdkGC	        *gc,
		     const GdkTrapezoid *trapezoids,
		     gint                n_trapezoids)
{
  cairo_t *cr;
  int i;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (n_trapezoids == 0 || trapezoids != NULL);

  cr = gdk_cairo_create (drawable);
  _gdk_gc_update_context (gc, cr, NULL, NULL, TRUE, drawable);
  
  for (i = 0; i < n_trapezoids; i++)
    {
      cairo_move_to (cr, trapezoids[i].x11, trapezoids[i].y1);
      cairo_line_to (cr, trapezoids[i].x21, trapezoids[i].y1);
      cairo_line_to (cr, trapezoids[i].x22, trapezoids[i].y2);
      cairo_line_to (cr, trapezoids[i].x12, trapezoids[i].y2);
      cairo_close_path (cr);
    }

  cairo_fill (cr);

  cairo_destroy (cr);
}

/**
 * gdk_drawable_copy_to_image:
 * @drawable: a #GdkDrawable
 * @image: (allow-none): a #GdkDrawable, or %NULL if a new @image should be created.
 * @src_x: x coordinate on @drawable
 * @src_y: y coordinate on @drawable
 * @dest_x: x coordinate within @image. Must be 0 if @image is %NULL
 * @dest_y: y coordinate within @image. Must be 0 if @image is %NULL
 * @width: width of region to get
 * @height: height or region to get
 *
 * Copies a portion of @drawable into the client side image structure
 * @image. If @image is %NULL, creates a new image of size @width x @height
 * and copies into that. See gdk_drawable_get_image() for further details.
 * 
 * Return value: @image, or a new a #GdkImage containing the contents
 *               of @drawable
 * 
 * Since: 2.4
 **/
GdkImage*
gdk_drawable_copy_to_image (GdkDrawable *drawable,
			    GdkImage    *image,
			    gint         src_x,
			    gint         src_y,
			    gint         dest_x,
			    gint         dest_y,
			    gint         width,
			    gint         height)
{
  GdkDrawable *composite;
  gint composite_x_offset = 0;
  gint composite_y_offset = 0;
  GdkImage *retval;
  GdkColormap *cmap;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (src_x >= 0, NULL);
  g_return_val_if_fail (src_y >= 0, NULL);

  /* FIXME? Note race condition since we get the size then
   * get the image, and the size may have changed.
   */
  
  if (width < 0 || height < 0)
    gdk_drawable_get_size (drawable,
                           width < 0 ? &width : NULL,
                           height < 0 ? &height : NULL);
  
  composite =
    GDK_DRAWABLE_GET_CLASS (drawable)->get_composite_drawable (drawable,
                                                               src_x, src_y,
                                                               width, height,
                                                               &composite_x_offset,
                                                               &composite_y_offset); 
  
  retval = GDK_DRAWABLE_GET_CLASS (composite)->_copy_to_image (composite,
							       image,
							       src_x - composite_x_offset,
							       src_y - composite_y_offset,
							       dest_x, dest_y,
							       width, height);

  g_object_unref (composite);

  if (!image && retval)
    {
      cmap = gdk_drawable_get_colormap (drawable);
      
      if (cmap)
	gdk_image_set_colormap (retval, cmap);
    }
  
  return retval;
}

static GdkDrawable *
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
  
  return g_object_ref (drawable);
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

/* Implementation of the old vfunc in terms of the new one
   in case someone calls it directly (which they shouldn't!) */
static void
gdk_drawable_real_draw_drawable (GdkDrawable  *drawable,
				 GdkGC	       *gc,
				 GdkDrawable  *src,
				 gint		xsrc,
				 gint		ysrc,
				 gint		xdest,
				 gint		ydest,
				 gint		width,
				 gint		height)
{
  GDK_DRAWABLE_GET_CLASS (drawable)->draw_drawable_with_src (drawable,
							     gc,
							     src,
							     xsrc,
							     ysrc,
							     xdest,
							     ydest,
							     width,
							     height,
							     src);
}

/************************************************************************/

/**
 * _gdk_drawable_get_scratch_gc:
 * @drawable: A #GdkDrawable
 * @graphics_exposures: Whether the returned #GdkGC should generate graphics exposures 
 * 
 * Returns a #GdkGC suitable for drawing on @drawable. The #GdkGC has
 * the standard values for @drawable, except for the graphics_exposures
 * field which is determined by the @graphics_exposures parameter.
 *
 * The foreground color of the returned #GdkGC is undefined. The #GdkGC
 * must not be altered in any way, except to change its foreground color.
 * 
 * Return value: A #GdkGC suitable for drawing on @drawable
 * 
 * Since: 2.4
 **/
GdkGC *
_gdk_drawable_get_scratch_gc (GdkDrawable *drawable,
			      gboolean     graphics_exposures)
{
  GdkScreen *screen;
  gint depth;

  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  screen = gdk_drawable_get_screen (drawable);

  g_return_val_if_fail (!screen->closed, NULL);

  depth = gdk_drawable_get_depth (drawable) - 1;

  if (graphics_exposures)
    {
      if (!screen->exposure_gcs[depth])
	{
	  GdkGCValues values;
	  GdkGCValuesMask mask;

	  values.graphics_exposures = TRUE;
	  mask = GDK_GC_EXPOSURES;  

	  screen->exposure_gcs[depth] =
	    gdk_gc_new_with_values (drawable, &values, mask);
	}

      return screen->exposure_gcs[depth];
    }
  else
    {
      if (!screen->normal_gcs[depth])
	{
	  screen->normal_gcs[depth] =
	    gdk_gc_new (drawable);
	}

      return screen->normal_gcs[depth];
    }
}

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
