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
#include "gtkstack.h"
#include "gtklistbox.h"
#include "gtkstylecontext.h"
#include "gtksizegroup.h"

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
  GtkWidget *list;
  GtkSizeGroup *prefix;
  GtkSizeGroup *name;
  GtkSizeGroup *enabled;
  GtkSizeGroup *parameter;
  GtkSizeGroup *state;
  GHashTable *groups;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorActions, gtk_inspector_actions, GTK_TYPE_BOX)

static void
gtk_inspector_actions_init (GtkInspectorActions *sl)
{
  sl->priv = gtk_inspector_actions_get_instance_private (sl);
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
  gboolean enabled;
  const gchar *parameter;
  GVariant *state;
  gchar *state_string;
  GtkWidget *row;
  GtkWidget *label;
  GtkWidget *box;
  char *key = g_strconcat (prefix, ".", name, NULL);
  GtkWidget *editor;

  enabled = g_action_group_get_action_enabled (group, name);
  parameter = (const gchar *)g_action_group_get_action_parameter_type (group, name);
  state = g_action_group_get_action_state (group, name);
  if (state)
    state_string = g_variant_print (state, FALSE);
  else
    state_string = g_strdup ("");

  row = gtk_list_box_row_new ();
  g_object_set_data_full (G_OBJECT (row), "key", key, g_free);

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = gtk_label_new (prefix);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (sl->priv->prefix, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = gtk_label_new (name);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (sl->priv->name, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = gtk_label_new (enabled ? "+" : "-");
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (sl->priv->enabled, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  g_object_set_data (G_OBJECT (row), "enabled", label);

  label = gtk_label_new (parameter);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (sl->priv->parameter, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = gtk_label_new (state_string);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_size_group_add_widget (sl->priv->state, label);
  gtk_container_add (GTK_CONTAINER (box), label);
  g_object_set_data (G_OBJECT (row), "state", label);

  editor = gtk_inspector_action_editor_new (group, prefix, name);
  gtk_style_context_add_class (gtk_widget_get_style_context (editor), "cell");
  gtk_container_add (GTK_CONTAINER (box), editor);

  gtk_container_add (GTK_CONTAINER (sl->priv->list), row);

  g_free (state_string);
}

static GtkWidget *
find_row (GtkInspectorActions *sl,
          const char          *prefix,
          const char          *action_name)
{
  GtkWidget *row = NULL;
  GtkWidget *widget;
  char *key = g_strconcat (prefix, ".", action_name, NULL);

  for (widget = gtk_widget_get_first_child (sl->priv->list);
       widget;
       widget = gtk_widget_get_next_sibling (widget))
    {
      const char *rkey = g_object_get_data (G_OBJECT (widget), "key");
      if (g_str_equal (key, rkey))
        {
          row = widget;
          break;
        }
    }

  g_free (key);

  return row;
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
  GtkWidget *row;

  prefix = g_hash_table_lookup (sl->priv->groups, group);
  row = find_row (sl, prefix, action_name);
  if (row)
    gtk_container_remove (GTK_CONTAINER (sl->priv->list), row);
}

static void
action_enabled_changed_cb (GActionGroup        *group,
                           const gchar         *action_name,
                           gboolean             enabled,
                           GtkInspectorActions *sl)
{
  const gchar *prefix;
  GtkWidget *row;
  GtkWidget *label;

  prefix = g_hash_table_lookup (sl->priv->groups, group);

  row = find_row (sl, prefix, action_name);
  label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "enabled"));
  gtk_label_set_label (GTK_LABEL (label), enabled ? "+" : "-" );
}

static void
action_state_changed_cb (GActionGroup        *group,
                         const gchar         *action_name,
                         GVariant            *state,
                         GtkInspectorActions *sl)
{
  const gchar *prefix;
  gchar *state_string;
  GtkWidget *row;
  GtkWidget *label;

  prefix = g_hash_table_lookup (sl->priv->groups, group);

  row = find_row (sl, prefix, action_name);
  if (state)
    state_string = g_variant_print (state, FALSE);
  else
    state_string = g_strdup ("");
  label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "state"));
  gtk_label_set_label (GTK_LABEL (label), state_string);
  g_free (state_string);
}

static void
add_group (GtkInspectorActions *sl,
           GtkStackPage        *page,
           GActionGroup        *group,
           const gchar         *prefix)
{
  gint i;
  gchar **names;

  g_object_set (page, "visible", TRUE, NULL);

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
  GtkWidget *stack;
  GtkStackPage *page;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  g_object_set (page, "visible", FALSE, NULL);
  g_hash_table_foreach (sl->priv->groups, disconnect_group, sl);
  g_hash_table_remove_all (sl->priv->groups);

  if (GTK_IS_APPLICATION (object))
    add_group (sl, page, G_ACTION_GROUP (object), "app");
  else if (GTK_IS_WIDGET (object))
    {
      const gchar **prefixes;
      GActionGroup *group;
      gint i;

      prefixes = gtk_widget_list_action_prefixes (GTK_WIDGET (object));
      if (prefixes)
        {
          for (i = 0; prefixes[i]; i++)
            {
              group = gtk_widget_get_action_group (GTK_WIDGET (object), prefixes[i]);
              add_group (sl, page, group, prefixes[i]);
            }
          g_free (prefixes);
        }
    }
}

static void
gtk_inspector_actions_class_init (GtkInspectorActionsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, prefix);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, enabled);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, parameter);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, state);
}

// vim: set et sw=2 ts=2:
