/* child-index.c
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

#include <gtk/gtk.h>

#include "gtk/gtkaccessibleprivate.h"
#include "gtk/a11y/gtkatspicontextprivate.h"
#include "gtk/gtkaccessiblechildindexprivate.h"
#include "gtk/gtkwidgetprivate.h"

typedef struct
{
  GtkWidget  parent_instance;
  GtkWidget *append_parent;
  GtkWidget *invalidate_parent;
} ReentrantWidget;

typedef struct
{
  GtkWidgetClass parent_class;
} ReentrantWidgetClass;

static GtkATContext *(* parent_get_at_context) (GtkAccessible *self);

static GtkATContext *
reentrant_get_at_context (GtkAccessible *self)
{
  ReentrantWidget *widget = (ReentrantWidget *) self;

  if (widget->append_parent != NULL)
    {
      GtkWidget *parent = widget->append_parent;

      widget->append_parent = NULL;
      gtk_box_append (GTK_BOX (parent), gtk_label_new ("Added during indexing"));
    }

  if (widget->invalidate_parent != NULL)
    {
      GtkWidget *parent = widget->invalidate_parent;

      widget->invalidate_parent = NULL;
      gtk_widget_invalidate_accessible_child_index (parent);
    }

  return parent_get_at_context (self);
}

static void
reentrant_widget_accessible_init (GtkAccessibleInterface *iface)
{
  GtkAccessibleInterface *parent_iface = g_type_interface_peek_parent (iface);

  parent_get_at_context = parent_iface->get_at_context;
  iface->get_at_context = reentrant_get_at_context;
}

G_DEFINE_TYPE_WITH_CODE (ReentrantWidget, reentrant_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE, reentrant_widget_accessible_init))

static void
reentrant_widget_class_init (ReentrantWidgetClass *klass)
{
}

static void
reentrant_widget_init (ReentrantWidget *self)
{
}

typedef struct
{
  GtkWidget parent_instance;
} CustomParent;

typedef struct
{
  GtkWidgetClass parent_class;
} CustomParentClass;

static GtkAccessible *
custom_parent_get_first_child (GtkAccessible *self)
{
  return NULL;
}

static void
custom_parent_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_first_accessible_child = custom_parent_get_first_child;
}

G_DEFINE_TYPE_WITH_CODE (CustomParent, custom_parent, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE, custom_parent_accessible_init))

typedef struct
{
  GtkWidget parent_instance;
} CustomChild;

typedef struct
{
  GtkWidgetClass parent_class;
} CustomChildClass;

static GtkAccessible *
custom_child_get_next_sibling (GtkAccessible *self)
{
  return NULL;
}

static void
custom_child_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_next_accessible_sibling = custom_child_get_next_sibling;
}

G_DEFINE_TYPE_WITH_CODE (CustomChild, custom_child, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE, custom_child_accessible_init))

static void
custom_child_class_init (CustomChildClass *klass)
{
}

static void
custom_child_init (CustomChild *self)
{
}

static void
custom_parent_class_init (CustomParentClass *klass)
{
}

static void
custom_parent_init (CustomParent *self)
{
}

static int
traversal_index (GtkWidget *parent,
                 GtkWidget *child)
{
  int position = 0;

  for (GtkWidget *candidate = gtk_widget_get_first_child (parent);
       candidate != NULL;
       candidate = gtk_widget_get_next_sibling (candidate))
    {
      if (candidate == child)
        return position;

      if (gtk_accessible_should_present (GTK_ACCESSIBLE (candidate)))
        position++;
    }

  return -1;
}

static void
check_children (GtkWidget *parent)
{
  for (GtkWidget *child = gtk_widget_get_first_child (parent);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int expected = traversal_index (parent, child);

      g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, child), ==, expected);

      if (g_strcmp0 (g_getenv ("GTK_A11Y"), "atspi") == 0)
        {
          GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child));

          g_assert_true (GTK_IS_AT_SPI_CONTEXT (context));
          g_assert_cmpint (gtk_at_spi_context_get_index_in_parent (GTK_AT_SPI_CONTEXT (context)), ==, expected);
          g_object_unref (context);
        }
    }
}

static void
test_child_index (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *other_window = gtk_window_new ();
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *other = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *presentation;
  GtkWidget *weak_child;
  GtkWidget *children[32];

  gtk_widget_set_visible (parent, TRUE);
  gtk_widget_set_visible (other, TRUE);
  gtk_window_set_child (GTK_WINDOW (window), parent);
  gtk_window_set_child (GTK_WINDOW (other_window), other);
  gtk_window_present (GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (other_window));

  for (guint i = 0; i < G_N_ELEMENTS (children); i++)
    {
      children[i] = gtk_label_new ("Child");
      gtk_widget_set_visible (children[i], TRUE);
      gtk_box_append (GTK_BOX (parent), children[i]);
      check_children (parent);
    }

  presentation = gtk_label_new ("Decoration");
  gtk_widget_set_accessible_role (presentation, GTK_ACCESSIBLE_ROLE_PRESENTATION);
  gtk_widget_set_visible (presentation, TRUE);
  gtk_widget_insert_before (presentation, parent, children[10]);
  check_children (parent);

  gtk_widget_insert_after (children[31], parent, NULL);
  check_children (parent);
  gtk_widget_insert_after (children[5], parent, children[17]);
  check_children (parent);
  gtk_widget_insert_before (children[20], parent, children[3]);
  check_children (parent);

  for (guint i = 0; i < 10; i++)
    {
      gtk_widget_set_visible (children[12], FALSE);
      check_children (parent);
      gtk_widget_set_visible (children[12], TRUE);
      check_children (parent);
    }

  gtk_accessible_update_state (GTK_ACCESSIBLE (children[7]),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
  check_children (parent);
  gtk_accessible_reset_state (GTK_ACCESSIBLE (children[7]),
                              GTK_ACCESSIBLE_STATE_HIDDEN);
  check_children (parent);

  g_object_ref (children[4]);
  gtk_widget_unparent (children[4]);
  check_children (parent);
  gtk_box_append (GTK_BOX (other), children[4]);
  g_object_unref (children[4]);
  check_children (other);

  gtk_accessible_set_accessible_parent (GTK_ACCESSIBLE (children[6]),
                                        GTK_ACCESSIBLE (other), NULL);
  g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, children[6]), ==, -1);

  {
    GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (children[6]));

    gtk_at_context_set_next_accessible_sibling (context, GTK_ACCESSIBLE (children[7]));
    g_object_unref (context);
  }

  g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, children[6]), ==, -1);
  gtk_accessible_set_accessible_parent (GTK_ACCESSIBLE (children[6]), NULL, NULL);
  check_children (parent);

  weak_child = children[0];
  g_object_add_weak_pointer (G_OBJECT (weak_child), (gpointer *) &weak_child);
  gtk_window_destroy (GTK_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (other_window));
  g_assert_null (weak_child);
}

static void
test_mapped_index (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_window_set_child (GTK_WINDOW (window), box);
  for (int i = 0; i < 8; i++)
    {
      GtkWidget *child = gtk_label_new ("Child");

      gtk_widget_set_visible (child, TRUE);
      gtk_box_append (GTK_BOX (box), child);
    }

  gtk_window_present (GTK_WINDOW (window));
  check_children (box);
  gtk_widget_set_visible (window, FALSE);
  check_children (box);
  gtk_widget_set_visible (window, TRUE);
  check_children (box);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_reentrant_index (void)
{
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  ReentrantWidget *child = g_object_new (reentrant_widget_get_type (), NULL);

  gtk_box_append (GTK_BOX (parent), GTK_WIDGET (child));
  gtk_widget_invalidate_accessible_child_index (parent);
  child->append_parent = parent;

  g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, GTK_WIDGET (child)), ==, -1);
  g_assert_null (child->append_parent);
  check_children (parent);

  g_object_ref_sink (parent);
  g_object_unref (parent);
}

static void
test_reentrant_index_update (void)
{
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  ReentrantWidget *child = g_object_new (reentrant_widget_get_type (), NULL);
  GtkWidget *sibling = gtk_label_new ("Sibling");

  g_object_ref_sink (parent);
  gtk_box_append (GTK_BOX (parent), GTK_WIDGET (child));
  gtk_box_append (GTK_BOX (parent), sibling);
  check_children (parent);

  gtk_widget_set_visible (GTK_WIDGET (child), FALSE);
  child->invalidate_parent = parent;
  gtk_widget_set_visible (GTK_WIDGET (child), TRUE);
  g_assert_null (child->invalidate_parent);
  check_children (parent);

  child->invalidate_parent = parent;
  gtk_widget_insert_after (GTK_WIDGET (child), parent, sibling);
  g_assert_null (child->invalidate_parent);
  check_children (parent);

  g_object_unref (parent);
}

static void
test_custom_hierarchy (void)
{
  GtkWidget *parent = g_object_new (custom_parent_get_type (), NULL);
  GtkWidget *child = gtk_label_new ("Physical child");
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *ordinary = gtk_label_new ("Ordinary child");
  GtkWidget *custom = g_object_new (custom_child_get_type (), NULL);

  g_object_ref_sink (parent);
  gtk_widget_set_parent (child, parent);
  g_assert_null (gtk_accessible_get_first_accessible_child (GTK_ACCESSIBLE (parent)));
  g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, child), ==, -1);
  gtk_widget_unparent (child);
  g_object_unref (parent);

  g_object_ref_sink (box);
  gtk_box_append (GTK_BOX (box), ordinary);
  g_assert_cmpint (gtk_widget_get_accessible_child_index (box, ordinary), ==, 0);
  gtk_widget_insert_before (custom, box, ordinary);
  g_assert_cmpint (gtk_widget_get_accessible_child_index (box, ordinary), ==, -1);
  gtk_widget_unparent (custom);
  g_assert_cmpint (gtk_widget_get_accessible_child_index (box, ordinary), ==, 0);
  g_object_unref (box);
}

static void
test_bulk_index (gconstpointer data)
{
  int n_children = GPOINTER_TO_INT (data);
  GtkWidget *window = gtk_window_new ();
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GPtrArray *children = g_ptr_array_sized_new (n_children);
  gint64 start = g_get_monotonic_time ();

  gtk_widget_set_visible (parent, TRUE);
  gtk_window_set_child (GTK_WINDOW (window), parent);
  gtk_window_present (GTK_WINDOW (window));

  for (int i = 0; i < n_children; i++)
    {
      GtkWidget *child = gtk_label_new ("Child");

      gtk_widget_set_visible (child, TRUE);
      gtk_box_append (GTK_BOX (parent), child);
      g_ptr_array_add (children, child);
      g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, child), ==, i);
    }

  for (int i = 0; i < n_children; i++)
    {
      gtk_widget_set_visible (g_ptr_array_index (children, i), FALSE);
      g_assert_cmpint (gtk_widget_get_accessible_child_index (parent, g_ptr_array_index (children, n_children - 1)),
                       ==,
                       MAX (0, n_children - i - 2));
    }

  g_test_message ("%d children: %.3f seconds",
                  n_children,
                  (g_get_monotonic_time () - start) / (double) G_USEC_PER_SEC);

  g_ptr_array_unref (children);
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  g_test_add_func ("/a11y/child-index", test_child_index);
  g_test_add_func ("/a11y/child-index-mapped", test_mapped_index);
  g_test_add_func ("/a11y/child-index-reentrant", test_reentrant_index);
  g_test_add_func ("/a11y/child-index-reentrant-update", test_reentrant_index_update);
  g_test_add_func ("/a11y/child-index-custom-hierarchy", test_custom_hierarchy);
  g_test_add_data_func ("/a11y/child-index-bulk/600", GINT_TO_POINTER (600), test_bulk_index);
  g_test_add_data_func ("/a11y/child-index-bulk/1200", GINT_TO_POINTER (1200), test_bulk_index);
  g_test_add_data_func ("/a11y/child-index-bulk/2401", GINT_TO_POINTER (2401), test_bulk_index);
  g_test_add_data_func ("/a11y/child-index-bulk/4800", GINT_TO_POINTER (4800), test_bulk_index);
  return g_test_run ();
}
