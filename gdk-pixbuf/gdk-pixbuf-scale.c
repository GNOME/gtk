#include "gdk-pixbuf.h"
#include "pixops/pixops.h"
#include "math.h"

/**
 * gdk_pixbuf_scale:
 * @src: a #GdkPixbuf
 * @dest: the #GdkPixbuf into which to render the results
 * @dest_x: 
 * @dest_y: 
 * @dest_width: 
 * @dest_height: 
 * @offset_x: the offset in the X direction (currently rounded to an integer)
 * @offset_y: the offset in the Y direction (currently rounded to an integer)
 * @scale_x: the scale factor in the X direction
 * @scale_y: the scale factor in the Y direction
 * @filter_level: the filter quality for the transformation.
 * 
 * Transforms the image by source image by scaling by @scale_x and @scale_y then
 * translating by @offset_x and @offset_y, then renders the rectangle
 * (@dest,@dest_y,@dest_width,@dest_height) of the resulting image onto the
 * destination drawable replacing the previous contents.
 **/
void
gdk_pixbuf_scale (GdkPixbuf      *src,
		  GdkPixbuf      *dest,
		  int             dest_x,
		  int             dest_y,
		  int             dest_width,
		  int             dest_height,
		  double          offset_x,
		  double          offset_y,
		  double          scale_x,
		  double          scale_y,
		  ArtFilterLevel  filter_level)
{
  offset_x = floor(offset_x + 0.5);
  offset_y = floor(offset_y + 0.5);
  
  pixops_scale (dest->art_pixbuf->pixels + dest_y * dest->art_pixbuf->rowstride + dest_x * dest->art_pixbuf->n_channels,
		-offset_x, -offset_y, dest_width - offset_x, dest_height - offset_y,
		dest->art_pixbuf->rowstride, dest->art_pixbuf->n_channels, dest->art_pixbuf->has_alpha,
		src->art_pixbuf->pixels, src->art_pixbuf->width, src->art_pixbuf->height,
		src->art_pixbuf->rowstride, src->art_pixbuf->n_channels, src->art_pixbuf->has_alpha,
		scale_x, scale_y, filter_level);
}

/**
 * gdk_pixbuf_composite:
 * @src: a #GdkPixbuf
 * @dest: the #GdkPixbuf into which to render the results
 * @dest_x: 
 * @dest_y: 
 * @dest_width: 
 * @dest_height: 
 * @offset_x: the offset in the X direction (currently rounded to an integer)
 * @offset_y: the offset in the Y direction (currently rounded to an integer)
 * @scale_x: the scale factor in the X direction
 * @scale_y: the scale factor in the Y direction
 * @filter_level: the filter quality for the transformation.
 * @overall_alpha: overall alpha for source image (0..255)
 * 
 * Transforms the image by source image by scaling by @scale_x and @scale_y then
 * translating by @offset_x and @offset_y, then composites the rectangle
 * (@dest,@dest_y,@dest_width,@dest_height) of the resulting image onto the
 * destination drawable.
 **/
void
gdk_pixbuf_composite (GdkPixbuf      *src,
		      GdkPixbuf      *dest,
		      int             dest_x,
		      int             dest_y,
		      int             dest_width,
		      int             dest_height,
		      double          offset_x,
		      double          offset_y,
		      double          scale_x,
		      double          scale_y,
		      ArtFilterLevel  filter_level,
		      int             overall_alpha)
{
  offset_x = floor(offset_x + 0.5);
  offset_y = floor(offset_y + 0.5);
  pixops_composite (dest->art_pixbuf->pixels + dest_y * dest->art_pixbuf->rowstride + dest_x * dest->art_pixbuf->n_channels,
		    -offset_x, -offset_y, dest_width - offset_x, dest_height - offset_y,
		    dest->art_pixbuf->rowstride, dest->art_pixbuf->n_channels, dest->art_pixbuf->has_alpha,
		    src->art_pixbuf->pixels, src->art_pixbuf->width, src->art_pixbuf->height,
		    src->art_pixbuf->rowstride, src->art_pixbuf->n_channels, src->art_pixbuf->has_alpha,
		    scale_x, scale_y, filter_level, overall_alpha);
}

/**
 * gdk_pixbuf_composite_color:
 * @src: a #GdkPixbuf
 * @dest: the #GdkPixbuf into which to render the results
 * @dest_x: 
 * @dest_y: 
 * @dest_width: 
 * @dest_height: 
 * @offset_x: the offset in the X direction (currently rounded to an integer)
 * @offset_y: the offset in the Y direction (currently rounded to an integer)
 * @scale_x: the scale factor in the X direction
 * @scale_y: the scale factor in the Y direction
 * @filter_level: the filter quality for the transformation.
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
gdk_pixbuf_composite_color (GdkPixbuf      *src,
			    GdkPixbuf      *dest,
			    int             dest_x,
			    int             dest_y,
			    int             dest_width,
			    int             dest_height,
			    double          offset_x,
			    double          offset_y,
			    double          scale_x,
			    double          scale_y,
			    ArtFilterLevel  filter_level,
			    int             overall_alpha,
			    int             check_x,
			    int             check_y,
			    int             check_size,
			    art_u32         color1,
			    art_u32         color2)
{
  offset_x = floor(offset_x + 0.5);
  offset_y = floor(offset_y + 0.5);
  
  pixops_composite_color (dest->art_pixbuf->pixels + dest_y * dest->art_pixbuf->rowstride + dest_x * dest->art_pixbuf->n_channels,
			  -offset_x, -offset_y, dest_width - offset_x, dest_height - offset_y,
			  dest->art_pixbuf->rowstride, dest->art_pixbuf->n_channels, dest->art_pixbuf->has_alpha,
			  src->art_pixbuf->pixels, src->art_pixbuf->width, src->art_pixbuf->height,
			  src->art_pixbuf->rowstride, src->art_pixbuf->n_channels, src->art_pixbuf->has_alpha,
			  scale_x, scale_y, filter_level, overall_alpha, check_x, check_y, check_size, color1, color2);
}

/**
 * gdk_pixbuf_scale_simple:
 * @src: a #GdkPixbuf
 * @dest_width: the width of destination image
 * @dest_height: the height of destination image
 * @filter_level: the filter quality for the transformation.
 * 
 * Scale the #GdkPixbuf @src to @dest_width x @dest_height and render the result into
 * a new #GdkPixbuf.
 * 
 * Return value: the new #GdkPixbuf
 **/
GdkPixbuf *
gdk_pixbuf_scale_simple (GdkPixbuf      *src,
			 int             dest_width,
			 int             dest_height,
			 ArtFilterLevel  filter_level)
{
  GdkPixbuf *dest = gdk_pixbuf_new (ART_PIX_RGB, src->art_pixbuf->has_alpha, 8, dest_width, dest_height);

  gdk_pixbuf_scale (src, dest,  0, 0, dest_width, dest_height, 0, 0,
		    (double)dest_width / src->art_pixbuf->width,
		    (double)dest_height / src->art_pixbuf->height,
		    filter_level);

  return dest;
}

/**
 * gdk_pixbuf_composite_color_simple:
 * @src: a #GdkPixbuf
 * @dest_width: the width of destination image
 * @dest_height: the height of destination image
 * @filter_level: the filter quality for the transformation.
 * @overall_alpha: overall alpha for source image (0..255)
 * @check_size: the size of checks in the checkboard (must be a power of two)
 * @color1: the color of check at upper left
 * @color2: the color of the other check
 * 
 * Scale the #GdkPixbuf @src to @dest_width x @dest_height composite the result with
 * a checkboard of colors @color1 and @color2 and render the result into
 * a new #GdkPixbuf.
 * 
 * Return value: the new #GdkPixbuf
 **/
GdkPixbuf *
gdk_pixbuf_composite_color_simple (GdkPixbuf      *src,
				   int             dest_width,
				   int             dest_height,
				   ArtFilterLevel  filter_level,
				   int             overall_alpha,
				   int             check_size,
				   art_u32         color1,
				   art_u32         color2)
{
  GdkPixbuf *dest = gdk_pixbuf_new (ART_PIX_RGB, src->art_pixbuf->has_alpha, 8, dest_width, dest_height);

  gdk_pixbuf_composite_color (src, dest, 0, 0, dest_width, dest_height, 0, 0,
			      (double)dest_width / src->art_pixbuf->width,
			      (double)dest_height / src->art_pixbuf->height,
			      filter_level, overall_alpha, 0, 0, check_size, color1, color2);

  return dest;
}



