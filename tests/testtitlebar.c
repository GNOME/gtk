#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *check;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  grid = gtk_grid_new ();
  g_object_set (grid,
                "halign", GTK_ALIGN_CENTER,
                "margin", 20,
                "row-spacing", 12,
                "column-spacing", 12,
                NULL);

  label = gtk_label_new ("Title");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  g_object_bind_property (entry, "text",
                          header, "title",
                          G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

  label = gtk_label_new ("Subtitle");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  g_object_bind_property (entry, "text",
                          header, "subtitle",
                          G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);

  label = gtk_label_new ("Has Subtitle");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_object_bind_property (check, "active",
                          header, "has-subtitle",
                          G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 3, 1, 1, 1);

  label = gtk_label_new ("Close Button");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_object_bind_property (check, "active",
                          header, "show-close-button",
                          G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 3, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show_all (window);

  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
