#include <stdio.h>
#include "x11/gdkx.h"
#include <gtk/gtk.h>

gint num_monitors;

void request (GtkWidget *widget,
	      GdkEventMotion *event,
	      gpointer user_data)
{
  
  gchar *str = g_new0 (gchar, 300);
  gint i = gdk_screen_get_monitor_num_at_window (gtk_widget_get_screen (widget),
						 GDK_WINDOW_XWINDOW (widget->window));
  GdkRectangle *monitor = gdk_screen_get_monitor_geometry (gtk_widget_get_screen (widget),
							   i);
  
  sprintf (str, "<big><span foreground=\"white\" background=\"black\">"
	   "Screen %d of %d</span></big>\n"
	   "<i>Width - Height       </i>: (%d,%d)\n"
	   "<i>Top left coordinate </i>: (%d,%d)",i+1, num_monitors,
	   monitor->width, monitor->height, monitor->x, monitor->y);  
  gtk_label_set_markup (GTK_LABEL (user_data), str);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label, *vbox, *button;
  GdkScreen *screen;
  gint i;

  gtk_init (&argc, &argv);

  screen = gdk_get_default_screen ();

  if (!gdk_screen_use_virtual_screen (screen))
    {
      g_print ("The current display is not supporting Xinerama\n");
      exit (1);
    }

  num_monitors = gdk_screen_get_num_monitors (screen);

  if (num_monitors == 1)
    {
      g_error ("The current display is supporting Xinerama but has only one monitor...");
      exit (1);
    }
  
  for (i=0; i<num_monitors; i++)
    {
      gchar *str = g_new0 (gchar, 300);
      
      GdkRectangle *monitor = gdk_screen_get_monitor_geometry (screen, i);
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
      gtk_window_move (GTK_WINDOW (window), (monitor->width - 200) / 2 + monitor->x,
		       (monitor->height - 200) / 2 + monitor->y);
      
      label = gtk_label_new (NULL);
      sprintf (str, "<big><span foreground=\"white\" background=\"black\">"
		    "Screen %d of %d</span></big>\n"
		    "<i>Width - Height       </i>: (%d,%d)\n"
		    "<i>Top left coordinate </i>: (%d,%d)",i+1, num_monitors,
		    monitor->width, monitor->height, monitor->x, monitor->y);
      gtk_label_set_markup (GTK_LABEL (label), str);
      button = gtk_button_new_with_label ("Close");
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (gtk_main_quit), NULL);
      g_signal_connect (G_OBJECT (window), "configure-event", G_CALLBACK (request), label);
      vbox = gtk_vbox_new (TRUE, 1);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      gtk_container_add (GTK_CONTAINER (vbox), label);
      gtk_container_add (GTK_CONTAINER (vbox), button);
      gtk_widget_show_all (window);
      
    }
  
  gtk_main ();

  return 0;
}
