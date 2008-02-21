#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* It has to be a widget with a popup, GtkComboBox, GtkVolumeButton, ... */
  GtkWidget *combobox = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), "combobox");
  gtk_container_add (GTK_CONTAINER (vbox), combobox);

  GtkWidget *notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
  gtk_container_add (GTK_CONTAINER (vbox), notebook);

  GtkWidget *label = gtk_label_new ("Hello world!");
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), label, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), label, TRUE);

  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}



