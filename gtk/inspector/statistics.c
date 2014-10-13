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

#include "statistics.h"

#include "gtkstack.h"
#include "gtktreeview.h"
#include "gtkcellrenderertext.h"
#include "gtkcelllayout.h"

struct _GtkInspectorStatisticsPrivate
{
  GtkWidget *stack;
  GtkTreeModel *model;
  GtkTreeView  *view;
  GtkWidget *button;
  GHashTable *data;
  GtkTreeViewColumn *column_self1;
  GtkCellRenderer *renderer_self1;
  GtkTreeViewColumn *column_cumulative1;
  GtkCellRenderer *renderer_cumulative1;
  GtkTreeViewColumn *column_self2;
  GtkCellRenderer *renderer_self2;
  GtkTreeViewColumn *column_cumulative2;
  GtkCellRenderer *renderer_cumulative2;
  gint snapshot_count;
  GHashTable *counts;
};

typedef struct {
  GType type;
  gint self1;
  gint cumulative1;
  gint self2;
  gint cumulative2;
} TypeData;

enum
{
  COLUMN_TYPE,
  COLUMN_TYPE_NAME,
  COLUMN_SELF1,
  COLUMN_CUMULATIVE1,
  COLUMN_SELF2,
  COLUMN_CUMULATIVE2
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorStatistics, gtk_inspector_statistics, GTK_TYPE_BOX)

static gint
add_type_count (GtkInspectorStatistics *sl, GType type)
{
  gint cumulative;
  gint self;
  GType *children;
  guint n_children;
  gint i;
  TypeData *data;

  cumulative = 0;

  children = g_type_children (type, &n_children);
  for (i = 0; i < n_children; i++)
    cumulative += add_type_count (sl, children[i]);

  data = g_hash_table_lookup (sl->priv->counts, GSIZE_TO_POINTER (type));
  if (!data)
    {
      data = g_new0 (TypeData, 1);
      data->type = type;
      g_hash_table_insert (sl->priv->counts, GSIZE_TO_POINTER (type), data);
    }

  self = g_type_get_instance_count (type);
  cumulative += self;

  data->self2 = self;
  data->cumulative2 = cumulative;

  return cumulative;
}

static void
update_type_counts (GtkInspectorStatistics *sl)
{
  GType type;
  gpointer class;

  for (type = G_TYPE_INTERFACE; type <= G_TYPE_FUNDAMENTAL_MAX; type += (1 << G_TYPE_FUNDAMENTAL_SHIFT))
    {
      class = g_type_class_peek (type);
      if (class == NULL)
        continue;

      if (!G_TYPE_IS_INSTANTIATABLE (type))
        continue;

      add_type_count (sl, type);
    }
}

static void
refresh_clicked (GtkWidget *button, GtkInspectorStatistics *sl)
{
  GHashTableIter iter;
  TypeData *data;
  GtkTreeIter treeiter;

  g_hash_table_iter_init (&iter, sl->priv->counts);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&data))
    {
      data->self1 = data->self2;
      data->cumulative1 = data->cumulative2;
    }

  if (sl->priv->snapshot_count == 0)
    {
      gtk_tree_view_column_set_visible (sl->priv->column_self2, TRUE);
      gtk_tree_view_column_set_visible (sl->priv->column_cumulative2, TRUE);
    }
  else if (sl->priv->snapshot_count == 1)
    {
      gtk_tree_view_column_set_visible (sl->priv->column_self1, TRUE);
      gtk_tree_view_column_set_visible (sl->priv->column_cumulative1, TRUE);
    }

  update_type_counts (sl);

  gtk_list_store_clear (GTK_LIST_STORE (sl->priv->model));

  g_hash_table_iter_init (&iter, sl->priv->counts);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&data))
    {
      gtk_list_store_append (GTK_LIST_STORE (sl->priv->model), &treeiter);
      gtk_list_store_set (GTK_LIST_STORE (sl->priv->model), &treeiter,
                          COLUMN_TYPE, data->type,
                          COLUMN_TYPE_NAME, g_type_name (data->type),
                          COLUMN_SELF1, data->self1,
                          COLUMN_CUMULATIVE1, data->cumulative1,
                          COLUMN_SELF2, data->self2,
                          COLUMN_CUMULATIVE2, data->cumulative2,
                          -1);
    }

  sl->priv->snapshot_count++;
}

static gboolean
has_instance_counts (void)
{
  const gchar *string;
  guint flags = 0;

  string = g_getenv ("GOBJECT_DEBUG");
  if (string != NULL)
    {
      GDebugKey debug_keys[] = {
        { "objects", 1 },
        { "instance-count", 2 },
        { "signals", 4 }
      };

     flags = g_parse_debug_string (string, debug_keys, G_N_ELEMENTS (debug_keys));
    }

  return (flags & 2) != 0;
}

static void
cell_data_data (GtkCellLayout   *layout,
                GtkCellRenderer *cell,
                GtkTreeModel    *model,
                GtkTreeIter     *iter,
                gpointer         data)
{
  gint column;
  gint count;
  gchar *text;

  column = GPOINTER_TO_INT (data);

  gtk_tree_model_get (model, iter, column, &count, -1);

  text = g_strdup_printf ("%d", count);
  g_object_set (cell, "text", text, NULL);
  g_free (text);
}

static void
cell_data_delta (GtkCellLayout   *layout,
                 GtkCellRenderer *cell,
                 GtkTreeModel    *model,
                 GtkTreeIter     *iter,
                 gpointer         data)
{
  gint column;
  gint count1;
  gint count2;
  gchar *text;

  column = GPOINTER_TO_INT (data);

  gtk_tree_model_get (model, iter, column - 2, &count1, column, &count2, -1);

  if (count2 > count1)
    text = g_strdup_printf ("%d (↗ %d)", count2, count2 - count1);
  else if (count2 < count1)
    text = g_strdup_printf ("%d (↘ %d)", count2, count1 - count2);
  else
    text = g_strdup_printf ("%d", count2);
  g_object_set (cell, "text", text, NULL);
  g_free (text);
}

static void
gtk_inspector_statistics_init (GtkInspectorStatistics *sl)
{
  sl->priv = gtk_inspector_statistics_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (sl->priv->column_self1),
                                      sl->priv->renderer_self1,
                                      cell_data_data,
                                      GINT_TO_POINTER (COLUMN_SELF1), NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (sl->priv->column_cumulative1),
                                      sl->priv->renderer_cumulative1,
                                      cell_data_data,
                                      GINT_TO_POINTER (COLUMN_CUMULATIVE1), NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (sl->priv->column_self2),
                                      sl->priv->renderer_self2,
                                      cell_data_delta,
                                      GINT_TO_POINTER (COLUMN_SELF2), NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (sl->priv->column_cumulative2),
                                      sl->priv->renderer_cumulative2,
                                      cell_data_delta,
                                      GINT_TO_POINTER (COLUMN_CUMULATIVE2), NULL);
  sl->priv->counts = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  if (has_instance_counts ())
    update_type_counts (sl);
  else
    gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "excuse");
}

static void
finalize (GObject *object)
{
  GtkInspectorStatistics *sl = GTK_INSPECTOR_STATISTICS (object);

  g_hash_table_unref (sl->priv->counts);

  G_OBJECT_CLASS (gtk_inspector_statistics_parent_class)->finalize (object);
}

static void
gtk_inspector_statistics_class_init (GtkInspectorStatisticsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/statistics.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, column_self1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, renderer_self1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, column_cumulative1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, renderer_cumulative1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, column_self2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, renderer_self2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, column_cumulative2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, renderer_cumulative2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, button);
  gtk_widget_class_bind_template_callback (widget_class, refresh_clicked);
}

// vim: set et sw=2 ts=2:
