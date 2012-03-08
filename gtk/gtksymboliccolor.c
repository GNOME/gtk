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
 * <link linkend="gtkcssprovider-symbolic-colors">color expression</link>.
 * To obtain the color represented by a GtkSymbolicColor, it has to
 * be resolved with gtk_symbolic_color_resolve(), which replaces all
 * symbolic color references by the colors they refer to (in a given
 * context) and evaluates mix, shade and other expressions, resulting
 * in a #GdkRGBA value.
 *
 * It is not normally necessary to deal directly with #GtkSymbolicColors,
 * since they are mostly used behind the scenes by #GtkStyleContext and
 * #GtkCssProvider.
 */

G_DEFINE_BOXED_TYPE (GtkSymbolicColor, gtk_symbolic_color,
                     gtk_symbolic_color_ref, gtk_symbolic_color_unref)

/* Symbolic colors */
typedef enum {
  COLOR_TYPE_LITERAL,
  COLOR_TYPE_NAME,
  COLOR_TYPE_SHADE,
  COLOR_TYPE_ALPHA,
  COLOR_TYPE_MIX,
  COLOR_TYPE_WIN32,
  COLOR_TYPE_CURRENT_COLOR
} ColorType;

struct _GtkSymbolicColor
{
  ColorType type;
  guint ref_count;
  GtkCssValue *last_value;

  union
  {
    gchar *name;

    struct
    {
      GtkSymbolicColor *color;
      gdouble factor;
    } shade, alpha;

    struct
    {
      GtkSymbolicColor *color1;
      GtkSymbolicColor *color2;
      gdouble factor;
    } mix;

    struct
    {
      gchar *theme_class;
      gint id;
    } win32;
  };
};

/**
 * gtk_symbolic_color_new_literal:
 * @color: a #GdkRGBA
 *
 * Creates a symbolic color pointing to a literal color.
 *
 * Returns: a newly created #GtkSymbolicColor
 *
 * Since: 3.0
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_literal (const GdkRGBA *color)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_LITERAL;
  symbolic_color->last_value = _gtk_css_value_new_from_rgba (color);
  symbolic_color->ref_count = 1;

  return symbolic_color;
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
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_name (const gchar *name)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (name != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_NAME;
  symbolic_color->name = g_strdup (name);
  symbolic_color->ref_count = 1;

  return symbolic_color;
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
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_shade (GtkSymbolicColor *color,
                              gdouble           factor)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_SHADE;
  symbolic_color->shade.color = gtk_symbolic_color_ref (color);
  symbolic_color->shade.factor = factor;
  symbolic_color->ref_count = 1;

  return symbolic_color;
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
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_alpha (GtkSymbolicColor *color,
                              gdouble           factor)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_ALPHA;
  symbolic_color->alpha.color = gtk_symbolic_color_ref (color);
  symbolic_color->alpha.factor = factor;
  symbolic_color->ref_count = 1;

  return symbolic_color;
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
 **/
GtkSymbolicColor *
gtk_symbolic_color_new_mix (GtkSymbolicColor *color1,
                            GtkSymbolicColor *color2,
                            gdouble           factor)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color1 != NULL, NULL);
  g_return_val_if_fail (color1 != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_MIX;
  symbolic_color->mix.color1 = gtk_symbolic_color_ref (color1);
  symbolic_color->mix.color2 = gtk_symbolic_color_ref (color2);
  symbolic_color->mix.factor = factor;
  symbolic_color->ref_count = 1;

  return symbolic_color;
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
 */
GtkSymbolicColor *
gtk_symbolic_color_new_win32 (const gchar *theme_class,
                              gint         id)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (theme_class != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_WIN32;
  symbolic_color->win32.theme_class = g_strdup (theme_class);
  symbolic_color->win32.id = id;
  symbolic_color->ref_count = 1;

  return symbolic_color;
}

/**
 * _gtk_symbolic_color_get_current_color:
 *
 * Gets the color representing the CSS 'currentColor' keyword.
 * This color will resolve to the color set for the color property.
 *
 * Returns: (transfer none): The singleton representing the
 *     'currentColor' keyword
 **/
GtkSymbolicColor *
_gtk_symbolic_color_get_current_color (void)
{
  static GtkSymbolicColor *current_color = NULL;

  if (G_UNLIKELY (current_color == NULL))
    {
      current_color = g_slice_new0 (GtkSymbolicColor);
      current_color->type = COLOR_TYPE_CURRENT_COLOR;
      current_color->ref_count = 1;
    }

  return current_color;
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
 **/
void
gtk_symbolic_color_unref (GtkSymbolicColor *color)
{
  g_return_if_fail (color != NULL);

  color->ref_count--;

  if (color->ref_count == 0)
    {
      _gtk_css_value_unref (color->last_value);
      switch (color->type)
	{
	case COLOR_TYPE_NAME:
	  g_free (color->name);
	  break;
	case COLOR_TYPE_SHADE:
	  gtk_symbolic_color_unref (color->shade.color);
	  break;
	case COLOR_TYPE_ALPHA:
	  gtk_symbolic_color_unref (color->alpha.color);
	  break;
	case COLOR_TYPE_MIX:
	  gtk_symbolic_color_unref (color->mix.color1);
	  gtk_symbolic_color_unref (color->mix.color2);
	  break;
	case COLOR_TYPE_WIN32:
	  g_free (color->win32.theme_class);
	  break;
	default:
	  break;
	}

      g_slice_free (GtkSymbolicColor, color);
    }
}

static void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;
  
  red = *r;
  green = *g;
  blue = *b;
  
  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;
      
      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;
      
      if (red < blue)
        min = red;
      else
        min = blue;
    }
  
  l = (max + min) / 2;
  s = 0;
  h = 0;
  
  if (max != min)
    {
      if (l <= 0.5)
        s = (max - min) / (max + min);
      else
        s = (max - min) / (2 - max - min);
      
      delta = max -min;
      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;
      
      h *= 60;
      if (h < 0.0)
        h += 360;
    }
  
  *r = h;
  *g = l;
  *b = s;
}

static void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;
  
  lightness = *l;
  saturation = *s;
  
  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;
  
  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        r = m2;
      else if (hue < 240)
        r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        r = m1;
      
      hue = *h;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        g = m2;
      else if (hue < 240)
        g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        g = m1;
      
      hue = *h - 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        b = m2;
      else if (hue < 240)
        b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        b = m1;
      
      *h = r;
      *l = g;
      *s = b;
    }
}

static void
_shade_color (GdkRGBA *color,
              gdouble  factor)
{
  GdkRGBA temp;

  temp = *color;
  rgb_to_hls (&temp.red, &temp.green, &temp.blue);

  temp.green *= factor;
  if (temp.green > 1.0)
    temp.green = 1.0;
  else if (temp.green < 0.0)
    temp.green = 0.0;

  temp.blue *= factor;
  if (temp.blue > 1.0)
    temp.blue = 1.0;
  else if (temp.blue < 0.0)
    temp.blue = 0.0;

  hls_to_rgb (&temp.red, &temp.green, &temp.blue);
  *color = temp;
}

static GtkSymbolicColor *
resolve_lookup_color (gpointer data, const char *name)
{
  if (data == NULL)
    return NULL;

  return gtk_style_properties_lookup_color (data, name);
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
 * if @color can't be resolved, it is due to it being defined on
 * top of a named color that doesn't exist in @props.
 *
 * When @props is %NULL, resolving of named colors will fail, so if
 * your @color is or references such a color, this function will
 * return %FALSE.
 *
 * Returns: %TRUE if the color has been resolved
 *
 * Since: 3.0
 **/
gboolean
gtk_symbolic_color_resolve (GtkSymbolicColor   *color,
			    GtkStyleProperties *props,
			    GdkRGBA            *resolved_color)
{
  GtkCssValue *v;

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (resolved_color != NULL, FALSE);
  g_return_val_if_fail (props == NULL || GTK_IS_STYLE_PROPERTIES (props), FALSE);

  v =_gtk_symbolic_color_resolve_full (color,
				       resolve_lookup_color,
				       props);
  if (v == NULL)
    return FALSE;

  *resolved_color = *_gtk_css_value_get_rgba (v);
  _gtk_css_value_unref (v);
  return TRUE;
}

GtkCssValue *
_gtk_symbolic_color_resolve_full (GtkSymbolicColor           *color,
				  GtkSymbolicColorLookupFunc  func,
				  gpointer                    data)
{
  GtkCssValue *value;

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (func != NULL, FALSE);

  value = NULL;
  switch (color->type)
    {
    case COLOR_TYPE_LITERAL:
      return _gtk_css_value_ref (color->last_value);
    case COLOR_TYPE_NAME:
      {
	GtkSymbolicColor *named_color;

	named_color = func (data, color->name);

	if (!named_color)
	  return NULL;

	return _gtk_symbolic_color_resolve_full (named_color, func, data);
      }

      break;
    case COLOR_TYPE_SHADE:
      {
	GtkCssValue *val;
	GdkRGBA shade;

	val = _gtk_symbolic_color_resolve_full (color->shade.color, func, data);
	if (val == NULL)
	  return NULL;

	shade = *_gtk_css_value_get_rgba (val);
	_shade_color (&shade, color->shade.factor);

	_gtk_css_value_unref (val);

	value = _gtk_css_value_new_from_rgba (&shade);
      }

      break;
    case COLOR_TYPE_ALPHA:
      {
	GtkCssValue *val;
	GdkRGBA alpha;

	val = _gtk_symbolic_color_resolve_full (color->alpha.color, func, data);
	if (val == NULL)
	  return NULL;

	alpha = *_gtk_css_value_get_rgba (val);
	alpha.alpha = CLAMP (alpha.alpha * color->alpha.factor, 0, 1);

	_gtk_css_value_unref (val);

	value = _gtk_css_value_new_from_rgba (&alpha);
      }
      break;

    case COLOR_TYPE_MIX:
      {
	GtkCssValue *val;
	GdkRGBA color1, color2, res;

	val = _gtk_symbolic_color_resolve_full (color->mix.color1, func, data);
	if (val == NULL)
	  return NULL;
	color1 = *_gtk_css_value_get_rgba (val);
	_gtk_css_value_unref (val);

	val = _gtk_symbolic_color_resolve_full (color->mix.color2, func, data);
	if (val == NULL)
	  return NULL;
	color2 = *_gtk_css_value_get_rgba (val);
	_gtk_css_value_unref (val);


	res.red = CLAMP (color1.red + ((color2.red - color1.red) * color->mix.factor), 0, 1);
	res.green = CLAMP (color1.green + ((color2.green - color1.green) * color->mix.factor), 0, 1);
	res.blue = CLAMP (color1.blue + ((color2.blue - color1.blue) * color->mix.factor), 0, 1);
	res.alpha = CLAMP (color1.alpha + ((color2.alpha - color1.alpha) * color->mix.factor), 0, 1);

	value =_gtk_css_value_new_from_rgba (&res);
      }

      break;
    case COLOR_TYPE_WIN32:
      {
	GdkRGBA res;

	if (!_gtk_win32_theme_color_resolve (color->win32.theme_class,
					     color->win32.id,
					     &res))
	  return NULL;

	value = _gtk_css_value_new_from_rgba (&res);
      }

      break;
    case COLOR_TYPE_CURRENT_COLOR:
      return NULL;
      break;
    default:
      g_assert_not_reached ();
    }

  if (value != NULL)
    {
      if (color->last_value != NULL &&
	  gdk_rgba_equal (_gtk_css_value_get_rgba (color->last_value),
			  _gtk_css_value_get_rgba (value)))
	{
	  _gtk_css_value_unref (value);
	  value = _gtk_css_value_ref (color->last_value);
	}
      else
	{
	  if (color->last_value != NULL)
	    _gtk_css_value_unref (color->last_value);
	  color->last_value = _gtk_css_value_ref (value);
	}
    }

  return value;
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
 **/
char *
gtk_symbolic_color_to_string (GtkSymbolicColor *color)
{
  char *s;

  g_return_val_if_fail (color != NULL, NULL);

  switch (color->type)
    {
    case COLOR_TYPE_LITERAL:
      s = gdk_rgba_to_string (_gtk_css_value_get_rgba (color->last_value));
      break;
    case COLOR_TYPE_NAME:
      s = g_strconcat ("@", color->name, NULL);
      break;
    case COLOR_TYPE_SHADE:
      {
        char *color_string = gtk_symbolic_color_to_string (color->shade.color);
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_ascii_dtostr (factor, sizeof (factor), color->shade.factor);
        s = g_strdup_printf ("shade (%s, %s)", color_string, factor);
        g_free (color_string);
      }
      break;
    case COLOR_TYPE_ALPHA:
      {
        char *color_string = gtk_symbolic_color_to_string (color->shade.color);
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_ascii_dtostr (factor, sizeof (factor), color->alpha.factor);
        s = g_strdup_printf ("alpha (%s, %s)", color_string, factor);
        g_free (color_string);
      }
      break;
    case COLOR_TYPE_MIX:
      {
        char *color_string1 = gtk_symbolic_color_to_string (color->mix.color1);
        char *color_string2 = gtk_symbolic_color_to_string (color->mix.color2);
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_ascii_dtostr (factor, sizeof (factor), color->mix.factor);
        s = g_strdup_printf ("mix (%s, %s, %s)", color_string1, color_string2, factor);
        g_free (color_string1);
        g_free (color_string2);
      }
      break;
    case COLOR_TYPE_WIN32:
      {
        s = g_strdup_printf (GTK_WIN32_THEME_SYMBOLIC_COLOR_NAME"(%s, %d)", 
			     color->win32.theme_class, color->win32.id);
      }
      break;
    case COLOR_TYPE_CURRENT_COLOR:
      s = g_strdup ("currentColor");
      break;
    default:
      g_assert_not_reached ();
    }

  return s;

}
