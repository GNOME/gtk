/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - TIFF image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
 *          Søren Sandmann <sandmann@daimi.au.dk>
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

#include <config.h>
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
#define O_RDWR _O_RDWR
#endif


typedef struct _TiffContext TiffContext;
struct _TiffContext
{
	ModulePreparedNotifyFunc prepare_func;
	ModuleUpdatedNotifyFunc update_func;
	gpointer user_data;
        
        guchar *buffer;
        guint allocated;
        guint used;
        guint pos;
};



/* There's no user data for the error handlers, so we just have to
 * put a big-ass lock on the whole TIFF loader
 */
G_LOCK_DEFINE_STATIC (tiff_loader);
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
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     error_code,
                     "%s%s%s",
                     msg, global_error ? ": " : "", global_error);

        if (global_error) {
                g_free (global_error);
                global_error = NULL;
        }
}



static void free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

static tileContigRoutine tiff_put_contig;
static tileSeparateRoutine tiff_put_separate;

/* We're lucky that TIFFRGBAImage uses the same RGBA packing
   as gdk-pixbuf, thus we can simple reuse the default libtiff
   put routines, only adjusting the coordinate system.
 */ 
static void
put_contig (TIFFRGBAImage *img, uint32 *raster,
            uint32 x, uint32 y, uint32 w, uint32 h,
            int32 fromskew, int32 toskew, unsigned char *cp) 
{
        uint32 *data = raster - y * img->width - x;

        tiff_put_contig (img, data + img->width * (img->height - 1 - y) + x, 
                         x, y, w, h, fromskew, -toskew - 2*(int32)w, cp);
}

static void
put_separate (TIFFRGBAImage *img, uint32 *raster,
              uint32 x, uint32 y, uint32 w, uint32 h,
              int32 fromskew, int32 toskew, 
              unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a)
{
        uint32 *data = raster - y * img->width - x;

        tiff_put_separate (img, data + img->width * (img->height - 1 - y) + x, 
                           x, y, w, h, fromskew, -toskew - 2*w, r, g, b, a);
}

static GdkPixbuf *
tiff_image_parse (TIFF *tiff, TiffContext *context, GError **error)
{
	guchar *pixels = NULL;
	gint width, height, rowstride, bytes;
	GdkPixbuf *pixbuf;
        TIFFRGBAImage img;
        gchar emsg[1024];

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
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Width or height of TIFF image is zero"));
                return NULL;                
        }
        
        rowstride = width * 4;
        if (rowstride / 4 != width) { /* overflow */
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Dimensions of TIFF image too large"));
                return NULL;                
        }
        
        bytes = height * rowstride;
        if (bytes / rowstride != height) { /* overflow */
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Dimensions of TIFF image too large"));
                return NULL;                
        }

        pixels = g_try_malloc (bytes);

        if (!pixels) {
                g_set_error (error,
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
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                             _("Insufficient memory to open TIFF file"));
                return NULL;
        }

        G_UNLOCK (tiff_loader);
	if (context)
		(* context->prepare_func) (pixbuf, NULL, context->user_data);
        G_LOCK (tiff_loader);
                
        if (!TIFFRGBAImageBegin (&img, tiff, 1, emsg) || global_error) {
                tiff_set_error (error,
                                GDK_PIXBUF_ERROR_FAILED,
                                _("Failed to load RGB data from TIFF file"));
                g_object_unref (pixbuf);
                return NULL;
        }

        if (img.isContig) {
                tiff_put_contig = img.put.contig;
                img.put.contig = put_contig;
        }
        else {
                tiff_put_separate = img.put.separate;
                img.put.separate = put_separate;
        }

        TIFFRGBAImageGet (&img, (uint32 *)pixels, width, height);
        TIFFRGBAImageEnd (&img);

        G_UNLOCK (tiff_loader);
	if (context)
		(* context->update_func) (pixbuf, 0, 0, width, height, context->user_data);
        G_LOCK (tiff_loader);
        
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

        G_LOCK (tiff_loader);

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

                G_UNLOCK (tiff_loader);
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

        G_UNLOCK (tiff_loader);
        
        return pixbuf;
}



/* Progressive loader */

static gpointer
gdk_pixbuf__tiff_image_begin_load (ModulePreparedNotifyFunc prepare_func,
				   ModuleUpdatedNotifyFunc update_func,
				   gpointer user_data,
                                   GError **error)
{
	TiffContext *context;
        
	context = g_new0 (TiffContext, 1);
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
tiff_read (thandle_t handle, tdata_t buf, tsize_t size)
{
        TiffContext *context = (TiffContext *)handle;
        
        if (context->pos + size > context->used)
                return 0;
        
        memcpy (buf, context->buffer + context->pos, size);
        context->pos += size;
        return size;
}

static tsize_t
tiff_write (thandle_t handle, tdata_t buf, tsize_t size)
{
        return -1;
}

static toff_t
tiff_seek (thandle_t handle, toff_t offset, int whence)
{
        TiffContext *context = (TiffContext *)handle;
        
        switch (whence) {
        case SEEK_SET:
                if (offset > context->used || offset < 0)
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
                break;
        }
        return context->pos;
}

static int
tiff_close (thandle_t context)
{
        return 0;
}

static toff_t
tiff_size (thandle_t handle)
{
        TiffContext *context = (TiffContext *)handle;
        
        return context->used;
}

static int
tiff_map_file (thandle_t handle, tdata_t *buf, toff_t *size)
{
        TiffContext *context = (TiffContext *)handle;
        
        *buf = context->buffer;
        *size = context->used;
        
        return 0;
}

static void
tiff_unmap_file (thandle_t handle, tdata_t data, toff_t offset)
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

        G_LOCK (tiff_loader);
        
        tiff_push_handlers ();
        
        tiff = TIFFClientOpen ("libtiff-pixbuf", "r", data, 
                               tiff_read, tiff_write, 
                               tiff_seek, tiff_close, 
                               tiff_size, 
                               tiff_map_file, tiff_unmap_file);
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
                TIFFClose (tiff);
                if (global_error)
                        {
                                tiff_set_error (error,
                                                GDK_PIXBUF_ERROR_FAILED,
                                                _("Failed to load TIFF image"));
                                retval = FALSE;
                        }
        }

        g_assert (!global_error);
        
        g_free (context->buffer);
        g_free (context);

        tiff_pop_handlers ();

        G_UNLOCK (tiff_loader);

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
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                             _("Insufficient memory to open TIFF file"));
                return FALSE;
        }
        
        memcpy (context->buffer + context->used, buf, size);
        context->used += size;
	return TRUE;
}

void
gdk_pixbuf__tiff_fill_vtable (GdkPixbufModule *module)
{
        module->load = gdk_pixbuf__tiff_image_load;
        module->begin_load = gdk_pixbuf__tiff_image_begin_load;
        module->stop_load = gdk_pixbuf__tiff_image_stop_load;
        module->load_increment = gdk_pixbuf__tiff_image_load_increment;
}
