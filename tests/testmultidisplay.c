#include <strings.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkstock.h>
#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>

const gchar *screen2_name = NULL;

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
    exit (1);
  g_return_if_fail (GTK_IS_ENTRY (data));
  entry = GTK_ENTRY (data);
  screen2_name = gtk_entry_get_text (entry);
}

void
clear_entry (GtkWidget * widget, gpointer * data)
{
/*  GtkEntry *entry = GTK_ENTRY (data);
  gtk_entry_set_text (entry,"");*/
  MyDoubleGtkEntry *de = (MyDoubleGtkEntry *) data;
  gtk_entry_set_text (de->e2, gtk_entry_get_text (de->e1));
}

void
changed (GtkEntry * entry, gpointer * data)
{
  GtkEntry *other_entry = GTK_ENTRY (data);
  printf ("%s | %s\n", gtk_entry_get_text (entry),
	  gtk_entry_get_text (other_entry));
}

void
make_selection_dialog (GdkScreen * screen,
		       GtkWidget * entry, GtkWidget * other_entry)
{
  GtkWidget *window, *vbox, *button_box, *clear, *quit;
  MyDoubleGtkEntry *double_entry = g_new (MyDoubleGtkEntry, 1);
  double_entry->e1 = GTK_ENTRY (entry);
  double_entry->e2 = GTK_ENTRY (other_entry);

  if (!screen)
    screen = gdk_get_default_screen ();

  window = gtk_widget_new (gtk_window_get_type (),
			   "screen", screen,
			   "user_data", NULL,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title",
			   "MultiDisplay Cut & Paste",
			   "border_width", 10, NULL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_vbox_new (TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (changed), other_entry);

  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  button_box = gtk_hbutton_box_new ();

  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

  clear = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  quit = gtk_button_new_from_stock (GTK_STOCK_QUIT);

  gtk_box_pack_start (GTK_BOX (button_box), clear, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (button_box), quit, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (quit), "clicked", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (G_OBJECT (quit), "clicked", G_CALLBACK (clear_entry), double_entry);

  gtk_widget_show_all (window);
}





int
main (int argc, char *argv[])
{
  GtkWidget *dialog, *display_entry, *dialog_label;
  GtkWidget *entry, *entry2;
  GdkDisplay *dpy2;
  GdkScreen *scr2 = NULL;	/* Quiet GCC */
  gboolean correct_second_display = FALSE;

  gtk_init (&argc, &argv);

  /* Get the second display */

  dialog = gtk_dialog_new_with_buttons ("Second Display Selection",
					NULL,
					GTK_DIALOG_MODAL,
					GTK_STOCK_OK,
					GTK_RESPONSE_OK, NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  display_entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (display_entry), TRUE);
  dialog_label =
    gtk_label_new ("Please enter the name of\nthe second display\n");

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_label);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
		     display_entry);
  gtk_signal_connect (GTK_OBJECT (dialog), "response",
		      GTK_SIGNAL_FUNC (get_dialog_response), display_entry);

  gtk_widget_grab_focus (display_entry);
  
  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));

  while (!correct_second_display)
    {
      if (!strcmp (screen2_name, ""))
	g_print ("No display name, reverting to default display\n");

      dpy2 = gdk_display_init_new (0, NULL, screen2_name);
      if (dpy2)
	{
	  scr2 = gdk_display_get_default_screen (dpy2);
	  correct_second_display = TRUE;
	}
      else
	{
	  char *error_msg =
	    g_new (char, sizeof (char) * (strlen (screen2_name) + 50));
	  sprintf (error_msg,
		   "Can't open display :\n\t%s\nplease try another one\n",
		   screen2_name);
	  gtk_label_set_text (GTK_LABEL (dialog_label), error_msg);
	  gtk_widget_show_all (dialog);
	  gtk_dialog_run (GTK_DIALOG (dialog));
	}
    }
  gtk_widget_destroy (dialog);

  entry = gtk_widget_new (gtk_entry_get_type (),
			  "activates_default", TRUE,
			  "visible", TRUE,
			  NULL);
  entry2 = gtk_widget_new (gtk_entry_get_type (),
			   "visible", TRUE, NULL);

  /* for default display */
  make_selection_dialog (NULL, entry2, entry);
  /* for selected display */
  make_selection_dialog (scr2, entry, entry2);


  gtk_main ();

  return 0;
}
