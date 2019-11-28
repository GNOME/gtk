/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <locale.h>

#include <gtk/gtk.h>

static void
inc_counter (gpointer data)
{
  guint *counter = data;

  *counter += 1;
}

static void
test_property (void)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expr;
  GtkExpressionWatch *watch;
  GtkStringFilter *filter;
  guint counter = 0;

  filter = GTK_STRING_FILTER (gtk_string_filter_new ());
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");
  watch = gtk_expression_watch (expr, filter, inc_counter, &counter, NULL);

  g_assert (gtk_expression_evaluate (expr, filter, &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, NULL);
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "Hello World");
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  g_assert (gtk_expression_evaluate (expr, filter , &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "Hello World");
  g_value_unset (&value);

  gtk_expression_watch_unwatch (watch);
  g_assert_cmpint (counter, ==, 0);

  gtk_expression_unref (expr);
  g_object_unref (filter);
}

static char *
print_filter_info (GtkStringFilter         *filter,
                   const char              *search,
                   gboolean                 ignore_case,
                   GtkStringFilterMatchMode match_mode)
{
  g_assert_cmpstr (search, ==, gtk_string_filter_get_search (filter));
  g_assert_cmpint (ignore_case, ==, gtk_string_filter_get_ignore_case (filter));
  g_assert_cmpint (match_mode, ==, gtk_string_filter_get_match_mode (filter));

  return g_strdup ("OK");
}

static void
test_closure (void)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expr, *pexpr[3];
  GtkExpressionWatch *watch;
  GtkStringFilter *filter;
  guint counter = 0;

  filter = GTK_STRING_FILTER (gtk_string_filter_new ());
  pexpr[0] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");
  pexpr[1] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "ignore-case");
  pexpr[2] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "match-mode");
  expr = gtk_cclosure_expression_new (G_TYPE_STRING,
                                      NULL,
                                      3,
                                      pexpr,
                                      G_CALLBACK (print_filter_info),
                                      NULL,
                                      NULL);
  watch = gtk_expression_watch (expr, filter, inc_counter, &counter, NULL);

  g_assert (gtk_expression_evaluate (expr, filter, &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "Hello World");
  g_assert_cmpint (counter, ==, 1);
  g_assert (gtk_expression_evaluate (expr, filter , &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_string_filter_set_ignore_case (filter, FALSE);
  g_assert_cmpint (counter, ==, 2);
  g_assert (gtk_expression_evaluate (expr, filter , &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "Hello");
  gtk_string_filter_set_ignore_case (filter, TRUE);
  gtk_string_filter_set_match_mode (filter, GTK_STRING_FILTER_MATCH_MODE_EXACT);
  g_assert_cmpint (counter, ==, 5);
  g_assert (gtk_expression_evaluate (expr, filter , &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_expression_watch_unwatch (watch);
  g_assert_cmpint (counter, ==, 5);

  gtk_expression_unref (expr);
  g_object_unref (filter);
}

static void
test_constant (void)
{
  GtkExpression *expr;
  GValue value = G_VALUE_INIT;
  gboolean res;

  expr = gtk_constant_expression_new (G_TYPE_INT, 22);
  g_assert_cmpint (gtk_expression_get_value_type (expr), ==, G_TYPE_INT);
  g_assert_true (gtk_expression_is_static (expr));

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpint (g_value_get_int (&value), ==, 22);

  gtk_expression_unref (expr);
}

/* Test that object expressions fail to evaluate when
 * the object is gone.
 */
static void
test_object (void)
{
  GtkExpression *expr;
  GObject *obj;
  GValue value = G_VALUE_INIT;
  gboolean res;

  obj = G_OBJECT (gtk_string_filter_new ());

  expr = gtk_object_expression_new (obj);
  g_assert_true (!gtk_expression_is_static (expr));
  g_assert_cmpint (gtk_expression_get_value_type (expr), ==, GTK_TYPE_STRING_FILTER);

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_true (g_value_get_object (&value) == obj);
  g_value_unset (&value);

  g_clear_object (&obj);
  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_false (res);

  gtk_expression_unref (expr);
}

/* Some basic tests that nested expressions work; in particular test
 * that watching works when things change deeper in the expression tree
 *
 * The setup we use is GtkFilterListModel -> GtkFilter -> "search" property,
 * which gives us an expression tree like
 *
 * GtkPropertyExpression "search"
 *    -> GtkPropertyExpression "filter"
 *         -> GtkObjectExpression listmodel
 *
 * We test setting both the search property and the filter property.
 */
static void
test_nested (void)
{
  GtkExpression *list_expr;
  GtkExpression *filter_expr;
  GtkExpression *expr;
  GtkFilter *filter;
  GListModel *list;
  GtkFilterListModel *filtered;
  GValue value = G_VALUE_INIT;
  gboolean res;
  GtkExpressionWatch *watch;
  guint counter = 0;

  filter = gtk_string_filter_new ();
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "word");
  list = G_LIST_MODEL (g_list_store_new (G_TYPE_OBJECT));
  filtered = gtk_filter_list_model_new (list, filter);

  list_expr = gtk_object_expression_new (G_OBJECT (filtered));
  filter_expr = gtk_property_expression_new (GTK_TYPE_FILTER_LIST_MODEL, list_expr, "filter");
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, filter_expr, "search");

  g_assert_true (!gtk_expression_is_static (expr));
  g_assert_cmpint (gtk_expression_get_value_type (expr), ==, G_TYPE_STRING);

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "word");
  g_value_unset (&value);

  watch = gtk_expression_watch (expr, NULL, inc_counter, &counter, NULL);
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "salad");
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "salad");
  g_value_unset (&value);

  gtk_filter_list_model_set_filter (filtered, filter);
  g_assert_cmpint (counter, ==, 0);

  g_clear_object (&filter);
  filter = gtk_string_filter_new ();
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "salad");
  gtk_filter_list_model_set_filter (filtered, filter);
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "salad");
  g_value_unset (&value);

  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "bar");
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "bar");
  g_value_unset (&value);

  gtk_filter_list_model_set_filter (filtered, NULL);
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_false (res);

  gtk_expression_watch_unwatch (watch);
  g_assert_cmpint (counter, ==, 0);

  g_object_unref (filtered);
  g_object_unref (list);
  g_object_unref (filter);
  gtk_expression_unref (expr);
}

/* This test uses the same setup as the last test, but
 * passes the filter as the "this" object when creating
 * the watch.
 *
 * So when we set a new filter and the old one gets desroyed,
 * the watch should invalidate itself because its this object
 * is gone.
 */
static void
test_nested_this_destroyed (void)
{
  GtkExpression *list_expr;
  GtkExpression *filter_expr;
  GtkExpression *expr;
  GtkFilter *filter;
  GListModel *list;
  GtkFilterListModel *filtered;
  GValue value = G_VALUE_INIT;
  gboolean res;
  GtkExpressionWatch *watch;
  guint counter = 0;

  filter = gtk_string_filter_new ();
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "word");
  list = G_LIST_MODEL (g_list_store_new (G_TYPE_OBJECT));
  filtered = gtk_filter_list_model_new (list, filter);

  list_expr = gtk_object_expression_new (G_OBJECT (filtered));
  filter_expr = gtk_property_expression_new (GTK_TYPE_FILTER_LIST_MODEL, list_expr, "filter");
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, filter_expr, "search");

  watch = gtk_expression_watch (expr, filter, inc_counter, &counter, NULL);
  gtk_expression_watch_ref (watch);
  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "word");
  g_value_unset (&value);

  g_clear_object (&filter);
  g_assert_cmpint (counter, ==, 0);

  filter = gtk_string_filter_new ();
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "salad");
  gtk_filter_list_model_set_filter (filtered, filter);
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_false (res);

  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "bar");
  g_assert_cmpint (counter, ==, 0);

  gtk_filter_list_model_set_filter (filtered, NULL);
  g_assert_cmpint (counter, ==, 0);

  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_false (res);
  g_assert_false (G_IS_VALUE (&value));

  /* We unwatch on purpose here to make sure it doesn't do bad things. */
  gtk_expression_watch_unwatch (watch);
  gtk_expression_watch_unref (watch);
  g_assert_cmpint (counter, ==, 0);

  g_object_unref (filtered);
  g_object_unref (list);
  g_object_unref (filter);
  gtk_expression_unref (expr);
}

/* Test that property expressions fail to evaluate if the
 * expression evaluates to an object of the wrong type
 */
static void
test_type_mismatch (void)
{
  GtkFilter *filter;
  GtkExpression *expr;
  GValue value = G_VALUE_INIT;
  gboolean res;

  filter = gtk_any_filter_new ();

  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_constant_expression_new (GTK_TYPE_ANY_FILTER, filter), "search");

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_false (res);
  g_assert_false (G_IS_VALUE (&value));

  gtk_expression_unref (expr);
  g_object_unref (filter);
}

/* Some basic tests around 'this' */
static void
test_this (void)
{
  GtkFilter *filter;
  GtkFilter *filter2;
  GtkExpression *expr;
  GValue value = G_VALUE_INIT;
  gboolean res;

  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");

  filter = gtk_string_filter_new ();
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "word");

  filter2 = gtk_string_filter_new ();
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter2), "sausage");

  res = gtk_expression_evaluate (expr, filter, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "word");
  g_value_unset (&value);

  res = gtk_expression_evaluate (expr, filter2, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "sausage");
  g_value_unset (&value);

  gtk_expression_unref (expr);
  g_object_unref (filter2);
  g_object_unref (filter);
}

/* Check that even for static expressions, watches can be created
 * and destroying the "this" argument does invalidate the
 * expression.
 */
static void
test_constant_watch_this_destroyed (void)
{
  GtkExpression *expr;
  GObject *this;
  guint counter = 0;

  this = g_object_new (G_TYPE_OBJECT, NULL);
  expr = gtk_constant_expression_new (G_TYPE_INT, 42);
  gtk_expression_watch (expr, this, inc_counter, &counter, NULL);
  g_assert_cmpint (counter, ==, 0);

  g_clear_object (&this);
  g_assert_cmpint (counter, ==, 1);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/expression/property", test_property);
  g_test_add_func ("/expression/closure", test_closure);
  g_test_add_func ("/expression/constant", test_constant);
  g_test_add_func ("/expression/constant-watch-this-destroyed", test_constant_watch_this_destroyed);
  g_test_add_func ("/expression/object", test_object);
  g_test_add_func ("/expression/nested", test_nested);
  g_test_add_func ("/expression/nested-this-destroyed", test_nested_this_destroyed);
  g_test_add_func ("/expression/type-mismatch", test_type_mismatch);
  g_test_add_func ("/expression/this", test_this);

  return g_test_run ();
}
