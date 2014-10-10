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
  GtkTreeViewColumn *column_self;
  GtkCellRenderer *renderer_self;
  GtkTreeViewColumn *column_cumulative;
  GtkCellRenderer *renderer_cumulative;
  GtkWidget *button;
};

enum
{
  COLUMN_TYPE,
  COLUMN_SELF,
  COLUMN_CUMULATIVE
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorStatistics, gtk_inspector_statistics, GTK_TYPE_BOX)

static void
cell_data_func (GtkCellLayout   *layout,
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

static gint
add_type_count (GtkInspectorStatistics *sl, GType type)
{
  gint cumulative;
  gint self;
  GType *children;
  guint n_children;
  GtkTreeIter iter;
  gint i;

  cumulative = 0;

  children = g_type_children (type, &n_children);
  for (i = 0; i < n_children; i++)
    cumulative += add_type_count (sl, children[i]);

  self = g_type_get_instance_count (type);
  cumulative += self;
  gtk_list_store_append (GTK_LIST_STORE (sl->priv->model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (sl->priv->model), &iter,
                      COLUMN_TYPE, g_type_name (type),
                      COLUMN_SELF, self,
                      COLUMN_CUMULATIVE, cumulative,
                      -1);

  return cumulative;
}

static void
refresh_clicked (GtkWidget *button, GtkInspectorStatistics *sl)
{
  GType type;
  gpointer class;

  gtk_list_store_clear (GTK_LIST_STORE (sl->priv->model));

  for (type = G_TYPE_INTERFACE; type <= G_TYPE_FUNDAMENTAL_MAX; type += G_TYPE_FUNDAMENTAL_SHIFT)
    {
      class = g_type_class_peek (type);
      if (class == NULL)
        continue;

      if (!G_TYPE_IS_INSTANTIATABLE (type))
        continue;

      add_type_count (sl, type);
    }
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
gtk_inspector_statistics_init (GtkInspectorStatistics *sl)
{
  sl->priv = gtk_inspector_statistics_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (sl->priv->column_self),
                                      sl->priv->renderer_self,
                                      cell_data_func,
                                      GINT_TO_POINTER (COLUMN_SELF), NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (sl->priv->column_cumulative),
                                      sl->priv->renderer_cumulative,
                                      cell_data_func,
                                      GINT_TO_POINTER (COLUMN_CUMULATIVE), NULL);
  if (has_instance_counts ())
    add_type_count (sl, G_TYPE_OBJECT);
  else
    gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "excuse");
}

static void
gtk_inspector_statistics_class_init (GtkInspectorStatisticsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/statistics.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, column_self);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, renderer_self);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, column_cumulative);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, renderer_cumulative);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, button);
  gtk_widget_class_bind_template_callback (widget_class, refresh_clicked);
}

// vim: set et sw=2 ts=2:
