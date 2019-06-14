/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkwidgetactiongroupprivate.h"


enum {
  PROP_WIDGET = 1,
  PROP_PREFIX,
  PROP_ACTIONS
};

struct _GtkWidgetActionGroup {
  GObject parent;

  GtkWidget *widget;
  char *prefix;

  GPtrArray *actions;
};

typedef struct {
  GObjectClass parent_class;
} GtkWidgetActionGroupClass;

static void gtk_widget_action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkWidgetActionGroup, gtk_widget_action_group, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gtk_widget_action_group_iface_init))

static char **
gtk_widget_action_group_list_actions (GActionGroup *action_group)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (action_group);
  GPtrArray *actions;
  int i;

  actions = g_ptr_array_new ();
 
  for (i = 0; i < group->actions->len; i++)
    {
      GtkWidgetAction *action = g_ptr_array_index (group->actions, i);

      if (strcmp (group->prefix, action->prefix) == 0)
        g_ptr_array_add (actions, g_strdup (action->name));
    }

  g_ptr_array_add (actions, NULL);

  return (char **)g_ptr_array_free (actions, FALSE);
}

static gboolean
gtk_widget_action_group_query_action (GActionGroup        *action_group,
                                      const gchar         *action_name,
                                      gboolean            *enabled,
                                      const GVariantType **parameter_type,
                                      const GVariantType **state_type,
                                      GVariant           **state_hint,
                                      GVariant           **state)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (action_group);
  int i;

  for (i = 0; i < group->actions->len; i++)
    {
      GtkWidgetAction *action = g_ptr_array_index (group->actions, i);

      if (strcmp (action->prefix, group->prefix) == 0 &&
          strcmp (action->name, action_name) == 0)
        {
          return action->query (group->widget,
                                action->name,
                                enabled,
                                parameter_type,
                                state_type,
                                state_hint,
                                state);
        }
    }

  return FALSE;
}

static void
gtk_widget_action_group_change_action_state (GActionGroup *action_group,
                                             const gchar  *action_name,
                                             GVariant     *value)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (action_group);
  int i;

  for (i = 0; i < group->actions->len; i++)
    {
      GtkWidgetAction *action = g_ptr_array_index (group->actions, i);

      if (strcmp (action->prefix, group->prefix) == 0 &&
          strcmp (action->name, action_name) == 0)
        {
          if (action->change)
            action->change (group->widget, action->name, value);

          break;
        }
    }
}

static void
gtk_widget_action_group_activate_action (GActionGroup *action_group,
                                         const  char  *action_name,
                                         GVariant     *parameter)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (action_group);
  int i;

  for (i = 0; i < group->actions->len; i++)
    {
      GtkWidgetAction *action = g_ptr_array_index (group->actions, i);

      if (strcmp (action->prefix, group->prefix) == 0 &&
          strcmp (action->name, action_name) == 0)
        {
          action->activate (group->widget, action->name, parameter);
          break;
        }
    }
}

static void
gtk_widget_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = gtk_widget_action_group_list_actions;
  iface->query_action = gtk_widget_action_group_query_action;
  iface->change_action_state = gtk_widget_action_group_change_action_state;
  iface->activate_action = gtk_widget_action_group_activate_action;
}

static void
gtk_widget_action_group_init (GtkWidgetActionGroup *group)
{
}

static void
gtk_widget_action_group_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      group->widget = g_value_get_object (value);
      break;

    case PROP_PREFIX:
      group->prefix = g_value_dup_string (value);
      break;

    case PROP_ACTIONS:
      group->actions = g_value_dup_boxed (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_widget_action_group_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, group->widget);
      break;

    case PROP_PREFIX:
      g_value_set_string (value, group->prefix);
      break;

    case PROP_ACTIONS:
      g_value_set_boxed (value, group->actions);
      break;

    default:
      g_assert_not_reached ();
    }
}
static void
gtk_widget_action_group_finalize (GObject *object)
{
  GtkWidgetActionGroup *group = GTK_WIDGET_ACTION_GROUP (object);

  g_free (group->prefix);
  g_ptr_array_unref (group->actions);

  G_OBJECT_CLASS (gtk_widget_action_group_parent_class)->finalize (object);
}

static void
gtk_widget_action_group_class_init (GtkWidgetActionGroupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = gtk_widget_action_group_set_property;
  object_class->get_property = gtk_widget_action_group_get_property;
  object_class->finalize = gtk_widget_action_group_finalize;

  g_object_class_install_property (object_class, PROP_WIDGET,
      g_param_spec_object ("widget",
                           "The widget",
                           "The widget to which this action group belongs",
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PREFIX,
      g_param_spec_string ("prefix",
                           "The prefix",
                           "The prefix for actions in this group",
                           NULL,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ACTIONS,
      g_param_spec_boxed ("actions",
                          "The actions",
                          "The actions",
                          G_TYPE_PTR_ARRAY,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
}

GActionGroup *
gtk_widget_action_group_new (GtkWidget  *widget,
                             const char *prefix,
                             GPtrArray  *actions)
{
  return (GActionGroup *)g_object_new (GTK_TYPE_WIDGET_ACTION_GROUP,
                                       "widget", widget,
                                       "prefix", prefix,
                                       "actions", actions,
                                       NULL);
}

GtkWidget *
gtk_widget_action_group_get_widget (GtkWidgetActionGroup *group)
{
  return group->widget;
}
