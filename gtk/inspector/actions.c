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
#include "gtkactionmuxerprivate.h"
#include "gtkpopover.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtklistbox.h"
#include "gtkstylecontext.h"
#include "gtksizegroup.h"

struct _GtkInspectorActionsPrivate
{
  GtkWidget *list;
  GtkSizeGroup *name;
  GtkSizeGroup *enabled;
  GtkSizeGroup *parameter;
  GtkSizeGroup *state;
  GtkSizeGroup *activate;
  GActionGroup *group;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorActions, gtk_inspector_actions, GTK_TYPE_BOX)

static void
gtk_inspector_actions_init (GtkInspectorActions *sl)
{
  sl->priv = gtk_inspector_actions_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static void
add_action (GtkInspectorActions *sl,
            GActionGroup        *group,
            const gchar         *name)
{
  gboolean enabled;
  const gchar *parameter;
  GVariant *state;
  gchar *state_string;
  GtkWidget *row;
  GtkWidget *label;
  GtkWidget *box;
  char *key = g_strdup (name);
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

  editor = gtk_inspector_action_editor_new (group, name, sl->priv->activate);
  gtk_style_context_add_class (gtk_widget_get_style_context (editor), "cell");
  gtk_container_add (GTK_CONTAINER (box), editor);

  gtk_container_add (GTK_CONTAINER (sl->priv->list), row);

  g_free (state_string);
}

static GtkWidget *
find_row (GtkInspectorActions *sl,
          const char          *action_name)
{
  GtkWidget *row = NULL;
  GtkWidget *widget;

  for (widget = gtk_widget_get_first_child (sl->priv->list);
       widget;
       widget = gtk_widget_get_next_sibling (widget))
    {
      const char *key = g_object_get_data (G_OBJECT (widget), "key");
      if (g_str_equal (key, action_name))
        {
          row = widget;
          break;
        }
    }

  return row;
}

static void
action_added_cb (GActionGroup        *group,
                 const gchar         *action_name,
                 GtkInspectorActions *sl)
{
  add_action (sl, group, action_name);
}

static void
action_removed_cb (GActionGroup        *group,
                   const gchar         *action_name,
                   GtkInspectorActions *sl)
{
  GtkWidget *row;

  row = find_row (sl, action_name);
  if (row)
    gtk_widget_destroy (row);
}

static void
action_enabled_changed_cb (GActionGroup        *group,
                           const gchar         *action_name,
                           gboolean             enabled,
                           GtkInspectorActions *sl)
{
  GtkWidget *row;
  GtkWidget *label;

  row = find_row (sl, action_name);
  label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "enabled"));
  gtk_label_set_label (GTK_LABEL (label), enabled ? "+" : "-" );
}

static void
action_state_changed_cb (GActionGroup        *group,
                         const gchar         *action_name,
                         GVariant            *state,
                         GtkInspectorActions *sl)
{
  gchar *state_string;
  GtkWidget *row;
  GtkWidget *label;

  row = find_row (sl, action_name);
  if (state)
    state_string = g_variant_print (state, FALSE);
  else
    state_string = g_strdup ("");
  label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "state"));
  gtk_label_set_label (GTK_LABEL (label), state_string);
  g_free (state_string);
}

static void
connect_group (GActionGroup *group,
               GtkInspectorActions *sl)
{
  g_signal_connect (group, "action-added", G_CALLBACK (action_added_cb), sl);
  g_signal_connect (group, "action-removed", G_CALLBACK (action_removed_cb), sl);
  g_signal_connect (group, "action-enabled-changed", G_CALLBACK (action_enabled_changed_cb), sl);
  g_signal_connect (group, "action-state-changed", G_CALLBACK (action_state_changed_cb), sl);
}

static void
disconnect_group (GActionGroup *group,
                  GtkInspectorActions *sl)
{
  g_signal_handlers_disconnect_by_func (group, action_added_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_removed_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_enabled_changed_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_state_changed_cb, sl);
}

static void
add_group (GtkInspectorActions *sl,
           GtkStackPage        *page,
           GActionGroup        *group)
{
  gint i;
  gchar **names;

  g_object_set (page, "visible", TRUE, NULL);

  connect_group (group, sl);

  names = g_action_group_list_actions (group);
  for (i = 0; names[i]; i++)
    add_action (sl, group, names[i]);
  g_strfreev (names);

  g_set_object (&sl->priv->group, group);
}

static void
remove_group (GtkInspectorActions *sl,
              GtkStackPage        *page,
              GActionGroup        *group)
{
  g_object_set (page, "visible", FALSE, NULL);

  disconnect_group (group, sl);

  g_set_object (&sl->priv->group, NULL);
}

void
gtk_inspector_actions_set_object (GtkInspectorActions *sl,
                                  GObject             *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkWidget *child;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  remove_group (sl, page, sl->priv->group);

  while ((child = gtk_widget_get_first_child (sl->priv->list)))
    gtk_widget_destroy (child);

  if (GTK_IS_APPLICATION (object))
    add_group (sl, page, G_ACTION_GROUP (object));
  else if (GTK_IS_WIDGET (object))
    {
      GtkActionMuxer *muxer;

      muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (object), FALSE);
      if (muxer)
        add_group (sl, page, G_ACTION_GROUP (muxer));
    }
}

static void
gtk_inspector_actions_class_init (GtkInspectorActionsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, enabled);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, parameter);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, state);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, activate);
}

// vim: set et sw=2 ts=2:
