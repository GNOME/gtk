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
#include "gtkactionmuxerprofileprivate.h"
#include "gtkactionkeyprivate.h"
#include "gtkactiontreeprivate.h"

#include "gtkactionobservableprivate.h"
#include "gtkactionobserverprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkmarshalers.h"
#include "gtkwidgetprivate.h"
#include "gsettings-mapping.h"
#include "gtkdebug.h"
#include "gtkprivate.h"

#include <string.h>

static GtkActionMuxerProfile action_muxer_profile;

void
_gtk_action_muxer_profile_reset (void)
{
  memset (&action_muxer_profile, 0, sizeof action_muxer_profile);
}

void
_gtk_action_muxer_profile_get (GtkActionMuxerProfile *profile)
{
  g_return_if_fail (profile != NULL);

  *profile = action_muxer_profile;
}

void
_gtk_action_muxer_profile_binding_touched (void)
{
  action_muxer_profile.touched_bindings++;
}

#define PROFILE_CALLBACK() (action_muxer_profile.callback_deliveries++)
#define PROFILE_ALLOCATION() (action_muxer_profile.allocations++)
#define PROFILE_RELAY_EDGE() (action_muxer_profile.relay_edges++)

/*< private >
 * GtkActionMuxer:
 *
 * GtkActionMuxer aggregates and monitors actions from multiple sources.
 *
 * `GtkActionMuxer` is `GtkActionObservable` and `GtkActionObserver` that
 * offers a `GActionGroup`-like api and is capable of containing other
 * `GActionGroup` instances. `GtkActionMuxer` does not implement the
 * `GActionGroup` interface because it requires excessive signal emissions
 * and has poor scalability. We use the `GtkActionObserver` machinery
 * instead to propagate changes between action muxer instances and
 * to other users.
 *
 * Beyond action groups, `GtkActionMuxer` can incorporate actions that
 * are associated with widget classes (*class actions*) and actions
 * that are associated with the parent widget, allowing for recursive
 * lookup.
 *
 * In addition to the action attributes provided by `GActionGroup`,
 * `GtkActionMuxer` maintains a *primary accelerator* string for
 * actions that can be shown in menuitems.
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
 * `GtkActionMuxer` with the prefixes “app” and “win”, respectively.
 * This would expose the actions as “app.quit” and “win.fullscreen”
 * on the `GActionGroup`-like interface presented by the `GtkActionMuxer`.
 *
 * Activations and state change requests on the `GtkActionMuxer` are
 * wired through to the underlying actions in the expected way.
 *
 * This class is typically only used at the site of “consumption” of
 * actions (eg: when displaying a menu that contains many actions on
 * different objects).
 */

static void     gtk_action_muxer_observable_iface_init    (GtkActionObservableInterface *iface);
static void     gtk_action_muxer_observer_iface_init      (GtkActionObserverInterface *iface);

typedef GObjectClass GtkActionMuxerClass;

struct _GtkActionMuxer
{
  GObject parent_instance;
  GtkActionMuxer *parent;
  GtkWidget *widget;
  GtkActionNode *node;

  GHashTable *observed_actions;
  GHashTable *groups;
  GtkBitmask *widget_actions_disabled;
  guint owns_node : 1;
};

G_DEFINE_TYPE_WITH_CODE (GtkActionMuxer, gtk_action_muxer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVER, gtk_action_muxer_observer_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVABLE, gtk_action_muxer_observable_iface_init))

enum
{
  PROP_0,
  PROP_PARENT,
  PROP_WIDGET,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

typedef struct _Action Action;
typedef struct _Watcher Watcher;

struct _Watcher
{
  Action *action;
  GtkActionObserver *observer;
  Watcher *previous;
  Watcher *next;
  Watcher *garbage_next;
  GtkActionSubscription *subscription;
  guint cancelled : 1;
  guint initializing : 1;
};

struct _Action
{
  GtkActionMuxer *muxer;
  Watcher *watchers;
  Watcher *garbage;
  GtkActionKey *key;
  guint n_watchers;
  guint dispatch_depth;
  guint local_add_serial;
  guint upstream_registered : 1;
  guint pending_remove : 1;
};

typedef struct
{
  GtkActionMuxer *muxer;
  GActionGroup *group;
  char         *prefix;
  gulong        handler_ids[4];
} Group;

GtkWidgetAction *
gtk_widget_class_lookup_action (GtkWidgetClass *widget_class,
                                const char     *action_name)
{
  GtkWidgetClassPrivate *priv;
  guint i;

  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), NULL);
  g_return_val_if_fail (action_name != NULL, NULL);

  priv = widget_class->priv;
  if (priv->actions == NULL)
    return NULL;

  if (priv->actions->len > 4)
    {
      if (priv->action_index == NULL)
        {
          priv->action_index = g_hash_table_new (g_str_hash, g_str_equal);
          for (i = 0; i < priv->actions->len; i++)
            {
              GtkWidgetAction *action = g_ptr_array_index (priv->actions, i);

              if (!g_hash_table_contains (priv->action_index, action->name))
                g_hash_table_insert (priv->action_index, action->name, action);
            }
        }

      return g_hash_table_lookup (priv->action_index, action_name);
    }

  for (i = 0; i < priv->actions->len; i++)
    {
      GtkWidgetAction *action = g_ptr_array_index (priv->actions, i);

      if (strcmp (action->name, action_name) == 0)
        return action;
    }

  return NULL;
}

static void
gtk_action_muxer_append_group_actions (const char *prefix,
                                       Group      *group,
                                       GHashTable *actions)
{
  char **group_actions;
  char **action;

  group_actions = g_action_group_list_actions (group->group);
  for (action = group_actions; *action; action++)
    {
      char *name = g_strconcat (prefix, ".", *action, NULL);
      PROFILE_ALLOCATION ();
      g_hash_table_add (actions, name);
    }

  g_strfreev (group_actions);
}

char **
gtk_action_muxer_list_actions (GtkActionMuxer *muxer,
                               gboolean        local_only)
{
  GHashTable *actions;
  char **keys;

  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);

  actions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for ( ; muxer != NULL; muxer = muxer->parent)
    {
      GHashTableIter iter;
      const char *prefix;
      Group *group;

      if (muxer->widget)
        {
          GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (muxer->widget);
          GtkWidgetClassPrivate *priv = klass->priv;
          guint i;

          for (i = 0; priv->actions != NULL && i < priv->actions->len; i++)
            {
              GtkWidgetAction *action = g_ptr_array_index (priv->actions, i);

              PROFILE_ALLOCATION ();
              g_hash_table_add (actions, g_strdup (action->name));
            }
        }

      if (muxer->groups)
        {
          g_hash_table_iter_init (&iter, muxer->groups);
          while (g_hash_table_iter_next (&iter, (gpointer *)&prefix, (gpointer *)&group))
            gtk_action_muxer_append_group_actions (prefix, group, actions);
        }

      if (local_only)
        break;
    }

  keys = (char **)g_hash_table_get_keys_as_array (actions, NULL);

  g_hash_table_steal_all (actions);
  g_hash_table_unref (actions);

  return (char **)keys;
}

static Group *
gtk_action_muxer_lookup_group (GtkActionMuxer  *muxer,
                               const char      *full_name,
                               const char     **action_name)
{
  g_autoptr(GtkActionKey) key = NULL;
  gsize prefix_len;
  char *prefix;
  Group *group;

  if (!muxer->groups)
    return NULL;

  if (!(key = gtk_action_key_new (full_name)))
    return NULL;

  prefix_len = gtk_action_key_get_prefix_length (key);
  prefix = g_alloca (prefix_len + 1);
  memcpy (prefix, gtk_action_key_get_full_name (key), prefix_len);
  prefix[prefix_len] = '\0';

  group = g_hash_table_lookup (muxer->groups, prefix);

  if (action_name)
    *action_name = full_name + prefix_len + 1;

  return group;
}

GActionGroup *
gtk_action_muxer_find (GtkActionMuxer  *muxer,
                       const char      *action_name,
                       const char     **unprefixed_name)
{
  const char *name;
  Group *group;

  group = gtk_action_muxer_lookup_group (muxer, action_name, &name);
  if (group && g_action_group_has_action (group->group, name))
    {
      if (unprefixed_name)
        *unprefixed_name = name;

      return group->group;
    }

  return NULL;
}

GActionGroup *
gtk_action_muxer_get_group (GtkActionMuxer *muxer,
                            const char     *group_name)
{
  Group *group;

  if (!muxer->groups)
    return NULL;

  group = g_hash_table_lookup (muxer->groups, group_name);
  if (group)
    return group->group;

  return NULL;
}

static inline Action *
find_observers (GtkActionMuxer *muxer,
                const char     *action_name)
{
  if (muxer->observed_actions)
    return g_hash_table_lookup (muxer->observed_actions, action_name);

  return NULL;
}

static gboolean
action_has_observer (Action            *action,
                     GtkActionObserver *observer)
{
  Watcher *watcher;

  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    if (!watcher->cancelled && watcher->observer == observer)
      return TRUE;

  return FALSE;
}

static void
action_collect_garbage (Action *action)
{
  Watcher *watcher;

  g_assert (action->dispatch_depth == 0);

  while ((watcher = action->garbage))
    {
      action->garbage = watcher->garbage_next;
      g_free (watcher);
    }
}

static void
action_dispatch_end (Action *action)
{
  g_assert (action->dispatch_depth > 0);

  action->dispatch_depth--;
  if (action->dispatch_depth == 0)
    {
      action_collect_garbage (action);

      if (action->pending_remove)
        g_hash_table_remove (action->muxer->observed_actions,
                             gtk_action_key_get_full_name (action->key));
    }
}

void
gtk_action_muxer_action_enabled_changed (GtkActionMuxer *muxer,
                                         const char     *action_name,
                                         gboolean        enabled)
{
  GtkWidgetAction *iter;
  Action *action;
  Watcher *watcher;

  if (muxer->widget)
    {
      if ((iter = gtk_widget_class_lookup_action (GTK_WIDGET_GET_CLASS (muxer->widget),
                                                  action_name)))
        {
          muxer->widget_actions_disabled =
            _gtk_bitmask_set (muxer->widget_actions_disabled, iter->slot, !enabled);
          if (muxer->node != NULL)
            gtk_action_node_class_action_enabled_changed (muxer->node, action_name);
        }
    }

  action = find_observers (muxer, action_name);

  if (action)
    action->dispatch_depth++;
  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    if (!watcher->cancelled)
      {
        PROFILE_CALLBACK ();
        gtk_action_observer_action_enabled_changed (watcher->observer,
                                                    GTK_ACTION_OBSERVABLE (muxer),
                                                    action_name,
                                                    enabled);
      }
  if (action)
    action_dispatch_end (action);
}

static void
gtk_action_muxer_group_action_enabled_changed (GActionGroup *action_group,
                                               const char   *action_name,
                                               gboolean      enabled,
                                               gpointer      user_data)
{
  Group *group = user_data;
  char *fullname;

  PROFILE_ALLOCATION ();
  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_enabled_changed (group->muxer, fullname, enabled);
  g_free (fullname);
}

void
gtk_action_muxer_action_state_changed (GtkActionMuxer *muxer,
                                       const char     *action_name,
                                       GVariant       *state)
{
  Action *action;
  Watcher *watcher;

  action = find_observers (muxer, action_name);
  if (action)
    action->dispatch_depth++;
  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    if (!watcher->cancelled)
      {
        PROFILE_CALLBACK ();
        gtk_action_observer_action_state_changed (watcher->observer,
                                                  GTK_ACTION_OBSERVABLE (muxer),
                                                  action_name,
                                                  state);
      }
  if (action)
    action_dispatch_end (action);
}

static void
gtk_action_muxer_group_action_state_changed (GActionGroup *action_group,
                                             const char   *action_name,
                                             GVariant     *state,
                                             gpointer      user_data)
{
  Group *group = user_data;
  char *fullname;

  PROFILE_ALLOCATION ();
  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_state_changed (group->muxer, fullname, state);
  g_free (fullname);
}

static gboolean action_muxer_query_action (GtkActionMuxer      *muxer,
                                           const char          *action_name,
                                           gboolean            *enabled,
                                           const GVariantType **parameter_type,
                                           const GVariantType **state_type,
                                           GVariant           **state_hint,
                                           GVariant           **state,
                                           gboolean             recurse);
static void gtk_action_muxer_action_added (GtkActionMuxer     *muxer,
                                           const char         *action_name,
                                           const GVariantType *parameter_type,
                                           gboolean            enabled,
                                           GVariant           *state);
static void gtk_action_muxer_action_removed (GtkActionMuxer *muxer,
                                             const char     *action_name);

static void
notify_observers_added (GtkActionMuxer *muxer,
                        GtkActionMuxer *parent)
{
  GPtrArray *names;
  GHashTableIter iter;
  gpointer key;

  if (!muxer->observed_actions)
    return;

  names = g_ptr_array_new_with_free_func (g_free);
  g_hash_table_iter_init (&iter, muxer->observed_actions);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      PROFILE_ALLOCATION ();
      g_ptr_array_add (names, g_strdup (key));
    }

  for (guint i = 0; i < names->len; i++)
    {
      const char *action_name = g_ptr_array_index (names, i);
      const GVariantType *parameter_type;
      gboolean enabled;
      GVariant *state = NULL;
      Action *action = find_observers (muxer, action_name);
      Watcher *watcher;

      if (!action || action->n_watchers == 0)
        continue;
      if (action->watchers->subscription != NULL)
        continue;

      action->dispatch_depth++;
      for (watcher = action->watchers; watcher; watcher = watcher->next)
        if (!watcher->cancelled)
          {
            PROFILE_CALLBACK ();
            gtk_action_observer_primary_accel_changed (watcher->observer,
                                                       GTK_ACTION_OBSERVABLE (muxer),
                                                       action_name,
                                                       NULL);
          }
      action_dispatch_end (action);

      action = find_observers (muxer, action_name);
      if (!action || action->n_watchers == 0)
        continue;

      gtk_action_observable_subscribe (GTK_ACTION_OBSERVABLE (parent),
                                       action_name,
                                       GTK_ACTION_OBSERVER (muxer),
                                       NULL, NULL, NULL);
      PROFILE_RELAY_EDGE ();
      action->upstream_registered = TRUE;

      if (action_muxer_query_action (muxer, action_name,
                                     &enabled, &parameter_type,
                                     NULL, NULL, &state,
                                     TRUE))
        gtk_action_muxer_action_added (muxer, action_name, parameter_type, enabled, state);

      g_clear_pointer (&state, g_variant_unref);
    }

  g_ptr_array_unref (names);
}

static void
notify_observers_removed (GtkActionMuxer *muxer,
                          GtkActionMuxer *parent)
{
  GPtrArray *names;
  GHashTableIter iter;
  gpointer key;

  if (!muxer->observed_actions)
    return;

  names = g_ptr_array_new_with_free_func (g_free);
  g_hash_table_iter_init (&iter, muxer->observed_actions);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      PROFILE_ALLOCATION ();
      g_ptr_array_add (names, g_strdup (key));
    }

  for (guint i = 0; i < names->len; i++)
    {
      const char *action_name = g_ptr_array_index (names, i);
      Action *action = find_observers (muxer, action_name);

      if (!action || !action->upstream_registered)
        continue;
      if (action->watchers->subscription != NULL)
        continue;

      action->upstream_registered = FALSE;
      gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (parent),
                                                 action_name,
                                                 GTK_ACTION_OBSERVER (muxer));
      gtk_action_muxer_action_removed (muxer, action_name);
    }

  g_ptr_array_unref (names);
}

static void
gtk_action_muxer_action_added (GtkActionMuxer     *muxer,
                               const char         *action_name,
                               const GVariantType *parameter_type,
                               gboolean            enabled,
                               GVariant           *state)
{
  Action *action;
  Watcher *watcher;

  action = find_observers (muxer, action_name);
  if (action)
    action->dispatch_depth++;
  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    if (!watcher->cancelled)
      {
        PROFILE_CALLBACK ();
        gtk_action_observer_action_added (watcher->observer,
                                          GTK_ACTION_OBSERVABLE (muxer),
                                          action_name, parameter_type, enabled, state);
      }
  if (action)
    action_dispatch_end (action);
}

static void
gtk_action_muxer_action_added_to_group (GActionGroup *action_group,
                                        const char   *action_name,
                                        gpointer      user_data)
{
  Group *group = user_data;
  GtkActionMuxer *muxer = group->muxer;
  Action *action;
  const GVariantType *parameter_type;
  gboolean enabled;
  GVariant *state;
  char *fullname;

  PROFILE_ALLOCATION ();
  fullname = g_strconcat (group->prefix, ".", action_name, NULL);

  action = find_observers (muxer, fullname);

  if (action)
    action->local_add_serial++;

  if (action && action->upstream_registered)
    {
      action->upstream_registered = FALSE;
      gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (muxer->parent),
                                                 fullname,
                                                 GTK_ACTION_OBSERVER (muxer));
    }

  if (action && action->n_watchers > 0 &&
      (++action_muxer_profile.source_queries,
       g_action_group_query_action (action_group, action_name,
                                    &enabled, &parameter_type, NULL, NULL, &state)))
    {
      gtk_action_muxer_action_added (muxer, fullname, parameter_type, enabled, state);

      if (state)
        g_variant_unref (state);
    }

  g_free (fullname);
}

static void
gtk_action_muxer_action_removed (GtkActionMuxer *muxer,
                                 const char     *action_name)
{
  Action *action;
  Watcher *watcher;

  action = find_observers (muxer, action_name);
  if (action)
    action->dispatch_depth++;
  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    if (!watcher->cancelled)
      {
        PROFILE_CALLBACK ();
        gtk_action_observer_action_removed (watcher->observer,
                                            GTK_ACTION_OBSERVABLE (muxer),
                                            action_name);
      }
  if (action)
    action_dispatch_end (action);
}

static void
gtk_action_muxer_action_removed_from_group (GActionGroup *action_group,
                                            const char   *action_name,
                                            gpointer      user_data)
{
  Group *group = user_data;
  GtkActionMuxer *muxer = group->muxer;
  Action *action;
  guint local_add_serial;
  char *fullname;

  PROFILE_ALLOCATION ();
  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  action = find_observers (muxer, fullname);
  local_add_serial = action ? action->local_add_serial : 0;
  gtk_action_muxer_action_removed (muxer, fullname);

  action = find_observers (muxer, fullname);

  if (action && action->local_add_serial == local_add_serial &&
      action->n_watchers > 0 && muxer->parent)
    {
      const GVariantType *parameter_type;
      gboolean enabled;
      GVariant *state = NULL;
      gboolean present;

      if (!action->upstream_registered)
        {
          action->upstream_registered = TRUE;
          gtk_action_observable_subscribe (GTK_ACTION_OBSERVABLE (muxer->parent),
                                           fullname,
                                           GTK_ACTION_OBSERVER (muxer),
                                           NULL, NULL, NULL);
        }

      present = action_muxer_query_action (muxer->parent, fullname,
                                           &enabled, &parameter_type,
                                           NULL, NULL, &state, TRUE);
      if (present)
        gtk_action_muxer_action_added (muxer, fullname, parameter_type, enabled, state);
      g_clear_pointer (&state, g_variant_unref);
    }

  g_free (fullname);
}

static void
gtk_action_muxer_primary_accel_changed (GtkActionMuxer *muxer,
                                        const char     *action_name,
                                        const char     *action_and_target)
{
  Action *action;
  Watcher *watcher;

  if (!action_name)
    action_name = strrchr (action_and_target, '|') + 1;

  action = find_observers (muxer, action_name);
  if (action)
    action->dispatch_depth++;
  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    if (!watcher->cancelled)
      {
        PROFILE_CALLBACK ();
        gtk_action_observer_primary_accel_changed (watcher->observer,
                                                   GTK_ACTION_OBSERVABLE (muxer),
                                                   action_name,
                                                   action_and_target);
      }
  if (action)
    action_dispatch_end (action);
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

gboolean
gtk_widget_action_query (GtkWidget       *widget,
                         GtkWidgetAction *action,
                         gboolean        *enabled,
                         GVariant       **state_hint,
                         GVariant       **state)
{
  GtkActionMuxer *muxer;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (action != NULL, FALSE);

  muxer = _gtk_widget_get_action_muxer (widget, TRUE);

  if (enabled != NULL)
    *enabled = !_gtk_bitmask_get (muxer->widget_actions_disabled, action->slot);
  if (state_hint != NULL)
    *state_hint = action->pspec != NULL ? prop_action_get_state_hint (widget, action) : NULL;
  if (state != NULL)
    *state = action->pspec != NULL ? prop_action_get_state (widget, action) : NULL;

  return TRUE;
}

void
gtk_widget_action_activate (GtkWidget       *widget,
                            GtkWidgetAction *action,
                            GVariant        *parameter)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (action != NULL);

  if (action->activate != NULL)
    action->activate (widget, action->name, parameter);
  else if (action->pspec != NULL)
    prop_action_activate (widget, action, parameter);
}

void
gtk_widget_action_change_state (GtkWidget       *widget,
                                GtkWidgetAction *action,
                                GVariant        *state)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (action != NULL);
  g_return_if_fail (state != NULL);

  if (action->pspec != NULL)
    prop_action_set_state (widget, action, state);
}

static gboolean
action_muxer_query_action (GtkActionMuxer      *muxer,
                           const char          *action_name,
                           gboolean            *enabled,
                           const GVariantType **parameter_type,
                           const GVariantType **state_type,
                           GVariant           **state_hint,
                           GVariant           **state,
                           gboolean             recurse)
{
  GtkWidgetAction *action;
  Group *group;
  const char *unprefixed_name;

  action_muxer_profile.resolutions++;

  if (muxer->widget)
    {
      if ((action = gtk_widget_class_lookup_action (GTK_WIDGET_GET_CLASS (muxer->widget),
                                                    action_name)))
        {
          if (enabled)
            *enabled = !_gtk_bitmask_get (muxer->widget_actions_disabled, action->slot);
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

  group = gtk_action_muxer_lookup_group (muxer, action_name, &unprefixed_name);

  if (group && g_action_group_has_action (group->group, unprefixed_name))
    {
      action_muxer_profile.source_queries++;
      return g_action_group_query_action (group->group, unprefixed_name, enabled,
                                          parameter_type, state_type, state_hint, state);
    }

  if (muxer->parent && recurse)
    return gtk_action_muxer_query_action (muxer->parent, action_name,
                                          enabled, parameter_type,
                                          state_type, state_hint, state);

  return FALSE;
}

gboolean
gtk_action_muxer_query_action (GtkActionMuxer      *muxer,
                               const char          *action_name,
                               gboolean            *enabled,
                               const GVariantType **parameter_type,
                               const GVariantType **state_type,
                               GVariant           **state_hint,
                               GVariant           **state)
{
  return action_muxer_query_action (muxer, action_name,
                                    enabled, parameter_type,
                                    state_type, state_hint, state,
                                    TRUE);
}

gboolean
gtk_action_muxer_has_action (GtkActionMuxer *muxer,
                             const char     *action_name)
{
  return action_muxer_query_action (muxer, action_name,
                                    NULL, NULL, NULL, NULL, NULL,
                                    TRUE);
}

gboolean
gtk_action_muxer_activate_action (GtkActionMuxer *muxer,
                                  const char     *action_name,
                                  GVariant       *parameter)
{
  const char *unprefixed_name;
  Group *group;

  if (muxer->widget)
    {
      GtkWidgetAction *action;

      if ((action = gtk_widget_class_lookup_action (GTK_WIDGET_GET_CLASS (muxer->widget),
                                                    action_name)))
        {
          if (!_gtk_bitmask_get (muxer->widget_actions_disabled, action->slot))
            {
              if (action->activate)
                {
                  GTK_DEBUG (ACTIONS, "%s: activate action", action->name);
                  action->activate (muxer->widget, action->name, parameter);
                }
              else if (action->pspec)
                {
                  GTK_DEBUG (ACTIONS, "%s: activate prop action", action->pspec->name);
                  prop_action_activate (muxer->widget, action, parameter);
                }
            }

          return TRUE;
        }
    }

  group = gtk_action_muxer_lookup_group (muxer, action_name, &unprefixed_name);

  if (group && g_action_group_has_action (group->group, unprefixed_name))
    {
      g_action_group_activate_action (group->group, unprefixed_name, parameter);
      return TRUE;
    }
  else if (muxer->parent)
    return gtk_action_muxer_activate_action (muxer->parent, action_name, parameter);

  return FALSE;
}

void
gtk_action_muxer_change_action_state (GtkActionMuxer *muxer,
                                      const char     *action_name,
                                      GVariant       *state)
{
  GtkWidgetAction *action;
  const char *unprefixed_name;
  Group *group;

  if (muxer->widget)
    {
      if ((action = gtk_widget_class_lookup_action (GTK_WIDGET_GET_CLASS (muxer->widget),
                                                    action_name)))
        {
          if (action->pspec)
            prop_action_set_state (muxer->widget, action, state);

          return;
        }
    }

  group = gtk_action_muxer_lookup_group (muxer, action_name, &unprefixed_name);

  if (group && g_action_group_has_action (group->group, unprefixed_name))
    g_action_group_change_action_state (group->group, unprefixed_name, state);
  else if (muxer->parent)
    gtk_action_muxer_change_action_state (muxer->parent, action_name, state);
}

static void
gtk_action_muxer_unregister_internal (Watcher *watcher)
{
  Action *action = watcher->action;
  GtkActionMuxer *muxer = action->muxer;

  g_assert (!watcher->cancelled);
  g_assert (action->n_watchers > 0);

  watcher->cancelled = TRUE;
  if (watcher->subscription != NULL)
    {
      gtk_action_subscription_cancel (watcher->subscription);
      watcher->subscription = NULL;
    }
  if (watcher->previous)
    watcher->previous->next = watcher->next;
  else
    action->watchers = watcher->next;
  if (watcher->next)
    watcher->next->previous = watcher->previous;

  action->n_watchers--;
  if (action->dispatch_depth > 0)
    {
      watcher->garbage_next = action->garbage;
      action->garbage = watcher;
    }
  else
    g_free (watcher);

  if (action->n_watchers == 0)
    {
      if (action->upstream_registered)
        {
          action->upstream_registered = FALSE;
          gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (muxer->parent),
                                                     gtk_action_key_get_full_name (action->key),
                                                     GTK_ACTION_OBSERVER (muxer));
        }

      if (action->dispatch_depth > 0)
        action->pending_remove = TRUE;
      else
        g_hash_table_remove (muxer->observed_actions,
                             gtk_action_key_get_full_name (action->key));
    }
}

static void
gtk_action_muxer_weak_notify (gpointer  data,
                              GObject  *where_the_object_was)
{
  Watcher *watcher = data;

  gtk_action_muxer_unregister_internal (watcher);
}

static void gtk_action_muxer_free_action (gpointer data);

static void
gtk_action_muxer_subscription_changed (GtkActionSubscription   *subscription,
                                       GtkActionChange          changed,
                                       const GtkActionSnapshot *snapshot,
                                       gpointer                 user_data)
{
  Watcher *watcher = user_data;
  Action *action = watcher->action;
  GtkActionMuxer *muxer = action->muxer;
  const char *name = gtk_action_key_get_full_name (action->key);
  gboolean provider_changed;

  if (watcher->cancelled || watcher->initializing)
    return;

  provider_changed = ((changed & GTK_ACTION_CHANGE_PROVIDER) != 0 &&
                      snapshot->present);

  action->dispatch_depth++;
  if ((changed & GTK_ACTION_CHANGE_PRESENT) != 0 && !snapshot->present)
    gtk_action_observer_action_removed (watcher->observer,
                                        GTK_ACTION_OBSERVABLE (muxer),
                                        name);
  else if (provider_changed)
    {
      gtk_action_observer_action_removed (watcher->observer,
                                          GTK_ACTION_OBSERVABLE (muxer),
                                          name);
      if (!watcher->cancelled)
        gtk_action_observer_action_added (watcher->observer,
                                          GTK_ACTION_OBSERVABLE (muxer),
                                          name,
                                          snapshot->parameter_type,
                                          snapshot->enabled,
                                          snapshot->state);
    }
  else if ((changed & GTK_ACTION_CHANGE_PRESENT) != 0 && snapshot->present)
    gtk_action_observer_action_added (watcher->observer,
                                      GTK_ACTION_OBSERVABLE (muxer),
                                      name,
                                      snapshot->parameter_type,
                                      snapshot->enabled,
                                      snapshot->state);
  else
    {
      if ((changed & GTK_ACTION_CHANGE_ENABLED) != 0)
        gtk_action_observer_action_enabled_changed (watcher->observer,
                                                    GTK_ACTION_OBSERVABLE (muxer),
                                                    name,
                                                    snapshot->enabled);
      if (!watcher->cancelled && (changed & GTK_ACTION_CHANGE_STATE) != 0)
        gtk_action_observer_action_state_changed (watcher->observer,
                                                  GTK_ACTION_OBSERVABLE (muxer),
                                                  name,
                                                  snapshot->state);
      if (!watcher->cancelled && (changed & GTK_ACTION_CHANGE_ACCEL) != 0)
        gtk_action_observer_primary_accel_changed (watcher->observer,
                                                   GTK_ACTION_OBSERVABLE (muxer),
                                                   name,
                                                   name);
    }
  action_dispatch_end (action);
}

static gboolean
gtk_action_muxer_subscribe (GtkActionObservable *observable,
                            const char          *name,
                            GtkActionObserver   *observer,
                            gboolean            *enabled,
                            const GVariantType **parameter_type,
                            GVariant           **state)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  Action *action;
  Watcher *watcher;
  gboolean present;

  if (!muxer->observed_actions)
    muxer->observed_actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gtk_action_muxer_free_action);

  action = g_hash_table_lookup (muxer->observed_actions, name);

  if (action == NULL)
    {
      GtkActionKey *key;

      if (!(key = gtk_action_key_new (name)))
        return FALSE;

      PROFILE_ALLOCATION ();
      action = g_new0 (Action, 1);
      action->muxer = muxer;
      action->key = key;

      g_hash_table_insert (muxer->observed_actions,
                           (gpointer) gtk_action_key_get_full_name (action->key),
                           action);
    }

  PROFILE_ALLOCATION ();
  watcher = g_new0 (Watcher, 1);
  action->pending_remove = FALSE;
  watcher->action = action;
  watcher->observer = observer;
  watcher->initializing = TRUE;
  watcher->next = action->watchers;
  if (watcher->next)
    watcher->next->previous = watcher;
  action->watchers = watcher;
  action->n_watchers++;
  g_object_weak_ref (G_OBJECT (observer), gtk_action_muxer_weak_notify, watcher);

  if (muxer->node != NULL)
    {
      watcher->subscription = gtk_action_node_subscribe (muxer->node,
                                                         action->key,
                                                         NULL,
                                                         (GTK_ACTION_INTEREST_PRESENT |
                                                          GTK_ACTION_INTEREST_ENABLED |
                                                          GTK_ACTION_INTEREST_RAW_STATE |
                                                          GTK_ACTION_INTEREST_ACCEL),
                                                         gtk_action_muxer_subscription_changed,
                                                         watcher,
                                                         NULL);
    }
  watcher->initializing = FALSE;

  if (watcher->subscription == NULL && action->n_watchers == 1 && muxer->parent)
    {
      gtk_action_observable_subscribe (GTK_ACTION_OBSERVABLE (muxer->parent),
                                       name,
                                       GTK_ACTION_OBSERVER (muxer),
                                       NULL, NULL, NULL);
      PROFILE_RELAY_EDGE ();
      action->upstream_registered = TRUE;
    }

  if (watcher->subscription != NULL)
    {
      const GtkActionSnapshot *snapshot;

      snapshot = gtk_action_subscription_get_snapshot (watcher->subscription);
      present = snapshot->present;
      if (enabled != NULL)
        *enabled = snapshot->enabled;
      if (parameter_type != NULL)
        *parameter_type = snapshot->parameter_type;
      if (state != NULL)
        *state = snapshot->state != NULL ? g_variant_ref (snapshot->state) : NULL;
    }
  else
    present = action_muxer_query_action (muxer, name,
                                         enabled, parameter_type,
                                         NULL, NULL, state, TRUE);

  return present;
}

static void
gtk_action_muxer_register_observer (GtkActionObservable *observable,
                                    const char          *name,
                                    GtkActionObserver   *observer)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  const GVariantType *parameter_type;
  gboolean enabled;
  gboolean duplicate;
  GVariant *state = NULL;

  duplicate = action_has_observer (find_observers (muxer, name), observer);

  if (gtk_action_muxer_subscribe (observable, name, observer,
                                  &enabled, &parameter_type, &state) &&
      !duplicate)
    {
      PROFILE_CALLBACK ();
      gtk_action_observer_action_added (observer, observable, name,
                                        parameter_type, enabled, state);
    }

  g_clear_pointer (&state, g_variant_unref);
}

static void
gtk_action_muxer_unregister_observer (GtkActionObservable *observable,
                                      const char          *name,
                                      GtkActionObserver   *observer)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  Action *action = find_observers (muxer, name);
  Watcher *watcher;

  for (watcher = action ? action->watchers : NULL; watcher; watcher = watcher->next)
    {
      if (!watcher->cancelled && watcher->observer == observer)
        {
          g_object_weak_unref (G_OBJECT (observer), gtk_action_muxer_weak_notify, watcher);
          gtk_action_muxer_unregister_internal (watcher);
          break;
        }
    }
}

static void
gtk_action_muxer_free_group (gpointer data)
{
  Group *group = data;
  int i;

  /* 'for loop' or 'four loop'? */
  for (i = 0; i < 4; i++)
    g_clear_signal_handler (&group->handler_ids[i], group->group);

  g_object_unref (group->group);
  g_free (group->prefix);

  g_free (group);
}

static void
gtk_action_muxer_free_action (gpointer data)
{
  Action *action = data;
  Watcher *watcher;

  g_assert (action->dispatch_depth == 0);

  while ((watcher = action->watchers))
    {
      action->watchers = watcher->next;
      if (watcher->subscription != NULL)
        {
          gtk_action_subscription_cancel (watcher->subscription);
          watcher->subscription = NULL;
        }
      g_object_weak_unref (G_OBJECT (watcher->observer),
                           gtk_action_muxer_weak_notify,
                           watcher);
      g_free (watcher);
    }
  action_collect_garbage (action);
  gtk_action_key_unref (action->key);

  g_free (action);
}

static void
gtk_action_muxer_finalize (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  if (muxer->observed_actions)
    {
      g_assert_cmpint (g_hash_table_size (muxer->observed_actions), ==, 0);
      g_hash_table_unref (muxer->observed_actions);
    }
  if (muxer->groups)
    g_hash_table_unref (muxer->groups);


  _gtk_bitmask_free (muxer->widget_actions_disabled);

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)->finalize (object);
}

static void
gtk_action_muxer_dispose (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  g_clear_object (&muxer->parent);
  if (muxer->observed_actions)
    g_hash_table_remove_all (muxer->observed_actions);

  if (muxer->owns_node && muxer->node != NULL)
    gtk_action_node_remove (muxer->node);
  muxer->node = NULL;

  muxer->widget = NULL;

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)->dispose (object);
}

void
gtk_action_muxer_connect_class_actions (GtkActionMuxer *muxer)
{
  g_return_if_fail (GTK_IS_ACTION_MUXER (muxer));
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_action_muxer_init (GtkActionMuxer *muxer)
{
  muxer->widget_actions_disabled = _gtk_bitmask_new ();
}

static void
gtk_action_muxer_observable_iface_init (GtkActionObservableInterface *iface)
{
  iface->register_observer = gtk_action_muxer_register_observer;
  iface->unregister_observer = gtk_action_muxer_unregister_observer;
  iface->subscribe = gtk_action_muxer_subscribe;
}


static void
gtk_action_muxer_observer_action_added (GtkActionObserver    *observer,
                                        GtkActionObservable  *observable,
                                        const char           *action_name,
                                        const GVariantType   *parameter_type,
                                        gboolean              enabled,
                                        GVariant             *state)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_added (GTK_ACTION_MUXER (observer),
                                 action_name,
                                 parameter_type,
                                 enabled,
                                 state);
}

static void
gtk_action_muxer_observer_action_removed (GtkActionObserver   *observer,
                                          GtkActionObservable *observable,
                                          const char          *action_name)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_removed (GTK_ACTION_MUXER (observer), action_name);
}

static void
gtk_action_muxer_observer_action_enabled_changed (GtkActionObserver   *observer,
                                                  GtkActionObservable *observable,
                                                  const char          *action_name,
                                                  gboolean             enabled)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_enabled_changed (GTK_ACTION_MUXER (observer), action_name, enabled);
}

static void
gtk_action_muxer_observer_action_state_changed (GtkActionObserver   *observer,
                                                GtkActionObservable *observable,
                                                const char          *action_name,
                                                GVariant            *state)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_state_changed (GTK_ACTION_MUXER (observer), action_name, state);
}

static void
gtk_action_muxer_observer_primary_accel_changed (GtkActionObserver   *observer,
                                                 GtkActionObservable *observable,
                                                 const char          *action_name,
                                                 const char          *action_and_target)
{
  gtk_action_muxer_primary_accel_changed (GTK_ACTION_MUXER (observer),
                                          action_name,
                                          action_and_target);
}

static void
gtk_action_muxer_observer_iface_init (GtkActionObserverInterface *iface)
{
  iface->action_added = gtk_action_muxer_observer_action_added;
  iface->action_removed = gtk_action_muxer_observer_action_removed;
  iface->action_enabled_changed = gtk_action_muxer_observer_action_enabled_changed;
  iface->action_state_changed = gtk_action_muxer_observer_action_state_changed;
  iface->primary_accel_changed = gtk_action_muxer_observer_primary_accel_changed;
}

static void
gtk_action_muxer_class_init (GObjectClass *class)
{
  class->get_property = gtk_action_muxer_get_property;
  class->set_property = gtk_action_muxer_set_property;
  class->finalize = gtk_action_muxer_finalize;
  class->dispose = gtk_action_muxer_dispose;

  properties[PROP_PARENT] = g_param_spec_object ("parent", NULL, NULL,
                                                 GTK_TYPE_ACTION_MUXER,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_NAME);

  properties[PROP_WIDGET] = g_param_spec_object ("widget", NULL, NULL,
                                                 GTK_TYPE_WIDGET,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT_ONLY |
                                                 G_PARAM_STATIC_NAME);

  g_object_class_install_properties (class, NUM_PROPERTIES, properties);
}

/*< private >
 * gtk_action_muxer_insert:
 * @muxer: a `GtkActionMuxer`
 * @prefix: the prefix string for the action group
 * @action_group: a `GActionGroup`
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
 * If any `GtkActionObserver`s are registered for actions in the group,
 * “action_added” notifications will be emitted, as appropriate.
 *
 * @prefix must not contain a dot ('.').
 */
void
gtk_action_muxer_insert (GtkActionMuxer *muxer,
                         const char     *prefix,
                         GActionGroup   *action_group)
{
  char **actions;
  Group *group;
  int i;

  /* TODO: diff instead of ripout and replace */
  if (muxer->node != NULL && muxer->groups != NULL)
    {
      Group *old_group = g_hash_table_lookup (muxer->groups, prefix);

      if (old_group != NULL)
        {
          g_hash_table_steal (muxer->groups, prefix);
          gtk_action_muxer_free_group (old_group);
        }
    }
  else
    gtk_action_muxer_remove (muxer, prefix);

  if (!muxer->groups)
    muxer->groups = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gtk_action_muxer_free_group);

  action_muxer_profile.allocations += 2;
  group = g_new0 (Group, 1);
  group->muxer = muxer;
  group->group = g_object_ref (action_group);
  group->prefix = g_strdup (prefix);

  g_hash_table_insert (muxer->groups, group->prefix, group);

  if (muxer->node != NULL)
    {
      gtk_action_node_insert_group (muxer->node, prefix, action_group);
      return;
    }

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
 * @muxer: a `GtkActionMuxer`
 * @prefix: the prefix of the action group to remove
 *
 * Removes a `GActionGroup` from the `GtkActionMuxer`.
 *
 * If any `GtkActionObserver`s are registered for actions in the group,
 * “action_removed” notifications will be emitted, as appropriate.
 */
void
gtk_action_muxer_remove (GtkActionMuxer *muxer,
                         const char     *prefix)
{
  Group *group;

  if (!muxer->groups)
    return;

  group = g_hash_table_lookup (muxer->groups, prefix);

  if (group != NULL)
    {
      char **actions;
      int i;

      g_hash_table_steal (muxer->groups, prefix);

      if (muxer->node != NULL)
        {
          gtk_action_node_remove_group (muxer->node, prefix);
          gtk_action_muxer_free_group (group);
          return;
        }

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
 *
 * Creates a new `GtkActionMuxer`.
 */
GtkActionMuxer *
gtk_action_muxer_new (GtkWidget *widget)
{
  GtkActionMuxer *muxer;

  muxer = g_object_new (GTK_TYPE_ACTION_MUXER,
                        "widget", widget,
                        NULL);
  if (widget != NULL)
    muxer->node = _gtk_widget_get_action_node (widget, TRUE);
  else
    {
      muxer->node = gtk_action_node_new_synthetic (muxer);
      muxer->owns_node = TRUE;
    }

  return muxer;
}

GtkActionNode *
gtk_action_muxer_get_node (GtkActionMuxer *muxer)
{
  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);

  return muxer->node;
}

/*< private >
 * gtk_action_muxer_get_parent:
 * @muxer: a `GtkActionMuxer`
 *
 * Returns: (transfer none): the parent of @muxer, or NULL.
 */
GtkActionMuxer *
gtk_action_muxer_get_parent (GtkActionMuxer *muxer)
{
  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);

  return muxer->parent;
}

/*< private >
 * gtk_action_muxer_set_parent:
 * @muxer: a `GtkActionMuxer`
 * @parent: (nullable): the new parent `GtkActionMuxer`
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
      notify_observers_removed (muxer, muxer->parent);
      g_object_unref (muxer->parent);
    }

  muxer->parent = parent;

  if (muxer->owns_node)
    gtk_action_node_set_synthetic_parent (muxer->node,
                                          parent != NULL ? parent->node : NULL);

  if (muxer->parent != NULL)
    {
      g_object_ref (muxer->parent);
      notify_observers_added (muxer, muxer->parent);
    }

  g_object_notify_by_pspec (G_OBJECT (muxer), properties[PROP_PARENT]);
}

void
gtk_action_muxer_set_primary_accel (GtkActionMuxer *muxer,
                                    const char     *action_and_target,
                                    const char     *primary_accel)
{
  g_autoptr(GtkActionKey) action_key = NULL;
  g_autoptr(GVariant) target = NULL;
  g_autoptr(GError) error = NULL;
  const char *separator;

  g_return_if_fail (GTK_IS_ACTION_MUXER (muxer));
  g_return_if_fail (action_and_target != NULL);

  separator = strrchr (action_and_target, '|');
  if (separator == NULL)
    return;

  if (!(action_key = gtk_action_key_new (separator + 1)))
    return;

  if (separator != action_and_target)
    {
      target = g_variant_parse (NULL,
                                action_and_target,
                                separator,
                                NULL,
                                &error);
      if (target == NULL)
        return;
    }

  gtk_action_muxer_set_primary_accel_for (muxer, action_key, target, primary_accel);
}

void
gtk_action_muxer_set_primary_accel_for (GtkActionMuxer *muxer,
                                        GtkActionKey   *key,
                                        GVariant       *target,
                                        const char     *primary_accel)
{
  g_return_if_fail (GTK_IS_ACTION_MUXER (muxer));
  g_return_if_fail (key != NULL);

  gtk_action_node_set_primary_accel (muxer->node, key, target, primary_accel);
}

const char *
gtk_action_muxer_get_primary_accel (GtkActionMuxer *muxer,
                                    const char     *action_and_target)
{
  g_autoptr(GtkActionKey) action_key = NULL;
  g_autoptr(GVariant) target = NULL;
  g_autoptr(GError) error = NULL;
  const char *separator;

  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);
  g_return_val_if_fail (action_and_target != NULL, NULL);

  separator = strrchr (action_and_target, '|');
  if (separator == NULL)
    return NULL;

  if (!(action_key = gtk_action_key_new (separator + 1)))
    return NULL;

  if (separator != action_and_target)
    {
      target = g_variant_parse (NULL,
                                action_and_target,
                                separator,
                                NULL,
                                &error);
      if (target == NULL)
        return NULL;
    }

  return gtk_action_muxer_get_primary_accel_for (muxer, action_key, target);
}

const char *
gtk_action_muxer_get_primary_accel_for (GtkActionMuxer *muxer,
                                        GtkActionKey   *key,
                                        GVariant       *target)
{
  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return gtk_action_node_get_primary_accel (muxer->node, key, target);
}

char *
gtk_print_action_and_target (const char *action_namespace,
                             const char *action_name,
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

char *
gtk_normalise_detailed_action_name (const char *detailed_action_name)
{
  GError *error = NULL;
  char *action_and_target;
  char *action_name;
  GVariant *target;

  g_action_parse_detailed_name (detailed_action_name, &action_name, &target, &error);
  g_assert_no_error (error);

  action_and_target = gtk_print_action_and_target (NULL, action_name, target);

  if (target)
    g_variant_unref (target);

  g_free (action_name);

  return action_and_target;
}
