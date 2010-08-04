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

#include "config.h"
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdkpixbuf.h"
#include "gdkscreen.h"
#include "gdkinternals.h"


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
                                   int        src_x,
                                   int        src_y,
                                   int        dest_x,
                                   int        dest_y,
                                   int        width,
                                   int        height,
                                   int        alpha_threshold)
{
  cairo_t *cr;
  int x, y;
  guchar *p;
  int start, start_status;
  int status;

  g_return_if_fail (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
  g_return_if_fail (gdk_pixbuf_get_n_channels (pixbuf) == 3 ||
                        gdk_pixbuf_get_n_channels (pixbuf) == 4);
  g_return_if_fail (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

  if (width == -1) 
    width = gdk_pixbuf_get_width (pixbuf);
  if (height == -1)
    height = gdk_pixbuf_get_height (pixbuf);

  g_return_if_fail (bitmap != NULL);
  g_return_if_fail (width >= 0 && height >= 0);
  g_return_if_fail (src_x >= 0 && src_x + width <= gdk_pixbuf_get_width (pixbuf));
  g_return_if_fail (src_y >= 0 && src_y + height <= gdk_pixbuf_get_height (pixbuf));

  g_return_if_fail (alpha_threshold >= 0 && alpha_threshold <= 255);

  if (width == 0 || height == 0)
    return;

  cr = gdk_cairo_create (bitmap);
  cairo_rectangle (cr, dest_x, dest_y, width, height);
  cairo_clip (cr);

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      cairo_set_source_rgba (cr, 0, 0, 0, alpha_threshold == 255 ? 0 : 1);
      cairo_paint (cr);
      cairo_destroy (cr);
      return;
    }

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  if (alpha_threshold == 128)
    {
      gdk_cairo_set_source_pixbuf (cr, pixbuf, src_x - dest_x, src_y - dest_y);
      cairo_paint (cr);
      cairo_destroy (cr);
      return;
    }

  cairo_set_source_rgb (cr, 0, 0, 0);

  for (y = 0; y < height; y++)
    {
      p = (gdk_pixbuf_get_pixels (pixbuf) + (y + src_y) * gdk_pixbuf_get_rowstride (pixbuf) + src_x * gdk_pixbuf_get_n_channels (pixbuf)
	   + gdk_pixbuf_get_n_channels (pixbuf) - 1);
	    
      start = 0;
      start_status = *p < alpha_threshold;
	    
      for (x = 0; x < width; x++)
	{
	  status = *p < alpha_threshold;
	  
	  if (status != start_status)
	    {
	      if (!start_status)
                cairo_rectangle (cr, 
                                 start + dest_x, y + dest_y,
			         x + dest_x, y + dest_y + 1);
	      
	      start = x;
	      start_status = status;
	    }
	  
	  p += gdk_pixbuf_get_n_channels (pixbuf);
	}
      
      if (!start_status)
        cairo_rectangle (cr, 
                         start + dest_x, y + dest_y,
                         x + dest_x, y + dest_y + 1);
    }

  cairo_fill (cr);
  cairo_destroy (cr);
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
 * given drawables should use Cairo and gdk_pixbuf_render_threshold_alpha().
 *
 * The pixmap that is created is created for the colormap returned
 * by gdk_colormap_get_system(). You normally will want to instead use
 * the actual colormap for a widget, and use
 * gdk_pixbuf_render_pixmap_and_mask_for_colormap().
 *
 * If the pixbuf does not have an alpha channel, then *@mask_return will be set
 * to %NULL.
 **/
void
gdk_pixbuf_render_pixmap_and_mask (GdkPixbuf  *pixbuf,
				   GdkPixmap **pixmap_return,
				   GdkBitmap **mask_return,
				   int         alpha_threshold)
{
  gdk_pixbuf_render_pixmap_and_mask_for_colormap (pixbuf,
						  gdk_colormap_get_system (),
						  pixmap_return, mask_return,
						  alpha_threshold);
}

static GdkPixbuf *
remove_alpha_channel (GdkPixbuf *orig)
{
  GdkPixbuf *pixbuf;

  unsigned int x, y, width, height, stride;
  unsigned char *data;

  if (!gdk_pixbuf_get_has_alpha (orig))
    return g_object_ref (orig);

  pixbuf = gdk_pixbuf_copy (orig);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  stride = gdk_pixbuf_get_rowstride (pixbuf);
  data = gdk_pixbuf_get_pixels (pixbuf);

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          data[x * 4 + 3] = 0xFF;
        }

      data += stride;
    }

  return pixbuf;
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
 * given drawables should use Cairo and gdk_pixbuf_render_threshold_alpha().
 *
 * The pixmap that is created uses the #GdkColormap specified by @colormap.
 * This colormap must match the colormap of the window where the pixmap
 * will eventually be used or an error will result.
 *
 * If the pixbuf does not have an alpha channel, then *@mask_return will be set
 * to %NULL.
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
      GdkPixbuf *tmp_pixbuf;
      cairo_t *cr;

      *pixmap_return = gdk_pixmap_new (gdk_screen_get_root_window (screen),
				       gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
				       gdk_colormap_get_visual (colormap)->depth);

      gdk_drawable_set_colormap (GDK_DRAWABLE (*pixmap_return), colormap);

      /* If the pixbuf has an alpha channel, using gdk_cairo_set_source_pixbuf()
       * would give
       * random pixel values in the area that are within the mask, but semi-
       * transparent. So we treat the pixbuf like a pixbuf without alpha channel;
       * see bug #487865.
       */
      tmp_pixbuf = remove_alpha_channel (pixbuf);

      cr = gdk_cairo_create (*pixmap_return);
      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);

      cairo_destroy (cr);
      g_object_unref (tmp_pixbuf);
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
