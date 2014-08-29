/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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

#include <config.h>
#include <cairo-gobject.h>

#include <math.h>

#include "gtkborderimageprivate.h"
#include "gtkcssbordervalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkstylepropertiesprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

gboolean
_gtk_border_image_init (GtkBorderImage   *image,
                        GtkStyleContext  *context)
{
  image->source = _gtk_css_image_value_get_image (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE));
  if (image->source == NULL)
    return FALSE;

  image->slice = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE);
  image->width = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH);
  image->repeat = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT);

  return TRUE;
}

typedef struct _GtkBorderImageSliceSize GtkBorderImageSliceSize;
struct _GtkBorderImageSliceSize {
  double offset;
  double size;
};

static void
gtk_border_image_compute_border_size (GtkBorderImageSliceSize  sizes[3],
                                      double                   offset,
                                      double                   area_size,
                                      double                   start_border_width,
                                      double                   end_border_width,
                                      const GtkCssValue       *start_border,
                                      const GtkCssValue       *end_border)
{
  double start, end;

  if (_gtk_css_number_value_get_unit (start_border) == GTK_CSS_NUMBER)
    start = start_border_width * _gtk_css_number_value_get (start_border, 100);
  else
    start = _gtk_css_number_value_get (start_border, area_size);
  if (_gtk_css_number_value_get_unit (end_border) == GTK_CSS_NUMBER)
    end = end_border_width * _gtk_css_number_value_get (end_border, 100);
  else
    end = _gtk_css_number_value_get (end_border, area_size);

  /* XXX: reduce vertical and horizontal by the same factor */
  if (start + end > area_size)
    {
      start = start * area_size / (start + end);
      end = end * area_size / (start + end);
    }

  sizes[0].offset = offset;
  sizes[0].size = start;
  sizes[1].offset = offset + start;
  sizes[1].size = area_size - start - end;
  sizes[2].offset = offset + area_size - end;
  sizes[2].size = end;
}

static void
gtk_border_image_render_slice (cairo_t           *cr,
                               cairo_surface_t   *slice,
                               double             slice_width,
                               double             slice_height,
                               double             x,
                               double             y,
                               double             width,
                               double             height,
                               GtkCssRepeatStyle  hrepeat,
                               GtkCssRepeatStyle  vrepeat)
{
  double hscale, vscale;
  double xstep, ystep;
  cairo_extend_t extend = CAIRO_EXTEND_PAD;
  cairo_matrix_t matrix;
  cairo_pattern_t *pattern;

  /* We can't draw center tiles yet */
  g_assert (hrepeat == GTK_CSS_REPEAT_STYLE_STRETCH || vrepeat == GTK_CSS_REPEAT_STYLE_STRETCH);

  hscale = width / slice_width;
  vscale = height / slice_height;
  xstep = width;
  ystep = height;

  switch (hrepeat)
    {
    case GTK_CSS_REPEAT_STYLE_REPEAT:
      extend = CAIRO_EXTEND_REPEAT;
      hscale = vscale;
      break;
    case GTK_CSS_REPEAT_STYLE_SPACE:
      {
        double space, n;

        extend = CAIRO_EXTEND_NONE;
        hscale = vscale;

        xstep = hscale * slice_width;
        n = floor (width / xstep);
        space = (width - n * xstep) / (n + 1);
        xstep += space;
        x += space;
        width -= 2 * space;
      }
      break;
    case GTK_CSS_REPEAT_STYLE_STRETCH:
      break;
    case GTK_CSS_REPEAT_STYLE_ROUND:
      extend = CAIRO_EXTEND_REPEAT;
      hscale = width / (slice_width * MAX (round (width / (slice_width * vscale)), 1));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  switch (vrepeat)
    {
    case GTK_CSS_REPEAT_STYLE_REPEAT:
      extend = CAIRO_EXTEND_REPEAT;
      vscale = hscale;
      break;
    case GTK_CSS_REPEAT_STYLE_SPACE:
      {
        double space, n;

        extend = CAIRO_EXTEND_NONE;
        vscale = hscale;

        ystep = vscale * slice_height;
        n = floor (height / ystep);
        space = (height - n * ystep) / (n + 1);
        ystep += space;
        y += space;
        height -= 2 * space;
      }
      break;
    case GTK_CSS_REPEAT_STYLE_STRETCH:
      break;
    case GTK_CSS_REPEAT_STYLE_ROUND:
      extend = CAIRO_EXTEND_REPEAT;
      vscale = height / (slice_height * MAX (round (height / (slice_height * hscale)), 1));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  pattern = cairo_pattern_create_for_surface (slice);

  cairo_matrix_init_translate (&matrix,
                               hrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? slice_width / 2 : 0,
                               vrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? slice_height / 2 : 0);
  cairo_matrix_scale (&matrix, 1 / hscale, 1 / vscale);
  cairo_matrix_translate (&matrix,
                          hrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? - width / 2 : 0,
                          vrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? - height / 2 : 0);

  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_pattern_set_extend (pattern, extend);

  cairo_save (cr);
  cairo_translate (cr, x, y);

  for (y = 0; y < height; y += ystep)
    {
      for (x = 0; x < width; x += xstep)
        {
          cairo_save (cr);
          cairo_translate (cr, x, y);
          cairo_set_source (cr, pattern);
          cairo_rectangle (cr, 0, 0, xstep, ystep);
          cairo_fill (cr);
          cairo_restore (cr);
        }
    }

  cairo_restore (cr);

  cairo_pattern_destroy (pattern);
}

static void
gtk_border_image_compute_slice_size (GtkBorderImageSliceSize sizes[3],
                                     int                     surface_size,
                                     int                     start_size,
                                     int                     end_size)
{
  sizes[0].size = MIN (start_size, surface_size);
  sizes[0].offset = 0;

  sizes[2].size = MIN (end_size, surface_size);
  sizes[2].offset = surface_size - sizes[2].size;

  sizes[1].size = MAX (0, surface_size - sizes[0].size - sizes[2].size);
  sizes[1].offset = sizes[0].size;
}

void
_gtk_border_image_render (GtkBorderImage   *image,
                          const double      border_width[4],
                          cairo_t          *cr,
                          gdouble           x,
                          gdouble           y,
                          gdouble           width,
                          gdouble           height)
{
  cairo_surface_t *surface, *slice;
  GtkBorderImageSliceSize vertical_slice[3], horizontal_slice[3];
  GtkBorderImageSliceSize vertical_border[3], horizontal_border[3];
  double source_width, source_height;
  int h, v;

  _gtk_css_image_get_concrete_size (image->source,
                                    0, 0,
                                    width, height,
                                    &source_width, &source_height);

  /* XXX: Optimize for (source_width == width && source_height == height) */

  surface = _gtk_css_image_get_surface (image->source,
                                        cairo_get_target (cr),
                                        source_width, source_height);

  gtk_border_image_compute_slice_size (horizontal_slice,
                                       source_width, 
                                       _gtk_css_number_value_get (_gtk_css_border_value_get_left (image->slice), source_width),
                                       _gtk_css_number_value_get (_gtk_css_border_value_get_right (image->slice), source_width));
  gtk_border_image_compute_slice_size (vertical_slice,
                                       source_height, 
                                       _gtk_css_number_value_get (_gtk_css_border_value_get_top (image->slice), source_height),
                                       _gtk_css_number_value_get (_gtk_css_border_value_get_bottom (image->slice), source_height));
  gtk_border_image_compute_border_size (horizontal_border,
                                        x,
                                        width,
                                        border_width[GTK_CSS_LEFT],
                                        border_width[GTK_CSS_RIGHT],
                                        _gtk_css_border_value_get_left (image->width),
                                        _gtk_css_border_value_get_right (image->width));
  gtk_border_image_compute_border_size (vertical_border,
                                        y,
                                        height,
                                        border_width[GTK_CSS_TOP],
                                        border_width[GTK_CSS_BOTTOM],
                                        _gtk_css_border_value_get_top (image->width),
                                        _gtk_css_border_value_get_bottom(image->width));
  
  for (v = 0; v < 3; v++)
    {
      if (vertical_slice[v].size == 0 ||
          vertical_border[v].size == 0)
        continue;

      for (h = 0; h < 3; h++)
        {
          if (horizontal_slice[h].size == 0 ||
              horizontal_border[h].size == 0)
            continue;

          if (h == 1 && v == 1)
            continue;

          slice = cairo_surface_create_for_rectangle (surface,
                                                      horizontal_slice[h].offset,
                                                      vertical_slice[v].offset,
                                                      horizontal_slice[h].size,
                                                      vertical_slice[v].size);

          gtk_border_image_render_slice (cr,
                                         slice,
                                         horizontal_slice[h].size,
                                         vertical_slice[v].size,
                                         horizontal_border[h].offset,
                                         vertical_border[v].offset,
                                         horizontal_border[h].size,
                                         vertical_border[v].size,
                                         h == 1 ? _gtk_css_border_repeat_value_get_x (image->repeat) : GTK_CSS_REPEAT_STYLE_STRETCH,
                                         v == 1 ? _gtk_css_border_repeat_value_get_y (image->repeat) : GTK_CSS_REPEAT_STYLE_STRETCH);

          cairo_surface_destroy (slice);
        }
    }

  cairo_surface_destroy (surface);
}
