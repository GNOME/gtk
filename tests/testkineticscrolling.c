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
  GtkWidget *window, *swindow, *table;
  GtkWidget *label;
  GtkWidget *vbox, *button;
  GtkWidget *treeview;
  GtkCellRenderer *renderer;
  GtkListStore *store;
  GtkWidget *textview;
  gint i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  g_signal_connect (window, "delete_event",
                    G_CALLBACK (gtk_main_quit), NULL);

  table = gtk_table_new (2, 3, FALSE);

  label = gtk_label_new ("Non scrollable widget using viewport");
  gtk_table_attach (GTK_TABLE (table), label,
                    0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("Scrollable widget: TreeView");
  gtk_table_attach (GTK_TABLE (table), label,
                    1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("Scrollable widget: TextView");
  gtk_table_attach (GTK_TABLE (table), label,
                    2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 1);
  for (i = 0; i < 80; i++)
    {
      gchar *label = g_strdup_printf ("Button number %d", i);

      button = gtk_button_new_with_label (label);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (on_button_clicked),
                        GINT_TO_POINTER (i));
      g_free (label);
    }

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (swindow), vbox);
  gtk_widget_show (vbox);

  gtk_table_attach_defaults (GTK_TABLE (table), swindow,
                             0, 1, 1, 2);
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
  gtk_container_add (GTK_CONTAINER (swindow), treeview);
  gtk_widget_show (treeview);

  gtk_table_attach_defaults (GTK_TABLE (table), swindow,
                             1, 2, 1, 2);
  gtk_widget_show (swindow);

  textview = gtk_text_view_new ();
  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_container_add (GTK_CONTAINER (swindow), textview);
  gtk_widget_show (textview);

  gtk_table_attach_defaults (GTK_TABLE (table), swindow,
                             2, 3, 1, 2);
  gtk_widget_show (swindow);

  gtk_container_add (GTK_CONTAINER (window), table);
  gtk_widget_show (table);

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
