/* GNOME libraries - GdkPixbuf item for the GNOME canvas
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
#include "gnome-canvas-pixbuf.h"



static void gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class);
static void gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *cpb);
static void gnome_canvas_pixbuf_destroy (GtkObject *object);

static GnomeCanvasItemClass *parent_class;



/**
 * gnome_canvas_pixbuf_get_type:
 * @void: 
 * 
 * Registers the &GnomeCanvasPixbuf class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GnomeCanvasPixbuf class.
 **/
GtkType
gnome_canvas_pixbuf_get_type (void)
{
	static GtkType canvas_pixbuf_type = 0;

	if (!canvas_pixbuf_type) {
		static const GtkTypeInfo canvas_pixbuf_info = {
			"GnomeCanvasPixbuf",
			sizeof (GnomeCanvasPixbuf),
			sizeof (GnomeCanvasPixbufClass),
			(GtkClassInitFunc) gnome_canvas_pixbuf_class_init,
			(GtkObjectInitFunc) gnome_canvas_pixbuf_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		canvas_pixbuf_type = gtk_type_unique (gnome_canvas_item_get_type (),
						      &canvas_pixbuf_info);
	}

	return canvas_pixbuf_type;
}

/* Class initialization function for the pixbuf canvas item */
