/* action-performance.c
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

#include <gtk/gtk.h>

#ifndef N_BINDINGS
# define N_BINDINGS 10000
#endif
#ifndef N_ACTIONS
# define N_ACTIONS 100
#endif
#ifndef N_TARGETS
# define N_TARGETS 1000
#endif

typedef void (*WorkloadFunc) (void);

typedef struct
{
  const char   *name;
  WorkloadFunc func;
} Workload;

typedef struct
{
  GtkWidget parent_instance;
  gboolean active;
} ActionPerformanceWidget;

typedef GtkWidgetClass ActionPerformanceWidgetClass;

enum
{
  PROP_0,
  PROP_ACTIVE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (ActionPerformanceWidget, action_performance_widget, GTK_TYPE_WIDGET)

static void
action_performance_widget_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ActionPerformanceWidget *self = (ActionPerformanceWidget *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
action_performance_widget_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ActionPerformanceWidget *self = (ActionPerformanceWidget *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      self->active = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
action_performance_widget_class_init (ActionPerformanceWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = action_performance_widget_get_property;
  object_class->set_property = action_performance_widget_set_property;

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_install_property_action (GTK_WIDGET_CLASS (class),
                                            "bench.toggle", "active");
}

static void
action_performance_widget_init (ActionPerformanceWidget *self)
{
}

static GSimpleActionGroup *
create_group (guint n_actions)
{
  GSimpleActionGroup *group;
  guint i;

  group = g_simple_action_group_new ();
  for (i = 0; i < n_actions; i++)
    {
      GSimpleAction *action;
      char name[32];

      g_snprintf (name, sizeof name, "action-%u", i);
      action = g_simple_action_new (name, NULL);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_object_unref (action);
    }

  return group;
}

static GtkWidget *
create_bindings (GtkWidget  *parent,
                 guint       n_bindings,
                 guint       n_actions,
                 const char *prefix)
{
  GtkWidget *box;
  guint i;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (parent), box);

  for (i = 0; i < n_bindings; i++)
    {
      GtkWidget *button;
      char name[64];

      g_snprintf (name, sizeof name, "%s.action-%u", prefix, i % n_actions);
      button = gtk_button_new ();
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), name);
      gtk_box_append (GTK_BOX (box), button);
    }

  return box;
}

static void
workload_bindings (guint n_actions)
{
  GSimpleActionGroup *group;
  GtkWidget *root;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  group = create_group (n_actions);
  gtk_widget_insert_action_group (root, "app", G_ACTION_GROUP (group));
  create_bindings (root, N_BINDINGS, n_actions, "app");

  g_object_unref (root);
  g_object_unref (group);
}

static void
workload_same_name (void)
{
  workload_bindings (1);
}

static void
workload_action_names (void)
{
  workload_bindings (N_ACTIONS);
}

static void
workload_deep_shadowing (void)
{
  GPtrArray *groups;
  GtkWidget *root;
  GtkWidget *parent;
  guint i;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  parent = root;
  groups = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < N_ACTIONS; i++)
    {
      GSimpleActionGroup *group;
      GtkWidget *child;

      group = create_group (1);
      g_ptr_array_add (groups, group);
      gtk_widget_insert_action_group (parent, "app", G_ACTION_GROUP (group));
      child = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (parent), child);
      parent = child;
    }

  create_bindings (parent, N_BINDINGS, 1, "app");
  g_object_unref (root);
  g_ptr_array_unref (groups);
}

static void
workload_state_toggles (void)
{
  GSimpleActionGroup *group;
  GSimpleAction *action;
  GtkWidget *root;
  guint i;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  group = g_simple_action_group_new ();
  action = g_simple_action_new_stateful ("action-0", NULL, g_variant_new_boolean (FALSE));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_widget_insert_action_group (root, "app", G_ACTION_GROUP (group));
  create_bindings (root, N_BINDINGS, 1, "app");

  for (i = 0; i < 100; i++)
    g_simple_action_set_state (action, g_variant_new_boolean ((i & 1) != 0));

  g_object_unref (root);
  g_object_unref (action);
  g_object_unref (group);
}

static void
workload_radio_targets (void)
{
  GSimpleActionGroup *group;
  GSimpleAction *action;
  GtkWidget *root;
  guint i;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  group = g_simple_action_group_new ();
  action = g_simple_action_new_stateful ("radio", G_VARIANT_TYPE_STRING,
                                         g_variant_new_string ("target-0"));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  gtk_widget_insert_action_group (root, "app", G_ACTION_GROUP (group));

  for (i = 0; i < N_BINDINGS; i++)
    {
      GtkWidget *button;
      char target[32];

      g_snprintf (target, sizeof target, "target-%u", i % N_TARGETS);
      button = gtk_button_new ();
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.radio");
      gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "s", target);
      gtk_box_append (GTK_BOX (root), button);
    }

  for (i = 0; i < N_TARGETS; i++)
    {
      char target[32];

      g_snprintf (target, sizeof target, "target-%u", i);
      g_simple_action_set_state (action, g_variant_new_string (target));
    }

  g_object_unref (root);
  g_object_unref (action);
  g_object_unref (group);
}

static void
workload_subtree_moves (void)
{
  GSimpleActionGroup *group;
  GtkWidget *root;
  GtkWidget *left;
  GtkWidget *right;
  GtkWidget *subtree;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  left = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  right = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (root), left);
  gtk_box_append (GTK_BOX (root), right);
  group = create_group (1);
  gtk_widget_insert_action_group (root, "app", G_ACTION_GROUP (group));
  subtree = create_bindings (left, N_BINDINGS, 1, "app");

  g_object_ref (subtree);
  gtk_box_remove (GTK_BOX (left), subtree);
  gtk_box_append (GTK_BOX (right), subtree);
  g_object_unref (subtree);

  g_object_unref (root);
  g_object_unref (group);
}

static void
workload_group_replacement (void)
{
  GSimpleActionGroup *first;
  GSimpleActionGroup *second;
  GtkWidget *root;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  first = create_group (N_ACTIONS);
  second = create_group (N_ACTIONS);
  gtk_widget_insert_action_group (root, "app", G_ACTION_GROUP (first));
  create_bindings (root, N_BINDINGS, N_ACTIONS, "app");
  gtk_widget_insert_action_group (root, "app", G_ACTION_GROUP (second));

  g_object_unref (root);
  g_object_unref (first);
  g_object_unref (second);
}

static void
workload_property_actions (void)
{
  GtkWidget *root;
  guint i;

  root = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  for (i = 0; i < N_BINDINGS; i++)
    {
      GtkWidget *widget;

      widget = g_object_new (action_performance_widget_get_type (), NULL);
      gtk_box_append (GTK_BOX (root), widget);
      gtk_widget_activate_action (widget, "bench.toggle", NULL);
    }

  g_object_unref (root);
}

static void
run_workload (const Workload *workload,
              guint64         iterations)
{
  gint64 begin;
  gint64 elapsed;
  guint64 i;

  begin = g_get_monotonic_time ();
  for (i = 0; i < iterations; i++)
    workload->func ();
  elapsed = g_get_monotonic_time () - begin;

  g_print ("%-20s %10.3f ms\n", workload->name, elapsed / (1000.0 * iterations));
}

int
main (int   argc,
      char *argv[])
{
  static const Workload workloads[] = {
    { "same-name-bindings", workload_same_name },
    { "100-action-names", workload_action_names },
    { "deep-shadowing", workload_deep_shadowing },
    { "state-toggles", workload_state_toggles },
    { "radio-targets", workload_radio_targets },
    { "subtree-moves", workload_subtree_moves },
    { "group-replacement", workload_group_replacement },
    { "property-actions", workload_property_actions },
  };
  guint64 iterations = 1;
  guint i;

  gtk_init_check ();

  if (argc > 3)
    {
      g_printerr ("Usage: %s [workload [iterations]]\n", argv[0]);
      return 1;
    }

  if (argc > 2 &&
      (!g_ascii_string_to_unsigned (argv[2], 10, 1, G_MAXUINT, &iterations, NULL)))
    {
      g_printerr ("Invalid iteration count: %s\n", argv[2]);
      return 1;
    }

  for (i = 0; i < G_N_ELEMENTS (workloads); i++)
    {
      if (argc > 1 &&
          !g_str_equal (argv[1], "all") &&
          !g_str_equal (argv[1], workloads[i].name))
        continue;

      run_workload (&workloads[i], iterations);
    }

  return 0;
}
