/* Multihead Support/Multidisplay demo
 *
 * Demonstrates a multidisplay application, here multi display cut and paste.
 */

#include <gtk/gtk.h>
#include "demo-common.h"
#include <stdio.h>

gchar *screen2_name = NULL;
static GtkWidget *entry = NULL;
static GtkWidget *entry2 = NULL;

typedef struct
{
  GtkEntry *e1;
  GtkEntry *e2;
}
MyDoubleGtkEntry;

void
get_dialog_response (GtkDialog * dialog, gint arg1, gpointer data)
{
  GtkEntry *entry;
  if (arg1 == GTK_RESPONSE_DELETE_EVENT)
    return;
  g_return_if_fail (GTK_IS_ENTRY (data));
  entry = GTK_ENTRY (data);
  screen2_name = g_strdup (gtk_entry_get_text (entry));
}

void
clear_entry (GtkWidget * widget, gpointer * data)
{
  MyDoubleGtkEntry *de = (MyDoubleGtkEntry *) data;
  gtk_entry_set_text (de->e2, gtk_entry_get_text (de->e1));
}

void
quit_all (GtkWidget * widget, gpointer * data)
{
  MyDoubleGtkEntry *de = (MyDoubleGtkEntry *) data;
  if (entry)
    gtk_widget_destroy (gtk_widget_get_toplevel (entry));
  if (entry2)
    gtk_widget_destroy (gtk_widget_get_toplevel (entry2));
  if (de)
    g_free (de);
  entry = NULL;
  entry2 = NULL;
}

void
make_selection_dialog (GdkScreen * screen,
		       GtkWidget * entry, GtkWidget * other_entry)
{
  GtkWidget *window, *vbox, *button_box, *clear, *quit;
  MyDoubleGtkEntry *double_entry = g_new (MyDoubleGtkEntry, 1);
  double_entry->e1 = GTK_ENTRY (entry);
  double_entry->e2 = GTK_ENTRY (other_entry);

  window = gtk_widget_new (gtk_window_get_type (),
			   "screen", screen,
			   "user_data", NULL,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title",
			   "MultiDisplay Cut & Paste",
			   "border_width", 10, NULL);
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (quit_all), double_entry);

  vbox = gtk_vbox_new (TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  button_box = gtk_hbutton_box_new ();

  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

  clear = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  quit = gtk_button_new_from_stock (GTK_STOCK_QUIT);

  gtk_box_pack_start (GTK_BOX (button_box), clear, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (button_box), quit, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (quit), "clicked", G_CALLBACK (quit_all),
		    double_entry);
  g_signal_connect (G_OBJECT (clear), "clicked", G_CALLBACK (clear_entry),
		    double_entry);

  gtk_widget_show_all (window);
}

GtkWidget *
do_multidisplay (GtkWidget * do_widget)
{
  GtkWidget *dialog, *display_entry, *dialog_label;
  GdkDisplay *dpy2;
  GdkScreen *scr2 = NULL;	/* Quiet GCC */
  gboolean correct_second_display = FALSE;

  /* Get the second display */

  if (!entry)
    {
      char label_text[300] =
	"          <big><span foreground=\"white\" background=\"black\">"
	"Multiple Display Test</span></big>\n"
	"Please enter the name of the second display";

      dialog = gtk_dialog_new_with_buttons ("Second Display Selection",
					    GTK_WINDOW
					    (gtk_widget_get_toplevel
					     (do_widget)), GTK_DIALOG_MODAL,
					    GTK_STOCK_OK, GTK_RESPONSE_OK,
					    NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      display_entry = gtk_entry_new ();
      gtk_entry_set_activates_default (GTK_ENTRY (display_entry), TRUE);
      dialog_label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (dialog_label), label_text);

      gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
			 dialog_label);
      gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
			 display_entry);
      g_signal_connect (G_OBJECT (dialog), "response",
			G_CALLBACK (get_dialog_response), display_entry);

      gtk_widget_grab_focus (display_entry);

      gtk_widget_show_all (dialog);
      gtk_dialog_run (GTK_DIALOG (dialog));

      while (!correct_second_display)
	{

	  if (!g_strcasecmp (screen2_name, ""))
	    g_print ("No display name, reverting to default display\n");

	  dpy2 = gdk_display_new (0, NULL, screen2_name);
	  if (dpy2)
	    {
	      scr2 = gdk_display_get_default_screen (dpy2);
	      correct_second_display = TRUE;
	    }
	  else
	    {
	      char *error_msg = g_new (char, 1000);
	      sprintf (error_msg,
		       "<big><span foreground=\"white\" background=\"black\">"
		       "<b>Can't open display :</b></span>"
		       "\n\t%s\nplease try another one </big>", screen2_name);
	      gtk_label_set_markup (GTK_LABEL (dialog_label), error_msg);
	      gtk_entry_set_text (GTK_ENTRY (display_entry), "");
	      gtk_widget_show_all (dialog);
	      gtk_dialog_run (GTK_DIALOG (dialog));
	      g_free (error_msg);
	    }
	}
      g_free (screen2_name);
      gtk_widget_destroy (dialog);

      entry = gtk_widget_new (gtk_entry_get_type (),
			      "activates_default", TRUE,
			      "visible", TRUE, NULL);
      entry2 = gtk_widget_new (gtk_entry_get_type (), "visible", TRUE, NULL);

      /* for default display */
      make_selection_dialog (gtk_widget_get_screen (do_widget), entry2, entry);
      /* for selected display */
      make_selection_dialog (scr2, entry, entry2);
    }
  else
    {
      if (entry)
	{
	  gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (entry)));
	  entry = NULL;
	}
      if (entry2)
	{
	  gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (entry2)));
	  entry2 = NULL;
	}
    }
  return entry;
}
