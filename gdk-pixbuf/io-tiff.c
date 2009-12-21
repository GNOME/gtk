/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - TIFF image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
 *          S�ren Sandmann <sandmann@daimi.au.dk>
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

/* Following code (almost) blatantly ripped from Imlib */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <tiffio.h>
#include <errno.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"

#ifdef G_OS_WIN32
#include <fcntl.h>
#include <io.h>
#define lseek(a,b,c) _lseek(a,b,c)
#define O_RDWR _O_RDWR
#endif


typedef struct _TiffContext TiffContext;
struct _TiffContext
{
	GdkPixbufModuleSizeFunc size_func;
	GdkPixbufModulePreparedFunc prepare_func;
	GdkPixbufModuleUpdatedFunc update_func;
	gpointer user_data;
        
        guchar *buffer;
        guint allocated;
        guint used;
        guint pos;
};



static char *global_error = NULL;
static TIFFErrorHandler orig_error_handler = NULL;
static TIFFErrorHandler orig_warning_handler = NULL;

static void
tiff_warning_handler (const char *mod, const char *fmt, va_list ap)
{
        /* Don't print anything; we should not be dumping junk to
         * stderr, since that may be bad for some apps.
         */

        /* libTIFF seems to occasionally warn about things that
         * are really errors, so maybe we should just call tiff_error_handler
         * here.
         */
}

static void
tiff_error_handler (const char *mod, const char *fmt, va_list ap)
{
        if (global_error) {                
                /* Blah, loader called us twice */
                return;
        }

        global_error = g_strdup_vprintf (fmt, ap);
}

static void
tiff_push_handlers (void)
{
        if (global_error)
                g_warning ("TIFF loader left crufty global_error around, FIXME");
        
        orig_error_handler = TIFFSetErrorHandler (tiff_error_handler);
        orig_warning_handler = TIFFSetWarningHandler (tiff_warning_handler);
}

static void
tiff_pop_handlers (void)
{
        if (global_error)
                g_warning ("TIFF loader left crufty global_error around, FIXME");
        
        TIFFSetErrorHandler (orig_error_handler);
        TIFFSetWarningHandler (orig_warning_handler);
}

static void
tiff_set_error (GError    **error,
                int         error_code,
                const char *msg)
{
        /* Take the error message from libtiff and merge it with
         * some context we provide.
         */
        if (global_error) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             error_code,
                             "%s%s%s", msg, ": ", global_error);

                g_free (global_error);
                global_error = NULL;
        }
        else {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     error_code, msg);
        }
}



static void free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

static GdkPixbuf *
tiff_image_parse (TIFF *tiff, TiffContext *context, GError **error)
{
	guchar *pixels = NULL;
	gint width, height, rowstride, bytes;
	GdkPixbuf *pixbuf;
	uint16 orientation = 0;
	uint16 transform = 0;
        uint16 codec;
        gchar *icc_profile_base64;
        const gchar *icc_profile;
        guint icc_profile_size;
        gint retval;

        /* We're called with the lock held. */
        
        g_return_val_if_fail (global_error == NULL, NULL);

	if (!TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &width) || global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Could not get image width (bad TIFF file)"));
                return NULL;
        }
        
        if (!TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &height) || global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Could not get image height (bad TIFF file)"));
                return NULL;
        }

        if (width <= 0 || height <= 0) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Width or height of TIFF image is zero"));
                return NULL;                
        }
        
        rowstride = width * 4;
        if (rowstride / 4 != width) { /* overflow */
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Dimensions of TIFF image too large"));
                return NULL;                
        }
        
        bytes = height * rowstride;
        if (bytes / rowstride != height) { /* overflow */
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Dimensions of TIFF image too large"));
                return NULL;                
        }

	if (context && context->size_func) {
                gint w = width;
                gint h = height;
		(* context->size_func) (&w, &h, context->user_data);
                
		/* This is a signal that this function is being called
		   to support gdk_pixbuf_get_file_info, so we can stop
		   parsing the tiff file at this point. It is not an
		   error condition. */

                if (w == 0 || h == 0)
                    return NULL;
        }

        pixels = g_try_malloc (bytes);

        if (!pixels) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Insufficient memory to open TIFF file"));
                return NULL;
        }

	pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, TRUE, 8, 
                                           width, height, rowstride,
                                           free_buffer, NULL);
        if (!pixbuf) {
                g_free (pixels);
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Insufficient memory to open TIFF file"));
                return NULL;
        }

	/* Set the "orientation" key associated with this image. libtiff 
	   orientation handling is odd, so further processing is required
	   by higher-level functions based on this tag. If the embedded
	   orientation tag is 1-4, libtiff flips/mirrors the image as
	   required, and no client processing is required - so we report 
	   no orientation. Orientations 5-8 require rotations which would 
	   swap the width and height of the image. libtiff does not do this. 
	   Instead it interprets orientations 5-8 the same as 1-4. 
	   See http://bugzilla.remotesensing.org/show_bug.cgi?id=1548.
	   To correct for this, the client must apply the transform normally
	   used for orientation 5 to both orientations 5 and 7, and apply
	   the transform normally used for orientation 7 for both
	   orientations 6 and 8. Then everythings works out OK! */
	
	TIFFGetField (tiff, TIFFTAG_ORIENTATION, &orientation);

	switch (orientation) {
		case 5:
		case 7:
			transform = 5;
			break;
		case 6:
		case 8:
			transform = 7;
			break;
		default:
			transform = 0;
			break;
	}

	if (transform > 0 ) {
		gchar str[5];
		g_snprintf (str, sizeof (str), "%d", transform);
		gdk_pixbuf_set_option (pixbuf, "orientation", str);
	}

        TIFFGetField (tiff, TIFFTAG_COMPRESSION, &codec);
        if (codec > 0) {
          gchar str[5];
          g_snprintf (str, sizeof (str), "%d", codec);
          gdk_pixbuf_set_option (pixbuf, "compression", str);
        }

        /* Extract embedded ICC profile */
        retval = TIFFGetField (tiff, TIFFTAG_ICCPROFILE, &icc_profile_size, &icc_profile);
        if (retval == 1) {
                icc_profile_base64 = g_base64_encode ((const guchar *) icc_profile, icc_profile_size);
                gdk_pixbuf_set_option (pixbuf, "icc-profile", icc_profile_base64);
                g_free (icc_profile_base64);
        }

	if (context && context->prepare_func)
		(* context->prepare_func) (pixbuf, NULL, context->user_data);

	if (!TIFFReadRGBAImageOriented (tiff, width, height, (uint32 *)pixels, ORIENTATION_TOPLEFT, 1) || global_error) {
		tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Failed to load RGB data from TIFF file"));
		g_object_unref (pixbuf);
		return NULL;
	}

#if G_BYTE_ORDER == G_BIG_ENDIAN
	/* Turns out that the packing used by TIFFRGBAImage depends on 
         * the host byte order... 
         */ 
	while (pixels < pixbuf->pixels + bytes) {
		uint32 pixel = *(uint32 *)pixels;
		int r = TIFFGetR(pixel);
		int g = TIFFGetG(pixel);
		int b = TIFFGetB(pixel);
		int a = TIFFGetA(pixel);
		*pixels++ = r;
		*pixels++ = g;
		*pixels++ = b;
		*pixels++ = a;
	}
#endif

	if (context && context->update_func)
		(* context->update_func) (pixbuf, 0, 0, width, height, context->user_data);

        return pixbuf;
}



/* Static loader */

static GdkPixbuf *
gdk_pixbuf__tiff_image_load (FILE *f, GError **error)
{
        TIFF *tiff;
        int fd;
        GdkPixbuf *pixbuf;
        
        g_return_val_if_fail (f != NULL, NULL);

        tiff_push_handlers ();
        
        fd = fileno (f);

        /* On OSF, apparently fseek() works in some on-demand way, so
         * the fseek gdk_pixbuf_new_from_file() doesn't work here
         * since we are using the raw file descriptor. So, we call lseek() on the fd
         * before using it. (#60840)
         */
        lseek (fd, 0, SEEK_SET);
        tiff = TIFFFdOpen (fd, "libpixbuf-tiff", "r");
        
        if (!tiff || global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                _("Failed to open TIFF image"));
                tiff_pop_handlers ();

                return NULL;
        }

        pixbuf = tiff_image_parse (tiff, NULL, error);
        
        TIFFClose (tiff);
        if (global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("TIFFClose operation failed"));
        }
        
        tiff_pop_handlers ();

        return pixbuf;
}



/* Progressive loader */

static gpointer
gdk_pixbuf__tiff_image_begin_load (GdkPixbufModuleSizeFunc size_func,
                                   GdkPixbufModulePreparedFunc prepare_func,
				   GdkPixbufModuleUpdatedFunc update_func,
				   gpointer user_data,
                                   GError **error)
{
	TiffContext *context;
        
	context = g_new0 (TiffContext, 1);
	context->size_func = size_func;
	context->prepare_func = prepare_func;
	context->update_func = update_func;
	context->user_data = user_data;
        context->buffer = NULL;
        context->allocated = 0;
        context->used = 0;
        context->pos = 0;
        
	return context;
}

static tsize_t
tiff_load_read (thandle_t handle, tdata_t buf, tsize_t size)
{
        TiffContext *context = (TiffContext *)handle;
        
        if (context->pos + size > context->used)
                return 0;
        
        memcpy (buf, context->buffer + context->pos, size);
        context->pos += size;
        return size;
}

static tsize_t
tiff_load_write (thandle_t handle, tdata_t buf, tsize_t size)
{
        return -1;
}

static toff_t
tiff_load_seek (thandle_t handle, toff_t offset, int whence)
{
        TiffContext *context = (TiffContext *)handle;
        
        switch (whence) {
        case SEEK_SET:
                if (offset > context->used)
                        return -1;
                context->pos = offset;
                break;
        case SEEK_CUR:
                if (offset + context->pos >= context->used)
                        return -1;
                context->pos += offset;
                break;
        case SEEK_END:
                if (offset + context->used > context->used)
                        return -1;
                context->pos = context->used + offset;
                break;
        default:
                return -1;
        }
        return context->pos;
}

static int
tiff_load_close (thandle_t context)
{
        return 0;
}

static toff_t
tiff_load_size (thandle_t handle)
{
        TiffContext *context = (TiffContext *)handle;
        
        return context->used;
}

static int
tiff_load_map_file (thandle_t handle, tdata_t *buf, toff_t *size)
{
        TiffContext *context = (TiffContext *)handle;
        
        *buf = context->buffer;
        *size = context->used;
        
        return 0;
}

static void
tiff_load_unmap_file (thandle_t handle, tdata_t data, toff_t offset)
{
}

static gboolean
gdk_pixbuf__tiff_image_stop_load (gpointer data,
                                  GError **error)
{
        TiffContext *context = data;
        TIFF *tiff;
        gboolean retval;
        
        g_return_val_if_fail (data != NULL, FALSE);

        tiff_push_handlers ();
        
        tiff = TIFFClientOpen ("libtiff-pixbuf", "r", data, 
                               tiff_load_read, tiff_load_write, 
                               tiff_load_seek, tiff_load_close, 
                               tiff_load_size, 
                               tiff_load_map_file, tiff_load_unmap_file);
        if (!tiff || global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Failed to load TIFF image"));
                retval = FALSE;
        } else {
                GdkPixbuf *pixbuf;
                
                pixbuf = tiff_image_parse (tiff, context, error);
                if (pixbuf)
                        g_object_unref (pixbuf);
                retval = pixbuf != NULL;
                if (global_error)
                        {
                                tiff_set_error (error,
                                                GDK_PIXBUF_ERROR_FAILED,
                                                _("Failed to load TIFF image"));
                                tiff_pop_handlers ();

                                retval = FALSE;
                        }
        }

        if (tiff)
                TIFFClose (tiff);

        g_assert (!global_error);
        
        g_free (context->buffer);
        g_free (context);

        tiff_pop_handlers ();

        return retval;
}

static gboolean
make_available_at_least (TiffContext *context, guint needed)
{
        guchar *new_buffer = NULL;
        guint need_alloc;
        
        need_alloc = context->used + needed;
        if (need_alloc > context->allocated) {
                guint new_size = 1;
                while (new_size < need_alloc)
                        new_size *= 2;
                
                new_buffer = g_try_realloc (context->buffer, new_size);
                if (new_buffer) {
                        context->buffer = new_buffer;
                        context->allocated = new_size;
                        return TRUE;
                }
                return FALSE;
        }
        return TRUE;
}

static gboolean
gdk_pixbuf__tiff_image_load_increment (gpointer data, const guchar *buf,
                                       guint size, GError **error)
{
	TiffContext *context = (TiffContext *) data;
        
	g_return_val_if_fail (data != NULL, FALSE);
        
        if (!make_available_at_least (context, size)) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Insufficient memory to open TIFF file"));
                return FALSE;
        }
        
        memcpy (context->buffer + context->used, buf, size);
        context->used += size;
	return TRUE;
}

typedef struct {
        gchar *buffer;
        guint allocated;
        guint used;
        guint pos;
} TiffSaveContext;

static tsize_t
tiff_save_read (thandle_t handle, tdata_t buf, tsize_t size)
{
        return -1;
}

static tsize_t
tiff_save_write (thandle_t handle, tdata_t buf, tsize_t size)
{
        TiffSaveContext *context = (TiffSaveContext *)handle;

        /* Modify buffer length */
        if (context->pos + size > context->used)
                context->used = context->pos + size;

        /* Realloc */
        if (context->used > context->allocated) {
                context->buffer = g_realloc (context->buffer, context->pos + size);
                context->allocated = context->used;
        }

        /* Now copy the data */
        memcpy (context->buffer + context->pos, buf, size);

        /* Update pos */
        context->pos += size;

        return size;
}

static toff_t
tiff_save_seek (thandle_t handle, toff_t offset, int whence)
{
        TiffSaveContext *context = (TiffSaveContext *)handle;

        switch (whence) {
        case SEEK_SET:
                context->pos = offset;
                break;
        case SEEK_CUR:
                context->pos += offset;
                break;
        case SEEK_END:
                context->pos = context->used + offset;
                break;
        default:
                return -1;
        }
        return context->pos;
}

static int
tiff_save_close (thandle_t context)
{
        return 0;
}

static toff_t
tiff_save_size (thandle_t handle)
{
        return -1;
}

static TiffSaveContext *
create_save_context (void)
{
        TiffSaveContext *context;

        context = g_new (TiffSaveContext, 1);
        context->buffer = NULL;
        context->allocated = 0;
        context->used = 0;
        context->pos = 0;

        return context;
}

static void
free_save_context (TiffSaveContext *context)
{
        g_free (context->buffer);
        g_free (context);
}

static gboolean
gdk_pixbuf__tiff_image_save_to_callback (GdkPixbufSaveFunc   save_func,
                                         gpointer            user_data,
                                         GdkPixbuf          *pixbuf, 
                                         gchar             **keys,
                                         gchar             **values,
                                         GError            **error)
{
        TIFF *tiff;
        gint width, height, rowstride;
        guchar *pixels;
        gboolean has_alpha;
        gushort alpha_samples[1] = { EXTRASAMPLE_UNASSALPHA };
        int y;
        TiffSaveContext *context;
        gboolean retval;
        guchar *icc_profile = NULL;
        gsize icc_profile_size = 0;

        tiff_push_handlers ();

        context = create_save_context ();
        tiff = TIFFClientOpen ("libtiff-pixbuf", "w", context,  
                               tiff_save_read, tiff_save_write, 
                               tiff_save_seek, tiff_save_close, 
                               tiff_save_size, 
                               NULL, NULL);

        if (!tiff || global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Failed to save TIFF image"));

                tiff_pop_handlers ();

                free_save_context (context);
                return FALSE;
        }

        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);

        has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

        height = gdk_pixbuf_get_height (pixbuf);
        width = gdk_pixbuf_get_width (pixbuf);

        TIFFSetField (tiff, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField (tiff, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField (tiff, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField (tiff, TIFFTAG_SAMPLESPERPIXEL, has_alpha ? 4 : 3);
        TIFFSetField (tiff, TIFFTAG_ROWSPERSTRIP, height);

        /* libtiff supports a number of 'codecs' such as:
           1 None, 2 Huffman, 5 LZW, 7 JPEG, 8 Deflate, see tiff.h */
        if (keys && *keys && values && *values) {
            guint i = 0;

            while (keys[i]) {
                if (g_str_equal (keys[i], "compression")) {
                    guint16 codec = strtol (values[i], NULL, 0);
                    if (TIFFIsCODECConfigured (codec))
                        TIFFSetField (tiff, TIFFTAG_COMPRESSION, codec);
                    else {
                        tiff_set_error (error,
                                        GDK_PIXBUF_ERROR_FAILED,
                                        _("TIFF compression doesn't refer to a valid codec."));
                        retval = FALSE;
                        goto cleanup;
                    }
                } else if (g_str_equal (keys[i], "icc-profile")) {
                        /* decode from base64 */
                        icc_profile = g_base64_decode (values[i], &icc_profile_size);
                        if (icc_profile_size < 127) {
                            g_set_error (error,
                                         GDK_PIXBUF_ERROR,
                                         GDK_PIXBUF_ERROR_BAD_OPTION,
                                         _("Color profile has invalid length '%d'."),
                                         icc_profile_size);
                            retval = FALSE;
                            goto cleanup;
                        }
                }
                i++;
            }
        }

        if (has_alpha)
                TIFFSetField (tiff, TIFFTAG_EXTRASAMPLES, 1, alpha_samples);

        TIFFSetField (tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField (tiff, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);        
        TIFFSetField (tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

        if (icc_profile != NULL)
                TIFFSetField (tiff, TIFFTAG_ICCPROFILE, icc_profile_size, icc_profile);

        for (y = 0; y < height; y++) {
                if (TIFFWriteScanline (tiff, pixels + y * rowstride, y, 0) == -1 ||
                    global_error)
                        break;
        }

        if (global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Failed to write TIFF data"));

                TIFFClose (tiff);
                retval = FALSE;
                goto cleanup;
        }

        TIFFClose (tiff);
        if (global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("TIFFClose operation failed"));
                retval = FALSE;
                goto cleanup;
        }


        /* Now call the callback */
        retval = save_func (context->buffer, context->used, error, user_data);

cleanup:
        g_free (icc_profile);
        tiff_pop_handlers ();
        free_save_context (context);
        return retval;
}

static gboolean
save_to_file_cb (const gchar *buf,
		 gsize count,
		 GError **error,
		 gpointer data)
{
	gint bytes;
	
	while (count > 0) {
		bytes = fwrite (buf, sizeof (gchar), count, (FILE *) data);
		if (bytes <= 0)
			break;
		count -= bytes;
		buf += bytes;
	}

	if (count) {
		g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_FAILED,
                                     _("Couldn't write to TIFF file"));
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
gdk_pixbuf__tiff_image_save (FILE          *f, 
                             GdkPixbuf     *pixbuf, 
                             gchar        **keys,
                             gchar        **values,
                             GError       **error)
{
	return gdk_pixbuf__tiff_image_save_to_callback (save_to_file_cb,
                                                        f, pixbuf, keys,
                                                        values, error);
}

#ifndef INCLUDE_tiff
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__tiff_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
        module->load = gdk_pixbuf__tiff_image_load;
        module->begin_load = gdk_pixbuf__tiff_image_begin_load;
        module->stop_load = gdk_pixbuf__tiff_image_stop_load;
        module->load_increment = gdk_pixbuf__tiff_image_load_increment;
        module->save = gdk_pixbuf__tiff_image_save;
        module->save_to_callback = gdk_pixbuf__tiff_image_save_to_callback;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
        static GdkPixbufModulePattern signature[] = {
                { "MM \x2a", "  z ", 100 },
                { "II\x2a ", "   z", 100 },
                { "II* \020   CR\002 ", "   z zzz   z", 0 },
                { NULL, NULL, 0 }
        };
	static gchar * mime_types[] = {
		"image/tiff",
		NULL
	};
	static gchar * extensions[] = {
		"tiff",
		"tif",
		NULL
	};

	info->name = "tiff";
        info->signature = signature;
	info->description = N_("The TIFF image format");
	info->mime_types = mime_types;
	info->extensions = extensions;
        /* not threadsafe, due to the error handler handling */
	info->flags = GDK_PIXBUF_FORMAT_WRITABLE;
	info->license = "LGPL";
}
