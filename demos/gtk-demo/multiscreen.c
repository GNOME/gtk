/* Multihead Support/Multiple Screen demo
 *
 * Demonstrates an application displaying a window per screen  
 *
 */
#include <gtk/gtk.h>
#include <stdio.h>
#include "demo-common.h"

void
multiscreen_close_all (GtkWidget * widget)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GtkWidget **window = g_object_get_data (G_OBJECT (display),
					  "do_multiscreen");
  if (window)
    {
      gint j = gdk_display_get_n_screens (display);
      j--;
      g_object_set_data (G_OBJECT (display), "do_multiscreen", NULL);
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

GtkWidget *
do_multiscreen (GtkWidget * do_widget)
{
  gint i, num_screen;
  GdkDisplay *dpy = gtk_widget_get_display (do_widget);
  GtkWidget *vbox, *label, *button;
  GtkWidget **window = g_object_get_data (G_OBJECT (dpy), "do_multiscreen");

  if (!window)
    {
      num_screen = gdk_display_get_n_screens (dpy);

      if (num_screen <= 1)
	{
	  GtkWidget *dialog;
	  GtkWidget *w = gtk_widget_get_toplevel (do_widget);

	  dialog = gtk_message_dialog_new (GTK_WINDOW (w),
					   GTK_DIALOG_MODAL,
					   GTK_MESSAGE_ERROR,
					   GTK_BUTTONS_CLOSE,
					   "This display (%s) manages only one screen.",
					   gdk_display_get_name (dpy));

	  g_signal_connect (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy), NULL);

	  gtk_widget_show (dialog);
	  return dialog;
	}

      window = g_new (GtkWidget *, num_screen);
      g_object_set_data (G_OBJECT (dpy), "do_multiscreen", window);

      for (i = 0; i < num_screen; i++)
	{
	  char *label_text = g_new (char, 300);
	  GdkScreen *screen = gdk_display_get_screen (dpy, i);

	  window[i] = gtk_widget_new (gtk_window_get_type (), "screen",
				      screen,
				      "user_data", NULL, "type",
				      GTK_WINDOW_TOPLEVEL, "title", label,
				      "allow_grow", FALSE, "allow_shrink",
				      FALSE, "border_width", 10, NULL);
	  /* you can also use gtk_window_new and set the screen afterwards with 
	   * gtk_window_set_screen */

	  g_signal_connect (G_OBJECT (window[i]), "destroy",
			    G_CALLBACK (multiscreen_close_all), NULL);

	  vbox = gtk_vbox_new (TRUE, 0);

	  gtk_container_add (GTK_CONTAINER (window[i]), vbox);

	  label = gtk_label_new (NULL);

	  sprintf (label_text,
		   "       <big><span foreground=\"white\" background=\"black\">"
		   "Screen %d of %d</span></big>\n"
		   "<span background=\"darkcyan\"><i>Width - Height : (%d,%d)\n</i></span>",
		   i + 1, num_screen, gdk_screen_get_width (screen),
		   gdk_screen_get_height (screen));

	  gtk_label_set_markup (GTK_LABEL (label), label_text);

	  gtk_container_add (GTK_CONTAINER (vbox), label);

	  button = gtk_button_new_with_label ("Close");
	  g_signal_connect (G_OBJECT (button), "clicked",
			    G_CALLBACK (multiscreen_close_all), NULL);
	  gtk_container_add (GTK_CONTAINER (vbox), button);
	  gtk_widget_show_all (window[i]);
	}
      return window[0];
    }
  else
    {
      multiscreen_close_all (do_widget);
      return NULL;
    }


}
