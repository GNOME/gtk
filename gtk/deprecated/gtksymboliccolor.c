/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkhslaprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtksymboliccolorprivate.h"
#include "gtkstyleproperties.h"
#include "gtkintl.h"
#include "gtkwin32themeprivate.h"

/**
 * SECTION:gtksymboliccolor
 * @Short_description: Symbolic colors
 * @Title: GtkSymbolicColor
 *
 * GtkSymbolicColor is a boxed type that represents a symbolic color.
 * It is the result of parsing a
 * [color expression][gtkcssprovider-symbolic-colors].
 * To obtain the color represented by a GtkSymbolicColor, it has to
 * be resolved with gtk_symbolic_color_resolve(), which replaces all
 * symbolic color references by the colors they refer to (in a given
 * context) and evaluates mix, shade and other expressions, resulting
 * in a #GdkRGBA value.
 *
 * It is not normally necessary to deal directly with #GtkSymbolicColors,
 * since they are mostly used behind the scenes by #GtkStyleContext and
 * #GtkCssProvider.
 *
 * #GtkSymbolicColor is deprecated. Symbolic colors are considered an
 * implementation detail of GTK+.
 */

G_DEFINE_BOXED_TYPE (GtkSymbolicColor, gtk_symbolic_color,
                     gtk_symbolic_color_ref, gtk_symbolic_color_unref)

struct _GtkSymbolicColor
{
  GtkCssValue *value;
  gint ref_count;
};

static GtkSymbolicColor *
gtk_symbolic_color_new (GtkCssValue *value)
{
  GtkSymbolicColor *symbolic;

  symbolic = g_slice_new0 (GtkSymbolicColor);
  symbolic->value = value;
  symbolic->ref_count = 1;

  return symbolic;
}

/**
 * gtk_symbolic_color_new_literal:
 * @color: a #GdkRGBA
 *
 * Creates a symbolic color pointing to a literal color.
 *
 * Returns: a newly created #GtkSymbolicColor
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_literal (const GdkRGBA *color)
{
  g_return_val_if_fail (color != NULL, NULL);

  return gtk_symbolic_color_new (_gtk_css_color_value_new_literal (color));
}

/**
 * gtk_symbolic_color_new_name:
 * @name: color name
 *
 * Creates a symbolic color pointing to an unresolved named
 * color. See gtk_style_context_lookup_color() and
 * gtk_style_properties_lookup_color().
 *
 * Returns: a newly created #GtkSymbolicColor
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_name (const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return gtk_symbolic_color_new (_gtk_css_color_value_new_name (name));
}

/**
 * gtk_symbolic_color_new_shade: (constructor)
 * @color: another #GtkSymbolicColor
 * @factor: shading factor to apply to @color
 *
 * Creates a symbolic color defined as a shade of
 * another color. A factor > 1.0 would resolve to
 * a brighter color, while < 1.0 would resolve to
 * a darker color.
 *
 * Returns: A newly created #GtkSymbolicColor
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_shade (GtkSymbolicColor *color,
                              gdouble           factor)
{
  g_return_val_if_fail (color != NULL, NULL);

  return gtk_symbolic_color_new (_gtk_css_color_value_new_shade (color->value,
                                                                 factor));
}

/**
 * gtk_symbolic_color_new_alpha: (constructor)
 * @color: another #GtkSymbolicColor
 * @factor: factor to apply to @color alpha
 *
 * Creates a symbolic color by modifying the relative alpha
 * value of @color. A factor < 1.0 would resolve to a more
 * transparent color, while > 1.0 would resolve to a more
 * opaque color.
 *
 * Returns: A newly created #GtkSymbolicColor
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_alpha (GtkSymbolicColor *color,
                              gdouble           factor)
{
  g_return_val_if_fail (color != NULL, NULL);

  return gtk_symbolic_color_new (_gtk_css_color_value_new_alpha (color->value,
                                                                 factor));
}

/**
 * gtk_symbolic_color_new_mix: (constructor)
 * @color1: color to mix
 * @color2: another color to mix
 * @factor: mix factor
 *
 * Creates a symbolic color defined as a mix of another
 * two colors. a mix factor of 0 would resolve to @color1,
 * while a factor of 1 would resolve to @color2.
 *
 * Returns: A newly created #GtkSymbolicColor
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_mix (GtkSymbolicColor *color1,
                            GtkSymbolicColor *color2,
                            gdouble           factor)
{
  g_return_val_if_fail (color1 != NULL, NULL);
  g_return_val_if_fail (color1 != NULL, NULL);

  return gtk_symbolic_color_new (_gtk_css_color_value_new_mix (color1->value,
                                                               color2->value,
                                                               factor));
}

/**
 * gtk_symbolic_color_new_win32: (constructor)
 * @theme_class: The theme class to pull color from
 * @id: The color id
 *
 * Creates a symbolic color based on the current win32
 * theme.
 *
 * Note that while this call is available on all platforms
 * the actual value returned is not reliable on non-win32
 * platforms.
 *
 * Returns: A newly created #GtkSymbolicColor
 *
 * Since: 3.4
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 */
GtkSymbolicColor *
gtk_symbolic_color_new_win32 (const gchar *theme_class,
                              gint         id)
{
  g_return_val_if_fail (theme_class != NULL, NULL);

  return gtk_symbolic_color_new (_gtk_css_color_value_new_win32 (theme_class, id));
}

/**
 * gtk_symbolic_color_ref:
 * @color: a #GtkSymbolicColor
 *
 * Increases the reference count of @color
 *
 * Returns: the same @color
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
gtk_symbolic_color_ref (GtkSymbolicColor *color)
{
  g_return_val_if_fail (color != NULL, NULL);

  color->ref_count++;

  return color;
}

/**
 * gtk_symbolic_color_unref:
 * @color: a #GtkSymbolicColor
 *
 * Decreases the reference count of @color, freeing its memory if the
 * reference count reaches 0.
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
void
gtk_symbolic_color_unref (GtkSymbolicColor *color)
{
  g_return_if_fail (color != NULL);

  if (--color->ref_count)
    return;

  _gtk_css_value_unref (color->value);

  g_slice_free (GtkSymbolicColor, color);
}

/**
 * gtk_symbolic_color_resolve:
 * @color: a #GtkSymbolicColor
 * @props: (allow-none): #GtkStyleProperties to use when resolving
 *    named colors, or %NULL
 * @resolved_color: (out): return location for the resolved color
 *
 * If @color is resolvable, @resolved_color will be filled in
 * with the resolved color, and %TRUE will be returned. Generally,
 * if @color can’t be resolved, it is due to it being defined on
 * top of a named color that doesn’t exist in @props.
 *
 * When @props is %NULL, resolving of named colors will fail, so if
 * your @color is or references such a color, this function will
 * return %FALSE.
 *
 * Returns: %TRUE if the color has been resolved
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
gboolean
gtk_symbolic_color_resolve (GtkSymbolicColor   *color,
			    GtkStyleProperties *props,
			    GdkRGBA            *resolved_color)
{
  GdkRGBA pink = { 1.0, 0.5, 0.5, 1.0 };
  GtkCssValue *v, *current;

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (resolved_color != NULL, FALSE);
  g_return_val_if_fail (props == NULL || GTK_IS_STYLE_PROPERTIES (props), FALSE);

  current = _gtk_css_rgba_value_new_from_rgba (&pink);
  v = _gtk_css_color_value_resolve (color->value,
                                    GTK_STYLE_PROVIDER_PRIVATE (props),
                                    current,
                                    0,
                                    NULL,
                                    NULL);
  _gtk_css_value_unref (current);
  if (v == NULL)
    return FALSE;

  *resolved_color = *_gtk_css_rgba_value_get_rgba (v);
  _gtk_css_value_unref (v);
  return TRUE;
}

/**
 * gtk_symbolic_color_to_string:
 * @color: color to convert to a string
 *
 * Converts the given @color to a string representation. This is useful
 * both for debugging and for serialization of strings. The format of
 * the string may change between different versions of GTK, but it is
 * guaranteed that the GTK css parser is able to read the string and
 * create the same symbolic color from it.
 *
 * Returns: a new string representing @color
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
char *
gtk_symbolic_color_to_string (GtkSymbolicColor *color)
{
  g_return_val_if_fail (color != NULL, NULL);

  return _gtk_css_value_to_string (color->value);
}

GtkSymbolicColor *
_gtk_css_symbolic_value_new (GtkCssParser *parser)
{
  GtkCssValue *value;

  value = _gtk_css_color_value_parse (parser);
  if (value == NULL)
    return NULL;

  return gtk_symbolic_color_new (value);
}

GtkCssValue *
_gtk_symbolic_color_get_css_value (GtkSymbolicColor *symbolic)
{
  return symbolic->value;
}

