/* GdkPixbuf library - Main header file
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
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

#ifndef GDK_PIXBUF_H
#define GDK_PIXBUF_H

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_pixbuf.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif



/* GdkPixbuf structure */

typedef struct _GdkPixbuf GdkPixbuf;

struct _GdkPixbuf {
	/* Reference count */
	int ref_count;

	/* Libart pixbuf */
	ArtPixBuf *art_pixbuf;
};



/* Reference counting */

void gdk_pixbuf_ref (GdkPixbuf *pixbuf);
void gdk_pixbuf_unref (GdkPixbuf *pixbuf);

/* Wrap a libart pixbuf */

GdkPixbuf *gdk_pixbuf_new_from_art_pixbuf (ArtPixBuf *art_pixbuf);

/* Simple loading */

GdkPixbuf *gdk_pixbuf_new_from_file (const char *filename);
GdkPixbuf *gdk_pixbuf_new_from_data (guchar *data, ArtPixFormat format, gboolean has_alpha,
				     int width, int height, int rowstride,
				     ArtDestroyNotify dfunc, gpointer dfunc_data);
GdkPixbuf *gdk_pixbuf_new_from_xpm_data (const gchar **data);

	
/* Transformations */
#if 0
GdkPixbuf *gdk_pixbuf_scale (const GdkPixbuf *pixbuf, gint w, gint h);
GdkPixbuf *gdk_pixbuf_rotate (GdkPixbuf *pixbuf, gdouble angle);
#endif



#ifdef __cplusplus
}
#endif

#endif
