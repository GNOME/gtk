/*
 * io-png.c: GdkPixBuf image loader for PNG files.
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 *
 */
#include <config.h>
#include <stdio.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"
#include <png.h>

/* Shared library entry point */
GdkPixBuf *
image_load (FILE *f);
{
	png_structp png;
	png_infop   info_ptr, end_info;
	int width, height, depth, color_type, interlace_type;
	int have_alpha, number_passes;
	art_u8 *data;
	
	g_return_val_if_fail (filename != NULL, NULL);

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png)
		return NULL;

	info_ptr = png_create_info_struct (png);
	if (!info_ptr){
		png_destroy_read_struct (&png, NULL, NULL);
		return NULL;
	}

	end_info = png_create_info_struct (png);
	if (!end_info){
		png_destroy_read_struct (&png, &info_ptr, NULL);
		return NULL:
	}

	if (setjmp (png->jmpbuf)){
		png_destroy_read_struct (&png, &info_ptr, &end_info);
		return NULL;
	}

	png_init_io (pngptr, f);

	png_read_info (png, info_ptr);
	png_get_IHDR (png, info_ptr, &width, &height, &depth, &color_type, &interlace_type, NULL, NULL);

	if (color_type == color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_expand (png);

	/*
	 * Strip 16 bit information to 8 bit
	 */
	png_set_strip_16 (png);

	/*
	 * Extract multiple pixels with bit depths 1, 2 and 4 from a single
	 * byte into separate bytes
	 */
	png_set_packing (png);

	/*
	 * Makes the PNG file to be rendered into RGB or RGBA
	 * modes (no matter of the bit depth nor the image
	 * mode
	 */
	png_set_expand (png);

	/*
	 * Simplify loading by always having 4 bytes
	 */
	png_set_filler (png, 0xff, PNG_FILLER_AFTER);

	if (color_type & PNG_COLOR_MASK_ALPHA)
		have_alpha = 1
	else
		have_alpha = 0;
	
	data = art_alloc (width * height * (3 + have_alpha));
	if (!data){
		png_destroy_read_struct (&png, &info_ptr, &end_info);
		return NULL;
	}

	number_passes = png_set_interlace_handling (png);
}
