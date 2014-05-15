/*
 * Copyright (c) 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include "data-list.h"

enum
{
  COLUMN_NUMBER,
  COLUMN_TYPE
};

struct _GtkInspectorDataListPrivate
{
  GtkListStore *model;
  GtkTreeViewColumn *number_column;
  GtkCellRenderer *number_renderer;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorDataList, gtk_inspector_data_list, GTK_TYPE_BOX)

static void
render_number (GtkTreeViewColumn *column,
               GtkCellRenderer   *renderer,
               GtkTreeModel      *model,
               GtkTreeIter       *iter,
               gpointer           data)
{
  gint number;
  gchar text[100];

  gtk_tree_model_get (model, iter, COLUMN_NUMBER, &number, -1);
  g_snprintf (text, 100, "%d", number);
  g_object_set (renderer, "text", text, NULL);
}

static void
gtk_inspector_data_list_init (GtkInspectorDataList *sl)
{
  sl->priv = gtk_inspector_data_list_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
  gtk_tree_view_column_set_cell_data_func (sl->priv->number_column,
                                           sl->priv->number_renderer,
                                           render_number,
                                           NULL, NULL);
}

void
gtk_inspector_data_list_set_object (GtkInspectorDataList *sl,
                                    GObject              *object)
{
  GtkTreeIter iter;
  gint i;

  gtk_list_store_clear (sl->priv->model);

  if (!GTK_IS_TREE_MODEL (object))
    {
      gtk_widget_hide (GTK_WIDGET (sl));
      return;
    }

  gtk_widget_show (GTK_WIDGET (sl));

  for (i = 0; i < gtk_tree_model_get_n_columns (GTK_TREE_MODEL (object)); i++)
    {
      GType type;
      type = gtk_tree_model_get_column_type (GTK_TREE_MODEL (object), i);

      gtk_list_store_append (sl->priv->model, &iter);
      gtk_list_store_set (sl->priv->model, &iter,
                          COLUMN_NUMBER, i,
                          COLUMN_TYPE, g_type_name (type),
                          -1);
    }  
}

static void
gtk_inspector_data_list_class_init (GtkInspectorDataListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/data-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorDataList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorDataList, number_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorDataList, number_renderer);
}

// vim: set et sw=2 ts=2:
