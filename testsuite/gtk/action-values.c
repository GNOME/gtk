/* action-values.c
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

#include "gtk/gtkactionkeyprivate.h"
#include "gtk/gtkactionsnapshotprivate.h"
#include "gtk/gtkactiontreeprivate.h"

static void
test_action_key (void)
{
  GtkActionKey *first = NULL;
  GtkActionKey *second = NULL;
  GtkActionKey *replacement = NULL;
  guint initial_pool_size;

  initial_pool_size = gtk_action_key_pool_get_size ();

  first = gtk_action_key_new ("win.document.save");
  second = gtk_action_key_new ("win.document.save");

  g_assert_nonnull (first);
  g_assert_true (first == second);
  g_assert_cmpstr (gtk_action_key_get_full_name (first), ==, "win.document.save");
  g_assert_cmpstr (gtk_action_key_get_local_name (first), ==, "document.save");
  g_assert_cmpuint (gtk_action_key_get_prefix_length (first), ==, 3);
  g_assert_cmpuint (gtk_action_key_hash (first), ==, g_str_hash ("win.document.save"));

  g_clear_pointer (&first, gtk_action_key_unref);
  g_clear_pointer (&second, gtk_action_key_unref);

  g_assert_cmpuint (gtk_action_key_pool_get_size (), ==, initial_pool_size);

  replacement = gtk_action_key_new ("win.document.save");
  g_assert_nonnull (replacement);

  gtk_action_key_unref (replacement);
}

static void
test_action_key_invalid (void)
{
  const char *invalid[] = {
    NULL,
    "",
    "save",
    ".save",
    "win.",
    "win.save|now",
  };

  for (guint i = 1; i < G_N_ELEMENTS (invalid); i++)
    g_assert_null (gtk_action_key_new (invalid[i]));
}

static void
test_invocation_key (void)
{
  GtkActionKey *action = gtk_action_key_new ("app.open");
  GtkActionInvocationKey *a = NULL;
  GtkActionInvocationKey *b = NULL;
  GtkActionInvocationKey *c = NULL;
  GtkActionInvocationKey *tuple_a = NULL;
  GtkActionInvocationKey *tuple_b = NULL;

  a = gtk_action_invocation_key_new (action, g_variant_new_string ("document"));
  b = gtk_action_invocation_key_new (action, g_variant_new_string ("document"));
  c = gtk_action_invocation_key_new (action, g_variant_new_string ("other"));

  g_assert_true (gtk_action_invocation_key_equal (a, b));
  g_assert_cmpuint (gtk_action_invocation_key_hash (a), ==, gtk_action_invocation_key_hash (b));
  g_assert_false (gtk_action_invocation_key_equal (a, c));
  g_assert_true (gtk_action_invocation_key_get_action (a) == action);
  g_assert_cmpvariant (gtk_action_invocation_key_get_target (a), g_variant_new_string ("document"));

  tuple_a = gtk_action_invocation_key_new (action, g_variant_new ("(si)", "page", 3));
  tuple_b = gtk_action_invocation_key_new (action, g_variant_new ("(si)", "page", 3));
  g_assert_true (gtk_action_invocation_key_equal (tuple_a, tuple_b));
  g_assert_cmpuint (gtk_action_invocation_key_hash (tuple_a), ==,
                    gtk_action_invocation_key_hash (tuple_b));

  g_clear_pointer (&tuple_b, gtk_action_invocation_key_unref);
  g_clear_pointer (&tuple_a, gtk_action_invocation_key_unref);
  g_clear_pointer (&c, gtk_action_invocation_key_unref);
  g_clear_pointer (&b, gtk_action_invocation_key_unref);
  g_clear_pointer (&a, gtk_action_invocation_key_unref);
  g_clear_pointer (&action, gtk_action_key_unref);
}

static void
test_node_structured_accels (void)
{
  GtkActionKey *key = gtk_action_key_new ("app.open");
  GVariant *first_target = g_variant_ref_sink (g_variant_new_string ("a|b"));
  GVariant *second_target = g_variant_ref_sink (g_variant_new_string ("other"));
  GtkActionNode *parent;
  GtkActionNode *child;
  int parent_owner;
  int child_owner;

  parent = gtk_action_node_new_synthetic (&parent_owner);
  child = gtk_action_node_new_synthetic (&child_owner);
  gtk_action_node_set_synthetic_parent (child, parent);

  gtk_action_node_set_primary_accel (parent, key, first_target, "<Control>O");
  gtk_action_node_set_primary_accel (parent, key, second_target, "<Control><Shift>O");

  g_assert_cmpstr (gtk_action_node_get_primary_accel (child, key, first_target), ==, "<Control>O");
  g_assert_cmpstr (gtk_action_node_get_primary_accel (child, key, second_target), ==, "<Control><Shift>O");

  gtk_action_node_set_primary_accel (parent, key, first_target, NULL);
  g_assert_null (gtk_action_node_get_primary_accel (child, key, first_target));
  g_assert_cmpstr (gtk_action_node_get_primary_accel (child, key, second_target), ==, "<Control><Shift>O");

  gtk_action_node_remove (child);
  gtk_action_node_remove (parent);

  g_clear_pointer (&second_target, g_variant_unref);
  g_clear_pointer (&first_target, g_variant_unref);
  g_clear_pointer (&key, gtk_action_key_unref);
}

static void
test_snapshot (void)
{
  GtkActionSnapshot *shared = NULL;
  GtkActionSnapshot a = GTK_ACTION_SNAPSHOT_INIT;
  GtkActionSnapshot b = GTK_ACTION_SNAPSHOT_INIT;
  int provider;

  shared = gtk_action_snapshot_new ();
  g_assert_true (gtk_action_snapshot_ref (shared) == shared);
  gtk_action_snapshot_unref (shared);

  a.parameter_type = g_variant_type_copy (G_VARIANT_TYPE_STRING);
  a.state_type = g_variant_type_copy (G_VARIANT_TYPE_STRING);
  a.state_hint = g_variant_ref_sink (g_variant_new_strv ((const char *[]) { "one", "two", NULL }, -1));
  a.state = g_variant_ref_sink (g_variant_new_string ("one"));
  a.primary_accel = g_strdup ("<Control>O");
  a.provider = &provider;
  a.revision = 7;
  a.present = TRUE;
  a.enabled = TRUE;

  gtk_action_snapshot_copy (&b, &a);
  g_assert_true (gtk_action_snapshot_equal (&a, &b));
  g_assert_true (a.parameter_type != b.parameter_type);
  g_assert_true (a.state == b.state);
  g_assert_true (a.primary_accel != b.primary_accel);

  g_clear_pointer (&b.state, g_variant_unref);
  b.state = g_variant_ref_sink (g_variant_new_string ("two"));
  b.enabled = FALSE;
  g_clear_pointer (&b.primary_accel, g_free);
  b.primary_accel = g_strdup ("<Control>T");

  g_assert_cmpuint (gtk_action_snapshot_difference (&a, &b), ==,
                    (GTK_ACTION_CHANGE_ENABLED |
                     GTK_ACTION_CHANGE_STATE |
                     GTK_ACTION_CHANGE_ACCEL));

  b.provider = NULL;
  g_clear_pointer (&b.parameter_type, g_variant_type_free);
  b.parameter_type = g_variant_type_copy (G_VARIANT_TYPE_INT32);
  g_assert_cmpuint (gtk_action_snapshot_difference (&a, &b), ==,
                    (GTK_ACTION_CHANGE_PROVIDER |
                     GTK_ACTION_CHANGE_SIGNATURE |
                     GTK_ACTION_CHANGE_ENABLED |
                     GTK_ACTION_CHANGE_STATE |
                     GTK_ACTION_CHANGE_ACCEL));

  gtk_action_snapshot_clear (&a);
  gtk_action_snapshot_clear (&b);

  g_clear_pointer (&shared, gtk_action_snapshot_unref);
}

int
main (int   argc,
      char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/action-key/canonical", test_action_key);
  g_test_add_func ("/action-key/invalid", test_action_key_invalid);
  g_test_add_func ("/action-key/invocation", test_invocation_key);
  g_test_add_func ("/action-key/node-structured-accels", test_node_structured_accels);
  g_test_add_func ("/action-snapshot/ownership-and-difference", test_snapshot);

  return g_test_run ();
}
