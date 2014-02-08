#include <gtk/gtk.h>

static void
toggle_center (GtkCheckButton *button,
               GParamSpec     *pspec,
               GtkActionBar   *bar)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      GtkWidget *button;

      button = gtk_button_new_with_label ("Center");
      gtk_widget_show (button);
      gtk_action_bar_set_center_widget (bar, button);
    }
  else
    {
      gtk_action_bar_set_center_widget (bar, NULL);
    }
}

static void
toggle_visibility (GtkCheckButton *button,
                   GParamSpec     *pspec,
                   GtkActionBar   *bar)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    gtk_widget_show (GTK_WIDGET (bar));
  else
    gtk_widget_hide (GTK_WIDGET (bar));
}

static void
create_widgets (GtkActionBar  *bar,
                GtkPackType    pack_type,
                gint           n)
{
  GList *children, *l;
  GtkWidget *child;
  gint i;
  gchar *label;

  children = gtk_container_get_children (GTK_CONTAINER (bar));
  for (l = children; l; l = l->next)
    {
      GtkPackType type;

      child = l->data;
      gtk_container_child_get (GTK_CONTAINER (bar), child, "pack-type", &type, NULL);
      if (type == pack_type)
        gtk_container_remove (GTK_CONTAINER (bar), child);
    }
  g_list_free (children);

  for (i = 0; i < n; i++)
    {
      label = g_strdup_printf ("%d", i);
      child = gtk_button_new_with_label (label);
      g_free (label);

      gtk_widget_show (child);
      if (pack_type == GTK_PACK_START)
        gtk_action_bar_pack_start (bar, child);
      else
        gtk_action_bar_pack_end (bar, child);
    }
}

static void
change_start (GtkSpinButton *button,
              GParamSpec    *pspec,
              GtkActionBar  *bar)
{
  create_widgets (bar,
                  GTK_PACK_START,
                  gtk_spin_button_get_value_as_int (button));
}

static void
change_end (GtkSpinButton *button,
            GParamSpec    *pspec,
            GtkActionBar  *bar)
{
  create_widgets (bar,
                  GTK_PACK_END,
                  gtk_spin_button_get_value_as_int (button));
}

static void
activate (GApplication *gapp)
{
  GtkApplication *app = GTK_APPLICATION (gapp);
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *spin;
  GtkWidget *check;
  GtkWidget *bar;

  window = gtk_application_window_new (app);
  gtk_application_add_window (app, GTK_WINDOW (window));

  bar = gtk_action_bar_new ();
  gtk_widget_set_no_show_all (bar, TRUE);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  grid = gtk_grid_new ();
  g_object_set (grid,
                "halign", GTK_ALIGN_CENTER,
                "margin", 20,
                "row-spacing", 12,
                "column-spacing", 12,
                NULL);
  gtk_box_pack_start (GTK_BOX (box), grid, FALSE, FALSE, 0);

  label = gtk_label_new ("Start");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  spin = gtk_spin_button_new_with_range (0, 10, 1);
  g_signal_connect (spin, "notify::value",
                    G_CALLBACK (change_start), bar);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, 0, 1, 1);

  label = gtk_label_new ("Center");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_signal_connect (check, "notify::active",
                    G_CALLBACK (toggle_center), bar);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 1, 1, 1, 1);

  label = gtk_label_new ("End");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  spin = gtk_spin_button_new_with_range (0, 10, 1);
  g_signal_connect (spin, "notify::value",
                    G_CALLBACK (change_end), bar);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, 2, 1, 1);

  label = gtk_label_new ("Visible");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_signal_connect (check, "notify::active",
                    G_CALLBACK (toggle_visibility), bar);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 1, 3, 1, 1);

  gtk_box_pack_end (GTK_BOX (box), bar, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.ActionBar", 0);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
