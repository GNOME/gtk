/* Paned Widgets
 *
 * The GtkPaned Widget divides its content area into two panes
 * with a divider in between that the user can adjust. A separate
 * child is placed into each pane. GtkPaned widgets can be split
 * horizontally or vertially.
 *
 * There are a number of options that can be set for each pane.
 * This test contains both a horizontal and a vertical GtkPaned
 * widget, and allows you to adjust the options for each side of
 * each widget.
 */

#include <gtk/gtk.h>

void
toggle_resize (GtkWidget *widget,
               GtkWidget *child)
{
  GtkWidget *parent;
  GtkPaned *paned;
  gboolean is_child1;
  gboolean resize, shrink;

  parent = gtk_widget_get_parent (child);
  paned = GTK_PANED (parent);

  is_child1 = (child == gtk_paned_get_child1 (paned));

  if (is_child1)
    g_object_get (paned,
                  "resize-child1", &resize,
                  "shrink-child1", &shrink,
                  NULL);
  else
    g_object_get (paned,
                  "resize-child2", &resize,
                  "shrink-child2", &shrink,
                  NULL);

  g_object_ref (child);
  gtk_container_remove (GTK_CONTAINER (parent), child);
  if (is_child1)
    gtk_paned_pack1 (paned, child, !resize, shrink);
  else
    gtk_paned_pack2 (paned, child, !resize, shrink);
  g_object_unref (child);
}

void
toggle_shrink (GtkWidget *widget,
               GtkWidget *child)
{
  GtkWidget *parent;
  GtkPaned *paned;
  gboolean is_child1;
  gboolean resize, shrink;

  parent = gtk_widget_get_parent (child);
  paned = GTK_PANED (parent);

  is_child1 = (child == gtk_paned_get_child1 (paned));

  if (is_child1)
    g_object_get (paned,
                  "resize-child1", &resize,
                  "shrink-child1", &shrink,
                  NULL);
  else
    g_object_get (paned,
                  "resize-child2", &resize,
                  "shrink-child2", &shrink,
                  NULL);

  g_object_ref (child);
  gtk_container_remove (GTK_CONTAINER (parent), child);
  if (is_child1)
    gtk_paned_pack1 (paned, child, resize, !shrink);
  else
    gtk_paned_pack2 (paned, child, resize, !shrink);
  g_object_unref (child);
}

GtkWidget *
create_pane_options (GtkPaned    *paned,
                     const gchar *frame_label,
                     const gchar *label1,
                     const gchar *label2)
{
  GtkWidget *child1, *child2;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *check_button;

  child1 = gtk_paned_get_child1 (paned);
  child2 = gtk_paned_get_child2 (paned);

  frame = gtk_frame_new (frame_label);
  g_object_set (frame, "margin", 4, NULL);

  table = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (frame), table);

  label = gtk_label_new (label1);
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

  check_button = gtk_check_button_new_with_mnemonic ("_Resize");
  gtk_grid_attach (GTK_GRID (table), check_button, 0, 1, 1, 1);
  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (toggle_resize), child1);

  check_button = gtk_check_button_new_with_mnemonic ("_Shrink");
  gtk_grid_attach (GTK_GRID (table), check_button, 0, 2, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);
  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (toggle_shrink), child1);

  label = gtk_label_new (label2);
  gtk_grid_attach (GTK_GRID (table), label, 1, 0, 1, 1);

  check_button = gtk_check_button_new_with_mnemonic ("_Resize");
  gtk_grid_attach (GTK_GRID (table), check_button, 1, 1, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);
  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (toggle_resize), child2);

  check_button = gtk_check_button_new_with_mnemonic ("_Shrink");
  gtk_grid_attach (GTK_GRID (table), check_button, 1, 2, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);
  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (toggle_shrink), child2);

  return frame;
}

GtkWidget *
do_panes (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *button;
  GtkWidget *vbox;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "Paned Widgets");

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
      g_object_set (vpaned, "margin", 5, NULL);
      gtk_container_add (GTK_CONTAINER (vbox), vpaned);

      hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_paned_add1 (GTK_PANED (vpaned), hpaned);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_size_request (frame, 60, 60);
      gtk_paned_add1 (GTK_PANED (hpaned), frame);

      button = gtk_button_new_with_mnemonic ("_Hi there");
      gtk_container_add (GTK_CONTAINER(frame), button);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_size_request (frame, 80, 60);
      gtk_paned_add2 (GTK_PANED (hpaned), frame);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_size_request (frame, 60, 80);
      gtk_paned_add2 (GTK_PANED (vpaned), frame);

      /* Now create toggle buttons to control sizing */

      gtk_container_add (GTK_CONTAINER (vbox),
                          create_pane_options (GTK_PANED (hpaned),
                                               "Horizontal",
                                               "Left",
                                               "Right"));

      gtk_container_add (GTK_CONTAINER (vbox),
                          create_pane_options (GTK_PANED (vpaned),
                                               "Vertical",
                                               "Top",
                                               "Bottom"));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
