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

#include "gtkrender.h"
#include "gtkrenderprivate.h"

#include <math.h>

#include "gtkborderimageprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssenginevalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimagebuiltinprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkhslaprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkstylecontextprivate.h"

#include "fallback-c89.c"

static gboolean
render_icon_image (GtkStyleContext *context,
                   cairo_t         *cr,
                   double           x,
                   double           y,
                   double           width,
                   double           height)
{
  const GtkCssValue *shadows;
  cairo_matrix_t matrix, transform_matrix;
  GtkCssImage *image;

  image = _gtk_css_image_value_get_image (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SOURCE));
  if (image == NULL)
    return TRUE;

  if (GTK_IS_CSS_IMAGE_BUILTIN (image))
    return FALSE;

  shadows = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SHADOW);

  cairo_translate (cr, x, y);

  if (_gtk_css_transform_value_get_matrix (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_TRANSFORM), &transform_matrix))
    {
      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      cairo_matrix_init_translate (&matrix, width / 2, height / 2);
      cairo_matrix_multiply (&matrix, &transform_matrix, &matrix);
      cairo_matrix_translate (&matrix, - width / 2, - height / 2);

      if (_gtk_css_shadows_value_is_none (shadows))
        {
          cairo_transform (cr, &matrix);
          _gtk_css_image_draw (image, cr, width, height);
        }
      else
        {
          cairo_push_group (cr);
          cairo_transform (cr, &matrix);
          _gtk_css_image_draw (image, cr, width, height);
          cairo_pop_group_to_source (cr);
          _gtk_css_shadows_value_paint_icon (shadows, cr);
          cairo_paint (cr);
        }
    }

  return TRUE;
}

static void
gtk_do_render_check (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  const GdkRGBA *fg_color, *bg_color;
  GtkStateFlags flags;
  gint exterior_size, interior_size, thickness, pad;
  GtkBorderStyle border_style;
  GtkBorder border;
  gint border_width;

  if (render_icon_image (context, cr, x, y, width, height))
    return;

  flags = gtk_style_context_get_state (context);
  cairo_save (cr);

  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  bg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR));
  border.top = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100);
  border.right = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100);
  border.bottom = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100);
  border.left = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100);
  border_style = _gtk_css_border_style_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_STYLE));

  border_width = MIN (MIN (border.top, border.bottom),
                      MIN (border.left, border.right));
  exterior_size = MIN (width, height);

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  /* FIXME: thickness */
  thickness = 1;
  pad = thickness + MAX (1, (exterior_size - 2 * thickness) / 9);
  interior_size = MAX (1, exterior_size - 2 * pad);

  if (interior_size < 7)
    {
      interior_size = 7;
      pad = MAX (0, (exterior_size - interior_size) / 2);
    }

  x -= (1 + exterior_size - (gint) width) / 2;
  y -= (1 + exterior_size - (gint) height) / 2;

  if (border_style == GTK_BORDER_STYLE_SOLID)
    {
      const GdkRGBA *border_color;

      cairo_set_line_width (cr, border_width);
      border_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_COLOR));

      cairo_rectangle (cr, x + 0.5, y + 0.5, exterior_size - 1, exterior_size - 1);
      gdk_cairo_set_source_rgba (cr, bg_color);
      cairo_fill_preserve (cr);

      gdk_cairo_set_source_rgba (cr, border_color);
      cairo_stroke (cr);
    }

  gdk_cairo_set_source_rgba (cr, fg_color);

  if (flags & GTK_STATE_FLAG_INCONSISTENT)
    {
      int line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
		       x + pad,
		       y + pad + (1 + interior_size - line_thickness) / 2,
		       interior_size,
		       line_thickness);
      cairo_fill (cr);
    }
  else
    {
      if (flags & GTK_STATE_FLAG_CHECKED)
        {
          cairo_translate (cr,
                           x + pad, y + pad);

          cairo_scale (cr, interior_size / 7., interior_size / 7.);

          cairo_rectangle (cr, 0, 0, 7, 7);
          cairo_clip (cr);

          cairo_move_to  (cr, 7.0, 0.0);
          cairo_line_to  (cr, 7.5, 1.0);
          cairo_curve_to (cr, 5.3, 2.0,
                          4.3, 4.0,
                          3.5, 7.0);
          cairo_curve_to (cr, 3.0, 5.7,
                          1.3, 4.7,
                          0.0, 4.7);
          cairo_line_to  (cr, 0.2, 3.5);
          cairo_curve_to (cr, 1.1, 3.5,
                          2.3, 4.3,
                          3.0, 5.0);
          cairo_curve_to (cr, 1.0, 3.9,
                          2.4, 4.1,
                          3.2, 4.9);
          cairo_curve_to (cr, 3.5, 3.1,
                          5.2, 2.0,
                          7.0, 0.0);

          cairo_fill (cr);
        }
    }

  cairo_restore (cr);
}

/**
 * gtk_render_check:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a checkmark (as in a #GtkCheckButton).
 *
 * The %GTK_STATE_FLAG_CHECKED state determines whether the check is
 * on or off, and %GTK_STATE_FLAG_INCONSISTENT determines whether it
 * should be marked as undefined.
 *
 * Typical checkmark rendering:
 *
 * ![](checks.png)
 *
 * Since: 3.0
 **/
void
gtk_render_check (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_check (context, cr, x, y, width, height);

  cairo_restore (cr);
}

static void
gtk_do_render_option (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height)
{
  GtkStateFlags flags;
  const GdkRGBA *fg_color, *bg_color;
  gint exterior_size, interior_size, pad, thickness, border_width;
  GtkBorderStyle border_style;
  GtkBorder border;

  if (render_icon_image (context, cr, x, y, width, height))
    return;

  flags = gtk_style_context_get_state (context);

  cairo_save (cr);

  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  bg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR));
  border.top = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100);
  border.right = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100);
  border.bottom = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100);
  border.left = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100);
  border_style = _gtk_css_border_style_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_STYLE));

  exterior_size = MIN (width, height);
  border_width = MIN (MIN (border.top, border.bottom),
                      MIN (border.left, border.right));

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  x -= (1 + exterior_size - width) / 2;
  y -= (1 + exterior_size - height) / 2;

  if (border_style == GTK_BORDER_STYLE_SOLID)
    {
      const GdkRGBA *border_color;

      cairo_set_line_width (cr, border_width);
      border_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_COLOR));

      cairo_new_sub_path (cr);
      cairo_arc (cr,
                 x + exterior_size / 2.,
                 y + exterior_size / 2.,
                 (exterior_size - 1) / 2.,
                 0, 2 * G_PI);

      gdk_cairo_set_source_rgba (cr, bg_color);
      cairo_fill_preserve (cr);

      gdk_cairo_set_source_rgba (cr, border_color);
      cairo_stroke (cr);
    }

  gdk_cairo_set_source_rgba (cr, fg_color);

  /* FIXME: thickness */
  thickness = 1;

  if (flags & GTK_STATE_FLAG_INCONSISTENT)
    {
      gint line_thickness;

      pad = thickness + MAX (1, (exterior_size - 2 * thickness) / 9);
      interior_size = MAX (1, exterior_size - 2 * pad);

      if (interior_size < 7)
        {
          interior_size = 7;
          pad = MAX (0, (exterior_size - interior_size) / 2);
        }

      line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
                       x + pad,
                       y + pad + (interior_size - line_thickness) / 2.,
                       interior_size,
                       line_thickness);
      cairo_fill (cr);
    }
  if (flags & GTK_STATE_FLAG_CHECKED)
    {
      pad = thickness + MAX (1, 2 * (exterior_size - 2 * thickness) / 9);
      interior_size = MAX (1, exterior_size - 2 * pad);

      if (interior_size < 5)
        {
          interior_size = 7;
          pad = MAX (0, (exterior_size - interior_size) / 2);
        }

      cairo_new_sub_path (cr);
      cairo_arc (cr,
                 x + pad + interior_size / 2.,
                 y + pad + interior_size / 2.,
                 interior_size / 2.,
                 0, 2 * G_PI);
      cairo_fill (cr);
    }

  cairo_restore (cr);
}

/**
 * gtk_render_option:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an option mark (as in a #GtkRadioButton), the %GTK_STATE_FLAG_CHECKED
 * state will determine whether the option is on or off, and
 * %GTK_STATE_FLAG_INCONSISTENT whether it should be marked as undefined.
 *
 * Typical option mark rendering:
 *
 * ![](options.png)
 *
 * Since: 3.0
 **/
void
gtk_render_option (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_option (context, cr, x, y, width, height);

  cairo_restore (cr);
}

static void
gtk_do_render_arrow (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          angle,
                     gdouble          x,
                     gdouble          y,
                     gdouble          size)
{
  double line_width;
  const GdkRGBA *color;

  if (render_icon_image (context, cr, x, y, size, size))
    return;

  cairo_save (cr);

  line_width = size / 3.0 / sqrt (2);
  cairo_set_line_width (cr, line_width);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  cairo_translate (cr, x + size / 2.0, y + size / 2.0);
  cairo_rotate (cr, angle - G_PI_2);
  cairo_translate (cr, size / 4.0, 0);

  cairo_scale (cr,
               (size / (size + line_width)),
               (size / (size + line_width)));

  cairo_move_to (cr, -size / 2.0, -size / 2.0);
  cairo_rel_line_to (cr, size / 2.0, size / 2.0);
  cairo_rel_line_to (cr, - size / 2.0, size / 2.0);

  color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  gdk_cairo_set_source_rgba (cr, color);
  cairo_stroke (cr);

  cairo_restore (cr);
}

/**
 * gtk_render_arrow:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @angle: arrow angle from 0 to 2 * %G_PI, being 0 the arrow pointing to the north
 * @x: X origin of the render area
 * @y: Y origin of the render area
 * @size: square side for render area
 *
 * Renders an arrow pointing to @angle.
 *
 * Typical arrow rendering at 0, 1&solidus;2 &pi;, &pi; and 3&solidus;2 &pi;:
 *
 * ![](arrows.png)
 *
 * Since: 3.0
 **/
void
gtk_render_arrow (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          angle,
                  gdouble          x,
                  gdouble          y,
                  gdouble          size)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (size <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_ARROW);

  gtk_do_render_arrow (context, cr, angle, x, y, size);

  gtk_style_context_restore (context);
  cairo_restore (cr);
}

static void
color_shade (const GdkRGBA *color,
             gdouble        factor,
             GdkRGBA       *color_return)
{
  GtkHSLA hsla;

  _gtk_hsla_init_from_rgba (&hsla, color);
  _gtk_hsla_shade (&hsla, &hsla, factor);
  _gdk_rgba_init_from_hsla (color_return, &hsla);
}

/**
 * gtk_render_background:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders the background of an element.
 *
 * Typical background rendering, showing the effect of
 * `background-image`, `border-width` and `border-radius`:
 *
 * ![](background.png)
 *
 * Since: 3.0.
 **/
void
gtk_render_background (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          width,
                       gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_css_style_render_background (gtk_style_context_lookup_style (context),
                                   cr, x, y, width, height,
                                   gtk_style_context_get_junction_sides (context));

  cairo_restore (cr);
}

static void
hide_border_sides (double         border[4],
                   GtkBorderStyle border_style[4],
                   guint          hidden_side)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (hidden_side & (1 << i) ||
          border_style[i] == GTK_BORDER_STYLE_NONE ||
          border_style[i] == GTK_BORDER_STYLE_HIDDEN)
        border[i] = 0;
    }
}

static void
render_frame_fill (cairo_t       *cr,
                   GtkRoundedBox *border_box,
                   const double   border_width[4],
                   GdkRGBA        colors[4],
                   guint          hidden_side)
{
  GtkRoundedBox padding_box;
  guint i, j;

  padding_box = *border_box;
  _gtk_rounded_box_shrink (&padding_box,
                           border_width[GTK_CSS_TOP],
                           border_width[GTK_CSS_RIGHT],
                           border_width[GTK_CSS_BOTTOM],
                           border_width[GTK_CSS_LEFT]);

  if (hidden_side == 0 &&
      gdk_rgba_equal (&colors[0], &colors[1]) &&
      gdk_rgba_equal (&colors[0], &colors[2]) &&
      gdk_rgba_equal (&colors[0], &colors[3]))
    {
      gdk_cairo_set_source_rgba (cr, &colors[0]);

      _gtk_rounded_box_path (border_box, cr);
      _gtk_rounded_box_path (&padding_box, cr);
      cairo_fill (cr);
    }
  else
    {
      for (i = 0; i < 4; i++) 
        {
          if (hidden_side & (1 << i))
            continue;

          for (j = 0; j < 4; j++)
            { 
              if (hidden_side & (1 << j))
                continue;

              if (i == j || 
                  (gdk_rgba_equal (&colors[i], &colors[j])))
                {
                  /* We were already painted when i == j */
                  if (i > j)
                    break;

                  if (j == 0)
                    _gtk_rounded_box_path_top (border_box, &padding_box, cr);
                  else if (j == 1)
                    _gtk_rounded_box_path_right (border_box, &padding_box, cr);
                  else if (j == 2)
                    _gtk_rounded_box_path_bottom (border_box, &padding_box, cr);
                  else if (j == 3)
                    _gtk_rounded_box_path_left (border_box, &padding_box, cr);
                }
            }
          /* We were already painted when i == j */
          if (i > j)
            continue;

          gdk_cairo_set_source_rgba (cr, &colors[i]);

          cairo_fill (cr);
        }
    }
}

static void
set_stroke_style (cairo_t        *cr,
                  double          line_width,
                  GtkBorderStyle  style,
                  double          length)
{
  double segments[2];
  double n;

  cairo_set_line_width (cr, line_width);

  if (style == GTK_BORDER_STYLE_DOTTED)
    {
      n = round (0.5 * length / line_width);

      segments[0] = 0;
      segments[1] = n ? length / n : 2;
      cairo_set_dash (cr, segments, G_N_ELEMENTS (segments), 0);

      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    }
  else
    {
      n = length / line_width;
      /* Optimize the common case of an integer-sized rectangle
       * Again, we care about focus rectangles.
       */
      if (n == nearbyint (n))
        {
          segments[0] = 1;
          segments[1] = 2;
        }
      else
        {
          n = round ((1. / 3) * n);

          segments[0] = n ? (1. / 3) * length / n : 1;
          segments[1] = 2 * segments[0];
        }
      cairo_set_dash (cr, segments, G_N_ELEMENTS (segments), 0);

      cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
      cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
    }
}

static void
render_frame_stroke (cairo_t       *cr,
                     GtkRoundedBox *border_box,
                     const double   border_width[4],
                     GdkRGBA        colors[4],
                     guint          hidden_side,
                     GtkBorderStyle stroke_style)
{
  gboolean different_colors, different_borders;
  GtkRoundedBox stroke_box;
  guint i;

  different_colors = !gdk_rgba_equal (&colors[0], &colors[1]) ||
                     !gdk_rgba_equal (&colors[0], &colors[2]) ||
                     !gdk_rgba_equal (&colors[0], &colors[3]);
  different_borders = border_width[0] != border_width[1] ||
                      border_width[0] != border_width[2] ||
                      border_width[0] != border_width[3] ;

  stroke_box = *border_box;
  _gtk_rounded_box_shrink (&stroke_box,
                           border_width[GTK_CSS_TOP] / 2.0,
                           border_width[GTK_CSS_RIGHT] / 2.0,
                           border_width[GTK_CSS_BOTTOM] / 2.0,
                           border_width[GTK_CSS_LEFT] / 2.0);

  if (!different_colors && !different_borders && hidden_side == 0)
    {
      double length = 0;

      /* FAST PATH:
       * Mostly expected to trigger for focus rectangles */
      for (i = 0; i < 4; i++) 
        {
          length += _gtk_rounded_box_guess_length (&stroke_box, i);
          _gtk_rounded_box_path_side (&stroke_box, cr, i);
        }

      gdk_cairo_set_source_rgba (cr, &colors[0]);
      set_stroke_style (cr, border_width[0], stroke_style, length);
      cairo_stroke (cr);
    }
  else
    {
      GtkRoundedBox padding_box;

      padding_box = *border_box;
      _gtk_rounded_box_path (&padding_box, cr);
      _gtk_rounded_box_shrink (&padding_box,
                               border_width[GTK_CSS_TOP],
                               border_width[GTK_CSS_RIGHT],
                               border_width[GTK_CSS_BOTTOM],
                               border_width[GTK_CSS_LEFT]);

      for (i = 0; i < 4; i++) 
        {
          if (hidden_side & (1 << i))
            continue;

          cairo_save (cr);

          if (i == 0)
            _gtk_rounded_box_path_top (border_box, &padding_box, cr);
          else if (i == 1)
            _gtk_rounded_box_path_right (border_box, &padding_box, cr);
          else if (i == 2)
            _gtk_rounded_box_path_bottom (border_box, &padding_box, cr);
          else if (i == 3)
            _gtk_rounded_box_path_left (border_box, &padding_box, cr);
          cairo_clip (cr);

          _gtk_rounded_box_path_side (&stroke_box, cr, i);

          gdk_cairo_set_source_rgba (cr, &colors[i]);
          set_stroke_style (cr,
                            border_width[i],
                            stroke_style,
                            _gtk_rounded_box_guess_length (&stroke_box, i));
          cairo_stroke (cr);

          cairo_restore (cr);
        }
    }
}

static void
render_border (cairo_t       *cr,
               GtkRoundedBox *border_box,
               const double   border_width[4],
               guint          hidden_side,
               GdkRGBA        colors[4],
               GtkBorderStyle border_style[4])
{
  guint i, j;

  cairo_save (cr);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

  for (i = 0; i < 4; i++)
    {
      if (hidden_side & (1 << i))
        continue;

      /* NB: code below divides by this value */
      /* a border smaller than this will not noticably modify
       * pixels on screen, and since we don't compare with 0,
       * we'll use this value */
      if (border_width[i] < 1.0 / 1024)
        continue;

      switch (border_style[i])
        {
        case GTK_BORDER_STYLE_NONE:
        case GTK_BORDER_STYLE_HIDDEN:
        case GTK_BORDER_STYLE_SOLID:
          break;
        case GTK_BORDER_STYLE_INSET:
          if (i == 1 || i == 2)
            color_shade (&colors[i], 1.8, &colors[i]);
          break;
        case GTK_BORDER_STYLE_OUTSET:
          if (i == 0 || i == 3)
            color_shade (&colors[i], 1.8, &colors[i]);
          break;
        case GTK_BORDER_STYLE_DOTTED:
        case GTK_BORDER_STYLE_DASHED:
          {
            guint dont_draw = hidden_side;

            for (j = 0; j < 4; j++)
              {
                if (border_style[j] == border_style[i])
                  hidden_side |= (1 << j);
                else
                  dont_draw |= (1 << j);
              }
            
            render_frame_stroke (cr, border_box, border_width, colors, dont_draw, border_style[i]);
          }
          break;
        case GTK_BORDER_STYLE_DOUBLE:
          {
            GtkRoundedBox other_box;
            double other_border[4];
            guint dont_draw = hidden_side;

            for (j = 0; j < 4; j++)
              {
                if (border_style[j] == GTK_BORDER_STYLE_DOUBLE)
                  hidden_side |= (1 << j);
                else
                  dont_draw |= (1 << j);
                
                other_border[i] = border_width[i] / 3;
              }
            
            render_frame_fill (cr, border_box, other_border, colors, dont_draw);
            
            other_box = *border_box;
            _gtk_rounded_box_shrink (&other_box,
                                     2 * other_border[GTK_CSS_TOP],
                                     2 * other_border[GTK_CSS_RIGHT],
                                     2 * other_border[GTK_CSS_BOTTOM],
                                     2 * other_border[GTK_CSS_LEFT]);
            render_frame_fill (cr, &other_box, other_border, colors, dont_draw);
          }
          break;
        case GTK_BORDER_STYLE_GROOVE:
        case GTK_BORDER_STYLE_RIDGE:
          {
            GtkRoundedBox other_box;
            GdkRGBA other_colors[4];
            guint dont_draw = hidden_side;
            double other_border[4];

            for (j = 0; j < 4; j++)
              {
                other_colors[j] = colors[j];
                if ((j == 0 || j == 3) ^ (border_style[j] == GTK_BORDER_STYLE_RIDGE))
                  color_shade (&other_colors[j], 1.8, &other_colors[j]);
                else
                  color_shade (&colors[j], 1.8, &colors[j]);
                if (border_style[j] == GTK_BORDER_STYLE_GROOVE ||
                    border_style[j] == GTK_BORDER_STYLE_RIDGE)
                  hidden_side |= (1 << j);
                else
                  dont_draw |= (1 << j);
                other_border[i] = border_width[i] / 2;
              }
            
            render_frame_fill (cr, border_box, other_border, colors, dont_draw);
            
            other_box = *border_box;
            _gtk_rounded_box_shrink (&other_box,
                                     other_border[GTK_CSS_TOP],
                                     other_border[GTK_CSS_RIGHT],
                                     other_border[GTK_CSS_BOTTOM],
                                     other_border[GTK_CSS_LEFT]);
            render_frame_fill (cr, &other_box, other_border, other_colors, dont_draw);
          }
          break;
        default:
          g_assert_not_reached ();
          break;
        }
    }
  
  render_frame_fill (cr, border_box, border_width, colors, hidden_side);

  cairo_restore (cr);
}

static void
gtk_css_style_render_frame (GtkCssStyle      *style,
                            cairo_t          *cr,
                            gdouble           x,
                            gdouble           y,
                            gdouble           width,
                            gdouble           height,
                            guint             hidden_side,
                            GtkJunctionSides  junction)
{
  GtkBorderImage border_image;
  double border_width[4];

  border_width[0] = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100);
  border_width[1] = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100);
  border_width[2] = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100);
  border_width[3] = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100);

  if (_gtk_border_image_init (&border_image, style))
    {
      _gtk_border_image_render (&border_image, border_width, cr, x, y, width, height);
    }
  else
    {
      GtkBorderStyle border_style[4];
      GtkRoundedBox border_box;
      GdkRGBA colors[4];

      /* Optimize the most common case of "This widget has no border" */
      if (border_width[0] == 0 &&
          border_width[1] == 0 &&
          border_width[2] == 0 &&
          border_width[3] == 0)
        return;

      border_style[0] = _gtk_css_border_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_STYLE));
      border_style[1] = _gtk_css_border_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE));
      border_style[2] = _gtk_css_border_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE));
      border_style[3] = _gtk_css_border_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_STYLE));

      hide_border_sides (border_width, border_style, hidden_side);

      colors[0] = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_COLOR));
      colors[1] = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR));
      colors[2] = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR));
      colors[3] = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_COLOR));

      _gtk_rounded_box_init_rect (&border_box, x, y, width, height);
      _gtk_rounded_box_apply_border_radius_for_style (&border_box, style, junction);

      render_border (cr, &border_box, border_width, hidden_side, colors, border_style);
    }
}

/**
 * gtk_render_frame:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a frame around the rectangle defined by @x, @y, @width, @height.
 *
 * Examples of frame rendering, showing the effect of `border-image`,
 * `border-color`, `border-width`, `border-radius` and junctions:
 *
 * ![](frames.png)
 *
 * Since: 3.0
 **/
void
gtk_render_frame (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_css_style_render_frame (gtk_style_context_lookup_style (context),
                              cr,
                              x, y, width, height,
                              0,
                              gtk_style_context_get_junction_sides (context));


  cairo_restore (cr);
}

static void
gtk_do_render_expander (GtkStyleContext *context,
                        cairo_t         *cr,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height)
{
  GtkStateFlags flags;
  const GdkRGBA *outline_color, *fg_color;
  double vertical_overshoot;
  int diameter;
  double radius;
  double interp;		/* interpolation factor for center position */
  double x_double_horz, y_double_horz;
  double x_double_vert, y_double_vert;
  double x_double, y_double;
  gdouble angle;
  gint line_width;
  gboolean is_rtl;
  gdouble progress;

  if (render_icon_image (context, cr, x, y, width, height))
    return;

  cairo_save (cr);
  flags = gtk_style_context_get_state (context);

  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  outline_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_COLOR));

  is_rtl = (gtk_style_context_get_state (context) & GTK_STATE_FLAG_DIR_RTL);
  line_width = 1;
  progress = (flags & GTK_STATE_FLAG_CHECKED) ? 1 : 0;

  if (!gtk_style_context_has_class (context, GTK_STYLE_CLASS_HORIZONTAL))
    {
      if (is_rtl)
        angle = (G_PI) - ((G_PI / 2) * progress);
      else
        angle = (G_PI / 2) * progress;
    }
  else
    {
      if (is_rtl)
        angle = (G_PI / 2) + ((G_PI / 2) * progress);
      else
        angle = (G_PI / 2) - ((G_PI / 2) * progress);
    }

  interp = progress;

  /* Compute distance that the stroke extends beyonds the end
   * of the triangle we draw.
   */
  vertical_overshoot = line_width / 2.0 * (1. / tan (G_PI / 8));

  /* For odd line widths, we end the vertical line of the triangle
   * at a half pixel, so we round differently.
   */
  if (line_width % 2 == 1)
    vertical_overshoot = ceil (0.5 + vertical_overshoot) - 0.5;
  else
    vertical_overshoot = ceil (vertical_overshoot);

  /* Adjust the size of the triangle we draw so that the entire stroke fits
   */
  diameter = (gint) MAX (3, width - 2 * vertical_overshoot);

  /* If the line width is odd, we want the diameter to be even,
   * and vice versa, so force the sum to be odd. This relationship
   * makes the point of the triangle look right.
   */
  diameter -= (1 - (diameter + line_width) % 2);

  radius = diameter / 2.;

  /* Adjust the center so that the stroke is properly aligned with
   * the pixel grid. The center adjustment is different for the
   * horizontal and vertical orientations. For intermediate positions
   * we interpolate between the two.
   */
  x_double_vert = floor ((x + width / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;
  y_double_vert = (y + height / 2) - 0.5;

  x_double_horz = (x + width / 2) - 0.5;
  y_double_horz = floor ((y + height / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;

  x_double = x_double_vert * (1 - interp) + x_double_horz * interp;
  y_double = y_double_vert * (1 - interp) + y_double_horz * interp;

  cairo_translate (cr, x_double, y_double);
  cairo_rotate (cr, angle);

  cairo_move_to (cr, - radius / 2., - radius);
  cairo_line_to (cr,   radius / 2.,   0);
  cairo_line_to (cr, - radius / 2.,   radius);
  cairo_close_path (cr);

  cairo_set_line_width (cr, line_width);

  gdk_cairo_set_source_rgba (cr, fg_color);

  cairo_fill_preserve (cr);

  gdk_cairo_set_source_rgba (cr, outline_color);
  cairo_stroke (cr);

  cairo_restore (cr);
}

/**
 * gtk_render_expander:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an expander (as used in #GtkTreeView and #GtkExpander) in the area
 * defined by @x, @y, @width, @height. The state %GTK_STATE_FLAG_CHECKED
 * determines whether the expander is collapsed or expanded.
 *
 * Typical expander rendering:
 *
 * ![](expanders.png)
 *
 * Since: 3.0
 **/
void
gtk_render_expander (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_expander (context, cr, x, y, width, height);

  cairo_restore (cr);
}

static void
gtk_do_render_focus (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkBorderStyle border_style[4];
  GtkRoundedBox border_box;
  double border_width[4];
  GdkRGBA colors[4];

  border_style[0] = _gtk_css_border_style_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_STYLE));
  if (border_style[0] != GTK_BORDER_STYLE_NONE)
    {
      int offset;

      border_style[1] = border_style[2] = border_style[3] = border_style[0];
      border_width[0] = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_WIDTH), 100);
      border_width[3] = border_width[2] = border_width[1] = border_width[0];
      colors[0] = *_gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_COLOR));
      colors[3] = colors[2] = colors[1] = colors[0];
      offset = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_OFFSET), 100);

      _gtk_rounded_box_init_rect (&border_box, x, y, width, height);
      _gtk_rounded_box_shrink (&border_box,
                               - border_width[GTK_CSS_TOP] - offset,
                               - border_width[GTK_CSS_RIGHT] - offset,
                               - border_width[GTK_CSS_LEFT] - offset,
                               - border_width[GTK_CSS_BOTTOM] - offset);
      _gtk_rounded_box_apply_outline_radius_for_style (&border_box, gtk_style_context_lookup_style (context), GTK_JUNCTION_NONE);

      render_border (cr, &border_box, border_width, 0, colors, border_style);
    }
}

/**
 * gtk_render_focus:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a focus indicator on the rectangle determined by @x, @y, @width, @height.
 *
 * Typical focus rendering:
 *
 * ![](focus.png)
 *
 * Since: 3.0
 **/
void
gtk_render_focus (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_focus (context, cr, x, y, width, height);

  cairo_restore (cr);
}

static void
prepare_context_for_layout (cairo_t *cr,
                            gdouble x,
                            gdouble y,
                            PangoLayout *layout)
{
  const PangoMatrix *matrix;

  matrix = pango_context_get_matrix (pango_layout_get_context (layout));

  cairo_move_to (cr, x, y);

  if (matrix)
    {
      cairo_matrix_t cairo_matrix;

      cairo_matrix_init (&cairo_matrix,
                         matrix->xx, matrix->yx,
                         matrix->xy, matrix->yy,
                         matrix->x0, matrix->y0);

      cairo_transform (cr, &cairo_matrix);
    }
}

static void
gtk_do_render_layout (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      PangoLayout     *layout)
{
  const GdkRGBA *fg_color;

  cairo_save (cr);
  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));

  prepare_context_for_layout (cr, x, y, layout);

  _gtk_css_shadows_value_paint_layout (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_TEXT_SHADOW),
                                       cr, layout);

  gdk_cairo_set_source_rgba (cr, fg_color);
  pango_cairo_show_layout (cr, layout);

  cairo_restore (cr);
}

/**
 * gtk_render_layout:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout to render
 *
 * Renders @layout on the coordinates @x, @y
 *
 * Since: 3.0
 **/
void
gtk_render_layout (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   PangoLayout     *layout)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_layout (context, cr, x, y, layout);

  cairo_restore (cr);
}

static void
gtk_do_render_line (GtkStyleContext *context,
                    cairo_t         *cr,
                    gdouble          x0,
                    gdouble          y0,
                    gdouble          x1,
                    gdouble          y1)
{
  const GdkRGBA *color;

  cairo_save (cr);

  color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1);

  cairo_move_to (cr, x0 + 0.5, y0 + 0.5);
  cairo_line_to (cr, x1 + 0.5, y1 + 0.5);

  gdk_cairo_set_source_rgba (cr, color);
  cairo_stroke (cr);

  cairo_restore (cr);
}

/**
 * gtk_render_line:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x0: X coordinate for the origin of the line
 * @y0: Y coordinate for the origin of the line
 * @x1: X coordinate for the end of the line
 * @y1: Y coordinate for the end of the line
 *
 * Renders a line from (x0, y0) to (x1, y1).
 *
 * Since: 3.0
 **/
void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 gdouble          x0,
                 gdouble          y0,
                 gdouble          x1,
                 gdouble          y1)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_line (context, cr, x0, y0, x1, y1);

  cairo_restore (cr);
}

static void
gtk_do_render_slider (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkOrientation   orientation)
{
  GtkCssStyle *style;
  GtkJunctionSides junction;

  style = gtk_style_context_lookup_style (context);
  junction = gtk_style_context_get_junction_sides (context);

  gtk_css_style_render_background (style,
                                   cr,
                                   x, y, width, height,
                                   junction);
  gtk_css_style_render_frame (style,
                              cr,
                              x, y, width, height,
                              0,
                              junction);
}

/**
 * gtk_render_slider:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @orientation: orientation of the slider
 *
 * Renders a slider (as in #GtkScale) in the rectangle defined by @x, @y,
 * @width, @height. @orientation defines whether the slider is vertical
 * or horizontal.
 *
 * Typical slider rendering:
 *
 * ![](sliders.png)
 *
 * Since: 3.0
 **/
void
gtk_render_slider (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height,
                   GtkOrientation   orientation)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_slider (context, cr, x, y, width, height, orientation);

  cairo_restore (cr);
}

static void
gtk_css_style_render_frame_gap (GtkCssStyle     *style,
                                cairo_t         *cr,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height,
                                GtkPositionType  gap_side,
                                gdouble          xy0_gap,
                                gdouble          xy1_gap,
                                GtkJunctionSides junction)
{
  gint border_width;
  GtkCssValue *corner[4];
  gdouble x0, y0, x1, y1, xc = 0.0, yc = 0.0, wc = 0.0, hc = 0.0;
  GtkBorder border;

  border.top = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100);
  border.right = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100);
  border.bottom = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100);
  border.left = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100);
  corner[GTK_CSS_TOP_LEFT] = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS);
  corner[GTK_CSS_TOP_RIGHT] = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS);
  corner[GTK_CSS_BOTTOM_LEFT] = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS);
  corner[GTK_CSS_BOTTOM_RIGHT] = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS);

  border_width = MIN (MIN (border.top, border.bottom),
                      MIN (border.left, border.right));

  cairo_save (cr);

  switch (gap_side)
    {
    case GTK_POS_TOP:
      xc = x + xy0_gap + border_width;
      yc = y;
      wc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      hc = border_width;

      if (xy0_gap < _gtk_css_corner_value_get_x (corner[GTK_CSS_TOP_LEFT], width))
        junction |= GTK_JUNCTION_CORNER_TOPLEFT;

      if (xy1_gap > width - _gtk_css_corner_value_get_x (corner[GTK_CSS_TOP_RIGHT], width))
        junction |= GTK_JUNCTION_CORNER_TOPRIGHT;
      break;
    case GTK_POS_BOTTOM:
      xc = x + xy0_gap + border_width;
      yc = y + height - border_width;
      wc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      hc = border_width;

      if (xy0_gap < _gtk_css_corner_value_get_x (corner[GTK_CSS_BOTTOM_LEFT], width))
        junction |= GTK_JUNCTION_CORNER_BOTTOMLEFT;

      if (xy1_gap > width - _gtk_css_corner_value_get_x (corner[GTK_CSS_BOTTOM_RIGHT], width))
        junction |= GTK_JUNCTION_CORNER_BOTTOMRIGHT;

      break;
    case GTK_POS_LEFT:
      xc = x;
      yc = y + xy0_gap + border_width;
      wc = border_width;
      hc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);

      if (xy0_gap < _gtk_css_corner_value_get_y (corner[GTK_CSS_TOP_LEFT], height))
        junction |= GTK_JUNCTION_CORNER_TOPLEFT;

      if (xy1_gap > height - _gtk_css_corner_value_get_y (corner[GTK_CSS_BOTTOM_LEFT], height))
        junction |= GTK_JUNCTION_CORNER_BOTTOMLEFT;

      break;
    case GTK_POS_RIGHT:
      xc = x + width - border_width;
      yc = y + xy0_gap + border_width;
      wc = border_width;
      hc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);

      if (xy0_gap < _gtk_css_corner_value_get_y (corner[GTK_CSS_TOP_RIGHT], height))
        junction |= GTK_JUNCTION_CORNER_TOPRIGHT;

      if (xy1_gap > height - _gtk_css_corner_value_get_y (corner[GTK_CSS_BOTTOM_RIGHT], height))
        junction |= GTK_JUNCTION_CORNER_BOTTOMRIGHT;

      break;
    }

  cairo_clip_extents (cr, &x0, &y0, &x1, &y1);
  cairo_rectangle (cr, x0, y0, x1 - x0, yc - y0);
  cairo_rectangle (cr, x0, yc, xc - x0, hc);
  cairo_rectangle (cr, xc + wc, yc, x1 - (xc + wc), hc);
  cairo_rectangle (cr, x0, yc + hc, x1 - x0, y1 - (yc + hc));
  cairo_clip (cr);

  gtk_css_style_render_frame (style, cr,
                              x, y, width, height,
                              0, junction);

  cairo_restore (cr);
}

/**
 * gtk_render_frame_gap:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 * @xy0_gap: initial coordinate (X or Y depending on @gap_side) for the gap
 * @xy1_gap: end coordinate (X or Y depending on @gap_side) for the gap
 *
 * Renders a frame around the rectangle defined by (@x, @y, @width, @height),
 * leaving a gap on one side. @xy0_gap and @xy1_gap will mean X coordinates
 * for %GTK_POS_TOP and %GTK_POS_BOTTOM gap sides, and Y coordinates for
 * %GTK_POS_LEFT and %GTK_POS_RIGHT.
 *
 * Typical rendering of a frame with a gap:
 *
 * ![](frame-gap.png)
 *
 * Since: 3.0
 **/
void
gtk_render_frame_gap (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side,
                      gdouble          xy0_gap,
                      gdouble          xy1_gap)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (xy0_gap <= xy1_gap);
  g_return_if_fail (xy0_gap >= 0);

  if (width <= 0 || height <= 0)
    return;

  if (gap_side == GTK_POS_LEFT ||
      gap_side == GTK_POS_RIGHT)
    g_return_if_fail (xy1_gap <= height);
  else
    g_return_if_fail (xy1_gap <= width);

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_css_style_render_frame_gap (gtk_style_context_lookup_style (context),
                                  cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap,
                                  gtk_style_context_get_junction_sides (context));


  cairo_restore (cr);
}

static void
gtk_css_style_render_extension (GtkCssStyle     *style,
                                cairo_t         *cr,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height,
                                GtkPositionType  gap_side)
{
  GtkJunctionSides junction = 0;
  guint hidden_side = 0;

  switch (gap_side)
    {
    case GTK_POS_LEFT:
      junction = GTK_JUNCTION_LEFT;
      hidden_side = (1 << GTK_CSS_LEFT);
      break;
    case GTK_POS_RIGHT:
      junction = GTK_JUNCTION_RIGHT;
      hidden_side = (1 << GTK_CSS_RIGHT);
      break;
    case GTK_POS_TOP:
      junction = GTK_JUNCTION_TOP;
      hidden_side = (1 << GTK_CSS_TOP);
      break;
    case GTK_POS_BOTTOM:
      junction = GTK_JUNCTION_BOTTOM;
      hidden_side = (1 << GTK_CSS_BOTTOM);
      break;
    }

  gtk_css_style_render_background (style,
                                   cr,
                                   x, y,
                                   width, height,
                                   junction);

  gtk_css_style_render_frame (style, cr,
                              x, y, width, height,
                              hidden_side, junction);
}

/**
 * gtk_render_extension:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 *
 * Renders a extension (as in a #GtkNotebook tab) in the rectangle
 * defined by @x, @y, @width, @height. The side where the extension
 * connects to is defined by @gap_side.
 *
 * Typical extension rendering:
 *
 * ![](extensions.png)
 *
 * Since: 3.0
 **/
void
gtk_render_extension (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_css_style_render_extension (gtk_style_context_lookup_style (context),
                                  cr,
                                  x, y, width, height,
                                  gap_side);

  cairo_restore (cr);
}

static void
render_dot (cairo_t       *cr,
            const GdkRGBA *lighter,
            const GdkRGBA *darker,
            gdouble        x,
            gdouble        y,
            gdouble        size)
{
  size = CLAMP ((gint) size, 2, 3);

  if (size == 2)
    {
      gdk_cairo_set_source_rgba (cr, lighter);
      cairo_rectangle (cr, x, y, 1, 1);
      cairo_rectangle (cr, x + 1, y + 1, 1, 1);
      cairo_fill (cr);
    }
  else if (size == 3)
    {
      gdk_cairo_set_source_rgba (cr, lighter);
      cairo_rectangle (cr, x, y, 2, 1);
      cairo_rectangle (cr, x, y, 1, 2);
      cairo_fill (cr);

      gdk_cairo_set_source_rgba (cr, darker);
      cairo_rectangle (cr, x + 1, y + 1, 2, 1);
      cairo_rectangle (cr, x + 2, y, 1, 2);
      cairo_fill (cr);
    }
}

static void
add_path_line (cairo_t        *cr,
               gdouble         x1,
               gdouble         y1,
               gdouble         x2,
               gdouble         y2)
{
  /* Adjust endpoints */
  if (y1 == y2)
    {
      y1 += 0.5;
      y2 += 0.5;
      x2 += 1;
    }
  else if (x1 == x2)
    {
      x1 += 0.5;
      x2 += 0.5;
      y2 += 1;
    }

  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
}

static void
gtk_do_render_handle (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height)
{
  const GdkRGBA *bg_color;
  GdkRGBA lighter, darker;
  GtkJunctionSides sides;
  gint xx, yy;

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  if (render_icon_image (context, cr, x, y, width, height))
    return;

  cairo_save (cr);

  cairo_set_line_width (cr, 1.0);
  sides = gtk_style_context_get_junction_sides (context);
  bg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BACKGROUND_COLOR));

  color_shade (bg_color, 0.7, &darker);
  color_shade (bg_color, 1.3, &lighter);

  if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_GRIP))
    {
      /* reduce confusing values to a meaningful state */
      if ((sides & (GTK_JUNCTION_CORNER_TOPLEFT | GTK_JUNCTION_CORNER_BOTTOMRIGHT)) == (GTK_JUNCTION_CORNER_TOPLEFT | GTK_JUNCTION_CORNER_BOTTOMRIGHT))
        sides &= ~GTK_JUNCTION_CORNER_TOPLEFT;

      if ((sides & (GTK_JUNCTION_CORNER_TOPRIGHT | GTK_JUNCTION_CORNER_BOTTOMLEFT)) == (GTK_JUNCTION_CORNER_TOPRIGHT | GTK_JUNCTION_CORNER_BOTTOMLEFT))
        sides &= ~GTK_JUNCTION_CORNER_TOPRIGHT;

      if (sides == 0)
        sides = GTK_JUNCTION_CORNER_BOTTOMRIGHT;

      /* align drawing area to the connected side */
      if (sides == GTK_JUNCTION_LEFT)
        {
          if (height < width)
            width = height;
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPLEFT)
        {
          if (width < height)
            height = width;
          else if (height < width)
            width = height;
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMLEFT)
        {
          /* make it square, aligning to bottom left */
          if (width < height)
            {
              y += (height - width);
              height = width;
            }
          else if (height < width)
            width = height;
        }
      else if (sides == GTK_JUNCTION_RIGHT)
        {
          /* aligning to right */
          if (height < width)
            {
              x += (width - height);
              width = height;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPRIGHT)
        {
          if (width < height)
            height = width;
          else if (height < width)
            {
              x += (width - height);
              width = height;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMRIGHT)
        {
          /* make it square, aligning to bottom right */
          if (width < height)
            {
              y += (height - width);
              height = width;
            }
          else if (height < width)
            {
              x += (width - height);
              width = height;
            }
        }
      else if (sides == GTK_JUNCTION_TOP)
        {
          if (width < height)
            height = width;
        }
      else if (sides == GTK_JUNCTION_BOTTOM)
        {
          /* align to bottom */
          if (width < height)
            {
              y += (height - width);
              height = width;
            }
        }
      else
        g_assert_not_reached ();

      if (sides == GTK_JUNCTION_LEFT ||
          sides == GTK_JUNCTION_RIGHT)
        {
          gint xi;

          xi = x;

          while (xi < x + width)
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, x, y, x, y + height);
              cairo_stroke (cr);
              xi++;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y, xi, y + height);
              cairo_stroke (cr);
              xi += 2;
            }
        }
      else if (sides == GTK_JUNCTION_TOP ||
               sides == GTK_JUNCTION_BOTTOM)
        {
          gint yi;

          yi = y;

          while (yi < y + height)
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, x, yi, x + width, yi);
              cairo_stroke (cr);
              yi++;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, x, yi, x + width, yi);
              cairo_stroke (cr);
              yi += 2;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPLEFT)
        {
          gint xi, yi;

          xi = x + width;
          yi = y + height;

          while (xi > x + 3)
            {
              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y, x, yi);
              cairo_stroke (cr);

              --xi;
              --yi;

              add_path_line (cr, xi, y, x, yi);
              cairo_stroke (cr);

              --xi;
              --yi;

              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, xi, y, x, yi);
              cairo_stroke (cr);

              xi -= 3;
              yi -= 3;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_TOPRIGHT)
        {
          gint xi, yi;

          xi = x;
          yi = y + height;

          while (xi < (x + width - 3))
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, xi, y, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              --yi;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              --yi;

              add_path_line (cr, xi, y, x + width, yi);
              cairo_stroke (cr);

              xi += 3;
              yi -= 3;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMLEFT)
        {
          gint xi, yi;

          xi = x + width;
          yi = y;

          while (xi > x + 3)
            {
              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, x, yi, xi, y + height);
              cairo_stroke (cr);

              --xi;
              ++yi;

              add_path_line (cr, x, yi, xi, y + height);
              cairo_stroke (cr);

              --xi;
              ++yi;

              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, x, yi, xi, y + height);
              cairo_stroke (cr);

              xi -= 3;
              yi += 3;
            }
        }
      else if (sides == GTK_JUNCTION_CORNER_BOTTOMRIGHT)
        {
          gint xi, yi;

          xi = x;
          yi = y;

          while (xi < (x + width - 3))
            {
              gdk_cairo_set_source_rgba (cr, &lighter);
              add_path_line (cr, xi, y + height, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              ++yi;

              gdk_cairo_set_source_rgba (cr, &darker);
              add_path_line (cr, xi, y + height, x + width, yi);
              cairo_stroke (cr);

              ++xi;
              ++yi;

              add_path_line (cr, xi, y + height, x + width, yi);
              cairo_stroke (cr);

              xi += 3;
              yi += 3;
            }
        }
    }
  else if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_PANE_SEPARATOR))
    {
      if (width > height)
        for (xx = x + width / 2 - 15; xx <= x + width / 2 + 15; xx += 5)
          render_dot (cr, &lighter, &darker, xx, y + height / 2 - 1, 3);
      else
        for (yy = y + height / 2 - 15; yy <= y + height / 2 + 15; yy += 5)
          render_dot (cr, &lighter, &darker, x + width / 2 - 1, yy, 3);
    }
  else
    {
      for (yy = y; yy < y + height; yy += 3)
        for (xx = x; xx < x + width; xx += 6)
          {
            render_dot (cr, &lighter, &darker, xx, yy, 2);
            render_dot (cr, &lighter, &darker, xx + 3, yy + 1, 2);
          }
    }

  cairo_restore (cr);
}

/**
 * gtk_render_handle:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a handle (as in #GtkHandleBox, #GtkPaned and
 * #GtkWindow’s resize grip), in the rectangle
 * determined by @x, @y, @width, @height.
 *
 * Handles rendered for the paned and grip classes:
 *
 * ![](handles.png)
 *
 * Since: 3.0
 **/
void
gtk_render_handle (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_handle (context, cr, x, y, width, height);

  cairo_restore (cr);
}

void
gtk_render_paint_spinner (cairo_t *cr,
                          gdouble  radius,
                          gdouble  progress)
{
  guint num_steps, step;
  gdouble half;
  gint i;

  num_steps = 12;

  if (progress >= 0)
    step = (guint) (progress * num_steps);
  else
    step = 0;

  cairo_save (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_line_width (cr, 2.0);

  half = num_steps / 2;

  for (i = 0; i < num_steps; i++)
    {
      gint inset = 0.7 * radius;

      /* transparency is a function of time and intial value */
      gdouble t = 1.0 - (gdouble) ((i + step) % num_steps) / num_steps;
      gdouble xscale = - sin (i * G_PI / half);
      gdouble yscale = - cos (i * G_PI / half);

      cairo_push_group (cr);

      cairo_move_to (cr,
                     (radius - inset) * xscale,
                     (radius - inset) * yscale);
      cairo_line_to (cr,
                     radius * xscale,
                     radius * yscale);

      cairo_stroke (cr);

      cairo_pop_group_to_source (cr);
      cairo_paint_with_alpha (cr, t);
    }

  cairo_restore (cr);
}

static void
render_spinner (GtkStyleContext *context,
                cairo_t         *cr,
                gdouble          x,
                gdouble          y,
                gdouble          width,
                gdouble          height)
{
  const GdkRGBA *color;
  gdouble radius;

  radius = MIN (width / 2, height / 2);

  color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));

  cairo_save (cr);
  cairo_translate (cr, x + width / 2, y + height / 2);

  _gtk_css_shadows_value_paint_spinner (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SHADOW),
                                        cr,
                                        radius,
                                        -1);

  gdk_cairo_set_source_rgba (cr, color);
  gtk_render_paint_spinner (cr, radius, -1);

  cairo_restore (cr);
}

static void
gtk_do_render_activity (GtkStyleContext *context,
                        cairo_t         *cr,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height)
{
  if (render_icon_image (context, cr, x, y, width, height))
    return;

  render_spinner (context, cr, x, y, width, height);
}

/**
 * gtk_render_activity:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an activity indicator (such as in #GtkSpinner).
 * The state %GTK_STATE_FLAG_CHECKED determines whether there is
 * activity going on.
 *
 * Since: 3.0
 **/
void
gtk_render_activity (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_activity (context, cr, x, y, width, height);

  cairo_restore (cr);
}

static void
colorshift_source (cairo_t *cr,
		   gdouble shift)
{
  cairo_pattern_t *source;

  cairo_save (cr);
  cairo_paint (cr);

  source = cairo_pattern_reference (cairo_get_source (cr));

  cairo_set_source_rgb (cr, shift, shift, shift);
  cairo_set_operator (cr, CAIRO_OPERATOR_COLOR_DODGE);

  cairo_mask (cr, source);

  cairo_pattern_destroy (source);
  cairo_restore (cr);
}

static GdkPixbuf *
scale_or_ref (GdkPixbuf *src,
              gint       width,
              gint       height)
{
  if (width == gdk_pixbuf_get_width (src) &&
      height == gdk_pixbuf_get_height (src))
    return g_object_ref (src);
  else
    return gdk_pixbuf_scale_simple (src,
                                    width, height,
                                    GDK_INTERP_BILINEAR);
}

static GdkPixbuf *
gtk_do_render_icon_pixbuf (GtkStyleContext     *context,
                           const GtkIconSource *source,
                           GtkIconSize          size)
{
  GdkPixbuf *scaled;
  GdkPixbuf *stated;
  GdkPixbuf *base_pixbuf;
  GtkStateFlags state;
  gint width = 1;
  gint height = 1;
  cairo_t *cr;
  cairo_surface_t *surface;
  gboolean wildcarded;
  GtkCssImageEffect image_effect;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  base_pixbuf = gtk_icon_source_get_pixbuf (source);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  state = gtk_style_context_get_state (context);

  g_return_val_if_fail (base_pixbuf != NULL, NULL);

  if (size != (GtkIconSize) -1 &&
      !gtk_icon_size_lookup (size, &width, &height))
    {
      g_warning (G_STRLOC ": invalid icon size '%d'", size);
      return NULL;
    }

  /* If the size was wildcarded, and we're allowed to scale, then scale; otherwise,
   * leave it alone.
   */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  wildcarded = gtk_icon_source_get_size_wildcarded (source);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  if (size != (GtkIconSize) -1 && wildcarded)
    scaled = scale_or_ref (base_pixbuf, width, height);
  else
    scaled = g_object_ref (base_pixbuf);

  /* If the state was wildcarded, then generate a state. */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  wildcarded = gtk_icon_source_get_state_wildcarded (source);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!wildcarded)
    return scaled;

  image_effect = _gtk_css_image_effect_value_get
    (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_GTK_IMAGE_EFFECT));

  if (image_effect == GTK_CSS_IMAGE_EFFECT_DIM ||
      state & GTK_STATE_FLAG_INSENSITIVE)
    {
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					    gdk_pixbuf_get_width (scaled),
					    gdk_pixbuf_get_height (scaled));
      cr = cairo_create (surface);
      gdk_cairo_set_source_pixbuf (cr, scaled, 0, 0);
      cairo_paint_with_alpha (cr, 0.5);

      cairo_destroy (cr);

      g_object_unref (scaled);
      stated = gdk_pixbuf_get_from_surface (surface, 0, 0,
					    cairo_image_surface_get_width (surface),
					    cairo_image_surface_get_height (surface));
      cairo_surface_destroy (surface);
    }
  else if (image_effect == GTK_CSS_IMAGE_EFFECT_HIGHLIGHT ||
	   state & GTK_STATE_FLAG_PRELIGHT)
    {
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					    gdk_pixbuf_get_width (scaled),
					    gdk_pixbuf_get_height (scaled));

      cr = cairo_create (surface);
      gdk_cairo_set_source_pixbuf (cr, scaled, 0, 0);
      colorshift_source (cr, 0.10);

      cairo_destroy (cr);

      g_object_unref (scaled);
      stated = gdk_pixbuf_get_from_surface (surface, 0, 0,
					    cairo_image_surface_get_width (surface),
					    cairo_image_surface_get_height (surface));
      cairo_surface_destroy (surface);
    }
  else
    stated = scaled;

  return stated;
}

/**
 * gtk_render_icon_pixbuf:
 * @context: a #GtkStyleContext
 * @source: the #GtkIconSource specifying the icon to render
 * @size: (type int): the size to render the icon at. A size of (GtkIconSize) -1
 *        means render at the size of the source and don’t scale.
 *
 * Renders the icon specified by @source at the given @size, returning the result
 * in a pixbuf.
 *
 * Returns: (transfer full): a newly-created #GdkPixbuf containing the rendered icon
 *
 * Since: 3.0
 *
 * Deprecated: 3.10: Use gtk_icon_theme_load_icon() instead.
 **/
GdkPixbuf *
gtk_render_icon_pixbuf (GtkStyleContext     *context,
                        const GtkIconSource *source,
                        GtkIconSize          size)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == -1, NULL);
  g_return_val_if_fail (source != NULL, NULL);

  return gtk_do_render_icon_pixbuf (context, source, size);
}

static void
gtk_do_render_icon (GtkStyleContext *context,
                    cairo_t         *cr,
                    GdkPixbuf       *pixbuf,
                    gdouble          x,
                    gdouble          y)
{
  cairo_save (cr);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);

  _gtk_css_shadows_value_paint_icon (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SHADOW), cr);

  cairo_paint (cr);

  cairo_restore (cr);
}

/**
 * gtk_render_icon:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @pixbuf: a #GdkPixbuf containing the icon to draw
 * @x: X position for the @pixbuf
 * @y: Y position for the @pixbuf
 *
 * Renders the icon in @pixbuf at the specified @x and @y coordinates.
 *
 * Since: 3.2
 **/
void
gtk_render_icon (GtkStyleContext *context,
                 cairo_t         *cr,
                 GdkPixbuf       *pixbuf,
                 gdouble          x,
                 gdouble          y)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_icon (context, cr, pixbuf, x, y);

  cairo_restore (cr);
}

static void
gtk_do_render_icon_surface (GtkStyleContext *context,
                            cairo_t         *cr,
                            cairo_surface_t *surface,
                            gdouble          x,
                            gdouble          y)
{
  cairo_save (cr);

  cairo_set_source_surface (cr, surface, x, y);

  _gtk_css_shadows_value_paint_icon (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_SHADOW), cr);

  cairo_paint (cr);

  cairo_restore (cr);
}

/**
 * gtk_render_icon_surface:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @surface: a #cairo_surface_t containing the icon to draw
 * @x: X position for the @icon
 * @y: Y position for the @incon
 *
 * Renders the icon in @surface at the specified @x and @y coordinates.
 *
 * Since: 3.10
 **/
void
gtk_render_icon_surface (GtkStyleContext *context,
			 cairo_t         *cr,
			 cairo_surface_t *surface,
			 gdouble          x,
			 gdouble          y)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_do_render_icon_surface (context, cr, surface, x, y);

  cairo_restore (cr);
}

/*
 * gtk_render_content_path:
 * @context: style context to get style information from
 * @cr: cairo context to add path to
 * @x: x coordinate of CSS box
 * @y: y coordinate of CSS box
 * @width: width of CSS box
 * @height: height of CSS box
 *
 * Adds the path of the content box to @cr for a given border box.
 * This function respects rounded corners.
 *
 * This is useful if you are drawing content that is supposed to
 * fill the whole content area, like the color buttons in
 * #GtkColorChooserDialog.
 **/
void
gtk_render_content_path (GtkStyleContext *context,
                         cairo_t         *cr,
                         double           x,
                         double           y,
                         double           width,
                         double           height)
{
  GtkRoundedBox box;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  _gtk_rounded_box_init_rect (&box, x, y, width, height);
  _gtk_rounded_box_apply_border_radius_for_style (&box, gtk_style_context_lookup_style (context), 0);

  _gtk_rounded_box_shrink (&box,
                           _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100)
                           + _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_TOP), 100),
                           _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100)
                           + _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_RIGHT), 100),
                           _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100)
                           + _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_BOTTOM), 100),
                           _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100)
                           + _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_LEFT), 100));

  _gtk_rounded_box_path (&box, cr);
}
