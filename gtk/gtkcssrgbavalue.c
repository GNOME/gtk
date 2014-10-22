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
gtk_css_value_rgba_compute (GtkCssValue             *value,
                            guint                    property_id,
                            GtkStyleProviderPrivate *provider,
			    int                      scale,
                            GtkCssStyle    *values,
                            GtkCssStyle    *parent_values,
                            GtkCssDependencies      *dependencies)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_rgba_equal (const GtkCssValue *rgba1,
                          const GtkCssValue *rgba2)
{
  return gdk_rgba_equal (&rgba1->rgba, &rgba2->rgba);
}

static GtkCssValue *
gtk_css_value_rgba_transition (GtkCssValue *start,
                               GtkCssValue *end,
                               guint        property_id,
                               double       progress)
{
  GdkRGBA transition;

  progress = CLAMP (progress, 0, 1);
  transition.red = start->rgba.red + (end->rgba.red - start->rgba.red) * progress;
  transition.green = start->rgba.green + (end->rgba.green - start->rgba.green) * progress;
  transition.blue = start->rgba.blue + (end->rgba.blue - start->rgba.blue) * progress;
  transition.alpha = start->rgba.alpha + (end->rgba.alpha - start->rgba.alpha) * progress;

  return _gtk_css_rgba_value_new_from_rgba (&transition);
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
