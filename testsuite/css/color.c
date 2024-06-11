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
#include "gtk/gtkcsscolorprivate.h"
#include "gtk/gtkcsscolorvalueprivate.h"
#include "gtk/gtkcssstylepropertyprivate.h"
#include "gtk/gtkcssstaticstyleprivate.h"

#define EPSILON 0.005

static gboolean
color_is_near (const GtkCssColor *color1,
               const GtkCssColor *color2)
{
  return color1->color_space == color2->color_space &&
         color1->missing == color2->missing &&
         fabs (color1->values[0] - color2->values[0]) <= EPSILON &&
         fabs (color1->values[1] - color2->values[1]) <= EPSILON &&
         fabs (color1->values[2] - color2->values[2]) <= EPSILON &&
         fabs (color1->values[3] - color2->values[3]) <= EPSILON;
}

static void
error_cb (GtkCssParser         *parser,
          const GtkCssLocation *start,
          const GtkCssLocation *end,
          const GError         *error,
          gpointer              user_data)
{
  *(GError **)user_data = g_error_copy (error);
}

static void
color_from_string (const char  *str,
                   GtkCssColor *res)
{
  GtkStyleProperty *prop;
  GBytes *bytes;
  GtkCssParser *parser;
  GtkCssValue *value;
  GError *error = NULL;

  bytes = g_bytes_new_static (str, strlen (str));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, error_cb, &error, NULL);
  prop = _gtk_style_property_lookup ("color");
  value = _gtk_style_property_parse_value (prop, parser);
  g_assert_nonnull (value);
  g_assert_no_error (error);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  gtk_css_color_init_from_color (res, gtk_css_color_value_get_color (value));

  gtk_css_value_unref (value);
}

static void
print_css_color (const char        *prefix,
                 const GtkCssColor *c)
{
  GtkCssValue *v;
  char *s;

  v = gtk_css_color_value_new_color (c->color_space,
                                     FALSE,
                                     c->values,
                                     (gboolean[4]) {
                                       gtk_css_color_component_missing (c, 0),
                                       gtk_css_color_component_missing (c, 1),
                                       gtk_css_color_component_missing (c, 2),
                                       gtk_css_color_component_missing (c, 3),
                                     });
  s = gtk_css_value_to_string (v);
  g_print ("%s: %s\n", prefix, s);
  g_free (s);
  gtk_css_value_unref (v);
}

/* Tests for css color conversions */

typedef struct {
  const char *input;
  GtkCssColorSpace dest;
  const char *expected;
} ColorConversionTest;

static ColorConversionTest conversion_tests[] = {
  { "rgb(255,0,0)", GTK_CSS_COLOR_SPACE_SRGB_LINEAR, "color(srgb-linear 1 0 0)" },
  { "color(srgb 0.5 none 1 / 0.7)", GTK_CSS_COLOR_SPACE_SRGB_LINEAR, "color(srgb-linear 0.214041 0 1 / 0.7)" },
  { "rgb(100,100,100)", GTK_CSS_COLOR_SPACE_HSL, "hsla(0deg 0 39.215687 / 1)" },
  /* the following are from color-4, Example 26 */
  { "oklch(40.101% 0.12332 21.555)", GTK_CSS_COLOR_SPACE_SRGB, "rgb(49.06% 13.87% 15.9%)" },
  { "oklch(59.686% 0.15619 49.7694)", GTK_CSS_COLOR_SPACE_SRGB, "rgb(77.61% 36.34% 2.45%)" },
  { "oklch(0.65125 0.13138 104.097)", GTK_CSS_COLOR_SPACE_SRGB, "rgb(61.65% 57.51% 9.28%)" },
  { "oklch(0.66016 0.15546 134.231)", GTK_CSS_COLOR_SPACE_SRGB, "rgb(40.73% 65.12% 22.35%)" },
  { "oklch(72.322% 0.12403 247.996)", GTK_CSS_COLOR_SPACE_SRGB, "rgb(38.29% 67.27% 93.85%)" },
  { "oklch(42.1% 48.25% 328.4)", GTK_CSS_COLOR_SPACE_SRGB, "color(srgb 0.501808 0.00257216 0.501403)" },
  /* some self-conversions */
  { "oklch(0.392 0.4 none)", GTK_CSS_COLOR_SPACE_OKLCH, "oklch(0.392 0.4 0)" },
  { "color(display-p3 1 1 1)", GTK_CSS_COLOR_SPACE_DISPLAY_P3, "color(display-p3 1 1 1)" },
  { "color(rec2020 1 1 1)", GTK_CSS_COLOR_SPACE_REC2020, "color(rec2020 1 1 1)" },
  { "color(rec2100-pq 0.58 0.58 0.58)", GTK_CSS_COLOR_SPACE_REC2100_PQ, "color(rec2100-pq 0.58 0.58 0.58)" },
  /* more random tests */
  { "color(rec2100-pq 0.58 0.58 0.58)", GTK_CSS_COLOR_SPACE_REC2020, "color(rec2020 1 1 1)" },
  { "color(xyz 0.5 0.7 99%)", GTK_CSS_COLOR_SPACE_DISPLAY_P3, "color(display-p3 0.48 0.93 0.96)" },
  { "hsl(250 100 20)", GTK_CSS_COLOR_SPACE_REC2020, "color(rec2020 0.042 0.008 0.3226)" },
};

static void
test_conversion (gconstpointer data)
{
  ColorConversionTest *test = &conversion_tests[GPOINTER_TO_INT (data)];
  GtkCssColor input;
  GtkCssColor expected;
  GtkCssColor result;

  color_from_string (test->input, &input);
  color_from_string (test->expected, &expected);

  gtk_css_color_convert (&input, test->dest, &result);

  if (g_test_verbose ())
    {
      print_css_color ("expected", &expected);
      print_css_color ("converted", &result);
    }

  g_assert_true (color_is_near (&result, &expected));
}

/* Tests for css color interpolation */

typedef struct {
  const char *input1;
  const char *input2;
  float progress;
  GtkCssColorSpace in;
  GtkCssHueInterpolation interp;
  const char *expected;
} ColorInterpolationTest;

static ColorInterpolationTest interpolation_tests[] = {
  /* color-4, example 33 */
  { "oklch(78.3% 0.108 326.5)", "oklch(39.2% 0.4 none)", 0.5, GTK_CSS_COLOR_SPACE_OKLCH, GTK_CSS_HUE_INTERPOLATION_SHORTER, "oklch(58.75% 0.254 326.5)" },
  /* color-4, example 34 */
  { "oklch(0.783 0.108 326.5 / 0.5)", "oklch(0.392 0.4 0 / none)", 0.5, GTK_CSS_COLOR_SPACE_OKLCH, GTK_CSS_HUE_INTERPOLATION_SHORTER, "oklch(0.5875 0.254 343.25 / 0.5)" },
  /* color-4, example 35 */
  { "rgb(24% 12% 98% / 0.4)", "rgb(62% 26% 64% / 0.6)", 0.5, GTK_CSS_COLOR_SPACE_SRGB, GTK_CSS_HUE_INTERPOLATION_SHORTER, "rgb(46.8% 20.4% 77.6% / 0.5)" },
  /* color-4, example 38 */
  { "oklch(0.6 0.24 30)", "oklch(0.8 0.15 90)", 0.5, GTK_CSS_COLOR_SPACE_OKLCH, GTK_CSS_HUE_INTERPOLATION_SHORTER, "oklch(0.7 0.195 60)" },
  /* color-4, example 39 */
  { "oklch(0.6 0.24 30)", "oklch(0.8 0.15 90)", 0.5, GTK_CSS_COLOR_SPACE_OKLCH, GTK_CSS_HUE_INTERPOLATION_LONGER, "oklch(0.7 0.195 240)" },
};

static void
test_interpolation (gconstpointer data)
{
  ColorInterpolationTest *test = &interpolation_tests[GPOINTER_TO_INT (data)];
  GtkCssColor input1;
  GtkCssColor input2;
  GtkCssColor expected;
  GtkCssColor result;

  color_from_string (test->input1, &input1);
  color_from_string (test->input2, &input2);
  color_from_string (test->expected, &expected);

  gtk_css_color_interpolate (&input1,
                             &input2,
                             test->progress,
                             test->in,
                             test->interp,
                             &result);

  if (g_test_verbose ())
    {
      print_css_color ("expected", &expected);
      print_css_color ("interpolated", &result);
    }

  g_assert_true (color_is_near (&result, &expected));
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv);

  for (int i = 0; i < G_N_ELEMENTS (conversion_tests); i++)
    {
      char *path;

      path = g_strdup_printf ("/css/color/conversion/%d", i);
      g_test_add_data_func (path, GINT_TO_POINTER (i), test_conversion);
      g_free (path);
    }

  for (int i = 0; i < G_N_ELEMENTS (interpolation_tests); i++)
    {
      char *path;

      path = g_strdup_printf ("/css/color/interpolation/%d", i);
      g_test_add_data_func (path, GINT_TO_POINTER (i), test_interpolation);
      g_free (path);
    }

  return g_test_run ();
}
