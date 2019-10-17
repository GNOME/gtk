/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkactionmuxerprivate.h"

#include "gtkactionobservableprivate.h"
#include "gtkactionobserverprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkwidget.h"
#include "gsettings-mapping.h"

#include <string.h>

/*< private >
 * SECTION:gtkactionmuxer
 * @short_description: Aggregate and monitor several action groups
 *
 * #GtkActionMuxer is a #GActionGroup and #GtkActionObservable that is
 * capable of containing other #GActionGroup instances.
 *
 * The typical use is aggregating all of the actions applicable to a
 * particular context into a single action group, with namespacing.
 *
 * Consider the case of two action groups -- one containing actions
 * applicable to an entire application (such as “quit”) and one
 * containing actions applicable to a particular window in the
 * application (such as “fullscreen”).
 *
 * In this case, each of these action groups could be added to a
 * #GtkActionMuxer with the prefixes “app” and “win”, respectively.  This
 * would expose the actions as “app.quit” and “win.fullscreen” on the
 * #GActionGroup interface presented by the #GtkActionMuxer.
 *
 * Activations and state change requests on the #GtkActionMuxer are wired
 * through to the underlying action group in the expected way.
 *
 * This class is typically only used at the site of “consumption” of
 * actions (eg: when displaying a menu that contains many actions on
 * different objects).
 */

static void     gtk_action_muxer_group_iface_init         (GActionGroupInterface        *iface);
static void     gtk_action_muxer_observable_iface_init    (GtkActionObservableInterface *iface);

typedef GObjectClass GtkActionMuxerClass;

struct _GtkActionMuxer
{
  GObject parent_instance;

  GHashTable *observed_actions;
  GHashTable *groups;
  GHashTable *primary_accels;
  GtkActionMuxer *parent;

  GtkWidget *widget;
  GPtrArray *widget_actions;
  gboolean *widget_actions_enabled;
};

G_DEFINE_TYPE_WITH_CODE (GtkActionMuxer, gtk_action_muxer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gtk_action_muxer_group_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVABLE, gtk_action_muxer_observable_iface_init))

enum
{
  PROP_0,
  PROP_PARENT,
  PROP_WIDGET,
  PROP_WIDGET_ACTIONS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

guint accel_signal;

typedef struct
{
  GtkActionMuxer *muxer;
  GSList       *watchers;
  gchar        *fullname;
} Action;

typedef struct
{
  GtkActionMuxer *muxer;
  GActionGroup *group;
  gchar        *prefix;
  gulong        handler_ids[4];
} Group;

static void
gtk_action_muxer_append_group_actions (const char *prefix,
                                       Group      *group,
                                       GHashTable *actions)
{
  gchar **group_actions;
  gchar **action;

  group_actions = g_action_group_list_actions (group->group);
  for (action = group_actions; *action; action++)
    {
      char *name = g_strconcat (prefix, ".", *action, NULL);
      g_hash_table_add (actions, name);
    }

  g_strfreev (group_actions);
}

static gchar **
gtk_action_muxer_list_actions (GActionGroup *action_group)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (action_group);
  GHashTable *actions;
  char **keys;

  actions = g_hash_table_new_full (g_str_hash, g_str_equal,
                                   g_free, NULL);

  for ( ; muxer != NULL; muxer = muxer->parent)
    {
      GHashTableIter iter;
      const char *prefix;
      Group *group;

      if (muxer->widget_actions)
        {
          int i;

          for (i = 0; i < muxer->widget_actions->len; i++)
            {
              GtkWidgetAction *action = g_ptr_array_index (muxer->widget_actions, i);
              g_hash_table_add (actions, g_strdup (action->name));
            }
        }

      g_hash_table_iter_init (&iter, muxer->groups);
      while (g_hash_table_iter_next (&iter, (gpointer *)&prefix, (gpointer *)&group))
        gtk_action_muxer_append_group_actions (prefix, group, actions);
    }

  keys = (char **)g_hash_table_get_keys_as_array (actions, NULL);

  g_hash_table_steal_all (actions);
  g_hash_table_unref (actions);

  return (char **)keys;
}

static Group *
gtk_action_muxer_find_group (GtkActionMuxer  *muxer,
                             const gchar     *full_name,
                             const gchar    **action_name)
{
  const gchar *dot;
  gchar *prefix;
  const char *name;
  Group *group;

  dot = strchr (full_name, '.');

  if (!dot)
    return NULL;

  name = dot + 1;

  prefix = g_strndup (full_name, dot - full_name);
  group = g_hash_table_lookup (muxer->groups, prefix);
  g_free (prefix);

  if (action_name)
    *action_name = name;

  if (group &&
      g_action_group_has_action (group->group, name))
    return group;

  return NULL;
}

GActionGroup *
gtk_action_muxer_find (GtkActionMuxer  *muxer,
                       const char      *action_name,
                       const char     **unprefixed_name)
{
  Group *group;

  group = gtk_action_muxer_find_group (muxer, action_name, unprefixed_name);
  if (group)
    return group->group;

  return NULL;
}

void
gtk_action_muxer_action_enabled_changed (GtkActionMuxer *muxer,
                                         const gchar    *action_name,
                                         gboolean        enabled)
{
  Action *action;
  GSList *node;

  if (muxer->widget_actions)
    {
      int i;
      for (i = 0; i < muxer->widget_actions->len; i++)
        {
          GtkWidgetAction *a = g_ptr_array_index (muxer->widget_actions, i);
          if (strcmp (a->name, action_name) == 0)
            {
              muxer->widget_actions_enabled[i] = enabled;
              break;
            }
        }
    }
  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_enabled_changed (node->data, GTK_ACTION_OBSERVABLE (muxer), action_name, enabled);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (muxer), action_name, enabled);
}

static void
gtk_action_muxer_group_action_enabled_changed (GActionGroup *action_group,
                                               const gchar  *action_name,
                                               gboolean      enabled,
                                               gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_enabled_changed (group->muxer, fullname, enabled);

  g_free (fullname);
}

static void
gtk_action_muxer_parent_action_enabled_changed (GActionGroup *action_group,
                                                const gchar  *action_name,
                                                gboolean      enabled,
                                                gpointer      user_data)
{
  GtkActionMuxer *muxer = user_data;

  gtk_action_muxer_action_enabled_changed (muxer, action_name, enabled);
}

void
gtk_action_muxer_action_state_changed (GtkActionMuxer *muxer,
                                       const gchar    *action_name,
                                       GVariant       *state)
{
  Action *action;
  GSList *node;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_state_changed (node->data, GTK_ACTION_OBSERVABLE (muxer), action_name, state);
  g_action_group_action_state_changed (G_ACTION_GROUP (muxer), action_name, state);
}

static void
gtk_action_muxer_group_action_state_changed (GActionGroup *action_group,
                                             const gchar  *action_name,
                                             GVariant     *state,
                                             gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_state_changed (group->muxer, fullname, state);

  g_free (fullname);
}

static void
gtk_action_muxer_parent_action_state_changed (GActionGroup *action_group,
                                              const gchar  *action_name,
                                              GVariant     *state,
                                              gpointer      user_data)
{
  GtkActionMuxer *muxer = user_data;

  gtk_action_muxer_action_state_changed (muxer, action_name, state);
}

static void
gtk_action_muxer_action_added (GtkActionMuxer *muxer,
                               const gchar    *action_name,
                               GActionGroup   *original_group,
                               const gchar    *orignal_action_name)
{
  const GVariantType *parameter_type;
  gboolean enabled;
  GVariant *state;
  Action *action;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);

  if (action && action->watchers &&
      g_action_group_query_action (original_group, orignal_action_name,
                                   &enabled, &parameter_type, NULL, NULL, &state))
    {
      GSList *node;

      for (node = action->watchers; node; node = node->next)
        gtk_action_observer_action_added (node->data,
                                        GTK_ACTION_OBSERVABLE (muxer),
                                        action_name, parameter_type, enabled, state);

      if (state)
        g_variant_unref (state);
    }

  g_action_group_action_added (G_ACTION_GROUP (muxer), action_name);
}

static void
gtk_action_muxer_action_added_to_group (GActionGroup *action_group,
                                        const gchar  *action_name,
                                        gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_added (group->muxer, fullname, action_group, action_name);

  g_free (fullname);
}

static void
gtk_action_muxer_action_added_to_parent (GActionGroup *action_group,
                                         const gchar  *action_name,
                                         gpointer      user_data)
{
  GtkActionMuxer *muxer = user_data;

  gtk_action_muxer_action_added (muxer, action_name, action_group, action_name);
}

static void
gtk_action_muxer_action_removed (GtkActionMuxer *muxer,
                                 const gchar    *action_name)
{
  Action *action;
  GSList *node;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_removed (node->data, GTK_ACTION_OBSERVABLE (muxer), action_name);
  g_action_group_action_removed (G_ACTION_GROUP (muxer), action_name);
}

static void
gtk_action_muxer_action_removed_from_group (GActionGroup *action_group,
                                            const gchar  *action_name,
                                            gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_removed (group->muxer, fullname);

  g_free (fullname);
}

static void
gtk_action_muxer_action_removed_from_parent (GActionGroup *action_group,
                                             const gchar  *action_name,
                                             gpointer      user_data)
{
  GtkActionMuxer *muxer = user_data;

  gtk_action_muxer_action_removed (muxer, action_name);
}

static void
gtk_action_muxer_primary_accel_changed (GtkActionMuxer *muxer,
                                        const gchar    *action_name,
                                        const gchar    *action_and_target)
{
  Action *action;
  GSList *node;

  if (!action_name)
    action_name = strrchr (action_and_target, '|') + 1;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_primary_accel_changed (node->data, GTK_ACTION_OBSERVABLE (muxer),
                                               action_name, action_and_target);
  g_signal_emit (muxer, accel_signal, 0, action_name, action_and_target);
}

static void
gtk_action_muxer_parent_primary_accel_changed (GtkActionMuxer *parent,
                                               const gchar    *action_name,
                                               const gchar    *action_and_target,
                                               gpointer        user_data)
{
  GtkActionMuxer *muxer = user_data;

  /* If it's in our table then don't let the parent one filter through */
  if (muxer->primary_accels && g_hash_table_lookup (muxer->primary_accels, action_and_target))
    return;

  gtk_action_muxer_primary_accel_changed (muxer, action_name, action_and_target);
}

static GVariant *
prop_action_get_state (GtkWidget       *widget,
                       GtkWidgetAction *action)
{
  GValue value = G_VALUE_INIT;
  GVariant *result;

  g_value_init (&value, action->pspec->value_type);
  g_object_get_property (G_OBJECT (widget), action->pspec->name, &value);

  result = g_settings_set_mapping (&value, action->state_type, NULL);
  g_value_unset (&value);

  return g_variant_ref_sink (result);
}

static GVariant *
prop_action_get_state_hint (GtkWidget       *widget,
                            GtkWidgetAction *action)
{
  if (action->pspec->value_type == G_TYPE_INT)
    {
      GParamSpecInt *pspec = (GParamSpecInt *)action->pspec;
      return g_variant_new ("(ii)", pspec->minimum, pspec->maximum);
    }
  else if (action->pspec->value_type == G_TYPE_UINT)
    {
      GParamSpecUInt *pspec = (GParamSpecUInt *)action->pspec;
      return g_variant_new ("(uu)", pspec->minimum, pspec->maximum);
    }
  else if (action->pspec->value_type == G_TYPE_FLOAT)
    {
      GParamSpecFloat *pspec = (GParamSpecFloat *)action->pspec;
      return g_variant_new ("(dd)", (double)pspec->minimum, (double)pspec->maximum);
    }
  else if (action->pspec->value_type == G_TYPE_DOUBLE)
    {
      GParamSpecDouble *pspec = (GParamSpecDouble *)action->pspec;
      return g_variant_new ("(dd)", pspec->minimum, pspec->maximum);
    }

  return NULL;
}

static void
prop_action_set_state (GtkWidget       *widget,
                       GtkWidgetAction *action,
                       GVariant        *state)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, action->pspec->value_type);
  g_settings_get_mapping (&value, state, NULL);

  g_object_set_property (G_OBJECT (widget), action->pspec->name, &value);
  g_value_unset (&value);
}

static void
prop_action_activate (GtkWidget       *widget,
                      GtkWidgetAction *action,
                      GVariant        *parameter)
{
  if (action->pspec->value_type == G_TYPE_BOOLEAN)
    {
      gboolean value;

      g_return_if_fail (parameter == NULL);

      g_object_get (G_OBJECT (widget), action->pspec->name, &value, NULL);
      value = !value;
      g_object_set (G_OBJECT (widget), action->pspec->name, value, NULL);
    }
  else
    {
      g_return_if_fail (parameter != NULL && g_variant_is_of_type (parameter, action->state_type));

      prop_action_set_state (widget, action, parameter);
    }
}

static void
prop_action_notify (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  GtkActionMuxer *muxer = user_data;
  int i;
  GtkWidgetAction *action = NULL;
  GVariant *state;

  g_assert ((GObject *)muxer->widget == object);

  for (i = 0; i < muxer->widget_actions->len; i++)
    {
      action = g_ptr_array_index (muxer->widget_actions, i);
      if (action->pspec == pspec)
        break;
      action = NULL;
    }

  g_assert (action != NULL);

  state = prop_action_get_state (muxer->widget, action);
  gtk_action_muxer_action_state_changed (muxer, action->name, state);
  g_variant_unref (state);
}

static void
prop_actions_connect (GtkActionMuxer *muxer)
{
  int i;

  if (!muxer->widget || !muxer->widget_actions)
    return;

  for (i = 0; i < muxer->widget_actions->len; i++)
    {
      GtkWidgetAction *action = g_ptr_array_index (muxer->widget_actions, i);
      char *detailed;

      if (!action->pspec)
        continue;

      detailed = g_strconcat ("notify::", action->pspec->name, NULL);
      g_signal_connect (muxer->widget, detailed,
                        G_CALLBACK (prop_action_notify), muxer);
      g_free (detailed);
    }
}


static gboolean
gtk_action_muxer_query_action (GActionGroup        *action_group,
                               const gchar         *action_name,
                               gboolean            *enabled,
                               const GVariantType **parameter_type,
                               const GVariantType **state_type,
                               GVariant           **state_hint,
                               GVariant           **state)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (action_group);
  Group *group;
  const gchar *unprefixed_name;

  if (muxer->widget_actions)
    {
      int i;

      for (i = 0; i < muxer->widget_actions->len; i++)
        {
          GtkWidgetAction *action = g_ptr_array_index (muxer->widget_actions, i);
          if (strcmp (action->name, action_name) == 0)
            {
              if (enabled)
                *enabled = muxer->widget_actions_enabled[i];
              if (parameter_type)
                *parameter_type = action->parameter_type;
              if (state_type)
                *state_type = action->state_type;

              if (state_hint)
                *state_hint = NULL;
              if (state)
                *state = NULL;

              if (action->pspec)
                {
                  if (state)
                    *state = prop_action_get_state (muxer->widget, action);
                  if (state_hint)
                    *state_hint = prop_action_get_state_hint (muxer->widget, action);
                }

              return TRUE;
            }
       }
    }

  group = gtk_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    return g_action_group_query_action (group->group, unprefixed_name, enabled,
                                        parameter_type, state_type, state_hint, state);

  if (muxer->parent)
    return g_action_group_query_action (G_ACTION_GROUP (muxer->parent), action_name,
                                        enabled, parameter_type,
                                        state_type, state_hint, state);

  return FALSE;
}

static void
gtk_action_muxer_activate_action (GActionGroup *action_group,
                                  const gchar  *action_name,
                                  GVariant     *parameter)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (action_group);
  Group *group;
  const gchar *unprefixed_name;

  if (muxer->widget_actions)
    {
      int i;

      for (i = 0; i < muxer->widget_actions->len; i++)
        {
          GtkWidgetAction *action = g_ptr_array_index (muxer->widget_actions, i);
          if (strcmp (action->name, action_name) == 0)
            {
              if (muxer->widget_actions_enabled[i])
                {
                  if (action->activate)
                    action->activate (muxer->widget, action->name, parameter);
                  else if (action->pspec)
                    prop_action_activate (muxer->widget, action, parameter);
                }

              return;
            }
        }
    }

  group = gtk_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    g_action_group_activate_action (group->group, unprefixed_name, parameter);
  else if (muxer->parent)
    g_action_group_activate_action (G_ACTION_GROUP (muxer->parent), action_name, parameter);
}

static void
gtk_action_muxer_change_action_state (GActionGroup *action_group,
                                      const gchar  *action_name,
                                      GVariant     *state)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (action_group);
  Group *group;
  const gchar *unprefixed_name;

  if (muxer->widget_actions)
    {
      int i;

      for (i = 0; i < muxer->widget_actions->len; i++)
        {
          GtkWidgetAction *action = g_ptr_array_index (muxer->widget_actions, i);
          if (strcmp (action->name, action_name) == 0)
            {
              if (action->pspec)
                prop_action_set_state (muxer->widget, action, state);

              return;
            }
        }
    }

  group = gtk_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    g_action_group_change_action_state (group->group, unprefixed_name, state);
  else if (muxer->parent)
    g_action_group_change_action_state (G_ACTION_GROUP (muxer->parent), action_name, state);
}

static void
gtk_action_muxer_unregister_internal (Action   *action,
                                      gpointer  observer)
{
  GtkActionMuxer *muxer = action->muxer;
  GSList **ptr;

  for (ptr = &action->watchers; *ptr; ptr = &(*ptr)->next)
    if ((*ptr)->data == observer)
      {
        *ptr = g_slist_remove (*ptr, observer);

        if (action->watchers == NULL)
            g_hash_table_remove (muxer->observed_actions, action->fullname);

        break;
      }
}

static void
gtk_action_muxer_weak_notify (gpointer  data,
                              GObject  *where_the_object_was)
{
  Action *action = data;

  gtk_action_muxer_unregister_internal (action, where_the_object_was);
}

static void
gtk_action_muxer_register_observer (GtkActionObservable *observable,
                                    const gchar         *name,
                                    GtkActionObserver   *observer)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  Action *action;

  action = g_hash_table_lookup (muxer->observed_actions, name);

  if (action == NULL)
    {
      action = g_slice_new (Action);
      action->muxer = muxer;
      action->fullname = g_strdup (name);
      action->watchers = NULL;

      g_hash_table_insert (muxer->observed_actions, action->fullname, action);
    }

  action->watchers = g_slist_prepend (action->watchers, observer);
  g_object_weak_ref (G_OBJECT (observer), gtk_action_muxer_weak_notify, action);
}

static void
gtk_action_muxer_unregister_observer (GtkActionObservable *observable,
                                      const gchar         *name,
                                      GtkActionObserver   *observer)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  Action *action;

  action = g_hash_table_lookup (muxer->observed_actions, name);
  g_object_weak_unref (G_OBJECT (observer), gtk_action_muxer_weak_notify, action);
  gtk_action_muxer_unregister_internal (action, observer);
}

static void
gtk_action_muxer_free_group (gpointer data)
{
  Group *group = data;
  gint i;

  /* 'for loop' or 'four loop'? */
  for (i = 0; i < 4; i++)
    g_signal_handler_disconnect (group->group, group->handler_ids[i]);

  g_object_unref (group->group);
  g_free (group->prefix);

  g_slice_free (Group, group);
}

static void
gtk_action_muxer_free_action (gpointer data)
{
  Action *action = data;
  GSList *it;

  for (it = action->watchers; it; it = it->next)
    g_object_weak_unref (G_OBJECT (it->data), gtk_action_muxer_weak_notify, action);

  g_slist_free (action->watchers);
  g_free (action->fullname);

  g_slice_free (Action, action);
}

static void
gtk_action_muxer_finalize (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  g_assert_cmpint (g_hash_table_size (muxer->observed_actions), ==, 0);
  g_hash_table_unref (muxer->observed_actions);
  g_hash_table_unref (muxer->groups);
  if (muxer->primary_accels)
    g_hash_table_unref (muxer->primary_accels);

  g_free (muxer->widget_actions_enabled);

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)
    ->finalize (object);
}

static void
gtk_action_muxer_dispose (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  if (muxer->parent)
  {
    g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_action_added_to_parent, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_action_removed_from_parent, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_parent_action_enabled_changed, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_parent_action_state_changed, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_parent_primary_accel_changed, muxer);

    g_clear_object (&muxer->parent);
  }

  g_hash_table_remove_all (muxer->observed_actions);

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)
    ->dispose (object);
}

static void
gtk_action_muxer_constructed (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  prop_actions_connect (muxer);

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)->constructed (object);
}

static void
gtk_action_muxer_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  switch (property_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, gtk_action_muxer_get_parent (muxer));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, muxer->widget);
      break;

    case PROP_WIDGET_ACTIONS:
      g_value_set_boxed (value, muxer->widget_actions);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_action_muxer_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  switch (property_id)
    {
    case PROP_PARENT:
      gtk_action_muxer_set_parent (muxer, g_value_get_object (value));
      break;

    case PROP_WIDGET:
      muxer->widget = g_value_get_object (value);
      break;

    case PROP_WIDGET_ACTIONS:
      muxer->widget_actions = g_value_get_boxed (value);
      if (muxer->widget_actions)
        {
          int i;

          muxer->widget_actions_enabled = g_new (gboolean, muxer->widget_actions->len);
          for (i = 0; i < muxer->widget_actions->len; i++)
            muxer->widget_actions_enabled[i] = TRUE;
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_action_muxer_init (GtkActionMuxer *muxer)
{
  muxer->observed_actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gtk_action_muxer_free_action);
  muxer->groups = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gtk_action_muxer_free_group);
}

static void
gtk_action_muxer_observable_iface_init (GtkActionObservableInterface *iface)
{
  iface->register_observer = gtk_action_muxer_register_observer;
  iface->unregister_observer = gtk_action_muxer_unregister_observer;
}

static void
gtk_action_muxer_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = gtk_action_muxer_list_actions;
  iface->query_action = gtk_action_muxer_query_action;
  iface->activate_action = gtk_action_muxer_activate_action;
  iface->change_action_state = gtk_action_muxer_change_action_state;
}

static void
gtk_action_muxer_class_init (GObjectClass *class)
{
  class->get_property = gtk_action_muxer_get_property;
  class->set_property = gtk_action_muxer_set_property;
  class->constructed = gtk_action_muxer_constructed;
  class->finalize = gtk_action_muxer_finalize;
  class->dispose = gtk_action_muxer_dispose;

  accel_signal = g_signal_new (I_("primary-accel-changed"),
                               GTK_TYPE_ACTION_MUXER,
                               G_SIGNAL_RUN_LAST,
                               0,
                               NULL, NULL,
                               _gtk_marshal_VOID__STRING_STRING,
                               G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
  g_signal_set_va_marshaller (accel_signal,
                              G_TYPE_FROM_CLASS (class),
                              _gtk_marshal_VOID__STRING_STRINGv);

  properties[PROP_PARENT] = g_param_spec_object ("parent", "Parent",
                                                 "The parent muxer",
                                                 GTK_TYPE_ACTION_MUXER,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDGET] = g_param_spec_object ("widget", "Widget",
                                                 "The widget that owns the muxer",
                                                 GTK_TYPE_WIDGET,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT_ONLY |
                                                 G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDGET_ACTIONS] = g_param_spec_boxed ("widget-actions", "Widget actions",
                                                        "Widget actions",
                                                        G_TYPE_PTR_ARRAY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (class, NUM_PROPERTIES, properties);
}

/*< private >
 * gtk_action_muxer_insert:
 * @muxer: a #GtkActionMuxer
 * @prefix: the prefix string for the action group
 * @action_group: a #GActionGroup
 *
 * Adds the actions in @action_group to the list of actions provided by
 * @muxer.  @prefix is prefixed to each action name, such that for each
 * action `x` in @action_group, there is an equivalent
 * action @prefix`.x` in @muxer.
 *
 * For example, if @prefix is “`app`” and @action_group
 * contains an action called “`quit`”, then @muxer will
 * now contain an action called “`app.quit`”.
 *
 * If any #GtkActionObservers are registered for actions in the group,
 * “action_added” notifications will be emitted, as appropriate.
 *
 * @prefix must not contain a dot ('.').
 */
void
gtk_action_muxer_insert (GtkActionMuxer *muxer,
                         const gchar    *prefix,
                         GActionGroup   *action_group)
{
  gchar **actions;
  Group *group;
  gint i;

  /* TODO: diff instead of ripout and replace */
  gtk_action_muxer_remove (muxer, prefix);

  group = g_slice_new (Group);
  group->muxer = muxer;
  group->group = g_object_ref (action_group);
  group->prefix = g_strdup (prefix);

  g_hash_table_insert (muxer->groups, group->prefix, group);

  actions = g_action_group_list_actions (group->group);
  for (i = 0; actions[i]; i++)
    gtk_action_muxer_action_added_to_group (group->group, actions[i], group);
  g_strfreev (actions);

  group->handler_ids[0] = g_signal_connect (group->group, "action-added",
                                            G_CALLBACK (gtk_action_muxer_action_added_to_group), group);
  group->handler_ids[1] = g_signal_connect (group->group, "action-removed",
                                            G_CALLBACK (gtk_action_muxer_action_removed_from_group), group);
  group->handler_ids[2] = g_signal_connect (group->group, "action-enabled-changed",
                                            G_CALLBACK (gtk_action_muxer_group_action_enabled_changed), group);
  group->handler_ids[3] = g_signal_connect (group->group, "action-state-changed",
                                            G_CALLBACK (gtk_action_muxer_group_action_state_changed), group);
}

/*< private >
 * gtk_action_muxer_remove:
 * @muxer: a #GtkActionMuxer
 * @prefix: the prefix of the action group to remove
 *
 * Removes a #GActionGroup from the #GtkActionMuxer.
 *
 * If any #GtkActionObservers are registered for actions in the group,
 * “action_removed” notifications will be emitted, as appropriate.
 */
void
gtk_action_muxer_remove (GtkActionMuxer *muxer,
                         const gchar    *prefix)
{
  Group *group;

  group = g_hash_table_lookup (muxer->groups, prefix);

  if (group != NULL)
    {
      gchar **actions;
      gint i;

      g_hash_table_steal (muxer->groups, prefix);

      actions = g_action_group_list_actions (group->group);
      for (i = 0; actions[i]; i++)
        gtk_action_muxer_action_removed_from_group (group->group, actions[i], group);
      g_strfreev (actions);

      gtk_action_muxer_free_group (group);
    }
}

/*< private >
 * gtk_action_muxer_new:
 * @widget: the widget to which the muxer belongs
 * @actions: widget actions
 *
 * Creates a new #GtkActionMuxer.
 */
GtkActionMuxer *
gtk_action_muxer_new (GtkWidget *widget,
                      GPtrArray *actions)
{
  return g_object_new (GTK_TYPE_ACTION_MUXER,
                       "widget", widget,
                       "widget-actions", actions,
                       NULL);
}

/*< private >
 * gtk_action_muxer_get_parent:
 * @muxer: a #GtkActionMuxer
 *
 * Returns: (transfer none): the parent of @muxer, or NULL.
 */
GtkActionMuxer *
gtk_action_muxer_get_parent (GtkActionMuxer *muxer)
{
  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);

  return muxer->parent;
}

static void
emit_changed_accels (GtkActionMuxer  *muxer,
                     GtkActionMuxer  *parent)
{
  while (parent)
    {
      if (parent->primary_accels)
        {
          GHashTableIter iter;
          gpointer key;

          g_hash_table_iter_init (&iter, parent->primary_accels);
          while (g_hash_table_iter_next (&iter, &key, NULL))
            gtk_action_muxer_primary_accel_changed (muxer, NULL, key);
        }

      parent = parent->parent;
    }
}

/*< private >
 * gtk_action_muxer_set_parent:
 * @muxer: a #GtkActionMuxer
 * @parent: (allow-none): the new parent #GtkActionMuxer
 *
 * Sets the parent of @muxer to @parent.
 */
void
gtk_action_muxer_set_parent (GtkActionMuxer *muxer,
                             GtkActionMuxer *parent)
{
  g_return_if_fail (GTK_IS_ACTION_MUXER (muxer));
  g_return_if_fail (parent == NULL || GTK_IS_ACTION_MUXER (parent));

  if (muxer->parent == parent)
    return;

  if (muxer->parent != NULL)
    {
      gchar **actions;
      gchar **it;

      actions = g_action_group_list_actions (G_ACTION_GROUP (muxer->parent));
      for (it = actions; *it; it++)
        gtk_action_muxer_action_removed (muxer, *it);
      g_strfreev (actions);

      emit_changed_accels (muxer, muxer->parent);

      g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_action_added_to_parent, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_action_removed_from_parent, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_parent_action_enabled_changed, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_parent_action_state_changed, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, gtk_action_muxer_parent_primary_accel_changed, muxer);

      g_object_unref (muxer->parent);
    }

  muxer->parent = parent;

  if (muxer->parent != NULL)
    {
      gchar **actions;
      gchar **it;

      g_object_ref (muxer->parent);

      actions = g_action_group_list_actions (G_ACTION_GROUP (muxer->parent));
      for (it = actions; *it; it++)
        gtk_action_muxer_action_added (muxer, *it, G_ACTION_GROUP (muxer->parent), *it);
      g_strfreev (actions);

      emit_changed_accels (muxer, muxer->parent);

      g_signal_connect (muxer->parent, "action-added",
                        G_CALLBACK (gtk_action_muxer_action_added_to_parent), muxer);
      g_signal_connect (muxer->parent, "action-removed",
                        G_CALLBACK (gtk_action_muxer_action_removed_from_parent), muxer);
      g_signal_connect (muxer->parent, "action-enabled-changed",
                        G_CALLBACK (gtk_action_muxer_parent_action_enabled_changed), muxer);
      g_signal_connect (muxer->parent, "action-state-changed",
                        G_CALLBACK (gtk_action_muxer_parent_action_state_changed), muxer);
      g_signal_connect (muxer->parent, "primary-accel-changed",
                        G_CALLBACK (gtk_action_muxer_parent_primary_accel_changed), muxer);
    }

  g_object_notify_by_pspec (G_OBJECT (muxer), properties[PROP_PARENT]);
}

void
gtk_action_muxer_set_primary_accel (GtkActionMuxer *muxer,
                                    const gchar    *action_and_target,
                                    const gchar    *primary_accel)
{
  if (!muxer->primary_accels)
    muxer->primary_accels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (primary_accel)
    g_hash_table_insert (muxer->primary_accels, g_strdup (action_and_target), g_strdup (primary_accel));
  else
    g_hash_table_remove (muxer->primary_accels, action_and_target);

  gtk_action_muxer_primary_accel_changed (muxer, NULL, action_and_target);
}

const gchar *
gtk_action_muxer_get_primary_accel (GtkActionMuxer *muxer,
                                    const gchar    *action_and_target)
{
  if (muxer->primary_accels)
    {
      const gchar *primary_accel;

      primary_accel = g_hash_table_lookup (muxer->primary_accels, action_and_target);

      if (primary_accel)
        return primary_accel;
    }

  if (!muxer->parent)
    return NULL;

  return gtk_action_muxer_get_primary_accel (muxer->parent, action_and_target);
}

gchar *
gtk_print_action_and_target (const gchar *action_namespace,
                             const gchar *action_name,
                             GVariant    *target)
{
  GString *result;

  g_return_val_if_fail (strchr (action_name, '|') == NULL, NULL);
  g_return_val_if_fail (action_namespace == NULL || strchr (action_namespace, '|') == NULL, NULL);

  result = g_string_new (NULL);

  if (target)
    g_variant_print_string (target, result, TRUE);
  g_string_append_c (result, '|');

  if (action_namespace)
    {
      g_string_append (result, action_namespace);
      g_string_append_c (result, '.');
    }

  g_string_append (result, action_name);

  return g_string_free (result, FALSE);
}

gchar *
gtk_normalise_detailed_action_name (const gchar *detailed_action_name)
{
  GError *error = NULL;
  gchar *action_and_target;
  gchar *action_name;
  GVariant *target;

  g_action_parse_detailed_name (detailed_action_name, &action_name, &target, &error);
  g_assert_no_error (error);

  action_and_target = gtk_print_action_and_target (NULL, action_name, target);

  if (target)
    g_variant_unref (target);

  g_free (action_name);

  return action_and_target;
}

