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

#include "gactionmuxer.h"

#include "gactionobservable.h"
#include "gactionobserver.h"

#include <string.h>

/*
 * SECTION:gactionmuxer
 * @short_description: Aggregate and monitor several action groups
 *
 * #GActionMuxer is a #GActionGroup and #GActionObservable that is
 * capable of containing other #GActionGroup instances.
 *
 * The typical use is aggregating all of the actions applicable to a
 * particular context into a single action group, with namespacing.
 *
 * Consider the case of two action groups -- one containing actions
 * applicable to an entire application (such as 'quit') and one
 * containing actions applicable to a particular window in the
 * application (such as 'fullscreen').
 *
 * In this case, each of these action groups could be added to a
 * #GActionMuxer with the prefixes "app" and "win", respectively.  This
 * would expose the actions as "app.quit" and "win.fullscreen" on the
 * #GActionGroup interface presented by the #GActionMuxer.
 *
 * Activations and state change requests on the #GActionMuxer are wired
 * through to the underlying action group in the expected way.
 *
 * This class is typically only used at the site of "consumption" of
 * actions (eg: when displaying a menu that contains many actions on
 * different objects).
 */

static void     g_action_muxer_group_iface_init         (GActionGroupInterface      *iface);
static void     g_action_muxer_observable_iface_init    (GActionObservableInterface *iface);

typedef GObjectClass GActionMuxerClass;

struct _GActionMuxer
{
  GObject parent_instance;

  GHashTable *observed_actions;
  GHashTable *groups;
  GActionMuxer *parent;
};

G_DEFINE_TYPE_WITH_CODE (GActionMuxer, g_action_muxer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_muxer_group_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_OBSERVABLE, g_action_muxer_observable_iface_init))

enum
{
  PROP_0,
  PROP_PARENT,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

typedef struct
{
  GActionMuxer *muxer;
  GSList       *watchers;
  gchar        *fullname;
} Action;

typedef struct
{
  GActionMuxer *muxer;
  GActionGroup *group;
  gchar        *prefix;
  gulong        handler_ids[4];
} Group;

static void
g_action_muxer_append_group_actions (gpointer key,
                                     gpointer value,
                                     gpointer user_data)
{
  const gchar *prefix = key;
  Group *group = value;
  GArray *actions = user_data;
  gchar **group_actions;
  gchar **action;

  group_actions = g_action_group_list_actions (group->group);
  for (action = group_actions; *action; action++)
    {
      gchar *fullname;

      fullname = g_strconcat (prefix, ".", *action, NULL);
      g_array_append_val (actions, fullname);
    }

  g_strfreev (group_actions);
}

static gchar **
g_action_muxer_list_actions (GActionGroup *action_group)
{
  GActionMuxer *muxer = G_ACTION_MUXER (action_group);
  GArray *actions;

  actions = g_array_new (TRUE, FALSE, sizeof (gchar *));

  for ( ; muxer != NULL; muxer = muxer->parent)
    {
      g_hash_table_foreach (muxer->groups,
                            g_action_muxer_append_group_actions,
                            actions);
    }

  return (gchar **) g_array_free (actions, FALSE);
}

static Group *
g_action_muxer_find_group (GActionMuxer  *muxer,
                           const gchar   *full_name,
                           const gchar  **action_name)
{
  const gchar *dot;
  gchar *prefix;
  Group *group;

  dot = strchr (full_name, '.');

  if (!dot)
    return NULL;

  prefix = g_strndup (full_name, dot - full_name);
  group = g_hash_table_lookup (muxer->groups, prefix);
  g_free (prefix);

  if (action_name)
    *action_name = dot + 1;

  return group;
}

static void
g_action_muxer_action_enabled_changed (GActionMuxer *muxer,
                                       const gchar  *action_name,
                                       gboolean      enabled)
{
  Action *action;
  GSList *node;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    g_action_observer_action_enabled_changed (node->data, G_ACTION_OBSERVABLE (muxer), action_name, enabled);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (muxer), action_name, enabled);
}

static void
g_action_muxer_group_action_enabled_changed (GActionGroup *action_group,
                                             const gchar  *action_name,
                                             gboolean      enabled,
                                             gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  g_action_muxer_action_enabled_changed (group->muxer, fullname, enabled);

  g_free (fullname);
}

static void
g_action_muxer_parent_action_enabled_changed (GActionGroup *action_group,
                                              const gchar  *action_name,
                                              gboolean      enabled,
                                              gpointer      user_data)
{
  GActionMuxer *muxer = user_data;

  g_action_muxer_action_enabled_changed (muxer, action_name, enabled);
}

static void
g_action_muxer_action_state_changed (GActionMuxer *muxer,
                                     const gchar  *action_name,
                                     GVariant     *state)
{
  Action *action;
  GSList *node;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    g_action_observer_action_state_changed (node->data, G_ACTION_OBSERVABLE (muxer), action_name, state);
  g_action_group_action_state_changed (G_ACTION_GROUP (muxer), action_name, state);
}

static void
g_action_muxer_group_action_state_changed (GActionGroup *action_group,
                                           const gchar  *action_name,
                                           GVariant     *state,
                                           gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  g_action_muxer_action_state_changed (group->muxer, fullname, state);

  g_free (fullname);
}

static void
g_action_muxer_parent_action_state_changed (GActionGroup *action_group,
                                            const gchar  *action_name,
                                            GVariant     *state,
                                            gpointer      user_data)
{
  GActionMuxer *muxer = user_data;

  g_action_muxer_action_state_changed (muxer, action_name, state);
}

static void
g_action_muxer_action_added (GActionMuxer *muxer,
                             const gchar  *action_name,
                             GActionGroup *original_group,
                             const gchar  *orignal_action_name)
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
        g_action_observer_action_added (node->data,
                                        G_ACTION_OBSERVABLE (muxer),
                                        action_name, parameter_type, enabled, state);

      if (state)
        g_variant_unref (state);
    }

  g_action_group_action_added (G_ACTION_GROUP (muxer), action_name);
}

static void
g_action_muxer_action_added_to_group (GActionGroup *action_group,
                                      const gchar  *action_name,
                                      gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  g_action_muxer_action_added (group->muxer, fullname, action_group, action_name);

  g_free (fullname);
}

static void
g_action_muxer_action_added_to_parent (GActionGroup *action_group,
                                       const gchar  *action_name,
                                       gpointer      user_data)
{
  GActionMuxer *muxer = user_data;

  g_action_muxer_action_added (muxer, action_name, action_group, action_name);
}

static void
g_action_muxer_action_removed (GActionMuxer *muxer,
                               const gchar  *action_name)
{
  Action *action;
  GSList *node;

  action = g_hash_table_lookup (muxer->observed_actions, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    g_action_observer_action_removed (node->data, G_ACTION_OBSERVABLE (muxer), action_name);
  g_action_group_action_removed (G_ACTION_GROUP (muxer), action_name);
}

static void
g_action_muxer_action_removed_from_group (GActionGroup *action_group,
                                          const gchar  *action_name,
                                          gpointer      user_data)
{
  Group *group = user_data;
  gchar *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  g_action_muxer_action_removed (group->muxer, fullname);

  g_free (fullname);
}

static void
g_action_muxer_action_removed_from_parent (GActionGroup *action_group,
                                           const gchar  *action_name,
                                           gpointer      user_data)
{
  GActionMuxer *muxer = user_data;

  g_action_muxer_action_removed (muxer, action_name);
}

static gboolean
g_action_muxer_query_action (GActionGroup        *action_group,
                             const gchar         *action_name,
                             gboolean            *enabled,
                             const GVariantType **parameter_type,
                             const GVariantType **state_type,
                             GVariant           **state_hint,
                             GVariant           **state)
{
  GActionMuxer *muxer = G_ACTION_MUXER (action_group);
  Group *group;
  const gchar *unprefixed_name;

  group = g_action_muxer_find_group (muxer, action_name, &unprefixed_name);

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
g_action_muxer_activate_action (GActionGroup *action_group,
                                const gchar  *action_name,
                                GVariant     *parameter)
{
  GActionMuxer *muxer = G_ACTION_MUXER (action_group);
  Group *group;
  const gchar *unprefixed_name;

  group = g_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    g_action_group_activate_action (group->group, unprefixed_name, parameter);
  else if (muxer->parent)
    g_action_group_activate_action (G_ACTION_GROUP (muxer->parent), action_name, parameter);
}

static void
g_action_muxer_change_action_state (GActionGroup *action_group,
                                    const gchar  *action_name,
                                    GVariant     *state)
{
  GActionMuxer *muxer = G_ACTION_MUXER (action_group);
  Group *group;
  const gchar *unprefixed_name;

  group = g_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    g_action_group_change_action_state (group->group, unprefixed_name, state);
  else if (muxer->parent)
    g_action_group_change_action_state (G_ACTION_GROUP (muxer->parent), action_name, state);
}

static void
g_action_muxer_unregister_internal (Action   *action,
                                    gpointer  observer)
{
  GActionMuxer *muxer = action->muxer;
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
g_action_muxer_weak_notify (gpointer  data,
                            GObject  *where_the_object_was)
{
  Action *action = data;

  g_action_muxer_unregister_internal (action, where_the_object_was);
}

static void
g_action_muxer_register_observer (GActionObservable *observable,
                                  const gchar       *name,
                                  GActionObserver   *observer)
{
  GActionMuxer *muxer = G_ACTION_MUXER (observable);
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
  g_object_weak_ref (G_OBJECT (observer), g_action_muxer_weak_notify, action);
}

static void
g_action_muxer_unregister_observer (GActionObservable *observable,
                                    const gchar       *name,
                                    GActionObserver   *observer)
{
  GActionMuxer *muxer = G_ACTION_MUXER (observable);
  Action *action;

  action = g_hash_table_lookup (muxer->observed_actions, name);
  g_object_weak_unref (G_OBJECT (observer), g_action_muxer_weak_notify, action);
  g_action_muxer_unregister_internal (action, observer);
}

static void
g_action_muxer_free_group (gpointer data)
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
g_action_muxer_free_action (gpointer data)
{
  Action *action = data;
  GSList *it;

  for (it = action->watchers; it; it = it->next)
    g_object_weak_unref (G_OBJECT (it->data), g_action_muxer_weak_notify, action);

  g_slist_free (action->watchers);
  g_free (action->fullname);

  g_slice_free (Action, action);
}

static void
g_action_muxer_finalize (GObject *object)
{
  GActionMuxer *muxer = G_ACTION_MUXER (object);

  g_assert_cmpint (g_hash_table_size (muxer->observed_actions), ==, 0);
  g_hash_table_unref (muxer->observed_actions);
  g_hash_table_unref (muxer->groups);

  G_OBJECT_CLASS (g_action_muxer_parent_class)
    ->finalize (object);
}

static void
g_action_muxer_dispose (GObject *object)
{
  GActionMuxer *muxer = G_ACTION_MUXER (object);

  if (muxer->parent)
  {
    g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_action_added_to_parent, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_action_removed_from_parent, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_parent_action_enabled_changed, muxer);
    g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_parent_action_state_changed, muxer);

    g_clear_object (&muxer->parent);
  }

  g_hash_table_remove_all (muxer->observed_actions);

  G_OBJECT_CLASS (g_action_muxer_parent_class)
    ->dispose (object);
}

static void
g_action_muxer_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GActionMuxer *muxer = G_ACTION_MUXER (object);

  switch (property_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, g_action_muxer_get_parent (muxer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_action_muxer_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GActionMuxer *muxer = G_ACTION_MUXER (object);

  switch (property_id)
    {
    case PROP_PARENT:
      g_action_muxer_set_parent (muxer, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_action_muxer_init (GActionMuxer *muxer)
{
  muxer->observed_actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_action_muxer_free_action);
  muxer->groups = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_action_muxer_free_group);
}

static void
g_action_muxer_observable_iface_init (GActionObservableInterface *iface)
{
  iface->register_observer = g_action_muxer_register_observer;
  iface->unregister_observer = g_action_muxer_unregister_observer;
}

static void
g_action_muxer_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_action_muxer_list_actions;
  iface->query_action = g_action_muxer_query_action;
  iface->activate_action = g_action_muxer_activate_action;
  iface->change_action_state = g_action_muxer_change_action_state;
}

static void
g_action_muxer_class_init (GObjectClass *class)
{
  class->get_property = g_action_muxer_get_property;
  class->set_property = g_action_muxer_set_property;
  class->finalize = g_action_muxer_finalize;
  class->dispose = g_action_muxer_dispose;

  properties[PROP_PARENT] = g_param_spec_object ("parent", "Parent",
                                                 "The parent muxer",
                                                 G_TYPE_ACTION_MUXER,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (class, NUM_PROPERTIES, properties);
}

/*
 * g_action_muxer_insert:
 * @muxer: a #GActionMuxer
 * @prefix: the prefix string for the action group
 * @action_group: a #GActionGroup
 *
 * Adds the actions in @action_group to the list of actions provided by
 * @muxer.  @prefix is prefixed to each action name, such that for each
 * action <varname>x</varname> in @action_group, there is an equivalent
 * action @prefix<literal>.</literal><varname>x</varname> in @muxer.
 *
 * For example, if @prefix is "<literal>app</literal>" and @action_group
 * contains an action called "<literal>quit</literal>", then @muxer will
 * now contain an action called "<literal>app.quit</literal>".
 *
 * If any #GActionObservers are registered for actions in the group,
 * "action_added" notifications will be emitted, as appropriate.
 *
 * @prefix must not contain a dot ('.').
 */
void
g_action_muxer_insert (GActionMuxer *muxer,
                       const gchar  *prefix,
                       GActionGroup *action_group)
{
  gchar **actions;
  Group *group;
  gint i;

  /* TODO: diff instead of ripout and replace */
  g_action_muxer_remove (muxer, prefix);

  group = g_slice_new (Group);
  group->muxer = muxer;
  group->group = g_object_ref (action_group);
  group->prefix = g_strdup (prefix);

  g_hash_table_insert (muxer->groups, group->prefix, group);

  actions = g_action_group_list_actions (group->group);
  for (i = 0; actions[i]; i++)
    g_action_muxer_action_added_to_group (group->group, actions[i], group);
  g_strfreev (actions);

  group->handler_ids[0] = g_signal_connect (group->group, "action-added",
                                            G_CALLBACK (g_action_muxer_action_added_to_group), group);
  group->handler_ids[1] = g_signal_connect (group->group, "action-removed",
                                            G_CALLBACK (g_action_muxer_action_removed_from_group), group);
  group->handler_ids[2] = g_signal_connect (group->group, "action-enabled-changed",
                                            G_CALLBACK (g_action_muxer_group_action_enabled_changed), group);
  group->handler_ids[3] = g_signal_connect (group->group, "action-state-changed",
                                            G_CALLBACK (g_action_muxer_group_action_state_changed), group);
}

/*
 * g_action_muxer_remove:
 * @muxer: a #GActionMuxer
 * @prefix: the prefix of the action group to remove
 *
 * Removes a #GActionGroup from the #GActionMuxer.
 *
 * If any #GActionObservers are registered for actions in the group,
 * "action_removed" notifications will be emitted, as appropriate.
 */
void
g_action_muxer_remove (GActionMuxer *muxer,
                       const gchar  *prefix)
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
        g_action_muxer_action_removed_from_group (group->group, actions[i], group);
      g_strfreev (actions);

      g_action_muxer_free_group (group);
    }
}

/*
 * g_action_muxer_new:
 *
 * Creates a new #GActionMuxer.
 */
GActionMuxer *
g_action_muxer_new (void)
{
  return g_object_new (G_TYPE_ACTION_MUXER, NULL);
}

/* g_action_muxer_get_parent:
 * @muxer: a #GActionMuxer
 *
 * Returns: (transfer-none): the parent of @muxer, or NULL.
 */
GActionMuxer *
g_action_muxer_get_parent (GActionMuxer *muxer)
{
  g_return_val_if_fail (G_IS_ACTION_MUXER (muxer), NULL);

  return muxer->parent;
}

/* g_action_muxer_set_parent:
 * @muxer: a #GActionMuxer
 * @parent: (allow-none): the new parent #GActionMuxer
 *
 * Sets the parent of @muxer to @parent.
 */
void
g_action_muxer_set_parent (GActionMuxer *muxer,
                           GActionMuxer *parent)
{
  g_return_if_fail (G_IS_ACTION_MUXER (muxer));
  g_return_if_fail (parent == NULL || G_IS_ACTION_MUXER (parent));

  if (muxer->parent == parent)
    return;

  if (muxer->parent != NULL)
    {
      gchar **actions;
      gchar **it;

      actions = g_action_group_list_actions (G_ACTION_GROUP (muxer->parent));
      for (it = actions; *it; it++)
        g_action_muxer_action_removed (muxer, *it);
      g_strfreev (actions);

      g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_action_added_to_parent, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_action_removed_from_parent, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_parent_action_enabled_changed, muxer);
      g_signal_handlers_disconnect_by_func (muxer->parent, g_action_muxer_parent_action_state_changed, muxer);

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
        g_action_muxer_action_added (muxer, *it, G_ACTION_GROUP (muxer->parent), *it);
      g_strfreev (actions);

      g_signal_connect (muxer->parent, "action-added",
                        G_CALLBACK (g_action_muxer_action_added_to_parent), muxer);
      g_signal_connect (muxer->parent, "action-removed",
                        G_CALLBACK (g_action_muxer_action_removed_from_parent), muxer);
      g_signal_connect (muxer->parent, "action-enabled-changed",
                        G_CALLBACK (g_action_muxer_parent_action_enabled_changed), muxer);
      g_signal_connect (muxer->parent, "action-state-changed",
                        G_CALLBACK (g_action_muxer_parent_action_state_changed), muxer);
    }

  g_object_notify_by_pspec (G_OBJECT (muxer), properties[PROP_PARENT]);
}
