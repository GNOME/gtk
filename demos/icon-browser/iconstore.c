#include "iconstore.h"
#include <gtk/gtk.h>

struct _IconStore
{
  GtkListStore parent;

  gint text_column;
};

struct _IconStoreClass
{
  GtkListStoreClass parent_class;
};

static void icon_store_drag_source_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (IconStore, icon_store, GTK_TYPE_LIST_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                icon_store_drag_source_init))


static void
icon_store_init (IconStore *store)
{
  GType types[4] = { G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING };

  gtk_list_store_set_column_types (GTK_LIST_STORE (store), 4, types);

  store->text_column = ICON_STORE_NAME_COLUMN;
}

static void
icon_store_class_init (IconStoreClass *class)
{
}

static gboolean
row_draggable (GtkTreeDragSource *drag_source,
               GtkTreePath       *path)
{
  return TRUE;
}

static gboolean
drag_data_delete (GtkTreeDragSource *drag_source,
                  GtkTreePath       *path)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source), &iter, path))
    return gtk_list_store_remove (GTK_LIST_STORE (drag_source), &iter);
  return FALSE;
}

static gboolean
drag_data_get (GtkTreeDragSource *drag_source,
               GtkTreePath       *path,
               GtkSelectionData  *selection)
{
  GtkTreeIter iter;
  gchar *text;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source), &iter, path))
    return FALSE;

  gtk_tree_model_get (GTK_TREE_MODEL (drag_source), &iter,
                      ICON_STORE (drag_source)->text_column, &text,
                      -1);

  gtk_selection_data_set_text (selection, text, -1);

  g_free (text);

  return TRUE;
}


static void
icon_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = row_draggable;
  iface->drag_data_delete = drag_data_delete;
  iface->drag_data_get = drag_data_get;
}

void
icon_store_set_text_column (IconStore *store, gint text_column)
{
  store->text_column = text_column;
}
