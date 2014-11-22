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

#include "signals-list.h"

#include "gtkcellrenderer.h"
#include "gtkliststore.h"
#include "gtktextbuffer.h"
#include "gtktogglebutton.h"
#include "gtktreeviewcolumn.h"
#include "gtklabel.h"

enum
{
  COLUMN_NAME,
  COLUMN_CLASS,
  COLUMN_CONNECTED,
  COLUMN_COUNT,
  COLUMN_NO_HOOKS,
  COLUMN_SIGNAL_ID,
  COLUMN_HOOK_ID
};

struct _GtkInspectorSignalsListPrivate
{
  GtkWidget *view;
  GtkListStore *model;
  GtkTextBuffer *text;
  GtkWidget *log_win;
  GtkWidget *trace_button;
  GtkWidget *clear_button;
  GtkWidget *object_title;
  GtkTreeViewColumn *count_column;
  GtkCellRenderer *count_renderer;
  GObject *object;
  GHashTable *iters;
  gboolean tracing;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorSignalsList, gtk_inspector_signals_list, GTK_TYPE_PANED)

static GType *
get_types (GObject *object, guint *length)
{
  GHashTable *seen;
  GType *ret;
  GType type;
  GType *iface;
  gint i;

  seen = g_hash_table_new (g_direct_hash, g_direct_equal);

  type = ((GTypeInstance*)object)->g_class->g_type;
  while (type)
    {
      g_hash_table_add (seen, GSIZE_TO_POINTER (type));
      iface = g_type_interfaces (type, NULL);
      for (i = 0; iface[i]; i++)
        g_hash_table_add (seen, GSIZE_TO_POINTER (iface[i]));
      g_free (iface);
      type = g_type_parent (type);
    }
 
  ret = (GType *)g_hash_table_get_keys_as_array (seen, length); 
  g_hash_table_unref (seen);

  return ret;
}

static void
add_signals (GtkInspectorSignalsList *sl,
             GType                    type,
             GObject                 *object)
{
  guint *ids;
  guint n_ids;
  gint i;
  GSignalQuery query;
  GtkTreeIter iter;
  gboolean has_handler;

  if (!G_TYPE_IS_INSTANTIATABLE (type) && !G_TYPE_IS_INTERFACE (type))
    return;

  ids = g_signal_list_ids (type, &n_ids);
  for (i = 0; i < n_ids; i++)
    {
      g_signal_query (ids[i], &query);
      has_handler = g_signal_has_handler_pending (object, ids[i], 0, TRUE);
      gtk_list_store_append (sl->priv->model, &iter);
      gtk_list_store_set (sl->priv->model, &iter,
                          COLUMN_NAME, query.signal_name,
                          COLUMN_CLASS, g_type_name (type),
                          COLUMN_CONNECTED, has_handler ? _("Yes") : "",
                          COLUMN_COUNT, 0,
                          COLUMN_NO_HOOKS, (query.signal_flags & G_SIGNAL_NO_HOOKS) != 0,
                          COLUMN_SIGNAL_ID, ids[i],
                          COLUMN_HOOK_ID, 0,
                          -1);
      g_hash_table_insert (sl->priv->iters,
                           GINT_TO_POINTER (ids[i]), gtk_tree_iter_copy (&iter));
    }
  g_free (ids);
}

static void
read_signals_from_object (GtkInspectorSignalsList *sl,
                          GObject                 *object)
{
  GType *types;
  guint length;
  gint i;

  types = get_types (object, &length);
  for (i = 0; i < length; i++)
    add_signals (sl, types[i], object);
  g_free (types);
}

static void stop_tracing (GtkInspectorSignalsList *sl);

void
gtk_inspector_signals_list_set_object (GtkInspectorSignalsList *sl,
                                       GObject                 *object)
{
  if (sl->priv->object == object)
    return;

  stop_tracing (sl);
  gtk_list_store_clear (sl->priv->model);
  g_hash_table_remove_all (sl->priv->iters);

  sl->priv->object = object;

  if (object)
    {
      const gchar *title;

      title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
      gtk_label_set_label (GTK_LABEL (sl->priv->object_title), title);

      read_signals_from_object (sl, object);
    }
}

static void
render_count (GtkTreeViewColumn *column,
              GtkCellRenderer   *renderer,
              GtkTreeModel      *model,
              GtkTreeIter       *iter,
              gpointer           data)
{
  gint count;
  gboolean no_hooks;
  gchar text[100];

  gtk_tree_model_get (model, iter,
                      COLUMN_COUNT, &count,
                      COLUMN_NO_HOOKS, &no_hooks,
                      -1);
  if (no_hooks)
    {
      g_object_set (renderer, "markup", "<i>(untraceable)</i>", NULL);
    }
  else if (count != 0)
    {
      g_snprintf (text, 100, "%d", count);
      g_object_set (renderer, "text", text, NULL);
    }
  else
    g_object_set (renderer, "text", "", NULL);
}

static void
gtk_inspector_signals_list_init (GtkInspectorSignalsList *sl)
{
  sl->priv = gtk_inspector_signals_list_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));

  gtk_tree_view_column_set_cell_data_func (sl->priv->count_column,
                                           sl->priv->count_renderer,
                                           render_count,
                                           NULL, NULL);

  sl->priv->iters = g_hash_table_new_full (g_direct_hash, 
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify) gtk_tree_iter_free);
}

static gboolean
trace_hook (GSignalInvocationHint *ihint,
            guint                  n_param_values,
            const GValue          *param_values,
            gpointer               data)
{
  GtkInspectorSignalsList *sl = data;
  GObject *object;

  object = g_value_get_object (param_values);

  if (object == sl->priv->object)
    {
      gint count;
      GtkTreeIter *iter;

      iter = (GtkTreeIter *)g_hash_table_lookup (sl->priv->iters, GINT_TO_POINTER (ihint->signal_id));

      gtk_tree_model_get (GTK_TREE_MODEL (sl->priv->model), iter, COLUMN_COUNT, &count, -1);
      gtk_list_store_set (sl->priv->model, iter, COLUMN_COUNT, count + 1, -1);
    }

  return TRUE;
}

static gboolean
start_tracing_cb (GtkTreeModel *model,
                  GtkTreePath  *path,
                  GtkTreeIter  *iter,
                  gpointer      data)
{
  GtkInspectorSignalsList *sl = data;
  guint signal_id;
  gulong hook_id;
  gboolean no_hooks;

  gtk_tree_model_get (model, iter,
                      COLUMN_SIGNAL_ID, &signal_id,
                      COLUMN_HOOK_ID, &hook_id,
                      COLUMN_NO_HOOKS, &no_hooks,
                      -1);

  g_assert (signal_id != 0);
  g_assert (hook_id == 0);

  if (!no_hooks)
    {
      hook_id = g_signal_add_emission_hook (signal_id, 0, trace_hook, sl, NULL);

      gtk_list_store_set (GTK_LIST_STORE (model), iter,
                          COLUMN_COUNT, 0,
                          COLUMN_HOOK_ID, hook_id,
                          -1);
    }

  return FALSE;
}

static gboolean
stop_tracing_cb (GtkTreeModel *model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      data)
{
  guint signal_id;
  gulong hook_id;

  gtk_tree_model_get (model, iter,
                      COLUMN_SIGNAL_ID, &signal_id,
                      COLUMN_HOOK_ID, &hook_id,
                      -1);

  g_assert (signal_id != 0);

  if (hook_id != 0)
    {
      g_signal_remove_emission_hook (signal_id, hook_id);
      gtk_list_store_set (GTK_LIST_STORE (model), iter,
                          COLUMN_HOOK_ID, 0,
                          -1);
    }

  return FALSE;
}

static void
start_tracing (GtkInspectorSignalsList *sl)
{
  sl->priv->tracing = TRUE;
  gtk_tree_model_foreach (GTK_TREE_MODEL (sl->priv->model), start_tracing_cb, sl);
}

static void
stop_tracing (GtkInspectorSignalsList *sl)
{
  sl->priv->tracing = FALSE;
  gtk_tree_model_foreach (GTK_TREE_MODEL (sl->priv->model), stop_tracing_cb, sl);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sl->priv->trace_button), FALSE);
}

static void
toggle_tracing (GtkToggleButton *button, GtkInspectorSignalsList *sl)
{
  if (gtk_toggle_button_get_active (button) == sl->priv->tracing)
    return;

  //gtk_widget_show (sl->priv->log_win);

  if (gtk_toggle_button_get_active (button))
    start_tracing (sl);
  else
    stop_tracing (sl);
}

static gboolean
clear_log_cb (GtkTreeModel *model,
              GtkTreePath  *path,
              GtkTreeIter  *iter,
              gpointer      data)
{
  gtk_list_store_set (GTK_LIST_STORE (model), iter, COLUMN_COUNT, 0, -1);
  return FALSE;
}

static void
clear_log (GtkButton *button, GtkInspectorSignalsList *sl)
{
  gtk_text_buffer_set_text (sl->priv->text, "", -1);

  gtk_tree_model_foreach (GTK_TREE_MODEL (sl->priv->model), clear_log_cb, sl);
}

static void
gtk_inspector_signals_list_class_init (GtkInspectorSignalsListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/signals-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, text);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, log_win);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, count_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, count_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, trace_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, clear_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSignalsList, object_title);
  gtk_widget_class_bind_template_callback (widget_class, toggle_tracing);
  gtk_widget_class_bind_template_callback (widget_class, clear_log);
}

// vim: set et sw=2 ts=2:
