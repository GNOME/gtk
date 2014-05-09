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

#include "prop-list.h"
#include "widget-tree.h"
#include <string.h>

enum
{
  OBJECT,
  OBJECT_TYPE,
  OBJECT_NAME,
  OBJECT_ADDRESS,
  SENSITIVE
};


enum
{
  WIDGET_CHANGED,
  LAST_SIGNAL
};


struct _GtkInspectorWidgetTreePrivate
{
  GtkTreeStore *model;
  GHashTable *iters;
};

static guint widget_tree_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorWidgetTree, gtk_inspector_widget_tree, GTK_TYPE_TREE_VIEW)

static void
on_widget_selected (GtkTreeSelection       *selection,
                    GtkInspectorWidgetTree *wt)
{
  g_signal_emit (wt, widget_tree_signals[WIDGET_CHANGED], 0);
}

typedef struct
{
  GObject *object;
  GtkTreeIter *iter;
  gulong map_handler;
  gulong unmap_handler;
} ObjectData;

static void
object_data_free (gpointer data)
{
  ObjectData *od = data;

  gtk_tree_iter_free (od->iter);

  if (od->map_handler)
    {
      g_signal_handler_disconnect (od->object, od->map_handler);
      g_signal_handler_disconnect (od->object, od->unmap_handler);
    }

  g_free (od);
}

static void
gtk_inspector_widget_tree_init (GtkInspectorWidgetTree *wt)
{
  wt->priv = gtk_inspector_widget_tree_get_instance_private (wt);
  wt->priv->iters = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify) object_data_free);
  gtk_widget_init_template (GTK_WIDGET (wt));

  gtk_inspector_widget_tree_append_object (wt, G_OBJECT (gtk_settings_get_default ()), NULL, NULL);
}

static void
gtk_inspector_widget_tree_class_init (GtkInspectorWidgetTreeClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  klass->widget_changed = NULL;

  widget_tree_signals[WIDGET_CHANGED] =
      g_signal_new ("widget-changed",
                    G_OBJECT_CLASS_TYPE(klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                    G_STRUCT_OFFSET(GtkInspectorWidgetTreeClass, widget_changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/widget-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorWidgetTree, model);
  gtk_widget_class_bind_template_callback (widget_class, on_widget_selected);
}

GtkWidget *
gtk_inspector_widget_tree_new (void)
{
  return g_object_new (GTK_TYPE_INSPECTOR_WIDGET_TREE, NULL);
}


GObject *
gtk_inspector_widget_tree_get_selected_object (GtkInspectorWidgetTree *wt)
{
  GtkTreeIter iter;
  GtkTreeSelection *sel;
  GtkTreeModel *model;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt));

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
map_or_unmap (GtkWidget *widget, GtkInspectorWidgetTree *wt)
{
  GtkTreeIter iter;

  if (gtk_inspector_widget_tree_find_object (wt, G_OBJECT (widget), &iter))
    {
      gtk_tree_store_set (wt->priv->model, &iter,
                          SENSITIVE, gtk_widget_get_mapped (widget),
                          -1);
    }
}

typedef struct
{
  GtkInspectorWidgetTree *wt;
  GtkTreeIter *iter;
} FindAllData;

static void
on_container_forall (GtkWidget *widget,
                     gpointer   data)
{
  FindAllData *d = data;
  gtk_inspector_widget_tree_append_object (d->wt, G_OBJECT (widget), d->iter, NULL);
}

void
gtk_inspector_widget_tree_append_object (GtkInspectorWidgetTree *wt,
                                         GObject                *object,
                                         GtkTreeIter            *parent_iter,
                                         const gchar            *name)
{
  GtkTreeIter iter;
  const gchar *class_name = G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object));
  gchar *address;
  gboolean mapped;
  ObjectData *od;

  mapped = FALSE;

  if (GTK_IS_WIDGET (object))
    {
      GtkWidget *widget = GTK_WIDGET (object);
       if (name == NULL)
         name = gtk_widget_get_name (GTK_WIDGET (object));
      mapped = gtk_widget_get_mapped (widget);
    }

  if (name == NULL || g_strcmp0 (name, class_name) == 0)
    {
      if (GTK_IS_LABEL (object))
        name = gtk_label_get_text (GTK_LABEL (object));
      else if (GTK_IS_BUTTON (object))
        name = gtk_button_get_label (GTK_BUTTON (object));
      else if (GTK_IS_WINDOW (object))
        name = gtk_window_get_title (GTK_WINDOW (object));
      else
        name = "";
    }

  address = g_strdup_printf ("%p", object);

  gtk_tree_store_append (wt->priv->model, &iter, parent_iter);
  gtk_tree_store_set (wt->priv->model, &iter,
                      OBJECT, object,
                      OBJECT_TYPE, class_name,
                      OBJECT_NAME, name,
                      OBJECT_ADDRESS, address,
                      SENSITIVE, !GTK_IS_WIDGET (object) || mapped,
                      -1);

  od = g_new0 (ObjectData, 1);
  od->object = object;
  od->iter = gtk_tree_iter_copy (&iter);
  if (GTK_IS_WIDGET (object))
    {
      od->map_handler = g_signal_connect (object, "map", G_CALLBACK (map_or_unmap), wt);
      od->unmap_handler = g_signal_connect (object, "unmap", G_CALLBACK (map_or_unmap), wt);
    }

  g_hash_table_insert (wt->priv->iters, object, od);

  g_free (address);

  if (GTK_IS_CONTAINER (object))
    {
      FindAllData data;

      data.wt = wt;
      data.iter = &iter;

      gtk_container_forall (GTK_CONTAINER (object), on_container_forall, &data);
    }

  if (GTK_IS_TREE_VIEW (object))
    {
      gint n_columns, i;
      GObject *column;

      n_columns = gtk_tree_view_get_n_columns (GTK_TREE_VIEW (object));
      for (i = 0; i < n_columns; i++)
        {
          column = G_OBJECT (gtk_tree_view_get_column (GTK_TREE_VIEW (object), i));
          gtk_inspector_widget_tree_append_object (wt, column, &iter, NULL);
        }
    }

  if (GTK_IS_CELL_LAYOUT (object))
    {
      GList *cells, *l;
      GObject *cell;
      GtkCellArea *area;

      area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (object));
      cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (object));
      for (l = cells; l; l = l->next)
        {
          cell = l->data;
          g_object_set_data (cell, "gtk-inspector-cell-area", area);
          gtk_inspector_widget_tree_append_object (wt, cell, &iter, NULL);
        }
      g_list_free (cells);
    }
}

void
gtk_inspector_widget_tree_scan (GtkInspectorWidgetTree *wt,
                                GtkWidget              *window)
{
  gtk_tree_store_clear (wt->priv->model);
  g_hash_table_remove_all (wt->priv->iters);
  gtk_inspector_widget_tree_append_object (wt, G_OBJECT (gtk_settings_get_default ()), NULL, NULL);
  if (g_application_get_default ())
    gtk_inspector_widget_tree_append_object (wt, G_OBJECT (g_application_get_default ()), NULL, NULL);
  gtk_inspector_widget_tree_append_object (wt, G_OBJECT (window), NULL, NULL);

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (wt));
}

gboolean
gtk_inspector_widget_tree_find_object (GtkInspectorWidgetTree *wt,
                                       GObject                *object,
                                       GtkTreeIter            *iter)
{
  ObjectData *od;

  od = g_hash_table_lookup (wt->priv->iters, object);
  if (od)
    {
      *iter = *od->iter;
      return TRUE;
    }

  return FALSE;
}

void
gtk_inspector_widget_tree_select_object (GtkInspectorWidgetTree *wt,
                                         GObject                *object)
{
  GtkTreeIter iter;

  if (gtk_inspector_widget_tree_find_object (wt, object, &iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (wt), path);
      gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (wt)), &iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (wt), path, NULL, FALSE, 0, 0);
    }
}


// vim: set et sw=2 ts=2:
