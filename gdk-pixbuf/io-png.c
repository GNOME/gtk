/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <png.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



/* Destroy notification function for the libart pixbuf */
static void
free_buffer (gpointer user_data, gpointer data)
{
	free (data);
}

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *f)
{
	png_structp png_ptr;
	png_infop info_ptr, end_info;
	gint i, depth, ctype, inttype, passes, bpp;
	png_uint_32 w, h;
	png_bytepp rows;
	guchar *pixels;

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return NULL;

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		return NULL;
	}

	end_info = png_create_info_struct (png_ptr);
	if (!end_info) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	png_init_io (png_ptr, f);
	png_read_info (png_ptr, info_ptr);
	png_get_IHDR (png_ptr, info_ptr, &w, &h, &depth, &ctype, &inttype, NULL, NULL);

	/* Ok, we want to work with 24 bit images.
	 * However, PNG can vary depth per channel.
	 * So, we use the png_set_expand function to expand
	 * everything into a format libart expects.
	 * We also use png_set_strip_16 to reduce down to 8 bit/chan.
	 */
	if (ctype == PNG_COLOR_TYPE_PALETTE && depth <= 8)
		png_set_expand (png_ptr);

	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
		png_set_expand (png_ptr);

	if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_expand (png_ptr);
		g_warning ("FIXME: We are going to crash");
	}

	if (depth == 16)
		png_set_strip_16 (png_ptr);

	/* We also have png "packing" bits into bytes if < 8 */
	if (depth < 8)
		png_set_packing (png_ptr);

	/* Lastly, if the PNG is greyscale, convert to RGB */
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb (png_ptr);

	/* ...and if we're interlaced... */
	passes = png_set_interlace_handling (png_ptr);

	png_read_update_info (png_ptr, info_ptr);

	if (ctype & PNG_COLOR_MASK_ALPHA)
		bpp = 4;
	else
		bpp = 3;

	pixels = malloc (w * h * bpp);
	if (!pixels) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	rows = g_new (png_bytep, h);

	for (i = 0; i < h; i++)
		rows[i] = pixels + i * w * bpp;

	png_read_image (png_ptr, rows);
	png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
	g_free (rows);

	if (ctype & PNG_COLOR_MASK_ALPHA)
		return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, TRUE,
						 w, h, w * 4,
						 free_buffer, NULL);
	else
		return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, FALSE,
						 w, h, w * 3,
						 free_buffer, NULL);
}

/* These avoid the setjmp()/longjmp() crap in libpng */
static void png_error_callback  (png_structp png_read_ptr,
                                 png_const_charp error_msg);

static void png_warning_callback(png_structp png_read_ptr,
                                 png_const_charp warning_msg);

/* Called at the start of the progressive load */
static void png_info_callback   (png_structp png_read_ptr,
                                 png_infop   png_info_ptr);

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void png_row_callback   (png_structp png_read_ptr,
                                png_bytep   new_row,
                                png_uint_32 row_num,
                                int pass_num);

/* Called after reading the entire image */
static void png_end_callback   (png_structp png_read_ptr,
                                png_infop   png_info_ptr);

typedef struct _LoadContext LoadContext;

struct _LoadContext {
        png_structp png_read_ptr;
        png_infop   png_info_ptr;

        ModulePreparedNotifyFunc notify_func;
        gpointer notify_user_data;

        GdkPixbuf* pixbuf;
        
        guint fatal_error_occurred : 1;

};

gpointer
image_begin_load (ModulePreparedNotifyFunc func, gpointer user_data)
{
        LoadContext* lc;
        
        lc = g_new0(LoadContext, 1);
        
        lc->fatal_error_occurred = FALSE;

        lc->notify_func = func;
        lc->notify_user_data = user_data;

        /* Create the main PNG context struct */

        lc->png_read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                  lc, /* error/warning callback data */
                                                  png_error_callback,
                                                  png_warning_callback);

        if (lc->png_read_ptr == NULL) {
                g_free(lc);
                return NULL;
        }

        /* Create the two auxiliary context structs */

        lc->png_info_ptr = png_create_info_struct(lc->png_read_ptr);

        if (lc->png_info_ptr == NULL) {
                png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
                g_free(lc);
                return NULL;
        }

        png_set_progressive_read_fn(lc->png_read_ptr,
                                    lc, /* callback data */
                                    png_info_callback,
                                    png_row_callback,
                                    png_end_callback);
        

        return lc;
}

void
image_stop_load (gpointer context)
{
        LoadContext* lc = context;

        g_return_if_fail(lc != NULL);

        gdk_pixbuf_unref(lc->pixbuf);
        
        png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
        g_free(lc);
}

gboolean
image_load_increment(gpointer context, guchar *buf, guint size)
{
        LoadContext* lc = context;

        g_return_val_if_fail(lc != NULL, FALSE);

        /* Invokes our callbacks as needed */
        png_process_data(lc->png_read_ptr, lc->png_info_ptr, buf, size);

        if (lc->fatal_error_occurred)
                return FALSE;
        else
                return TRUE;
}

/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback   (png_structp png_read_ptr,
                     png_infop   png_info_ptr)
{
        LoadContext* lc;
        png_uint_32 width, height;
        int bit_depth, color_type, filter_type,
          compression_type, interlace_type, channels;
        gboolean have_alpha = FALSE;
        
        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;
        
        /* Get the image info */

        png_get_IHDR (lc->png_read_ptr, lc->png_info_ptr,
                      &width, &height,
                      &bit_depth,
                      &color_type,
                      &interlace_type,
                      &compression_type,
                      &filter_type);

        /* set_expand() basically needs to be called unless
           we are already in RGB/RGBA mode
        */
        if (color_type == PNG_COLOR_TYPE_PALETTE &&
            bit_depth <= 8) {

                /* Convert indexed images to RGB */
                png_set_expand (lc->png_read_ptr);

        } else if (color_type == PNG_COLOR_TYPE_GRAY &&
                   bit_depth < 8) {

                /* Convert grayscale to RGB */
                png_set_expand (lc->png_read_ptr);

        } else if (png_get_valid (lc->png_read_ptr, 
                                  lc->png_info_ptr, PNG_INFO_tRNS)) {

                /* If we have transparency header, convert it to alpha
                   channel */
                png_set_expand(lc->png_read_ptr);
                
        } else if (bit_depth < 8) {

                /* If we have < 8 scale it up to 8 */
                png_set_expand(lc->png_read_ptr);


                /* Conceivably, png_set_packing() is a better idea;
                 * God only knows how libpng works
                 */
        }

        /* If we are 16-bit, convert to 8-bit */
        if (bit_depth == 16) {
                png_set_strip_16(lc->png_read_ptr);
        }

        /* If gray scale, convert to RGB */
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(lc->png_read_ptr);
        }
        
        /* If we have alpha, set a flag */
        if (color_type & PNG_COLOR_MASK_ALPHA)
          have_alpha = TRUE;

        /* Update the info the reflect our transformations */
        png_read_update_info(lc->png_read_ptr, lc->png_info_ptr);
        
        png_get_IHDR (lc->png_read_ptr, lc->png_info_ptr,
                      &width, &height,
                      &bit_depth,
                      &color_type,
                      &interlace_type,
                      &compression_type,
                      &filter_type);

#ifndef G_DISABLE_CHECKS
        /* Check that the new info is what we want */
        
        if (bit_depth != 8) {
                g_warning("Bits per channel of transformed PNG is %d, not 8.", bit_depth);
                lc->fatal_error_occurred = TRUE;
                return;
        }

        if ( ! (color_type == PNG_COLOR_TYPE_RGB ||
                color_type == PNG_COLOR_TYPE_RGB_ALPHA) ) {
                g_warning("Transformed PNG not RGB or RGBA.");
                lc->fatal_error_occurred = TRUE;
                return;
        }

        channels = png_get_channels(lc->png_read_ptr, lc->png_info_ptr);
        if ( ! (channels == 3 || channels == 4) ) {
                g_warning("Transformed PNG has %d channels, must be 3 or 4.", channels);
                lc->fatal_error_occurred = TRUE;
                return;
        }
#endif

        lc->pixbuf = gdk_pixbuf_new(have_alpha, width, height);

        if (lc->pixbuf == NULL) {
                /* Failed to allocate memory */
                lc->fatal_error_occurred = TRUE;
                return;
        }
        
        /* Notify the client that we are ready to go */

        if (lc->notify_func)
                (* lc->notify_func) (lc->pixbuf, lc->notify_user_data);
        
        return;
}

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback   (png_structp png_read_ptr,
                    png_bytep   new_row,
                    png_uint_32 row_num,
                    int pass_num)
{
        LoadContext* lc;
        guchar* old_row = NULL;
        
        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;
                
        old_row = lc->pixbuf->art_pixbuf->pixels + (row_num * lc->pixbuf->art_pixbuf->rowstride);
        
        png_progressive_combine_row(lc->png_read_ptr, old_row, new_row);
}

/* Called after reading the entire image */
static void
png_end_callback   (png_structp png_read_ptr,
                    png_infop   png_info_ptr)
{
        LoadContext* lc;

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;

        /* Doesn't do anything for now */
}


static void
png_error_callback(png_structp png_read_ptr,
                   png_const_charp error_msg)
{
        LoadContext* lc;
        
        lc = png_get_error_ptr(png_read_ptr);
        
        lc->fatal_error_occurred = TRUE;
        
        fprintf(stderr, "Fatal error loading PNG: %s\n", error_msg);
}

static void
png_warning_callback(png_structp png_read_ptr,
                     png_const_charp warning_msg)
{
        LoadContext* lc;
        
        lc = png_get_error_ptr(png_read_ptr);
        
        fprintf(stderr, "Warning loading PNG: %s\n", warning_msg);
}

