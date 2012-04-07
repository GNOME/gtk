/*
 * Copyright © 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssimagegradientprivate.h"

#include "gtkcssprovider.h"
#include "gtksymboliccolorprivate.h"

G_DEFINE_TYPE (GtkCssImageGradient, _gtk_css_image_gradient, GTK_TYPE_CSS_IMAGE)

static GtkCssImage *
gtk_css_image_gradient_compute (GtkCssImage     *image,
                                GtkStyleContext *context)
{
  GtkCssImageGradient *gradient = GTK_CSS_IMAGE_GRADIENT (image);
  GtkCssImageGradient *copy;

  if (gradient->pattern)
    return g_object_ref (gradient);

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_GRADIENT, NULL);
  copy->gradient = gtk_gradient_ref (gradient->gradient);
  copy->pattern = gtk_gradient_resolve_for_context (copy->gradient, context);

  return GTK_CSS_IMAGE (copy);
}

static void
gtk_css_image_gradient_draw (GtkCssImage        *image,
                             cairo_t            *cr,
                             double              width,
                             double              height)
{
  GtkCssImageGradient *gradient = GTK_CSS_IMAGE_GRADIENT (image);

  if (!gradient->pattern)
    {
      g_warning ("trying to paint unresolved gradient");
      return;
    }

  cairo_scale (cr, width, height);

  cairo_rectangle (cr, 0, 0, 1, 1);
  cairo_set_source (cr, gradient->pattern);
  cairo_fill (cr);
}

static gboolean
gtk_css_image_gradient_parse (GtkCssImage  *image,
                              GtkCssParser *parser,
                              GFile        *base)
{
  GtkCssImageGradient *gradient = GTK_CSS_IMAGE_GRADIENT (image);

  gradient->gradient = _gtk_gradient_parse (parser);

  return gradient->gradient != NULL;
}

static void
gtk_css_image_gradient_print (GtkCssImage *image,
                              GString     *string)
{
  GtkCssImageGradient *gradient = GTK_CSS_IMAGE_GRADIENT (image);
  char *s;

  s = gtk_gradient_to_string (gradient->gradient);
  g_string_append (string, s);
  g_free (s);
}

static void
gtk_css_image_gradient_dispose (GObject *object)
{
  GtkCssImageGradient *gradient = GTK_CSS_IMAGE_GRADIENT (object);

  if (gradient->gradient)
    {
      gtk_gradient_unref (gradient->gradient);
      gradient->gradient = NULL;
    }
  if (gradient->pattern)
    {
      cairo_pattern_destroy (gradient->pattern);
      gradient->pattern = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_gradient_parent_class)->dispose (object);
}

static void
_gtk_css_image_gradient_class_init (GtkCssImageGradientClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->compute = gtk_css_image_gradient_compute;
  image_class->draw = gtk_css_image_gradient_draw;
  image_class->parse = gtk_css_image_gradient_parse;
  image_class->print = gtk_css_image_gradient_print;

  object_class->dispose = gtk_css_image_gradient_dispose;
}

static void
_gtk_css_image_gradient_init (GtkCssImageGradient *image_gradient)
{
}

GtkGradient *
_gtk_gradient_parse (GtkCssParser *parser)
{
  GtkGradient *gradient;
  cairo_pattern_type_t type;
  gdouble coords[6];
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  if (!_gtk_css_parser_try (parser, "-gtk-gradient", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '-gtk-gradient'");
      return NULL;
    }

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '(' after '-gtk-gradient'");
      return NULL;
    }

  /* Parse gradient type */
  if (_gtk_css_parser_try (parser, "linear", TRUE))
    type = CAIRO_PATTERN_TYPE_LINEAR;
  else if (_gtk_css_parser_try (parser, "radial", TRUE))
    type = CAIRO_PATTERN_TYPE_RADIAL;
  else
    {
      _gtk_css_parser_error (parser,
                             "Gradient type must be 'radial' or 'linear'");
      return NULL;
    }

  /* Parse start/stop position parameters */
  for (i = 0; i < 2; i++)
    {
      if (! _gtk_css_parser_try (parser, ",", TRUE))
        {
          _gtk_css_parser_error (parser,
                                 "Expected ','");
          return NULL;
        }

      if (_gtk_css_parser_try (parser, "left", TRUE))
        coords[i * 3] = 0;
      else if (_gtk_css_parser_try (parser, "right", TRUE))
        coords[i * 3] = 1;
      else if (_gtk_css_parser_try (parser, "center", TRUE))
        coords[i * 3] = 0.5;
      else if (!_gtk_css_parser_try_double (parser, &coords[i * 3]))
        {
          _gtk_css_parser_error (parser,
                                 "Expected a valid X coordinate");
          return NULL;
        }

      if (_gtk_css_parser_try (parser, "top", TRUE))
        coords[i * 3 + 1] = 0;
      else if (_gtk_css_parser_try (parser, "bottom", TRUE))
        coords[i * 3 + 1] = 1;
      else if (_gtk_css_parser_try (parser, "center", TRUE))
        coords[i * 3 + 1] = 0.5;
      else if (!_gtk_css_parser_try_double (parser, &coords[i * 3 + 1]))
        {
          _gtk_css_parser_error (parser,
                                 "Expected a valid Y coordinate");
          return NULL;
        }

      if (type == CAIRO_PATTERN_TYPE_RADIAL)
        {
          /* Parse radius */
          if (! _gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser,
                                     "Expected ','");
              return NULL;
            }

          if (! _gtk_css_parser_try_double (parser, &coords[(i * 3) + 2]))
            {
              _gtk_css_parser_error (parser,
                                     "Expected a numer for the radius");
              return NULL;
            }
        }
    }

  if (type == CAIRO_PATTERN_TYPE_LINEAR)
    gradient = gtk_gradient_new_linear (coords[0], coords[1], coords[3], coords[4]);
  else
    gradient = gtk_gradient_new_radial (coords[0], coords[1], coords[2],
                                        coords[3], coords[4], coords[5]);

  while (_gtk_css_parser_try (parser, ",", TRUE))
    {
      GtkSymbolicColor *color;
      gdouble position;

      if (_gtk_css_parser_try (parser, "from", TRUE))
        {
          position = 0;

          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected '('");
              return NULL;
            }

        }
      else if (_gtk_css_parser_try (parser, "to", TRUE))
        {
          position = 1;

          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected '('");
              return NULL;
            }

        }
      else if (_gtk_css_parser_try (parser, "color-stop", TRUE))
        {
          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected '('");
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &position))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected a valid number");
              return NULL;
            }

          if (!_gtk_css_parser_try (parser, ",", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected a comma");
              return NULL;
            }
        }
      else
        {
          gtk_gradient_unref (gradient);
          _gtk_css_parser_error (parser,
                                 "Not a valid color-stop definition");
          return NULL;
        }

      color = _gtk_symbolic_color_new_take_value (_gtk_css_symbolic_value_new (parser));
      if (color == NULL)
        {
          gtk_gradient_unref (gradient);
          return NULL;
        }

      gtk_gradient_add_color_stop (gradient, position, color);
      gtk_symbolic_color_unref (color);

      if (!_gtk_css_parser_try (parser, ")", TRUE))
        {
          gtk_gradient_unref (gradient);
          _gtk_css_parser_error (parser,
                                 "Expected ')'");
          return NULL;
        }
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      gtk_gradient_unref (gradient);
      _gtk_css_parser_error (parser,
                             "Expected ')'");
      return NULL;
    }

  return gradient;
}
