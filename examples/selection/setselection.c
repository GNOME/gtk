/* example-start selection setselection.c */

#include <gtk/gtk.h>
#include <time.h>

/* Callback when the user toggles the selection */
void selection_toggled( GtkWidget *widget,
                        gint      *have_selection )
{
  if (GTK_TOGGLE_BUTTON(widget)->active)
    {
      *have_selection = gtk_selection_owner_set (widget,
						 GDK_SELECTION_PRIMARY,
						 GDK_CURRENT_TIME);
      /* if claiming the selection failed, we return the button to
	 the out state */
      if (!*have_selection)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), FALSE);
    }
  else
    {
      if (*have_selection)
	{
	  /* Before clearing the selection by setting the owner to NULL,
	     we check if we are the actual owner */
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				     GDK_CURRENT_TIME);
	  *have_selection = FALSE;
	}
    }
}

/* Called when another application claims the selection */
gint selection_clear( GtkWidget         *widget,
                      GdkEventSelection *event,
                      gint              *have_selection )
{
  *have_selection = FALSE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), FALSE);

  return TRUE;
}

/* Supplies the current time as the selection. */
void selection_handle( GtkWidget        *widget, 
                       GtkSelectionData *selection_data,
                       guint             info,
                       guint             time_stamp,
                       gpointer          data )
{
  gchar *timestr;
  time_t current_time;

  current_time = time(NULL);
  timestr = asctime (localtime(&current_time)); 
  /* When we return a single string, it should not be null terminated.
     That will be done for us */

  gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
			  8, timestr, strlen(timestr));
}

int main( int   argc,
          char *argv[] )
{
  GtkWidget *window;
  GtkWidget *selection_button;

  static int have_selection = FALSE;
  
  gtk_init (&argc, &argv);

  /* Create the toplevel window */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Event Box");
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_exit), NULL);

  /* Create a toggle button to act as the selection */

  selection_button = gtk_toggle_button_new_with_label ("Claim Selection");
  gtk_container_add (GTK_CONTAINER (window), selection_button);
  gtk_widget_show (selection_button);

  gtk_signal_connect (GTK_OBJECT(selection_button), "toggled",
		      GTK_SIGNAL_FUNC (selection_toggled), &have_selection);
  gtk_signal_connect (GTK_OBJECT(selection_button), "selection_clear_event",
		      GTK_SIGNAL_FUNC (selection_clear), &have_selection);

  gtk_selection_add_target (selection_button,
			    GDK_SELECTION_PRIMARY,
			    GDK_SELECTION_TYPE_STRING,
		            1);
  gtk_signal_connect (GTK_OBJECT(selection_button), "selection_get",
		      GTK_SIGNAL_FUNC (selection_handle), &have_selection);

  gtk_widget_show (selection_button);
  gtk_widget_show (window);
  
  gtk_main ();
  
  return 0;
}
/* example-end */
