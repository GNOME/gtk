#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>
#include <stdio.h>

GtkWidget **images;
GtkWidget **vbox;

void
hello (GtkWidget * button, char *label)
{
  g_print ("Click from %s\n", label);
}

void
show_hide (GtkWidget * button, int num_screen)
{
  static gboolean visible = TRUE;
  if (visible)
    {
      gtk_widget_hide (images [num_screen]);
      gtk_button_set_label (GTK_BUTTON (button), "Show Icon");
      visible = FALSE;
    }else{
      gtk_widget_show (images [num_screen]);
      gtk_button_set_label (GTK_BUTTON (button), "Hide Icon");
      visible = TRUE;
    }
}
void
move (GtkWidget * button, GtkVBox *vbox)
{
  GdkScreen *screen = gtk_widget_get_screen (button);
  GtkWidget *toplevel = gtk_widget_get_toplevel (button);
  GtkWidget *new_toplevel;  
  GdkDisplay *display = gdk_screen_get_display (screen);
  int number_of_screens = gdk_display_get_n_screens (display);
  int screen_num = gdk_screen_get_number (screen);
  g_print ("This button is on screen %d\n", gdk_screen_get_number (screen));
  if (toplevel) 
    {
      new_toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      if ((screen_num +1) < number_of_screens)
	gtk_window_set_screen (GTK_WINDOW (new_toplevel), 
			       gdk_display_get_screen (display,
						       screen_num + 1));
      else
	gtk_window_set_screen (GTK_WINDOW (new_toplevel), 
			       gdk_display_get_screen (display, 0));

      gtk_widget_reparent (GTK_WIDGET (vbox), new_toplevel);
      gtk_widget_unrealize (toplevel);
      gtk_widget_show_all (new_toplevel);
    }else{
      g_error ("toplevel is NULL\n");
    }
}


int
main (int argc, char *argv[])
{
  GtkWidget **window;
  GtkWidget **button;
  GtkWidget *moving_window, *moving_button, *moving_vbox, *moving_image;
  gint num_screen = 0;
  gchar *displayname = NULL;
  gint i;
  GdkScreen **screen_list;
  GdkDisplay *dpy;
  GSList *ids;
  
  gtk_init (&argc, &argv);

  dpy = gdk_get_default_display ();
  num_screen = gdk_display_get_n_screens (dpy);
  displayname = g_strdup (gdk_display_get_name (dpy));
  if (num_screen <= 1)
    {
      printf ("This Xserver (%s) manages only one screen. exiting...\n",
	      displayname);
      exit (1);
    }
  else
    {
      printf ("This Xserver (%s) manages %d screens.\n", displayname,
	      num_screen);
    }
  screen_list = g_new (GdkScreen *, num_screen);
  window = g_new (GtkWidget *, num_screen);
  button = g_new (GtkWidget *, num_screen);
  images = g_new (GtkWidget *, num_screen);
  vbox = g_new (GtkWidget *, num_screen);

  ids = gtk_stock_list_ids ();

  for (i = 0; i < num_screen; i++)
    {
      char *label = g_new (char, 10);
      screen_list[i] = gdk_display_get_screen (dpy, i);
      sprintf (label, "Screen %d", i);

      window[i] = g_object_connect (gtk_widget_new (gtk_window_get_type (),
						    "screen", screen_list[i],
						    "user_data", NULL,
						    "type",
						    GTK_WINDOW_TOPLEVEL,
						    "title", label,
						    "allow_grow", FALSE,
						    "allow_shrink", FALSE,
						    "border_width", 10, NULL),
				    "signal::destroy", gtk_main_quit, NULL,
				    NULL);

      vbox[i] = gtk_vbox_new (TRUE, 0);

      gtk_container_add (GTK_CONTAINER (window[i]), vbox[i]);
      /* g_print ("i = %d, stock icon %s\n",i+1 , (char *)g_slist_nth (ids, i+1)->data); */

      images [i] = gtk_image_new_from_stock (g_slist_nth (ids, i+1)->data,
					     GTK_ICON_SIZE_BUTTON);

      
      button[i] = 
	g_object_connect (gtk_widget_new
			  (gtk_button_get_type (), "GtkButton::label", label,
			   "GtkWidget::parent", vbox[i],
			   "GtkWidget::visible", TRUE, NULL),
			  "signal::clicked", hello, label, NULL);
      
      gtk_container_add (GTK_CONTAINER (vbox[i]), images[i]);
      
      g_object_connect (gtk_widget_new
			  (gtk_button_get_type (), "GtkButton::label", "Hide Icon",
			   "GtkWidget::parent", vbox[i],
			   "GtkWidget::visible", TRUE, NULL),
			  "signal::clicked", show_hide, i, NULL);
      
    }
  for (i = 0; i < num_screen; i++)
    gtk_widget_show_all (window[i]);

  moving_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  moving_vbox = gtk_vbox_new (TRUE, 0);

  gtk_container_add (GTK_CONTAINER (moving_window), moving_vbox);
  moving_button = gtk_widget_new ( gtk_button_get_type (),
				       "GtkButton::label", "Move to Next Screen",
				       "GtkWidget::visible", TRUE, NULL);
  
  g_signal_connect ( G_OBJECT (moving_button), "clicked", 
		     G_CALLBACK (move), moving_vbox);
  g_signal_connect ( G_OBJECT (moving_window), "destroy", 
		     G_CALLBACK (gtk_main_quit), NULL);

  gtk_container_add (GTK_CONTAINER (moving_vbox), moving_button);

  moving_image = gtk_image_new_from_stock (g_slist_nth (ids, num_screen + 2)->data,
					   GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (moving_vbox), moving_image);
  gtk_widget_show_all (moving_window);

  gtk_main ();

  return 0;
}
