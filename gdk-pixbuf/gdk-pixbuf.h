/* GdkPixbuf library
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Carsten Haitzler <raster@rasterman.com>
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

#ifndef GDK_PIXBUF_H
#define GDK_PIXBUF_H

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_pixbuf.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef struct _GdkPixbuf GdkPixbuf;
typedef void (* GdkPixbufUnrefFunc) (GdkPixbuf *pixbuf);

struct _GdkPixbuf {
	int ref_count;
	ArtPixBuf *art_pixbuf;
	GdkPixbufUnrefFunc *unref_fn;
};



GdkPixbuf *gdk_pixbuf_load_image (const char *file);
void gdk_pixbuf_save_image (const char *format_id, const char *file, ...);

GdkPixbuf *gdk_pixbuf_new (ArtPixBuf *art_pixbuf, GdkPixbufUnrefFunc *unref_fn);

void gdk_pixbuf_ref (GdkPixbuf *pixbuf);
void gdk_pixbuf_unref (GdkPixbuf *pixbuf);

GdkPixbuf *gdk_pixbuf_duplicate (const GdkPixbuf *pixbuf);
GdkPixbuf *gdk_pixbuf_scale (const GdkPixbuf *pixbuf, gint w, gint h);
GdkPixbuf *gdk_pixbuf_rotate (GdkPixbuf *pixbuf, gdouble angle);

void gdk_pixbuf_destroy (GdkPixbuf *pixbuf);

GdkPixbuf *gdk_pixbuf_load_image_from_rgb_d (unsigned char *data, int rgb_width, int rgb_height);



#ifdef __cplusplus
}
#endif

#endif
