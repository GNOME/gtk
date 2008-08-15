/* This file is part of GTK+
 *
 * AUTHORS
 *     Sven Herzberg
 *
 * Copyright (C) 2008  Sven Herzberg
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib/gstdio.h>
#include <gdk/gdk.h>
#ifdef CAIRO_HAS_QUARTZ_SURFACE
#include <cairo-quartz.h>
#endif

static void
test (cairo_t* cr)
{
	cairo_save (cr);
	 cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	 cairo_paint (cr);
	cairo_restore (cr);

	cairo_move_to (cr, 10.0, 20.0);
	cairo_line_to (cr, 10.0, 30.0);
	cairo_stroke (cr);

	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
	cairo_arc (cr, 0.0, 0.0, 10.0, 0.0, G_PI/2);
	cairo_stroke (cr);
}

static void
test_pixmap_orientation (void)
{
	GdkPixmap* pixmap;
	GdkPixbuf* pixbuf;
	GdkPixbuf* pbuf_platform;
	GdkPixbuf* pbuf_imagesrf;
	GError   * error = NULL;
	cairo_surface_t* surface;
	cairo_t* cr;
	guchar* data_platform;
	guchar* data_imagesrf;
	guint i;

	/* create "platform.png" via GdkPixmap */
	pixmap = gdk_pixmap_new (NULL /* drawable */, 100 /* w */, 80 /* h */, 24 /* d */);
	cr = gdk_cairo_create (pixmap);
	test (cr);
	cairo_destroy (cr);

	pixbuf = gdk_pixbuf_get_from_drawable (NULL,
					       pixmap,
					       gdk_rgb_get_colormap (),
					       0, 0,
					       0, 0,
					       100, 80);
	if (!gdk_pixbuf_save (pixbuf, "gdksurface.png", "png", NULL, NULL)) {
		g_error ("Eeek! Couldn't save the file \"gdksurface.png\"");
	}
	g_object_unref (pixbuf);

	g_object_unref (pixmap);

	/* create "cairosurface.png" via pure cairo */
#ifndef CAIRO_HAS_QUARTZ_SURFACE
	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 100, 80);
#else
	surface = cairo_quartz_surface_create (CAIRO_FORMAT_RGB24, 100, 80);
#endif
	cr = cairo_create (surface);
	test (cr);
	cairo_destroy (cr);
	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png (surface, "cairosurface.png")) {
		g_error ("Eeek! Couldn't save the file \"cairosurface.png\"");
	}
	cairo_surface_destroy (surface);

	/* compare the images */
	pbuf_platform = gdk_pixbuf_new_from_file ("gdksurface.png", &error);
	if (!pbuf_platform || error) {
		g_error ("Eeek! Error loading \"gdksurface.png\"");
	}
	pbuf_imagesrf = gdk_pixbuf_new_from_file ("cairosurface.png", &error);
	if (!pbuf_imagesrf || error) {
		g_object_unref (pbuf_platform);
		g_error ("Eeek! Error loading \"cairosurface.png\"");
	}

	g_assert (gdk_pixbuf_get_width (pbuf_platform) ==
		  gdk_pixbuf_get_width (pbuf_imagesrf));
	g_assert (gdk_pixbuf_get_height (pbuf_platform) ==
		  gdk_pixbuf_get_height (pbuf_imagesrf));
	g_assert (gdk_pixbuf_get_rowstride (pbuf_platform) ==
		  gdk_pixbuf_get_rowstride (pbuf_imagesrf));
	g_assert (gdk_pixbuf_get_n_channels (pbuf_platform) ==
		  gdk_pixbuf_get_n_channels (pbuf_imagesrf));

	data_platform = gdk_pixbuf_get_pixels (pbuf_platform);
	data_imagesrf = gdk_pixbuf_get_pixels (pbuf_imagesrf);

	for (i = 0; i < gdk_pixbuf_get_height (pbuf_platform) * gdk_pixbuf_get_rowstride (pbuf_platform); i++) {
		if (data_platform[i] != data_imagesrf[i]) {
			g_warning ("Eeek! Images are differing at byte %d", i);
			g_object_unref (pbuf_platform);
			g_object_unref (pbuf_imagesrf);
			g_assert_not_reached ();
		}
	}

	g_object_unref (pbuf_platform);
	g_object_unref (pbuf_imagesrf);

	g_unlink ("gdksurface.png");
	g_unlink ("cairosurface.png");
}

int
main (int   argc,
      char**argv)
{
	g_test_init (&argc, &argv, NULL);
	gdk_init (&argc, &argv);

	g_test_add_func ("/gdk/pixmap/orientation",
			 test_pixmap_orientation);

	return g_test_run ();
}

