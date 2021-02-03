/*
 * Copyright (C) 2021 Red Hat Inc.
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
#include "gtk/gtkcsscolorvalueprivate.h"
#include "gtk/gtkcssstylepropertyprivate.h"

static gboolean
color_is_near (const GdkRGBA *color1,
               const GdkRGBA *color2)
{
  if (fabs (color1->red - color2->red) > FLT_EPSILON)
    return FALSE;
  if (fabs (color1->green - color2->green) > FLT_EPSILON)
    return FALSE;
  if (fabs (color1->blue- color2->blue) > FLT_EPSILON)
    return FALSE;
  if (fabs (color1->alpha- color2->alpha) > FLT_EPSILON)
    return FALSE;
  return TRUE;
}

static gboolean
value_is_near (int          prop,
               GtkCssValue *value1,
               GtkCssValue *value2)
{
  if (_gtk_css_value_equal (value1, value2))
    return TRUE;

  switch (prop)
    {
    case GTK_CSS_PROPERTY_COLOR:
      {
        GtkCssValue *v1, *v2;
        const GdkRGBA *c1, *c2;
        gboolean res;

        v1 = _gtk_css_value_compute (value1, prop, NULL, NULL, NULL);
        v2 = _gtk_css_value_compute (value2, prop, NULL, NULL, NULL);
        c1 = gtk_css_color_value_get_rgba (v1);
        c2 = gtk_css_color_value_get_rgba (v2);

        res = color_is_near (c1, c2);

        gtk_css_value_unref (v1);
        gtk_css_value_unref (v2);

        return res;
      }
      break;

    default:
      break;
    }

  return FALSE;
}

typedef struct {
  int prop;
  const char *value1;
  const char *value2;
  double progress;
  const char *expected;
} ValueTransitionTest;

static ValueTransitionTest tests[] = {
  { GTK_CSS_PROPERTY_COLOR, "transparent", "rgb(255,0,0)", 0.25, "rgba(255,0,0,0.25)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "none", "2px 2px 10px 4px rgb(200,200,200)", 0.5, "1px 1px 5px 2px rgb(100,100,100)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "2px 2px 10px 4px rgb(200,200,200)", "none", 0.5, "1px 1px 5px 2px rgb(100,100,100)" },
};

static void
test_transition (gconstpointer data)
{
  ValueTransitionTest *test = &tests[GPOINTER_TO_INT (data)];
  GtkStyleProperty *prop;
  GtkCssValue *value1;
  GtkCssValue *value2;
  GtkCssValue *expected;
  GtkCssValue *result;
  GtkCssParser *parser;
  GBytes *bytes;

  prop = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (test->prop);

  bytes = g_bytes_new_static (test->value1, strlen (test->value1));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL, NULL);
  value1 = _gtk_style_property_parse_value (prop, parser);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  bytes = g_bytes_new_static (test->value1, strlen (test->value2));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL, NULL);
  value2 = _gtk_style_property_parse_value (prop, parser);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  bytes = g_bytes_new_static (test->value1, strlen (test->expected));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL, NULL);
  expected = _gtk_style_property_parse_value (prop, parser);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  result = _gtk_css_value_transition (value1, value2, test->prop, test->progress);
  g_assert_true (value_is_near (test->prop, result, expected));

  gtk_css_value_unref (value1);
  gtk_css_value_unref (value2);
  gtk_css_value_unref (expected);
  gtk_css_value_unref (result);
}

int
main (int argc, char **argv)
{
  GtkStyleProperty *previous;
  int j;

  gtk_test_init (&argc, &argv);

  previous = NULL;
  j = 0;
  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      ValueTransitionTest *test = &tests[i];
      GtkStyleProperty *prop;
      char *path;

      prop = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (test->prop);
      if (prop != previous)
        {
          previous = prop;
          j = 0;
        }
      else
        j++;

      path = g_strdup_printf ("/css/value/transition/%s/%d", _gtk_style_property_get_name ((GtkStyleProperty *)prop), j);
      g_test_add_data_func (path, GINT_TO_POINTER (i), test_transition);
      g_free (path);
    }

  return g_test_run ();
}
