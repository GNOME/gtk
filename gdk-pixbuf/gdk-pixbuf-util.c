/* GdkPixbuf library - Utilities and miscellaneous convenience functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gimp.org>
 *          Cody Russell  <bratsche@gnome.org>
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
#include "gdk-pixbuf-private.h"
#include <string.h>



/**
 * gdk_pixbuf_add_alpha:
 * @pixbuf: A pixbuf.
 * @substitute_color: Whether to substitute a color for zero opacity.  If this
 * is #FALSE, then the (@r, @g, @b) arguments will be ignored.
 * @r: Red value to substitute.
 * @g: Green value to substitute.
 * @b: Blue value to substitute.
 *
 * Takes an existing pixbuf and adds an alpha channel to it.  If the original
 * pixbuf already had alpha information, then the contents of the new pixbuf are
 * exactly the same as the original's.  Otherwise, the new pixbuf will have all
 * pixels with full opacity if @substitute_color is #FALSE.  If
 * @substitute_color is #TRUE, then the color specified by (@r, @g, @b) will be
 * substituted for zero opacity.
 *
 * Return value: A newly-created pixbuf with a reference count of 1.
 **/
GdkPixbuf *
gdk_pixbuf_add_alpha (const GdkPixbuf *pixbuf,
		      gboolean substitute_color, guchar r, guchar g, guchar b)
{
	GdkPixbuf *new_pixbuf;
	int x, y;

	g_return_val_if_fail (pixbuf != NULL, NULL);
	g_return_val_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4, NULL);
	g_return_val_if_fail (pixbuf->bits_per_sample == 8, NULL);

	if (pixbuf->has_alpha) {
		new_pixbuf = gdk_pixbuf_copy (pixbuf);
		if (!new_pixbuf)
			return NULL;

		return new_pixbuf;
	}

	new_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pixbuf->width, pixbuf->height);
	if (!new_pixbuf)
		return NULL;

	for (y = 0; y < pixbuf->height; y++) {
		guchar *src, *dest;
		guchar tr, tg, tb;

		src = pixbuf->pixels + y * pixbuf->rowstride;
		dest = new_pixbuf->pixels + y * new_pixbuf->rowstride;

		for (x = 0; x < pixbuf->width; x++) {
			tr = *dest++ = *src++;
			tg = *dest++ = *src++;
			tb = *dest++ = *src++;

			if (substitute_color && tr == r && tg == g && tb == b)
				*dest++ = 0;
			else
				*dest++ = 255;
		}
	}

	return new_pixbuf;
}

/**
 * gdk_pixbuf_copy_area:
 * @src_pixbuf: Source pixbuf.
 * @src_x: Source X coordinate within @src_pixbuf.
 * @src_y: Source Y coordinate within @src_pixbuf.
 * @width: Width of the area to copy.
 * @height: Height of the area to copy.
 * @dest_pixbuf: Destination pixbuf.
 * @dest_x: X coordinate within @dest_pixbuf.
 * @dest_y: Y coordinate within @dest_pixbuf.
 *
 * Copies a rectangular area from @src_pixbuf to @dest_pixbuf.  Conversion of
 * pixbuf formats is done automatically.
 **/
void
gdk_pixbuf_copy_area (const GdkPixbuf *src_pixbuf,
		      int src_x, int src_y,
		      int width, int height,
		      GdkPixbuf *dest_pixbuf,
		      int dest_x, int dest_y)
{
	g_return_if_fail (src_pixbuf != NULL);
	g_return_if_fail (dest_pixbuf != NULL);

	g_return_if_fail (src_x >= 0 && src_x + width <= src_pixbuf->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= src_pixbuf->height);

	g_return_if_fail (dest_x >= 0 && dest_x + width <= dest_pixbuf->width);
	g_return_if_fail (dest_y >= 0 && dest_y + height <= dest_pixbuf->height);

        g_return_if_fail (!(gdk_pixbuf_get_has_alpha (src_pixbuf) && !gdk_pixbuf_get_has_alpha (dest_pixbuf)));
        
	/* This will perform format conversions automatically */

	gdk_pixbuf_scale (src_pixbuf,
			  dest_pixbuf,
			  dest_x, dest_y,
			  width, height,
			  (double) (dest_x - src_x),
			  (double) (dest_y - src_y),
			  1.0, 1.0,
			  GDK_INTERP_NEAREST);
}



#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

/**
 * gdk_pixbuf_saturate_and_pixelate:
 * @src: source image
 * @dest: place to write modified version of @src
 * @saturation: saturation factor
 * @pixelate: whether to pixelate
 *
 * Modifies saturation and optionally pixelates @src, placing the
 * result in @dest. @src and @dest may be the same pixbuf with no ill
 * effects.  If @saturation is 1.0 then saturation is not changed. If
 * it's less than 1.0, saturation is reduced (the image is darkened);
 * if greater than 1.0, saturation is increased (the image is
 * brightened). If @pixelate is TRUE, then pixels are faded in a
 * checkerboard pattern to create a pixelated image. @src and @dest
 * must have the same image format, size, and rowstride.
 * 
 **/
void
gdk_pixbuf_saturate_and_pixelate(const GdkPixbuf *src,
                                 GdkPixbuf *dest,
                                 gfloat saturation,
                                 gboolean pixelate)
{
        /* NOTE that src and dest MAY be the same pixbuf! */
  
        g_return_if_fail (GDK_IS_PIXBUF (src));
        g_return_if_fail (GDK_IS_PIXBUF (dest));
        g_return_if_fail (gdk_pixbuf_get_height (src) == gdk_pixbuf_get_height (dest));
        g_return_if_fail (gdk_pixbuf_get_width (src) == gdk_pixbuf_get_width (dest));
        g_return_if_fail (gdk_pixbuf_get_rowstride (src) == gdk_pixbuf_get_rowstride (dest));
        g_return_if_fail (gdk_pixbuf_get_colorspace (src) == gdk_pixbuf_get_colorspace (dest));
  
        if (saturation == 1.0 && !pixelate) {
                if (dest != src)
                        memcpy (gdk_pixbuf_get_pixels (dest),
                                gdk_pixbuf_get_pixels (src),
                                gdk_pixbuf_get_height (src) * gdk_pixbuf_get_rowstride (src));

                return;
        } else {
                gint i, j;
                gint width, height, has_alpha, rowstride;
                guchar *target_pixels;
                guchar *original_pixels;
                guchar *current_pixel;
                guchar intensity;

                has_alpha = gdk_pixbuf_get_has_alpha (src);
                width = gdk_pixbuf_get_width (src);
                height = gdk_pixbuf_get_height (src);
                rowstride = gdk_pixbuf_get_rowstride (src);
                
                target_pixels = gdk_pixbuf_get_pixels (dest);
                original_pixels = gdk_pixbuf_get_pixels (src);

                for (i = 0; i < height; i++) {
                        for (j = 0; j < width; j++) {
                                current_pixel = original_pixels + i*rowstride + j*(has_alpha?4:3);
                                intensity = INTENSITY (*(current_pixel), *(current_pixel + 1), *(current_pixel + 2));
                                if (pixelate && (i+j)%2 == 0) {
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3)) = intensity/2 + 127;
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 1) = intensity/2 + 127;
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 2) = intensity/2 + 127;
                                } else if (pixelate) {
#define DARK_FACTOR 0.7
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3)) =
                                                (guchar) (((1.0 - saturation) * intensity
                                                           + saturation * (*(current_pixel)))) * DARK_FACTOR;
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 1) =
                                                (guchar) (((1.0 - saturation) * intensity
                                                           + saturation * (*(current_pixel + 1)))) * DARK_FACTOR;
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 2) =
                                                (guchar) (((1.0 - saturation) * intensity
                                                           + saturation * (*(current_pixel + 2)))) * DARK_FACTOR;
                                } else {
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3)) =
                                                (guchar) ((1.0 - saturation) * intensity
                                                          + saturation * (*(current_pixel)));
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 1) =
                                                (guchar) ((1.0 - saturation) * intensity
                                                          + saturation * (*(current_pixel + 1)));
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 2) =
                                                (guchar) ((1.0 - saturation) * intensity
                                                          + saturation * (*(current_pixel + 2)));
                                }
              
                                if (has_alpha)
                                        *(target_pixels + i*rowstride + j*(has_alpha?4:3) + 3) = *(current_pixel + 3);
                        }
                }

                return;
        }
}
