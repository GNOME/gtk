/*
 * Copyright (C) 2024 Red Hat Inc.
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

#include "config.h"

#include <gtk/gtk.h>
#include "gtk/gtkcssvalueprivate.h"
#include "gtk/gtkcssnumbervalueprivate.h"
#include "gtk/gtkcsscolorvalueprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/gtkwidgetprivate.h"
#include "gtk/gtkcssnodeprivate.h"

static GtkWidget *dummy;

typedef struct {
  const char *input;
  gboolean is_computed;
  const char *specified;
  const char *computed;
  double expected_value;
} CssNumberValueTest;

static CssNumberValueTest number_tests[] = {
  { "calc(10 + 2)", TRUE, "12", "12", 12 },
  { "calc(10% + 2%)", FALSE, "12%", "12%", 12 },
  { "calc(10% + 2px + 2%)", FALSE, "calc(2px + 12%)", "calc(2px + 12%)", 14 },
  { "calc(32mm + 2px)", FALSE, "calc(32mm + 2px)", "calc(120.94488188976378px + 2px)", 32 * 96 * 0.039370078740157477 + 2, },
  { "calc(32deg * 3 + 0.1turn)", TRUE, "132deg", "132deg", 132 },
  { "calc(1s + 500ms)", TRUE, "1.5s", "1.5s", 1.5 },
  { "10", TRUE, "10", "10", 10 },
  { "calc(2 + 3 + pi)", TRUE, "8.1415926535897931", "8.1415926535897931", 5 + G_PI },
  { "calc(2 * 3 * pi)", TRUE, "18.849555921538759", "18.849555921538759", 6 * G_PI },
  { "calc(2mm + 3cm)", FALSE, "32mm", "120.94488188976378px", 32 * 96 * 0.039370078740157477, },
  { "sin(2 * pi)", TRUE, NULL, NULL, 0 },
  { "10%", FALSE, "10%", "10%", 10 },
};

G_GNUC_NORETURN
static void
error_func (GtkCssParser         *parser,
            const GtkCssLocation *start,
            const GtkCssLocation *end,
            const GError         *error,
            gpointer              user_data)
{
  g_assert_not_reached ();
}

static void
test_number_value (gconstpointer data)
{
  CssNumberValueTest *test = (CssNumberValueTest *) data;
  GtkCssValue *value;
  GtkCssComputeContext context;
  GtkCssValue *res;
  GtkCssParser *parser;
  GBytes *bytes;
  GtkCssNode *node;

  if (g_test_verbose ())
    g_test_message ("input: %s", test->input);

  node = gtk_widget_get_css_node (dummy);
  context.provider = gtk_css_node_get_style_provider (node);
  context.style = gtk_css_node_get_style (node);
  context.parent_style = NULL;
  context.variables = NULL;
  context.shorthands = NULL;

  bytes = g_bytes_new_static (test->input, strlen (test->input));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, error_func, NULL, NULL);

  value = gtk_css_number_value_parse (parser, GTK_CSS_PARSE_PERCENT |
                                              GTK_CSS_PARSE_NUMBER |
                                              GTK_CSS_PARSE_LENGTH |
                                              GTK_CSS_PARSE_ANGLE |
                                              GTK_CSS_PARSE_TIME);

  g_assert_true (gtk_css_value_is_computed (value) == test->is_computed);

  if (test->specified)
    {
      char *str = gtk_css_value_to_string (value);
      g_assert_cmpstr (str, ==, test->specified);
      g_free (str);
    }

  res = gtk_css_value_compute (value, GTK_CSS_PROPERTY_LETTER_SPACING, &context);

  if (test->is_computed)
    g_assert_true (res == value);

  g_assert_true (gtk_css_number_value_has_percent (res) || gtk_css_value_is_computed (res));

  if (test->computed)
    {
      char *str = gtk_css_value_to_string (res);
      g_assert_cmpstr (str, ==, test->computed);
      g_free (str);
    }

  g_assert_cmpfloat_with_epsilon (gtk_css_number_value_get_canonical (res, 100), test->expected_value, FLT_EPSILON);

  gtk_css_value_unref (value);
  gtk_css_value_unref (res);

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);
}

typedef struct {
  const char *input;
  gboolean is_computed;
  gboolean contains_current_color;
  const char *specified;
  const char *computed;
  const char *current;
  const char *used;
} CssColorValueTest;

static CssColorValueTest color_tests[] = {
  { "rgba(255, 255, 128, 0.1)", TRUE, FALSE, "rgba(255,255,128,0.1)", "rgba(255,255,128,0.1)" },
  { "currentcolor", TRUE, TRUE, "currentcolor", "currentcolor", "color(srgb 1 0 0)", "color(srgb 1 0 0)" },
  { "color(from color(srgb 0.5 0.5 0.2) srgb 0.5 calc(r * g) b / calc(alpha / 2))", TRUE, FALSE, "color(srgb 0.5 0.25 0.2 / 0.5)", "color(srgb 0.5 0.25 0.2 / 0.5)" },
  { "rgb(from currentcolor r g 40% / 50%)", TRUE, TRUE, "color(from currentcolor srgb r g 40% / 50%)", "color(from currentcolor srgb r g 40% / 50%)", "color(srgb 1 0 0)", "color(srgb 1 0 0.4 / 0.5)" },
  { "rgb(from darkgoldenrod r g 100 / 50%)", TRUE, FALSE, "color(srgb 0.721569 0.52549 0.392157 / 0.5)", "color(srgb 0.721569 0.52549 0.392157 / 0.5)" },
  { "rgb(from white 100% 100% 100% / 100%)", TRUE, FALSE, "color(srgb 1 1 1)", "color(srgb 1 1 1)" },
  { "color(from white srgb 100% 100% 100% / 100%)", TRUE, FALSE, "color(srgb 1 1 1)", "color(srgb 1 1 1)" },
};

static void
test_color_value (gconstpointer data)
{
  CssColorValueTest *test = (CssColorValueTest *) data;
  GtkCssValue *value;
  GtkCssComputeContext context;
  GtkCssValue *res;
  GtkCssParser *parser;
  GBytes *bytes;
  GtkCssNode *node;

  if (g_test_verbose ())
    g_test_message ("input: %s", test->input);

  node = gtk_widget_get_css_node (dummy);
  context.provider = gtk_css_node_get_style_provider (node);
  context.style = gtk_css_node_get_style (node);
  context.parent_style = NULL;
  context.variables = NULL;
  context.shorthands = NULL;

  bytes = g_bytes_new_static (test->input, strlen (test->input));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, error_func, NULL, NULL);

  value = gtk_css_color_value_parse (parser);

  g_assert_true (gtk_css_value_is_computed (value) == test->is_computed);
  g_assert_true (gtk_css_value_contains_current_color (value) == test->contains_current_color);

  if (test->specified)
    {
      char *str = gtk_css_value_to_string (value);
      g_assert_cmpstr (str, ==, test->specified);
      g_free (str);
    }

  res = gtk_css_value_compute (value, GTK_CSS_PROPERTY_COLOR, &context);

  if (test->is_computed)
    g_assert_true (res == value);

  g_assert_true (gtk_css_value_is_computed (res));
  g_assert_true (gtk_css_value_contains_current_color (res) == test->contains_current_color);

  if (test->computed)
    {
      char *str = gtk_css_value_to_string (res);
      g_assert_cmpstr (str, ==, test->computed);
      g_free (str);
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  if (gtk_css_value_contains_current_color (res))
    {
      GtkCssValue *current;
      GtkCssValue *used;

      bytes = g_bytes_new_static (test->current, strlen (test->current));
      parser = gtk_css_parser_new_for_bytes (bytes, NULL, error_func, NULL, NULL);

      current = gtk_css_color_value_parse (parser);

      used = gtk_css_value_resolve (res, &context, current);

      g_assert_true (gtk_css_value_is_computed (used));
      g_assert_false (gtk_css_value_contains_current_color (used));

      if (test->used)
        {
          char *str = gtk_css_value_to_string (used);
          g_assert_cmpstr (str, ==, test->used);
          g_free (str);
        }

      gtk_css_parser_unref (parser);
      g_bytes_unref (bytes);
    }

  gtk_css_value_unref (value);
  gtk_css_value_unref (res);

}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv);

  dummy = gtk_window_new ();
  gtk_widget_realize (dummy);

  for (int i = 0; i < G_N_ELEMENTS (number_tests); i++)
    {
      char *path;

      path = g_strdup_printf ("/css/compute/number/%d", i);
      g_test_add_data_func (path, &number_tests[i], test_number_value);
      g_free (path);
    }

  for (int i = 0; i < G_N_ELEMENTS (color_tests); i++)
    {
      char *path;

      path = g_strdup_printf ("/css/compute/color/%d", i);
      g_test_add_data_func (path, &color_tests[i], test_color_value);
      g_free (path);
    }

  return g_test_run ();
}
