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
#include "gtkstyleset.h"
#include "gtkintl.h"

G_DEFINE_BOXED_TYPE (GtkSymbolicColor, gtk_symbolic_color,
		     gtk_symbolic_color_ref, gtk_symbolic_color_unref)
G_DEFINE_BOXED_TYPE (GtkGradient, gtk_gradient,
                     gtk_gradient_ref, gtk_gradient_unref)

/* Symbolic colors */
typedef enum {
  COLOR_TYPE_LITERAL,
  COLOR_TYPE_NAME,
  COLOR_TYPE_SHADE,
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
    } shade;

    struct
    {
      GtkSymbolicColor *color1;
      GtkSymbolicColor *color2;
      gdouble factor;
    } mix;
  };
};

typedef struct ColorStop ColorStop;

struct ColorStop
{
  gdouble offset;
  GtkSymbolicColor *color;
};

struct _GtkGradient
{
  gdouble x0;
  gdouble y0;
  gdouble x1;
  gdouble y1;
  gdouble radius0;
  gdouble radius1;

  GArray *stops;

  guint ref_count;
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
gtk_symbolic_color_new_literal (GdkRGBA *color)
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
 * gtk_style_set_lookup_color().
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
  symbolic_color->shade.factor = CLAMP (factor, 0, 1);
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
  symbolic_color->mix.factor = CLAMP (factor, 0, 1);
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

/**
 * gtk_symbolic_color_resolve:
 * @color: a #GtkSymbolicColor
 * @style_set: #GtkStyleSet to use when resolving named colors
 * @resolved_color: (out): return location for the resolved color
 *
 * If @color is resolvable, @resolved_color will be filled in
 * with the resolved color, and %TRUE will be returned. Generally,
 * if @color can't be resolved, it is due to it being defined on
 * top of a named color that doesn't exist in @style_set.
 *
 * Returns: %TRUE if the color has been resolved
 *
 * Since: 3.0
 **/
gboolean
gtk_symbolic_color_resolve (GtkSymbolicColor *color,
                            GtkStyleSet      *style_set,
                            GdkRGBA          *resolved_color)
{
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_STYLE_SET (style_set), FALSE);
  g_return_val_if_fail (resolved_color != NULL, FALSE);

  switch (color->type)
    {
    case COLOR_TYPE_LITERAL:
      *resolved_color = color->color;
      return TRUE;
    case COLOR_TYPE_NAME:
      {
        GtkSymbolicColor *named_color;

        named_color = gtk_style_set_lookup_color (style_set, color->name);

        if (!named_color)
          return FALSE;

        return gtk_symbolic_color_resolve (named_color, style_set, resolved_color);
      }

      break;
    case COLOR_TYPE_SHADE:
      {
        GdkRGBA shade;

        if (!gtk_symbolic_color_resolve (color->shade.color, style_set, &shade))
          return FALSE;

        resolved_color->red = CLAMP (shade.red * color->shade.factor, 0, 1);
        resolved_color->green = CLAMP (shade.green * color->shade.factor, 0, 1);
        resolved_color->blue = CLAMP (shade.blue * color->shade.factor, 0, 1);

        return TRUE;
      }

      break;
    case COLOR_TYPE_MIX:
      {
        GdkRGBA color1, color2;

        if (!gtk_symbolic_color_resolve (color->mix.color1, style_set, &color1))
          return FALSE;

        if (!gtk_symbolic_color_resolve (color->mix.color2, style_set, &color2))
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

/* GtkGradient */
/**
 * gtk_gradient_new_linear:
 * @x0: X coordinate of the starting point
 * @y0: Y coordinate of the starting point
 * @x1: X coordinate of the end point
 * @y1: Y coordinate of the end point
 *
 * Creates a new linear gradient along the line defined by (x0, y0) and (x1, y1). Before using the gradient
 * a number of stop colors must be added through gtk_gradient_add_color_stop().
 *
 * Returns: A newly created #GtkGradient
 *
 * Since: 3.0
 **/
GtkGradient *
gtk_gradient_new_linear (gdouble x0,
                         gdouble y0,
                         gdouble x1,
                         gdouble y1)
{
  GtkGradient *gradient;

  gradient = g_slice_new (GtkGradient);
  gradient->stops = g_array_new (FALSE, FALSE, sizeof (ColorStop));

  gradient->x0 = x0;
  gradient->y0 = y0;
  gradient->x1 = x1;
  gradient->y1 = y1;
  gradient->radius0 = 0;
  gradient->radius1 = 0;

  gradient->ref_count = 1;

  return gradient;
}

/**
 * gtk_gradient_new_radial:
 * @x0: X coordinate of the start circle
 * @y0: Y coordinate of the start circle
 * @radius0: radius of the start circle
 * @x1: X coordinate of the end circle
 * @y1: Y coordinate of the end circle
 * @radius1: radius of the end circle
 *
 * Creates a new radial gradient along the two circles defined by (x0, y0, radius0) and
 * (x1, y1, radius1). Before using the gradient a number of stop colors must be added
 * through gtk_gradient_add_color_stop().
 *
 * Returns: A newly created #GtkGradient
 *
 * Since: 3.0
 **/
GtkGradient *
gtk_gradient_new_radial (gdouble x0,
			 gdouble y0,
			 gdouble radius0,
			 gdouble x1,
			 gdouble y1,
			 gdouble radius1)
{
  GtkGradient *gradient;

  gradient = g_slice_new (GtkGradient);
  gradient->stops = g_array_new (FALSE, FALSE, sizeof (ColorStop));

  gradient->x0 = x0;
  gradient->y0 = y0;
  gradient->x1 = x1;
  gradient->y1 = y1;
  gradient->radius0 = radius0;
  gradient->radius1 = radius1;

  gradient->ref_count = 1;

  return gradient;
}

/**
 * gtk_gradient_add_color_stop:
 * @gradient: a #GtkGradient
 * @offset: offset for the color stop
 * @color: color to use
 *
 * Adds a stop color to @gradient.
 *
 * Since: 3.0
 **/
void
gtk_gradient_add_color_stop (GtkGradient      *gradient,
                             gdouble           offset,
                             GtkSymbolicColor *color)
{
  ColorStop stop;

  g_return_if_fail (gradient != NULL);

  stop.offset = offset;
  stop.color = gtk_symbolic_color_ref (color);

  g_array_append_val (gradient->stops, stop);
}

/**
 * gtk_gradient_ref:
 * @gradient: a #GtkGradient
 *
 * Increases the reference count of @gradient.
 *
 * Returns: The same @gradient
 *
 * Since: 3.0
 **/
GtkGradient *
gtk_gradient_ref (GtkGradient *gradient)
{
  g_return_val_if_fail (gradient != NULL, NULL);

  gradient->ref_count++;

  return gradient;
}

/**
 * gtk_gradient_unref:
 * @gradient: a #GtkGradient
 *
 * Decreases the reference count of @gradient, freeing its memory
 * if the reference count reaches 0.
 *
 * Since: 3.0
 **/
void
gtk_gradient_unref (GtkGradient *gradient)
{
  g_return_if_fail (gradient != NULL);

  gradient->ref_count--;

  if (gradient->ref_count == 0)
    {
      guint i;

      for (i = 0; i < gradient->stops->len; i++)
        {
          ColorStop *stop;

          stop = &g_array_index (gradient->stops, ColorStop, i);
          gtk_symbolic_color_unref (stop->color);
        }

      g_array_free (gradient->stops, TRUE);
      g_slice_free (GtkGradient, gradient);
    }
}

/**
 * gtk_gradient_resolve:
 * @gradient: a #GtkGradient
 * @style_set: #GtkStyleSet to use when resolving named colors
 * @resolved_gradient: (out): return location for the resolved pattern
 *
 * If @gradient is resolvable, @resolved_gradient will be filled in
 * with the resolved gradient as a cairo_pattern_t, and %TRUE will
 * be returned. Generally, if @gradient can't be resolved, it is
 * due to it being defined on top of a named color that doesn't
 * exist in @style_set.
 *
 * Returns: %TRUE if the gradient has been resolved
 *
 * Since: 3.0
 **/
gboolean
gtk_gradient_resolve (GtkGradient      *gradient,
                      GtkStyleSet      *style_set,
                      cairo_pattern_t **resolved_gradient)
{
  cairo_pattern_t *pattern;
  guint i;

  g_return_val_if_fail (gradient != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_STYLE_SET (style_set), FALSE);
  g_return_val_if_fail (resolved_gradient != NULL, FALSE);

  if (gradient->radius0 == 0 && gradient->radius1 == 0)
    pattern = cairo_pattern_create_linear (gradient->x0, gradient->y0,
                                           gradient->x1, gradient->y1);
  else
    pattern = cairo_pattern_create_radial (gradient->x0, gradient->y0,
                                           gradient->radius0,
                                           gradient->x1, gradient->y1,
                                           gradient->radius1);

  for (i = 0; i < gradient->stops->len; i++)
    {
      ColorStop *stop;
      GdkRGBA color;

      stop = &g_array_index (gradient->stops, ColorStop, i);

      if (!gtk_symbolic_color_resolve (stop->color, style_set, &color))
        {
          cairo_pattern_destroy (pattern);
          return FALSE;
        }

      cairo_pattern_add_color_stop_rgba (pattern, stop->offset,
                                         color.red, color.green,
                                         color.blue, color.alpha);
    }

  *resolved_gradient = pattern;
  return TRUE;
}
