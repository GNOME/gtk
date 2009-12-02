#include <gtk/gtk.h>
#include <math.h>

static void
size_group_toggled_cb (GtkToggleButton *button,
                       GtkSizeGroup    *group)
{
  if (gtk_toggle_button_get_active (button))
    gtk_size_group_set_mode (group, GTK_SIZE_GROUP_HORIZONTAL);
  else
    gtk_size_group_set_mode (group, GTK_SIZE_GROUP_NONE);
}

static void
ellipsize_toggled_cb (GtkToggleButton *button,
                      GtkWidget       *vbox)
{
  GList *rows, *row_iter, *cells, *cell_iter;
  PangoEllipsizeMode mode;

  if (gtk_toggle_button_get_active (button))
    mode = PANGO_ELLIPSIZE_END;
  else
    mode = PANGO_ELLIPSIZE_NONE;

  rows = gtk_container_get_children (GTK_CONTAINER (vbox));
  for (row_iter = rows; row_iter; row_iter = row_iter->next)
    {
      if (!GTK_IS_CONTAINER (row_iter->data))
        break;

      cells = gtk_container_get_children (row_iter->data);

      for (cell_iter = cells; cell_iter; cell_iter = cell_iter->next)
        if (GTK_IS_LABEL (cell_iter->data))
          gtk_label_set_ellipsize (cell_iter->data, mode);

      g_list_free (cells);
    }

  g_list_free (rows);
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *window, *vbox, *button;
  GtkSizeGroup *groups[5];
  gint x, y;

  gtk_init (&argc, &argv);

  for (x = 0; x < G_N_ELEMENTS (groups); ++x)
    groups[x] = gtk_size_group_new (GTK_SIZE_GROUP_NONE);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

  for (y = 0; y < 4; ++y)
    {
      GtkWidget *hbox = gtk_hbox_new (FALSE, 6);

      for (x = 0; x < G_N_ELEMENTS (groups); ++x)
        {
          gchar *text = g_strdup_printf ("Label #%.0f.%d", pow(10, y), x + 1);
          GtkWidget *label = gtk_label_new (text);
          g_free (text);

          text = g_strdup_printf ("label/%d/%d", y, x);
          gtk_widget_set_name (label, text);
          g_free (text);

          if (1 != x)
            gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
          if (x > 0)
            gtk_box_pack_start (GTK_BOX (hbox), gtk_vseparator_new (), FALSE, TRUE, 0);

          gtk_box_pack_start (GTK_BOX (hbox), label, 1 == x, TRUE, 0);
          gtk_size_group_add_widget (groups[x], label);
        }

      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    }

  gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, TRUE, 0);

  for (x = 0; x < G_N_ELEMENTS (groups); ++x)
    {
      gchar *text = g_strdup_printf ("Size Group #%d", x + 1);

      button = gtk_check_button_new_with_label (text);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 0);
      g_free (text);

      g_signal_connect (button, "toggled", G_CALLBACK (size_group_toggled_cb), groups[x]);
  }

  gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, TRUE, 0);

  button = gtk_check_button_new_with_label ("Ellipsize");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (ellipsize_toggled_cb),
                    vbox);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show_all (window);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  gtk_main ();

  return 0;
}
