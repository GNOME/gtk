/* GTK - The GIMP Toolkit
 * Copyright © 2016 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkcssfontsizevalueprivate.h"
#include "gtkcssdimensionvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtksettingsprivate.h"

#include <string.h>

#define DEFAULT_FONT_SIZE_PT 10

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE

  guint is_symbolic: 1;
  union {
    struct {
      GtkCssFontSize value;
      const char *name;
    } symbolic;
    GtkCssValue *number;
  };
};

static void
gtk_css_value_font_size_free (GtkCssValue *value)
{
  if (!value->is_symbolic)
    _gtk_css_value_unref (value->number);

  g_slice_free (GtkCssValue, value);
}

static double
get_dpi (GtkCssStyle *style)
{
  return _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_DPI), 96);
}

double
gtk_css_font_size_get_default_px (GtkStyleProvider *provider,
                                  GtkCssStyle      *style)
{
  GtkSettings *settings;
  int font_size;

  settings = gtk_style_provider_get_settings (provider);
  if (settings == NULL)
    return DEFAULT_FONT_SIZE_PT * get_dpi (style) / 72.0;

  font_size = gtk_settings_get_font_size (settings);
  if (font_size == 0)
    return DEFAULT_FONT_SIZE_PT * get_dpi (style) / 72.0;
  else if (gtk_settings_get_font_size_is_absolute (settings))
    return (double) font_size / PANGO_SCALE;
  else
    return ((double) font_size / PANGO_SCALE) * get_dpi (style) / 72.0;
}

static double
get_base_font_size_px (guint             property_id,
                       GtkStyleProvider *provider,
                       GtkCssStyle      *style,
                       GtkCssStyle      *parent_style)
{
  if (parent_style)
    return gtk_css_font_size_value_get_value (gtk_css_style_get_value (parent_style, GTK_CSS_PROPERTY_FONT_SIZE));
  else
    return gtk_css_font_size_get_default_px (provider, style);
}

static GtkCssValue *
gtk_css_value_font_size_compute (GtkCssValue      *value,
                                 guint             property_id,
                                 GtkStyleProvider *provider,
                                 GtkCssStyle      *style,
                                 GtkCssStyle      *parent_style)
{

  if (value->is_symbolic)
    {
      double font_size;

      switch (value->symbolic.value)
        {
        case GTK_CSS_FONT_SIZE_XX_SMALL:
          font_size = gtk_css_font_size_get_default_px (provider, style) * 3. / 5;
          break;
        case GTK_CSS_FONT_SIZE_X_SMALL:
          font_size = gtk_css_font_size_get_default_px (provider, style) * 3. / 4;
          break;
        case GTK_CSS_FONT_SIZE_SMALL:
          font_size = gtk_css_font_size_get_default_px (provider, style) * 8. / 9;
          break;
        default:
          g_assert_not_reached ();
          /* fall thru */
        case GTK_CSS_FONT_SIZE_MEDIUM:
          font_size = gtk_css_font_size_get_default_px (provider, style);
          break;
        case GTK_CSS_FONT_SIZE_LARGE:
          font_size = gtk_css_font_size_get_default_px (provider, style) * 6. / 5;
          break;
        case GTK_CSS_FONT_SIZE_X_LARGE:
          font_size = gtk_css_font_size_get_default_px (provider, style) * 3. / 2;
          break;
        case GTK_CSS_FONT_SIZE_XX_LARGE:
          font_size = gtk_css_font_size_get_default_px (provider, style) * 2;
          break;
        case GTK_CSS_FONT_SIZE_SMALLER:
          if (parent_style)
            font_size = gtk_css_font_size_value_get_value (gtk_css_style_get_value (parent_style, GTK_CSS_PROPERTY_FONT_SIZE));
          else
            font_size = gtk_css_font_size_get_default_px (provider, style);
          /* XXX: This is what WebKit does... */
          font_size /= 1.2;
          break;
        case GTK_CSS_FONT_SIZE_LARGER:
          if (parent_style)
            font_size = gtk_css_font_size_value_get_value (gtk_css_style_get_value (parent_style, GTK_CSS_PROPERTY_FONT_SIZE));
          else
            font_size = gtk_css_font_size_get_default_px (provider, style);
          /* XXX: This is what WebKit does... */
          font_size *= 1.2;
          break;
        }
      return gtk_css_font_size_value_new (_gtk_css_number_value_new (font_size, GTK_CSS_PX));
    }

  if (gtk_css_number_value_has_percent (value->number) &&
      property_id == GTK_CSS_PROPERTY_FONT_SIZE)
    {
      GtkCssValue *size_value = gtk_css_dimension_value_new (_gtk_css_number_value_get (value->number, 100) / 100.0 *
                                                             get_base_font_size_px (property_id, provider, style, parent_style),
                                                             GTK_CSS_PX);

      return gtk_css_font_size_value_new (size_value);
    }

  return gtk_css_font_size_value_new (_gtk_css_value_compute (value->number,
                                                              property_id,
                                                              provider,
                                                              style,
                                                              parent_style));
}

static gboolean
gtk_css_value_font_size_equal (const GtkCssValue *value1,
                               const GtkCssValue *value2)
{
  if (value1->is_symbolic != value2->is_symbolic)
    return FALSE;

  if (value1->is_symbolic)
    return value1->symbolic.value == value2->symbolic.value;

  return _gtk_css_value_equal0 (value1->number, value2->number);
}

static GtkCssValue *
gtk_css_value_font_size_transition (GtkCssValue *start,
                                    GtkCssValue *end,
                                    guint        property_id,
                                    double       progress)
{
  /* Can't transition symbolic values */
  if (start->is_symbolic || end->is_symbolic)
    return NULL;

  return gtk_css_font_size_value_new (_gtk_css_value_transition (start->number,
                                                                 end->number,
                                                                 property_id,
                                                                 progress));
}

static void
gtk_css_value_font_size_print (const GtkCssValue *value,
                               GString           *string)
{
  if (value->is_symbolic)
    {
      g_string_append (string, value->symbolic.name);
      return;
    }

  _gtk_css_value_print (value->number, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_SIZE = {
  "GtkCssFontSizeValue",
  gtk_css_value_font_size_free,
  gtk_css_value_font_size_compute,
  gtk_css_value_font_size_equal,
  gtk_css_value_font_size_transition,
  NULL,
  NULL,
  gtk_css_value_font_size_print
};

GtkCssValue *
gtk_css_font_size_value_new (GtkCssValue *number)
{
  GtkCssValue *result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_SIZE);

  result->is_symbolic = FALSE;
  result->number = number;
  result->is_computed = gtk_css_value_is_computed (number);

  return result;
}

static GtkCssValue font_size_values[] = {
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_SMALLER, "smaller" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_LARGER, "larger" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_XX_SMALL, "xx-small" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_X_SMALL, "x-small" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_SMALL, "small" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_MEDIUM, "medium" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_LARGE, "large" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_X_LARGE, "x-large" }},
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, TRUE, .symbolic = { GTK_CSS_FONT_SIZE_XX_LARGE, "xx-large" }}
};

GtkCssValue *
gtk_css_font_size_value_new_enum (GtkCssFontSize font_size)
{
  return _gtk_css_value_ref (&font_size_values[font_size]);
}

GtkCssValue *
gtk_css_font_size_value_parse (GtkCssParser *parser)
{
  guint i;
  GtkCssValue *number;

  /* Try enum values first */
  for (i = 0; i < G_N_ELEMENTS (font_size_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_size_values[i].symbolic.name))
        return _gtk_css_value_ref (&font_size_values[i]);
    }

  /* Then try numbers */
  number = _gtk_css_number_value_parse (parser,
                                        GTK_CSS_PARSE_LENGTH |
                                        GTK_CSS_PARSE_PERCENT |
                                        GTK_CSS_POSITIVE_ONLY);

  if (number)
    return gtk_css_font_size_value_new (number);

  return NULL;
}

double
gtk_css_font_size_value_get_value (GtkCssValue *value)
{
  if (value->is_symbolic)
    return 0;

  return _gtk_css_number_value_get (value->number, 100);
}
