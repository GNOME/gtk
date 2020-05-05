#include <gtk/gtk.h>

static void
edit_widget (GtkWidget *button)
{
  GtkWidget *dialog;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *check;

  dialog = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "dialog"));

  if (!dialog)
    {
      dialog = gtk_dialog_new_with_buttons ("",
                                            GTK_WINDOW (gtk_widget_get_root (button)),
                                            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                            NULL, NULL);

      grid = gtk_grid_new ();
      g_object_set (grid,
                    "margin-start", 20,
                    "margin-end", 20,
                    "margin-top", 20,
                    "margin-bottom", 20,
                    "row-spacing", 10,
                    "column-spacing", 10,
                    NULL);
      gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid);

      label = gtk_label_new ("Label:");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      entry = gtk_entry_new ();
      g_object_bind_property (button, "label",
                              entry, "text",
                              G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
      gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

      label = gtk_label_new ("Visible:");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      check = gtk_check_button_new ();
      g_object_bind_property (button, "visible",
                              check, "active",
                              G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
      gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), check, 1, 1, 1, 1);

      g_object_set_data (G_OBJECT (button), "dialog", dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static GtkWidget *
test_widget (const gchar *label)
{
  GtkWidget *w;

  w = gtk_button_new_with_label (label);
  g_signal_connect (w, "clicked", G_CALLBACK (edit_widget), NULL);

  return w;
}

static void
spacing_changed (GtkSpinButton *spin, GtkBox *box)
{
  gint spacing;

  spacing = gtk_spin_button_get_value_as_int (spin);
  gtk_box_set_spacing (box, spacing);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkWidget *check;
  GtkWidget *b;
  GtkWidget *label;
  GtkWidget *spin;

  gtk_init ();

  window = gtk_window_new ();

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), test_widget ("1"));
  gtk_container_add (GTK_CONTAINER (box), test_widget ("2"));
  gtk_container_add (GTK_CONTAINER (box), test_widget ("3"));
  gtk_container_add (GTK_CONTAINER (box), test_widget ("4"));
  gtk_container_add (GTK_CONTAINER (box), test_widget ("5"));
  gtk_container_add (GTK_CONTAINER (box), test_widget ("6"));

  gtk_container_add (GTK_CONTAINER (vbox), box);

  check = gtk_check_button_new_with_label ("Homogeneous");
  g_object_bind_property (box, "homogeneous",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_widget_set_margin_start (check, 10);
  gtk_widget_set_margin_end (check, 10);
  gtk_widget_set_margin_top (check, 10);
  gtk_widget_set_margin_bottom (check, 10);
  gtk_widget_set_halign (check, GTK_ALIGN_CENTER);
  gtk_widget_show (check);
  gtk_container_add (GTK_CONTAINER (vbox), check);

  b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_margin_start (b, 10);
  gtk_widget_set_margin_end (b, 10);
  gtk_widget_set_margin_top (b, 10);
  gtk_widget_set_margin_bottom (b, 10);
  gtk_widget_set_halign (b, GTK_ALIGN_CENTER);
  label = gtk_label_new ("Spacing:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_container_add (GTK_CONTAINER (b), label);

  spin = gtk_spin_button_new_with_range (0, 10, 1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
  gtk_widget_set_halign (spin, GTK_ALIGN_START);
  g_signal_connect (spin, "value-changed",
                    G_CALLBACK (spacing_changed), box);
  gtk_container_add (GTK_CONTAINER (b), spin);
  gtk_container_add (GTK_CONTAINER (vbox), b);

  gtk_widget_show (window);

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
