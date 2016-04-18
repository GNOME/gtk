/*
 * Copyright © 2012 Red Hat Inc.
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

#include "gtkcssimagelinearprivate.h"

#include <math.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssprovider.h"

G_DEFINE_TYPE (GtkCssImageLinear, _gtk_css_image_linear, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_linear_get_start_end (GtkCssImageLinear *linear,
                                    double             length,
                                    double            *start,
                                    double            *end)
{
  GtkCssImageLinearColorStop *stop;
  double pos;
  guint i;
      
  if (linear->repeating)
    {
      stop = &g_array_index (linear->stops, GtkCssImageLinearColorStop, 0);
      if (stop->offset == NULL)
        *start = 0;
      else
        *start = _gtk_css_number_value_get (stop->offset, length) / length;

      *end = *start;

      for (i = 0; i < linear->stops->len; i++)
        {
          stop = &g_array_index (linear->stops, GtkCssImageLinearColorStop, i);
          
          if (stop->offset == NULL)
            continue;

          pos = _gtk_css_number_value_get (stop->offset, length) / length;

          *end = MAX (pos, *end);
        }
      
      if (stop->offset == NULL)
        *end = MAX (*end, 1.0);
    }
  else
    {
      *start = 0;
      *end = 1;
    }
}

static void
gtk_css_image_linear_compute_start_point (double angle_in_degrees,
                                          double width,
                                          double height,
                                          double *x,
                                          double *y)
{
  double angle, c, slope, perpendicular;
  
  angle = fmod (angle_in_degrees, 360);
  if (angle < 0)
    angle += 360;

  if (angle == 0)
    {
      *x = 0;
      *y = -height;
      return;
    }
  else if (angle == 90)
    {
      *x = width;
      *y = 0;
      return;
    }
  else if (angle == 180)
    {
      *x = 0;
      *y = height;
      return;
    }
  else if (angle == 270)
    {
      *x = -width;
      *y = 0;
      return;
    }

  /* The tan() is confusing because the angle is clockwise
   * from 'to top' */
  perpendicular = tan (angle * G_PI / 180);
  slope = -1 / perpendicular;

  if (angle > 180)
    width = - width;
  if (angle < 90 || angle > 270)
    height = - height;
  
  /* Compute c (of y = mx + c) of perpendicular */
  c = height - perpendicular * width;

  *x = c / (slope - perpendicular);
  *y = perpendicular * *x + c;
}
                                         
static void
gtk_css_image_linear_draw (GtkCssImage        *image,
                           cairo_t            *cr,
                           double              width,
                           double              height)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  cairo_pattern_t *pattern;
  double angle; /* actual angle of the gradiant line in degrees */
  double x, y; /* coordinates of start point */
  double length; /* distance in pixels for 100% */
  double start, end; /* position of first/last point on gradient line - with gradient line being [0, 1] */
  double offset;
  int i, last;

  if (linear->side)
    {
      /* special casing the regular cases here so we don't get rounding errors */
      switch (linear->side)
      {
        case 1 << GTK_CSS_RIGHT:
          angle = 90;
          break;
        case 1 << GTK_CSS_LEFT:
          angle = 270;
          break;
        case 1 << GTK_CSS_TOP:
          angle = 0;
          break;
        case 1 << GTK_CSS_BOTTOM:
          angle = 180;
          break;
        default:
          angle = atan2 (linear->side & 1 << GTK_CSS_TOP ? -width : width,
                         linear->side & 1 << GTK_CSS_LEFT ? -height : height);
          angle = 180 * angle / G_PI + 90;
          break;
      }
    }
  else
    {
      angle = _gtk_css_number_value_get (linear->angle, 100);
    }

  gtk_css_image_linear_compute_start_point (angle,
                                            width, height,
                                            &x, &y);

  length = sqrt (x * x + y * y);
  gtk_css_image_linear_get_start_end (linear, length, &start, &end);
  pattern = cairo_pattern_create_linear (x * (start - 0.5), y * (start - 0.5),
                                         x * (end - 0.5),   y * (end - 0.5));
  if (linear->repeating)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  else
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  offset = start;
  last = -1;
  for (i = 0; i < linear->stops->len; i++)
    {
      GtkCssImageLinearColorStop *stop;
      double pos, step;
      
      stop = &g_array_index (linear->stops, GtkCssImageLinearColorStop, i);

      if (stop->offset == NULL)
        {
          if (i == 0)
            pos = 0.0;
          else if (i + 1 == linear->stops->len)
            pos = 1.0;
          else
            continue;
        }
      else
        pos = _gtk_css_number_value_get (stop->offset, length) / length;

      pos = MAX (pos, offset);
      step = (pos - offset) / (i - last);
      for (last = last + 1; last <= i; last++)
        {
          const GdkRGBA *rgba;

          stop = &g_array_index (linear->stops, GtkCssImageLinearColorStop, last);

          rgba = _gtk_css_rgba_value_get_rgba (stop->color);
          offset += step;

          cairo_pattern_add_color_stop_rgba (pattern,
                                             (offset - start) / (end - start),
                                             rgba->red,
                                             rgba->green,
                                             rgba->blue,
                                             rgba->alpha);
        }

      offset = pos;
      last = i;
    }

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_translate (cr, width / 2, height / 2);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
}


static gboolean
gtk_css_image_linear_parse (GtkCssImage  *image,
                            GtkCssParser *parser)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  guint i;

  if (_gtk_css_parser_try (parser, "repeating-linear-gradient(", TRUE))
    linear->repeating = TRUE;
  else if (_gtk_css_parser_try (parser, "linear-gradient(", TRUE))
    linear->repeating = FALSE;
  else
    {
      _gtk_css_parser_error (parser, "Not a linear gradient");
      return FALSE;
    }

  if (_gtk_css_parser_try (parser, "to", TRUE))
    {
      for (i = 0; i < 2; i++)
        {
          if (_gtk_css_parser_try (parser, "left", TRUE))
            {
              if (linear->side & ((1 << GTK_CSS_LEFT) | (1 << GTK_CSS_RIGHT)))
                {
                  _gtk_css_parser_error (parser, "Expected 'top', 'bottom' or comma");
                  return FALSE;
                }
              linear->side |= (1 << GTK_CSS_LEFT);
            }
          else if (_gtk_css_parser_try (parser, "right", TRUE))
            {
              if (linear->side & ((1 << GTK_CSS_LEFT) | (1 << GTK_CSS_RIGHT)))
                {
                  _gtk_css_parser_error (parser, "Expected 'top', 'bottom' or comma");
                  return FALSE;
                }
              linear->side |= (1 << GTK_CSS_RIGHT);
            }
          else if (_gtk_css_parser_try (parser, "top", TRUE))
            {
              if (linear->side & ((1 << GTK_CSS_TOP) | (1 << GTK_CSS_BOTTOM)))
                {
                  _gtk_css_parser_error (parser, "Expected 'left', 'right' or comma");
                  return FALSE;
                }
              linear->side |= (1 << GTK_CSS_TOP);
            }
          else if (_gtk_css_parser_try (parser, "bottom", TRUE))
            {
              if (linear->side & ((1 << GTK_CSS_TOP) | (1 << GTK_CSS_BOTTOM)))
                {
                  _gtk_css_parser_error (parser, "Expected 'left', 'right' or comma");
                  return FALSE;
                }
              linear->side |= (1 << GTK_CSS_BOTTOM);
            }
          else
            break;
        }

      if (linear->side == 0)
        {
          _gtk_css_parser_error (parser, "Expected side that gradient should go to");
          return FALSE;
        }

      if (!_gtk_css_parser_try (parser, ",", TRUE))
        {
          _gtk_css_parser_error (parser, "Expected a comma");
          return FALSE;
        }
    }
  else if (gtk_css_number_value_can_parse (parser))
    {
      linear->angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (linear->angle == NULL)
        return FALSE;

      if (!_gtk_css_parser_try (parser, ",", TRUE))
        {
          _gtk_css_parser_error (parser, "Expected a comma");
          return FALSE;
        }
    }
  else
    linear->side = 1 << GTK_CSS_BOTTOM;

  do {
    GtkCssImageLinearColorStop stop;

    stop.color = _gtk_css_color_value_parse (parser);
    if (stop.color == NULL)
      return FALSE;

    if (gtk_css_number_value_can_parse (parser))
      {
        stop.offset = _gtk_css_number_value_parse (parser,
                                                   GTK_CSS_PARSE_PERCENT
                                                   | GTK_CSS_PARSE_LENGTH);
        if (stop.offset == NULL)
          {
            _gtk_css_value_unref (stop.color);
            return FALSE;
          }
      }
    else
      {
        stop.offset = NULL;
      }

    g_array_append_val (linear->stops, stop);

  } while (_gtk_css_parser_try (parser, ",", TRUE));

  if (linear->stops->len < 2)
    {
      _gtk_css_parser_error_full (parser,
                                  GTK_CSS_PROVIDER_ERROR_DEPRECATED,
                                  "Using one color stop with %s() is deprecated.",
                                  linear->repeating ? "repeating-linear-gradient" : "linear-gradient");
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing closing bracket at end of linear gradient");
      return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_linear_print (GtkCssImage *image,
                            GString     *string)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  guint i;

  if (linear->repeating)
    g_string_append (string, "repeating-linear-gradient(");
  else
    g_string_append (string, "linear-gradient(");

  if (linear->side)
    {
      if (linear->side != (1 << GTK_CSS_BOTTOM))
        {
          g_string_append (string, "to");

          if (linear->side & (1 << GTK_CSS_TOP))
            g_string_append (string, " top");
          else if (linear->side & (1 << GTK_CSS_BOTTOM))
            g_string_append (string, " bottom");

          if (linear->side & (1 << GTK_CSS_LEFT))
            g_string_append (string, " left");
          else if (linear->side & (1 << GTK_CSS_RIGHT))
            g_string_append (string, " right");

          g_string_append (string, ", ");
        }
    }
  else
    {
      _gtk_css_value_print (linear->angle, string);
      g_string_append (string, ", ");
    }

  for (i = 0; i < linear->stops->len; i++)
    {
      GtkCssImageLinearColorStop *stop;
      
      if (i > 0)
        g_string_append (string, ", ");

      stop = &g_array_index (linear->stops, GtkCssImageLinearColorStop, i);

      _gtk_css_value_print (stop->color, string);

      if (stop->offset)
        {
          g_string_append (string, " ");
          _gtk_css_value_print (stop->offset, string);
        }
    }
  
  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_linear_compute (GtkCssImage             *image,
                              guint                    property_id,
                              GtkStyleProviderPrivate *provider,
                              GtkCssStyle             *style,
                              GtkCssStyle             *parent_style)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  GtkCssImageLinear *copy;
  guint i;

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_LINEAR, NULL);
  copy->repeating = linear->repeating;
  copy->side = linear->side;

  if (linear->angle)
    copy->angle = _gtk_css_value_compute (linear->angle, property_id, provider, style, parent_style);
  
  g_array_set_size (copy->stops, linear->stops->len);
  for (i = 0; i < linear->stops->len; i++)
    {
      GtkCssImageLinearColorStop *stop, *scopy;

      stop = &g_array_index (linear->stops, GtkCssImageLinearColorStop, i);
      scopy = &g_array_index (copy->stops, GtkCssImageLinearColorStop, i);
              
      scopy->color = _gtk_css_value_compute (stop->color, property_id, provider, style, parent_style);
      
      if (stop->offset)
        {
          scopy->offset = _gtk_css_value_compute (stop->offset, property_id, provider, style, parent_style);
        }
      else
        {
          scopy->offset = NULL;
        }
    }

  return GTK_CSS_IMAGE (copy);
}

static GtkCssImage *
gtk_css_image_linear_transition (GtkCssImage *start_image,
                                 GtkCssImage *end_image,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssImageLinear *start, *end, *result;
  guint i;

  start = GTK_CSS_IMAGE_LINEAR (start_image);

  if (end_image == NULL)
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);

  if (!GTK_IS_CSS_IMAGE_LINEAR (end_image))
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);

  end = GTK_CSS_IMAGE_LINEAR (end_image);

  if ((start->repeating != end->repeating)
      || (start->stops->len != end->stops->len))
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);

  result = g_object_new (GTK_TYPE_CSS_IMAGE_LINEAR, NULL);
  result->repeating = start->repeating;

  if (start->side != end->side)
    goto fail;

  result->side = start->side;
  if (result->side == 0)
    result->angle = _gtk_css_value_transition (start->angle, end->angle, property_id, progress);
  if (result->angle == NULL)
    goto fail;
  
  for (i = 0; i < start->stops->len; i++)
    {
      GtkCssImageLinearColorStop stop, *start_stop, *end_stop;

      start_stop = &g_array_index (start->stops, GtkCssImageLinearColorStop, i);
      end_stop = &g_array_index (end->stops, GtkCssImageLinearColorStop, i);

      if ((start_stop->offset != NULL) != (end_stop->offset != NULL))
        goto fail;

      if (start_stop->offset == NULL)
        {
          stop.offset = NULL;
        }
      else
        {
          stop.offset = _gtk_css_value_transition (start_stop->offset,
                                                   end_stop->offset,
                                                   property_id,
                                                   progress);
          if (stop.offset == NULL)
            goto fail;
        }

      stop.color = _gtk_css_value_transition (start_stop->color,
                                              end_stop->color,
                                              property_id,
                                              progress);
      if (stop.color == NULL)
        {
          if (stop.offset)
            _gtk_css_value_unref (stop.offset);
          goto fail;
        }

      g_array_append_val (result->stops, stop);
    }

  return GTK_CSS_IMAGE (result);

fail:
  g_object_unref (result);
  return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);
}

static gboolean
gtk_css_image_linear_equal (GtkCssImage *image1,
                            GtkCssImage *image2)
{
  GtkCssImageLinear *linear1 = GTK_CSS_IMAGE_LINEAR (image1);
  GtkCssImageLinear *linear2 = GTK_CSS_IMAGE_LINEAR (image2);
  guint i;

  if (linear1->repeating != linear2->repeating ||
      linear1->side != linear2->side ||
      (linear1->side == 0 && !_gtk_css_value_equal (linear1->angle, linear2->angle)) ||
      linear1->stops->len != linear2->stops->len)
    return FALSE;

  for (i = 0; i < linear1->stops->len; i++)
    {
      GtkCssImageLinearColorStop *stop1, *stop2;

      stop1 = &g_array_index (linear1->stops, GtkCssImageLinearColorStop, i);
      stop2 = &g_array_index (linear2->stops, GtkCssImageLinearColorStop, i);

      if (!_gtk_css_value_equal0 (stop1->offset, stop2->offset) ||
          !_gtk_css_value_equal (stop1->color, stop2->color))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_linear_dispose (GObject *object)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (object);

  if (linear->stops)
    {
      g_array_free (linear->stops, TRUE);
      linear->stops = NULL;
    }

  linear->side = 0;
  if (linear->angle)
    {
      _gtk_css_value_unref (linear->angle);
      linear->angle = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_linear_parent_class)->dispose (object);
}

static void
_gtk_css_image_linear_class_init (GtkCssImageLinearClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->draw = gtk_css_image_linear_draw;
  image_class->parse = gtk_css_image_linear_parse;
  image_class->print = gtk_css_image_linear_print;
  image_class->compute = gtk_css_image_linear_compute;
  image_class->equal = gtk_css_image_linear_equal;
  image_class->transition = gtk_css_image_linear_transition;

  object_class->dispose = gtk_css_image_linear_dispose;
}

static void
gtk_css_image_clear_color_stop (gpointer color_stop)
{
  GtkCssImageLinearColorStop *stop = color_stop;

  _gtk_css_value_unref (stop->color);
  if (stop->offset)
    _gtk_css_value_unref (stop->offset);
}

static void
_gtk_css_image_linear_init (GtkCssImageLinear *linear)
{
  linear->stops = g_array_new (FALSE, FALSE, sizeof (GtkCssImageLinearColorStop));
  g_array_set_clear_func (linear->stops, gtk_css_image_clear_color_stop);
}

