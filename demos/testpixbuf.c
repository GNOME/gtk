/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */


/* Note: these #includes differ slightly from the testrgb.c file included
   in the GdkRgb release. */

/* For gettimeofday */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gtk/gtk.h>
#include "gdk-pixbuf.h"

static void
quit_func (GtkWidget *widget, gpointer dummy)
{
  gtk_main_quit ();
}

expose_func (GtkWidget *drawing_area, GdkEventExpose *event, gpointer data)
{
  GdkPixBuf *pixbuf = (GdkPixBuf *)data;
  gint x1, y1, x2, y2;

  if (pixbuf->art_pixbuf->has_alpha){
    gdk_draw_rgb_32_image (drawing_area->window,
			   drawing_area->style->black_gc,
			   event->area.x, event->area.y, 
			   event->area.width, 
			   event->area.height,
			   GDK_RGB_DITHER_MAX, 
			   pixbuf->art_pixbuf->pixels 
			   + (event->area.y * pixbuf->art_pixbuf->rowstride) 
			   + (event->area.x * pixbuf->art_pixbuf->n_channels),
			   pixbuf->art_pixbuf->rowstride);
  }else{
    gdk_draw_rgb_image (drawing_area->window,
			drawing_area->style->white_gc,
			event->area.x, event->area.y, 
			event->area.width, 
			event->area.height,
			GDK_RGB_DITHER_NORMAL,
			pixbuf->art_pixbuf->pixels 
			+ (event->area.y * pixbuf->art_pixbuf->rowstride) 
			+ (event->area.x * pixbuf->art_pixbuf->n_channels),
			pixbuf->art_pixbuf->rowstride);
  }
}  

void
new_testrgb_window (GdkPixBuf *pixbuf)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *drawing_area;
  gint w, h;
 
  w = pixbuf->art_pixbuf->width;
  h = pixbuf->art_pixbuf->height;

  window = gtk_widget_new (gtk_window_get_type (),
			   "GtkObject::user_data", NULL,
			   "GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			   "GtkWindow::title", "testrgb",
			   "GtkWindow::allow_shrink", FALSE,
			   NULL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      (GtkSignalFunc) quit_func, NULL);

  vbox = gtk_vbox_new (FALSE, 0);

  drawing_area = gtk_drawing_area_new ();

  gtk_widget_set_usize (drawing_area, w, h);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT(drawing_area), "expose_event",
		      GTK_SIGNAL_FUNC (expose_func), pixbuf);

  gtk_widget_show (drawing_area);

  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) gtk_widget_destroy,
			     GTK_OBJECT (window));

  gtk_widget_show (button);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  gtk_widget_show (window);
}

int
main (int argc, char **argv)
{
  int i;
  int found_valid = FALSE; 

  GdkPixBuf *pixbuf;

  gtk_init (&argc, &argv);

  gdk_rgb_set_verbose (TRUE);

  gdk_rgb_init ();

  gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
  gtk_widget_set_default_visual (gdk_rgb_get_visual ());

  i = 1;
  for (i = 1; i < argc; i++)
    {
      if (argv[i])
	{
	  pixbuf = gdk_pixbuf_load_image (argv[i]);
   
	  if (pixbuf)
	    {
	      new_testrgb_window (pixbuf);
	      found_valid = TRUE;
	    }
	}
    }

  if (found_valid)
    gtk_main ();

  return 0;
}
