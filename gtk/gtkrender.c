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

#include "gsk/gskroundedrectprivate.h"

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

  gtk_css_style_render_background (gtk_style_context_lookup_style (context),
                                   cr, x, y, width, height);
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
                               x, y, width, height);
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

  style = gtk_style_context_lookup_style (context);

  gtk_css_style_render_background (style,
                                   cr,
                                   x, y, width, height);
  gtk_css_style_render_border (style,
                               cr,
                               x, y, width, height);
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
                                gdouble          xy1_gap)
{
  gint border_width;
  gdouble x0, y0, x1, y1, xc = 0.0, yc = 0.0, wc = 0.0, hc = 0.0;
  GtkBorder border;

  border.top = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100);
  border.right = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100);
  border.bottom = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100);
  border.left = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100);

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
      break;

    case GTK_POS_BOTTOM:
      xc = x + xy0_gap + border_width;
      yc = y + height - border_width;
      wc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      hc = border_width;
      break;

    case GTK_POS_LEFT:
      xc = x;
      yc = y + xy0_gap + border_width;
      wc = border_width;
      hc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      break;

    case GTK_POS_RIGHT:
      xc = x + width - border_width;
      yc = y + xy0_gap + border_width;
      wc = border_width;
      hc = MAX (xy1_gap - xy0_gap - 2 * border_width, 0);
      break;
    }

  cairo_clip_extents (cr, &x0, &y0, &x1, &y1);
  cairo_rectangle (cr, x0, y0, x1 - x0, yc - y0);
  cairo_rectangle (cr, x0, yc, xc - x0, hc);
  cairo_rectangle (cr, xc + wc, yc, x1 - (xc + wc), hc);
  cairo_rectangle (cr, x0, yc + hc, x1 - x0, y1 - (yc + hc));
  cairo_clip (cr);

  gtk_css_style_render_border (style, cr,
                               x, y, width, height);

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

  gtk_css_style_render_frame_gap (gtk_style_context_lookup_style (context),
                                  cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap);
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

  if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_PANE_SEPARATOR))
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

