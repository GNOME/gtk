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

#include "gtktreeview.h"
#include "gtkcellrenderertext.h"
#include "gtktoggletoolbutton.h"

struct _GtkInspectorDataListPrivate
{
  GtkTreeModel *object;
  GtkTreeModel *types;
  GtkTreeView *view;
  gboolean show_data;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorDataList, gtk_inspector_data_list, GTK_TYPE_BOX)

static void
gtk_inspector_data_list_init (GtkInspectorDataList *sl)
{
  sl->priv = gtk_inspector_data_list_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static void
cell_data_func (GtkTreeViewColumn *col,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  gint num;
  GValue gvalue = { 0, };
  gchar *value;

  num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (col), "num"));
  gtk_tree_model_get_value (model, iter, num, &gvalue);
  value = g_strdup_value_contents (&gvalue);
  g_object_set (cell, "text", value ? value : "", NULL);
  g_free (value);
  g_value_unset (&gvalue);
}

static void
add_columns (GtkInspectorDataList *sl)
{
  gint n_columns;
  GtkCellRenderer *cell;
  GType type;
  gchar *title;
  GtkTreeViewColumn *col;
  gint i;

  n_columns = gtk_tree_model_get_n_columns (sl->priv->object);
  for (i = 0; i < n_columns; i++)
    {
      cell = gtk_cell_renderer_text_new ();
      type = gtk_tree_model_get_column_type (sl->priv->object, i);
      title = g_strdup_printf ("%d: %s", i, g_type_name (type));
      col = gtk_tree_view_column_new_with_attributes (title, cell, NULL);
      g_object_set_data (G_OBJECT (col), "num", GINT_TO_POINTER (i));
      gtk_tree_view_column_set_cell_data_func (col, cell, cell_data_func, sl, NULL);
      gtk_tree_view_append_column (sl->priv->view, col);
      g_free (title);
    } 
}

static void
show_types (GtkInspectorDataList *sl)
{
  gtk_tree_view_set_model (sl->priv->view, NULL);
  sl->priv->show_data = FALSE;
}

static void
show_data (GtkInspectorDataList *sl)
{
  gtk_tree_view_set_model (sl->priv->view, sl->priv->object);
  sl->priv->show_data = TRUE;
}

static void
clear_view (GtkInspectorDataList *sl)
{
  gtk_tree_view_set_model (sl->priv->view, NULL);
  while (gtk_tree_view_get_n_columns (sl->priv->view) > 0)
    gtk_tree_view_remove_column (sl->priv->view,
                                 gtk_tree_view_get_column (sl->priv->view, 0));
}

void
gtk_inspector_data_list_set_object (GtkInspectorDataList *sl,
                                    GObject              *object)
{
  clear_view (sl);
  sl->priv->object = NULL;
  sl->priv->show_data = FALSE;

  if (!GTK_IS_TREE_MODEL (object))
    {
      gtk_widget_hide (GTK_WIDGET (sl));
      return;
    }

  gtk_widget_show (GTK_WIDGET (sl));

  sl->priv->object = GTK_TREE_MODEL (object);
  add_columns (sl);
  show_types (sl);
}

static void
toggle_show (GtkToggleToolButton  *button,
             GtkInspectorDataList *sl)
{
  if (gtk_toggle_tool_button_get_active (button) == sl->priv->show_data)
    return;

  if (gtk_toggle_tool_button_get_active (button))
    show_data (sl);
  else
    show_types (sl);
}

static void
gtk_inspector_data_list_class_init (GtkInspectorDataListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/data-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorDataList, view);
  gtk_widget_class_bind_template_callback (widget_class, toggle_show);
}

// vim: set et sw=2 ts=2:
