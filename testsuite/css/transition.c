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
#include "gtk/gtkcssnumbervalueprivate.h"
#include "gtk/gtkcssstylepropertyprivate.h"
#include "gtk/gtkcssstaticstyleprivate.h"

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
        const GdkRGBA *c1, *c2;
        gboolean res;

        c1 = gtk_css_color_value_get_rgba (value1);
        c2 = gtk_css_color_value_get_rgba (value2);

        res = color_is_near (c1, c2);

        return res;
      }
      break;

    case GTK_CSS_PROPERTY_FONT_SIZE:
      {
        double n1, n2;

        n1 = _gtk_css_number_value_get (value1, 100);
        n2 = _gtk_css_number_value_get (value2, 100);

        return fabs (n1 - n2) < FLT_EPSILON;
      }
      break;

    default:
      break;
    }

  return FALSE;
}

static void
assert_css_value (int          prop,
                  GtkCssValue *result,
                  GtkCssValue *expected)
{
  if (result == expected)
    return;

  if (((result == NULL) != (expected == NULL)) ||
      !value_is_near (prop, result, expected))
    {
      char *r = result ? _gtk_css_value_to_string (result) : g_strdup ("(nil)");
      char *e = expected ? _gtk_css_value_to_string (expected) : g_strdup ("(nil)");
      g_print ("Expected %s got %s\n", e, r);
      g_free (r);
      g_free (e);

      g_assert_not_reached ();
    }
}

/* Tests for css transitions */

typedef struct {
  int prop;
  const char *value1;
  const char *value2;
  double progress;
  const char *value3;
} ValueTransitionTest;

static ValueTransitionTest tests[] = {
  { GTK_CSS_PROPERTY_COLOR, "transparent", "rgb(255,0,0)", 0.25, "rgba(255,0,0,0.25)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "none", "2px 2px 10px 4px rgb(200,200,200)", 0.5, "1px 1px 5px 2px rgba(200,200,200,0.5)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "2px 2px 10px 4px rgb(200,200,200)", "none", 0.5, "1px 1px 5px 2px rgba(200,200,200,0.5)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "2px 2px 10px 4px rgb(200,200,200), 0px 10px 8px 6px rgb(200,100,0)", "none", 0.5, "1px 1px 5px 2px rgba(200,200,200,0.5), 0px 5px 4px 3px rgba(200,100,0,0.5)" },
  { GTK_CSS_PROPERTY_FONT_SIZE, "12px", "16px", 0.25, "13px" },
  { GTK_CSS_PROPERTY_FONT_SIZE, "10px", "10pt", 0.5, "11.66666667px" },
  { GTK_CSS_PROPERTY_FONT_FAMILY, "cantarell", "sans", 0, "cantarell"},
  { GTK_CSS_PROPERTY_FONT_FAMILY, "cantarell", "sans", 1, "sans" },
  { GTK_CSS_PROPERTY_FONT_FAMILY, "cantarell", "sans", 0.5, NULL },
  { GTK_CSS_PROPERTY_BACKGROUND_POSITION, "20px 10px", "40px", 0.5, "30px calc(5px + 25%)" },
  //TODO We don't currently transition border-image-width
  //{ GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH, "10px 20px", "0px", 0.5, "5px 10px 0.5px 0.5px" },
  { GTK_CSS_PROPERTY_FILTER, "none", "blur(6px)", 0.5, "blur(3px)" },
  { GTK_CSS_PROPERTY_FILTER, "none", "blur(6px),contrast(0.6)", 0.5, "blur(3px),contrast(0.3)" },
  { GTK_CSS_PROPERTY_FILTER, "contrast(0.6)", "blur(6px)", 0.5, NULL},
};

static GtkCssValue *
value_from_string (GtkStyleProperty *prop,
                   const char       *str)
{
  GBytes *bytes;
  GtkCssParser *parser;
  GtkCssValue *value;

  bytes = g_bytes_new_static (str, strlen (str));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL, NULL);
  value = _gtk_style_property_parse_value (prop, parser);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return value;
}

static void
test_transition (gconstpointer data)
{
  ValueTransitionTest *test = &tests[GPOINTER_TO_INT (data)];
  GtkStyleProperty *prop;
  GtkCssValue *value1;
  GtkCssValue *value2;
  GtkCssValue *value3;
  GtkCssValue *computed1;
  GtkCssValue *computed2;
  GtkCssValue *computed3;
  GtkCssValue *result;
  GtkStyleProvider *provider;
  GtkCssStyle *style;

  provider = GTK_STYLE_PROVIDER (gtk_settings_get_default ());
  style = gtk_css_static_style_get_default ();

  prop = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (test->prop);

  value1 = value_from_string (prop, test->value1);
  g_assert_nonnull (value1);
  computed1 = _gtk_css_value_compute (value1, test->prop, provider, style, NULL);

  value2 = value_from_string (prop, test->value2);
  g_assert_nonnull (value1);
  computed2 = _gtk_css_value_compute (value2, test->prop, provider, style, NULL);

  if (test->value3)
    {
      value3 = value_from_string (prop, test->value3);
      computed3 = _gtk_css_value_compute (value3, test->prop, provider, style, NULL);
    }
  else
    {
      value3 = computed3 = NULL;
    }

  result = _gtk_css_value_transition (computed1, computed2, test->prop, test->progress);
  assert_css_value (test->prop, result, computed3);

  gtk_css_value_unref (value1);
  gtk_css_value_unref (value2);
  if (value3)
    gtk_css_value_unref (value3);
  gtk_css_value_unref (computed1);
  gtk_css_value_unref (computed2);
  if (computed3)
    gtk_css_value_unref (computed3);
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
