/* action-tree.c
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

#include "gtk/gtk.h"
#include "gtk/gtkactiontreeprivate.h"
#include "gtk/gtkmenutrackerprivate.h"
#include "gtk/gtkmodelbuttonprivate.h"
#include "gtk/inspector/actions.h"

static void
test_sparse_insertion (void)
{
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *outer = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *middle = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *inner = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkActionNode *outer_node;
  GtkActionNode *inner_node;

  gtk_box_append (GTK_BOX (outer), middle);
  gtk_box_append (GTK_BOX (middle), inner);

  inner_node = gtk_action_tree_add_widget (tree, inner);
  g_assert_null (gtk_action_node_get_parent (inner_node));
  g_assert_cmpuint (gtk_widget_get_action_subtree_count (outer), ==, 1);
  g_assert_cmpuint (gtk_widget_get_action_subtree_count (middle), ==, 1);

  outer_node = gtk_action_tree_add_widget (tree, outer);
  g_assert_true (gtk_action_node_get_parent (inner_node) == outer_node);
  g_assert_true (gtk_action_node_get_first_child (outer_node) == inner_node);
  g_assert_cmpuint (gtk_widget_get_action_subtree_count (outer), ==, 2);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_assert_cmpuint (gtk_widget_get_action_subtree_count (outer), ==, 0);
  g_object_unref (outer);
}

static void
test_reparent_and_prune (void)
{
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *left = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *right = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *branch = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *leaf = gtk_button_new ();
  GtkActionNode *left_node;
  GtkActionNode *right_node;
  GtkActionNode *leaf_node;
  guint left_baseline;
  guint right_baseline;
  guint branch_baseline;

  gtk_box_append (GTK_BOX (left), branch);
  gtk_box_append (GTK_BOX (branch), leaf);
  left_baseline = gtk_widget_get_action_subtree_count (left);
  right_baseline = gtk_widget_get_action_subtree_count (right);
  branch_baseline = gtk_widget_get_action_subtree_count (branch);
  left_node = gtk_action_tree_add_widget (tree, left);
  right_node = gtk_action_tree_add_widget (tree, right);
  leaf_node = gtk_action_tree_add_widget (tree, leaf);
  g_assert_true (gtk_action_node_get_parent (leaf_node) == left_node);

  g_object_ref (branch);
  gtk_box_remove (GTK_BOX (left), branch);
  gtk_box_append (GTK_BOX (right), branch);
  g_object_unref (branch);
  gtk_action_node_sync_parent (leaf_node);

  g_assert_true (gtk_action_node_get_parent (leaf_node) == right_node);
  g_assert_cmpuint (gtk_widget_get_action_subtree_count (left), ==,
                    left_baseline - branch_baseline + 1);
  g_assert_cmpuint (gtk_widget_get_action_subtree_count (right), ==,
                    right_baseline + branch_baseline + 2);

  gtk_action_node_remove (right_node);
  g_assert_null (gtk_action_node_get_parent (leaf_node));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (left);
  g_object_unref (right);
}

static void
test_synthetic_scope (void)
{
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *window = g_object_ref_sink (gtk_window_new ());
  GtkActionNode *application;
  GtkActionNode *window_node;
  int application_owner;

  application = gtk_action_tree_add_synthetic (tree, &application_owner);
  window_node = gtk_action_tree_add_widget (tree, window);
  gtk_action_node_set_synthetic_parent (window_node, application);

  g_assert_true (gtk_action_node_get_parent (window_node) == application);
  g_assert_true (gtk_action_node_get_owner (application) == &application_owner);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (window);
}

typedef struct
{
  guint callbacks;
  guint destroys;
  gboolean in_first;
} TransactionState;

static void
retired_destroy (gpointer data)
{
  TransactionState *state = data;

  g_assert_false (state->in_first);
  state->destroys++;
}

static void
second_callback (GtkActionTree *tree,
                 gpointer       user_data)
{
  TransactionState *state = user_data;

  g_assert_cmpuint (gtk_action_tree_get_update_depth (tree), ==, 0);
  g_assert_cmpuint (gtk_action_tree_get_dispatch_depth (tree), ==, 1);
  state->callbacks++;
}

static void
first_callback (GtkActionTree *tree,
                gpointer       user_data)
{
  TransactionState *state = user_data;

  g_assert_cmpuint (gtk_action_tree_get_update_depth (tree), ==, 0);
  state->in_first = TRUE;
  state->callbacks++;
  gtk_action_tree_retire (tree, state, retired_destroy);
  gtk_action_tree_queue_callback (tree, second_callback, state, NULL);
  g_assert_cmpuint (state->destroys, ==, 0);
  state->in_first = FALSE;
}

static void
test_transactions (void)
{
  GtkActionTree *tree = gtk_action_tree_new ();
  TransactionState state = { 0 };

  gtk_action_tree_begin_update (tree);
  gtk_action_tree_begin_update (tree);
  gtk_action_tree_queue_callback (tree, first_callback, &state, NULL);
  gtk_action_tree_end_update (tree);
  g_assert_cmpuint (state.callbacks, ==, 0);
  gtk_action_tree_end_update (tree);

  g_assert_cmpuint (state.callbacks, ==, 2);
  g_assert_cmpuint (state.destroys, ==, 1);
  g_assert_true (gtk_action_tree_check_invariants (tree));
  gtk_action_tree_free (tree);
}

static void
test_route_inheritance_and_shadowing (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.save");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *middle = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *leaf = gtk_button_new ();
  GtkActionNode *root_node;
  GtkActionNode *middle_node;
  GtkActionNode *leaf_node;
  GtkActionRoute *root_route;
  GtkActionRoute *middle_route;
  GtkActionRoute *leaf_route;
  int root_provider;
  int middle_provider;

  gtk_box_append (GTK_BOX (root), middle);
  gtk_box_append (GTK_BOX (middle), leaf);
  root_node = gtk_action_tree_add_widget (tree, root);
  middle_node = gtk_action_tree_add_widget (tree, middle);
  leaf_node = gtk_action_tree_add_widget (tree, leaf);

  leaf_route = gtk_action_node_add_route_interest (leaf_node, key);
  root_route = gtk_action_node_lookup_route (root_node, key);
  middle_route = gtk_action_node_lookup_route (middle_node, key);
  g_assert_nonnull (root_route);
  g_assert_true (gtk_action_route_get_parent (leaf_route) == middle_route);
  g_assert_cmpuint (gtk_action_route_get_subtree_interest (root_route), ==, 1);

  gtk_action_route_set_local_provider (root_route, &root_provider);
  g_assert_true (gtk_action_route_get_effective_provider (leaf_route) == &root_provider);
  gtk_action_route_set_local_provider (middle_route, &middle_provider);
  g_assert_true (gtk_action_route_get_effective_provider (leaf_route) == &middle_provider);
  gtk_action_route_set_local_provider (root_route, NULL);
  g_assert_true (gtk_action_route_get_effective_provider (leaf_route) == &middle_provider);
  gtk_action_route_set_local_provider (middle_route, NULL);
  g_assert_null (gtk_action_route_get_effective_provider (leaf_route));

  gtk_action_route_remove_interest (leaf_route);
  g_assert_null (gtk_action_node_lookup_route (leaf_node, key));
  g_assert_null (gtk_action_node_lookup_route (middle_node, key));
  g_assert_null (gtk_action_node_lookup_route (root_node, key));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (root);

  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_route_identity_and_counts (void)
{
  GtkActionKey *first = gtk_action_key_new ("win.open");
  GtkActionKey *same = gtk_action_key_new ("win.open");
  GtkActionKey *other_prefix = gtk_action_key_new ("app.open");
  GtkActionKey *other_local = gtk_action_key_new ("win.close");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *widget = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkActionNode *node = gtk_action_tree_add_widget (tree, widget);
  GtkActionRoute *route;
  GtkActionRoute *duplicate;
  GtkActionRoute *prefix_route;
  GtkActionRoute *local_route;

  route = gtk_action_node_add_route_interest (node, first);
  duplicate = gtk_action_node_add_route_interest (node, same);
  prefix_route = gtk_action_node_add_route_interest (node, other_prefix);
  local_route = gtk_action_node_add_route_interest (node, other_local);

  g_assert_true (route == duplicate);
  g_assert_true (gtk_action_route_get_key (route) == first);
  g_assert_cmpuint (gtk_action_route_get_subtree_interest (route), ==, 2);
  g_assert_true (prefix_route != route);
  g_assert_true (local_route != route);
  g_assert_true (prefix_route != local_route);

  gtk_action_route_remove_interest (route);
  g_assert_cmpuint (gtk_action_route_get_subtree_interest (duplicate), ==, 1);
  gtk_action_route_remove_interest (duplicate);
  gtk_action_route_remove_interest (prefix_route);
  gtk_action_route_remove_interest (local_route);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (widget);

  g_clear_pointer (&other_local, gtk_action_key_unref);
  g_clear_pointer (&other_prefix, gtk_action_key_unref);
  g_clear_pointer (&same, gtk_action_key_unref);
  g_clear_pointer (&first, gtk_action_key_unref);
}

static void
test_route_reparenting (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.move");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *outer = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  GtkWidget *left = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *right = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *branch = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *leaf = gtk_button_new ();
  GtkActionNode *outer_node;
  GtkActionNode *left_node;
  GtkActionNode *right_node;
  GtkActionNode *leaf_node;
  GtkActionRoute *outer_route;
  GtkActionRoute *leaf_route;
  guint64 revision;
  int provider;

  gtk_box_append (GTK_BOX (outer), left);
  gtk_box_append (GTK_BOX (outer), right);
  gtk_box_append (GTK_BOX (left), branch);
  gtk_box_append (GTK_BOX (branch), leaf);
  outer_node = gtk_action_tree_add_widget (tree, outer);
  left_node = gtk_action_tree_add_widget (tree, left);
  right_node = gtk_action_tree_add_widget (tree, right);
  leaf_node = gtk_action_tree_add_widget (tree, leaf);
  leaf_route = gtk_action_node_add_route_interest (leaf_node, key);
  outer_route = gtk_action_node_lookup_route (outer_node, key);
  gtk_action_route_set_local_provider (outer_route, &provider);
  revision = gtk_action_route_get_revision (leaf_route);

  g_object_ref (branch);
  gtk_box_remove (GTK_BOX (left), branch);
  gtk_box_append (GTK_BOX (right), branch);
  g_object_unref (branch);
  gtk_action_node_sync_parent (leaf_node);

  g_assert_true (gtk_action_route_get_node (gtk_action_route_get_parent (leaf_route)) == right_node);
  g_assert_null (gtk_action_node_lookup_route (left_node, key));
  g_assert_cmpuint (gtk_action_route_get_subtree_interest (outer_route), ==, 1);
  g_assert_true (gtk_action_route_get_effective_provider (leaf_route) == &provider);
  g_assert_cmpuint (gtk_action_route_get_revision (leaf_route), ==, revision);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (outer);

  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_large_route_subtree_move (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.item");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *left = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *right = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *branch = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkActionNode *left_node = gtk_action_tree_add_widget (tree, left);
  GtkActionNode *right_node = gtk_action_tree_add_widget (tree, right);
  GPtrArray *leaf_nodes = g_ptr_array_new ();
  guint i;

  gtk_box_append (GTK_BOX (left), branch);
  for (i = 0; i < 256; i++)
    {
      GtkWidget *leaf = gtk_button_new ();
      GtkActionNode *leaf_node;

      gtk_box_append (GTK_BOX (branch), leaf);
      leaf_node = gtk_action_tree_add_widget (tree, leaf);
      gtk_action_node_add_route_interest (leaf_node, key);
      g_ptr_array_add (leaf_nodes, leaf_node);
    }

  g_assert_cmpuint (gtk_action_route_get_subtree_interest (
                      gtk_action_node_lookup_route (left_node, key)), ==, 256);
  g_object_ref (branch);
  gtk_box_remove (GTK_BOX (left), branch);
  gtk_box_append (GTK_BOX (right), branch);
  g_object_unref (branch);

  for (i = 0; i < leaf_nodes->len; i++)
    gtk_action_node_sync_parent (g_ptr_array_index (leaf_nodes, i));

  g_assert_null (gtk_action_node_lookup_route (left_node, key));
  g_assert_cmpuint (gtk_action_route_get_subtree_interest (
                      gtk_action_node_lookup_route (right_node, key)), ==, 256);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  g_ptr_array_unref (leaf_nodes);
  gtk_action_tree_free (tree);
  g_object_unref (left);
  g_object_unref (right);

  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  GtkActionNode     *node;
  GtkActionProvider *provider;
  guint              changes;
  GtkActionChange    changed;
  gboolean           remove_source;
} ProviderSubscriptionState;

static void
provider_subscription_changed (GtkActionSubscription   *subscription,
                               GtkActionChange          changed,
                               const GtkActionSnapshot *snapshot,
                               gpointer                 user_data)
{
  ProviderSubscriptionState *state = user_data;

  g_assert_nonnull (subscription);
  if (snapshot->provider != NULL)
    g_assert_true (snapshot->provider == state->provider);
  state->changes++;
  state->changed |= changed;

  if (state->remove_source)
    {
      state->remove_source = FALSE;
      gtk_action_node_remove_group (state->node, "win");
    }
}

static void
test_group_provider_changes (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.mode");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action = g_simple_action_new_stateful ("mode", NULL,
                                                                  g_variant_new_string ("a"));
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *widget = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkActionNode *node = gtk_action_tree_add_widget (tree, widget);
  GtkActionProvider *provider;
  GtkActionSubscription *subscription;
  const GtkActionSnapshot *snapshot;
  ProviderSubscriptionState state = { .node = node };

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (node, "win", G_ACTION_GROUP (group));
  gtk_action_node_add_route_interest (node, key);
  provider = gtk_action_node_resolve_provider (node, key);
  g_assert_nonnull (provider);
  snapshot = gtk_action_provider_get_snapshot (provider);
  g_assert_true (snapshot->present);
  g_assert_true (snapshot->enabled);
  g_assert_cmpstr (g_variant_get_string (snapshot->state, NULL), ==, "a");
  state.provider = provider;
  subscription = gtk_action_node_subscribe (node,
                                            key,
                                            NULL,
                                            (GTK_ACTION_INTEREST_ENABLED |
                                             GTK_ACTION_INTEREST_RAW_STATE),
                                            provider_subscription_changed,
                                            &state,
                                            NULL);
  g_assert_nonnull (subscription);
  state.changes = 0;
  state.changed = GTK_ACTION_CHANGE_NONE;

  g_simple_action_set_enabled (action, FALSE);
  g_simple_action_set_state (action, g_variant_new_string ("b"));
  g_assert_cmpuint (state.changes, ==, 2);
  g_assert_true ((state.changed & GTK_ACTION_CHANGE_ENABLED) != 0);
  g_assert_true ((state.changed & GTK_ACTION_CHANGE_STATE) != 0);
  g_assert_false (gtk_action_provider_get_snapshot (provider)->enabled);
  g_assert_cmpuint (gtk_action_provider_get_revision (provider), ==, 3);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (widget);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_dynamic_shadow_and_replacement (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.save");
  GtkActionKey *unrelated = gtk_action_key_new ("win.unrelated");
  GSimpleActionGroup *parent_group = g_simple_action_group_new ();
  GSimpleActionGroup *child_group = g_simple_action_group_new ();
  GSimpleActionGroup *replacement = g_simple_action_group_new ();
  GSimpleAction *parent_action = g_simple_action_new ("save", NULL);
  GSimpleAction *child_action = g_simple_action_new ("save", NULL);
  GSimpleAction *replacement_action = g_simple_action_new ("save", NULL);
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *leaf = gtk_button_new ();
  GtkActionNode *root_node;
  GtkActionNode *leaf_node;
  GtkActionProvider *parent_provider;
  GtkActionProvider *child_provider;

  gtk_box_append (GTK_BOX (root), leaf);
  root_node = gtk_action_tree_add_widget (tree, root);
  leaf_node = gtk_action_tree_add_widget (tree, leaf);
  g_action_map_add_action (G_ACTION_MAP (parent_group), G_ACTION (parent_action));
  gtk_action_node_insert_group (root_node, "win", G_ACTION_GROUP (parent_group));
  gtk_action_node_insert_group (leaf_node, "win", G_ACTION_GROUP (child_group));
  gtk_action_node_add_route_interest (leaf_node, key);
  parent_provider = gtk_action_node_resolve_provider (leaf_node, key);
  g_assert_nonnull (parent_provider);
  g_assert_null (gtk_action_node_lookup_route (leaf_node, unrelated));

  g_action_map_add_action (G_ACTION_MAP (child_group), G_ACTION (child_action));
  child_provider = gtk_action_node_resolve_provider (leaf_node, key);
  g_assert_nonnull (child_provider);
  g_assert_true (child_provider != parent_provider);
  g_action_map_remove_action (G_ACTION_MAP (child_group), "save");
  g_assert_true (gtk_action_node_resolve_provider (leaf_node, key) == parent_provider);

  g_action_map_add_action (G_ACTION_MAP (replacement), G_ACTION (replacement_action));
  gtk_action_node_insert_group (leaf_node, "win", G_ACTION_GROUP (replacement));
  g_assert_true (gtk_action_node_resolve_provider (leaf_node, key) != parent_provider);
  g_assert_null (gtk_action_node_lookup_route (leaf_node, unrelated));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (root);

  g_clear_object (&replacement_action);
  g_clear_object (&child_action);
  g_clear_object (&parent_action);
  g_clear_object (&replacement);
  g_clear_object (&child_group);
  g_clear_object (&parent_group);
  g_clear_pointer (&unrelated, gtk_action_key_unref);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_provider_resolution_and_callback_removal (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.toggle");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action = g_simple_action_new_stateful ("toggle", NULL,
                                                                  g_variant_new_boolean (FALSE));
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *widget = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkActionNode *node = gtk_action_tree_add_widget (tree, widget);
  GtkActionProvider *provider;
  GtkActionSubscription *subscription;
  ProviderSubscriptionState state = { .node = node };
  const char *local_name = NULL;
  GStrv actions = NULL;

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (node, "win", G_ACTION_GROUP (group));
  gtk_action_node_add_route_interest (node, key);
  provider = gtk_action_node_resolve_provider (node, key);
  state.provider = provider;
  subscription = gtk_action_node_subscribe (node,
                                            key,
                                            NULL,
                                            GTK_ACTION_INTEREST_RAW_STATE,
                                            provider_subscription_changed,
                                            &state,
                                            NULL);
  g_assert_nonnull (subscription);
  state.changes = 0;
  state.changed = GTK_ACTION_CHANGE_NONE;
  state.remove_source = TRUE;

  g_assert_true (gtk_action_node_find_group (node, key, &local_name) == G_ACTION_GROUP (group));
  g_assert_cmpstr (local_name, ==, "toggle");
  g_assert_true (gtk_action_node_get_group (node, "win") == G_ACTION_GROUP (group));
  actions = gtk_action_node_list_actions (node, TRUE);
  g_assert_cmpstr (actions[0], ==, "win.toggle");
  g_assert_null (actions[1]);
  g_assert_true (gtk_action_provider_change_state (provider, g_variant_new_boolean (TRUE)));
  g_assert_cmpuint (state.changes, ==, 2);
  g_assert_null (gtk_action_node_get_group (node, "win"));
  g_assert_null (gtk_action_node_resolve_provider (node, key));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (widget);

  g_clear_pointer (&actions, g_strfreev);
  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_source_destruction (void)
{
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *widget = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkActionNode *node = gtk_action_tree_add_widget (tree, widget);
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleActionGroup *weak_group = group;

  g_object_add_weak_pointer (G_OBJECT (group), (gpointer *)&weak_group);
  gtk_action_node_insert_group (node, "win", G_ACTION_GROUP (group));
  g_object_unref (group);
  g_assert_nonnull (weak_group);

  gtk_action_node_remove (node);
  g_assert_null (weak_group);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (widget);
}

static void
test_one_off_provider_node_resolution (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.save");
  GtkActionKey *missing = gtk_action_key_new ("win.missing");
  GSimpleActionGroup *parent_group = g_simple_action_group_new ();
  GSimpleActionGroup *child_group = g_simple_action_group_new ();
  GSimpleAction *parent_action = g_simple_action_new ("save", NULL);
  GSimpleAction *child_action = g_simple_action_new ("save", NULL);
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *child = gtk_button_new ();
  GtkActionNode *root_node;
  GtkActionNode *child_node;

  gtk_box_append (GTK_BOX (root), child);
  root_node = gtk_action_tree_add_widget (tree, root);
  child_node = gtk_action_tree_add_widget (tree, child);
  g_action_map_add_action (G_ACTION_MAP (parent_group), G_ACTION (parent_action));
  gtk_action_node_insert_group (root_node, "win", G_ACTION_GROUP (parent_group));

  g_assert_true (gtk_action_node_resolve_provider_node (root_node, key) == root_node);
  g_assert_null (gtk_action_node_lookup_route (root_node, key));
  g_assert_true (gtk_action_node_resolve_provider_node (child_node, key) == root_node);
  g_assert_null (gtk_action_node_lookup_route (child_node, key));
  g_assert_null (gtk_action_node_resolve_provider_node (child_node, missing));
  g_assert_null (gtk_action_node_lookup_route (child_node, missing));

  g_action_map_add_action (G_ACTION_MAP (child_group), G_ACTION (child_action));
  gtk_action_node_insert_group (child_node, "win", G_ACTION_GROUP (child_group));
  g_assert_true (gtk_action_node_resolve_provider_node (child_node, key) == child_node);
  g_assert_null (gtk_action_node_lookup_route (child_node, key));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (root);

  g_clear_object (&child_action);
  g_clear_object (&parent_action);
  g_clear_object (&child_group);
  g_clear_object (&parent_group);
  g_clear_pointer (&missing, gtk_action_key_unref);
  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  GtkActionSubscription *cancel;
  GtkActionNode         *node;
  guint                   deliveries;
  guint                   destroy_count;
  GtkActionChange         changed;
  gboolean                cancel_self;
  gboolean                remove_group;
} SubscriptionState;

static void
subscription_destroyed (gpointer user_data)
{
  SubscriptionState *state = user_data;

  state->destroy_count++;
}

static void
subscription_changed (GtkActionSubscription   *subscription,
                      GtkActionChange          changed,
                      const GtkActionSnapshot *snapshot,
                      gpointer                 user_data)
{
  SubscriptionState *state = user_data;

  state->deliveries++;
  state->changed |= changed;

  if (state->cancel != NULL)
    {
      GtkActionSubscription *cancel = state->cancel;

      state->cancel = NULL;
      gtk_action_subscription_cancel (cancel);
    }
  if (state->cancel_self && state->deliveries > 1)
    gtk_action_subscription_cancel (subscription);
  if (state->remove_group && state->deliveries > 1)
    {
      state->remove_group = FALSE;
      gtk_action_node_remove_group (state->node, "win");
    }
}

static void
test_subscriptions (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.mode");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action = g_simple_action_new_stateful ("mode", NULL,
                                                                  g_variant_new_string ("a"));
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *widget = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkActionNode *node = gtk_action_tree_add_widget (tree, widget);
  GtkActionSubscription *first;
  GtkActionSubscription *second;
  GtkActionSubscription *self_cancelling;
  GtkActionSubscription *removing;
  SubscriptionState first_state = { 0 };
  SubscriptionState second_state = { 0 };
  SubscriptionState self_state = { .cancel_self = TRUE };
  SubscriptionState remove_state = { .node = node, .remove_group = TRUE };

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (node, "win", G_ACTION_GROUP (group));

  first = gtk_action_node_subscribe (node, key, NULL,
                                     (GTK_ACTION_INTEREST_PRESENT |
                                      GTK_ACTION_INTEREST_ENABLED |
                                      GTK_ACTION_INTEREST_RAW_STATE),
                                     subscription_changed,
                                     &first_state,
                                     subscription_destroyed);
  second = gtk_action_node_subscribe (node, key, NULL,
                                      (GTK_ACTION_INTEREST_PRESENT |
                                       GTK_ACTION_INTEREST_ENABLED |
                                       GTK_ACTION_INTEREST_RAW_STATE),
                                      subscription_changed,
                                      &second_state,
                                      subscription_destroyed);
  g_assert_nonnull (first);
  g_assert_nonnull (second);
  g_assert_cmpuint (first_state.deliveries, ==, 1);
  g_assert_cmpuint (second_state.deliveries, ==, 1);
  g_assert_true (gtk_action_subscription_get_key (first) == key);
  g_assert_true (gtk_action_subscription_get_node (first) == node);

  gtk_action_tree_begin_update (tree);
  g_simple_action_set_enabled (action, FALSE);
  g_simple_action_set_state (action, g_variant_new_string ("b"));
  gtk_action_tree_end_update (tree);
  g_assert_cmpuint (first_state.deliveries, ==, 2);
  g_assert_cmpuint (second_state.deliveries, ==, 2);
  g_assert_true ((first_state.changed & GTK_ACTION_CHANGE_ENABLED) != 0);
  g_assert_true ((first_state.changed & GTK_ACTION_CHANGE_STATE) != 0);

  second_state.cancel = first;
  g_simple_action_set_enabled (action, TRUE);
  g_assert_cmpuint (first_state.deliveries, ==, 2);
  g_assert_cmpuint (second_state.deliveries, ==, 3);
  g_assert_cmpuint (first_state.destroy_count, ==, 1);

  gtk_action_subscription_cancel (second);
  g_assert_cmpuint (second_state.destroy_count, ==, 1);

  self_cancelling = gtk_action_node_subscribe (node, key, NULL,
                                               GTK_ACTION_INTEREST_ENABLED,
                                               subscription_changed,
                                               &self_state,
                                               subscription_destroyed);
  g_assert_nonnull (self_cancelling);
  g_assert_cmpuint (self_state.deliveries, ==, 1);
  g_simple_action_set_enabled (action, FALSE);
  g_assert_cmpuint (self_state.deliveries, ==, 2);
  g_assert_cmpuint (self_state.destroy_count, ==, 1);

  removing = gtk_action_node_subscribe (node, key, NULL,
                                        (GTK_ACTION_INTEREST_PRESENT |
                                         GTK_ACTION_INTEREST_ENABLED),
                                        subscription_changed,
                                        &remove_state,
                                        subscription_destroyed);
  g_assert_nonnull (removing);
  g_simple_action_set_enabled (action, TRUE);
  g_assert_cmpuint (remove_state.deliveries, ==, 3);
  g_assert_false (gtk_action_subscription_get_snapshot (removing)->present);
  gtk_action_subscription_cancel (removing);
  g_assert_cmpuint (remove_state.destroy_count, ==, 1);
  g_assert_null (gtk_action_node_lookup_route (node, key));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);
  g_object_unref (widget);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_subscription_owner_location (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.action");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  GtkActionSubscription *subscription;
  SubscriptionState state = { 0 };

  subscription = gtk_action_node_subscribe (node, key, NULL,
                                             GTK_ACTION_INTEREST_PRESENT,
                                             subscription_changed,
                                             &state,
                                             subscription_destroyed);
  g_assert_nonnull (subscription);
  gtk_action_subscription_set_owner_location (subscription, &subscription);

  gtk_action_node_remove (node);

  g_assert_null (subscription);
  g_assert_cmpuint (state.destroy_count, ==, 1);
  gtk_action_tree_free (tree);

  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  GtkActionNode *node;
  GtkActionKey  *key;
  guint          callbacks;
  guint          destroys;
  gboolean       in_outer_callback;
} InitialCancellationState;

static void
initial_cancellation_destroyed (gpointer user_data)
{
  InitialCancellationState *state = user_data;

  g_assert_false (state->in_outer_callback);
  state->destroys++;
}

static void
cancel_initial_binding (GtkActionBinding            *binding,
                        GtkActionChange              changed,
                        const GtkActionBindingState *binding_state,
                        gpointer                     user_data)
{
  InitialCancellationState *state = user_data;

  state->callbacks++;
  gtk_action_binding_cancel (binding);
}

static void
cancel_initial_subscription (GtkActionSubscription   *subscription,
                             GtkActionChange           changed,
                             const GtkActionSnapshot  *snapshot,
                             gpointer                  user_data)
{
  InitialCancellationState *state = user_data;

  state->callbacks++;
  gtk_action_subscription_cancel (subscription);
}

static void
cancel_initial_consumers (GtkActionTree *tree,
                          gpointer       user_data)
{
  InitialCancellationState *state = user_data;
  GtkActionBinding *binding;
  GtkActionSubscription *subscription;

  state->in_outer_callback = TRUE;
  binding = gtk_action_node_bind (state->node,
                                  state->key,
                                  NULL,
                                  GTK_ACTION_INTEREST_PRESENT,
                                  cancel_initial_binding,
                                  state,
                                  initial_cancellation_destroyed);
  subscription = gtk_action_node_subscribe (state->node,
                                             state->key,
                                             NULL,
                                             GTK_ACTION_INTEREST_PRESENT,
                                             cancel_initial_subscription,
                                             state,
                                             initial_cancellation_destroyed);
  g_assert_null (binding);
  g_assert_null (subscription);
  g_assert_cmpuint (state->destroys, ==, 0);
  state->in_outer_callback = FALSE;
}

static void
test_initial_cancellation_during_dispatch (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.action");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  InitialCancellationState state = { .node = node, .key = key };

  gtk_action_tree_queue_callback (tree, cancel_initial_consumers, &state, NULL);

  g_assert_cmpuint (state.callbacks, ==, 2);
  g_assert_cmpuint (state.destroys, ==, 2);
  g_assert_true (gtk_action_tree_check_invariants (tree));
  gtk_action_tree_free (tree);

  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  guint  deliveries;
  char  *accel;
} AccelSubscriptionState;

static void
accel_subscription_changed (GtkActionSubscription   *subscription,
                            GtkActionChange          changed,
                            const GtkActionSnapshot *snapshot,
                            gpointer                 user_data)
{
  AccelSubscriptionState *state = user_data;

  g_assert_true ((changed & GTK_ACTION_CHANGE_ACCEL) != 0);
  state->deliveries++;
  g_free (state->accel);
  state->accel = g_strdup (snapshot->primary_accel);
}

static void
test_accelerator_routing (void)
{
  GtkActionKey *key = gtk_action_key_new ("app.open");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *application = gtk_action_tree_add_synthetic (tree, &tree);
  GtkActionNode *window = gtk_action_tree_add_synthetic (tree, &application);
  GtkActionNode *widget = gtk_action_tree_add_synthetic (tree, &window);
  GtkActionNode *other = gtk_action_tree_add_synthetic (tree, &widget);
  GtkActionSubscription *plain;
  GtkActionSubscription *targeted;
  GtkActionSubscription *uninterested;
  AccelSubscriptionState plain_state = { 0 };
  AccelSubscriptionState targeted_state = { 0 };
  SubscriptionState uninterested_state = { 0 };

  gtk_action_node_set_synthetic_parent (window, application);
  gtk_action_node_set_synthetic_parent (widget, window);
  gtk_action_node_set_primary_accel (application, key, NULL, "<Control>O");
  gtk_action_node_set_primary_accel (application, key,
                                     g_variant_new_string ("recent"),
                                     "<Control>R");

  plain = gtk_action_node_subscribe (widget, key, NULL,
                                     GTK_ACTION_INTEREST_ACCEL,
                                     accel_subscription_changed,
                                     &plain_state, NULL);
  targeted = gtk_action_node_subscribe (widget, key,
                                        g_variant_new_string ("recent"),
                                        GTK_ACTION_INTEREST_ACCEL,
                                        accel_subscription_changed,
                                        &targeted_state, NULL);
  uninterested = gtk_action_node_subscribe (widget, key, NULL,
                                            GTK_ACTION_INTEREST_PRESENT,
                                            subscription_changed,
                                            &uninterested_state, NULL);
  g_assert_cmpstr (plain_state.accel, ==, "<Control>O");
  g_assert_cmpstr (targeted_state.accel, ==, "<Control>R");
  g_assert_cmpuint (uninterested_state.deliveries, ==, 1);

  gtk_action_node_set_primary_accel (window, key, NULL, "<Alt>O");
  g_assert_cmpstr (plain_state.accel, ==, "<Alt>O");
  g_assert_cmpuint (plain_state.deliveries, ==, 2);
  g_assert_cmpuint (targeted_state.deliveries, ==, 1);
  g_assert_cmpuint (uninterested_state.deliveries, ==, 1);
  gtk_action_node_set_primary_accel (window, key, NULL, "<Alt>O");
  g_assert_cmpuint (plain_state.deliveries, ==, 2);

  gtk_action_node_set_primary_accel (application, key, NULL, "<Shift>O");
  g_assert_cmpuint (plain_state.deliveries, ==, 2);
  gtk_action_node_set_primary_accel (window, key, NULL, NULL);
  g_assert_cmpstr (plain_state.accel, ==, "<Shift>O");
  g_assert_cmpuint (plain_state.deliveries, ==, 3);

  gtk_action_node_set_synthetic_parent (widget, other);
  g_assert_null (plain_state.accel);
  g_assert_null (targeted_state.accel);
  g_assert_cmpuint (plain_state.deliveries, ==, 4);
  g_assert_cmpuint (targeted_state.deliveries, ==, 2);

  gtk_action_subscription_cancel (plain);
  gtk_action_subscription_cancel (targeted);
  gtk_action_subscription_cancel (uninterested);
  g_clear_pointer (&plain_state.accel, g_free);
  g_clear_pointer (&targeted_state.accel, g_free);
  g_assert_true (gtk_action_tree_check_invariants (tree));
  gtk_action_tree_free (tree);

  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  GtkActionBinding *binding;
  guint             deliveries;
  guint             activations;
  guint             destroys;
  GtkActionChange   changed;
  gboolean          activate_on_change;
} BindingState;

static void
binding_destroyed (gpointer user_data)
{
  BindingState *state = user_data;

  state->destroys++;
}

static void
binding_changed (GtkActionBinding            *binding,
                 GtkActionChange              changed,
                 const GtkActionBindingState *binding_state,
                 gpointer                     user_data)
{
  BindingState *state = user_data;

  state->binding = binding;
  state->deliveries++;
  state->changed |= changed;
  g_assert_nonnull (binding_state);

  if (state->activate_on_change && state->deliveries > 1 && binding_state->enabled)
    {
      state->activate_on_change = FALSE;
      g_assert_true (gtk_action_binding_activate (binding));
    }
}

static void
binding_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  BindingState *state = user_data;

  state->activations++;
  g_assert_cmpstr (g_variant_get_string (parameter, NULL), ==, "a");
}

static void
test_binding_accelerator_ownership (void)
{
  GtkActionKey *key = gtk_action_key_new ("app.open");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action = g_simple_action_new ("open", NULL);
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  GtkActionBinding *binding;
  const GtkActionBindingState *state;
  const char *node_accel;
  BindingState binding_state = { 0 };

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (node, "app", G_ACTION_GROUP (group));
  gtk_action_node_set_primary_accel (node, key, NULL, "<Control>O");

  binding = gtk_action_node_bind (node, key, NULL,
                                  (GTK_ACTION_INTEREST_ENABLED |
                                   GTK_ACTION_INTEREST_ACCEL),
                                  binding_changed, &binding_state, NULL);
  g_assert_nonnull (binding);

  state = gtk_action_binding_get_state (binding);
  node_accel = gtk_action_node_get_primary_accel (node, key, NULL);
  g_assert_cmpstr (state->primary_accel, ==, "<Control>O");
  g_assert_true (state->primary_accel != node_accel);

  g_simple_action_set_enabled (action, FALSE);
  state = gtk_action_binding_get_state (binding);
  node_accel = gtk_action_node_get_primary_accel (node, key, NULL);
  g_assert_cmpstr (state->primary_accel, ==, "<Control>O");
  g_assert_true (state->primary_accel != node_accel);

  gtk_action_node_set_primary_accel (node, key, NULL, "<Control>P");
  state = gtk_action_binding_get_state (binding);
  node_accel = gtk_action_node_get_primary_accel (node, key, NULL);
  g_assert_cmpstr (state->primary_accel, ==, "<Control>P");
  g_assert_true (state->primary_accel != node_accel);

  gtk_action_binding_cancel (binding);
  gtk_action_tree_free (tree);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_binding_owner_location (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.action");
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  GtkActionBinding *binding;
  BindingState state = { 0 };

  binding = gtk_action_node_bind (node, key, NULL,
                                  GTK_ACTION_INTEREST_PRESENT,
                                  binding_changed,
                                  &state,
                                  binding_destroyed);
  g_assert_nonnull (binding);
  gtk_action_binding_set_owner_location (binding, &binding);

  gtk_action_node_remove (node);

  g_assert_null (binding);
  g_assert_cmpuint (state.destroys, ==, 1);
  gtk_action_tree_free (tree);

  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_model_button_action_teardown (void)
{
  GtkWidget *button = g_object_ref_sink (gtk_model_button_new ());

  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.action");
  g_object_unref (button);
}

static void
test_bindings (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.mode");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleActionGroup *replacement = g_simple_action_group_new ();
  GSimpleAction *action =
    g_simple_action_new_stateful ("mode", G_VARIANT_TYPE_STRING,
                                  g_variant_new_string ("a"));
  GSimpleAction *replacement_action =
    g_simple_action_new_stateful ("mode", NULL, g_variant_new_boolean (TRUE));
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkWidget *root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *leaf = gtk_button_new ();
  GtkActionNode *root_node;
  GtkActionNode *leaf_node;
  GtkActionBinding *binding;
  const GtkActionBindingState *derived;
  BindingState state = { 0 };
  guint key_pool_size;
  guint64 route_revision;
  guint i;

  gtk_box_append (GTK_BOX (root), leaf);
  root_node = gtk_action_tree_add_widget (tree, root);
  leaf_node = gtk_action_tree_add_widget (tree, leaf);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_action_map_add_action (G_ACTION_MAP (replacement), G_ACTION (replacement_action));
  g_signal_connect (action, "activate", G_CALLBACK (binding_activated), &state);
  gtk_action_node_insert_group (root_node, "win", G_ACTION_GROUP (group));

  binding = gtk_action_node_bind (leaf_node, key, g_variant_new_string ("a"),
                                  (GTK_ACTION_INTEREST_PRESENT |
                                   GTK_ACTION_INTEREST_ENABLED |
                                   GTK_ACTION_INTEREST_ACTIVE |
                                   GTK_ACTION_INTEREST_ROLE),
                                  binding_changed, &state, binding_destroyed);
  g_assert_nonnull (binding);
  g_assert_cmpuint (state.deliveries, ==, 1);
  derived = gtk_action_binding_get_state (binding);
  g_assert_true (derived->present);
  g_assert_true (derived->activatable);
  g_assert_true (derived->enabled);
  g_assert_true (derived->active);
  g_assert_cmpint (derived->role, ==, GTK_BUTTON_ROLE_RADIO);
  key_pool_size = gtk_action_key_pool_get_size ();
  route_revision = gtk_action_route_get_revision (
    gtk_action_node_lookup_route (leaf_node, key));
  for (i = 0; i < 100; i++)
    g_assert_true (gtk_action_binding_activate (binding));
  g_assert_cmpuint (state.activations, ==, 100);
  g_assert_cmpuint (gtk_action_key_pool_get_size (), ==, key_pool_size);
  g_assert_cmpuint (gtk_action_route_get_revision (
                      gtk_action_node_lookup_route (leaf_node, key)), ==, route_revision);

  gtk_action_binding_set_target (binding, g_variant_new_string ("b"));
  g_assert_false (gtk_action_binding_get_state (binding)->active);
  g_assert_true ((state.changed & GTK_ACTION_CHANGE_ACTIVE) != 0);
  gtk_action_binding_set_target (binding, g_variant_new_int32 (1));
  derived = gtk_action_binding_get_state (binding);
  g_assert_false (derived->activatable);
  g_assert_false (derived->enabled);
  g_assert_false (gtk_action_binding_activate (binding));

  gtk_action_binding_set_target (binding, g_variant_new_string ("a"));
  g_simple_action_set_enabled (action, FALSE);
  state.activate_on_change = TRUE;
  g_simple_action_set_enabled (action, TRUE);
  g_assert_cmpuint (state.activations, ==, 101);

  gtk_action_node_remove_group (root_node, "win");
  g_assert_false (gtk_action_binding_get_state (binding)->present);
  gtk_action_binding_set_target (binding, NULL);
  gtk_action_node_insert_group (root_node, "win", G_ACTION_GROUP (replacement));
  derived = gtk_action_binding_get_state (binding);
  g_assert_true (derived->present);
  g_assert_true (derived->active);
  g_assert_cmpint (derived->role, ==, GTK_BUTTON_ROLE_CHECK);

  gtk_action_binding_cancel (binding);
  g_assert_cmpuint (state.destroys, ==, 1);
  g_assert_null (gtk_action_node_lookup_route (leaf_node, key));
  g_assert_true (gtk_action_tree_check_invariants (tree));

  binding = gtk_action_node_bind (leaf_node, key, NULL,
                                  GTK_ACTION_INTEREST_PRESENT,
                                  binding_changed, &state, binding_destroyed);
  g_assert_nonnull (binding);

  gtk_action_tree_free (tree);
  g_assert_cmpuint (state.destroys, ==, 2);
  g_object_unref (root);

  g_clear_object (&replacement_action);
  g_clear_object (&action);
  g_clear_object (&replacement);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  GtkActionBinding *cancel;
  guint             deliveries;
  gboolean          cancel_on_change;
} TargetBindingState;

static void
target_binding_changed (GtkActionBinding            *binding,
                        GtkActionChange              changed,
                        const GtkActionBindingState *binding_state,
                        gpointer                     user_data)
{
  TargetBindingState *state = user_data;

  state->deliveries++;

  if (state->cancel_on_change && state->cancel != NULL)
    {
      state->cancel_on_change = FALSE;
      g_clear_pointer (&state->cancel, gtk_action_binding_cancel);
    }
}

static void
test_target_indexes (void)
{
  GtkActionKey *key = gtk_action_key_new ("win.mode");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action =
    g_simple_action_new_stateful ("mode", G_VARIANT_TYPE_STRING,
                                  g_variant_new_string ("target-0"));
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  GtkActionProvider *provider;
  GtkActionBinding *bindings[7];
  TargetBindingState states[7] = { 0 };
  guint i;

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (node, "win", G_ACTION_GROUP (group));

  for (i = 0; i < 6; i++)
    {
      char target[32];

      g_snprintf (target, sizeof target, "target-%u", i);
      bindings[i] = gtk_action_node_bind (node, key, g_variant_new_string (target),
                                          GTK_ACTION_INTEREST_ACTIVE,
                                          target_binding_changed, &states[i], NULL);
    }

  provider = gtk_action_node_resolve_provider (node, key);
  g_assert_nonnull (provider);
  g_assert_cmpuint (gtk_action_provider_get_target_count (provider), ==, 6);
  g_assert_true (gtk_action_provider_has_target_index (provider));

  gtk_action_provider_reset_touched_bindings (provider);
  g_simple_action_set_state (action, g_variant_new_string ("target-1"));
#ifdef G_ENABLE_DEBUG
  g_assert_cmpuint (gtk_action_provider_get_touched_bindings (provider), ==, 2);
#endif
  g_assert_cmpuint (states[0].deliveries, ==, 2);
  g_assert_cmpuint (states[1].deliveries, ==, 2);
  g_assert_cmpuint (states[2].deliveries, ==, 1);

  gtk_action_binding_set_target (bindings[5], g_variant_new_string ("target-1"));
  g_assert_cmpuint (gtk_action_provider_get_target_count (provider), ==, 5);

  bindings[6] = gtk_action_node_bind (node, key, g_variant_new_int32 (1),
                                      GTK_ACTION_INTEREST_ACTIVE,
                                      target_binding_changed, &states[6], NULL);
  gtk_action_provider_reset_touched_bindings (provider);
  g_simple_action_set_state (action, g_variant_new_string ("target-2"));
#ifdef G_ENABLE_DEBUG
  g_assert_cmpuint (gtk_action_provider_get_touched_bindings (provider), ==, 3);
#endif
  g_assert_cmpuint (states[6].deliveries, ==, 1);

  gtk_action_provider_reset_touched_bindings (provider);
  g_simple_action_set_state (action, g_variant_new_string ("target-2"));
#ifdef G_ENABLE_DEBUG
  g_assert_cmpuint (gtk_action_provider_get_touched_bindings (provider), ==, 0);
#endif

  states[2].cancel = bindings[3];
  states[2].cancel_on_change = TRUE;
  bindings[3] = NULL;
  g_simple_action_set_state (action, g_variant_new_string ("target-3"));
  g_assert_null (states[2].cancel);

  for (i = 0; i < G_N_ELEMENTS (bindings); i++)
    if (bindings[i] != NULL)
      gtk_action_binding_cancel (bindings[i]);
  g_assert_true (gtk_action_tree_check_invariants (tree));
  gtk_action_tree_free (tree);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_resolution_lifetime (void)
{
  GtkActionKey *key = gtk_action_key_new ("app.mode");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action =
    g_simple_action_new_stateful ("mode", G_VARIANT_TYPE_STRING,
                                  g_variant_new_string ("initial"));
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  GtkActionResolution resolution = GTK_ACTION_RESOLUTION_INIT;
  const GVariantType *parameter_type = NULL;
  const GVariantType *state_type = NULL;
  GVariant *state_hint = NULL;
  GVariant *state = NULL;
  gboolean enabled = FALSE;

  g_simple_action_set_state_hint (action, g_variant_new_strv ((const char *[]) { "initial", "other", NULL }, -1));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (node, "app", G_ACTION_GROUP (group));

  g_assert_true (gtk_action_resolution_init (&resolution, node, key));
  g_assert_true (gtk_action_resolution_query (&resolution, &enabled,
                                              &parameter_type, &state_type,
                                              &state_hint, &state));
  g_assert_true (enabled);
  g_assert_true (g_variant_type_equal (parameter_type, G_VARIANT_TYPE_STRING));
  g_assert_true (g_variant_type_equal (state_type, G_VARIANT_TYPE_STRING));
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "initial");
  g_assert_nonnull (gtk_action_node_lookup_route (node, key));

  gtk_action_resolution_clear (&resolution);
  g_assert_null (gtk_action_node_lookup_route (node, key));
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "initial");
  g_assert_cmpuint (g_variant_n_children (state_hint), ==, 2);

  g_variant_unref (state_hint);
  g_variant_unref (state);

  g_assert_true (gtk_action_resolution_init (&resolution, node, key));
  gtk_action_node_remove (node);
  g_assert_null (gtk_action_resolution_get_provider_node (&resolution));
  gtk_action_resolution_clear (&resolution);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

typedef struct
{
  GtkActionNode *node;
  guint          activations;
} ResolutionRemovalState;

static void
remove_resolution_node (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ResolutionRemovalState *state = user_data;

  state->activations++;
  g_clear_pointer (&state->node, gtk_action_node_remove);
}

static void
test_resolution_reentrant_node_removal (void)
{
  GtkActionKey *key = gtk_action_key_new ("app.remove");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action = g_simple_action_new ("remove", NULL);
  GtkActionTree *tree = gtk_action_tree_new ();
  ResolutionRemovalState state = { 0 };

  state.node = gtk_action_tree_add_synthetic (tree, tree);
  g_signal_connect (action,
                    "activate",
                    G_CALLBACK (remove_resolution_node),
                    &state);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_action_node_insert_group (state.node, "app", G_ACTION_GROUP (group));

  g_assert_true (gtk_action_node_activate (state.node, key, NULL));
  g_assert_null (state.node);
  g_assert_cmpuint (state.activations, ==, 1);
  g_assert_true (gtk_action_tree_check_invariants (tree));

  gtk_action_tree_free (tree);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
menu_tracker_insert (GtkMenuTrackerItem *item,
                     int                 position,
                     gpointer            user_data)
{
  GPtrArray *items = user_data;

  g_ptr_array_insert (items, position, g_object_ref (item));
}

static void
menu_tracker_remove (int      position,
                     gpointer user_data)
{
  GPtrArray *items = user_data;

  g_ptr_array_remove_index (items, position);
}

static void
menu_action_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  int *activated = user_data;

  (*activated)++;
}

static void
test_menu_tracker_node_context (void)
{
  GtkActionKey *run_key = gtk_action_key_new ("win.run");
  GtkActionKey *open_key = gtk_action_key_new ("win.submenu-open");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *run = g_simple_action_new ("run", NULL);
  GSimpleAction *open =
    g_simple_action_new_stateful ("submenu-open", NULL, g_variant_new_boolean (FALSE));
  GMenu *menu = g_menu_new ();
  GMenu *submenu = g_menu_new ();
  GMenuItem *submenu_item = NULL;
  GPtrArray *items = g_ptr_array_new_with_free_func (g_object_unref);
  GtkActionTree *tree = gtk_action_tree_new ();
  GtkActionNode *node = gtk_action_tree_add_synthetic (tree, tree);
  GtkMenuTracker *tracker;
  GVariant *state = NULL;
  int activated = 0;

  g_signal_connect (run, "activate", G_CALLBACK (menu_action_activated), &activated);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (run));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (open));
  gtk_action_node_insert_group (node, "win", G_ACTION_GROUP (group));

  g_menu_append (menu, "Run", "win.run");
  g_menu_append (submenu, "Child", "win.run");
  submenu_item = g_menu_item_new_submenu ("Submenu", G_MENU_MODEL (submenu));
  g_menu_item_set_attribute (submenu_item, "submenu-action", "s", "win.submenu-open");
  g_menu_append_item (menu, submenu_item);

  tracker = gtk_menu_tracker_new (node, G_MENU_MODEL (menu), FALSE, TRUE, FALSE, NULL,
                                  menu_tracker_insert, menu_tracker_remove, items);
  g_assert_cmpuint (items->len, ==, 2);
  gtk_menu_tracker_item_activated (g_ptr_array_index (items, 0));
  g_assert_cmpint (activated, ==, 1);

  gtk_menu_tracker_item_request_submenu_shown (g_ptr_array_index (items, 1), TRUE);
  g_assert_true (gtk_menu_tracker_item_get_submenu_shown (g_ptr_array_index (items, 1)));
  state = g_action_group_get_action_state (G_ACTION_GROUP (group), "submenu-open");
  g_assert_true (g_variant_get_boolean (state));
  gtk_menu_tracker_item_request_submenu_shown (g_ptr_array_index (items, 1), FALSE);

  gtk_menu_tracker_free (tracker);
  g_ptr_array_set_size (items, 0);
  g_assert_cmpuint (items->len, ==, 0);
  g_assert_null (gtk_action_node_lookup_route (node, run_key));
  g_assert_null (gtk_action_node_lookup_route (node, open_key));
  gtk_action_tree_free (tree);

  g_clear_pointer (&state, g_variant_unref);
  g_clear_pointer (&items, g_ptr_array_unref);
  g_clear_object (&submenu_item);
  g_clear_object (&submenu);
  g_clear_object (&menu);
  g_clear_object (&open);
  g_clear_object (&run);
  g_clear_object (&group);
  g_clear_pointer (&open_key, gtk_action_key_unref);
  g_clear_pointer (&run_key, gtk_action_key_unref);
}

static void
test_inspector_object_swap (void)
{
  GtkActionKey *key = gtk_action_key_new ("test.action");
  GSimpleActionGroup *first_group = g_simple_action_group_new ();
  GSimpleActionGroup *second_group = g_simple_action_group_new ();
  GSimpleAction *first_action = g_simple_action_new ("action", NULL);
  GSimpleAction *second_action = g_simple_action_new ("action", NULL);
  GtkWidget *first = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  GtkWidget *second = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  GtkWidget *button = g_object_ref_sink (gtk_button_new ());
  GtkWidget *stack = g_object_ref_sink (gtk_stack_new ());
  GtkInspectorActions *inspector;
  GtkActionNode *first_node;
  GtkActionNode *second_node;

  g_action_map_add_action (G_ACTION_MAP (first_group), G_ACTION (first_action));
  g_action_map_add_action (G_ACTION_MAP (second_group), G_ACTION (second_action));
  gtk_widget_insert_action_group (first, "test", G_ACTION_GROUP (first_group));
  gtk_widget_insert_action_group (second, "test", G_ACTION_GROUP (second_group));
  first_node = _gtk_widget_get_action_node (first, FALSE);
  second_node = _gtk_widget_get_action_node (second, FALSE);

  inspector = g_object_new (GTK_TYPE_INSPECTOR_ACTIONS, "button", button, NULL);
  gtk_stack_add_child (GTK_STACK (stack), GTK_WIDGET (inspector));

  gtk_inspector_actions_set_object (inspector, G_OBJECT (first));
  g_assert_nonnull (gtk_action_node_lookup_route (first_node, key));
  gtk_inspector_actions_set_object (inspector, G_OBJECT (second));
  g_assert_null (gtk_action_node_lookup_route (first_node, key));
  g_assert_nonnull (gtk_action_node_lookup_route (second_node, key));
  gtk_inspector_actions_set_object (inspector, NULL);
  g_assert_null (gtk_action_node_lookup_route (second_node, key));

  g_object_unref (stack);
  g_object_unref (button);
  g_object_unref (first);
  g_object_unref (second);

  g_clear_object (&second_action);
  g_clear_object (&first_action);
  g_clear_object (&second_group);
  g_clear_object (&first_group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_inspector_inherited_actions (void)
{
  GtkActionKey *key = gtk_action_key_new ("test.action");
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GSimpleAction *action = g_simple_action_new ("action", NULL);
  GtkWidget *parent = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  GtkWidget *child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *button = g_object_ref_sink (gtk_button_new ());
  GtkWidget *stack = g_object_ref_sink (gtk_stack_new ());
  GtkInspectorActions *inspector;
  GtkActionNode *child_node;

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_widget_insert_action_group (parent, "test", G_ACTION_GROUP (group));
  gtk_box_append (GTK_BOX (parent), child);
  g_assert_null (_gtk_widget_get_action_node (child, FALSE));

  inspector = g_object_new (GTK_TYPE_INSPECTOR_ACTIONS, "button", button, NULL);
  gtk_stack_add_child (GTK_STACK (stack), GTK_WIDGET (inspector));

  gtk_inspector_actions_set_object (inspector, G_OBJECT (child));
  child_node = _gtk_widget_get_action_node (child, FALSE);
  g_assert_nonnull (child_node);
  g_assert_nonnull (gtk_action_node_lookup_route (child_node, key));

  gtk_inspector_actions_set_object (inspector, NULL);
  g_assert_null (gtk_action_node_lookup_route (child_node, key));

  g_object_unref (stack);
  g_object_unref (button);
  g_object_unref (parent);

  g_clear_object (&action);
  g_clear_object (&group);
  g_clear_pointer (&key, gtk_action_key_unref);
}

#define N_MODEL_SCOPES 3
#define N_MODEL_ACTIONS 2
#define N_MODEL_BINDINGS 4

typedef struct
{
  GtkWidget          *widget;
  GtkActionNode      *node;
  GSimpleActionGroup *group;
  GSimpleAction      *actions[N_MODEL_ACTIONS];
  char               *accels[N_MODEL_ACTIONS][2];
  gboolean            attached;
} ReferenceScope;

typedef struct
{
  GtkActionBinding *binding;
  GtkActionKey     *key;
  GVariant         *target;
  guint             deliveries;
} ReferenceBinding;

typedef struct
{
  GtkActionTree    *tree;
  GtkWidget        *root;
  GtkWidget        *left;
  GtkWidget        *right;
  GtkWidget        *leaf;
  GtkActionNode    *leaf_node;
  ReferenceScope    scopes[N_MODEL_SCOPES];
  ReferenceBinding  bindings[N_MODEL_BINDINGS];
  GtkActionKey     *keys[N_MODEL_ACTIONS];
  GRand            *rand;
} ReferenceModel;

static void
reference_binding_changed (GtkActionBinding            *binding,
                           GtkActionChange              changed,
                           const GtkActionBindingState *state,
                           gpointer                     user_data)
{
  ReferenceBinding *model_binding = user_data;

  g_assert_nonnull (binding);
  g_assert_cmpint (changed, !=, GTK_ACTION_CHANGE_NONE);
  g_assert_nonnull (state);
  model_binding->deliveries++;
}

static ReferenceScope *
reference_model_find_scope (ReferenceModel *model,
                            GtkWidget      *widget)
{
  guint i;

  for (i = 0; i < N_MODEL_SCOPES; i++)
    {
      if (model->scopes[i].widget == widget)
        return &model->scopes[i];
    }

  return NULL;
}

static ReferenceScope *
reference_model_resolve (ReferenceModel        *model,
                         ReferenceBinding      *binding,
                         gboolean              *enabled,
                         const GVariantType   **parameter_type,
                         const GVariantType   **state_type,
                         GVariant             **state_hint,
                         GVariant             **state)
{
  const char *local_name = gtk_action_key_get_local_name (binding->key);
  GtkWidget *widget;

  for (widget = model->leaf; widget != NULL; widget = gtk_widget_get_parent (widget))
    {
      ReferenceScope *scope = reference_model_find_scope (model, widget);

      if (scope != NULL && scope->attached &&
          g_action_group_query_action (G_ACTION_GROUP (scope->group),
                                       local_name,
                                       enabled,
                                       parameter_type,
                                       state_type,
                                       state_hint,
                                       state))
        return scope;
    }

  return NULL;
}

static const char *
reference_model_get_accel (ReferenceModel   *model,
                           ReferenceBinding *binding)
{
  guint action_index = g_str_equal (gtk_action_key_get_full_name (binding->key),
                                    "win.mode-0") ? 0 : 1;
  guint target_index = binding->target != NULL &&
                       g_str_equal (g_variant_get_string (binding->target, NULL), "b");
  GtkWidget *widget;

  for (widget = model->leaf; widget != NULL; widget = gtk_widget_get_parent (widget))
    {
      ReferenceScope *scope = reference_model_find_scope (model, widget);

      if (scope != NULL && scope->accels[action_index][target_index] != NULL)
        return scope->accels[action_index][target_index];
    }

  return NULL;
}

static gboolean
variant_type_equal0 (const GVariantType *a,
                     const GVariantType *b)
{
  return a == b || (a != NULL && b != NULL && g_variant_type_equal (a, b));
}

static gboolean
variant_equal0 (GVariant *a,
                GVariant *b)
{
  return a == b || (a != NULL && b != NULL && g_variant_equal (a, b));
}

static void
reference_model_assert_binding (ReferenceModel   *model,
                                ReferenceBinding *binding)
{
  GtkActionResolution resolution = GTK_ACTION_RESOLUTION_INIT;
  const GtkActionBindingState *actual = gtk_action_binding_get_state (binding->binding);
  const GVariantType *parameter_type = NULL;
  const GVariantType *actual_parameter_type = NULL;
  const GVariantType *state_type = NULL;
  const GVariantType *actual_state_type = NULL;
  GVariant *state_hint = NULL;
  GVariant *actual_state_hint = NULL;
  GVariant *state = NULL;
  GVariant *actual_state = NULL;
  ReferenceScope *provider;
  gboolean present;
  gboolean enabled = FALSE;
  gboolean actual_enabled = FALSE;
  gboolean activatable;
  gboolean active = FALSE;
  GtkButtonRole role = GTK_BUTTON_ROLE_NORMAL;
  const char *accel;

  provider = reference_model_resolve (model, binding, &enabled,
                                      &parameter_type, &state_type,
                                      &state_hint, &state);
  present = provider != NULL;
  activatable = present && parameter_type != NULL && binding->target != NULL &&
                g_variant_is_of_type (binding->target, parameter_type);
  if (activatable && state != NULL)
    {
      active = g_variant_equal (state, binding->target);
      role = GTK_BUTTON_ROLE_RADIO;
    }
  accel = reference_model_get_accel (model, binding);

  g_assert_cmpint (actual->present, ==, present);
  g_assert_cmpint (actual->activatable, ==, activatable);
  g_assert_cmpint (actual->enabled, ==, activatable && enabled);
  g_assert_cmpint (actual->active, ==, active);
  g_assert_cmpint (actual->role, ==, role);
  g_assert_cmpstr (actual->primary_accel, ==, accel);
  g_assert_cmpstr (gtk_action_node_get_primary_accel (
                     model->leaf_node, binding->key, binding->target), ==, accel);

  if (gtk_action_resolution_init (&resolution,
                                  model->leaf_node,
                                  binding->key))
    {
      g_assert_true (present);
      g_assert_true (gtk_action_resolution_get_provider_node (&resolution) == provider->node);
      g_assert_true (gtk_action_resolution_query (&resolution, &actual_enabled,
                                                  &actual_parameter_type,
                                                  &actual_state_type,
                                                  &actual_state_hint,
                                                  &actual_state));
      g_assert_cmpint (actual_enabled, ==, enabled);
      g_assert_true (variant_type_equal0 (actual_parameter_type, parameter_type));
      g_assert_true (variant_type_equal0 (actual_state_type, state_type));
      g_assert_true (variant_equal0 (actual_state_hint, state_hint));
      g_assert_true (variant_equal0 (actual_state, state));
    }
  else
    g_assert_false (present);

  g_clear_pointer (&actual_state, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);
  g_clear_pointer (&actual_state_hint, g_variant_unref);
  g_clear_pointer (&state_hint, g_variant_unref);
  gtk_action_resolution_clear (&resolution);
}

static GSimpleAction *
reference_action_new (guint action_index,
                      guint state_index)
{
  GSimpleAction *action;
  char name[16];
  char state[16];

  g_snprintf (name, sizeof name, "mode-%u", action_index);
  g_snprintf (state, sizeof state, "%c", 'a' + state_index);
  action = g_simple_action_new_stateful (name, G_VARIANT_TYPE_STRING,
                                         g_variant_new_string (state));

  return action;
}

static void
reference_scope_replace_group (ReferenceScope *scope)
{
  GSimpleActionGroup *group = g_simple_action_group_new ();
  guint i;

  for (i = 0; i < N_MODEL_ACTIONS; i++)
    {
      if (scope->actions[i] != NULL &&
          g_action_group_has_action (G_ACTION_GROUP (scope->group),
                                     g_action_get_name (G_ACTION (scope->actions[i]))))
        {
          GVariant *state = g_action_get_state (G_ACTION (scope->actions[i]));
          GSimpleAction *replacement =
            g_simple_action_new_stateful (g_action_get_name (G_ACTION (scope->actions[i])),
                                           G_VARIANT_TYPE_STRING,
                                           state);

          g_simple_action_set_enabled (replacement,
                                       g_action_get_enabled (G_ACTION (scope->actions[i])));
          g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (replacement));
          g_clear_object (&scope->actions[i]);
          scope->actions[i] = replacement;

          g_clear_pointer (&state, g_variant_unref);
        }
      else
        g_clear_object (&scope->actions[i]);
    }

  g_clear_object (&scope->group);
  scope->group = group;
  if (scope->attached)
    gtk_action_node_insert_group (scope->node, "win", G_ACTION_GROUP (group));
}

static void
reference_binding_rebind (ReferenceModel   *model,
                          ReferenceBinding *binding,
                          guint             action_index)
{
  if (binding->binding != NULL)
    gtk_action_binding_cancel (binding->binding);
  g_clear_pointer (&binding->key, gtk_action_key_unref);
  binding->key = gtk_action_key_ref (model->keys[action_index]);
  binding->binding =
    gtk_action_node_bind (model->leaf_node,
                          binding->key,
                          binding->target,
                          (GTK_ACTION_INTEREST_PRESENT |
                           GTK_ACTION_INTEREST_ENABLED |
                           GTK_ACTION_INTEREST_ACTIVE |
                           GTK_ACTION_INTEREST_ROLE |
                           GTK_ACTION_INTEREST_ACCEL |
                           GTK_ACTION_INTEREST_RAW_STATE),
                          reference_binding_changed,
                          binding,
                          NULL);
  g_assert_nonnull (binding->binding);
}

static void
reference_model_assert (ReferenceModel *model)
{
  guint i;

  for (i = 0; i < N_MODEL_BINDINGS; i++)
    reference_model_assert_binding (model, &model->bindings[i]);
  g_assert_true (gtk_action_tree_check_invariants (model->tree));
}

static void
reference_model_mutate (ReferenceModel *model)
{
  guint operation = g_rand_int_range (model->rand, 0, 9);
  guint scope_index = g_rand_int_range (model->rand, 0, N_MODEL_SCOPES);
  guint action_index = g_rand_int_range (model->rand, 0, N_MODEL_ACTIONS);
  guint binding_index = g_rand_int_range (model->rand, 0, N_MODEL_BINDINGS);
  ReferenceScope *scope = &model->scopes[scope_index];
  ReferenceBinding *binding = &model->bindings[binding_index];
  GSimpleAction *action = scope->actions[action_index];
  guint deliveries_before = 0;
  guint deliveries_after = 0;
  char name[16];
  guint i;

  for (i = 0; i < N_MODEL_BINDINGS; i++)
    deliveries_before += model->bindings[i].deliveries;

  g_snprintf (name, sizeof name, "mode-%u", action_index);

  switch (operation)
    {
    case 0:
      scope->attached = !scope->attached;
      if (scope->attached)
        gtk_action_node_insert_group (scope->node, "win", G_ACTION_GROUP (scope->group));
      else
        gtk_action_node_remove_group (scope->node, "win");
      break;

    case 1:
      if (action != NULL &&
          g_action_group_has_action (G_ACTION_GROUP (scope->group), name))
        g_action_map_remove_action (G_ACTION_MAP (scope->group), name);
      else
        {
          if (action == NULL)
            scope->actions[action_index] = action = reference_action_new (action_index, 0);
          g_action_map_add_action (G_ACTION_MAP (scope->group), G_ACTION (action));
        }
      break;

    case 2:
      if (action != NULL)
        g_simple_action_set_enabled (action, !g_action_get_enabled (G_ACTION (action)));
      break;

    case 3:
      if (action != NULL)
        {
          guint state_index = g_rand_int_range (model->rand, 0, 2);

          g_simple_action_set_state (action,
                                     g_variant_new_string (state_index == 0 ? "a" : "b"));
        }
      break;

    case 4:
      if (gtk_widget_get_parent (model->leaf) != NULL)
        gtk_widget_unparent (model->leaf);
      if (scope_index == 0)
        gtk_box_append (GTK_BOX (model->left), model->leaf);
      else if (scope_index == 1)
        gtk_box_append (GTK_BOX (model->right), model->leaf);
      gtk_action_node_sync_parent (model->leaf_node);
      break;

    case 5:
      g_clear_pointer (&binding->target, g_variant_unref);
      binding->target = g_variant_ref_sink (
        g_variant_new_string (g_rand_boolean (model->rand) ? "a" : "b"));
      gtk_action_binding_set_target (binding->binding, binding->target);
      break;

    case 6:
      {
        guint target_index = g_rand_int_range (model->rand, 0, 2);
        const char *accel = g_rand_boolean (model->rand) ? "<Control>M" : NULL;
        GVariant *target = g_variant_new_string (target_index == 0 ? "a" : "b");

        g_free (scope->accels[action_index][target_index]);
        scope->accels[action_index][target_index] = g_strdup (accel);
        gtk_action_node_set_primary_accel (scope->node, model->keys[action_index],
                                           target, accel);
      }
      break;

    case 7:
      reference_scope_replace_group (scope);
      for (i = 0; i < N_MODEL_BINDINGS; i++)
        deliveries_after += model->bindings[i].deliveries;
      g_assert_cmpuint (deliveries_after, ==, deliveries_before);
      break;

    case 8:
      reference_binding_rebind (model, binding,
                                g_str_equal (gtk_action_key_get_full_name (binding->key),
                                             "win.mode-0") ? 1 : 0);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
test_seeded_reference_model (void)
{
  static const guint32 seeds[] = { 0x13579bdf, 0x2468ace0, 0xc0decafe };
  guint seed_index;

  for (seed_index = 0; seed_index < G_N_ELEMENTS (seeds); seed_index++)
    {
      ReferenceModel model = { 0 };
      guint i;
      guint j;

      model.tree = gtk_action_tree_new ();
      model.rand = g_rand_new_with_seed (seeds[seed_index]);
      model.root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
      model.left = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      model.right = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      model.leaf = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
      gtk_box_append (GTK_BOX (model.root), model.left);
      gtk_box_append (GTK_BOX (model.root), model.right);
      gtk_box_append (GTK_BOX (model.left), model.leaf);

      model.scopes[0].widget = model.root;
      model.scopes[1].widget = model.left;
      model.scopes[2].widget = model.right;
      for (i = 0; i < N_MODEL_SCOPES; i++)
        {
          ReferenceScope *scope = &model.scopes[i];

          scope->node = gtk_action_tree_add_widget (model.tree, scope->widget);
          scope->group = g_simple_action_group_new ();
          scope->attached = TRUE;
          for (j = 0; j < N_MODEL_ACTIONS; j++)
            {
              scope->actions[j] = reference_action_new (j, (i + j) % 2);
              g_action_map_add_action (G_ACTION_MAP (scope->group),
                                       G_ACTION (scope->actions[j]));
            }
          gtk_action_node_insert_group (scope->node, "win", G_ACTION_GROUP (scope->group));
        }
      model.leaf_node = gtk_action_tree_add_widget (model.tree, model.leaf);
      model.keys[0] = gtk_action_key_new ("win.mode-0");
      model.keys[1] = gtk_action_key_new ("win.mode-1");

      for (i = 0; i < N_MODEL_BINDINGS; i++)
        {
          ReferenceBinding *binding = &model.bindings[i];

          binding->target = g_variant_ref_sink (g_variant_new_string ((i & 1) ? "b" : "a"));
          reference_binding_rebind (&model, binding, i % N_MODEL_ACTIONS);
        }

      reference_model_assert (&model);
      for (i = 0; i < 500; i++)
        {
          reference_model_mutate (&model);
          reference_model_assert (&model);
        }

      for (i = 0; i < N_MODEL_BINDINGS; i++)
        {
          gtk_action_binding_cancel (model.bindings[i].binding);
          gtk_action_key_unref (model.bindings[i].key);
          g_variant_unref (model.bindings[i].target);
        }
      for (i = 0; i < N_MODEL_SCOPES; i++)
        {
          for (j = 0; j < N_MODEL_ACTIONS; j++)
            {
              g_clear_object (&model.scopes[i].actions[j]);
              g_clear_pointer (&model.scopes[i].accels[j][0], g_free);
              g_clear_pointer (&model.scopes[i].accels[j][1], g_free);
            }
          g_clear_object (&model.scopes[i].group);
        }
      gtk_action_key_unref (model.keys[0]);
      gtk_action_key_unref (model.keys[1]);
      gtk_action_tree_free (model.tree);
      if (gtk_widget_get_parent (model.leaf) != NULL)
        gtk_widget_unparent (model.leaf);
      g_object_unref (model.leaf);
      g_object_unref (model.root);
      g_rand_free (model.rand);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/action-tree/sparse-insertion", test_sparse_insertion);
  g_test_add_func ("/action-tree/reparent-and-prune", test_reparent_and_prune);
  g_test_add_func ("/action-tree/synthetic-scope", test_synthetic_scope);
  g_test_add_func ("/action-tree/transactions", test_transactions);
  g_test_add_func ("/action-tree/routes/inheritance-and-shadowing",
                   test_route_inheritance_and_shadowing);
  g_test_add_func ("/action-tree/routes/identity-and-counts", test_route_identity_and_counts);
  g_test_add_func ("/action-tree/routes/reparenting", test_route_reparenting);
  g_test_add_func ("/action-tree/routes/large-subtree-move", test_large_route_subtree_move);
  g_test_add_func ("/action-tree/providers/group-changes", test_group_provider_changes);
  g_test_add_func ("/action-tree/providers/dynamic-shadow-and-replacement",
                   test_dynamic_shadow_and_replacement);
  g_test_add_func ("/action-tree/providers/resolution-and-callback-removal",
                   test_provider_resolution_and_callback_removal);
  g_test_add_func ("/action-tree/providers/source-destruction", test_source_destruction);
  g_test_add_func ("/action-tree/providers/one-off-node-resolution",
                   test_one_off_provider_node_resolution);
  g_test_add_func ("/action-tree/subscriptions/tokens-cancellation-coalescing", test_subscriptions);
  g_test_add_func ("/action-tree/subscriptions/owner-location", test_subscription_owner_location);
  g_test_add_func ("/action-tree/consumers/initial-cancellation-during-dispatch",
                   test_initial_cancellation_during_dispatch);
  g_test_add_func ("/action-tree/accelerators/routing", test_accelerator_routing);
  g_test_add_func ("/action-tree/accelerators/binding-ownership", test_binding_accelerator_ownership);
  g_test_add_func ("/action-tree/bindings/owner-location", test_binding_owner_location);
  g_test_add_func ("/action-tree/consumers/model-button-action-teardown",
                   test_model_button_action_teardown);
  g_test_add_func ("/action-tree/bindings/derived-state-target-activation", test_bindings);
  g_test_add_func ("/action-tree/bindings/target-indexes", test_target_indexes);
  g_test_add_func ("/action-tree/resolutions/transfer-and-lifetime", test_resolution_lifetime);
  g_test_add_func ("/action-tree/resolutions/reentrant-node-removal",
                   test_resolution_reentrant_node_removal);
  g_test_add_func ("/action-tree/consumers/menu-node-context", test_menu_tracker_node_context);
  g_test_add_func ("/action-tree/consumers/inspector-object-swap", test_inspector_object_swap);
  g_test_add_func ("/action-tree/consumers/inspector-inherited-actions",
                   test_inspector_inherited_actions);
  g_test_add_func ("/action-tree/reference-model/seeded-mutations", test_seeded_reference_model);

  return g_test_run ();
}
