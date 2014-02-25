/* GdkPixbuf library - Scaling and compositing demo
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gimp.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdlib.h>
#include <gtk/gtk.h>
#include <math.h>



#define BACKGROUND_NAME "background.jpg"

static const char *image_names[] = {
	"apple-red.png",
	"gnome-applets.png",
	"gnome-calendar.png",
	"gnome-foot.png",
	"gnome-gmush.png",
	"gnome-gimp.png",
	"gnome-gsame.png",
	"gnu-keys.png"
};

#define N_IMAGES (sizeof (image_names) / sizeof (image_names[0]))

/* Current frame */
static GdkPixbuf *frame;

/* Background image */
static GdkPixbuf *background;
static int back_width, back_height;

/* Images */
static GdkPixbuf *images[N_IMAGES];

/* Widgets */
static GtkWidget *da;



/* Loads the images for the demo and returns whether the operation succeeded */
static gboolean
load_pixbufs (void)
{
	int i;

        /* We pass NULL for the error return location, we don't care
         * about the error message.
         */
        
	background = gdk_pixbuf_new_from_file (BACKGROUND_NAME, NULL);
	if (!background)
		return FALSE;

	back_width = gdk_pixbuf_get_width (background);
	back_height = gdk_pixbuf_get_height (background);

	for (i = 0; i < N_IMAGES; i++) {
		images[i] = gdk_pixbuf_new_from_file (image_names[i], NULL);
		if (!images[i])
			return FALSE;
	}

	return TRUE;
}

/* Expose callback for the drawing area */
static gboolean
draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
        gdk_cairo_set_source_pixbuf (cr, frame, 0, 0);
        cairo_paint (cr);

	return TRUE;
}

#define CYCLE_TIME 3000000 /* 3 seconds */

static gint64 start_time;

/* Handler to regenerate the frame */
static gboolean
on_tick (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       data)
{
	gint64	current_time;
	double f;
	int i;
	double xmid, ymid;
	double radius;

	gdk_pixbuf_copy_area (background, 0, 0, back_width, back_height,
			      frame, 0, 0);

        if (start_time == 0)
          start_time = gdk_frame_clock_get_frame_time (frame_clock);

	current_time = gdk_frame_clock_get_frame_time (frame_clock);
	f = ((current_time - start_time) % CYCLE_TIME) / (double)CYCLE_TIME;

	xmid = back_width / 2.0;
	ymid = back_height / 2.0;

	radius = MIN (xmid, ymid) / 2.0;

	for (i = 0; i < N_IMAGES; i++) {
		double ang;
		int xpos, ypos;
		int iw, ih;
		double r;
		GdkRectangle r1, r2, dest;
		double k;

		ang = 2.0 * G_PI * (double) i / N_IMAGES - f * 2.0 * G_PI;

		iw = gdk_pixbuf_get_width (images[i]);
		ih = gdk_pixbuf_get_height (images[i]);

		r = radius + (radius / 3.0) * sin (f * 2.0 * G_PI);

		xpos = floor (xmid + r * cos (ang) - iw / 2.0 + 0.5);
		ypos = floor (ymid + r * sin (ang) - ih / 2.0 + 0.5);

		k = (i & 1) ? sin (f * 2.0 * G_PI) : cos (f * 2.0 * G_PI);
		k = 2.0 * k * k;
		k = MAX (0.25, k);

		r1.x = xpos;
		r1.y = ypos;
		r1.width = iw * k;
		r1.height = ih * k;

		r2.x = 0;
		r2.y = 0;
		r2.width = back_width;
		r2.height = back_height;

		if (gdk_rectangle_intersect (&r1, &r2, &dest))
			gdk_pixbuf_composite (images[i],
					      frame,
					      dest.x, dest.y,
					      dest.width, dest.height,
					      xpos, ypos,
					      k, k,
					      GDK_INTERP_NEAREST,
					      ((i & 1)
					       ? MAX (127, fabs (255 * sin (f * 2.0 * G_PI)))
					       : MAX (127, fabs (255 * cos (f * 2.0 * G_PI)))));
	}

	gtk_widget_queue_draw (da);

	return G_SOURCE_CONTINUE;
}

/* Destroy handler for the window */
static void
destroy_cb (GObject *object, gpointer data)
{
	gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	if (!load_pixbufs ()) {
		g_message ("main(): Could not load all the pixbufs!");
		exit (EXIT_FAILURE);
	}

	frame = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, back_width, back_height);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_widget_set_size_request (window, back_width, back_height);

	g_signal_connect (window, "destroy",
			  G_CALLBACK (destroy_cb), NULL);

	da = gtk_drawing_area_new ();

	g_signal_connect (da, "draw",
			  G_CALLBACK (draw_cb), NULL);

	gtk_container_add (GTK_CONTAINER (window), da);

	gtk_widget_add_tick_callback (da, on_tick, NULL, NULL);

	gtk_widget_show_all (window);
	gtk_main ();

	return 0;
}
