
/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Arjan van de Ven <arjan@fenrus.demon.nl>
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
#include <glib.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



/* Header structure for sunras files.
   All values are in big-endian order on disk
 */
 
struct rasterfile 
{
	guint  magic;
	guint  width;
	guint  height;
	guint  depth;
	guint  length;
	guint  type;
	guint  maptype;
	guint  maplength;
};

/* 
This does a byte-order swap. Does glib have something like
be32_to_cpu() ??
*/
unsigned int ByteOrder(unsigned int i)
{
	unsigned int i2;
	i2 = ((i&255)<<24) | (((i>>8)&255)<<16) | (((i>>16)&255)<<8)|((i>>24)&255);
	return i2; 
}

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
        gboolean failed = FALSE;
	gint i, ctype, bpp;
	guchar *pixels;
	
	struct rasterfile Header;

	i=fread(&Header,1,sizeof(Header),f);
	g_assert(i==32);
	Header.width = ByteOrder(Header.width);
	Header.height = ByteOrder(Header.height);
	Header.depth = ByteOrder(Header.depth);
	
        
	if (Header.depth == 32)
		bpp = 4;
	else
		bpp = 3;

	pixels = (guchar*)malloc (Header.width * Header.height * bpp);
	if (!pixels) {
		return NULL;
	}

	fread(pixels,Header.width*bpp,Header.height,f);

	if (bpp==4)
		return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, TRUE,
						 Header.width, Header.height, 
						 Header.width * bpp,
						 free_buffer, NULL);
	else
		return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, FALSE,
						 Header.width, Header.height, Header.width * bpp,
						 free_buffer, NULL);
}

/* These avoid the setjmp()/longjmp() crap in libpng */

typedef struct _LoadContext LoadContext;

struct _LoadContext {

        ModulePreparedNotifyFunc notify_func;
        gpointer notify_user_data;

        GdkPixbuf* pixbuf;
        
        guint fatal_error_occurred : 1;

};

gpointer
image_begin_load (ModulePreparedNotifyFunc prepare_func,
		  ModuleUpdatedNotifyFunc update_func,
		  gpointer user_data)
{
        LoadContext* lc;
        
        lc = g_new0(LoadContext, 1);
        
        lc->fatal_error_occurred = FALSE;

        lc->notify_func = prepare_func;
        lc->notify_user_data = user_data;

        return lc;
}

void
image_stop_load (gpointer context)
{
        LoadContext* lc = context;

        g_return_if_fail(lc != NULL);

        gdk_pixbuf_unref(lc->pixbuf);
        
        g_free(lc);
}

gboolean
image_load_increment(gpointer context, guchar *buf, guint size)
{
        LoadContext* lc = context;

        g_return_val_if_fail(lc != NULL, FALSE);


        if (lc->fatal_error_occurred)
                return FALSE;
        else
                return TRUE;
}


