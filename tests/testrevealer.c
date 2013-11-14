#include <gtk/gtk.h>

gint
main (gint argc,
      gchar ** argv)
{
  GtkWidget *window, *revealer, *box, *widget, *entry;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 300, 300);

  box = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), box);

  widget = gtk_label_new ("Some filler text to avoid\nresizing of the window");
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
  gtk_widget_set_margin_start (widget, 10);
  gtk_widget_set_margin_end (widget, 10);
  gtk_grid_attach (GTK_GRID (box), widget, 1, 1, 1, 1);

  widget = gtk_label_new ("Some filler text to avoid\nresizing of the window");
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
  gtk_widget_set_margin_start (widget, 10);
  gtk_widget_set_margin_end (widget, 10);
  gtk_grid_attach (GTK_GRID (box), widget, 3, 3, 1, 1);

  widget = gtk_toggle_button_new_with_label ("None");
  gtk_grid_attach (GTK_GRID (box), widget, 0, 0, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_halign (revealer, GTK_ALIGN_START);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "00000");
  gtk_container_add (GTK_CONTAINER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 1, 0, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Fade");
  gtk_grid_attach (GTK_GRID (box), widget, 4, 4, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  gtk_widget_set_valign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "00000");
  gtk_container_add (GTK_CONTAINER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 3, 4, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Right");
  gtk_grid_attach (GTK_GRID (box), widget, 0, 2, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_hexpand (revealer, TRUE);
  gtk_widget_set_halign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "12345");
  gtk_container_add (GTK_CONTAINER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 1, 2, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Down");
  gtk_grid_attach (GTK_GRID (box), widget, 2, 0, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_vexpand (revealer, TRUE);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "23456");
  gtk_container_add (GTK_CONTAINER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 2, 1, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Left");
  gtk_grid_attach (GTK_GRID (box), widget, 4, 2, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_hexpand (revealer, TRUE);
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "34567");
  gtk_container_add (GTK_CONTAINER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 3, 2, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Up");
  gtk_grid_attach (GTK_GRID (box), widget, 2, 4, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_vexpand (revealer, TRUE);
  gtk_widget_set_valign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "45678");
  gtk_container_add (GTK_CONTAINER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 2, 3, 1, 1);

  gtk_widget_show_all (window);
  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
