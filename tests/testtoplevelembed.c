#include "config.h"
#include <gtk/gtk.h>



gint
main (gint argc, gchar **argv)
{
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *widget;
  GtkWidget *label;
  GdkWindow *gdk_win;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Toplevel widget embedding example");
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  notebook = gtk_notebook_new ();
  gtk_container_add (GTK_CONTAINER (window), notebook);

  gtk_widget_realize (notebook);
  gdk_win = gtk_widget_get_window (notebook);
  g_assert (gdk_win);

  widget = gtk_about_dialog_new ();
  label  = gtk_label_new ("GtkAboutDialog");
  gtk_widget_set_parent_window (widget, gdk_win);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

  widget = gtk_file_chooser_dialog_new ("the chooser", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
  label  = gtk_label_new ("GtkFileChooser");
  gtk_widget_set_parent_window (widget, gdk_win);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

  widget = gtk_color_selection_dialog_new ("the colorsel");
  label  = gtk_label_new ("GtkColorSelDialog");
  gtk_widget_set_parent_window (widget, gdk_win);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

  widget = gtk_font_selection_dialog_new ("the fontsel");
  label  = gtk_label_new ("GtkFontSelectionDialog");
  gtk_widget_set_parent_window (widget, gdk_win);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

  widget = gtk_recent_chooser_dialog_new ("the recent chooser", NULL,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					  NULL);
  label  = gtk_label_new ("GtkRecentChooserDialog");
  gtk_widget_set_parent_window (widget, gdk_win);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

  widget = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, 
				   GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
				   "Do you have any questions ?");
  label  = gtk_label_new ("GtkMessageDialog");
  gtk_widget_set_parent_window (widget, gdk_win);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

  gtk_widget_show_all (window);
  gtk_main ();
}
