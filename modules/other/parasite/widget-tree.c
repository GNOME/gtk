/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "parasite.h"
#include "prop-list.h"
#include "widget-tree.h"
#include <string.h>

enum
{
  OBJECT,
  OBJECT_TYPE,
  OBJECT_NAME,
  WIDGET_REALIZED,
  WIDGET_VISIBLE,
  WIDGET_MAPPED,
  OBJECT_ADDRESS,
  SENSITIVE,
  NUM_COLUMNS
};


enum
{
  WIDGET_CHANGED,
  LAST_SIGNAL
};


struct _ParasiteWidgetTreePrivate
{
  GtkTreeStore *model;
  GHashTable *iters;
};

static guint widget_tree_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteWidgetTree, parasite_widget_tree, GTK_TYPE_TREE_VIEW)

static void
parasite_widget_tree_on_widget_selected(GtkTreeSelection *selection,
                                        ParasiteWidgetTree *widget_tree)
{
    g_signal_emit(widget_tree, widget_tree_signals[WIDGET_CHANGED], 0);
}


static void
parasite_widget_tree_init (ParasiteWidgetTree *widget_tree)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    widget_tree->priv = parasite_widget_tree_get_instance_private (widget_tree);

    widget_tree->priv->iters = g_hash_table_new_full (g_direct_hash,
                                                      g_direct_equal,
                                                      NULL,
                                                      (GDestroyNotify) gtk_tree_iter_free);

    widget_tree->priv->model = gtk_tree_store_new(
        NUM_COLUMNS,
        G_TYPE_POINTER, // OBJECT
        G_TYPE_STRING,  // OBJECT_TYPE
        G_TYPE_STRING,  // OBJECT_NAME
        G_TYPE_BOOLEAN, // WIDGET_REALIZED
        G_TYPE_BOOLEAN, // WIDGET_VISIBLE
        G_TYPE_BOOLEAN, // WIDGET_MAPPED
        G_TYPE_STRING,  // OBJECT_ADDRESS
        G_TYPE_BOOLEAN);// SENSITIVE

    gtk_tree_view_set_model(GTK_TREE_VIEW(widget_tree),
                            GTK_TREE_MODEL(widget_tree->priv->model));
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(widget_tree), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(widget_tree), OBJECT_NAME);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget_tree));
    g_signal_connect(G_OBJECT(selection), "changed",
                     G_CALLBACK(parasite_widget_tree_on_widget_selected),
                     widget_tree);

    // Widget column
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "scale", TREE_TEXT_SCALE, NULL);
    column = gtk_tree_view_column_new_with_attributes("Widget", renderer,
                                                      "text", OBJECT_TYPE,
                                                      "sensitive", SENSITIVE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(widget_tree), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    // Name column
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "scale", TREE_TEXT_SCALE, NULL);
    column = gtk_tree_view_column_new_with_attributes("Name", renderer,
                                                      "text", OBJECT_NAME,
                                                      "sensitive", SENSITIVE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(widget_tree), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    // Realized column
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer),
                 "activatable", TRUE,
                 "indicator-size", TREE_CHECKBOX_SIZE,
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Realized",
                                                      renderer,
                                                      "active", WIDGET_REALIZED,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(widget_tree), column);

    // Mapped column
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer),
                 "activatable", TRUE,
                 "indicator-size", TREE_CHECKBOX_SIZE,
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Mapped",
                                                      renderer,
                                                      "active", WIDGET_MAPPED,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(widget_tree), column);

    // Visible column
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer),
                 "activatable", TRUE,
                 "indicator-size", TREE_CHECKBOX_SIZE,
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Visible",
                                                      renderer,
                                                      "active", WIDGET_VISIBLE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(widget_tree), column);

    // Poinder Address column
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer),
                 "scale", TREE_TEXT_SCALE,
                 "family", "monospace",
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Pointer Address",
                                                      renderer,
                                                      "text", OBJECT_ADDRESS,
                                                      "sensitive", SENSITIVE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(widget_tree), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

  parasite_widget_tree_append_object (widget_tree, G_OBJECT (gtk_settings_get_default ()), NULL);
}


static void
parasite_widget_tree_class_init(ParasiteWidgetTreeClass *klass)
{
    klass->widget_changed = NULL;

    widget_tree_signals[WIDGET_CHANGED] =
        g_signal_new("widget-changed",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(ParasiteWidgetTreeClass, widget_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}


GtkWidget *
parasite_widget_tree_new ()
{
  return g_object_new (PARASITE_TYPE_WIDGET_TREE, NULL);
}


GObject *
parasite_widget_tree_get_selected_object (ParasiteWidgetTree *widget_tree)
{
  GtkTreeIter iter;
  GtkTreeSelection *sel;
  GtkTreeModel *model;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget_tree));

  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    {
      GObject *object;
      gtk_tree_model_get (model, &iter,
                          OBJECT, &object,
                          -1);
      return object;
    }

  return NULL;
}

static void
on_container_forall(GtkWidget *widget, gpointer data)
{
    GList **list = (GList **)data;

    *list = g_list_append(*list, widget);
}

void
parasite_widget_tree_append_object (ParasiteWidgetTree *widget_tree,
                                    GObject            *object,
                                    GtkTreeIter        *parent_iter)
{
  GtkTreeIter iter;
  const char *class_name = G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object));
  const char *name = NULL;
  char *address;
  gboolean realized;
  gboolean mapped;
  gboolean visible;
  gboolean is_widget;
  GList *l;

  realized = mapped = visible = FALSE;

  is_widget = GTK_IS_WIDGET (object);
  if (is_widget)
    {
      GtkWidget *widget = GTK_WIDGET (object);
      name = gtk_widget_get_name (GTK_WIDGET (object));

      realized = gtk_widget_get_realized  (widget);
      mapped = gtk_widget_get_mapped (widget);
      visible = gtk_widget_get_visible (widget);
    }

  if (name == NULL || g_strcmp0 (name, class_name) == 0)
    {
      if (GTK_IS_LABEL (object))
        {
          name = gtk_label_get_text (GTK_LABEL (object));
        }
      else if (GTK_IS_BUTTON (object))
        {
          name = gtk_button_get_label (GTK_BUTTON (object));
        }
      else if (GTK_IS_WINDOW (object))
        {
          name = gtk_window_get_title (GTK_WINDOW (object));
        }
      else
        {
          name = "";
        }
    }

  address = g_strdup_printf ("%p", object);

  gtk_tree_store_append (widget_tree->priv->model, &iter, parent_iter);
  gtk_tree_store_set (widget_tree->priv->model, &iter,
                      OBJECT, object,
                      OBJECT_TYPE, class_name,
                      OBJECT_NAME, name,
                      WIDGET_REALIZED, realized,
                      WIDGET_MAPPED, mapped,
                      WIDGET_VISIBLE, visible,
                      OBJECT_ADDRESS, address,
                      SENSITIVE, !is_widget || (realized && mapped && visible),
                      -1);
  g_hash_table_insert (widget_tree->priv->iters, object, gtk_tree_iter_copy (&iter));

  g_free(address);

  if (GTK_IS_CONTAINER (object))
    {
      GList* children = NULL;

      /* Pick up all children, including those that are internal. */
      gtk_container_forall (GTK_CONTAINER (object),
                            on_container_forall, &children);

      for (l = children; l != NULL; l = l->next)
        {
          parasite_widget_tree_append_object (widget_tree, l->data, &iter);
        }

      g_list_free(children);
    }
}

void
parasite_widget_tree_scan (ParasiteWidgetTree *widget_tree,
                           GtkWidget          *window)
{
  gtk_tree_store_clear (widget_tree->priv->model);
  g_hash_table_remove_all (widget_tree->priv->iters);
  parasite_widget_tree_append_object (widget_tree, G_OBJECT (gtk_settings_get_default ()), NULL);
  parasite_widget_tree_append_object (widget_tree, G_OBJECT (window), NULL);

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget_tree));
}


/*
static GList *
get_parents(GtkWidget *widget,
            GList *parents)
{
    GtkWidget *parent = gtk_widget_get_parent(widget);

    parents = g_list_prepend(parents, widget);

    if (parent != NULL)
        return get_parents(parent, parents);

    return parents;
}

gboolean
parasite_widget_tree_find_widget (ParasiteWidgetTree *widget_tree,
                                  GtkWidget          *widget,
                                  GtkTreeIter        *iter)
{
  GList *parents = get_parents (widget, NULL);
  GList *l;
  GtkTreeIter inner_iter, parent_iter = {0};
  gboolean found = FALSE;
  gboolean in_root = TRUE;

  for (l = parents; l != NULL; l = l->next)
    {
      GtkWidget *cur_widget = GTK_WIDGET (l->data);
      gboolean valid;
      found = FALSE;

      for (valid = gtk_tree_model_iter_children (widget_tree->priv->model,
                                                 &inner_iter,
                                                 in_root ? NULL : &parent_iter);
           valid;
           valid = gtk_tree_model_iter_next (widget_tree->priv->model, &inner_iter))
        {
          GtkWidget *iter_widget;
          gtk_tree_model_get (widget_tree->priv->model,
                              &inner_iter,
                              WIDGET, &iter_widget,
                              -1);
          if (iter_widget == cur_widget)
            {
              parent_iter = inner_iter;
              in_root = FALSE;
              found = TRUE;
              break;
            }
        }
    }

  g_list_free(parents);

  *iter = inner_iter;
  return found;
}
*/

gboolean
parasite_widget_tree_find_object (ParasiteWidgetTree *widget_tree,
                                  GObject            *object,
                                  GtkTreeIter        *iter)
{
  GtkTreeIter *internal_iter = g_hash_table_lookup (widget_tree->priv->iters, object);
  if (internal_iter)
    {
      *iter = *internal_iter;
      return TRUE;
    }

  return FALSE;
}

void
parasite_widget_tree_select_object (ParasiteWidgetTree *widget_tree,
                                    GObject            *object)
{
  GtkTreeIter iter;

  if (parasite_widget_tree_find_object (widget_tree, object, &iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (widget_tree->priv->model), &iter);
      gtk_tree_view_expand_to_path(GTK_TREE_VIEW(widget_tree), path);
      gtk_tree_selection_select_iter(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(widget_tree)),
            &iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (widget_tree), path, NULL, FALSE, 0, 0);
    }

}


// vim: set et sw=4 ts=4:
