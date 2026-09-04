/* gtkactiontreeprivate.h
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

#pragma once

#include "gtkactionkeyprivate.h"
#include "gtkactionsnapshotprivate.h"
#include "gtkmodelbuttonprivate.h"
#include "gtkwidget.h"

G_BEGIN_DECLS

typedef struct _GtkActionBinding      GtkActionBinding;
typedef struct _GtkActionNode         GtkActionNode;
typedef struct _GtkActionProvider     GtkActionProvider;
typedef struct _GtkActionResolution   GtkActionResolution;
typedef struct _GtkActionRoute        GtkActionRoute;
typedef struct _GtkActionSource       GtkActionSource;
typedef struct _GtkActionSubscription GtkActionSubscription;
typedef struct _GtkActionTree         GtkActionTree;

struct _GtkActionResolution
{
  GtkActionRoute      *route;
  GtkActionResolution *prev;
  GtkActionResolution *next;
};

#define GTK_ACTION_RESOLUTION_INIT { NULL, NULL, NULL }

typedef struct _GtkActionBindingState
{
  const char    *primary_accel;
  GtkButtonRole  role;
  guint          present : 1;
  guint          activatable : 1;
  guint          enabled : 1;
  guint          active : 1;
} GtkActionBindingState;

typedef void (*GtkActionTreeCallback)         (GtkActionTree               *tree,
                                               gpointer                     user_data);
typedef void (*GtkActionSubscriptionCallback) (GtkActionSubscription       *subscription,
                                               GtkActionChange              changed,
                                               const GtkActionSnapshot     *snapshot,
                                               gpointer                     user_data);
typedef void (*GtkActionBindingCallback)      (GtkActionBinding            *binding,
                                               GtkActionChange              changed,
                                               const GtkActionBindingState *state,
                                               gpointer                     user_data);

gboolean                      gtk_action_binding_activate                (GtkActionBinding               *self);
void                          gtk_action_binding_cancel                  (GtkActionBinding               *self);
GtkActionKey                 *gtk_action_binding_get_key                 (GtkActionBinding               *self);
const GtkActionBindingState  *gtk_action_binding_get_state               (GtkActionBinding               *self);
GVariant                     *gtk_action_binding_get_target              (GtkActionBinding               *self);
void                          gtk_action_binding_set_owner_location      (GtkActionBinding               *self,
                                                                          GtkActionBinding              **owner_location);
void                          gtk_action_binding_set_target              (GtkActionBinding               *self,
                                                                          GVariant                       *target);
gboolean                      gtk_action_node_activate                   (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          GVariant                       *parameter);
GtkActionRoute               *gtk_action_node_add_route_interest         (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key);
GtkActionBinding             *gtk_action_node_bind                       (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          GVariant                       *target,
                                                                          GtkActionInterest               interest,
                                                                          GtkActionBindingCallback        callback,
                                                                          gpointer                        user_data,
                                                                          GDestroyNotify                  destroy);
gboolean                      gtk_action_node_change_state               (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          GVariant                       *state);
GActionGroup                 *gtk_action_node_find_group                 (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          const char                    **local_name);
GtkActionNode                *gtk_action_node_get_first_child            (GtkActionNode                  *self);
GActionGroup                 *gtk_action_node_get_group                  (GtkActionNode                  *self,
                                                                          const char                     *prefix);
GtkActionNode                *gtk_action_node_get_next_sibling           (GtkActionNode                  *self);
gpointer                      gtk_action_node_get_owner                  (GtkActionNode                  *self);
GtkActionNode                *gtk_action_node_get_parent                 (GtkActionNode                  *self);
const char                   *gtk_action_node_get_primary_accel          (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          GVariant                       *target);
GtkWidget                    *gtk_action_node_get_widget                 (GtkActionNode                  *self);
GtkActionSource              *gtk_action_node_insert_group               (GtkActionNode                  *self,
                                                                          const char                     *prefix,
                                                                          GActionGroup                   *group);
char                        **gtk_action_node_list_actions               (GtkActionNode                  *self,
                                                                          gboolean                        local_only);
GtkActionRoute               *gtk_action_node_lookup_route               (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key);
GtkActionNode                *gtk_action_node_new_synthetic              (gpointer                        owner);
void                          gtk_action_node_remove                     (GtkActionNode                  *self);
void                          gtk_action_node_remove_group               (GtkActionNode                  *self,
                                                                          const char                     *prefix);
GtkActionProvider            *gtk_action_node_resolve_provider           (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key);
GtkActionNode                *gtk_action_node_resolve_provider_node      (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key);
void                          gtk_action_node_set_class_action_enabled   (GtkActionNode                  *self,
                                                                          const char                     *action_name,
                                                                          gboolean                        enabled);
void                          gtk_action_node_set_primary_accel          (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          GVariant                       *target,
                                                                          const char                     *primary_accel);
void                          gtk_action_node_set_synthetic_parent       (GtkActionNode                  *self,
                                                                          GtkActionNode                  *parent);
GtkActionSubscription        *gtk_action_node_subscribe                  (GtkActionNode                  *self,
                                                                          GtkActionKey                   *key,
                                                                          GVariant                       *target,
                                                                          GtkActionInterest               interest,
                                                                          GtkActionSubscriptionCallback   callback,
                                                                          gpointer                        user_data,
                                                                          GDestroyNotify                  destroy);
void                          gtk_action_node_sync_parent                (GtkActionNode                  *self);
gboolean                      gtk_action_provider_activate               (GtkActionProvider              *self,
                                                                          GVariant                       *parameter);
gboolean                      gtk_action_provider_change_state           (GtkActionProvider              *self,
                                                                          GVariant                       *state);
GtkActionKey                 *gtk_action_provider_get_key                (GtkActionProvider              *self);
GtkActionNode                *gtk_action_provider_get_node               (GtkActionProvider              *self);
guint64                       gtk_action_provider_get_revision           (GtkActionProvider              *self);
const GtkActionSnapshot      *gtk_action_provider_get_snapshot           (GtkActionProvider              *self);
GtkActionSource              *gtk_action_provider_get_source             (GtkActionProvider              *self);
guint                         gtk_action_provider_get_target_count       (GtkActionProvider              *self);
guint64                       gtk_action_provider_get_touched_bindings   (GtkActionProvider              *self);
gboolean                      gtk_action_provider_has_target_index       (GtkActionProvider              *self);
gboolean                      gtk_action_provider_query                  (GtkActionProvider              *self,
                                                                          gboolean                       *enabled,
                                                                          const GVariantType            **parameter_type,
                                                                          const GVariantType            **state_type,
                                                                          GVariant                      **state_hint,
                                                                          GVariant                      **state);
void                          gtk_action_provider_reset_touched_bindings (GtkActionProvider              *self);
gboolean                      gtk_action_resolution_activate             (GtkActionResolution            *self,
                                                                          GVariant                       *parameter);
gboolean                      gtk_action_resolution_change_state         (GtkActionResolution            *self,
                                                                          GVariant                       *state);
void                          gtk_action_resolution_clear                (GtkActionResolution            *self);
GtkActionNode                *gtk_action_resolution_get_provider_node    (GtkActionResolution            *self);
gboolean                      gtk_action_resolution_init                 (GtkActionResolution            *self,
                                                                          GtkActionNode                  *node,
                                                                          GtkActionKey                   *key);
gboolean                      gtk_action_resolution_query                (GtkActionResolution            *self,
                                                                          gboolean                       *enabled,
                                                                          const GVariantType            **parameter_type,
                                                                          const GVariantType            **state_type,
                                                                          GVariant                      **state_hint,
                                                                          GVariant                      **state);
gpointer                      gtk_action_route_get_effective_provider    (GtkActionRoute                 *self);
GtkActionRoute               *gtk_action_route_get_first_child           (GtkActionRoute                 *self);
GtkActionKey                 *gtk_action_route_get_key                   (GtkActionRoute                 *self);
gpointer                      gtk_action_route_get_local_provider        (GtkActionRoute                 *self);
GtkActionRoute               *gtk_action_route_get_next_sibling          (GtkActionRoute                 *self);
GtkActionNode                *gtk_action_route_get_node                  (GtkActionRoute                 *self);
GtkActionRoute               *gtk_action_route_get_parent                (GtkActionRoute                 *self);
guint64                       gtk_action_route_get_revision              (GtkActionRoute                 *self);
guint                         gtk_action_route_get_subtree_interest      (GtkActionRoute                 *self);
void                          gtk_action_route_remove_interest           (GtkActionRoute                 *self);
void                          gtk_action_route_set_local_provider        (GtkActionRoute                 *self,
                                                                          gpointer                        provider);
GActionGroup                 *gtk_action_source_get_group                (GtkActionSource                *self);
const char                   *gtk_action_source_get_prefix               (GtkActionSource                *self);
void                          gtk_action_subscription_cancel             (GtkActionSubscription          *self);
GtkActionInterest             gtk_action_subscription_get_interest       (GtkActionSubscription          *self);
GtkActionKey                 *gtk_action_subscription_get_key            (GtkActionSubscription          *self);
GtkActionNode                *gtk_action_subscription_get_node           (GtkActionSubscription          *self);
const GtkActionSnapshot      *gtk_action_subscription_get_snapshot       (GtkActionSubscription          *self);
GVariant                     *gtk_action_subscription_get_target         (GtkActionSubscription          *self);
void                          gtk_action_subscription_set_owner_location (GtkActionSubscription          *self,
                                                                          GtkActionSubscription         **owner_location);
GtkActionNode                *gtk_action_tree_add_synthetic              (GtkActionTree                  *tree,
                                                                          gpointer                        owner);
GtkActionNode                *gtk_action_tree_add_widget                 (GtkActionTree                  *tree,
                                                                          GtkWidget                      *widget);
void                          gtk_action_tree_begin_update               (GtkActionTree                  *tree);
gboolean                      gtk_action_tree_check_invariants           (GtkActionTree                  *tree);
void                          gtk_action_tree_end_update                 (GtkActionTree                  *tree);
void                          gtk_action_tree_free                       (GtkActionTree                  *tree);
guint                         gtk_action_tree_get_dispatch_depth         (GtkActionTree                  *tree);
guint                         gtk_action_tree_get_update_depth           (GtkActionTree                  *tree);
GtkActionTree                *gtk_action_tree_new                        (void);
void                          gtk_action_tree_queue_callback             (GtkActionTree                  *tree,
                                                                          GtkActionTreeCallback           callback,
                                                                          gpointer                        user_data,
                                                                          GDestroyNotify                  destroy);
void                          gtk_action_tree_retire                     (GtkActionTree                  *tree,
                                                                          gpointer                        data,
                                                                          GDestroyNotify                  destroy);
guint                         gtk_widget_get_action_subtree_count        (GtkWidget                      *widget);
GtkActionNode                *_gtk_widget_get_action_node                (GtkWidget                      *widget,
                                                                          gboolean                        create);
void                          _gtk_widget_remove_action_node             (GtkWidget                      *widget);

G_END_DECLS

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (GtkActionResolution, gtk_action_resolution_clear)
