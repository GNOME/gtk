/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2005 Red Hat, Inc. 
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkcairo.h"

#include "gdkinternals.h"

#include <math.h>

/**
 * SECTION:cairo_interaction
 * @Short_description: Functions to support using cairo
 * @Title: Cairo Interaction
 *
 * [Cairo](http://cairographics.org) is a graphics
 * library that supports vector graphics and image compositing that
 * can be used with GDK. GTK+ does all of its drawing using cairo.
 *
 * GDK does not wrap the cairo API, instead it allows to create cairo
 * contexts which can be used to draw on #GdkWindows. Additional
 * functions allow use #GdkRectangles with cairo and to use #GdkColors,
 * #GdkRGBAs, #GdkPixbufs and #GdkWindows as sources for drawing
 * operations.
 */


/**
 * gdk_cairo_get_clip_rectangle:
 * @cr: a cairo context
 * @rect: (out) (allow-none): return location for the clip, or %NULL
 *
 * This is a convenience function around cairo_clip_extents().
 * It rounds the clip extents to integer coordinates and returns
 * a boolean indicating if a clip area exists.
 *
 * Returns: %TRUE if a clip rectangle exists, %FALSE if all of @cr is
 *     clipped and all drawing can be skipped
 */
gboolean
gdk_cairo_get_clip_rectangle (cairo_t      *cr,
                              GdkRectangle *rect)
{
  double x1, y1, x2, y2;
  gboolean clip_exists;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

  clip_exists = x1 < x2 && y1 < y2;

  if (rect)
    {
      x1 = floor (x1);
      y1 = floor (y1);
      x2 = ceil (x2);
      y2 = ceil (y2);

      rect->x      = CLAMP (x1,      G_MININT, G_MAXINT);
      rect->y      = CLAMP (y1,      G_MININT, G_MAXINT);
      rect->width  = CLAMP (x2 - x1, G_MININT, G_MAXINT);
      rect->height = CLAMP (y2 - y1, G_MININT, G_MAXINT);
    }

  return clip_exists;
}

/**
 * gdk_cairo_set_source_color:
 * @cr: a cairo context
 * @color: a #GdkColor
 *
 * Sets the specified #GdkColor as the source color of @cr.
 *
 * Since: 2.8
 *
 * Deprecated: 3.4: Use gdk_cairo_set_source_rgba() instead
 */
void
gdk_cairo_set_source_color (cairo_t        *cr,
                            const GdkColor *color)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (color != NULL);
    
  cairo_set_source_rgb (cr,
                        color->red / 65535.,
                        color->green / 65535.,
                        color->blue / 65535.);
}

/**
 * gdk_cairo_set_source_rgba:
 * @cr: a cairo context
 * @rgba: a #GdkRGBA
 *
 * Sets the specified #GdkRGBA as the source color of @cr.
 *
 * Since: 3.0
 */
void
gdk_cairo_set_source_rgba (cairo_t       *cr,
                           const GdkRGBA *rgba)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (rgba != NULL);

  cairo_set_source_rgba (cr,
                         rgba->red,
                         rgba->green,
                         rgba->blue,
                         rgba->alpha);
}

/**
 * gdk_cairo_rectangle:
 * @cr: a cairo context
 * @rectangle: a #GdkRectangle
 *
 * Adds the given rectangle to the current path of @cr.
 *
 * Since: 2.8
 */
void
gdk_cairo_rectangle (cairo_t            *cr,
                     const GdkRectangle *rectangle)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (rectangle != NULL);

  cairo_rectangle (cr,
                   rectangle->x,     rectangle->y,
                   rectangle->width, rectangle->height);
}

/**
 * gdk_cairo_region:
 * @cr: a cairo context
 * @region: a #cairo_region_t
 *
 * Adds the given region to the current path of @cr.
 *
 * Since: 2.8
 */
void
gdk_cairo_region (cairo_t              *cr,
                  const cairo_region_t *region)
{
  cairo_rectangle_int_t box;
  gint n_boxes, i;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (region != NULL);

  n_boxes = cairo_region_num_rectangles (region);

  for (i = 0; i < n_boxes; i++)
    {
      cairo_region_get_rectangle (region, i, &box);
      cairo_rectangle (cr, box.x, box.y, box.width, box.height);
    }
}

static void
gdk_cairo_surface_paint_pixbuf (cairo_surface_t *surface,
                                const GdkPixbuf *pixbuf)
{
  gint width, height;
  guchar *gdk_pixels, *cairo_pixels;
  int gdk_rowstride, cairo_stride;
  int n_channels;
  int j;

  /* This function can't just copy any pixbuf to any surface, be
   * sure to read the invariants here before calling it */

  g_assert (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE);
  g_assert (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_RGB24 ||
            cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32);
  g_assert (cairo_image_surface_get_width (surface) == gdk_pixbuf_get_width (pixbuf));
  g_assert (cairo_image_surface_get_height (surface) == gdk_pixbuf_get_height (pixbuf));

  cairo_surface_flush (surface);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
  gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  cairo_stride = cairo_image_surface_get_stride (surface);
  cairo_pixels = cairo_image_surface_get_data (surface);

  for (j = height; j; j--)
    {
      guchar *p = gdk_pixels;
      guchar *q = cairo_pixels;

      if (n_channels == 3)
        {
          guchar *end = p + 3 * width;

          while (p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              q[0] = p[2];
              q[1] = p[1];
              q[2] = p[0];
#else
              q[1] = p[0];
              q[2] = p[1];
              q[3] = p[2];
#endif
              p += 3;
              q += 4;
            }
        }
      else
        {
          guchar *end = p + 4 * width;
          guint t1,t2,t3;

#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x80; d = ((t >> 8) + t) >> 8; } G_STMT_END

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
      cairo_pixels += cairo_stride;
    }

  cairo_surface_mark_dirty (surface);
}

/**
 * gdk_cairo_surface_create_from_pixbuf:
 * @pixbuf: a #GdkPixbuf
 * @scale: the scale of the new surface, or 0 to use same as @window
 * @for_window: (allow-none): The window this will be drawn to, or %NULL
 *
 * Creates an image surface with the same contents as
 * the pixbuf.
 *
 * Returns: a new cairo surface, must be freed with cairo_surface_destroy()
 *
 * Since: 3.10
 */
cairo_surface_t *
gdk_cairo_surface_create_from_pixbuf (const GdkPixbuf *pixbuf,
                                      int              scale,
                                      GdkWindow       *for_window)
{
  cairo_format_t format;
  cairo_surface_t *surface;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (scale >= 0, NULL);
  g_return_val_if_fail (for_window == NULL || GDK_IS_WINDOW (for_window), NULL);

  if (gdk_pixbuf_get_n_channels (pixbuf) == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  surface =
     gdk_window_create_similar_image_surface (for_window,
					      format,
                                              gdk_pixbuf_get_width (pixbuf),
                                              gdk_pixbuf_get_height (pixbuf),
					      scale);

  gdk_cairo_surface_paint_pixbuf (surface, pixbuf);

  return surface;
}

/**
 * gdk_cairo_set_source_pixbuf:
 * @cr: a cairo context
 * @pixbuf: a #GdkPixbuf
 * @pixbuf_x: X coordinate of location to place upper left corner of @pixbuf
 * @pixbuf_y: Y coordinate of location to place upper left corner of @pixbuf
 *
 * Sets the given pixbuf as the source pattern for @cr.
 *
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixbuf is @pixbuf_x, @pixbuf_y.
 *
 * Since: 2.8
 */
void
gdk_cairo_set_source_pixbuf (cairo_t         *cr,
                             const GdkPixbuf *pixbuf,
                             gdouble          pixbuf_x,
                             gdouble          pixbuf_y)
{
  cairo_format_t format;
  cairo_surface_t *surface;

  if (gdk_pixbuf_get_n_channels (pixbuf) == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  surface = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                format,
                                                gdk_pixbuf_get_width (pixbuf),
                                                gdk_pixbuf_get_height (pixbuf));

  gdk_cairo_surface_paint_pixbuf (surface, pixbuf);

  cairo_set_source_surface (cr, surface, pixbuf_x, pixbuf_y);
  cairo_surface_destroy (surface);
}

/**
 * gdk_cairo_set_source_window:
 * @cr: a cairo context
 * @window: a #GdkWindow
 * @x: X coordinate of location to place upper left corner of @window
 * @y: Y coordinate of location to place upper left corner of @window
 *
 * Sets the given window as the source pattern for @cr.
 *
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @window is @x, @y. The window contains all its
 * subwindows when rendering.
 *
 * Note that the contents of @window are undefined outside of the
 * visible part of @window, so use this function with care.
 *
 * Since: 2.24
 */
void
gdk_cairo_set_source_window (cairo_t   *cr,
                             GdkWindow *window,
                             gdouble    x,
                             gdouble    y)
{
  cairo_surface_t *surface;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  surface = _gdk_window_ref_cairo_surface (window);
  cairo_set_source_surface (cr, surface, x, y);
  cairo_surface_destroy (surface);
}

/*
 * _gdk_cairo_surface_extents:
 * @surface: surface to measure
 * @extents: (out): rectangle to put the extents
 *
 * Measures the area covered by @surface and puts it into @extents.
 *
 * Note that this function respects device offsets set on @surface.
 * If @surface is unbounded, the resulting extents will be empty and
 * not be a maximal sized rectangle. This is to avoid careless coding.
 * You must explicitly check the return value of you want to handle
 * that case.
 *
 * Returns: %TRUE if the extents fit in a #GdkRectangle, %FALSE if not
 */
gboolean
_gdk_cairo_surface_extents (cairo_surface_t *surface,
                            GdkRectangle    *extents)
{
  double x1, x2, y1, y2;
  cairo_t *cr;

  g_return_val_if_fail (surface != NULL, FALSE);
  g_return_val_if_fail (extents != NULL, FALSE);

  cr = cairo_create (surface);
  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  cairo_destroy (cr);

  x1 = floor (x1);
  y1 = floor (y1);
  x2 = ceil (x2);
  y2 = ceil (y2);
  x2 -= x1;
  y2 -= y1;

  if (x1 < G_MININT || x1 > G_MAXINT ||
      y1 < G_MININT || y1 > G_MAXINT ||
      x2 > G_MAXINT || y2 > G_MAXINT)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return FALSE;
    }

  extents->x = x1;
  extents->y = y1;
  extents->width = x2;
  extents->height = y2;

  return TRUE;
}

/* This function originally from Jean-Edouard Lachand-Robert, and
 * available at www.codeguru.com. Simplified for our needs, not sure
 * how much of the original code left any longer. Now handles just
 * one-bit deep bitmaps (in Window parlance, ie those that GDK calls
 * bitmaps (and not pixmaps), with zero pixels being transparent.
 */
/**
 * gdk_cairo_region_create_from_surface:
 * @surface: a cairo surface
 *
 * Creates region that describes covers the area where the given
 * @surface is more than 50% opaque.
 *
 * This function takes into account device offsets that might be
 * set with cairo_surface_set_device_offset().
 *
 * Returns: A #cairo_region_t; must be freed with cairo_region_destroy()
 */
cairo_region_t *
gdk_cairo_region_create_from_surface (cairo_surface_t *surface)
{
  cairo_region_t *region;
  GdkRectangle extents, rect;
  cairo_surface_t *image;
  cairo_t *cr;
  gint x, y, stride;
  guchar *data;

  _gdk_cairo_surface_extents (surface, &extents);

  if (cairo_surface_get_content (surface) == CAIRO_CONTENT_COLOR)
    return cairo_region_create_rectangle (&extents);

  if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE ||
      cairo_image_surface_get_format (surface) != CAIRO_FORMAT_A1)
    {
      /* coerce to an A1 image */
      image = cairo_image_surface_create (CAIRO_FORMAT_A1,
                                          extents.width, extents.height);
      cr = cairo_create (image);
      cairo_set_source_surface (cr, surface, -extents.x, -extents.y);
      cairo_paint (cr);
      cairo_destroy (cr);
    }
  else
    image = cairo_surface_reference (surface);

  /* Flush the surface to make sure that the rendering is up to date. */
  cairo_surface_flush (image);

  data = cairo_image_surface_get_data (image);
  stride = cairo_image_surface_get_stride (image);

  region = cairo_region_create ();

  for (y = 0; y < extents.height; y++)
    {
      for (x = 0; x < extents.width; x++)
        {
          /* Search for a continuous range of "non transparent pixels"*/
          gint x0 = x;
          while (x < extents.width)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              if (((data[x / 8] >> (x%8)) & 1) == 0)
#else
              if (((data[x / 8] >> (7-(x%8))) & 1) == 0)
#endif
                /* This pixel is "transparent"*/
                break;
              x++;
            }

          if (x > x0)
            {
              /* Add the pixels (x0, y) to (x, y+1) as a new rectangle
               * in the region
               */
              rect.x = x0;
              rect.width = x - x0;
              rect.y = y;
              rect.height = 1;

              cairo_region_union_rectangle (region, &rect);
            }
        }
      data += stride;
    }

  cairo_surface_destroy (image);

  cairo_region_translate (region, extents.x, extents.y);

  return region;
}

cairo_region_t *
gdk_cairo_region_from_clip (cairo_t *cr)
{
  cairo_rectangle_list_t *rectangles;
  cairo_region_t *region;
  int i;

  rectangles = cairo_copy_clip_rectangle_list (cr);

  if (rectangles->status != CAIRO_STATUS_SUCCESS)
    return NULL;

  region = cairo_region_create ();
  for (i = 0; i < rectangles->num_rectangles; i++)
    {
      cairo_rectangle_int_t clip_rect;
      cairo_rectangle_t *rect;

      rect = &rectangles->rectangles[i];

      /* Here we assume clip rects are ints for direct targets, which
         is true for cairo */
      clip_rect.x = (int)rect->x;
      clip_rect.y = (int)rect->y;
      clip_rect.width = (int)rect->width;
      clip_rect.height = (int)rect->height;

      cairo_region_union_rectangle (region, &clip_rect);
    }

  cairo_rectangle_list_destroy (rectangles);

  return region;
}
