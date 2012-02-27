/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <gtk/gtk.h>
#include <string.h>

static void
test_scrolled_window_child_count (void)
{
  GtkWidget *sw;
  AtkObject *accessible;

  sw = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref_sink (sw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), gtk_label_new ("Bla"));

  accessible = gtk_widget_get_accessible (sw);
  g_assert_cmpint (atk_object_get_n_accessible_children (accessible), ==, 3);

  g_object_unref (sw);
}

typedef struct {
  gint count;
  gint index;
  gint n_children;
  gpointer parent;
} SignalData;

static void
children_changed (AtkObject  *accessible,
                  guint       index,
                  gpointer    child,
                  SignalData *data)
{
  data->count++;
  data->index = index;
  data->n_children = atk_object_get_n_accessible_children (accessible);
}

static void
add_child (GtkWidget *container,
           GtkWidget *child)
{
  if (GTK_IS_SCROLLED_WINDOW (container))
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (container), child);
  else
    gtk_container_add (GTK_CONTAINER (container), child);
}

static void
remove_child (GtkWidget *container,
              GtkWidget *child)
{
  if (GTK_IS_SCROLLED_WINDOW (container))
    {
      if (gtk_widget_get_parent (child) != container)
        child = gtk_widget_get_parent (child);
    }

  gtk_container_remove (GTK_CONTAINER (container), child);
}

static void
parent_notify (AtkObject *obj, GParamSpec *pspec, SignalData *data)
{
  data->count++;
  data->parent = atk_object_get_parent (obj);
}

static void
test_add_remove (GtkWidget *widget)
{
  AtkObject *accessible;
  AtkObject *child_accessible;
  SignalData add_data;
  SignalData remove_data;
  SignalData parent_data[3];
  GtkWidget *child[3];
  gint i, j;
  gint step_children;

  accessible = gtk_widget_get_accessible (widget);

  add_data.count = 0;
  remove_data.count = 0;
  g_signal_connect (accessible, "children_changed::add",
                    G_CALLBACK (children_changed), &add_data);
  g_signal_connect (accessible, "children_changed::remove",
                    G_CALLBACK (children_changed), &remove_data);

  step_children = atk_object_get_n_accessible_children (accessible);

  for (i = 0; i < 3; i++)
    {
      if (gtk_container_child_type (GTK_CONTAINER (widget)) == G_TYPE_NONE)
        break;

      child[i] = gtk_label_new ("bla");
      parent_data[i].count = 0;
      child_accessible = gtk_widget_get_accessible (child[i]);
      g_signal_connect (child_accessible, "notify::accessible-parent",
                        G_CALLBACK (parent_notify), &(parent_data[i]));
      add_child (widget, child[i]);

      g_assert_cmpint (add_data.count, ==, i + 1);
      g_assert_cmpint (add_data.n_children, ==, step_children + i + 1);
      g_assert_cmpint (remove_data.count, ==, 0);
      g_assert_cmpint (parent_data[i].count, ==, 1);
      if (GTK_IS_SCROLLED_WINDOW (widget) ||
          GTK_IS_NOTEBOOK (widget))
        g_assert (atk_object_get_parent (ATK_OBJECT (parent_data[i].parent)) == accessible);
      else
        g_assert (parent_data[i].parent == accessible);
    }
  for (j = 0 ; j < i; j++)
    {
      remove_child (widget, child[j]);
      g_assert_cmpint (add_data.count, ==, i);
      g_assert_cmpint (remove_data.count, ==, j + 1);
      g_assert_cmpint (remove_data.n_children, ==, step_children + i - j - 1);
      if (parent_data[j].count == 2)
        g_assert (parent_data[j].parent == NULL);
      else
        {
          AtkStateSet *set;
          set = atk_object_ref_state_set (ATK_OBJECT (parent_data[j].parent));
          g_assert (atk_state_set_contains_state (set, ATK_STATE_DEFUNCT));
          g_object_unref (set);
        }
    }

  g_signal_handlers_disconnect_by_func (accessible, G_CALLBACK (children_changed), &add_data);
  g_signal_handlers_disconnect_by_func (accessible, G_CALLBACK (children_changed), &remove_data);
}

static void
add_child_test (const gchar      *prefix,
                GTestFixtureFunc  test_func,
                GtkWidget        *widget)
{
  gchar *path;

  path = g_strdup_printf ("%s/%s", prefix, G_OBJECT_TYPE_NAME (widget));
  g_test_add_vtable (path,
                     0,
                     g_object_ref (widget),
                     0,
                     (GTestFixtureFunc) test_func,
                     (GTestFixtureFunc) g_object_unref);
  g_free (path);
}

static void
add_child_tests (GtkWidget *widget)
{
  g_object_ref_sink (widget);
  add_child_test ("/child/add-remove", (GTestFixtureFunc)test_add_remove, widget);
  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/scrolledwindow/child-count", test_scrolled_window_child_count);

  add_child_tests (gtk_scrolled_window_new (NULL, NULL));
  add_child_tests (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  add_child_tests (gtk_paned_new (GTK_ORIENTATION_HORIZONTAL));
  add_child_tests (gtk_grid_new ());
  add_child_tests (gtk_event_box_new ());
  add_child_tests (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  add_child_tests (gtk_assistant_new ());
  add_child_tests (gtk_frame_new ("frame"));
  add_child_tests (gtk_expander_new ("expander"));
  add_child_tests (gtk_table_new (2, 2, FALSE));
  add_child_tests (gtk_text_view_new ());
  add_child_tests (gtk_tree_view_new ());
#if 0
  /* gail doesn't handle non-label children in these */
  add_child_tests (gtk_button_new ());
  add_child_tests (gtk_statusbar_new ());
#endif
  add_child_tests (gtk_notebook_new ());

  return g_test_run ();
}

