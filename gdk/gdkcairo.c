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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gdkcairo.h"
#include "gdkdrawable.h"
#include "gdkinternals.h"
#include "gdkregion-generic.h"
#include "gdkalias.h"

/**
 * gdk_cairo_create:
 * @drawable: a #GdkDrawable
 * 
 * Creates a Cairo context for drawing to @drawable.
 *
 * Return value: A newly created Cairo context. Free with
 *  cairo_destroy() when you are done drawing.
 * 
 * Since: 2.8
 **/
cairo_t *
gdk_cairo_create (GdkDrawable *drawable)
{
  cairo_surface_t *surface;
  cairo_t *cr;
    
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  surface = _gdk_drawable_ref_cairo_surface (drawable);
  cr = cairo_create (surface);
  cairo_surface_destroy (surface);

  return cr;
}

/**
 * gdk_cairo_set_source_color:
 * @cr: a #cairo_t
 * @color: a #GdkColor
 * 
 * Sets the specified #GdkColor as the source color of @cr.
 *
 * Since: 2.8
 **/
void
gdk_cairo_set_source_color (cairo_t  *cr,
			    GdkColor *color)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (color != NULL);
    
  cairo_set_source_rgb (cr,
			color->red / 65535.,
			color->green / 65535.,
			color->blue / 65535.);
}

/**
 * gdk_cairo_rectangle:
 * @cr: a #cairo_t
 * @rectangle: a #GdkRectangle
 * 
 * Adds the given rectangle to the current path of @cr.
 *
 * Since: 2.8
 **/
void
gdk_cairo_rectangle (cairo_t      *cr,
		     GdkRectangle *rectangle)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (rectangle != NULL);

  cairo_rectangle (cr,
		   rectangle->x,     rectangle->y,
		   rectangle->width, rectangle->height);
}

/**
 * gdk_cairo_region:
 * @cr: a #cairo_t
 * @region: a #GdkRegion
 * 
 * Adds the given region to the current path of @cr.
 *
 * Since: 2.8
 **/
void
gdk_cairo_region (cairo_t   *cr,
		  GdkRegion *region)
{
  GdkRegionBox *boxes;
  gint n_boxes, i;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (region != NULL);

  boxes = region->rects;
  n_boxes = region->numRects;

  for (i = 0; i < n_boxes; i++)
    cairo_rectangle (cr,
		     boxes[i].x1,
		     boxes[i].y1,
		     boxes[i].x2 - boxes[i].x1,
		     boxes[i].y2 - boxes[i].y1);
}

/**
 * gdk_cairo_set_source_pixbuf:
 * @cr: a #Cairo context
 * @pixbuf: a #GdkPixbuf
 * @pixbuf_x: X coordinate of location to place upper left corner of @pixbuf
 * @pixbuf_y: Y coordinate of location to place upper left corner of @pixbuf
 * 
 * Sets the given pixbuf as the source pattern for the Cairo context.
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixbuf is @pixbuf_x, @pixbuf_y
 *
 * Since: 2.8
 **/
void
gdk_cairo_set_source_pixbuf (cairo_t   *cr,
			     GdkPixbuf *pixbuf,
			     double     pixbuf_x,
			     double     pixbuf_y)
{
  gint width = gdk_pixbuf_get_width (pixbuf);
  gint height = gdk_pixbuf_get_height (pixbuf);
  guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
  int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  guchar *cairo_pixels;
  cairo_format_t format;
  cairo_surface_t *surface;
  static const cairo_user_data_key_t key;
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
	    
#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

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

  cairo_set_source_surface (cr, surface, pixbuf_x, pixbuf_y);
  cairo_surface_destroy (surface);
}

/**
 * gdk_cairo_set_source_pixmap:
 * @cr: a #Cairo context
 * @pixmap: a #GdkPixmap
 * @pixmap_x: X coordinate of location to place upper left corner of @pixmap
 * @pixmap_y: Y coordinate of location to place upper left corner of @pixmap
 * 
 * Sets the given pixmap as the source pattern for the Cairo context.
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixbuf is @pixbuf_x, @pixbuf_y
 *
 * Since: 2.10
 **/
void
gdk_cairo_set_source_pixmap (cairo_t   *cr,
			     GdkPixmap *pixmap,
			     double     pixmap_x,
			     double     pixmap_y)
{
  cairo_surface_t *surface;
  
  surface = _gdk_drawable_ref_cairo_surface (GDK_DRAWABLE (pixmap));
  cairo_set_source_surface (cr, surface, pixmap_x, pixmap_y);
  cairo_surface_destroy (surface);
}


#define __GDK_CAIRO_C__
#include "gdkaliasdef.c"
