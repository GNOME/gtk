#include <gtk/gtk.h>

static void
expand_toggled (GtkToggleButton *b, GtkWidget *w)
{
  gboolean active;
  GtkWidget *parent;

  active = gtk_toggle_button_get_active (b);
  parent = gtk_widget_get_parent (w);
  gtk_container_child_set (GTK_CONTAINER (parent), w,
                           "expand", active,
                           NULL);
}

static void
fill_toggled (GtkToggleButton *b, GtkWidget *w)
{
  gboolean active;
  GtkWidget *parent;

  active = gtk_toggle_button_get_active (b);
  parent = gtk_widget_get_parent (w);
  gtk_container_child_set (GTK_CONTAINER (parent), w,
                           "fill", active,
                           NULL);
}

static void
edit_widget (GtkWidget *button)
{
  GtkWidget *dialog;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *check;
  gboolean expand, fill;

  dialog = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "dialog"));

  if (!dialog)
    {
      dialog = gtk_dialog_new_with_buttons ("",
                                            GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                            NULL, NULL);

      grid = gtk_grid_new ();
      g_object_set (grid,
                    "margin", 20,
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

      label = gtk_label_new ("Expand:");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      check = gtk_check_button_new ();
      gtk_box_query_child_packing (GTK_BOX (gtk_widget_get_parent (button)),
                                   button, &expand, NULL, NULL, NULL);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), expand);
      g_signal_connect (check, "toggled",
                        G_CALLBACK (expand_toggled), button);
      gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), check, 1, 2, 1, 1);

      label = gtk_label_new ("Fill:");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      check = gtk_check_button_new ();
      gtk_box_query_child_packing (GTK_BOX (gtk_widget_get_parent (button)),
                                   button, NULL, &fill, NULL, NULL);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), fill);
      g_signal_connect (check, "toggled",
                        G_CALLBACK (fill_toggled), button);
      gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), check, 1, 3, 1, 1);
 
      gtk_widget_show_all (grid);

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

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), test_widget ("1"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), test_widget ("2"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), test_widget ("3"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), test_widget ("4"), FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), test_widget ("5"), FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), test_widget ("6"), FALSE, TRUE, 0);

  gtk_box_set_center_widget (GTK_BOX (box), test_widget ("center"));
  gtk_container_add (GTK_CONTAINER (vbox), box);

  check = gtk_check_button_new_with_label ("Homogeneous");
  g_object_bind_property (box, "homogeneous",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  g_object_set (check, "margin", 10, NULL);
  gtk_widget_set_halign (check, GTK_ALIGN_CENTER);
  gtk_widget_show (check);
  gtk_container_add (GTK_CONTAINER (vbox), check);

  b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (b, "margin", 10, NULL);
  gtk_widget_set_halign (b, GTK_ALIGN_CENTER);
  label = gtk_label_new ("Spacing:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_box_pack_start (GTK_BOX (b), label, FALSE, TRUE, 0);

  spin = gtk_spin_button_new_with_range (0, 10, 1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
  gtk_widget_set_halign (spin, GTK_ALIGN_START);
  g_signal_connect (spin, "value-changed",
                    G_CALLBACK (spacing_changed), box);
  gtk_box_pack_start (GTK_BOX (b), spin, FALSE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), b);
  
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
