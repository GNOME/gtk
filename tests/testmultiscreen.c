/* testmultidisplay.c
 * Copyright (C) 2001 Sun Microsystems Inc.
 * Author: Erwann Chenede <erwann.chenede@sun.com>
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

#include "config.h"
#include <gtk/gtk.h>

static GtkWidget **images;
static GtkWidget **vbox;

static void
hello (GtkWidget * button, char *label)
{
  g_print ("Click from %s\n", label);
}

static void
show_hide (GtkWidget * button, gpointer data)
{
  gint num_screen = GPOINTER_TO_INT (data);
    
  static gboolean visible = TRUE;
  if (visible)
    {
      gtk_widget_hide (images[num_screen]);
      gtk_button_set_label (GTK_BUTTON (button), "Show Icon");
      visible = FALSE;
    }
  else
    {
      gtk_widget_show (images[num_screen]);
      gtk_button_set_label (GTK_BUTTON (button), "Hide Icon");
      visible = TRUE;
    }
}

static void
move (GtkWidget *button, GtkVBox *vbox)
{
  GdkScreen *screen = gtk_widget_get_screen (button);
  GtkWidget *toplevel = gtk_widget_get_toplevel (button);
  GtkWidget *new_toplevel;  
  GdkDisplay *display = gdk_screen_get_display (screen);
  gint number_of_screens = gdk_display_get_n_screens (display);
  gint screen_num = gdk_screen_get_number (screen);
  
  g_print ("This button is on screen %d\n", gdk_screen_get_number (screen));
  
  new_toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  if ((screen_num +1) < number_of_screens)
    gtk_window_set_screen (GTK_WINDOW (new_toplevel), 
			   gdk_display_get_screen (display,
						   screen_num + 1));
  else
    gtk_window_set_screen (GTK_WINDOW (new_toplevel), 
			   gdk_display_get_screen (display, 0));
  
  gtk_widget_reparent (GTK_WIDGET (vbox), new_toplevel);
  gtk_widget_destroy (toplevel);
  gtk_widget_show_all (new_toplevel);
}


int
main (int argc, char *argv[])
{
  GtkWidget **window;
  GtkWidget *moving_window, *moving_button, *moving_vbox, *moving_image;
  gint num_screen = 0;
  gchar *displayname = NULL;
  gint i;
  GdkScreen **screen_list;
  GdkDisplay *dpy;
  GSList *ids;
  
  gtk_init (&argc, &argv);

  dpy = gdk_display_get_default ();
  num_screen = gdk_display_get_n_screens (dpy);
  displayname = g_strdup (gdk_display_get_name (dpy));
  g_print ("This X Server (%s) manages %d screen(s).\n",
	   displayname, num_screen);
  screen_list = g_new (GdkScreen *, num_screen);
  window = g_new (GtkWidget *, num_screen);
  images = g_new (GtkWidget *, num_screen);
  vbox = g_new (GtkWidget *, num_screen);

  ids = gtk_stock_list_ids ();

  for (i = 0; i < num_screen; i++)
    {
      char *label = g_strdup_printf ("Screen %d", i);
      GtkWidget *button;
      
      screen_list[i] = gdk_display_get_screen (dpy, i);

      window[i] = g_object_new (GTK_TYPE_WINDOW,
				  "screen", screen_list[i],
				  "user_data", NULL,
				  "type", GTK_WINDOW_TOPLEVEL,
				  "title", label,
				  "allow_grow", FALSE,
				  "allow_shrink", FALSE,
				  "border_width", 10, NULL,
				  NULL);
      g_signal_connect (window[i], "destroy",
			G_CALLBACK (gtk_main_quit), NULL);

      vbox[i] = gtk_vbox_new (TRUE, 0);
      gtk_container_add (GTK_CONTAINER (window[i]), vbox[i]);

      button = g_object_new (GTK_TYPE_BUTTON,
			       "label", label,
			       "parent", vbox[i],
			       "visible", TRUE, NULL,
			       NULL);
      g_signal_connect (button, "clicked",
			G_CALLBACK (hello), label);
  
      images[i] = gtk_image_new_from_stock (g_slist_nth (ids, i+1)->data,
					     GTK_ICON_SIZE_BUTTON);
      
      gtk_container_add (GTK_CONTAINER (vbox[i]), images[i]);

      button = g_object_new (GTK_TYPE_BUTTON,
			       "label", "Hide Icon",
			       "parent", vbox[i],
			       "visible", TRUE, NULL,
			       NULL);
      g_signal_connect (button, "clicked",
			G_CALLBACK (show_hide), GINT_TO_POINTER (i));
    }
  
  for (i = 0; i < num_screen; i++)
    gtk_widget_show_all (window[i]);
  
  moving_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  moving_vbox = gtk_vbox_new (TRUE, 0);
  
  gtk_container_add (GTK_CONTAINER (moving_window), moving_vbox);
  moving_button = g_object_new (GTK_TYPE_BUTTON,
				  "label", "Move to Next Screen",
				  "visible", TRUE,
				  NULL);
  
  g_signal_connect (moving_button, "clicked", 
		    G_CALLBACK (move), moving_vbox);
  
  gtk_container_add (GTK_CONTAINER (moving_vbox), moving_button);
  
  moving_image = gtk_image_new_from_stock (g_slist_nth (ids, num_screen + 2)->data,
					   GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (moving_vbox), moving_image);
  gtk_widget_show_all (moving_window);

  gtk_main ();

  return 0;
}
