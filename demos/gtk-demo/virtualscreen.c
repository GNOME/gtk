/* Multihead Support/Virtual Screen Demo
 *
 * Demonstrates possible use of virtual screen information available when
 * a screen is made of multiple physical monitor screen.
 */

#include <stdio.h>
#include <gtk/gtk.h>
#include "demo-common.h"
#include "x11/gdkx.h"

static void
virtualscreen_close (GtkWidget * widget)
{
  GtkWidget **window = get_cached_pointer (widget, "do_virtualscreen");
  if (window)
    {
      gint j = gdk_screen_get_num_monitors (gtk_widget_get_screen (widget));
      j--;
      cache_pointer (widget, "do_virtualscreen", NULL);
      while (j >= 0)
	{
	  if (window[j])
	    {
	      gtk_widget_destroy (window[j]);
	      window[j] = NULL;
	    }
	  j--;
	}
      if (window)
	g_free (window);
    }
}

static void
virtualscreen_request (GtkWidget * widget,
		       GdkEventMotion * event, gpointer user_data)
{

  gchar *str = g_new0 (gchar, 300);
  GdkScreen *screen = gtk_widget_get_screen (widget);

  gint i = gdk_screen_get_monitor_num_at_window
    (screen, GDK_WINDOW_XWINDOW (widget->window));

  GdkRectangle *monitor = gdk_screen_get_monitor_geometry (screen, i);

  sprintf (str, "<big><span foreground=\"white\" background=\"black\">"
	   "Screen %d of %d</span></big>\n"
	   "<i>Width - Height       </i>: (%d,%d)\n"
	   "<i>Top left coordinate </i>: (%d,%d)", i + 1,
	   gdk_screen_get_num_monitors (screen),
	   monitor->width, monitor->height, monitor->x, monitor->y);
  gtk_label_set_markup (GTK_LABEL (user_data), str);
}

GtkWidget *
do_virtualscreen (GtkWidget * do_widget)
{
  GtkWidget *label, *vbox, *button;
  GdkScreen *screen;
  gint num_monitors;
  gint i;
  GtkWidget **window = get_cached_pointer (do_widget, "do_virtualscreen");

  screen = gtk_widget_get_screen (do_widget);

  if (!gdk_screen_use_virtual_screen (screen))
    {
      GtkWidget *dialog;
      GtkWidget *w = gtk_widget_get_toplevel (do_widget);

      dialog = gtk_message_dialog_new (GTK_WINDOW (w),
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "The current display is not "
				       "supporting Virtual screen Mode");

      g_signal_connect (dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (dialog);
      return dialog;
    }

  num_monitors = gdk_screen_get_num_monitors (screen);

  if (num_monitors == 1)
    {
      GtkWidget *dialog;
      GtkWidget *w = gtk_widget_get_toplevel (do_widget);

      dialog = gtk_message_dialog_new (GTK_WINDOW (w),
				       GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "The current display is supporting "
				       "Virtual screen Mode but has only "
				       "one monitor, Strange...");

      g_signal_connect (dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (dialog);
      return dialog;
    }

  if (!window)
    {
      window = g_new (GtkWidget *, num_monitors);
      cache_pointer (do_widget, "do_virtualscreen", window);

      for (i = 0; i < num_monitors; i++)
	{
	  gchar str[300];

	  GdkRectangle *monitor = gdk_screen_get_monitor_geometry (screen, i);

	  window[i] = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	  gtk_window_set_screen (GTK_WINDOW (window [i]), screen);

	  gtk_window_set_default_size (GTK_WINDOW (window[i]), 200, 200);
	  gtk_window_move (GTK_WINDOW (window[i]),
			   (monitor->width - 200) / 2 + monitor->x,
			   (monitor->height - 200) / 2 + monitor->y);

	  label = gtk_label_new (NULL);
	  sprintf (str,
		   "<big><span foreground=\"white\" background=\"black\">"
		   "Screen %d of %d</span></big>\n"
		   "<i>Width - Height       </i>: (%d,%d)\n"
		   "<i>Top left coordinate </i>: (%d,%d)", i + 1,
		   num_monitors, monitor->width, monitor->height, monitor->x,
		   monitor->y);
	  gtk_label_set_markup (GTK_LABEL (label), str);
	  button = gtk_button_new_with_label ("Close");
	  g_signal_connect (G_OBJECT (button), "clicked",
			    G_CALLBACK (virtualscreen_close), NULL);
	  /* check if the window is not a different monitor */
	  g_signal_connect (G_OBJECT (window[i]), "configure-event",
			    G_CALLBACK (virtualscreen_request), label);
	  g_signal_connect (G_OBJECT (window[i]), "destroy",
			    G_CALLBACK (virtualscreen_close), NULL);

	  vbox = gtk_vbox_new (TRUE, 1);
	  gtk_container_add (GTK_CONTAINER (window[i]), vbox);
	  gtk_container_add (GTK_CONTAINER (vbox), label);
	  gtk_container_add (GTK_CONTAINER (vbox), button);
	  gtk_widget_show_all (window[i]);
	}
      return window[0];
    }
  else
    {
      virtualscreen_close (do_widget);
      return NULL;
    }
}
