/* GdkPixbuf library - Image creation from in-memory buffers
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
#include "gdk-pixbuf.h"



/**
 * gdk_pixbuf_new_from_data:
 * @data: Image data in 8-bit/sample packed format.
 * @format: Color format used for the data.
 * @has_alpha: Whether the data has an opacity channel.
 * @width: Width of the image in pixels.
 * @height: Height of the image in pixels.
 * @rowstride: Distance in bytes between rows.
 * @dfunc: Function used to free the data when the pixbuf's reference count
 * drops to zero, or NULL if the data should not be freed.
 * @dfunc_data: Closure data to pass to the destroy notification function.
 * 
 * Creates a new #GdkPixbuf out of in-memory RGB data.
 * 
 * Return value: A newly-created #GdkPixbuf structure with a reference count of
 * 1.
 **/
GdkPixbuf *
gdk_pixbuf_new_from_data (const guchar *data, ArtPixFormat format, gboolean has_alpha,
			  int width, int height, int rowstride,
			  ArtDestroyNotify dfunc, gpointer dfunc_data)
{
	ArtPixBuf *art_pixbuf;

	/* Only 8-bit/sample RGB buffers are supported for now */

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (format == ART_PIX_RGB, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	if (has_alpha)
		art_pixbuf = art_pixbuf_new_rgba_dnotify ((art_u8 *)data, width, height, rowstride,
							  dfunc_data, dfunc);
	else
		art_pixbuf = art_pixbuf_new_rgb_dnotify ((art_u8 *)data, width, height, rowstride,
							 dfunc_data, dfunc);

	g_assert (art_pixbuf != NULL);

	return gdk_pixbuf_new_from_art_pixbuf (art_pixbuf);
}
