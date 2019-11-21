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
  g_assert (gtk_expression_evaluate (expr, filter , &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "Hello World");
  g_value_unset (&value);

  gtk_expression_unref (expr);
  g_object_unref (filter);
  gtk_expression_watch_unwatch (watch);
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

  gtk_expression_unref (expr);
  g_object_unref (filter);
  gtk_expression_watch_unwatch (watch);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/expression/property", test_property);
  g_test_add_func ("/expression/closure", test_closure);

  return g_test_run ();
}
