/* GdkPixbuf library - convert X drawable information to RGB
 *
 * Copyright (C) 1999 Michael Zucchi
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Cody Russell <bratsche@dfw.net>
 * 	    Federico Mena-Quintero <federico@gimp.org>
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcolor.h"
#include "gdkwindow.h"
#include "gdkpixbuf.h"
#include "gdkinternals.h"


/* Exported functions */

/**
 * gdk_pixbuf_get_from_window:
 * @dest: (allow-none): Destination pixbuf, or %NULL if a new pixbuf should be created.
 * @src: Source drawable.
 * @src_x: Source X coordinate within drawable.
 * @src_y: Source Y coordinate within drawable.
 * @dest_x: Destination X coordinate in pixbuf, or 0 if @dest is NULL.
 * @dest_y: Destination Y coordinate in pixbuf, or 0 if @dest is NULL.
 * @width: Width in pixels of region to get.
 * @height: Height in pixels of region to get.
 *
 * Transfers image data from a #GdkDrawable and converts it to an RGB(A)
 * representation inside a #GdkPixbuf. In other words, copies
 * image data from a server-side drawable to a client-side RGB(A) buffer.
 * This allows you to efficiently read individual pixels on the client side.
 * 
 * If the specified destination pixbuf @dest is %NULL, then this
 * function will create an RGB pixbuf with 8 bits per channel and no
 * alpha, with the same size specified by the @width and @height
 * arguments.  In this case, the @dest_x and @dest_y arguments must be
 * specified as 0.  If the specified destination pixbuf is not %NULL
 * and it contains alpha information, then the filled pixels will be
 * set to full opacity (alpha = 255).
 *
 * If the specified drawable is a window, and the window is off the
 * screen, then there is no image data in the obscured/offscreen
 * regions to be placed in the pixbuf. The contents of portions of the
 * pixbuf corresponding to the offscreen region are undefined.
 *
 * If the window you're obtaining data from is partially obscured by
 * other windows, then the contents of the pixbuf areas corresponding
 * to the obscured regions are undefined.
 * 
 * If the target drawable is not mapped (typically because it's
 * iconified/minimized or not on the current workspace), then %NULL
 * will be returned.
 *
 * If memory can't be allocated for the return value, %NULL will be returned
 * instead.
 *
 * (In short, there are several ways this function can fail, and if it fails
 *  it returns %NULL; so check the return value.)
 *
 * Return value: The same pixbuf as @dest if it was non-%NULL, or a newly-created
 * pixbuf with a reference count of 1 if no destination pixbuf was specified, or %NULL on error
 **/
GdkPixbuf *
gdk_pixbuf_get_from_window (GdkPixbuf   *dest,
                            GdkWindow   *src,
                            int src_x,  int src_y,
                            int dest_x, int dest_y,
                            int width,  int height)
{
  cairo_surface_t *surface;
  
  g_return_val_if_fail (GDK_IS_WINDOW (src), NULL);
  g_return_val_if_fail (gdk_window_is_viewable (src), NULL);

  if (!dest)
    g_return_val_if_fail (dest_x == 0 && dest_y == 0, NULL);
  else
    {
      g_return_val_if_fail (gdk_pixbuf_get_colorspace (dest) == GDK_COLORSPACE_RGB, NULL);
      g_return_val_if_fail (gdk_pixbuf_get_n_channels (dest) == 3 ||
                            gdk_pixbuf_get_n_channels (dest) == 4, NULL);
      g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (dest) == 8, NULL);
    }

  surface = _gdk_drawable_ref_cairo_surface (src);
  dest = gdk_pixbuf_get_from_surface (dest,
                                      surface,
                                      src_x, src_y,
                                      dest_x, dest_y,
                                      width, height);
  cairo_surface_destroy (surface);

  return dest;
}
        
static cairo_format_t
gdk_cairo_format_for_content (cairo_content_t content)
{
  switch (content)
    {
    case CAIRO_CONTENT_COLOR:
      return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
      return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
    default:
      return CAIRO_FORMAT_ARGB32;
    }
}

static cairo_surface_t *
gdk_cairo_surface_coerce_to_image (cairo_surface_t *surface,
                                   cairo_content_t content,
                                   int width,
                                   int height)
{
  cairo_surface_t *copy;
  cairo_t *cr;

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE &&
      cairo_surface_get_content (surface) == content &&
      cairo_image_surface_get_width (surface) >= width &&
      cairo_image_surface_get_height (surface) >= height)
    return cairo_surface_reference (surface);

  copy = cairo_image_surface_create (gdk_cairo_format_for_content (content),
                                     width,
                                     height);

  cr = cairo_create (copy);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return copy;
}

static void
convert_alpha (guchar  *dest_data,
               int      dest_stride,
               guchar  *src_data,
               int      src_stride,
               int      src_x,
               int      src_y,
               int      dest_x,
               int      dest_y,
               int      width,
               int      height)
{
  int x, y;

  dest_data += dest_stride * dest_y + dest_x * 4;
  src_data += src_stride * src_y + src_x * 4;

  for (y = 0; y < height; y++) {
    guint32 *src = (guint32 *) src_data;

    for (x = 0; x < width; x++) {
      guint alpha = src[x] >> 24;

      if (alpha == 0)
        {
          dest_data[x * 4 + 0] = 0;
          dest_data[x * 4 + 1] = 0;
          dest_data[x * 4 + 2] = 0;
        }
      else
        {
          dest_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
          dest_data[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
          dest_data[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
        }
      dest_data[x * 4 + 3] = alpha;
    }

    src_data += src_stride;
    dest_data += dest_stride;
  }
}

static void
convert_no_alpha (guchar  *dest_data,
                  int      dest_stride,
                  guchar  *src_data,
                  int      src_stride,
                  int      src_x,
                  int      src_y,
                  int      dest_x,
                  int      dest_y,
                  int      width,
                  int      height)
{
  int x, y;

  dest_data += dest_stride * dest_y + dest_x * 3;
  src_data += src_stride * src_y + src_x * 4;

  for (y = 0; y < height; y++) {
    guint32 *src = (guint32 *) src_data;

    for (x = 0; x < width; x++) {
      dest_data[x * 3 + 0] = src[x] >> 16;
      dest_data[x * 3 + 1] = src[x] >>  8;
      dest_data[x * 3 + 2] = src[x];
    }

    src_data += src_stride;
    dest_data += dest_stride;
  }
}

/**
 * gdk_pixbuf_get_from_surface:
 * @dest: (allow-none): Destination pixbuf, or %NULL if a new pixbuf should be created.
 * @surface: surface to copy from
 * @src_x: Source X coordinate within drawable.
 * @src_y: Source Y coordinate within drawable.
 * @dest_x: Destination X coordinate in pixbuf, or 0 if @dest is NULL.
 * @dest_y: Destination Y coordinate in pixbuf, or 0 if @dest is NULL.
 * @width: Width in pixels of region to get.
 * @height: Height in pixels of region to get.
 *
 * Transfers image data from a #cairo_surface_t and converts it to an RGB(A)
 * representation inside a #GdkPixbuf. This allows you to efficiently read individual
 * pixels from Cairo surfaces. For #GdkWindows, use gdk_pixbuf_get_from_drawable()
 * instead.
 * 
 * If the specified destination pixbuf @dest is %NULL, then this
 * function will create an RGB pixbuf with 8 bits per channel. The pixbuf will
 * contain an alpha channel if the @surface contains one. In this case, the @dest_x 
 * and @dest_y arguments must be specified as 0.
 *
 * If the specified drawable is a window, and the window is off the
 * screen, then there is no image data in the obscured/offscreen
 * regions to be placed in the pixbuf. The contents of portions of the
 * pixbuf corresponding to the offscreen region are undefined.
 *
 * If the window you're obtaining data from is partially obscured by
 * other windows, then the contents of the pixbuf areas corresponding
 * to the obscured regions are undefined.
 * 
 * If memory can't be allocated for the return value, %NULL will be returned
 * instead.
 *
 * (In short, there are several ways this function can fail, and if it fails
 *  it returns %NULL; so check the return value.)
 *
 * Return value: The same pixbuf as @dest if it was non-%NULL, or a newly-created
 * pixbuf with a reference count of 1 if no destination pixbuf was specified, or %NULL on error
 **/
GdkPixbuf *
gdk_pixbuf_get_from_surface  (GdkPixbuf       *dest,
                              cairo_surface_t *surface,
                              int              src_x,
                              int              src_y,
                              int              dest_x,
                              int              dest_y,
                              int              width,
                              int              height)
{
  cairo_content_t content;
  
  /* General sanity checks */
  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (src_x >= 0 && src_y >= 0, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  if (!dest)
    {
      g_return_val_if_fail (dest_x == 0 && dest_y == 0, NULL);
      
      content = cairo_surface_get_content (surface) | CAIRO_CONTENT_COLOR;
      dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
                             !!(content & CAIRO_CONTENT_ALPHA),
                             8,
                             width, height);
    }
  else
    {
      g_return_val_if_fail (gdk_pixbuf_get_colorspace (dest) == GDK_COLORSPACE_RGB, NULL);
      g_return_val_if_fail (gdk_pixbuf_get_n_channels (dest) == 3 ||
                            gdk_pixbuf_get_n_channels (dest) == 4, NULL);
      g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (dest) == 8, NULL);
      g_return_val_if_fail (dest_x >= 0 && dest_y >= 0, NULL);
      g_return_val_if_fail (dest_x + width <= gdk_pixbuf_get_width (dest), NULL);
      g_return_val_if_fail (dest_y + height <= gdk_pixbuf_get_height (dest), NULL);

      content = gdk_pixbuf_get_has_alpha (dest) ? CAIRO_CONTENT_COLOR_ALPHA : CAIRO_CONTENT_COLOR;
    }

  surface = gdk_cairo_surface_coerce_to_image (surface, content, src_x + width, src_y + height);
  cairo_surface_flush (surface);
  if (cairo_surface_status (surface) || dest == NULL)
    {
      cairo_surface_destroy (surface);
      return NULL;
    }

  if (gdk_pixbuf_get_has_alpha (dest))
    convert_alpha (gdk_pixbuf_get_pixels (dest),
                   gdk_pixbuf_get_rowstride (dest),
                   cairo_image_surface_get_data (surface),
                   cairo_image_surface_get_stride (surface),
                   src_x, src_y,
                   dest_x, dest_y,
                   width, height);
  else
    convert_no_alpha (gdk_pixbuf_get_pixels (dest),
                      gdk_pixbuf_get_rowstride (dest),
                      cairo_image_surface_get_data (surface),
                      cairo_image_surface_get_stride (surface),
                      src_x, src_y,
                      dest_x, dest_y,
                      width, height);

  cairo_surface_destroy (surface);
  return dest;
}

