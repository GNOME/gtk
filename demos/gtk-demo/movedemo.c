/* Multihead Support/Move Demo
 * 
 * Demostrates a way of recreating a widget tree from a top level window
 * to either a other screen of the same display or a different display.
 *
 */
#include <gtk/gtk.h>
#include "demo-common.h"

static GtkWidget *window = NULL;

typedef struct
{
  GtkEntry *entry;
  GtkWidget *radio_dpy;
  GtkWidget *toplevel;
  GtkWidget *dialog_window;
  GList *valid_display_list;
}
ScreenDisplaySelection;

/* Create a new toplevel and reparent  */
void
change_screen (GdkScreen * new_screen, GtkWidget * toplevel)
{
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (toplevel));
  GtkWidget *new_toplevel;

  new_toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_screen (GTK_WINDOW (new_toplevel), new_screen);

  gtk_widget_set_name (new_toplevel, "main window");
  gtk_window_set_default_size (GTK_WINDOW (new_toplevel), 600, 400);

  g_signal_connect (G_OBJECT (new_toplevel), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (G_OBJECT (new_toplevel), "delete-event",
		    G_CALLBACK (gtk_false), NULL);

  gtk_widget_reparent (GTK_WIDGET (child), new_toplevel);
  /* App dependant initialization */


  gtk_widget_show_all (new_toplevel);

  g_signal_handlers_disconnect_by_func (G_OBJECT (toplevel),
					G_CALLBACK (gtk_main_quit), NULL);
  g_signal_handlers_disconnect_by_func (G_OBJECT (toplevel),
					G_CALLBACK (gtk_false), NULL);
  gtk_widget_destroy (toplevel);
}

static gint
display_name_cmp (gconstpointer a, gconstpointer b)
{
  return g_ascii_strcasecmp (a, b);
}


void
screen_display_check (GtkWidget * widget, ScreenDisplaySelection * data)
{
  char *display_name;
  GdkDisplay *display = gtk_widget_get_display (widget);
  GtkWidget *dialog;
  GdkScreen *new_screen = NULL;
  GdkScreen *current_screen = gtk_widget_get_screen (widget);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->radio_dpy)))
    {
      display_name = g_strdup (gtk_entry_get_text (data->entry));
      display = gdk_display_new (0, NULL, (char *) display_name);

      if (!display)
	{
	  dialog =
	    gtk_message_dialog_new (GTK_WINDOW
				    (gtk_widget_get_toplevel (widget)),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				    "The display :\n%s\ncannot be opened",
				    display_name);
	  gtk_window_set_screen (GTK_WINDOW (dialog), current_screen);
	  gtk_widget_show (dialog);
	  g_signal_connect (G_OBJECT (dialog), "response",
			    G_CALLBACK (gtk_widget_destroy), NULL);
	}
      else
	{
	  if (!g_list_find_custom (data->valid_display_list,
				   display_name, display_name_cmp))
	    data->valid_display_list =
	      g_list_append (data->valid_display_list, display_name);

	  new_screen = gdk_display_get_default_screen (display);
	}
    }
  else
    {
      int number_of_screens = gdk_display_get_n_screens (display);
      int screen_num = gdk_screen_get_number (current_screen);
      if ((screen_num + 1) < number_of_screens)
	new_screen = gdk_display_get_screen (display, screen_num + 1);
      else
	new_screen = gdk_display_get_screen (display, 0);
    }
  if (new_screen)
    {
      change_screen (new_screen, data->toplevel);
      gtk_widget_destroy (data->dialog_window);
      window = NULL;
    }
}

void
screen_display_destroy_diag (GtkWidget * widget, GtkWidget * data)
{
  gtk_widget_destroy (data);
  window = NULL;
}

GtkWidget *
do_movedemo (GtkWidget * do_widget)
{
  GtkWidget *table, *frame, *combo_dpy, *vbox;
  GtkWidget *radio_dpy, *radio_scr, *applyb, *cancelb;
  GtkWidget *bbox;
  ScreenDisplaySelection *scr_dpy_data;
  GdkScreen *screen = gtk_widget_get_screen (do_widget);
  static GList *valid_display_list = NULL;

  if (!window)
    {
      GdkDisplay *display = gdk_screen_get_display (screen);

      window = gtk_widget_new (gtk_window_get_type (),
			       "screen", screen,
			       "user_data", NULL,
			       "type", GTK_WINDOW_TOPLEVEL,
			       "title",
			       "Screen or Display selection",
			       "border_width", 10, NULL);
      g_signal_connect (G_OBJECT (window), "destroy",
			G_CALLBACK (gtk_widget_destroy), NULL);

      vbox = gtk_vbox_new (FALSE, 3);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      frame = gtk_frame_new ("Select screen or display");
      gtk_container_add (GTK_CONTAINER (vbox), frame);

      table = gtk_table_new (2, 2, TRUE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 3);
      gtk_table_set_col_spacings (GTK_TABLE (table), 3);

      gtk_container_add (GTK_CONTAINER (frame), table);

      radio_dpy =
	gtk_radio_button_new_with_label (NULL, "move to another X display");
      if (gdk_display_get_n_screens (display) > 1)
	radio_scr = gtk_radio_button_new_with_label
	  (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_dpy)),
	   "move to next screen");
      else
	{
	  radio_scr = gtk_radio_button_new_with_label
	    (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_dpy)),
	     "only one screen on the current display");
	  gtk_widget_set_sensitive (radio_scr, FALSE);
	}
      combo_dpy = gtk_combo_new ();
      if (!valid_display_list)
	valid_display_list =
	  g_list_append (valid_display_list, "diabolo:0.0");

      gtk_combo_set_popdown_strings (GTK_COMBO (combo_dpy),
				     valid_display_list);

      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo_dpy)->entry),
			  "<hostname>:<X Server Num>.<Screen Num>");

      gtk_table_attach_defaults (GTK_TABLE (table), radio_dpy, 0, 1, 0, 1);
      gtk_table_attach_defaults (GTK_TABLE (table), radio_scr, 0, 1, 1, 2);
      gtk_table_attach_defaults (GTK_TABLE (table), combo_dpy, 1, 2, 0, 1);

      bbox = gtk_hbutton_box_new ();
      applyb = gtk_button_new_from_stock (GTK_STOCK_APPLY);
      cancelb = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

      gtk_container_add (GTK_CONTAINER (vbox), bbox);

      gtk_container_add (GTK_CONTAINER (bbox), applyb);
      gtk_container_add (GTK_CONTAINER (bbox), cancelb);

      scr_dpy_data = g_new0 (ScreenDisplaySelection, 1);

      scr_dpy_data->entry = GTK_ENTRY (GTK_COMBO (combo_dpy)->entry);
      scr_dpy_data->radio_dpy = radio_dpy;
      scr_dpy_data->toplevel = gtk_widget_get_toplevel (do_widget);
      scr_dpy_data->dialog_window = window;
      scr_dpy_data->valid_display_list = valid_display_list;

      g_signal_connect (G_OBJECT (cancelb), "clicked",
			G_CALLBACK (screen_display_destroy_diag),
			G_OBJECT (window));
      g_signal_connect (G_OBJECT (applyb), "clicked",
			G_CALLBACK (screen_display_check), scr_dpy_data);
      { /* FIXME need to realize the two notebook pages before moving display
	 * otherwise the text tags created on the first display get realized
	 * on the new display then core dump */
	extern GtkWidget *notebook;
	gint num_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), num_page ? 0 : 1);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), num_page);
      }
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
