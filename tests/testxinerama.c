#include <config.h>
#include <stdlib.h>
#include <gtk/gtk.h>

static gint num_monitors;

static void
request (GtkWidget      *widget,
	 GdkEventMotion *event,
	 gpointer        user_data)
{
  gchar *str;
  gint i = gdk_screen_get_monitor_at_window (gtk_widget_get_screen (widget),
					     widget->window);

  if (i < 0)
    str = g_strdup ("<big><span foreground='white' background='black'>Not on a monitor </span></big>");
  else
    {
      GdkRectangle monitor;
      gdk_screen_get_monitor_geometry (gtk_widget_get_screen (widget), i, &monitor);
      str = g_strdup_printf ("<big><span foreground='white' background='black'>"
			     "Monitor %d of %d</span></big>\n"
			     "<i>Width - Height       </i>: (%d,%d)\n"
			     "<i>Top left coordinate </i>: (%d,%d)",i+1, num_monitors,
			     monitor.width, monitor.height, monitor.x, monitor.y);
    }
  
  gtk_label_set_markup (GTK_LABEL (user_data), str);
  g_free (str);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label, *vbox, *button;
  GdkScreen *screen;
  gint i;

  gtk_init (&argc, &argv);

  screen = gdk_screen_get_default ();

  num_monitors = gdk_screen_get_n_monitors (screen);
  if (num_monitors == 1)
    g_warning ("The default screen of the current display only has one monitor.");
  
  for (i=0; i<num_monitors; i++)
    {
      GdkRectangle monitor; 
      gchar *str;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gdk_screen_get_monitor_geometry (screen, i, &monitor);
      gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
      gtk_window_move (GTK_WINDOW (window), (monitor.width - 200) / 2 + monitor.x,
		       (monitor.height - 200) / 2 + monitor.y);
      
      label = gtk_label_new (NULL);
      str = g_strdup_printf ("<big><span foreground='white' background='black'>"
			     "Monitor %d of %d</span></big>\n"
			     "<i>Width - Height       </i>: (%d,%d)\n"
			     "<i>Top left coordinate </i>: (%d,%d)",i+1, num_monitors,
			     monitor.width, monitor.height, monitor.x, monitor.y);
      gtk_label_set_markup (GTK_LABEL (label), str);
      g_free (str);
      button = gtk_button_new_with_label ("Close");
      g_signal_connect (button, "clicked", G_CALLBACK (gtk_main_quit), NULL);
      g_signal_connect (window, "configure-event", G_CALLBACK (request), label);
      vbox = gtk_vbox_new (TRUE, 1);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      gtk_container_add (GTK_CONTAINER (vbox), label);
      gtk_container_add (GTK_CONTAINER (vbox), button);
      gtk_widget_show_all (window);
    }
  
  gtk_main ();

  return 0;
}
