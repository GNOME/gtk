/* GTK - The GIMP Toolkit
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

static void
test_basic (void)
{
  GtkEnumList *list = gtk_enum_list_new (GTK_TYPE_SYMBOLIC_COLOR);

  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (list)) == GTK_TYPE_ENUM_LIST_ITEM);
  g_assert_true (g_list_model_get_n_items (G_LIST_MODEL (list)) == 5);
  g_assert_true (gtk_enum_list_find (list, GTK_SYMBOLIC_COLOR_ACCENT) == 4);

  g_object_unref (list);
}

static void
test_negative (void)
{
  GtkEnumList *list = gtk_enum_list_new (GTK_TYPE_RESPONSE_TYPE);
  GObject *item;

  g_assert_true (g_list_model_get_n_items (G_LIST_MODEL (list)) == 11);
  g_assert_true (gtk_enum_list_find (list, GTK_RESPONSE_OK) == 4);

  item = g_list_model_get_item (G_LIST_MODEL (list), 5);
  g_assert_true (GTK_IS_ENUM_LIST_ITEM (item));
  g_assert_true (gtk_enum_list_item_get_value (GTK_ENUM_LIST_ITEM (item)) == GTK_RESPONSE_CANCEL);
  g_assert_cmpstr (gtk_enum_list_item_get_name (GTK_ENUM_LIST_ITEM (item)), ==, "GTK_RESPONSE_CANCEL");
  g_assert_cmpstr (gtk_enum_list_item_get_nick (GTK_ENUM_LIST_ITEM (item)), ==, "cancel");
  g_object_unref (item);

  g_object_unref (list);
}

static void
test_gaps (void)
{
  GtkEnumList *list = gtk_enum_list_new (GTK_TYPE_CONSTRAINT_STRENGTH);

  g_assert_true (g_list_model_get_n_items (G_LIST_MODEL (list)) == 4);
  g_assert_true (gtk_enum_list_find (list, GTK_CONSTRAINT_STRENGTH_REQUIRED) != GTK_INVALID_LIST_POSITION);
  g_assert_true (gtk_enum_list_find (list, GTK_CONSTRAINT_STRENGTH_WEAK) != GTK_INVALID_LIST_POSITION);
  g_assert_true (gtk_enum_list_find (list, 5) == GTK_INVALID_LIST_POSITION);

  g_object_unref (list);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/enum-list/basic", test_basic);
  g_test_add_func ("/enum-list/negative", test_negative);
  g_test_add_func ("/enum-list/gaps", test_gaps);

  return g_test_run();
}
