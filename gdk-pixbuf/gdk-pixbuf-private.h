/* GdkPixbuf library - Private declarations
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Havoc Pennington <hp@redhat.com>
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

#ifndef GDK_PIXBUF_PRIVATE_H
#define GDK_PIXBUF_PRIVATE_H

#include "gdk-pixbuf.h"



typedef struct _GdkPixbufClass GdkPixbufClass;

#define GDK_PIXBUF_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF, GdkPixbufClass))
#define GDK_IS_PIXBUF_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF))
#define GDK_PIXBUF_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF, GdkPixbufClass))

/* Private part of the GdkPixbuf structure */
struct _GdkPixbuf {
        GObject parent_instance;

	/* Color space */
	GdkColorspace colorspace;

	/* Number of channels, alpha included */
	int n_channels;

	/* Bits per channel */
	int bits_per_sample;

	/* Size */
	int width, height;

	/* Offset between rows */
	int rowstride;

	/* The pixel array */
	guchar *pixels;

	/* Destroy notification function; it is supposed to free the pixel array */
	GdkPixbufDestroyNotify destroy_fn;

	/* User data for the destroy notification function */
	gpointer destroy_fn_data;

	/* Do we have an alpha channel? */
	guint has_alpha : 1;
};

struct _GdkPixbufClass {
        GObjectClass parent_class;

};

/* Private part of the GdkPixbufFrame structure */
struct _GdkPixbufFrame {
	/* The pixbuf with this frame's image data */
	GdkPixbuf *pixbuf;

	/* Offsets for overlaying onto the animation's area */
	int x_offset;
	int y_offset;

	/* Frame duration in ms */
	int delay_time;

	/* Overlay mode */
	GdkPixbufFrameAction action;
};

typedef struct _GdkPixbufAnimationClass GdkPixbufAnimationClass;

#define GDK_PIXBUF_ANIMATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_ANIMATION, GdkPixbufAnimationClass))
#define GDK_IS_PIXBUF_ANIMATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_ANIMATION))
#define GDK_PIXBUF_ANIMATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_ANIMATION, GdkPixbufAnimationClass))

/* Private part of the GdkPixbufAnimation structure */
struct _GdkPixbufAnimation {
        GObject parent_instance;

	/* Number of frames */
        int n_frames;

	/* List of GdkPixbufFrame structures */
        GList *frames;

	/* bounding box size */
	int width, height;
};

struct _GdkPixbufAnimationClass {
        GObjectClass parent_class;


};



#endif
