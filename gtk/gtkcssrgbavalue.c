/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcssrgbavalueprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkstylecontextprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GdkRGBA rgba;
};

static void
gtk_css_value_rgba_free (GtkCssValue *value)
{
  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_rgba_compute (GtkCssValue      *value,
                            guint             property_id,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *style,
                            GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_rgba_equal (const GtkCssValue *rgba1,
                          const GtkCssValue *rgba2)
{
  return gdk_rgba_equal (&rgba1->rgba, &rgba2->rgba);
}

static inline double
transition (double start,
            double end,
            double progress)
{
  return start + (end - start) * progress;
}

static GtkCssValue *
gtk_css_value_rgba_transition (GtkCssValue *start,
                               GtkCssValue *end,
                               guint        property_id,
                               double       progress)
{
  GdkRGBA result;

  progress = CLAMP (progress, 0, 1);
  result.alpha = transition (start->rgba.alpha, end->rgba.alpha, progress);
  if (result.alpha <= 0.0)
    {
      result.red = result.green = result.blue = 0.0;
    }
  else
    {
      result.red = transition (start->rgba.red * start->rgba.alpha,
                               end->rgba.red * end->rgba.alpha,
                               progress) / result.alpha;
      result.green = transition (start->rgba.green * start->rgba.alpha,
                                 end->rgba.green * end->rgba.alpha,
                                 progress) / result.alpha;
      result.blue = transition (start->rgba.blue * start->rgba.alpha,
                                end->rgba.blue * end->rgba.alpha,
                                progress) / result.alpha;
    }

  return _gtk_css_rgba_value_new_from_rgba (&result);
}

static void
gtk_css_value_rgba_print (const GtkCssValue *rgba,
                          GString           *string)
{
  char *s = gdk_rgba_to_string (&rgba->rgba);
  g_string_append (string, s);
  g_free (s);
}

static const GtkCssValueClass GTK_CSS_VALUE_RGBA = {
  gtk_css_value_rgba_free,
  gtk_css_value_rgba_compute,
  gtk_css_value_rgba_equal,
  gtk_css_value_rgba_transition,
  NULL,
  NULL,
  gtk_css_value_rgba_print
};

static GtkCssValue transparent_black_singleton = (GtkCssValue) { &GTK_CSS_VALUE_RGBA, 1, { 0, 0, 0, 0 }};
static GtkCssValue transparent_white_singleton = (GtkCssValue) { &GTK_CSS_VALUE_RGBA, 1, { 1, 1, 1, 0 }};
static GtkCssValue opaque_white_singleton      = (GtkCssValue) { &GTK_CSS_VALUE_RGBA, 1, { 1, 1, 1, 1 }};

GtkCssValue *
_gtk_css_rgba_value_new_from_rgba (const GdkRGBA *rgba)
{
  GtkCssValue *value;

  g_return_val_if_fail (rgba != NULL, NULL);

  if (gdk_rgba_is_clear (rgba))
    {
      if (rgba->red == 1 &&
          rgba->green == 1 &&
          rgba->blue == 1)
        return _gtk_css_value_ref (&transparent_white_singleton);

      if (rgba->red == 0 &&
          rgba->green == 0 &&
          rgba->blue == 0)
        return _gtk_css_value_ref (&transparent_black_singleton);
    }
  else if (gdk_rgba_is_opaque (rgba))
    {
      if (rgba->red == 1 &&
          rgba->green == 1 &&
          rgba->blue == 1)
        return _gtk_css_value_ref (&opaque_white_singleton);
    }

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_RGBA);
  value->rgba = *rgba;

  return value;
}

GtkCssValue *
_gtk_css_rgba_value_new_transparent (void)
{
  return _gtk_css_value_ref (&transparent_black_singleton);
}

GtkCssValue *
_gtk_css_rgba_value_new_white (void)
{
  return _gtk_css_value_ref (&opaque_white_singleton);
}

const GdkRGBA *
_gtk_css_rgba_value_get_rgba (const GtkCssValue *rgba)
{
  g_return_val_if_fail (rgba->class == &GTK_CSS_VALUE_RGBA, NULL);

  return &rgba->rgba;
}
