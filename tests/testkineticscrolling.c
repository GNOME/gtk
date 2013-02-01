#include <gtk/gtk.h>

enum
{
  TARGET_GTK_TREE_MODEL_ROW
};

static GtkTargetEntry row_targets[] =
{
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, TARGET_GTK_TREE_MODEL_ROW }
};

static void
on_button_clicked (GtkWidget *widget, gpointer data)
{
  g_print ("Button %d clicked\n", GPOINTER_TO_INT (data));
}

static void
kinetic_scrolling (void)
{
  GtkWidget *window, *swindow, *grid;
  GtkWidget *label;
  GtkWidget *button_grid, *button;
  GtkWidget *treeview;
  GtkCellRenderer *renderer;
  GtkListStore *store;
  GtkWidget *textview;
  gint i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  g_signal_connect (window, "delete_event",
                    G_CALLBACK (gtk_main_quit), NULL);

  grid = gtk_grid_new ();

  label = gtk_label_new ("Non scrollable widget using viewport");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_show (label);

  label = gtk_label_new ("Scrollable widget: TreeView");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_show (label);

  label = gtk_label_new ("Scrollable widget: TextView");
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_show (label);

  button_grid = gtk_grid_new ();
  for (i = 0; i < 80; i++)
    {
      gchar *label = g_strdup_printf ("Button number %d", i);

      button = gtk_button_new_with_label (label);
      gtk_grid_attach (GTK_GRID (button_grid), button, 0, i, 1, 1);
      gtk_widget_set_hexpand (button, TRUE);
      gtk_widget_show (button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (on_button_clicked),
                        GINT_TO_POINTER (i));
      g_free (label);
    }

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_set_capture_button_press (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_container_add (GTK_CONTAINER (swindow), button_grid);
  gtk_widget_show (button_grid);

  gtk_grid_attach (GTK_GRID (grid), swindow, 0, 1, 1, 1);
  gtk_widget_show (swindow);

  treeview = gtk_tree_view_new ();
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview),
                                          GDK_BUTTON1_MASK,
                                          row_targets,
                                          G_N_ELEMENTS (row_targets),
                                          GDK_ACTION_MOVE | GDK_ACTION_COPY);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (treeview),
                                        row_targets,
                                        G_N_ELEMENTS (row_targets),
                                        GDK_ACTION_MOVE | GDK_ACTION_COPY);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "editable", TRUE, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               0, "Title",
                                               renderer,
                                               "text", 0,
                                               NULL);
  store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = 0; i < 80; i++)
    {
      GtkTreeIter iter;
      gchar *label = g_strdup_printf ("Row number %d", i);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, label, -1);
      g_free (label);
    }
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
  g_object_unref (store);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_set_capture_button_press (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_container_add (GTK_CONTAINER (swindow), treeview);
  gtk_widget_show (treeview);

  gtk_grid_attach (GTK_GRID (grid), swindow, 1, 1, 1, 1);
  gtk_widget_set_hexpand (swindow, TRUE);
  gtk_widget_set_vexpand (swindow, TRUE);
  gtk_widget_show (swindow);

  textview = gtk_text_view_new ();
  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_set_capture_button_press (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_container_add (GTK_CONTAINER (swindow), textview);
  gtk_widget_show (textview);

  gtk_grid_attach (GTK_GRID (grid), swindow, 2, 1, 1, 1);
  gtk_widget_set_hexpand (swindow, TRUE);
  gtk_widget_set_vexpand (swindow, TRUE);
  gtk_widget_show (swindow);

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show (grid);

  gtk_widget_show (window);
}

int
main (int argc, char **argv)
{
  gtk_init (NULL, NULL);

  kinetic_scrolling ();

  gtk_main ();

  return 0;
}
