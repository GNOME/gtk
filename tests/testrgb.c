/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "config.h"

#include <glib.h>

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */


/* Note: these #includes differ slightly from the testrgb.c file included
   in the GdkRgb release. */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

#undef GDK_DISABLE_DEPRECATED

#include "gtk/gtk.h"

static void
quit_func (GtkWidget *widget, gpointer dummy)
{
  gtk_main_quit ();
}

#define WIDTH 640
#define HEIGHT 400
#define NUM_ITERS 50

static void
testrgb_rgb_test (GtkWidget *drawing_area)
{
  guchar *buf;
  gint i, j;
  gint offset;
  guchar val;
  gdouble start_time, total_time;
  gint x, y;
  gboolean dither;
  int dith_max;
  GTimer *timer;
  GdkPixbuf *pixbuf;
  gboolean to_pixmap;
  
  buf = g_malloc (WIDTH * HEIGHT * 8);

  val = 0;
  for (j = 0; j < WIDTH * HEIGHT * 8; j++)
    {
      val = (val + ((val + (rand () & 0xff)) >> 1)) >> 1;
      buf[j] = val;
    }

  /* Let's warm up the cache, and also wait for the window manager
     to settle. */
  for (i = 0; i < NUM_ITERS; i++)
    {
      offset = (rand () % (WIDTH * HEIGHT * 3)) & -4;
      gdk_draw_rgb_image (drawing_area->window,
			  drawing_area->style->white_gc,
			  0, 0, WIDTH, HEIGHT,
			  GDK_RGB_DITHER_NONE,
			  buf + offset, WIDTH * 3);
    }

  if (gdk_rgb_ditherable ())
    dith_max = 2;
  else
    dith_max = 1;

  timer = g_timer_new ();
  for (dither = 0; dither < dith_max; dither++)
    {
      start_time = g_timer_elapsed (timer, NULL);
      for (i = 0; i < NUM_ITERS; i++)
	{
	  offset = (rand () % (WIDTH * HEIGHT * 3)) & -4;
	  gdk_draw_rgb_image (drawing_area->window,
			      drawing_area->style->white_gc,
			      0, 0, WIDTH, HEIGHT,
			      dither ? GDK_RGB_DITHER_MAX :
			      GDK_RGB_DITHER_NONE,
			      buf + offset, WIDTH * 3);
	}
      gdk_flush ();
      total_time = g_timer_elapsed (timer, NULL) - start_time;
      g_print ("Color test%s time elapsed: %.2fs, %.1f fps, %.2f megapixels/s\n",
	       dither ? " (dithered)" : "",
	       total_time,
	       NUM_ITERS / total_time,
	       NUM_ITERS * (WIDTH * HEIGHT * 1e-6) / total_time);
    }

  for (dither = 0; dither < dith_max; dither++)
    {
      start_time = g_timer_elapsed (timer, NULL);
      for (i = 0; i < NUM_ITERS; i++)
	{
	  offset = (rand () % (WIDTH * HEIGHT)) & -4;
	  gdk_draw_gray_image (drawing_area->window,
			       drawing_area->style->white_gc,
			       0, 0, WIDTH, HEIGHT,
			       dither ? GDK_RGB_DITHER_MAX :
			       GDK_RGB_DITHER_NONE,
			       buf + offset, WIDTH);
	}
      gdk_flush ();
      total_time = g_timer_elapsed (timer, NULL) - start_time;
      g_print ("Grayscale test%s time elapsed: %.2fs, %.1f fps, %.2f megapixels/s\n",
	       dither ? " (dithered)" : "",
	       total_time,
	       NUM_ITERS / total_time,
	       NUM_ITERS * (WIDTH * HEIGHT * 1e-6) / total_time);
    }

  for (to_pixmap = FALSE; to_pixmap <= TRUE; to_pixmap++)
    {
      if (to_pixmap)
	{
	  GdkRectangle rect = { 0, 0, WIDTH, HEIGHT };
	  gdk_window_begin_paint_rect (drawing_area->window, &rect);
	}
	
      start_time = g_timer_elapsed (timer, NULL);
      for (i = 0; i < NUM_ITERS; i++)
	{
          cairo_t *cr;

	  offset = (rand () % (WIDTH * HEIGHT * 4)) & -4;
	  pixbuf = gdk_pixbuf_new_from_data (buf + offset, GDK_COLORSPACE_RGB, TRUE,
					     8, WIDTH, HEIGHT, WIDTH * 4,
					     NULL, NULL);
          cr = gdk_cairo_create (drawing_area->window);
          gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
          cairo_rectangle (cr, 0, 0, WIDTH, HEIGHT);
          cairo_fill (cr);
          cairo_destroy (cr);
	  g_object_unref (pixbuf);
	}
      gdk_flush ();
      total_time = g_timer_elapsed (timer, NULL) - start_time;

      if (to_pixmap)
	gdk_window_end_paint (drawing_area->window);
      
      g_print ("Alpha test%s time elapsed: %.2fs, %.1f fps, %.2f megapixels/s\n",
	       to_pixmap ? " (to pixmap)" : "",
	       total_time,
	       NUM_ITERS / total_time,
	       NUM_ITERS * (WIDTH * HEIGHT * 1e-6) / total_time);
    }
      
  g_print ("Please submit these results to http://www.levien.com/gdkrgb/survey.html\n");

#if 1
  for (x = 0; x < WIDTH; x++)
    {
      int cindex;

      cindex = (x * 8) / WIDTH;
      buf[x * 3] = cindex & 4 ? 0 : 255;
      buf[x * 3 + 1] = cindex & 2 ? 0 : 255;
      buf[x * 3 + 2] = cindex & 1 ? 0 : 255;
    }
  for (y = 1; y < (HEIGHT * 19) / 32; y++)
    {
      memcpy (buf + y * WIDTH * 3, buf, WIDTH * 3);
    }
  for (; y < (HEIGHT * 20) / 32; y++)
    {
      for (x = 0; x < WIDTH; x++)
	{
	  guchar gray;

	  gray = (x * 255) / (WIDTH - 1);
	  buf[y * WIDTH * 3 + x * 3] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 1] = 0;
	  buf[y * WIDTH * 3 + x * 3 + 2] = 0;
	}
    }
  for (; y < (HEIGHT * 21) / 32; y++)
    {
      for (x = 0; x < WIDTH; x++)
	{
	  guchar gray;

	  gray = (x * 255) / (WIDTH - 1);
	  buf[y * WIDTH * 3 + x * 3] = 0;
	  buf[y * WIDTH * 3 + x * 3 + 1] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 2] = 0;
	}
    }
  for (; y < (HEIGHT * 22) / 32; y++)
    {
      for (x = 0; x < WIDTH; x++)
	{
	  guchar gray;

	  gray = (x * 255) / (WIDTH - 1);
	  buf[y * WIDTH * 3 + x * 3] = 0;
	  buf[y * WIDTH * 3 + x * 3 + 1] = 0;
	  buf[y * WIDTH * 3 + x * 3 + 2] = gray;
	}
    }
  for (; y < (HEIGHT * 24) / 32; y++)
    {
      for (x = 0; x < WIDTH; x++)
	{
	  guchar gray;

	  gray = 112 + (x * 31) / (WIDTH - 1);
	  buf[y * WIDTH * 3 + x * 3] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 1] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 2] = gray;
	}
    }
  for (; y < (HEIGHT * 26) / 32; y++)
    {
      for (x = 0; x < WIDTH; x++)
	{
	  guchar gray;

	  gray = (x * 255) / (WIDTH - 1);
	  buf[y * WIDTH * 3 + x * 3] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 1] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 2] = gray;
	}
    }

  for (; y < HEIGHT; y++)
    {
      for (x = 0; x < WIDTH; x++)
	{
	  int cindex;
	  guchar gray;

	  cindex = (x * 16) / WIDTH;
	  gray = cindex < 3 ? 0 :
	    cindex < 5 ? 255 :
	    cindex < 7 ? 128 :
	    0;
	  buf[y * WIDTH * 3 + x * 3] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 1] = gray;
	  buf[y * WIDTH * 3 + x * 3 + 2] = gray;
	}
    }
  gdk_draw_rgb_image (drawing_area->window,
		      drawing_area->style->white_gc,
		      0, 0, WIDTH, HEIGHT, GDK_RGB_DITHER_MAX,
		      buf, WIDTH * 3);
#endif
}

void
new_testrgb_window (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *drawing_area;

  window = g_object_new (gtk_window_get_type (),
			   "GtkObject::user_data", NULL,
			   "GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			   "GtkWindow::title", "testrgb",
			   "GtkWindow::allow_shrink", FALSE,
			   NULL);
  g_signal_connect (window, "destroy",
		    G_CALLBACK (quit_func), NULL);

  vbox = gtk_vbox_new (FALSE, 0);

  drawing_area = gtk_drawing_area_new ();

  gtk_widget_set_size_request (drawing_area, WIDTH, HEIGHT);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area, FALSE, FALSE, 0);
  gtk_widget_show (drawing_area);

  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy), window);

  gtk_widget_show (button);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  gtk_widget_show (window);

  testrgb_rgb_test (drawing_area);
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  gdk_rgb_set_verbose (TRUE);

  gtk_widget_set_default_colormap (gdk_rgb_get_colormap ());
  new_testrgb_window ();

  gtk_main ();

  return 0;
}
