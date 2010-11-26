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
#include "gtkgradient.h"
#include "gtkstyleproperties.h"
#include "gtkintl.h"

/**
 * SECTION:gtkgradient
 * @Short_description: Gradients
 * @Title: GtkGradient
 *
 * GtkGradient is a boxed type that represents a gradient.
 * It is the result of parsing a
 * <link linkend="gtkcssprovider-gradients">gradient expression</link>.
 * To obtain the gradient represented by a GtkGradient, it has to
 * be resolved with gtk_gradient_resolve(), which replaces all
 * symbolic color references by the colors they refer to (in a given
 * context) and constructs a #cairo_pattern_t value.
 *
 * It is not normally necessary to deal directly with #GtkGradients,
 * since they are mostly used behind the scenes by #GtkStyleContext and
 * #GtkCssProvider.
 */

G_DEFINE_BOXED_TYPE (GtkGradient, gtk_gradient,
                     gtk_gradient_ref, gtk_gradient_unref)

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
 * @props: #GtkStyleProperties to use when resolving named colors
 * @resolved_gradient: (out): return location for the resolved pattern
 *
 * If @gradient is resolvable, @resolved_gradient will be filled in
 * with the resolved gradient as a cairo_pattern_t, and %TRUE will
 * be returned. Generally, if @gradient can't be resolved, it is
 * due to it being defined on top of a named color that doesn't
 * exist in @props.
 *
 * Returns: %TRUE if the gradient has been resolved
 *
 * Since: 3.0
 **/
gboolean
gtk_gradient_resolve (GtkGradient         *gradient,
                      GtkStyleProperties  *props,
                      cairo_pattern_t    **resolved_gradient)
{
  cairo_pattern_t *pattern;
  guint i;

  g_return_val_if_fail (gradient != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), FALSE);
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

      if (!gtk_symbolic_color_resolve (stop->color, props, &color))
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
