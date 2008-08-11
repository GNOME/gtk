
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

GtkWidget *colorseldlg = NULL;
GtkWidget *drawingarea = NULL;
GdkColor color;

/* Color changed handler */

static void color_changed_cb( GtkWidget         *widget,
                              GtkColorSelection *colorsel )
{
  GdkColor ncolor;

  gtk_color_selection_get_current_color (colorsel, &ncolor);
  gtk_widget_modify_bg (drawingarea, GTK_STATE_NORMAL, &ncolor);       
}

/* Drawingarea event handler */

static gboolean area_event( GtkWidget *widget,
                            GdkEvent  *event,
                            gpointer   client_data )
{
  gint handled = FALSE;
  gint response;
  GtkColorSelection *colorsel;

  /* Check if we've received a button pressed event */

  if (event->type == GDK_BUTTON_PRESS)
    {
      handled = TRUE;

       /* Create color selection dialog */
      if (colorseldlg == NULL)
        colorseldlg = gtk_color_selection_dialog_new ("Select background color");

      /* Get the ColorSelection widget */
      colorsel = GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (colorseldlg)->colorsel);

      gtk_color_selection_set_previous_color (colorsel, &color);
      gtk_color_selection_set_current_color (colorsel, &color);
      gtk_color_selection_set_has_palette (colorsel, TRUE);

      /* Connect to the "color-changed" signal, set the client-data
       * to the colorsel widget */
      g_signal_connect (colorsel, "color-changed",
                        G_CALLBACK (color_changed_cb),
                        colorsel);

      /* Show the dialog */
      response = gtk_dialog_run (GTK_DIALOG (colorseldlg));

      if (response == GTK_RESPONSE_OK)
        gtk_color_selection_get_current_color (colorsel, &color);
      else 
        gtk_widget_modify_bg (drawingarea, GTK_STATE_NORMAL, &color);

      gtk_widget_hide (colorseldlg);
    }

  return handled;
}

/* Close down and exit handler */

static gboolean destroy_window( GtkWidget *widget,
                                GdkEvent  *event,
                                gpointer   client_data )
{
  gtk_main_quit ();
  return TRUE;
}

/* Main */

gint main( gint   argc,
           gchar *argv[] )
{
  GtkWidget *window;

  /* Initialize the toolkit, remove gtk-related commandline stuff */

  gtk_init (&argc, &argv);

  /* Create toplevel window, set title and policies */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Color selection test");
  gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, TRUE);

  /* Attach to the "delete" and "destroy" events so we can exit */

  g_signal_connect (GTK_OBJECT (window), "delete_event",
                    GTK_SIGNAL_FUNC (destroy_window), (gpointer) window);
  
  /* Create drawingarea, set size and catch button events */

  drawingarea = gtk_drawing_area_new ();

  color.red = 0;
  color.blue = 65535;
  color.green = 0;
  gtk_widget_modify_bg (drawingarea, GTK_STATE_NORMAL, &color);       

  gtk_widget_set_size_request (GTK_WIDGET (drawingarea), 200, 200);

  gtk_widget_set_events (drawingarea, GDK_BUTTON_PRESS_MASK);

  g_signal_connect (GTK_OBJECT (drawingarea), "event", 
	            GTK_SIGNAL_FUNC (area_event), (gpointer) drawingarea);
  
  /* Add drawingarea to window, then show them both */

  gtk_container_add (GTK_CONTAINER (window), drawingarea);

  gtk_widget_show (drawingarea);
  gtk_widget_show (window);
  
  /* Enter the gtk main loop (this never returns) */

  gtk_main ();

  /* Satisfy grumpy compilers */

  return 0;
}
