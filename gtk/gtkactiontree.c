/* gtkactiontree.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkactiontreeprivate.h"
#include "gtkactionmuxerprofileprivate.h"
#include "gtkapplication.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

#include <string.h>

typedef struct _GtkActionTreeWork GtkActionTreeWork;
typedef struct _GtkActionProviderObserver GtkActionProviderObserver;
typedef struct _GtkActionTargetBucket GtkActionTargetBucket;
typedef struct _GtkActionPropertySource GtkActionPropertySource;

#define TARGET_INDEX_LINEAR_LIMIT 4

struct _GtkActionTargetBucket
{
  GVariant         *target;
  GtkActionBinding *first_binding;
  guint             hash;
};

struct _GtkActionBinding
{
  GtkActionRoute          *route;
  GtkActionProvider       *provider;
  GtkActionBinding        *route_prev;
  GtkActionBinding        *route_next;
  GtkActionBinding        *provider_prev;
  GtkActionBinding        *provider_next;
  GtkActionBinding        *target_prev;
  GtkActionBinding        *target_next;
  GtkActionBinding        *dirty_next;
  GtkActionTargetBucket   *target_bucket;
  GtkActionKey            *key;
  GVariant                *target;
  char                    *primary_accel;
  GtkActionBindingState    state;
  GtkActionBindingCallback callback;
  gpointer                 user_data;
  GDestroyNotify           destroy;
  GtkActionBinding       **owner_location;
  GtkActionInterest        interest;
  guint                    alive : 1;
  guint                    dirty : 1;
  guint                    queued : 1;
};

struct _GtkActionSubscription
{
  GtkActionRoute                *route;
  GtkActionProvider             *provider;
  GtkActionSubscription         *route_prev;
  GtkActionSubscription         *route_next;
  GtkActionSubscription         *provider_prev;
  GtkActionSubscription         *provider_next;
  GtkActionSubscription         *state_prev;
  GtkActionSubscription         *state_next;
  GtkActionKey                  *key;
  GVariant                      *target;
  GtkActionSnapshot              snapshot;
  GtkActionSubscriptionCallback  callback;
  gpointer                       user_data;
  GDestroyNotify                 destroy;
  GtkActionInterest              interest;
  guint                          queued : 1;
  guint                          cancelled : 1;
};

struct _GtkActionProviderObserver
{
  GtkActionProviderObserver *next;
  GtkActionProviderCallback  callback;
  gpointer                   user_data;
  GDestroyNotify             destroy;
  gulong                     id;
  guint                      removed : 1;
};

struct _GtkActionProvider
{
  GtkActionNode             *node;
  GtkActionKey              *key;
  GtkActionSource           *source;
  GtkWidgetAction           *widget_action;
  GtkActionPropertySource   *property_source;
  GtkActionProvider         *property_next;
  GtkActionSnapshot          snapshot;
  GtkActionProviderObserver *observers;
  GtkActionBinding          *first_binding;
  GtkActionBinding          *boolean_bindings;
  GtkActionBinding          *raw_state_bindings;
  GPtrArray                 *target_buckets;
  GHashTable                *target_index;
  GtkActionSubscription     *first_subscription;
  GtkActionSubscription     *first_raw_state_observer;
  guint64                    revision;
  guint64                    state_bindings_touched;
  guint                      dispatch_depth;
  guint                      retired : 1;
};

struct _GtkActionPropertySource
{
  GtkActionNode     *node;
  GParamSpec        *pspec;
  GtkActionProvider *providers;
  gulong             handler_id;
};

struct _GtkActionSource
{
  GtkActionNode     *node;
  GActionGroup      *group;
  char              *prefix;
  GHashTable        *providers;
  gulong             handler_ids[4];
  guint              retired : 1;
};

struct _GtkActionRoute
{
  GtkActionNode  *node;
  GtkActionKey   *key;

  GtkActionRoute *parent;
  GtkActionRoute *first_child;
  GtkActionRoute *last_child;
  GtkActionRoute *prev_sibling;
  GtkActionRoute *next_sibling;

  gpointer        local_provider;
  gpointer        effective_provider;
  GtkActionBinding *first_local_binding;
  GtkActionSubscription *first_subscription;

  guint           n_local_interests;
  guint           n_subtree_interests;
  guint64         revision;
};

struct _GtkActionTreeWork
{
  GtkActionTreeWork *next;
  GtkActionTreeCallback callback;
  gpointer data;
  GDestroyNotify destroy;
};

struct _GtkActionNode
{
  GtkActionTree *tree;
  GtkWidget *widget;
  gpointer owner;

  GtkActionNode *parent;
  GtkActionNode *first_child;
  GtkActionNode *last_child;
  GtkActionNode *prev_sibling;
  GtkActionNode *next_sibling;
  GtkActionNode *next;

  GtkActionNode *synthetic_parent;
  GPtrArray *counted_ancestors;
  GHashTable *routes;
  GHashTable *sources;
  GHashTable *property_sources;
  GHashTable *primary_accels;
  guint local_features;
  guint auto_prunable : 1;
  guint retired : 1;
};

struct _GtkActionTree
{
  GtkActionNode *nodes;
  GtkActionTreeWork *dirty_head;
  GtkActionTreeWork *dirty_tail;
  GtkActionBinding *dirty_binding_head;
  GtkActionBinding *dirty_binding_tail;
  GtkActionTreeWork *retired;
  guint update_depth;
  guint dispatch_depth;
  guint committing : 1;
};

static GtkActionTree *default_tree;

static void               drain_updates             (GtkActionTree         *tree);
static void               free_source               (gpointer               data);
static GtkActionProvider *materialize_provider      (GtkActionRoute        *route);
static GtkActionProvider *materialize_provider_full (GtkActionRoute        *route,
                                                     gboolean               include_widget_action);
static void               queue_binding             (GtkActionBinding      *binding);
static void               queue_subscription        (GtkActionSubscription *subscription);
static void               remove_source             (GtkActionSource       *source);
static void               sync_changed_node_routes  (GtkActionNode         *node,
                                                     GtkActionNode         *old_parent);

static void
target_bucket_free (gpointer data)
{
  GtkActionTargetBucket *bucket = data;

  g_assert_null (bucket->first_binding);
  g_clear_pointer (&bucket->target, g_variant_unref);
  g_free (bucket);
}

static GtkActionTargetBucket *
provider_lookup_target_bucket (GtkActionProvider *provider,
                               GVariant          *target)
{
  guint hash;
  guint i;

  g_assert (provider != NULL);
  g_assert (target != NULL);

  if (provider->target_index != NULL)
    return g_hash_table_lookup (provider->target_index, target);

  if (provider->target_buckets == NULL)
    return NULL;

  hash = g_variant_hash (target);
  for (i = 0; i < provider->target_buckets->len; i++)
    {
      GtkActionTargetBucket *bucket = g_ptr_array_index (provider->target_buckets, i);

      if (bucket->hash == hash &&
          g_variant_equal (bucket->target, target))
        return bucket;
    }

  return NULL;
}

static GtkActionTargetBucket *
provider_ensure_target_bucket (GtkActionProvider *provider,
                               GVariant          *target)
{
  GtkActionTargetBucket *bucket;
  guint i;

  g_assert (provider != NULL);
  g_assert (target != NULL);

  if ((bucket = provider_lookup_target_bucket (provider, target)))
    return bucket;

  if (provider->target_buckets == NULL)
    provider->target_buckets = g_ptr_array_new_with_free_func (target_bucket_free);

  bucket = g_new0 (GtkActionTargetBucket, 1);
  bucket->target = g_variant_ref (target);
  bucket->hash = g_variant_hash (target);
  g_ptr_array_add (provider->target_buckets, bucket);

  if (provider->target_buckets->len > TARGET_INDEX_LINEAR_LIMIT &&
      provider->target_index == NULL)
    {
      provider->target_index = g_hash_table_new (g_variant_hash, g_variant_equal);
      for (i = 0; i < provider->target_buckets->len; i++)
        {
          GtkActionTargetBucket *indexed = g_ptr_array_index (provider->target_buckets, i);

          g_hash_table_insert (provider->target_index, indexed->target, indexed);
        }
    }
  else if (provider->target_index != NULL)
    g_hash_table_insert (provider->target_index, bucket->target, bucket);

  return bucket;
}

static void
binding_unlink_state_index (GtkActionBinding *self)
{
  GtkActionBinding **head = NULL;
  GtkActionProvider *provider = self->provider;
  GtkActionTargetBucket *bucket = self->target_bucket;

  g_assert (self != NULL);

  if (provider == NULL)
    return;

  if (bucket != NULL)
    head = &bucket->first_binding;
  else if ((self->interest & GTK_ACTION_INTEREST_RAW_STATE) != 0)
    head = &provider->raw_state_bindings;
  else if ((self->interest & GTK_ACTION_INTEREST_ACTIVE) != 0 && self->target == NULL)
    head = &provider->boolean_bindings;
  else
    return;

  if (self->target_prev != NULL)
    self->target_prev->target_next = self->target_next;
  else
    *head = self->target_next;
  if (self->target_next != NULL)
    self->target_next->target_prev = self->target_prev;
  self->target_prev = NULL;
  self->target_next = NULL;
  self->target_bucket = NULL;

  if (bucket != NULL && bucket->first_binding == NULL)
    {
      if (provider->target_index != NULL)
        g_hash_table_remove (provider->target_index, bucket->target);
      g_ptr_array_remove (provider->target_buckets, bucket);
    }
}

static void
binding_link_state_index (GtkActionBinding *self)
{
  GtkActionBinding **head;

  g_assert (self != NULL);
  g_assert_null (self->target_prev);
  g_assert_null (self->target_next);
  g_assert_null (self->target_bucket);

  if (self->provider == NULL)
    return;

  if ((self->interest & GTK_ACTION_INTEREST_RAW_STATE) != 0)
    head = &self->provider->raw_state_bindings;
  else if ((self->interest & GTK_ACTION_INTEREST_ACTIVE) == 0)
    return;
  else if (self->target == NULL)
    head = &self->provider->boolean_bindings;
  else
    {
      self->target_bucket = provider_ensure_target_bucket (self->provider, self->target);
      head = &self->target_bucket->first_binding;
    }

  self->target_next = *head;
  if (self->target_next != NULL)
    self->target_next->target_prev = self;
  *head = self;
}

static void
binding_set_provider (GtkActionBinding  *self,
                      GtkActionProvider *provider)
{
  g_assert (self != NULL);

  if (self->provider == provider)
    return;

  binding_unlink_state_index (self);

  if (self->provider_prev != NULL)
    self->provider_prev->provider_next = self->provider_next;
  else if (self->provider != NULL)
    self->provider->first_binding = self->provider_next;
  if (self->provider_next != NULL)
    self->provider_next->provider_prev = self->provider_prev;

  self->provider = provider;
  self->provider_prev = NULL;
  self->provider_next = provider != NULL ? provider->first_binding : NULL;
  if (self->provider_next != NULL)
    self->provider_next->provider_prev = self;
  if (provider != NULL)
    provider->first_binding = self;
  binding_link_state_index (self);
}

static GtkActionChange
interest_changes (GtkActionInterest interest)
{
  GtkActionChange changes = GTK_ACTION_CHANGE_NONE;

  if ((interest & GTK_ACTION_INTEREST_PRESENT) != 0)
    changes |= (GTK_ACTION_CHANGE_PRESENT |
                GTK_ACTION_CHANGE_PROVIDER |
                GTK_ACTION_CHANGE_SIGNATURE);
  if ((interest & GTK_ACTION_INTEREST_ENABLED) != 0)
    changes |= GTK_ACTION_CHANGE_ENABLED;
  if ((interest & (GTK_ACTION_INTEREST_RAW_STATE |
                   GTK_ACTION_INTEREST_ACTIVE)) != 0)
    changes |= (GTK_ACTION_CHANGE_SIGNATURE | GTK_ACTION_CHANGE_STATE);
  if ((interest & GTK_ACTION_INTEREST_ACTIVE) != 0)
    changes |= GTK_ACTION_CHANGE_ACTIVE;
  if ((interest & GTK_ACTION_INTEREST_ROLE) != 0)
    changes |= (GTK_ACTION_CHANGE_SIGNATURE |
                GTK_ACTION_CHANGE_STATE |
                GTK_ACTION_CHANGE_ROLE);
  if ((interest & GTK_ACTION_INTEREST_ACCEL) != 0)
    changes |= GTK_ACTION_CHANGE_ACCEL;

  return changes;
}

static const char *
lookup_primary_accel (GtkActionNode *self,
                      GtkActionKey  *key,
                      GVariant      *target)
{
  GtkActionInvocationKey *invocation;
  const char *accel = NULL;

  g_assert (self != NULL);
  g_assert (key != NULL);

  invocation = gtk_action_invocation_key_new (key, target);

  for (; self != NULL; self = self->parent)
    {
      if (self->primary_accels != NULL &&
          (accel = g_hash_table_lookup (self->primary_accels, invocation)))
        break;
    }

  gtk_action_invocation_key_unref (invocation);

  return accel;
}

static void
snapshot_set_primary_accel (GtkActionSnapshot *snapshot,
                            GtkActionNode     *node,
                            GtkActionKey      *key,
                            GVariant          *target)
{
  const char *primary_accel;

  g_assert (snapshot != NULL);
  g_assert (node != NULL);
  g_assert (key != NULL);

  primary_accel = lookup_primary_accel (node, key, target);
  g_clear_pointer (&snapshot->primary_accel, g_free);
  snapshot->primary_accel = g_strdup (primary_accel);
}

static void
subscription_set_provider (GtkActionSubscription *self,
                           GtkActionProvider     *provider)
{
  gboolean observes_state;

  g_assert (self != NULL);

  if (self->provider == provider)
    return;

  observes_state = (self->interest & (GTK_ACTION_INTEREST_RAW_STATE |
                                      GTK_ACTION_INTEREST_ACTIVE |
                                      GTK_ACTION_INTEREST_ROLE)) != 0;

  if (observes_state)
    {
      if (self->state_prev != NULL)
        self->state_prev->state_next = self->state_next;
      else if (self->provider != NULL)
        self->provider->first_raw_state_observer = self->state_next;
      if (self->state_next != NULL)
        self->state_next->state_prev = self->state_prev;
      self->state_prev = NULL;
      self->state_next = NULL;
    }

  if (self->provider_prev != NULL)
    self->provider_prev->provider_next = self->provider_next;
  else if (self->provider != NULL)
    self->provider->first_subscription = self->provider_next;
  if (self->provider_next != NULL)
    self->provider_next->provider_prev = self->provider_prev;

  self->provider = provider;
  self->provider_prev = NULL;
  self->provider_next = provider != NULL ? provider->first_subscription : NULL;
  if (self->provider_next != NULL)
    self->provider_next->provider_prev = self;
  if (provider != NULL)
    provider->first_subscription = self;

  if (provider != NULL && observes_state)
    {
      self->state_next = provider->first_raw_state_observer;
      if (self->state_next != NULL)
        self->state_next->state_prev = self;
      provider->first_raw_state_observer = self;
    }
}

static void
adjust_subtree_count (GPtrArray *widgets,
                      int        adjustment)
{
  guint i;

  if (widgets == NULL)
    return;

  for (i = 0; i < widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (widgets, i);

      if (adjustment > 0)
        widget->priv->action_subtree_count += adjustment;
      else
        {
          g_assert (widget->priv->action_subtree_count >= (guint) -adjustment);
          widget->priv->action_subtree_count -= -adjustment;
        }
    }
}

static GPtrArray *
collect_ancestors (GtkWidget *widget)
{
  GPtrArray *widgets = g_ptr_array_new ();

  for (; widget != NULL; widget = _gtk_widget_get_parent (widget))
    g_ptr_array_add (widgets, widget);

  return widgets;
}

static gboolean
same_ancestors (GPtrArray *a,
                GPtrArray *b)
{
  guint i;

  if (a->len != b->len)
    return FALSE;

  for (i = 0; i < a->len; i++)
    {
      if (g_ptr_array_index (a, i) != g_ptr_array_index (b, i))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_action_node_unlink (GtkActionNode *self)
{
  if (self->prev_sibling != NULL)
    self->prev_sibling->next_sibling = self->next_sibling;
  else if (self->parent != NULL)
    self->parent->first_child = self->next_sibling;

  if (self->next_sibling != NULL)
    self->next_sibling->prev_sibling = self->prev_sibling;
  else if (self->parent != NULL)
    self->parent->last_child = self->prev_sibling;

  self->parent = NULL;
  self->prev_sibling = NULL;
  self->next_sibling = NULL;
}

static void
gtk_action_node_append (GtkActionNode *parent,
                        GtkActionNode *self)
{
  g_assert (self->parent == NULL);

  self->parent = parent;
  if (parent == NULL)
    return;

  self->prev_sibling = parent->last_child;
  if (parent->last_child != NULL)
    parent->last_child->next_sibling = self;
  else
    parent->first_child = self;
  parent->last_child = self;
}

static gboolean
widget_is_ancestor (GtkWidget *ancestor,
                    GtkWidget *widget)
{
  for (; widget != NULL; widget = _gtk_widget_get_parent (widget))
    {
      if (widget == ancestor)
        return TRUE;
    }

  return FALSE;
}

static GtkActionNode *
find_widget_node (GtkActionTree *tree,
                  GtkWidget     *widget)
{
  GtkActionNode *node;

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      if (!node->retired && node->widget == widget)
        return node;
    }

  return NULL;
}

static GtkActionNode *
find_synthetic_node (GtkActionTree *tree,
                     gpointer       owner)
{
  GtkActionNode *node;

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      if (!node->retired && node->widget == NULL && node->owner == owner)
        return node;
    }

  return NULL;
}

static GtkActionNode *
ensure_window_scope (GtkActionTree *tree,
                     GtkWindow     *window)
{
  GtkApplication *application = gtk_window_get_application (window);
  gpointer owner = application != NULL ? (gpointer)application : (gpointer)window;
  GtkActionNode *scope;

  if (!(scope = find_synthetic_node (tree, owner)))
    {
      scope = gtk_action_tree_add_synthetic (tree, owner);
      scope->auto_prunable = TRUE;
    }

  return scope;
}

static GtkActionNode *
find_compressed_parent (GtkActionNode *self)
{
  GtkWidget *parent;

  if (self->widget == NULL)
    return self->synthetic_parent;

  for (parent = _gtk_widget_get_parent (self->widget);
       parent != NULL;
       parent = _gtk_widget_get_parent (parent))
    {
      GtkActionNode *node = find_widget_node (self->tree, parent);

      if (node != NULL)
        return node;
    }

  return self->synthetic_parent;
}

static void
gtk_action_route_unlink (GtkActionRoute *self)
{
  if (self->prev_sibling != NULL)
    self->prev_sibling->next_sibling = self->next_sibling;
  else if (self->parent != NULL)
    self->parent->first_child = self->next_sibling;

  if (self->next_sibling != NULL)
    self->next_sibling->prev_sibling = self->prev_sibling;
  else if (self->parent != NULL)
    self->parent->last_child = self->prev_sibling;

  self->parent = NULL;
  self->prev_sibling = NULL;
  self->next_sibling = NULL;
}

static void
gtk_action_route_append (GtkActionRoute *parent,
                         GtkActionRoute *self)
{
  g_assert (self->parent == NULL);

  self->parent = parent;
  if (parent == NULL)
    return;

  self->prev_sibling = parent->last_child;
  if (parent->last_child != NULL)
    parent->last_child->next_sibling = self;
  else
    parent->first_child = self;
  parent->last_child = self;
}

static GtkActionRoute *
lookup_route (GtkActionNode *node,
              GtkActionKey  *key)
{
  if (node == NULL || node->routes == NULL)
    return NULL;

  return g_hash_table_lookup (node->routes, key);
}

static gboolean
source_matches_key (GtkActionSource *source,
                    GtkActionKey    *key)
{
  const char *full_name;
  gsize prefix_len;

  g_assert (source != NULL);
  g_assert (key != NULL);

  full_name = gtk_action_key_get_full_name (key);
  prefix_len = gtk_action_key_get_prefix_length (key);

  return (strlen (source->prefix) == prefix_len &&
          memcmp (source->prefix, full_name, prefix_len) == 0);
}

static GtkActionSource *
lookup_source_for_key (GtkActionNode *node,
                       GtkActionKey  *key)
{
  GHashTableIter iter;
  gpointer value;

  if (node == NULL || node->sources == NULL)
    return NULL;

  g_hash_table_iter_init (&iter, node->sources);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GtkActionSource *source = value;

      if (source_matches_key (source, key))
        return source;
    }

  return NULL;
}

static gpointer
inherited_provider (GtkActionRoute *self)
{
  if (self->local_provider != NULL)
    return self->local_provider;
  if (self->parent != NULL)
    return self->parent->effective_provider;

  return NULL;
}

static void
resolve_route (GtkActionRoute *self)
{
  GtkActionBinding *binding;
  GtkActionRoute *child;
  GtkActionSubscription *subscription;
  gpointer old_provider;
  gpointer new_provider;

  old_provider = self->effective_provider;
  new_provider = inherited_provider (self);
  if (old_provider == new_provider)
    return;

  self->effective_provider = new_provider;
  self->revision++;

  for (binding = self->first_local_binding;
       binding != NULL;
       binding = binding->route_next)
    {
      binding_set_provider (binding, new_provider);
      queue_binding (binding);
    }

  for (subscription = self->first_subscription;
       subscription != NULL;
       subscription = subscription->route_next)
    {
      subscription_set_provider (subscription, new_provider);
      queue_subscription (subscription);
    }

  for (child = self->first_child; child != NULL; child = child->next_sibling)
    {
      if (child->local_provider == NULL)
        resolve_route (child);
    }
}

static GtkActionRoute *
ensure_route (GtkActionNode *node,
              GtkActionKey  *key,
              gboolean       include_widget_action)
{
  GtkActionRoute *parent = NULL;
  GtkActionRoute *self;

  if ((self = lookup_route (node, key)))
    return self;

  if (node->parent != NULL)
    parent = ensure_route (node->parent, key, TRUE);

  if (node->routes == NULL)
    node->routes = g_hash_table_new (gtk_action_key_hash, gtk_action_key_equal);

  self = g_new0 (GtkActionRoute, 1);
  self->node = node;
  self->key = gtk_action_key_ref (key);
  self->effective_provider = parent != NULL ? parent->effective_provider : NULL;
  gtk_action_route_append (parent, self);
  g_hash_table_insert (node->routes, self->key, self);
  materialize_provider_full (self, include_widget_action);

  return self;
}

static void
adjust_route_ancestors (GtkActionRoute *self,
                        int             adjustment)
{
  for (; self != NULL; self = self->parent)
    {
      if (adjustment > 0)
        self->n_subtree_interests += adjustment;
      else
        {
          g_assert (self->n_subtree_interests >= (guint) -adjustment);
          self->n_subtree_interests -= -adjustment;
        }
    }
}

typedef struct
{
  GtkActionProvider *provider;
  GtkActionSnapshot  snapshot;
  GtkActionChange    changed;
} ProviderDelivery;

static void
provider_delivery_free (gpointer data)
{
  ProviderDelivery *delivery = data;

  gtk_action_snapshot_clear (&delivery->snapshot);
  g_free (delivery);
}

static void
subscription_free (gpointer data)
{
  GtkActionSubscription *self = data;

  g_assert (self->cancelled);
  g_assert_null (self->route);
  g_assert_null (self->provider);

  if (self->destroy != NULL)
    self->destroy (self->user_data);
  gtk_action_snapshot_clear (&self->snapshot);
  g_clear_pointer (&self->target, g_variant_unref);
  gtk_action_key_unref (self->key);
  g_free (self);
}

static void
binding_free (gpointer data)
{
  GtkActionBinding *self = data;

  g_assert_false (self->alive);
  g_assert_null (self->route);
  g_assert_null (self->provider);

  if (self->destroy != NULL)
    self->destroy (self->user_data);
  g_clear_pointer (&self->primary_accel, g_free);
  g_clear_pointer (&self->target, g_variant_unref);
  gtk_action_key_unref (self->key);
  g_free (self);
}

static GtkActionChange
derive_binding_state (GtkActionBinding      *self,
                      GtkActionBindingState *state)
{
  const GtkActionSnapshot *snapshot;
  GtkActionChange changed = GTK_ACTION_CHANGE_NONE;

  g_assert (self != NULL);
  g_assert (state != NULL);

  memset (state, 0, sizeof *state);
  state->role = GTK_BUTTON_ROLE_NORMAL;

  if (self->provider != NULL)
    {
      snapshot = &self->provider->snapshot;
      state->present = snapshot->present;
      state->activatable = snapshot->present &&
        ((self->target == NULL && snapshot->parameter_type == NULL) ||
         (self->target != NULL && snapshot->parameter_type != NULL &&
          g_variant_is_of_type (self->target, snapshot->parameter_type)));
      state->enabled = state->activatable && snapshot->enabled;
      if (state->activatable && self->target != NULL && snapshot->state != NULL)
        {
          state->active = g_variant_equal (snapshot->state, self->target);
          state->role = GTK_BUTTON_ROLE_RADIO;
        }
      else if (state->activatable && snapshot->state != NULL &&
               g_variant_is_of_type (snapshot->state, G_VARIANT_TYPE_BOOLEAN))
        {
          state->active = g_variant_get_boolean (snapshot->state);
          state->role = GTK_BUTTON_ROLE_CHECK;
        }
    }

  state->primary_accel = lookup_primary_accel (self->route->node,
                                                self->key,
                                                self->target);

  if (self->state.present != state->present ||
      self->state.activatable != state->activatable)
    changed |= (GTK_ACTION_CHANGE_PRESENT | GTK_ACTION_CHANGE_SIGNATURE);
  if (self->state.enabled != state->enabled)
    changed |= GTK_ACTION_CHANGE_ENABLED;
  if (self->state.active != state->active)
    changed |= GTK_ACTION_CHANGE_ACTIVE;
  if (self->state.role != state->role)
    changed |= GTK_ACTION_CHANGE_ROLE;
  if (g_strcmp0 (self->state.primary_accel, state->primary_accel) != 0)
    changed |= GTK_ACTION_CHANGE_ACCEL;

  return changed & interest_changes (self->interest);
}

static void
binding_deliver (GtkActionBinding *self)
{
  GtkActionBindingState state;
  GtkActionChange changed;

  g_assert (self != NULL);

  self->queued = FALSE;
  self->dirty_next = NULL;
  self->dirty = FALSE;
  if (!self->alive)
    return;

  changed = derive_binding_state (self, &state);
  if (changed != GTK_ACTION_CHANGE_NONE)
    {
      if (g_strcmp0 (self->primary_accel, state.primary_accel) != 0)
        {
          g_free (self->primary_accel);
          self->primary_accel = g_strdup (state.primary_accel);
        }
      state.primary_accel = self->primary_accel;
      self->state = state;
      self->callback (self, changed, &self->state, self->user_data);
    }
}

static void
queue_binding (GtkActionBinding *self)
{
  GtkActionTree *tree;

  g_assert (self != NULL);

  self->dirty = TRUE;
  if (!self->alive || self->queued)
    return;

  tree = self->route->node->tree;
  self->queued = TRUE;
  if (tree->dirty_binding_tail != NULL)
    tree->dirty_binding_tail->dirty_next = self;
  else
    tree->dirty_binding_head = self;
  tree->dirty_binding_tail = self;

  if (tree->update_depth == 0 && !tree->committing)
    drain_updates (tree);
}

static void
subscription_deliver (GtkActionTree *tree,
                      gpointer       user_data)
{
  GtkActionSubscription *self = user_data;
  GtkActionSnapshot snapshot = GTK_ACTION_SNAPSHOT_INIT;
  GtkActionChange changed;

  g_assert (self != NULL);

  self->queued = FALSE;
  if (self->cancelled)
    return;

  if (self->provider != NULL)
    gtk_action_snapshot_copy (&snapshot, &self->provider->snapshot);
  else
    snapshot.revision = self->route->revision;
  snapshot_set_primary_accel (&snapshot, self->route->node, self->key, self->target);

  changed = gtk_action_snapshot_difference (&self->snapshot, &snapshot);
  changed &= interest_changes (self->interest);
  if (changed != GTK_ACTION_CHANGE_NONE)
    {
      gtk_action_snapshot_copy (&self->snapshot, &snapshot);
      self->callback (self, changed, &self->snapshot, self->user_data);
    }

  gtk_action_snapshot_clear (&snapshot);
}

static void
queue_subscription (GtkActionSubscription *self)
{
  g_assert (self != NULL);

  if (self->cancelled || self->queued)
    return;

  self->queued = TRUE;
  gtk_action_tree_queue_callback (self->route->node->tree,
                                  subscription_deliver,
                                  self,
                                  NULL);
}

static void
provider_deliver (GtkActionTree *tree,
                  gpointer       user_data)
{
  ProviderDelivery *delivery = user_data;
  GtkActionProvider *provider = delivery->provider;
  GtkActionProviderObserver *observer;
  GtkActionProviderObserver **link;

  g_assert (provider != NULL);
  g_assert (tree == provider->node->tree);

  provider->dispatch_depth++;
  for (observer = provider->observers; observer != NULL; observer = observer->next)
    {
      if (!observer->removed)
        observer->callback (provider, delivery->changed, &delivery->snapshot,
                            observer->user_data);
    }
  provider->dispatch_depth--;

  if (provider->dispatch_depth == 0)
    {
      for (link = &provider->observers; *link != NULL;)
        {
          observer = *link;
          if (!observer->removed)
            link = &observer->next;
          else
            {
              *link = observer->next;
              if (observer->destroy != NULL)
                observer->destroy (observer->user_data);
              g_free (observer);
            }
        }
    }
}

static void
queue_provider_change (GtkActionProvider       *provider,
                       GtkActionChange          changed,
                       const GtkActionSnapshot *snapshot)
{
  ProviderDelivery *delivery;

  g_assert (provider != NULL);
  g_assert (snapshot != NULL);

  if (changed == GTK_ACTION_CHANGE_NONE || provider->observers == NULL)
    return;

  delivery = g_new0 (ProviderDelivery, 1);
  delivery->provider = provider;
  delivery->changed = changed;
  gtk_action_snapshot_init (&delivery->snapshot);
  gtk_action_snapshot_copy (&delivery->snapshot, snapshot);
  gtk_action_tree_queue_callback (provider->node->tree, provider_deliver, delivery,
                                  provider_delivery_free);
}

static void
provider_detach_property_source (GtkActionProvider *self)
{
  if (self->property_source != NULL)
    {
      GtkActionProvider **link;

      for (link = &self->property_source->providers;
           *link != NULL;
           link = &(*link)->property_next)
        {
          if (*link == self)
            {
              *link = self->property_next;
              break;
            }
        }

      if (self->property_source->providers == NULL)
        {
          GtkActionPropertySource *property_source = self->property_source;
          GtkActionNode *node = property_source->node;

          g_signal_handler_disconnect (node->widget, property_source->handler_id);
          g_hash_table_remove (node->property_sources, property_source->pspec);
          g_free (property_source);
          if (g_hash_table_size (node->property_sources) == 0)
            g_clear_pointer (&node->property_sources, g_hash_table_unref);
        }

      self->property_source = NULL;
      self->property_next = NULL;
    }
}

static void
free_provider (gpointer data)
{
  GtkActionProvider *self = data;
  GtkActionProviderObserver *observer;

  g_assert_null (self->first_binding);
  g_assert_null (self->first_subscription);
  g_assert_null (self->first_raw_state_observer);
  g_assert_null (self->boolean_bindings);
  g_assert_null (self->raw_state_bindings);
  g_assert_null (self->property_source);

  while ((observer = self->observers) != NULL)
    {
      self->observers = observer->next;
      if (observer->destroy != NULL)
        observer->destroy (observer->user_data);
      g_free (observer);
    }

  gtk_action_snapshot_clear (&self->snapshot);
  g_clear_pointer (&self->target_index, g_hash_table_unref);
  g_clear_pointer (&self->target_buckets, g_ptr_array_unref);
  gtk_action_key_unref (self->key);
  g_free (self);
}

static gboolean
query_widget_snapshot (GtkActionNode     *node,
                       GtkWidgetAction   *action,
                       GtkActionSnapshot *snapshot)
{
  gboolean enabled;

  g_assert (node != NULL);
  g_assert (node->widget != NULL);
  g_assert (action != NULL);
  g_assert (snapshot != NULL);

  gtk_widget_action_query (node->widget, action, &enabled,
                           &snapshot->state_hint, &snapshot->state);
  snapshot->present = TRUE;
  snapshot->enabled = enabled;
  snapshot->parameter_type = action->parameter_type != NULL
                           ? g_variant_type_copy (action->parameter_type) : NULL;
  snapshot->state_type = action->state_type != NULL
                       ? g_variant_type_copy (action->state_type) : NULL;

  return TRUE;
}

static gboolean
query_source_snapshot (GtkActionSource   *source,
                       GtkActionKey      *key,
                       GtkActionSnapshot *snapshot)
{
  const GVariantType *parameter_type = NULL;
  const GVariantType *state_type = NULL;
  GVariant *state_hint = NULL;
  GVariant *state = NULL;
  gboolean enabled = FALSE;
  gboolean present;

  g_assert (source != NULL);
  g_assert (key != NULL);
  g_assert (snapshot != NULL);

  present = g_action_group_query_action (source->group,
                                         gtk_action_key_get_local_name (key),
                                         &enabled,
                                         &parameter_type,
                                         &state_type,
                                         &state_hint,
                                         &state);
  snapshot->present = present;
  snapshot->enabled = present && enabled;
  snapshot->parameter_type = parameter_type != NULL ? g_variant_type_copy (parameter_type) : NULL;
  snapshot->state_type = state_type != NULL ? g_variant_type_copy (state_type) : NULL;
  snapshot->state_hint = state_hint;
  snapshot->state = state;

  return present;
}

static void
queue_state_binding_list (GtkActionProvider *provider,
                          GtkActionBinding  *binding)
{
  g_assert (provider != NULL);

  for (; binding != NULL; binding = binding->target_next)
    {
      provider->state_bindings_touched++;
      _gtk_action_muxer_profile_binding_touched ();
      queue_binding (binding);
    }
}

static void
queue_state_bindings (GtkActionProvider       *provider,
                      const GtkActionSnapshot *old_snapshot,
                      const GtkActionSnapshot *new_snapshot)
{
  GtkActionTargetBucket *old_bucket = NULL;
  GtkActionTargetBucket *new_bucket = NULL;

  g_assert (provider != NULL);
  g_assert (old_snapshot != NULL);
  g_assert (new_snapshot != NULL);

  if (old_snapshot->state != NULL)
    old_bucket = provider_lookup_target_bucket (provider, old_snapshot->state);
  if (new_snapshot->state != NULL)
    new_bucket = provider_lookup_target_bucket (provider, new_snapshot->state);

  if (old_bucket != NULL)
    queue_state_binding_list (provider, old_bucket->first_binding);
  if (new_bucket != NULL && new_bucket != old_bucket)
    queue_state_binding_list (provider, new_bucket->first_binding);
  queue_state_binding_list (provider, provider->boolean_bindings);
  queue_state_binding_list (provider, provider->raw_state_bindings);
}

static void
update_provider_snapshot (GtkActionProvider       *provider,
                          const GtkActionSnapshot *snapshot)
{
  GtkActionBinding *binding;
  GtkActionSubscription *subscription;
  GtkActionChange changed;

  g_assert (provider != NULL);
  g_assert (snapshot != NULL);

  changed = gtk_action_snapshot_difference (&provider->snapshot, snapshot);
  if (changed != GTK_ACTION_CHANGE_NONE)
    {
      gboolean state_only;

      state_only = (changed & ~GTK_ACTION_CHANGE_STATE) == 0;
      if (state_only)
        queue_state_bindings (provider, &provider->snapshot, snapshot);
      provider->revision++;
      gtk_action_snapshot_copy (&provider->snapshot, snapshot);
      provider->snapshot.provider = provider;
      provider->snapshot.revision = provider->revision;
      queue_provider_change (provider, changed, &provider->snapshot);
      if (!state_only)
        {
          for (binding = provider->first_binding;
               binding != NULL;
               binding = binding->provider_next)
            queue_binding (binding);
        }
      if (state_only)
        {
          for (subscription = provider->first_raw_state_observer;
               subscription != NULL;
               subscription = subscription->state_next)
            queue_subscription (subscription);
        }
      else
        {
          for (subscription = provider->first_subscription;
               subscription != NULL;
               subscription = subscription->provider_next)
            queue_subscription (subscription);
        }
    }
}

static void
refresh_provider (GtkActionProvider *provider)
{
  GtkActionSnapshot snapshot = GTK_ACTION_SNAPSHOT_INIT;

  g_assert (provider != NULL);

  if (provider->widget_action != NULL)
    query_widget_snapshot (provider->node, provider->widget_action, &snapshot);
  else
    query_source_snapshot (provider->source, provider->key, &snapshot);
  snapshot.provider = provider;
  update_provider_snapshot (provider, &snapshot);
  gtk_action_snapshot_clear (&snapshot);
}

static void
property_source_notify (GtkWidget               *widget,
                        GParamSpec              *pspec,
                        GtkActionPropertySource *source)
{
  GtkActionSnapshot snapshot = GTK_ACTION_SNAPSHOT_INIT;
  GtkActionProvider *provider;
  GVariant *state = NULL;

  g_assert (source != NULL);
  g_assert (source->node->widget == widget);
  g_assert (source->pspec == pspec);

  if (source->providers != NULL)
    gtk_widget_action_query (widget, source->providers->widget_action,
                             NULL, NULL, &state);

  gtk_action_tree_begin_update (source->node->tree);
  for (provider = source->providers;
       provider != NULL;
       provider = provider->property_next)
    {
      gtk_action_snapshot_copy (&snapshot, &provider->snapshot);
      g_clear_pointer (&snapshot.state, g_variant_unref);
      snapshot.state = state != NULL ? g_variant_ref (state) : NULL;
      update_provider_snapshot (provider, &snapshot);
      gtk_action_snapshot_clear (&snapshot);
    }
  gtk_action_tree_end_update (source->node->tree);
  g_clear_pointer (&state, g_variant_unref);
}

static void
provider_attach_property_source (GtkActionProvider *provider)
{
  GtkActionNode *node = provider->node;
  GtkActionPropertySource *source;

  g_assert (provider->widget_action != NULL);
  g_assert (provider->widget_action->pspec != NULL);

  if (node->property_sources == NULL)
    node->property_sources = g_hash_table_new (g_direct_hash, g_direct_equal);

  source = g_hash_table_lookup (node->property_sources, provider->widget_action->pspec);
  if (source == NULL)
    {
      source = g_new0 (GtkActionPropertySource, 1);
      source->node = node;
      source->pspec = provider->widget_action->pspec;
      source->handler_id =
        g_signal_connect_closure_by_id (node->widget,
                                        g_signal_lookup ("notify", G_TYPE_OBJECT),
                                        g_param_spec_get_name_quark (source->pspec),
                                        g_cclosure_new (G_CALLBACK (property_source_notify),
                                                        source,
                                                        NULL),
                                        FALSE);
      g_hash_table_insert (node->property_sources, source->pspec, source);
    }

  provider->property_source = source;
  provider->property_next = source->providers;
  source->providers = provider;
}

static GtkActionProvider *
materialize_provider_full (GtkActionRoute *route,
                           gboolean        include_widget_action)
{
  GtkActionSnapshot snapshot = GTK_ACTION_SNAPSHOT_INIT;
  GtkWidgetAction *widget_action = NULL;
  GtkActionSource *source;
  GtkActionProvider *provider;

  g_assert (route != NULL);

  if (route->local_provider != NULL)
    return route->local_provider;
  if (include_widget_action && route->node->widget != NULL)
    widget_action = gtk_widget_class_lookup_action (GTK_WIDGET_GET_CLASS (route->node->widget),
                                                    gtk_action_key_get_full_name (route->key));
  source = widget_action == NULL ? lookup_source_for_key (route->node, route->key) : NULL;
  if (widget_action == NULL && source == NULL)
    return NULL;
  if (widget_action != NULL)
    query_widget_snapshot (route->node, widget_action, &snapshot);
  else if (!query_source_snapshot (source, route->key, &snapshot))
    {
      gtk_action_snapshot_clear (&snapshot);
      return NULL;
    }

  provider = g_new0 (GtkActionProvider, 1);
  provider->node = route->node;
  provider->key = gtk_action_key_ref (route->key);
  provider->source = source;
  provider->widget_action = widget_action;
  gtk_action_snapshot_init (&provider->snapshot);
  gtk_action_snapshot_copy (&provider->snapshot, &snapshot);
  provider->revision = 1;
  provider->snapshot.provider = provider;
  provider->snapshot.revision = provider->revision;
  if (source != NULL)
    g_hash_table_insert (source->providers, provider->key, provider);
  if (widget_action != NULL && widget_action->pspec != NULL)
    provider_attach_property_source (provider);
  gtk_action_route_set_local_provider (route, provider);
  gtk_action_snapshot_clear (&snapshot);

  return provider;
}

static GtkActionProvider *
materialize_provider (GtkActionRoute *route)
{
  return materialize_provider_full (route, TRUE);
}

static void
free_route (GtkActionRoute *self)
{
  g_assert_null (self->first_local_binding);
  g_assert_null (self->first_subscription);

  gtk_action_key_unref (self->key);
  g_free (self);
}

static void
prune_route (GtkActionRoute *self)
{
  while (self != NULL && self->n_subtree_interests == 0)
    {
      GtkActionRoute *parent = self->parent;

      g_assert (self->first_child == NULL);
      gtk_action_route_unlink (self);
      if (self->local_provider != NULL)
        {
          GtkActionProvider *provider = self->local_provider;

          gtk_action_route_set_local_provider (self, NULL);
          if (provider->source != NULL)
            g_hash_table_steal (provider->source->providers, provider->key);
          provider_detach_property_source (provider);
          provider->retired = TRUE;
          gtk_action_tree_retire (self->node->tree, provider, free_provider);
        }
      g_hash_table_remove (self->node->routes, self->key);
      if (g_hash_table_size (self->node->routes) == 0)
        g_clear_pointer (&self->node->routes, g_hash_table_unref);
      free_route (self);
      self = parent;
    }
}

static void
sync_changed_node_routes (GtkActionNode *node,
                          GtkActionNode *old_parent)
{
  GHashTableIter iter;
  gpointer value;

  if (node->routes == NULL || old_parent == node->parent)
    return;

  g_hash_table_iter_init (&iter, node->routes);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GtkActionRoute *self = value;
      GtkActionRoute *new_parent = NULL;
      GtkActionRoute *old_route_parent;
      guint count = self->n_subtree_interests;

      if (node->parent != NULL)
        new_parent = ensure_route (node->parent, self->key, TRUE);
      if (self->parent == new_parent)
        continue;

      old_route_parent = self->parent;
      adjust_route_ancestors (self->parent, -(int)count);
      gtk_action_route_unlink (self);
      gtk_action_route_append (new_parent, self);
      adjust_route_ancestors (new_parent, count);

      if (node->parent == NULL &&
          self->local_provider != NULL &&
          ((GtkActionProvider *) self->local_provider)->widget_action != NULL &&
          self->n_local_interests > 0)
        {
          GtkActionProvider *provider = self->local_provider;

          gtk_action_route_set_local_provider (self, NULL);
          provider_detach_property_source (provider);
          provider->retired = TRUE;
          gtk_action_tree_retire (node->tree, provider, free_provider);
        }
      else if (node->parent != NULL && self->local_provider == NULL)
        {
          materialize_provider (self);
        }

      resolve_route (self);
      prune_route (old_route_parent);
    }
}

static void
queue_all_accel_consumers (GtkActionTree *tree)
{
  GtkActionNode *node;

  g_assert (tree != NULL);

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      GHashTableIter iter;
      gpointer value;

      if (node->retired || node->routes == NULL)
        continue;

      g_hash_table_iter_init (&iter, node->routes);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GtkActionRoute *route = value;
          GtkActionBinding *binding;
          GtkActionSubscription *subscription;

          for (binding = route->first_local_binding;
               binding != NULL;
               binding = binding->route_next)
            if ((binding->interest & GTK_ACTION_INTEREST_ACCEL) != 0)
              queue_binding (binding);

          for (subscription = route->first_subscription;
               subscription != NULL;
               subscription = subscription->route_next)
            if ((subscription->interest & GTK_ACTION_INTEREST_ACCEL) != 0)
              queue_subscription (subscription);
        }
    }
}

static void
rebuild_links (GtkActionTree *tree)
{
  GtkActionNode *node;
  GHashTable *old_parents;

  old_parents = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      g_hash_table_insert (old_parents, node, node->parent);
      node->parent = NULL;
      node->first_child = NULL;
      node->last_child = NULL;
      node->prev_sibling = NULL;
      node->next_sibling = NULL;
    }

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      if (!node->retired)
        gtk_action_node_append (find_compressed_parent (node), node);
    }

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      if (!node->retired)
        sync_changed_node_routes (node, g_hash_table_lookup (old_parents, node));
    }

  queue_all_accel_consumers (tree);

  g_hash_table_unref (old_parents);
}

static void
maybe_prune_synthetic (GtkActionNode *node)
{
  GtkActionNode *other;

  if (node == NULL || node->retired || !node->auto_prunable ||
      node->widget != NULL || node->first_child != NULL)
    return;

  for (other = node->tree->nodes; other != NULL; other = other->next)
    {
      if (!other->retired && other->synthetic_parent == node)
        return;
    }

  gtk_action_node_remove (node);
}

static void
reclaim_retired (GtkActionTree *tree)
{
  GtkActionTreeWork *work;

  g_assert (tree->dispatch_depth == 0);

  while ((work = tree->retired) != NULL)
    {
      tree->retired = work->next;
      if (work->destroy != NULL)
        work->destroy (work->data);
      g_free (work);
    }
}

static void
drain_updates (GtkActionTree *tree)
{
  g_assert (tree->update_depth == 0);

  if (tree->committing)
    return;

  tree->committing = TRUE;
  while (tree->dirty_head != NULL || tree->dirty_binding_head != NULL)
    {
      if (tree->dirty_head == NULL)
        {
          GtkActionBinding *binding = tree->dirty_binding_head;

          tree->dirty_binding_head = binding->dirty_next;
          if (tree->dirty_binding_head == NULL)
            tree->dirty_binding_tail = NULL;

          tree->dispatch_depth++;
          g_assert (tree->update_depth == 0);
          binding_deliver (binding);
          tree->dispatch_depth--;
        }
      else
        {
          GtkActionTreeWork *work = tree->dirty_head;

          tree->dirty_head = work->next;
          if (tree->dirty_head == NULL)
            tree->dirty_tail = NULL;

          tree->dispatch_depth++;
          g_assert (tree->update_depth == 0);
          work->callback (tree, work->data);
          tree->dispatch_depth--;

          if (work->destroy != NULL)
            work->destroy (work->data);
          g_free (work);
        }
    }
  tree->committing = FALSE;

  if (tree->dispatch_depth == 0)
    reclaim_retired (tree);
}

GtkActionTree *
gtk_action_tree_new (void)
{
  return g_new0 (GtkActionTree, 1);
}

void
gtk_action_tree_free (GtkActionTree *tree)
{
  GtkActionNode *node;

  g_return_if_fail (tree != NULL);
  g_return_if_fail (tree->update_depth == 0);
  g_return_if_fail (tree->dispatch_depth == 0);

  drain_updates (tree);
  while ((node = tree->nodes) != NULL)
    gtk_action_node_remove (node);
  reclaim_retired (tree);
  g_free (tree);
}

void
gtk_action_tree_begin_update (GtkActionTree *tree)
{
  g_return_if_fail (tree != NULL);

  tree->update_depth++;
}

void
gtk_action_tree_end_update (GtkActionTree *tree)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (tree->update_depth > 0);

  tree->update_depth--;
  if (tree->update_depth == 0)
    drain_updates (tree);
}

void
gtk_action_tree_queue_callback (GtkActionTree         *tree,
                                GtkActionTreeCallback  callback,
                                gpointer               user_data,
                                GDestroyNotify         destroy)
{
  GtkActionTreeWork *work;

  g_return_if_fail (tree != NULL);
  g_return_if_fail (callback != NULL);

  work = g_new0 (GtkActionTreeWork, 1);
  work->callback = callback;
  work->data = user_data;
  work->destroy = destroy;

  if (tree->dirty_tail != NULL)
    tree->dirty_tail->next = work;
  else
    tree->dirty_head = work;
  tree->dirty_tail = work;

  if (tree->update_depth == 0 && !tree->committing)
    drain_updates (tree);
}

void
gtk_action_tree_retire (GtkActionTree *tree,
                        gpointer       data,
                        GDestroyNotify destroy)
{
  GtkActionTreeWork *work;

  g_return_if_fail (tree != NULL);

  if (tree->update_depth == 0 && tree->dispatch_depth == 0 && !tree->committing)
    {
      if (destroy != NULL)
        destroy (data);
      return;
    }

  work = g_new0 (GtkActionTreeWork, 1);
  work->data = data;
  work->destroy = destroy;
  work->next = tree->retired;
  tree->retired = work;
}

gboolean
gtk_action_tree_check_invariants (GtkActionTree *tree)
{
  GtkActionNode *node;

  g_return_val_if_fail (tree != NULL, FALSE);

  for (node = tree->nodes; node != NULL; node = node->next)
    {
      GtkActionNode *child;
      GtkActionNode *previous = NULL;
      GHashTableIter iter;
      gpointer route_value;

      if (node->retired)
        continue;
      if (node->tree != tree || node->parent != find_compressed_parent (node))
        return FALSE;
      if ((node->first_child == NULL) != (node->last_child == NULL))
        return FALSE;

      for (child = node->first_child; child != NULL; child = child->next_sibling)
        {
          if (child->parent != node || child->prev_sibling != previous)
            return FALSE;
          if (node->widget != NULL && child->widget != NULL &&
              !widget_is_ancestor (node->widget, child->widget))
            return FALSE;
          previous = child;
        }
      if (previous != node->last_child)
        return FALSE;

      if (node->property_sources != NULL)
        {
          GHashTableIter property_iter;
          gpointer property_key;
          gpointer property_value;

          g_hash_table_iter_init (&property_iter, node->property_sources);
          while (g_hash_table_iter_next (&property_iter, &property_key, &property_value))
            {
              GtkActionPropertySource *property_source = property_value;
              GtkActionProvider *provider;

              if (property_source->node != node ||
                  property_source->pspec != property_key ||
                  property_source->handler_id == 0 ||
                  property_source->providers == NULL)
                return FALSE;

              for (provider = property_source->providers;
                   provider != NULL;
                   provider = provider->property_next)
                {
                  GtkActionRoute *route = lookup_route (node, provider->key);

                  if (provider->retired || provider->node != node ||
                      provider->source != NULL ||
                      provider->property_source != property_source ||
                      provider->widget_action == NULL ||
                      provider->widget_action->pspec != property_source->pspec ||
                      route == NULL || route->local_provider != provider)
                    return FALSE;
                }
            }
        }

      if (node->sources != NULL)
        {
          GHashTableIter source_iter;
          gpointer source_key;
          gpointer source_value;

          g_hash_table_iter_init (&source_iter, node->sources);
          while (g_hash_table_iter_next (&source_iter, &source_key, &source_value))
            {
              GtkActionSource *source = source_value;
              GHashTableIter provider_iter;
              gpointer provider_value;

              if (source->retired || source->node != node || source_key != source->prefix ||
                  g_hash_table_lookup (node->sources, source->prefix) != source ||
                  source->group == NULL || source->providers == NULL)
                return FALSE;

              g_hash_table_iter_init (&provider_iter, source->providers);
              while (g_hash_table_iter_next (&provider_iter, NULL, &provider_value))
                {
                  GtkActionProvider *provider = provider_value;
                  GtkActionRoute *provider_route = lookup_route (node, provider->key);
                  GtkActionBinding *binding;
                  GtkActionBinding *previous_binding = NULL;
                  GtkActionSubscription *subscription;
                  GtkActionSubscription *previous_subscription = NULL;
                  guint expected_indexed_bindings = 0;
                  guint indexed_bindings = 0;
                  guint i;

                  if (provider->retired || provider->node != node || provider->source != source ||
                      provider_route == NULL || provider_route->local_provider != provider ||
                      provider->snapshot.provider != provider ||
                      provider->snapshot.revision != provider->revision ||
                      !provider->snapshot.present || !source_matches_key (source, provider->key))
                    return FALSE;

                  for (binding = provider->first_binding;
                       binding != NULL;
                       binding = binding->provider_next)
                    {
                      if (!binding->alive || binding->provider != provider ||
                          binding->provider_prev != previous_binding)
                        return FALSE;
                      if ((binding->interest & (GTK_ACTION_INTEREST_ACTIVE |
                                                GTK_ACTION_INTEREST_RAW_STATE)) != 0)
                        expected_indexed_bindings++;
                      previous_binding = binding;
                    }

                  if (provider->target_buckets != NULL)
                    {
                      for (i = 0; i < provider->target_buckets->len; i++)
                        {
                          GtkActionTargetBucket *bucket =
                            g_ptr_array_index (provider->target_buckets, i);
                          GtkActionBinding *target_previous = NULL;

                          if (bucket->first_binding == NULL ||
                              bucket->hash != g_variant_hash (bucket->target) ||
                              (provider->target_index != NULL &&
                               g_hash_table_lookup (provider->target_index, bucket->target) != bucket))
                            return FALSE;

                          for (binding = bucket->first_binding;
                               binding != NULL;
                               binding = binding->target_next)
                            {
                              if (binding->provider != provider ||
                                  binding->target_bucket != bucket ||
                                  binding->target_prev != target_previous ||
                                  !g_variant_equal (binding->target, bucket->target))
                                return FALSE;
                              target_previous = binding;
                              indexed_bindings++;
                            }
                        }

                      if (provider->target_index != NULL &&
                          g_hash_table_size (provider->target_index) != provider->target_buckets->len)
                        return FALSE;
                    }

                  for (binding = provider->boolean_bindings;
                       binding != NULL;
                       binding = binding->target_next)
                    {
                      if (binding->provider != provider || binding->target != NULL ||
                          binding->target_bucket != NULL)
                        return FALSE;
                      indexed_bindings++;
                    }

                  for (binding = provider->raw_state_bindings;
                       binding != NULL;
                       binding = binding->target_next)
                    {
                      if (binding->provider != provider || binding->target_bucket != NULL)
                        return FALSE;
                      indexed_bindings++;
                    }

                  if (indexed_bindings != expected_indexed_bindings)
                    return FALSE;

                  for (subscription = provider->first_subscription;
                       subscription != NULL;
                       subscription = subscription->provider_next)
                    {
                      if (subscription->cancelled || subscription->provider != provider ||
                          subscription->provider_prev != previous_subscription)
                        return FALSE;
                      previous_subscription = subscription;
                    }
                }
            }
        }

      if (node->routes == NULL)
        continue;

      g_hash_table_iter_init (&iter, node->routes);
      while (g_hash_table_iter_next (&iter, NULL, &route_value))
        {
          GtkActionRoute *route = route_value;
          GtkActionRoute *route_child;
          GtkActionRoute *route_previous = NULL;
          GtkActionRoute *expected_parent;
          GtkActionBinding *binding;
          GtkActionBinding *previous_binding = NULL;
          GtkActionSubscription *subscription;
          GtkActionSubscription *previous_subscription = NULL;
          guint expected_count = route->n_local_interests;
          guint n_subscriptions = 0;
          guint n_bindings = 0;

          expected_parent = node->parent != NULL ? lookup_route (node->parent, route->key) : NULL;
          if (route->node != node || route->parent != expected_parent)
            return FALSE;
          if (g_hash_table_lookup (node->routes, route->key) != route)
            return FALSE;
          if ((route->first_child == NULL) != (route->last_child == NULL))
            return FALSE;
          if (route->effective_provider != inherited_provider (route))
            return FALSE;

          for (binding = route->first_local_binding;
               binding != NULL;
               binding = binding->route_next)
            {
              if (!binding->alive || binding->route != route ||
                  binding->provider != route->effective_provider ||
                  binding->route_prev != previous_binding)
                return FALSE;
              previous_binding = binding;
              n_bindings++;
            }

          for (subscription = route->first_subscription;
               subscription != NULL;
               subscription = subscription->route_next)
            {
              if (subscription->cancelled || subscription->route != route ||
                  subscription->provider != route->effective_provider ||
                  subscription->route_prev != previous_subscription)
                return FALSE;
              previous_subscription = subscription;
              n_subscriptions++;
            }
          if (n_bindings + n_subscriptions > route->n_local_interests)
            return FALSE;

          for (route_child = route->first_child;
               route_child != NULL;
               route_child = route_child->next_sibling)
            {
              if (route_child->parent != route ||
                  route_child->prev_sibling != route_previous ||
                  route_child->key != route->key)
                return FALSE;
              expected_count += route_child->n_subtree_interests;
              route_previous = route_child;
            }
          if (route_previous != route->last_child ||
              expected_count != route->n_subtree_interests ||
              route->n_subtree_interests == 0)
            return FALSE;
        }
    }

  if (tree->dirty_head == NULL && tree->dirty_tail != NULL)
    return FALSE;
  if (tree->dirty_binding_head == NULL && tree->dirty_binding_tail != NULL)
    return FALSE;
  if (tree->dispatch_depth > 0 && tree->update_depth > 0)
    return FALSE;

  return TRUE;
}

guint
gtk_action_tree_get_update_depth (GtkActionTree *tree)
{
  g_return_val_if_fail (tree != NULL, 0);
  return tree->update_depth;
}

guint
gtk_action_tree_get_dispatch_depth (GtkActionTree *tree)
{
  g_return_val_if_fail (tree != NULL, 0);
  return tree->dispatch_depth;
}

guint
gtk_action_tree_get_widget_subtree (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  return widget->priv->action_subtree_count;
}

GtkActionNode *
gtk_action_tree_add_widget (GtkActionTree *tree,
                            GtkWidget     *widget)
{
  GtkActionNode *self;

  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (find_widget_node (tree, widget) == NULL, NULL);

  self = g_new0 (GtkActionNode, 1);
  self->tree = tree;
  self->widget = widget;
  self->owner = widget;
  self->next = tree->nodes;
  tree->nodes = self;
  self->counted_ancestors = collect_ancestors (widget);
  adjust_subtree_count (self->counted_ancestors, 1);
  rebuild_links (tree);

  return self;
}

GtkActionNode *
gtk_action_tree_add_synthetic (GtkActionTree *tree,
                               gpointer       owner)
{
  GtkActionNode *self;

  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (owner != NULL, NULL);

  self = g_new0 (GtkActionNode, 1);
  self->tree = tree;
  self->owner = owner;
  self->next = tree->nodes;
  tree->nodes = self;
  rebuild_links (tree);

  return self;
}

GtkActionNode *
gtk_action_node_new_synthetic (gpointer owner)
{
  if (default_tree == NULL)
    default_tree = gtk_action_tree_new ();

  return gtk_action_tree_add_synthetic (default_tree, owner);
}

void
gtk_action_node_set_synthetic_parent (GtkActionNode *self,
                                      GtkActionNode *parent)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (parent == NULL || self->tree == parent->tree);
  g_return_if_fail (self != parent);

  self->synthetic_parent = parent;
  rebuild_links (self->tree);
}

void
gtk_action_node_sync_parent (GtkActionNode *self)
{
  GPtrArray *ancestors;
  GtkActionNode *old_synthetic_parent;

  g_return_if_fail (self != NULL);
  g_return_if_fail (!self->retired);

  old_synthetic_parent = self->synthetic_parent;
  if (self->widget != NULL)
    {
      if (GTK_IS_WINDOW (self->widget))
        self->synthetic_parent = ensure_window_scope (self->tree, GTK_WINDOW (self->widget));

      ancestors = collect_ancestors (self->widget);
      if (!same_ancestors (self->counted_ancestors, ancestors))
        {
          adjust_subtree_count (self->counted_ancestors, -1);
          adjust_subtree_count (ancestors, 1);
          g_clear_pointer (&self->counted_ancestors, g_ptr_array_unref);
          self->counted_ancestors = g_steal_pointer (&ancestors);
        }
      g_clear_pointer (&ancestors, g_ptr_array_unref);
    }
  rebuild_links (self->tree);
  if (old_synthetic_parent != self->synthetic_parent)
    maybe_prune_synthetic (old_synthetic_parent);
}

static void
free_node (gpointer data)
{
  GtkActionNode *self = data;
  GHashTableIter iter;
  gpointer value;

  g_clear_pointer (&self->counted_ancestors, g_ptr_array_unref);
  g_assert_null (self->sources);
  g_assert_null (self->property_sources);
  g_clear_pointer (&self->primary_accels, g_hash_table_unref);
  if (self->routes != NULL)
    {
      g_hash_table_iter_init (&iter, self->routes);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        free_route (value);
      g_clear_pointer (&self->routes, g_hash_table_unref);
    }
  g_free (self);
}

void
gtk_action_node_remove (GtkActionNode *self)
{
  GtkActionNode *node;
  GtkActionNode **link;
  GtkActionNode *synthetic_parent;
  GtkActionTree *tree;
  GHashTableIter iter;
  gpointer value;

  g_return_if_fail (self != NULL);
  g_return_if_fail (!self->retired);

  tree = self->tree;
  gtk_action_tree_begin_update (tree);
  synthetic_parent = self->synthetic_parent;
  while (self->routes != NULL)
    {
      GtkActionBinding *binding = NULL;
      GtkActionSubscription *subscription = NULL;

      g_hash_table_iter_init (&iter, self->routes);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GtkActionRoute *route = value;

          if (route->first_local_binding != NULL)
            {
              binding = route->first_local_binding;
              break;
            }
          else if (route->first_subscription != NULL)
            {
              subscription = route->first_subscription;
              break;
            }
        }

      if (binding == NULL && subscription == NULL)
        break;
      if (binding != NULL)
        gtk_action_binding_cancel (binding);
      else
        gtk_action_subscription_cancel (subscription);
    }
  if (self->sources != NULL)
    {
      g_hash_table_iter_init (&iter, self->sources);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          g_hash_table_iter_steal (&iter);
          remove_source (value);
        }
      g_clear_pointer (&self->sources, g_hash_table_unref);
    }
  if (self->routes != NULL)
    {
      g_hash_table_iter_init (&iter, self->routes);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GtkActionRoute *route = value;

          if (route->n_local_interests > 0)
            {
              adjust_route_ancestors (route, -(int)route->n_local_interests);
              route->n_local_interests = 0;
            }
        }
    }
  self->retired = TRUE;
  for (node = tree->nodes; node != NULL; node = node->next)
    {
      if (node->synthetic_parent == self)
        node->synthetic_parent = NULL;
    }
  if (self->widget != NULL)
    {
      adjust_subtree_count (self->counted_ancestors, -1);
      g_clear_pointer (&self->counted_ancestors, g_ptr_array_unref);
    }
  gtk_action_node_unlink (self);
  for (link = &tree->nodes; *link != NULL; link = &(*link)->next)
    {
      if (*link == self)
        {
          *link = self->next;
          break;
        }
    }
  rebuild_links (tree);

  if (self->routes != NULL)
    {
      g_hash_table_iter_init (&iter, self->routes);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GtkActionRoute *route = value;

          g_assert_cmpuint (route->n_subtree_interests, ==, 0);
          g_assert_null (route->first_child);
          gtk_action_route_unlink (route);
        }
    }
  gtk_action_tree_retire (tree, self, free_node);
  maybe_prune_synthetic (synthetic_parent);
  gtk_action_tree_end_update (tree);
}

GtkWidget *
gtk_action_node_get_widget (GtkActionNode *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->widget;
}

gpointer
gtk_action_node_get_owner (GtkActionNode *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->owner;
}

GtkActionNode *
gtk_action_node_get_parent (GtkActionNode *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->parent;
}

GtkActionNode *
gtk_action_node_get_first_child (GtkActionNode *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->first_child;
}

GtkActionNode *
gtk_action_node_get_next_sibling (GtkActionNode *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->next_sibling;
}

GtkActionRoute *
gtk_action_node_lookup_route (GtkActionNode *self,
                              GtkActionKey  *key)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return lookup_route (self, key);
}

GtkActionRoute *
gtk_action_node_add_route_interest (GtkActionNode *self,
                                    GtkActionKey  *key)
{
  GtkActionRoute *route;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->retired, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  route = ensure_route (self, key, self->parent != NULL);
  route->n_local_interests++;
  adjust_route_ancestors (route, 1);

  return route;
}

void
gtk_action_route_remove_interest (GtkActionRoute *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->n_local_interests > 0);

  self->n_local_interests--;
  adjust_route_ancestors (self, -1);
  prune_route (self);
}

GtkActionNode *
gtk_action_route_get_node (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->node;
}

GtkActionKey *
gtk_action_route_get_key (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->key;
}

GtkActionRoute *
gtk_action_route_get_parent (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->parent;
}

GtkActionRoute *
gtk_action_route_get_first_child (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->first_child;
}

GtkActionRoute *
gtk_action_route_get_next_sibling (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->next_sibling;
}

guint
gtk_action_route_get_subtree_interest (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->n_subtree_interests;
}

guint64
gtk_action_route_get_revision (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->revision;
}

gpointer
gtk_action_route_get_local_provider (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->local_provider;
}

gpointer
gtk_action_route_get_effective_provider (GtkActionRoute *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->effective_provider;
}

void
gtk_action_route_set_local_provider (GtkActionRoute *self,
                                     gpointer        provider)
{
  g_return_if_fail (self != NULL);

  if (self->local_provider == provider)
    return;

  self->local_provider = provider;
  resolve_route (self);
}

static GtkActionRoute *
source_lookup_route (GtkActionSource *source,
                     const char      *action_name)
{
  char *full_name;
  GtkActionKey *key;
  GtkActionRoute *route = NULL;

  g_assert (source != NULL);
  g_assert (action_name != NULL);

  full_name = g_strconcat (source->prefix, ".", action_name, NULL);
  if ((key = gtk_action_key_new (full_name)))
    {
      route = lookup_route (source->node, key);
      gtk_action_key_unref (key);
    }
  g_free (full_name);

  return route;
}

static void
source_action_added (GActionGroup    *group,
                     const char      *action_name,
                     GtkActionSource *source)
{
  GtkActionRoute *route;

  gtk_action_tree_begin_update (source->node->tree);
  if ((route = source_lookup_route (source, action_name)))
    materialize_provider (route);
  gtk_action_tree_end_update (source->node->tree);
}

static void
source_action_removed (GActionGroup    *group,
                       const char      *action_name,
                       GtkActionSource *source)
{
  GtkActionRoute *route;
  GtkActionProvider *provider;

  gtk_action_tree_begin_update (source->node->tree);
  if ((route = source_lookup_route (source, action_name)) &&
      (provider = route->local_provider) != NULL &&
      provider->source == source)
    {
      gtk_action_route_set_local_provider (route, NULL);
      g_hash_table_steal (source->providers, provider->key);
      provider->retired = TRUE;
      gtk_action_tree_retire (source->node->tree, provider, free_provider);
    }
  gtk_action_tree_end_update (source->node->tree);
}

static void
source_action_enabled_changed (GActionGroup    *group,
                               const char      *action_name,
                               gboolean         enabled,
                               GtkActionSource *source)
{
  GtkActionRoute *route;
  GtkActionProvider *provider;

  gtk_action_tree_begin_update (source->node->tree);
  if ((route = source_lookup_route (source, action_name)) &&
      (provider = route->local_provider) != NULL &&
      provider->source == source)
    refresh_provider (provider);
  gtk_action_tree_end_update (source->node->tree);
}

static void
source_action_state_changed (GActionGroup    *group,
                             const char      *action_name,
                             GVariant        *state,
                             GtkActionSource *source)
{
  GtkActionRoute *route;
  GtkActionProvider *provider;

  gtk_action_tree_begin_update (source->node->tree);
  if ((route = source_lookup_route (source, action_name)) &&
      (provider = route->local_provider) != NULL &&
      provider->source == source)
    refresh_provider (provider);
  gtk_action_tree_end_update (source->node->tree);
}

static void
free_source (gpointer data)
{
  GtkActionSource *self = data;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (self->handler_ids); i++)
    {
      if (self->handler_ids[i] != 0)
        g_signal_handler_disconnect (self->group, self->handler_ids[i]);
    }

  g_assert_cmpuint (g_hash_table_size (self->providers), ==, 0);
  g_clear_pointer (&self->providers, g_hash_table_unref);
  g_clear_object (&self->group);
  g_clear_pointer (&self->prefix, g_free);
  g_free (self);
}

static void
remove_source (GtkActionSource *source)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (source != NULL);
  g_assert (!source->retired);

  source->retired = TRUE;
  g_hash_table_iter_init (&iter, source->providers);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GtkActionProvider *provider = value;
      GtkActionRoute *route = lookup_route (source->node, provider->key);

      if (route != NULL && route->local_provider == provider)
        gtk_action_route_set_local_provider (route, NULL);
      provider->retired = TRUE;
      gtk_action_tree_retire (source->node->tree, provider, free_provider);
      g_hash_table_iter_steal (&iter);
    }

  gtk_action_tree_retire (source->node->tree, source, free_source);
}

GtkActionSource *
gtk_action_node_insert_group (GtkActionNode *self,
                              const char    *prefix,
                              GActionGroup  *group)
{
  GtkActionSource *source;
  GtkActionSource *old_source;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->retired, NULL);
  g_return_val_if_fail (prefix != NULL && strchr (prefix, '.') == NULL, NULL);
  g_return_val_if_fail (G_IS_ACTION_GROUP (group), NULL);

  gtk_action_tree_begin_update (self->tree);
  if (self->sources == NULL)
    self->sources = g_hash_table_new (g_str_hash, g_str_equal);

  if ((old_source = g_hash_table_lookup (self->sources, prefix)))
    {
      if (old_source->group == group)
        {
          gtk_action_tree_end_update (self->tree);
          return old_source;
        }
      g_hash_table_steal (self->sources, prefix);
      remove_source (old_source);
    }

  source = g_new0 (GtkActionSource, 1);
  source->node = self;
  source->group = g_object_ref (group);
  source->prefix = g_strdup (prefix);
  source->providers = g_hash_table_new (gtk_action_key_hash, gtk_action_key_equal);
  source->handler_ids[0] = g_signal_connect (group, "action-added",
                                             G_CALLBACK (source_action_added), source);
  source->handler_ids[1] = g_signal_connect (group, "action-removed",
                                             G_CALLBACK (source_action_removed), source);
  source->handler_ids[2] = g_signal_connect (group, "action-enabled-changed",
                                             G_CALLBACK (source_action_enabled_changed), source);
  source->handler_ids[3] = g_signal_connect (group, "action-state-changed",
                                             G_CALLBACK (source_action_state_changed), source);
  g_hash_table_insert (self->sources, source->prefix, source);

  if (self->routes != NULL)
    {
      g_hash_table_iter_init (&iter, self->routes);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GtkActionRoute *route = value;

          if (source_matches_key (source, route->key))
            materialize_provider (route);
        }
    }
  gtk_action_tree_end_update (self->tree);

  return source;
}

void
gtk_action_node_remove_group (GtkActionNode *self,
                              const char    *prefix)
{
  GtkActionSource *source;

  g_return_if_fail (self != NULL);
  g_return_if_fail (prefix != NULL);

  if (self->sources == NULL || !(source = g_hash_table_lookup (self->sources, prefix)))
    return;

  gtk_action_tree_begin_update (self->tree);
  g_hash_table_steal (self->sources, prefix);
  remove_source (source);
  if (g_hash_table_size (self->sources) == 0)
    g_clear_pointer (&self->sources, g_hash_table_unref);
  gtk_action_tree_end_update (self->tree);
}

GActionGroup *
gtk_action_node_get_group (GtkActionNode *self,
                           const char    *prefix)
{
  GtkActionSource *source;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (prefix != NULL, NULL);

  source = self->sources != NULL ? g_hash_table_lookup (self->sources, prefix) : NULL;
  return source != NULL ? source->group : NULL;
}

GActionGroup *
gtk_action_node_find_group (GtkActionNode  *self,
                            GtkActionKey   *key,
                            const char    **local_name)
{
  GtkActionProvider *provider;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (!(provider = gtk_action_node_resolve (self, key)))
    return NULL;
  if (provider->source == NULL)
    return NULL;

  if (local_name != NULL)
    *local_name = gtk_action_key_get_local_name (key);
  return provider->source->group;
}

static void
append_source_actions (GtkActionSource *source,
                       GHashTable      *actions)
{
  char **names;
  guint i;

  g_assert (source != NULL);
  g_assert (actions != NULL);

  names = g_action_group_list_actions (source->group);
  for (i = 0; names[i] != NULL; i++)
    g_hash_table_add (actions, g_strconcat (source->prefix, ".", names[i], NULL));
  g_strfreev (names);
}

char **
gtk_action_node_list_actions (GtkActionNode *self,
                              gboolean       local_only)
{
  GHashTable *actions;
  GtkActionNode *node;
  char **ret;

  g_return_val_if_fail (self != NULL, NULL);

  actions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  for (node = self; node != NULL; node = local_only ? NULL : node->parent)
    {
      GHashTableIter iter;
      gpointer value;
      guint i;

      if (node->widget != NULL)
        {
          GtkWidgetClassPrivate *priv = GTK_WIDGET_GET_CLASS (node->widget)->priv;

          for (i = 0; priv->actions != NULL && i < priv->actions->len; i++)
            {
              GtkWidgetAction *action = g_ptr_array_index (priv->actions, i);

              g_hash_table_add (actions, g_strdup (action->name));
            }
        }

      if (node->sources == NULL)
        continue;
      g_hash_table_iter_init (&iter, node->sources);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        append_source_actions (value, actions);
    }

  ret = (char **)g_hash_table_get_keys_as_array (actions, NULL);
  g_hash_table_steal_all (actions);
  g_hash_table_unref (actions);

  return ret;
}

GtkActionProvider *
gtk_action_node_resolve (GtkActionNode *self,
                         GtkActionKey  *key)
{
  GtkActionRoute *route;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  route = lookup_route (self, key);
  if (route == NULL)
    return NULL;

  return route->effective_provider;
}

static gboolean
targets_equal (GVariant *a,
               GVariant *b)
{
  return (a == b ||
          (a != NULL && b != NULL && g_variant_equal (a, b)));
}

static void
queue_accel_change (GtkActionNode *self,
                    GtkActionKey  *key,
                    GVariant      *target)
{
  GtkActionNode *node;

  g_assert (self != NULL);
  g_assert (key != NULL);

  for (node = self; node != NULL; node = node->next_sibling)
    {
      GtkActionRoute *route = lookup_route (node, key);
      GtkActionBinding *binding;
      GtkActionSubscription *subscription;

      if (route != NULL)
        {
          for (binding = route->first_local_binding;
               binding != NULL;
               binding = binding->route_next)
            if ((binding->interest & GTK_ACTION_INTEREST_ACCEL) != 0 &&
                targets_equal (binding->target, target))
              queue_binding (binding);

          for (subscription = route->first_subscription;
               subscription != NULL;
               subscription = subscription->route_next)
            if ((subscription->interest & GTK_ACTION_INTEREST_ACCEL) != 0 &&
                targets_equal (subscription->target, target))
              queue_subscription (subscription);
        }

      if (node->first_child != NULL)
        queue_accel_change (node->first_child, key, target);
    }
}

void
gtk_action_node_set_primary_accel (GtkActionNode *self,
                                   GtkActionKey  *key,
                                   GVariant      *target,
                                   const char    *primary_accel)
{
  GtkActionInvocationKey *invocation = NULL;
  const char *old_accel;

  g_return_if_fail (self != NULL);
  g_return_if_fail (!self->retired);
  g_return_if_fail (key != NULL);

  invocation = gtk_action_invocation_key_new (key, target);
  old_accel = self->primary_accels != NULL
            ? g_hash_table_lookup (self->primary_accels, invocation) : NULL;
  if (g_strcmp0 (old_accel, primary_accel) == 0)
    {
      gtk_action_invocation_key_unref (invocation);
      return;
    }

  gtk_action_tree_begin_update (self->tree);
  if (primary_accel != NULL)
    {
      if (self->primary_accels == NULL)
        self->primary_accels =
          g_hash_table_new_full (gtk_action_invocation_key_hash,
                                 gtk_action_invocation_key_equal,
                                 (GDestroyNotify) gtk_action_invocation_key_unref,
                                 g_free);
      g_hash_table_replace (self->primary_accels,
                            gtk_action_invocation_key_ref (invocation),
                            g_strdup (primary_accel));
    }
  else if (self->primary_accels != NULL)
    {
      g_hash_table_remove (self->primary_accels, invocation);
      if (g_hash_table_size (self->primary_accels) == 0)
        g_clear_pointer (&self->primary_accels, g_hash_table_unref);
    }
  queue_accel_change (self, key, target);
  gtk_action_tree_end_update (self->tree);

  g_clear_pointer (&invocation, gtk_action_invocation_key_unref);
}

const char *
gtk_action_node_get_primary_accel (GtkActionNode *self,
                                   GtkActionKey  *key,
                                   GVariant      *target)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->retired, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return lookup_primary_accel (self, key, target);
}

void
gtk_action_node_class_action_enabled_changed (GtkActionNode *self,
                                               const char    *action_name)
{
  g_autoptr(GtkActionKey) key = NULL;
  GtkActionProvider *provider;
  GtkActionRoute *route;

  g_return_if_fail (self != NULL);
  g_return_if_fail (action_name != NULL);

  if (!(key = gtk_action_key_new (action_name)) ||
      !(route = lookup_route (self, key)) ||
      !(provider = route->local_provider) ||
      provider->widget_action == NULL)
    return;

  gtk_action_tree_begin_update (self->tree);
  refresh_provider (provider);
  gtk_action_tree_end_update (self->tree);
}

GtkActionBinding *
gtk_action_node_bind (GtkActionNode            *self,
                      GtkActionKey             *key,
                      GVariant                 *target,
                      GtkActionInterest         interest,
                      GtkActionBindingCallback  callback,
                      gpointer                  user_data,
                      GDestroyNotify            destroy)
{
  GtkActionBindingState state;
  GtkActionBinding *binding;
  GtkActionRoute *route;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->retired, NULL);
  g_return_val_if_fail (key != NULL, NULL);
  g_return_val_if_fail (interest != GTK_ACTION_INTEREST_NONE, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  route = gtk_action_node_add_route_interest (self, key);
  binding = g_new0 (GtkActionBinding, 1);
  binding->route = route;
  binding->key = gtk_action_key_ref (key);
  binding->target = target != NULL ? g_variant_ref_sink (target) : NULL;
  binding->callback = callback;
  binding->user_data = user_data;
  binding->destroy = destroy;
  binding->interest = interest;
  binding->alive = TRUE;
  binding->state.role = GTK_BUTTON_ROLE_NORMAL;

  binding->route_next = route->first_local_binding;
  if (binding->route_next != NULL)
    binding->route_next->route_prev = binding;
  route->first_local_binding = binding;
  binding_set_provider (binding, route->effective_provider);

  derive_binding_state (binding, &state);
  binding->primary_accel = g_strdup (state.primary_accel);
  state.primary_accel = binding->primary_accel;
  binding->state = state;
  self->tree->dispatch_depth++;
  callback (binding, interest_changes (interest), &binding->state, user_data);
  self->tree->dispatch_depth--;

  if (!binding->alive)
    {
      if (self->tree->dispatch_depth == 0)
        reclaim_retired (self->tree);
      return NULL;
    }

  return binding;
}

void
gtk_action_binding_cancel (GtkActionBinding *self)
{
  GtkActionRoute *route;
  GtkActionTree *tree;

  g_return_if_fail (self != NULL);

  if (!self->alive)
    return;

  route = self->route;
  tree = route->node->tree;
  self->alive = FALSE;
  if (self->owner_location != NULL)
    {
      if (*self->owner_location == self)
        *self->owner_location = NULL;
      self->owner_location = NULL;
    }

  if (self->route_prev != NULL)
    self->route_prev->route_next = self->route_next;
  else
    route->first_local_binding = self->route_next;
  if (self->route_next != NULL)
    self->route_next->route_prev = self->route_prev;
  self->route_prev = NULL;
  self->route_next = NULL;
  binding_set_provider (self, NULL);
  self->route = NULL;

  gtk_action_route_remove_interest (route);
  gtk_action_tree_retire (tree, self, binding_free);
}

void
gtk_action_binding_set_owner_location (GtkActionBinding  *self,
                                       GtkActionBinding **owner_location)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->alive);
  g_return_if_fail (owner_location != NULL);
  g_return_if_fail (*owner_location == self);

  self->owner_location = owner_location;
}

void
gtk_action_binding_set_target (GtkActionBinding *self,
                               GVariant         *target)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->alive);

  if (target == self->target)
    return;

  if (target != NULL && self->target != NULL && g_variant_equal (target, self->target))
    {
      g_variant_unref (g_variant_ref_sink (target));
      return;
    }

  binding_unlink_state_index (self);
  g_clear_pointer (&self->target, g_variant_unref);
  self->target = target != NULL ? g_variant_ref_sink (target) : NULL;
  binding_link_state_index (self);
  queue_binding (self);
}

gboolean
gtk_action_binding_activate (GtkActionBinding *self)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (self->alive, FALSE);

  if (!self->state.activatable || self->provider == NULL)
    return FALSE;

  return gtk_action_provider_activate (self->provider, self->target);
}

GtkActionKey *
gtk_action_binding_get_key (GtkActionBinding *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->key;
}

GVariant *
gtk_action_binding_get_target (GtkActionBinding *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->target;
}

const GtkActionBindingState *
gtk_action_binding_get_state (GtkActionBinding *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->alive, NULL);

  return &self->state;
}

GtkActionSubscription *
gtk_action_node_subscribe (GtkActionNode                 *self,
                           GtkActionKey                  *key,
                           GVariant                      *target,
                           GtkActionInterest              interest,
                           GtkActionSubscriptionCallback  callback,
                           gpointer                       user_data,
                           GDestroyNotify                 destroy)
{
  GtkActionSnapshot initial = GTK_ACTION_SNAPSHOT_INIT;
  GtkActionSubscription *subscription;
  GtkActionRoute *route;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->retired, NULL);
  g_return_val_if_fail (key != NULL, NULL);
  g_return_val_if_fail (interest != GTK_ACTION_INTEREST_NONE, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  route = gtk_action_node_add_route_interest (self, key);
  subscription = g_new0 (GtkActionSubscription, 1);
  subscription->route = route;
  subscription->key = gtk_action_key_ref (key);
  subscription->target = target != NULL ? g_variant_ref_sink (target) : NULL;
  subscription->callback = callback;
  subscription->user_data = user_data;
  subscription->destroy = destroy;
  subscription->interest = interest;
  gtk_action_snapshot_init (&subscription->snapshot);

  subscription->route_next = route->first_subscription;
  if (subscription->route_next != NULL)
    subscription->route_next->route_prev = subscription;
  route->first_subscription = subscription;
  subscription_set_provider (subscription, route->effective_provider);

  if (subscription->provider != NULL)
    gtk_action_snapshot_copy (&initial, &subscription->provider->snapshot);
  else
    initial.revision = route->revision;
  snapshot_set_primary_accel (&initial, self, key, subscription->target);
  gtk_action_snapshot_copy (&subscription->snapshot, &initial);
  self->tree->dispatch_depth++;
  callback (subscription, interest_changes (interest), &subscription->snapshot, user_data);
  self->tree->dispatch_depth--;
  gtk_action_snapshot_clear (&initial);

  if (subscription->cancelled)
    {
      if (self->tree->dispatch_depth == 0)
        reclaim_retired (self->tree);
      return NULL;
    }

  return subscription;
}

void
gtk_action_subscription_cancel (GtkActionSubscription *self)
{
  GtkActionRoute *route;
  GtkActionTree *tree;

  g_return_if_fail (self != NULL);

  if (self->cancelled)
    return;

  route = self->route;
  tree = route->node->tree;
  self->cancelled = TRUE;

  if (self->route_prev != NULL)
    self->route_prev->route_next = self->route_next;
  else
    route->first_subscription = self->route_next;
  if (self->route_next != NULL)
    self->route_next->route_prev = self->route_prev;
  self->route_prev = NULL;
  self->route_next = NULL;
  subscription_set_provider (self, NULL);
  self->route = NULL;

  gtk_action_route_remove_interest (route);
  gtk_action_tree_retire (tree, self, subscription_free);
}

GtkActionNode *
gtk_action_subscription_get_node (GtkActionSubscription *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->cancelled, NULL);

  return self->route->node;
}

GtkActionKey *
gtk_action_subscription_get_key (GtkActionSubscription *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->key;
}

GVariant *
gtk_action_subscription_get_target (GtkActionSubscription *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->target;
}

GtkActionInterest
gtk_action_subscription_get_interest (GtkActionSubscription *self)
{
  g_return_val_if_fail (self != NULL, GTK_ACTION_INTEREST_NONE);

  return self->interest;
}

const GtkActionSnapshot *
gtk_action_subscription_get_snapshot (GtkActionSubscription *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!self->cancelled, NULL);

  return &self->snapshot;
}

GtkActionNode *
gtk_action_provider_get_node (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->node;
}

GtkActionKey *
gtk_action_provider_get_key (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->key;
}

GtkActionSource *
gtk_action_provider_get_source (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->source;
}

const GtkActionSnapshot *
gtk_action_provider_get_snapshot (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->snapshot;
}

guint64
gtk_action_provider_get_revision (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->revision;
}

guint
gtk_action_provider_get_target_count (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->target_buckets != NULL ? self->target_buckets->len : 0;
}

gboolean
gtk_action_provider_has_target_index (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->target_index != NULL;
}

guint64
gtk_action_provider_get_touched_bindings (GtkActionProvider *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->state_bindings_touched;
}

void
gtk_action_provider_reset_touched_bindings (GtkActionProvider *self)
{
  g_return_if_fail (self != NULL);

  self->state_bindings_touched = 0;
}

gboolean
gtk_action_provider_query (GtkActionProvider  *self,
                           gboolean           *enabled,
                           const GVariantType **parameter_type,
                           const GVariantType **state_type,
                           GVariant           **state_hint,
                           GVariant           **state)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (!self->snapshot.present)
    return FALSE;
  if (enabled != NULL)
    *enabled = self->snapshot.enabled;
  if (parameter_type != NULL)
    *parameter_type = self->snapshot.parameter_type;
  if (state_type != NULL)
    *state_type = self->snapshot.state_type;
  if (state_hint != NULL)
    *state_hint = self->snapshot.state_hint != NULL
                ? g_variant_ref (self->snapshot.state_hint)
                : NULL;
  if (state != NULL)
    *state = self->snapshot.state != NULL ? g_variant_ref (self->snapshot.state) : NULL;

  return TRUE;
}

gboolean
gtk_action_provider_activate (GtkActionProvider *self,
                              GVariant          *parameter)
{
  const GVariantType *expected;

  g_return_val_if_fail (self != NULL, FALSE);

  expected = self->snapshot.parameter_type;
  if ((expected == NULL) != (parameter == NULL) ||
      (expected != NULL && !g_variant_is_of_type (parameter, expected)))
    {
      if (self->source != NULL)
        g_action_group_activate_action (self->source->group,
                                        gtk_action_key_get_local_name (self->key),
                                        parameter);
      return FALSE;
    }

  if (self->widget_action != NULL)
    gtk_widget_action_activate (self->node->widget, self->widget_action, parameter);
  else
    g_action_group_activate_action (self->source->group,
                                    gtk_action_key_get_local_name (self->key),
                                    parameter);
  return TRUE;
}

gboolean
gtk_action_provider_change_state (GtkActionProvider *self,
                                  GVariant          *state)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (state != NULL, FALSE);

  if (self->snapshot.state_type == NULL ||
      !g_variant_is_of_type (state, self->snapshot.state_type))
    {
      if (self->source != NULL)
        g_action_group_change_action_state (self->source->group,
                                            gtk_action_key_get_local_name (self->key),
                                            state);
      return FALSE;
    }

  if (self->widget_action != NULL)
    gtk_widget_action_change_state (self->node->widget, self->widget_action, state);
  else
    g_action_group_change_action_state (self->source->group,
                                        gtk_action_key_get_local_name (self->key),
                                        state);
  return TRUE;
}

gulong
gtk_action_provider_add_observer (GtkActionProvider         *self,
                                  GtkActionProviderCallback  callback,
                                  gpointer                   user_data,
                                  GDestroyNotify             destroy)
{
  static gulong next_observer_id;
  GtkActionProviderObserver *observer;

  g_return_val_if_fail (self != NULL, 0);
  g_return_val_if_fail (callback != NULL, 0);

  observer = g_new0 (GtkActionProviderObserver, 1);
  observer->callback = callback;
  observer->user_data = user_data;
  observer->destroy = destroy;
  observer->id = ++next_observer_id;
  observer->next = self->observers;
  self->observers = observer;

  return observer->id;
}

void
gtk_action_provider_remove_observer (GtkActionProvider *self,
                                     gulong             observer_id)
{
  GtkActionProviderObserver *observer;
  GtkActionProviderObserver **link;

  g_return_if_fail (self != NULL);
  g_return_if_fail (observer_id != 0);

  for (link = &self->observers; (observer = *link) != NULL; link = &observer->next)
    {
      if (observer->id != observer_id)
        continue;

      if (self->dispatch_depth > 0)
        observer->removed = TRUE;
      else
        {
          *link = observer->next;
          if (observer->destroy != NULL)
            observer->destroy (observer->user_data);
          g_free (observer);
        }
      return;
    }
}

const char *
gtk_action_source_get_prefix (GtkActionSource *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->prefix;
}

GActionGroup *
gtk_action_source_get_group (GtkActionSource *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->group;
}

GtkActionNode *
_gtk_widget_get_action_node (GtkWidget *widget,
                             gboolean   create)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->action_node == NULL && create)
    {
      if (default_tree == NULL)
        default_tree = gtk_action_tree_new ();
      priv->action_node = gtk_action_tree_add_widget (default_tree, widget);
      if (GTK_IS_WINDOW (widget))
        gtk_action_node_set_synthetic_parent (priv->action_node,
                                              ensure_window_scope (default_tree,
                                                                   GTK_WINDOW (widget)));
    }

  return priv->action_node;
}

void
_gtk_widget_update_action_tree (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->action_node != NULL)
    gtk_action_node_sync_parent (priv->action_node);
}

void
_gtk_widget_remove_action_node (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_clear_pointer (&priv->action_node, gtk_action_node_remove);
}
