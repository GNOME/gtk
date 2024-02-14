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

#include "gtkcsscornervalueprivate.h"
#include "gtkcssimagebuiltinprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkhslaprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkstylecontextprivate.h"

#include "fallback-c89.c"

static void
gtk_do_render_check (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkStateFlags state;
  GtkCssImageBuiltinType image_type;

  state = gtk_style_context_get_state (context);
  if (state & GTK_STATE_FLAG_INCONSISTENT)
    image_type = GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT;
  else if (state & GTK_STATE_FLAG_CHECKED)
    image_type = GTK_CSS_IMAGE_BUILTIN_CHECK;
  else
    image_type = GTK_CSS_IMAGE_BUILTIN_NONE;

  gtk_css_style_render_icon (gtk_style_context_lookup_style (context), cr, x, y, width, height, image_type);
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

  gtk_do_render_check (context, cr, x, y, width, height);
}

static void
gtk_do_render_option (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height)
{
  GtkStateFlags state;
  GtkCssImageBuiltinType image_type;

  state = gtk_style_context_get_state (context);
  if (state & GTK_STATE_FLAG_INCONSISTENT)
    image_type = GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT;
  else if (state & GTK_STATE_FLAG_CHECKED)
    image_type = GTK_CSS_IMAGE_BUILTIN_OPTION;
  else
    image_type = GTK_CSS_IMAGE_BUILTIN_NONE;

  gtk_css_style_render_icon (gtk_style_context_lookup_style (context), cr, x, y, width, height, image_type);
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

  gtk_do_render_option (context, cr, x, y, width, height);
}

static void
gtk_do_render_arrow (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          angle,
                     gdouble          x,
                     gdouble          y,
                     gdouble          size)
{
  GtkCssImageBuiltinType image_type;

  /* map [0, 2 * pi) to [0, 4) */
  angle = round (2 * angle / G_PI);

  switch (((int) angle) & 3)
  {
  case 0:
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_UP;
    break;
  case 1:
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT;
    break;
  case 2:
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_DOWN;
    break;
  case 3:
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT;
    break;
  default:
    g_assert_not_reached ();
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_UP;
    break;
  }

  gtk_css_style_render_icon (gtk_style_context_lookup_style (context), cr, x, y, size, size, image_type);
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
 * Typical arrow rendering at 0, 1⁄2 π;, π; and 3⁄2 π:
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

  gtk_do_render_arrow (context, cr, angle, x, y, size);
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
 * Since: 3.0
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

  gtk_css_style_render_background (gtk_style_context_lookup_style (context),
                                   cr, x, y, width, height,
                                   gtk_style_context_get_junction_sides (context));
}

/**
 * gtk_render_background_get_clip:
 * @context: a #GtkStyleContext
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @out_clip: (out): return location for the clip
 *
 * Returns the area that will be affected (i.e. drawn to) when
 * calling gtk_render_background() for the given @context and
 * rectangle.
 *
 * Since: 3.20
 */
void
gtk_render_background_get_clip (GtkStyleContext *context,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height,
                                GdkRectangle    *out_clip)
{
  GtkBorder shadow;

  _gtk_css_shadows_value_get_extents (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BOX_SHADOW), &shadow);

  out_clip->x = floor (x) - shadow.left;
  out_clip->y = floor (y) - shadow.top;
  out_clip->width = ceil (width) + shadow.left + shadow.right;
  out_clip->height = ceil (height) + shadow.top + shadow.bottom;
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

  gtk_css_style_render_border (gtk_style_context_lookup_style (context),
                               cr,
                               x, y, width, height,
                               0,
                               gtk_style_context_get_junction_sides (context));
}

static void
gtk_do_render_expander (GtkStyleContext *context,
                        cairo_t         *cr,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height)
{
  GtkCssImageBuiltinType image_type;
  GtkStateFlags state;

  state = gtk_style_context_get_state (context);
  if (gtk_style_context_has_class (context, "horizontal"))
    {
      if (state & GTK_STATE_FLAG_DIR_RTL)
        image_type = (state & GTK_STATE_FLAG_CHECKED)
                     ? GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT_EXPANDED
                     : GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT;
      else
        image_type = (state & GTK_STATE_FLAG_CHECKED)
                     ? GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT_EXPANDED
                     : GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT;
    }
  else
    {
      if (state & GTK_STATE_FLAG_DIR_RTL)
        image_type = (state & GTK_STATE_FLAG_CHECKED)
                     ? GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT_EXPANDED
                     : GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT;
      else
        image_type = (state & GTK_STATE_FLAG_CHECKED)
                     ? GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT_EXPANDED
                     : GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT;
    }

  gtk_css_style_render_icon (gtk_style_context_lookup_style (context), cr, x, y, width, height, image_type);
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

  gtk_do_render_expander (context, cr, x, y, width, height);
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

  gtk_css_style_render_outline (gtk_style_context_lookup_style (context),
                                cr,
                                x, y, width, height);
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

  gtk_do_render_layout (context, cr, x, y, layout);
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

  gtk_do_render_line (context, cr, x0, y0, x1, y1);
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
  gtk_css_style_render_border (style,
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

  gtk_do_render_slider (context, cr, x, y, width, height, orientation);
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

  gtk_css_style_render_border (style, cr,
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
 *
 * Deprecated: 3.24: Use gtk_render_frame() instead. Themes can create gaps
 *     by omitting borders via CSS.
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

  gtk_css_style_render_frame_gap (gtk_style_context_lookup_style (context),
                                  cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap,
                                  gtk_style_context_get_junction_sides (context));
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

  gtk_css_style_render_border (style, cr,
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

  gtk_css_style_render_extension (gtk_style_context_lookup_style (context),
                                  cr,
                                  x, y, width, height,
                                  gap_side);
}

static void
gtk_do_render_handle (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height)
{
  GtkCssImageBuiltinType type;

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_GRIP))
    {
      GtkJunctionSides sides = gtk_style_context_get_junction_sides (context);

      /* order is important here for when too many (or too few) sides are set */
      if ((sides & GTK_JUNCTION_CORNER_BOTTOMRIGHT) == GTK_JUNCTION_CORNER_BOTTOMRIGHT)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMRIGHT;
      else if ((sides & GTK_JUNCTION_CORNER_TOPRIGHT) == GTK_JUNCTION_CORNER_TOPRIGHT)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_TOPRIGHT;
      else if ((sides & GTK_JUNCTION_CORNER_BOTTOMLEFT) == GTK_JUNCTION_CORNER_BOTTOMLEFT)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMLEFT;
      else if ((sides & GTK_JUNCTION_CORNER_TOPLEFT) == GTK_JUNCTION_CORNER_TOPLEFT)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_TOPLEFT;
      else if (sides & GTK_JUNCTION_RIGHT)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_RIGHT;
      else if (sides & GTK_JUNCTION_BOTTOM)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOM;
      else if (sides & GTK_JUNCTION_TOP)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_TOP;
      else if (sides & GTK_JUNCTION_LEFT)
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_LEFT;
      else
        type = GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMRIGHT;
    }
  else if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_PANE_SEPARATOR))
    {
      type = GTK_CSS_IMAGE_BUILTIN_PANE_SEPARATOR;
    }
  else
    {
      type = GTK_CSS_IMAGE_BUILTIN_HANDLE;
    }

  gtk_css_style_render_icon (gtk_style_context_lookup_style (context), cr, x, y, width, height, type);
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

  gtk_do_render_handle (context, cr, x, y, width, height);
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

  gtk_css_style_render_icon (gtk_style_context_lookup_style (context), cr, x, y, width, height, GTK_CSS_IMAGE_BUILTIN_SPINNER);
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

GdkPixbuf *
gtk_render_icon_pixbuf_unpacked (GdkPixbuf           *base_pixbuf,
                                 GtkIconSize          size,
                                 GtkCssIconEffect     icon_effect)
{
  GdkPixbuf *scaled;
  GdkPixbuf *stated;
  cairo_surface_t *surface;

  g_return_val_if_fail (base_pixbuf != NULL, NULL);

  /* If the size was wildcarded, and we're allowed to scale, then scale; otherwise,
   * leave it alone.
   */
  if (size != (GtkIconSize) -1)
    {
      int width = 1;
      int height = 1;

      if (!gtk_icon_size_lookup (size, &width, &height))
        {
          g_warning (G_STRLOC ": invalid icon size '%d'", size);
          return NULL;
        }

      scaled = scale_or_ref (base_pixbuf, width, height);
    }
  else
    {
      scaled = g_object_ref (base_pixbuf);
    }

  if (icon_effect != GTK_CSS_ICON_EFFECT_NONE)
    {
      surface = gdk_cairo_surface_create_from_pixbuf (scaled, 1, NULL);
      gtk_css_icon_effect_apply (icon_effect, surface);
      stated = gdk_pixbuf_get_from_surface (surface, 0, 0,
					    cairo_image_surface_get_width (surface),
					    cairo_image_surface_get_height (surface));
      cairo_surface_destroy (surface);
    }
  else
    {
      stated = scaled;
    }

  return stated;
}

/**
 * gtk_render_icon_pixbuf:
 * @context: a #GtkStyleContext
 * @source: the #GtkIconSource specifying the icon to render
 * @size: (type int): the size (#GtkIconSize) to render the icon at.
 *        A size of `(GtkIconSize) -1` means render at the size of the source
 *        and don’t scale.
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
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == (GtkIconSize)-1, NULL);
  g_return_val_if_fail (source != NULL, NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  return gtk_render_icon_pixbuf_unpacked (gtk_icon_source_get_pixbuf (source),
                                          gtk_icon_source_get_size_wildcarded (source) ? size : -1,
                                          gtk_icon_source_get_state_wildcarded (source)
                                          ? _gtk_css_icon_effect_value_get (
                                             _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_EFFECT))
                                          : GTK_CSS_ICON_EFFECT_NONE);
G_GNUC_END_IGNORE_DEPRECATIONS;
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
 * This function will render the icon in @pixbuf at exactly its size,
 * regardless of scaling factors, which may not be appropriate when
 * drawing on displays with high pixel densities.
 *
 * You probably want to use gtk_render_icon_surface() instead, if you
 * already have a Cairo surface.
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
  cairo_surface_t *surface;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);

  gtk_css_style_render_icon_surface (gtk_style_context_lookup_style (context),
                                     cr,
                                     surface,
                                     x, y);

  cairo_surface_destroy (surface);
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

  gtk_css_style_render_icon_surface (gtk_style_context_lookup_style (context),
                                     cr,
                                     surface,
                                     x, y);
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
