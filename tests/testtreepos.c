#include <gtk/gtk.h>

static gboolean
clicked_icon (GtkTreeView  *tv,
              gint          x,
              gint          y,
              GtkTreePath **path)
{
  GtkTreeViewColumn *col;
  gint cell_x, cell_y;
  gint cell_pos, cell_width;
  GList *cells, *l;
  gint depth;
  gint level_indentation;
  gint expander_size;
  gint indent;

  if (gtk_tree_view_get_path_at_pos (tv, x, y, path, &col, &cell_x, &cell_y))
    {
      cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (col));

#if 1
      /* ugly workaround to fix the problem:
       * manually calculate the indent for the row
       */
      depth = gtk_tree_path_get_depth (*path);
      level_indentation = gtk_tree_view_get_level_indentation (tv);
      gtk_widget_style_get (GTK_WIDGET (tv), "expander-size", &expander_size, NULL);
      expander_size += 4;
      indent = (depth - 1) * level_indentation + depth * expander_size;
#else
      indent = 0;
#endif

      for (l = cells; l; l = l->next)
        {
          gtk_tree_view_column_cell_get_position (col, l->data, &cell_pos, &cell_width);
          if (cell_pos + indent <= cell_x && cell_x <= cell_pos + indent + cell_width)
            {
              g_print ("clicked in %s\n", g_type_name_from_instance (l->data));
              if (GTK_IS_CELL_RENDERER_PIXBUF (l->data))
                {
                  g_list_free (cells);
                  return TRUE;
                }
            }
        }

      g_list_free (cells);
    }

  return FALSE;
}

static gboolean
release_event (GtkTreeView    *tv,
               GdkEventButton *event)
{
  GtkTreePath *path;

  if (event->type != GDK_BUTTON_RELEASE)
    return TRUE;

  if (clicked_icon (tv, event->x, event->y, &path))
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      gchar *text;

      model = gtk_tree_view_get_model (tv);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter, 0, &text, -1);

      g_print ("text was: %s\n", text);
      g_free (text);
      gtk_tree_path_free (path);

      return TRUE;
    }

  return FALSE;
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *tv;
  GtkTreeViewColumn *col;
  GtkCellRenderer *cell;
  GtkTreeStore *store;
  GtkTreeIter iter;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (window), sw);
  tv = gtk_tree_view_new ();
  gtk_container_add (GTK_CONTAINER (sw), tv);

  col = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (col, cell, TRUE);
  gtk_tree_view_column_add_attribute (col, cell, "text", 0);

  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (col, cell, FALSE);
  gtk_tree_view_column_add_attribute (col, cell, "active", 1);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (col, cell, TRUE);
  gtk_tree_view_column_add_attribute (col, cell, "text", 0);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (col, cell, FALSE);
  gtk_tree_view_column_add_attribute (col, cell, "icon-name", 2);

  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (col, cell, FALSE);
  gtk_tree_view_column_add_attribute (col, cell, "active", 1);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), col);

  store = gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
  gtk_tree_store_insert_with_values (store, NULL, NULL, 0, 0, "One row", 1, FALSE, 2, "document-open", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 1, 0, "Two row", 1, FALSE, 2, "dialog-warning", -1);
  gtk_tree_store_insert_with_values (store, NULL, &iter, 0, 0, "Three row", 1, FALSE, 2, "dialog-error", -1);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tv), GTK_TREE_MODEL (store));

  g_signal_connect (tv, "button-release-event",
                    G_CALLBACK (release_event), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
