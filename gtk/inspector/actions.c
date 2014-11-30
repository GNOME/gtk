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

#include "actions.h"
#include "action-editor.h"

#include "gtkapplication.h"
#include "gtkapplicationwindow.h"
#include "gtktreeview.h"
#include "gtkliststore.h"
#include "gtkwidgetprivate.h"
#include "gtkpopover.h"
#include "gtklabel.h"

enum
{
  COLUMN_PREFIX,
  COLUMN_NAME,
  COLUMN_ENABLED,
  COLUMN_PARAMETER,
  COLUMN_STATE,
  COLUMN_GROUP
};

struct _GtkInspectorActionsPrivate
{
  GtkListStore *model;
  GHashTable *groups;
  GHashTable *iters;
  GtkWidget *object_title;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorActions, gtk_inspector_actions, GTK_TYPE_BOX)

static void
gtk_inspector_actions_init (GtkInspectorActions *sl)
{
  sl->priv = gtk_inspector_actions_get_instance_private (sl);
  sl->priv->iters = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           (GDestroyNotify) gtk_tree_iter_free);
  sl->priv->groups = g_hash_table_new_full (g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            g_free);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static void
add_action (GtkInspectorActions *sl,
            GActionGroup        *group,
            const gchar         *prefix,
            const gchar         *name)
{
  GtkTreeIter iter;
  gboolean enabled;
  const gchar *parameter;
  GVariant *state;
  gchar *state_string;

  enabled = g_action_group_get_action_enabled (group, name);
  parameter = (const gchar *)g_action_group_get_action_parameter_type (group, name);
  state = g_action_group_get_action_state (group, name);
  if (state)
    state_string = g_variant_print (state, FALSE);
  else
    state_string = g_strdup ("");
  gtk_list_store_append (sl->priv->model, &iter);
  gtk_list_store_set (sl->priv->model, &iter,
                      COLUMN_PREFIX, prefix,
                      COLUMN_NAME, name,
                      COLUMN_ENABLED, enabled,
                      COLUMN_PARAMETER, parameter,
                      COLUMN_STATE, state_string,
                      COLUMN_GROUP, group,
                      -1);
  g_hash_table_insert (sl->priv->iters,
                       g_strconcat (prefix, ".", name, NULL),
                       gtk_tree_iter_copy (&iter));
  g_free (state_string);
}

static void
action_added_cb (GActionGroup        *group,
                 const gchar         *action_name,
                 GtkInspectorActions *sl)
{
  const gchar *prefix;
  prefix = g_hash_table_lookup (sl->priv->groups, group);
  add_action (sl, group, prefix, action_name);
}

static void
action_removed_cb (GActionGroup        *group,
                   const gchar         *action_name,
                   GtkInspectorActions *sl)
{
  const gchar *prefix;
  gchar *key;
  GtkTreeIter *iter;
  prefix = g_hash_table_lookup (sl->priv->groups, group);
  key = g_strconcat (prefix, ".", action_name, NULL);
  iter = g_hash_table_lookup (sl->priv->iters, key);
  gtk_list_store_remove (sl->priv->model, iter);
  g_hash_table_remove (sl->priv->iters, key);
  g_free (key);
}

static void
action_enabled_changed_cb (GActionGroup        *group,
                           const gchar         *action_name,
                           gboolean             enabled,
                           GtkInspectorActions *sl)
{
  const gchar *prefix;
  gchar *key;
  GtkTreeIter *iter;
  prefix = g_hash_table_lookup (sl->priv->groups, group);
  key = g_strconcat (prefix, ".", action_name, NULL);
  iter = g_hash_table_lookup (sl->priv->iters, key);
  gtk_list_store_set (sl->priv->model, iter,
                      COLUMN_ENABLED, enabled,
                      -1);
  g_free (key);
}

static void
action_state_changed_cb (GActionGroup        *group,
                         const gchar         *action_name,
                         GVariant            *state,
                         GtkInspectorActions *sl)
{
  const gchar *prefix;
  gchar *key;
  GtkTreeIter *iter;
  gchar *state_string;
  prefix = g_hash_table_lookup (sl->priv->groups, group);
  key = g_strconcat (prefix, ".", action_name, NULL);
  iter = g_hash_table_lookup (sl->priv->iters, key);
  if (state)
    state_string = g_variant_print (state, FALSE);
  else
    state_string = g_strdup ("");
  gtk_list_store_set (sl->priv->model, iter,
                      COLUMN_STATE, state_string,
                      -1);
  g_free (state_string);
  g_free (key);
}

static void
add_group (GtkInspectorActions *sl,
           GActionGroup        *group,
           const gchar         *prefix)
{
  gint i;
  gchar **names;

  gtk_widget_show (GTK_WIDGET (sl));

  g_signal_connect (group, "action-added", G_CALLBACK (action_added_cb), sl);
  g_signal_connect (group, "action-removed", G_CALLBACK (action_removed_cb), sl);
  g_signal_connect (group, "action-enabled-changed", G_CALLBACK (action_enabled_changed_cb), sl);
  g_signal_connect (group, "action-state-changed", G_CALLBACK (action_state_changed_cb), sl);
  g_hash_table_insert (sl->priv->groups, group, g_strdup (prefix));

  names = g_action_group_list_actions (group);
  for (i = 0; names[i]; i++)
    add_action (sl, group, prefix, names[i]);
  g_strfreev (names);
}

static void
disconnect_group (gpointer key, gpointer value, gpointer data)
{
  GActionGroup *group = key;
  GtkInspectorActions *sl = data;

  g_signal_handlers_disconnect_by_func (group, action_added_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_removed_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_enabled_changed_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_state_changed_cb, sl);
}

void
gtk_inspector_actions_set_object (GtkInspectorActions *sl,
                                  GObject             *object)
{
  gtk_widget_hide (GTK_WIDGET (sl));
  g_hash_table_foreach (sl->priv->groups, disconnect_group, sl);
  g_hash_table_remove_all (sl->priv->groups);
  g_hash_table_remove_all (sl->priv->iters);
  gtk_list_store_clear (sl->priv->model);
  
  if (GTK_IS_APPLICATION (object))
    add_group (sl, G_ACTION_GROUP (object), "app");
  else if (GTK_IS_APPLICATION_WINDOW (object))
    add_group (sl, G_ACTION_GROUP (object), "win");
  else if (GTK_IS_WIDGET (object))
    {
      gchar **prefixes;
      GActionGroup *group;
      gint i;

      prefixes = _gtk_widget_list_action_prefixes (GTK_WIDGET (object));
      if (prefixes)
        {
          for (i = 0; prefixes[i]; i++)
            {
              group = _gtk_widget_get_action_group (GTK_WIDGET (object), prefixes[i]);
              add_group (sl, group, prefixes[i]);
            }
          g_free (prefixes);
        }
    }

  if (G_IS_OBJECT (object))
    {
      const gchar *title;
      title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
      gtk_label_set_label (GTK_LABEL (sl->priv->object_title), title);
    }
}

static void
row_activated (GtkTreeView         *tv,
               GtkTreePath         *path,
               GtkTreeViewColumn   *col,
               GtkInspectorActions *sl)
{
  GtkTreeIter iter;
  GdkRectangle rect;
  GtkWidget *popover;
  gchar *prefix;
  gchar *name;
  GActionGroup *group;
  GtkWidget *editor;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (sl->priv->model), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (sl->priv->model),
                      &iter,
                      COLUMN_PREFIX, &prefix,
                      COLUMN_NAME, &name,
                      COLUMN_GROUP, &group,
                      -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (sl->priv->model), &iter, path);
  gtk_tree_view_get_cell_area (tv, path, col, &rect);
  gtk_tree_view_convert_bin_window_to_widget_coords (tv, rect.x, rect.y, &rect.x, &rect.y);

  popover = gtk_popover_new (GTK_WIDGET (tv));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  editor = gtk_inspector_action_editor_new (group, prefix, name);
  gtk_container_add (GTK_CONTAINER (popover), editor);
  gtk_widget_show (popover);

  g_signal_connect (popover, "hide", G_CALLBACK (gtk_widget_destroy), NULL);

  g_free (name);
  g_free (prefix);
}

static void
gtk_inspector_actions_class_init (GtkInspectorActionsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, object_title);
  gtk_widget_class_bind_template_callback (widget_class, row_activated);
}

// vim: set et sw=2 ts=2:
