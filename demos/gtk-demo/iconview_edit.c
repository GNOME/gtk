/* Icon View/Editing and Drag-and-Drop
 *
 * The GtkIconView widget supports Editing and Drag-and-Drop.
 * This example also demonstrates using the generic GtkCellLayout
 * interface to set up cell renderers in an icon view.
 */

#include <gtk/gtk.h>
#include <string.h>
#include "demo-common.h"

static GtkWidget *window = NULL;

enum
{
  COL_TEXT,
  NUM_COLS
};


static void
fill_store (GtkListStore *store)
{
  GtkTreeIter iter;
  const gchar *text[] = { "Red", "Green", "Blue", "Yellow" };
  gint i;

  /* First clear the store */
  gtk_list_store_clear (store);

  for (i = 0; i < 4; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, COL_TEXT, text[i], -1);
    }
}

static GtkListStore *
create_store (void)
{
  GtkListStore *store;

  store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING);

  return store;
}

static void
set_cell_color (GtkCellLayout   *cell_layout,
		GtkCellRenderer *cell,
		GtkTreeModel    *tree_model,
		GtkTreeIter     *iter,
		gpointer         data)
{
  gchar *text;
  GdkColor color;
  guint32 pixel = 0;
  GdkPixbuf *pixbuf;

  gtk_tree_model_get (tree_model, iter, COL_TEXT, &text, -1);
  if (gdk_color_parse (text, &color))
    pixel =
      (color.red   >> 8) << 24 |
      (color.green >> 8) << 16 |
      (color.blue  >> 8) << 8;

  g_free (text);

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 24, 24);
  gdk_pixbuf_fill (pixbuf, pixel);

  g_object_set (cell, "pixbuf", pixbuf, NULL);

  g_object_unref (pixbuf);
}

static void
edited (GtkCellRendererText *cell,
	gchar               *path_string,
	gchar               *text,
	gpointer             data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  model = gtk_icon_view_get_model (GTK_ICON_VIEW (data));
  path = gtk_tree_path_new_from_string (path_string);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      COL_TEXT, text, -1);

  gtk_tree_path_free (path);
}

GtkWidget *
do_iconview_edit (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *icon_view;
      GtkListStore *store;
      GtkCellRenderer *renderer;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Editing and Drag-and-Drop");

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      store = create_store ();
      fill_store (store);

      icon_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (store));
      g_object_unref (store);

      gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (icon_view),
					GTK_SELECTION_SINGLE);
      gtk_icon_view_set_orientation (GTK_ICON_VIEW (icon_view),
				     GTK_ORIENTATION_HORIZONTAL);
      gtk_icon_view_set_columns (GTK_ICON_VIEW (icon_view), 2);
      gtk_icon_view_set_reorderable (GTK_ICON_VIEW (icon_view), TRUE);

      renderer = gtk_cell_renderer_pixbuf_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_view),
				  renderer, TRUE);
      gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (icon_view),
					  renderer,
					  set_cell_color,
					  NULL, NULL);

      renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_view),
				  renderer, TRUE);
      g_object_set (renderer, "editable", TRUE, NULL);
      g_signal_connect (renderer, "edited", G_CALLBACK (edited), icon_view);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
				      renderer,
				      "text", COL_TEXT,
				      NULL);

      gtk_container_add (GTK_CONTAINER (window), icon_view);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}

