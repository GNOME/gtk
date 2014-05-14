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
#include "gtkwidgetprivate.h"

enum
{
  COLUMN_PREFIX,
  COLUMN_NAME,
  COLUMN_ENABLED,
  COLUMN_PARAMETER,
  COLUMN_STATE
};

struct _GtkInspectorActionsPrivate
{
  GtkListStore *model;
  GtkWidget *prefix_label;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorActions, gtk_inspector_actions, GTK_TYPE_BOX)

static void
gtk_inspector_actions_init (GtkInspectorActions *sl)
{
  sl->priv = gtk_inspector_actions_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static void
add_actions (GtkInspectorActions *sl,
             const gchar         *prefix,
             GActionGroup        *group)
{
  GtkTreeIter iter;
  gint i;
  gchar **names;
  gboolean enabled;
  const gchar *parameter;
  GVariant *state;
  gchar *state_string;

  gtk_widget_show (GTK_WIDGET (sl));

  names = g_action_group_list_actions (group);
  for (i = 0; names[i]; i++)
    {
      enabled = g_action_group_get_action_enabled (group, names[i]);
      parameter = (const gchar *)g_action_group_get_action_parameter_type (group, names[i]);
      state = g_action_group_get_action_state (group, names[i]);
      if (state)
        state_string = g_variant_print (state, FALSE);
      else
        state_string = g_strdup ("");
      gtk_list_store_append (sl->priv->model, &iter);
      gtk_list_store_set (sl->priv->model, &iter,
                          COLUMN_PREFIX, prefix,
                          COLUMN_NAME, names[i],
                          COLUMN_ENABLED, enabled,
                          COLUMN_PARAMETER, parameter,
                          COLUMN_STATE, state_string,
                          -1);
      g_free (state_string);
    }
  g_strfreev (names);
}

void
gtk_inspector_actions_set_object (GtkInspectorActions *sl,
                                  GObject             *object)
{
  gtk_list_store_clear (sl->priv->model);
  gtk_widget_hide (GTK_WIDGET (sl));

  if (GTK_IS_APPLICATION (object))
    add_actions (sl, "app", G_ACTION_GROUP (object));
  else if (GTK_IS_APPLICATION_WINDOW (object))
    add_actions (sl, "win", G_ACTION_GROUP (object));
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
              add_actions (sl, prefixes[i], group);
            }
          g_free (prefixes);
        }
    }
}

static void
gtk_inspector_actions_class_init (GtkInspectorActionsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorActions, prefix_label);
}

GtkWidget *
gtk_inspector_actions_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_ACTIONS, NULL));
}

// vim: set et sw=2 ts=2:
