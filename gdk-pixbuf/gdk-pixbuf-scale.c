/* GdkPixbuf library - Scaling and compositing functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Owen Taylor <otaylor@redhat.com>
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
#include <math.h>
#include "gdk-pixbuf-private.h"
#include "pixops/pixops.h"



/**
 * gdk_pixbuf_scale:
 * @src: a #GdkPixbuf
 * @dest: the #GdkPixbuf into which to render the results
 * @dest_x: the left coordinate for region to render
 * @dest_y: the top coordinate for region to render
 * @dest_width: the width of the region to render
 * @dest_height: the height of the region to render
 * @offset_x: the offset in the X direction (currently rounded to an integer)
 * @offset_y: the offset in the Y direction (currently rounded to an integer)
 * @scale_x: the scale factor in the X direction
 * @scale_y: the scale factor in the Y direction
 * @interp_type: the interpolation type for the transformation.
 * 
 * Transforms the image by source image by scaling by @scale_x and @scale_y then
 * translating by @offset_x and @offset_y, then renders the rectangle
 * (@dest,@dest_y,@dest_width,@dest_height) of the resulting image onto the
 * destination drawable replacing the previous contents.
 **/
void
gdk_pixbuf_scale (const GdkPixbuf *src,
		  GdkPixbuf       *dest,
		  int              dest_x,
		  int              dest_y,
		  int              dest_width,
		  int              dest_height,
		  double           offset_x,
		  double           offset_y,
		  double           scale_x,
		  double           scale_y,
		  GdkInterpType    interp_type)
{
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest_x >= 0 && dest_x + dest_width <= dest->width);
  g_return_if_fail (dest_y >= 0 && dest_y + dest_height <= dest->height);

  offset_x = floor (offset_x + 0.5);
  offset_y = floor (offset_y + 0.5);
  
  pixops_scale (dest->pixels + dest_y * dest->rowstride + dest_x * dest->n_channels,
		dest_x - offset_x, dest_y - offset_y, 
		dest_x + dest_width - offset_x, dest_y + dest_height - offset_y,
		dest->rowstride, dest->n_channels, dest->has_alpha,
		src->pixels, src->width, src->height,
		src->rowstride, src->n_channels, src->has_alpha,
		scale_x, scale_y, interp_type);
}

/**
 * gdk_pixbuf_composite:
 * @src: a #GdkPixbuf
 * @dest: the #GdkPixbuf into which to render the results
 * @dest_x: the left coordinate for region to render
 * @dest_y: the top coordinate for region to render
 * @dest_width: the width of the region to render
 * @dest_height: the height of the region to render
 * @offset_x: the offset in the X direction (currently rounded to an integer)
 * @offset_y: the offset in the Y direction (currently rounded to an integer)
 * @scale_x: the scale factor in the X direction
 * @scale_y: the scale factor in the Y direction
 * @interp_type: the interpolation type for the transformation.
 * @overall_alpha: overall alpha for source image (0..255)
 * 
 * Transforms the image by source image by scaling by @scale_x and @scale_y then
 * translating by @offset_x and @offset_y, then composites the rectangle
 * (@dest,@dest_y,@dest_width,@dest_height) of the resulting image onto the
 * destination drawable.
 **/
void
gdk_pixbuf_composite (const GdkPixbuf *src,
		      GdkPixbuf       *dest,
		      int              dest_x,
		      int              dest_y,
		      int              dest_width,
		      int              dest_height,
		      double           offset_x,
		      double           offset_y,
		      double           scale_x,
		      double           scale_y,
		      GdkInterpType    interp_type,
		      int              overall_alpha)
{
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest_x >= 0 && dest_x + dest_width <= dest->width);
  g_return_if_fail (dest_y >= 0 && dest_y + dest_height <= dest->height);
  g_return_if_fail (overall_alpha >= 0 && overall_alpha <= 255);

  offset_x = floor (offset_x + 0.5);
  offset_y = floor (offset_y + 0.5);
  pixops_composite (dest->pixels + dest_y * dest->rowstride + dest_x * dest->n_channels,
		    dest_x - offset_x, dest_y - offset_y, 
		    dest_x + dest_width - offset_x, dest_y + dest_height - offset_y,
		    dest->rowstride, dest->n_channels, dest->has_alpha,
		    src->pixels, src->width, src->height,
		    src->rowstride, src->n_channels, src->has_alpha,
		    scale_x, scale_y, interp_type, overall_alpha);
}

/**
 * gdk_pixbuf_composite_color:
 * @src: a #GdkPixbuf
 * @dest: the #GdkPixbuf into which to render the results
 * @dest_x: the left coordinate for region to render
 * @dest_y: the top coordinate for region to render
 * @dest_width: the width of the region to render
 * @dest_height: the height of the region to render
 * @offset_x: the offset in the X direction (currently rounded to an integer)
 * @offset_y: the offset in the Y direction (currently rounded to an integer)
 * @scale_x: the scale factor in the X direction
 * @scale_y: the scale factor in the Y direction
 * @interp_type: the interpolation type for the transformation.
 * @overall_alpha: overall alpha for source image (0..255)
 * @check_x: the X offset for the checkboard (origin of checkboard is at -@check_x, -@check_y)
 * @check_y: the Y offset for the checkboard 
 * @check_size: the size of checks in the checkboard (must be a power of two)
 * @color1: the color of check at upper left
 * @color2: the color of the other check
 * 
 * Transforms the image by source image by scaling by @scale_x and @scale_y then
 * translating by @offset_x and @offset_y, then composites the rectangle
 * (@dest,@dest_y,@dest_width,@dest_height) of the resulting image with
 * a checkboard of the colors @color1 and @color2 and renders it onto the
 * destination drawable.
 **/
void
gdk_pixbuf_composite_color (const GdkPixbuf *src,
			    GdkPixbuf       *dest,
			    int              dest_x,
			    int              dest_y,
			    int              dest_width,
			    int              dest_height,
			    double           offset_x,
			    double           offset_y,
			    double           scale_x,
			    double           scale_y,
			    GdkInterpType    interp_type,
			    int              overall_alpha,
			    int              check_x,
			    int              check_y,
			    int              check_size,
			    guint32          color1,
			    guint32          color2)
{
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest_x >= 0 && dest_x + dest_width <= dest->width);
  g_return_if_fail (dest_y >= 0 && dest_y + dest_height <= dest->height);
  g_return_if_fail (overall_alpha >= 0 && overall_alpha <= 255);

  offset_x = floor (offset_x + 0.5);
  offset_y = floor (offset_y + 0.5);
  
  pixops_composite_color (dest->pixels + dest_y * dest->rowstride + dest_x * dest->n_channels,
			  dest_x - offset_x, dest_y - offset_y, 
			  dest_x + dest_width - offset_x, dest_y + dest_height - offset_y,
			  dest->rowstride, dest->n_channels, dest->has_alpha,
			  src->pixels, src->width, src->height,
			  src->rowstride, src->n_channels, src->has_alpha,
			  scale_x, scale_y, interp_type, overall_alpha, check_x, check_y,
			  check_size, color1, color2);
}

/**
 * gdk_pixbuf_scale_simple:
 * @src: a #GdkPixbuf
 * @dest_width: the width of destination image
 * @dest_height: the height of destination image
 * @interp_type: the interpolation type for the transformation.
 * 
 * Scale the #GdkPixbuf @src to @dest_width x @dest_height and render the result into
 * a new #GdkPixbuf.
 * 
 * Return value: the new #GdkPixbuf, or NULL if not enough memory could be
 * allocated for it.
 **/
GdkPixbuf *
gdk_pixbuf_scale_simple (const GdkPixbuf *src,
			 int              dest_width,
			 int              dest_height,
			 GdkInterpType    interp_type)
{
  GdkPixbuf *dest;

  g_return_val_if_fail (src != NULL, NULL);
  g_return_val_if_fail (dest_width > 0, NULL);
  g_return_val_if_fail (dest_height > 0, NULL);

  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, src->has_alpha, 8, dest_width, dest_height);
  if (!dest)
    return NULL;

  gdk_pixbuf_scale (src, dest,  0, 0, dest_width, dest_height, 0, 0,
		    (double) dest_width / src->width,
		    (double) dest_height / src->height,
		    interp_type);

  return dest;
}

/**
 * gdk_pixbuf_composite_color_simple:
 * @src: a #GdkPixbuf
 * @dest_width: the width of destination image
 * @dest_height: the height of destination image
 * @interp_type: the interpolation type for the transformation.
 * @overall_alpha: overall alpha for source image (0..255)
 * @check_size: the size of checks in the checkboard (must be a power of two)
 * @color1: the color of check at upper left
 * @color2: the color of the other check
 * 
 * Scale the #GdkPixbuf @src to @dest_width x @dest_height composite the result with
 * a checkboard of colors @color1 and @color2 and render the result into
 * a new #GdkPixbuf.
 * 
 * Return value: the new #GdkPixbuf, or NULL if not enough memory could be
 * allocated for it.
 **/
GdkPixbuf *
gdk_pixbuf_composite_color_simple (const GdkPixbuf *src,
				   int              dest_width,
				   int              dest_height,
				   GdkInterpType    interp_type,
				   int              overall_alpha,
				   int              check_size,
				   guint32          color1,
				   guint32          color2)
{
  GdkPixbuf *dest;

  g_return_val_if_fail (src != NULL, NULL);
  g_return_val_if_fail (dest_width > 0, NULL);
  g_return_val_if_fail (dest_height > 0, NULL);
  g_return_val_if_fail (overall_alpha >= 0 && overall_alpha <= 255, NULL);

  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, src->has_alpha, 8, dest_width, dest_height);
  if (!dest)
    return NULL;

  gdk_pixbuf_composite_color (src, dest, 0, 0, dest_width, dest_height, 0, 0,
			      (double) dest_width / src->width,
			      (double) dest_height / src->height,
			      interp_type, overall_alpha, 0, 0, check_size, color1, color2);

  return dest;
}
