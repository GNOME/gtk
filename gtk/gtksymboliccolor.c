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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gtksymboliccolor.h"
#include "gtkstyleproperties.h"
#include "gtkintl.h"

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
  COLOR_TYPE_MIX
} ColorType;

struct _GtkSymbolicColor
{
  ColorType type;
  guint ref_count;

  union
  {
    GdkRGBA color;
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
  symbolic_color->color = *color;
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
 * gtk_symbolic_color_new_shade:
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
 * gtk_symbolic_color_new_alpha:
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
 * gtk_symbolic_color_new_mix:
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
 * @props must be non-%NULL if @color was created using
 * gtk_symbolic_color_named_new(), but can be omitted in other cases.
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
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (resolved_color != NULL, FALSE);

  switch (color->type)
    {
    case COLOR_TYPE_LITERAL:
      *resolved_color = color->color;
      return TRUE;
    case COLOR_TYPE_NAME:
      {
        GtkSymbolicColor *named_color;

        g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), FALSE);

        named_color = gtk_style_properties_lookup_color (props, color->name);

        if (!named_color)
          return FALSE;

        return gtk_symbolic_color_resolve (named_color, props, resolved_color);
      }

      break;
    case COLOR_TYPE_SHADE:
      {
        GdkRGBA shade;

        if (!gtk_symbolic_color_resolve (color->shade.color, props, &shade))
          return FALSE;

        _shade_color (&shade, color->shade.factor);
        *resolved_color = shade;

        return TRUE;
      }

      break;
    case COLOR_TYPE_ALPHA:
      {
        GdkRGBA alpha;

        if (!gtk_symbolic_color_resolve (color->alpha.color, props, &alpha))
          return FALSE;

        *resolved_color = alpha;
        resolved_color->alpha = CLAMP (alpha.alpha * color->alpha.factor, 0, 1);

        return TRUE;
      }
    case COLOR_TYPE_MIX:
      {
        GdkRGBA color1, color2;

        if (!gtk_symbolic_color_resolve (color->mix.color1, props, &color1))
          return FALSE;

        if (!gtk_symbolic_color_resolve (color->mix.color2, props, &color2))
          return FALSE;

        resolved_color->red = CLAMP (color1.red + ((color2.red - color1.red) * color->mix.factor), 0, 1);
        resolved_color->green = CLAMP (color1.green + ((color2.green - color1.green) * color->mix.factor), 0, 1);
        resolved_color->blue = CLAMP (color1.blue + ((color2.blue - color1.blue) * color->mix.factor), 0, 1);
        resolved_color->alpha = CLAMP (color1.alpha + ((color2.alpha - color1.alpha) * color->mix.factor), 0, 1);

        return TRUE;
      }

      break;
    default:
      g_assert_not_reached ();
    }

  return FALSE;
}
