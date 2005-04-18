/* GdkPixbuf library - Rendering functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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

#include <config.h>
#include <gdk/gdk.h>
#include "gdk-pixbuf-private.h"
#include "gdkpixbuf.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkalias.h"



/**
 * gdk_pixbuf_render_threshold_alpha:
 * @pixbuf: A pixbuf.
 * @bitmap: Bitmap where the bilevel mask will be painted to.
 * @src_x: Source X coordinate.
 * @src_y: source Y coordinate.
 * @dest_x: Destination X coordinate.
 * @dest_y: Destination Y coordinate.
 * @width: Width of region to threshold, or -1 to use pixbuf width
 * @height: Height of region to threshold, or -1 to use pixbuf height
 * @alpha_threshold: Opacity values below this will be painted as zero; all
 * other values will be painted as one.
 *
 * Takes the opacity values in a rectangular portion of a pixbuf and thresholds
 * them to produce a bi-level alpha mask that can be used as a clipping mask for
 * a drawable.
 *
 **/
void
gdk_pixbuf_render_threshold_alpha (GdkPixbuf *pixbuf,
				   GdkBitmap *bitmap,
				   int src_x,  int src_y,
				   int dest_x, int dest_y,
				   int width,  int height,
				   int alpha_threshold)
{
  GdkGC *gc;
  GdkColor color;
  int x, y;
  guchar *p;
  int start, start_status;
  int status;

  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  g_return_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB);
  g_return_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4);
  g_return_if_fail (pixbuf->bits_per_sample == 8);

  if (width == -1) 
    width = pixbuf->width;
  if (height == -1)
    height = pixbuf->height;

  g_return_if_fail (bitmap != NULL);
  g_return_if_fail (width >= 0 && height >= 0);
  g_return_if_fail (src_x >= 0 && src_x + width <= pixbuf->width);
  g_return_if_fail (src_y >= 0 && src_y + height <= pixbuf->height);

  g_return_if_fail (alpha_threshold >= 0 && alpha_threshold <= 255);

  if (width == 0 || height == 0)
    return;

  gc = _gdk_drawable_get_scratch_gc (bitmap, FALSE);

  if (!pixbuf->has_alpha)
    {
      color.pixel = (alpha_threshold == 255) ? 0 : 1;
      gdk_gc_set_foreground (gc, &color);
      gdk_draw_rectangle (bitmap, gc, TRUE, dest_x, dest_y, width, height);
      return;
    }

  color.pixel = 0;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_rectangle (bitmap, gc, TRUE, dest_x, dest_y, width, height);

  color.pixel = 1;
  gdk_gc_set_foreground (gc, &color);

  for (y = 0; y < height; y++)
    {
      p = (pixbuf->pixels + (y + src_y) * pixbuf->rowstride + src_x * pixbuf->n_channels
	   + pixbuf->n_channels - 1);
	    
      start = 0;
      start_status = *p < alpha_threshold;
	    
      for (x = 0; x < width; x++)
	{
	  status = *p < alpha_threshold;
	  
	  if (status != start_status)
	    {
	      if (!start_status)
		gdk_draw_line (bitmap, gc,
			       start + dest_x, y + dest_y,
			       x - 1 + dest_x, y + dest_y);
	      
	      start = x;
	      start_status = status;
	    }
	  
	  p += pixbuf->n_channels;
	}
      
      if (!start_status)
	gdk_draw_line (bitmap, gc,
		       start + dest_x, y + dest_y,
		       x - 1 + dest_x, y + dest_y);
    }
}



/**
 * gdk_pixbuf_render_to_drawable:
 * @pixbuf: A pixbuf.
 * @drawable: Destination drawable.
 * @gc: GC used for rendering.
 * @src_x: Source X coordinate within pixbuf.
 * @src_y: Source Y coordinate within pixbuf.
 * @dest_x: Destination X coordinate within drawable.
 * @dest_y: Destination Y coordinate within drawable.
 * @width: Width of region to render, in pixels, or -1 to use pixbuf width
 * @height: Height of region to render, in pixels, or -1 to use pixbuf height
 * @dither: Dithering mode for GdkRGB.
 * @x_dither: X offset for dither.
 * @y_dither: Y offset for dither.
 *
 * Renders a rectangular portion of a pixbuf to a drawable while using the
 * specified GC.  This is done using GdkRGB, so the specified drawable must have
 * the GdkRGB visual and colormap.  Note that this function will ignore the
 * opacity information for images with an alpha channel; the GC must already
 * have the clipping mask set if you want transparent regions to show through.
 *
 * For an explanation of dither offsets, see the GdkRGB documentation.  In
 * brief, the dither offset is important when re-rendering partial regions of an
 * image to a rendered version of the full image, or for when the offsets to a
 * base position change, as in scrolling.  The dither matrix has to be shifted
 * for consistent visual results.  If you do not have any of these cases, the
 * dither offsets can be both zero.
 *
 * Deprecated: This function is obsolete. Use gdk_draw_pixbuf() instead.
 **/
void
gdk_pixbuf_render_to_drawable (GdkPixbuf   *pixbuf,
			       GdkDrawable *drawable,
			       GdkGC       *gc,
			       int src_x,    int src_y,
			       int dest_x,   int dest_y,
			       int width,    int height,
			       GdkRgbDither dither,
			       int x_dither, int y_dither)
{
  gdk_draw_pixbuf (drawable, gc, pixbuf,
		   src_x, src_y, dest_x, dest_y, width, height,
		   dither, x_dither, y_dither);
}



/**
 * gdk_pixbuf_render_to_drawable_alpha:
 * @pixbuf: A pixbuf.
 * @drawable: Destination drawable.
 * @src_x: Source X coordinate within pixbuf.
 * @src_y: Source Y coordinates within pixbuf.
 * @dest_x: Destination X coordinate within drawable.
 * @dest_y: Destination Y coordinate within drawable.
 * @width: Width of region to render, in pixels, or -1 to use pixbuf width.
 * @height: Height of region to render, in pixels, or -1 to use pixbuf height.
 * @alpha_mode: Ignored. Present for backwards compatibility.
 * @alpha_threshold: Ignored. Present for backwards compatibility
 * @dither: Dithering mode for GdkRGB.
 * @x_dither: X offset for dither.
 * @y_dither: Y offset for dither.
 *
 * Renders a rectangular portion of a pixbuf to a drawable.  The destination
 * drawable must have a colormap. All windows have a colormap, however, pixmaps
 * only have colormap by default if they were created with a non-NULL window argument.
 * Otherwise a colormap must be set on them with gdk_drawable_set_colormap.
 *
 * On older X servers, rendering pixbufs with an alpha channel involves round trips
 * to the X server, and may be somewhat slow.
 *
 * Deprecated: This function is obsolete. Use gdk_draw_pixbuf() instead.
 **/
void
gdk_pixbuf_render_to_drawable_alpha (GdkPixbuf   *pixbuf,
				     GdkDrawable *drawable,
				     int src_x,    int src_y,
				     int dest_x,   int dest_y,
				     int width,    int height,
				     GdkPixbufAlphaMode alpha_mode,
				     int                alpha_threshold,
				     GdkRgbDither       dither,
				     int x_dither, int y_dither)
{
  gdk_draw_pixbuf (drawable, NULL, pixbuf,
		   src_x, src_y, dest_x, dest_y, width, height,
		   dither, x_dither, y_dither);
}

/**
 * gdk_pixbuf_render_pixmap_and_mask:
 * @pixbuf: A pixbuf.
 * @pixmap_return: Location to store a pointer to the created pixmap,
 *   or %NULL if the pixmap is not needed.
 * @mask_return: Location to store a pointer to the created mask,
 *   or %NULL if the mask is not needed.
 * @alpha_threshold: Threshold value for opacity values.
 *
 * Creates a pixmap and a mask bitmap which are returned in the @pixmap_return
 * and @mask_return arguments, respectively, and renders a pixbuf and its
 * corresponding thresholded alpha mask to them.  This is merely a convenience
 * function; applications that need to render pixbufs with dither offsets or to
 * given drawables should use gdk_draw_pixbuf() and gdk_pixbuf_render_threshold_alpha().
 *
 * The pixmap that is created is created for the colormap returned
 * by gdk_rgb_get_colormap(). You normally will want to instead use
 * the actual colormap for a widget, and use
 * gdk_pixbuf_render_pixmap_and_mask_for_colormap.
 *
 * If the pixbuf does not have an alpha channel, then *@mask_return will be set
 * to NULL.
 **/
void
gdk_pixbuf_render_pixmap_and_mask (GdkPixbuf  *pixbuf,
				   GdkPixmap **pixmap_return,
				   GdkBitmap **mask_return,
				   int         alpha_threshold)
{
  gdk_pixbuf_render_pixmap_and_mask_for_colormap (pixbuf,
						  gdk_rgb_get_colormap (),
						  pixmap_return, mask_return,
						  alpha_threshold);
}

/**
 * gdk_pixbuf_render_pixmap_and_mask_for_colormap:
 * @pixbuf: A pixbuf.
 * @colormap: A #GdkColormap
 * @pixmap_return: Location to store a pointer to the created pixmap,
 *   or %NULL if the pixmap is not needed.
 * @mask_return: Location to store a pointer to the created mask,
 *   or %NULL if the mask is not needed.
 * @alpha_threshold: Threshold value for opacity values.
 *
 * Creates a pixmap and a mask bitmap which are returned in the @pixmap_return
 * and @mask_return arguments, respectively, and renders a pixbuf and its
 * corresponding tresholded alpha mask to them.  This is merely a convenience
 * function; applications that need to render pixbufs with dither offsets or to
 * given drawables should use gdk_draw_pixbuf(), and gdk_pixbuf_render_threshold_alpha().
 *
 * The pixmap that is created uses the #GdkColormap specified by @colormap.
 * This colormap must match the colormap of the window where the pixmap
 * will eventually be used or an error will result.
 *
 * If the pixbuf does not have an alpha channel, then *@mask_return will be set
 * to NULL.
 **/
void
gdk_pixbuf_render_pixmap_and_mask_for_colormap (GdkPixbuf   *pixbuf,
						GdkColormap *colormap,
						GdkPixmap  **pixmap_return,
						GdkBitmap  **mask_return,
						int          alpha_threshold)
{
  GdkScreen *screen;

  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  screen = gdk_colormap_get_screen (colormap);
  
  if (pixmap_return)
    {
      GdkGC *gc;
      *pixmap_return = gdk_pixmap_new (gdk_screen_get_root_window (screen),
				       gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
				       gdk_colormap_get_visual (colormap)->depth);

      gdk_drawable_set_colormap (GDK_DRAWABLE (*pixmap_return), colormap);
      gc = _gdk_drawable_get_scratch_gc (*pixmap_return, FALSE);
      gdk_draw_pixbuf (*pixmap_return, gc, pixbuf, 
		       0, 0, 0, 0,
		       gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
		       GDK_RGB_DITHER_NORMAL,
		       0, 0);
    }
  
  if (mask_return)
    {
      if (gdk_pixbuf_get_has_alpha (pixbuf))
	{
	  *mask_return = gdk_pixmap_new (gdk_screen_get_root_window (screen),
					 gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf), 1);

	  gdk_pixbuf_render_threshold_alpha (pixbuf, *mask_return,
					     0, 0, 0, 0,
					     gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
					     alpha_threshold);
	}
      else
	*mask_return = NULL;
    }
}

/**
 * gdk_pixbuf_set_as_cairo_source:
 * @pixbuf: a #GdkPixbuf
 * @cr: a #Cairo context
 * 
 * Sets the given pixbuf as the source pattern for the Cairo context.
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixbuf is at the current point.
 **/
void
gdk_pixbuf_set_as_cairo_source (GdkPixbuf *pixbuf,
				cairo_t   *cr)
{
  gint width = gdk_pixbuf_get_width (pixbuf);
  gint height = gdk_pixbuf_get_height (pixbuf);
  guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
  int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  guchar *cairo_pixels;
  cairo_format_t format;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  static const cairo_user_data_key_t key;
  cairo_matrix_t *matrix;
  double x, y;
  int j;

  if (n_channels == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  cairo_pixels = g_malloc (4 * width * height);
  surface = cairo_image_surface_create_for_data ((unsigned char *)cairo_pixels,
						 format,
						 width, height, 4 * width);
  cairo_surface_set_user_data (surface, &key,
			       cairo_pixels, (cairo_destroy_func_t)g_free);

  for (j = height; j; j--)
    {
      guchar *p = gdk_pixels;
      guchar *q = cairo_pixels;

      if (n_channels == 3)
	{
	  guchar *end = p + 3 * width;
	  
	  while (p < end)
	    {
#if G_BYTE_ORDER == GDK_LSB_FIRST
	      q[0] = p[2];
	      q[1] = p[1];
	      q[2] = p[2];
#else	  
	      q[0] = p[0];
	      q[1] = p[1];
	      q[2] = p[2];
#endif
	      p += 3;
	      q += 4;
	    }
	}
      else
	{
	  guchar *end = p + 4 * width;
	  guint t1,t2,t3;
	    
#define MULT(d,c,a,t) G_STMT_START { t = c * a; d = ((t >> 8) + t) >> 8; } G_STMT_END

	  while (p < end)
	    {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	      MULT(q[0], p[2], p[3], t1);
	      MULT(q[1], p[1], p[3], t2);
	      MULT(q[2], p[0], p[3], t3);
	      q[3] = p[3];
#else	  
	      q[0] = p[3];
	      MULT(q[1], p[0], p[3], t1);
	      MULT(q[2], p[1], p[3], t2);
	      MULT(q[3], p[2], p[3], t3);
#endif
	      
	      p += 4;
	      q += 4;
	    }
	  
#undef MULT
	}

      gdk_pixels += gdk_rowstride;
      cairo_pixels += 4 * width;
    }

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_surface_destroy (surface);
  
  cairo_current_point (cr, &x, &y);
  matrix = cairo_matrix_create ();
  cairo_matrix_translate (matrix, -x, -y);
  cairo_pattern_set_matrix (pattern, matrix);
  cairo_matrix_destroy (matrix);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
}

#define __GDK_PIXBUF_RENDER_C__
#include "gdkaliasdef.c"

