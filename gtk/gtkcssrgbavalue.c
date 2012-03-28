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

#include "gtkstylecontextprivate.h"
#include "gtksymboliccolorprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GdkRGBA rgba;
};

static void
gtk_css_value_rgba_free (GtkCssValue *value)
{
  g_slice_free (GtkCssValue, value);
}

static gboolean
gtk_css_value_rgba_equal (const GtkCssValue *rgba1,
                          const GtkCssValue *rgba2)
{
  return gdk_rgba_equal (&rgba1->rgba, &rgba2->rgba);
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
  gtk_css_value_rgba_equal,
  gtk_css_value_rgba_print
};

GtkCssValue *
_gtk_css_rgba_value_new_from_rgba (const GdkRGBA *rgba)
{
  GtkCssValue *value;

  g_return_val_if_fail (rgba != NULL, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_RGBA);
  value->rgba = *rgba;

  return value;
}

const GdkRGBA *
_gtk_css_rgba_value_get_rgba (const GtkCssValue *rgba)
{
  g_return_val_if_fail (rgba->class == &GTK_CSS_VALUE_RGBA, NULL);

  return &rgba->rgba;
}

GtkCssValue *
_gtk_css_rgba_value_compute_from_symbolic (GtkCssValue     *rgba,
                                           GtkCssValue     *fallback,
                                           GtkStyleContext *context,
                                           gboolean         for_color_property)
{
  GtkSymbolicColor *symbolic;
  GtkCssValue *resolved;

  g_return_val_if_fail (rgba != NULL, NULL);

  symbolic = _gtk_css_value_get_symbolic_color (rgba);

  if (symbolic == _gtk_symbolic_color_get_current_color ())
    {
      /* The computed value of the ‘currentColor’ keyword is the computed
       * value of the ‘color’ property. If the ‘currentColor’ keyword is
       * set on the ‘color’ property itself, it is treated as ‘color: inherit’. 
       */
      if (for_color_property)
        {
          GtkStyleContext *parent = gtk_style_context_get_parent (context);

          if (parent)
            return _gtk_css_value_ref (_gtk_style_context_peek_property (parent, "color"));
          else
            return _gtk_css_rgba_value_compute_from_symbolic (fallback, NULL, context, TRUE);
        }
      else
        {
          return _gtk_css_value_ref (_gtk_style_context_peek_property (context, "color"));
        }
    }
  
  resolved = _gtk_style_context_resolve_color_value (context, symbolic);

  if (resolved == NULL)
    return _gtk_css_rgba_value_compute_from_symbolic (fallback, NULL, context, for_color_property);

  g_assert (resolved->class == &GTK_CSS_VALUE_RGBA);
  return resolved;
}

