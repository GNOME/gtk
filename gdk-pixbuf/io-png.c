/*
 * io-png.c: GdkPixBuf I/O for PNG files.
 *
 * Author:
 *    Mark Crichton <crichton@gimp.org>
 *
 */
#include <config.h>
#include <stdio.h>
#include <glib.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"
#include <png.h>

/* Shared library entry point */
GdkPixBuf *image_load(FILE * f)
{
	png_structp png_ptr;
	png_infop info_ptr, end_info;
	gint i, depth, ctype, inttype, passes, bpp;  /* bpp = BYTES/pixel */
	png_uint_32 w, h, x, y;
	png_bytepp rows;
	art_u8 *pixels, *temp, *rowdata;
	GdkPixBuf *pixbuf;

	g_return_val_if_fail (f != NULL, NULL);

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL,
		  NULL);

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	end_info = png_create_info_struct (png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	png_init_io(png_ptr, f);
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &w, &h, &depth, &ctype, &inttype,
		     NULL, NULL);

	/* Ok, we want to work with 24 bit images.
	 * However, PNG can vary depth per channel.
	 * So, we use the png_set_expand function to expand
	 * everything into a format libart expects.
	 * We also use png_set_strip_16 to reduce down to 8 bit/chan.
	 */

	if (ctype == PNG_COLOR_TYPE_PALETTE && depth <= 8)
		png_set_expand(png_ptr);

	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
		png_set_expand(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);

	if (depth == 16)
		png_set_strip_16(png_ptr);

	/* We also have png "packing" bits into bytes if < 8 */
	if (depth < 8)
		png_set_packing(png_ptr);

	/* Lastly, if the PNG is greyscale, convert to RGB */
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	/* ...and if we're interlaced... */
	passes = png_set_interlace_handling(png_ptr);

	/* Update our info structs */
	png_read_update_info(png_ptr, info_ptr);

	/* Allocate some memory and set up row array */
	/* This "inhales vigirously"... */
        if (ctype & PNG_COLOR_MASK_ALPHA)
                bpp = 4;
        else
                bpp = 3;

	pixels = art_alloc(w*h*bpp);
	rows = g_malloc(h*sizeof(png_bytep));

	if ((!pixels) || (!rows)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	/* Icky code, but it has to be done... */
	for (i = 0; i < h; i++) {
		if ((rows[i] = g_malloc(w*sizeof(art_u8)*bpp)) == NULL) {
			int n;
			for (n = 0; n < i; n++)
				g_free(rows[i]);
			g_free(rows);
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			return NULL;
		}
	}
	
	/* And we FINALLY get here... */
	png_read_image(png_ptr, rows);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	
	/* Now stuff the bytes into pixels & free rows[y] */
	
	temp = pixels;

	for (y = 0; y < h; y++) {
		(png_bytep)rowdata = rows[y];
		for (x = 0; x < w; x++) {
			temp[0] = rowdata[(x*bpp)];
			temp[1] = rowdata[(x*bpp)+1];
			temp[2] = rowdata[(x*bpp)+2];
			if (bpp == 4)
				temp[3] = rowdata[(x*bpp)+3];
			temp += bpp;
		}
		g_free(rows[y]);
	}
	g_free(rows);

	/* Return the GdkPixBuf */
	pixbuf = g_new(GdkPixBuf, 1);

	if (ctype & PNG_COLOR_MASK_ALPHA)
		pixbuf->art_pixbuf = art_pixbuf_new_rgba (pixels, w, h, (w*4));
	else
		pixbuf->art_pixbuf = art_pixbuf_new_rgb (pixels, w, h, (w*3));

	/* Ok, I'm anal...shoot me */
	if (!(pixbuf->art_pixbuf))
		return NULL;
	pixbuf->ref_count = 0;
	pixbuf->unref_func = NULL;

	return pixbuf;
}
