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
#include "gdk-pixbuf.h"
#include "gnome-canvas-pixbuf.h"



/* Private part of the GnomeCanvasPixbuf structure */
typedef struct {
	/* Our gdk-pixbuf */
	GdkPixbuf *pixbuf;

	/* Width value */
	double width;

	/* Height value */
	double height;

	/* Whether dimensions are set and whether they are in pixels or units */
	guint width_set : 1;
	guint width_pixels : 1;
	guint height_set : 1;
	guint height_pixels : 1;
} PixbufPrivate;



/* Object argument IDs */
enum {
	ARG_0,
	ARG_PIXBUF,
	ARG_WIDTH,
	ARG_WIDTH_SET,
	ARG_WIDTH_PIXELS,
	ARG_HEIGHT,
	ARG_HEIGHT_SET,
	ARG_HEIGHT_PIXELS
};

static void gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class);
static void gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *cpb);
static void gnome_canvas_pixbuf_destroy (GtkObject *object);
static void gnome_canvas_pixbuf_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gnome_canvas_pixbuf_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

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
static void
gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("GnomeCanvasPixbuf::pixbuf",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_PIXBUF);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::width",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::width_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WIDTH_SET);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::width_pixels",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WIDTH_PIXELS);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::height",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEIGHT);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::height_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HEIGHT_SET);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::height_pixels",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HEIGHT_PIXELS);

	object_class->destroy = gnome_canvas_pixbuf_destroy;
	object_class->set_arg = gnome_canvas_pixbuf_set_arg;
	object_class->get_arg = gnome_canvas_pixbuf_get_arg;

	item_class->update = gnome_canvas_pixbuf_update;
	item_class->draw = gnome_canvas_pixbuf_draw;
	item_class->point = gnome_canvas_pixbuf_point;
	item_class->bounds = gnome_canvas_pixbuf_bounds;
}

/* Object initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *gcp)
{
	PixbufPrivate *priv;

	priv = g_new0 (PixbufPrivate, 1);
	gcp->priv = priv;

	priv->width = 0.0;
	priv->height = 0.0;
}

static void
gnome_canvas_pixbuf_destroy (GtkObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	
}
