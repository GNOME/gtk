#include <gtk/gtk.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry;
  GIcon *icon;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Gtk Entry Icons Test");
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);

  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);

  table = gtk_table_new (2, 4, FALSE);
  gtk_container_add (GTK_CONTAINER (window), table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);

  /*
   * Open File - Sets the icon using a GIcon
   */
  label = gtk_label_new ("Open File:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  icon = g_themed_icon_new_with_default_fallbacks ("folder");
  gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 icon);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_PRIMARY,
				FALSE);

  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				   GTK_ENTRY_ICON_PRIMARY,
				   "Open a file");

  /*
   * Save File - sets the icon using a stock id.
   */
  label = gtk_label_new ("Save File:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_entry_set_text (GTK_ENTRY (entry), "‏Right-to-left");
  gtk_widget_set_direction (entry, GTK_TEXT_DIR_RTL);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 GTK_STOCK_SAVE);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				   GTK_ENTRY_ICON_PRIMARY,
				   "Save a file");

  /*
   * Search - Uses a helper function
   */
  label = gtk_label_new ("Search:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 2, 3,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 GTK_STOCK_FIND);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_SECONDARY,
				 GTK_STOCK_CLEAR);

  /*
   * Password - Sets the icon using a stock id
   */
  label = gtk_label_new ("Password:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 3, 4,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 GTK_STOCK_DIALOG_AUTHENTICATION);

  /* Name - Does not set any icons. */
  label = gtk_label_new ("Name:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 4, 5,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
