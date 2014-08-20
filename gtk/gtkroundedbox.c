/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkroundedboxprivate.h"

#include "gtkcsscornervalueprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkstylecontextprivate.h"

#include <string.h>

/**
 * _gtk_rounded_box_init_rect:
 * @box: box to initialize
 * @x: x coordinate of box
 * @y: y coordinate of box
 * @width: width of box
 * @height: height of box
 *
 * Initializes the given @box to represent the given rectangle.
 * The
 **/
void
_gtk_rounded_box_init_rect (GtkRoundedBox *box,
                            double         x,
                            double         y,
                            double         width,
                            double         height)
{
  memset (box, 0, sizeof (GtkRoundedBox));

  box->box.x = x;
  box->box.y = y;
  box->box.width = width;
  box->box.height = height;
}

/* clamp border radius, following CSS specs */
static void
gtk_rounded_box_clamp_border_radius (GtkRoundedBox *box)
{
  gdouble factor = 1.0;

  /* note: division by zero leads to +INF, which is > factor, so will be ignored */
  factor = MIN (factor, box->box.width / (box->corner[GTK_CSS_TOP_LEFT].horizontal +
                                          box->corner[GTK_CSS_TOP_RIGHT].horizontal));
  factor = MIN (factor, box->box.height / (box->corner[GTK_CSS_TOP_RIGHT].vertical +
                                           box->corner[GTK_CSS_BOTTOM_RIGHT].vertical));
  factor = MIN (factor, box->box.width / (box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal +
                                          box->corner[GTK_CSS_BOTTOM_LEFT].horizontal));
  factor = MIN (factor, box->box.height / (box->corner[GTK_CSS_TOP_LEFT].vertical +
                                           box->corner[GTK_CSS_BOTTOM_LEFT].vertical));

  box->corner[GTK_CSS_TOP_LEFT].horizontal *= factor;
  box->corner[GTK_CSS_TOP_LEFT].vertical *= factor;
  box->corner[GTK_CSS_TOP_RIGHT].horizontal *= factor;
  box->corner[GTK_CSS_TOP_RIGHT].vertical *= factor;
  box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal *= factor;
  box->corner[GTK_CSS_BOTTOM_RIGHT].vertical *= factor;
  box->corner[GTK_CSS_BOTTOM_LEFT].horizontal *= factor;
  box->corner[GTK_CSS_BOTTOM_LEFT].vertical *= factor;
}

static void
_gtk_rounded_box_apply_border_radius (GtkRoundedBox *box,
                                      GtkCssValue **corner,
                                      GtkJunctionSides junction)
{
  if (corner[GTK_CSS_TOP_LEFT] && (junction & GTK_JUNCTION_CORNER_TOPLEFT) == 0)
    {
      box->corner[GTK_CSS_TOP_LEFT].horizontal = _gtk_css_corner_value_get_x (corner[GTK_CSS_TOP_LEFT],
                                                                              box->box.width);
      box->corner[GTK_CSS_TOP_LEFT].vertical = _gtk_css_corner_value_get_y (corner[GTK_CSS_TOP_LEFT],
                                                                            box->box.height);
    }
  if (corner[GTK_CSS_TOP_RIGHT] && (junction & GTK_JUNCTION_CORNER_TOPRIGHT) == 0)
    {
      box->corner[GTK_CSS_TOP_RIGHT].horizontal = _gtk_css_corner_value_get_x (corner[GTK_CSS_TOP_RIGHT],
                                                                               box->box.width);
      box->corner[GTK_CSS_TOP_RIGHT].vertical = _gtk_css_corner_value_get_y (corner[GTK_CSS_TOP_RIGHT],
                                                                             box->box.height);
    }
  if (corner[GTK_CSS_BOTTOM_RIGHT] && (junction & GTK_JUNCTION_CORNER_BOTTOMRIGHT) == 0)
    {
      box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal = _gtk_css_corner_value_get_x (corner[GTK_CSS_BOTTOM_RIGHT],
                                                                                  box->box.width);
      box->corner[GTK_CSS_BOTTOM_RIGHT].vertical = _gtk_css_corner_value_get_y (corner[GTK_CSS_BOTTOM_RIGHT],
                                                                                box->box.height);
    }
  if (corner[GTK_CSS_BOTTOM_LEFT] && (junction & GTK_JUNCTION_CORNER_BOTTOMLEFT) == 0)
    {
      box->corner[GTK_CSS_BOTTOM_LEFT].horizontal = _gtk_css_corner_value_get_x (corner[GTK_CSS_BOTTOM_LEFT],
                                                                                 box->box.width);
      box->corner[GTK_CSS_BOTTOM_LEFT].vertical = _gtk_css_corner_value_get_y (corner[GTK_CSS_BOTTOM_LEFT],
                                                                               box->box.height);
    }

  gtk_rounded_box_clamp_border_radius (box);
}

void
_gtk_rounded_box_apply_border_radius_for_context (GtkRoundedBox    *box,
                                                  GtkStyleContext  *context,
                                                  GtkJunctionSides  junction)
{
  GtkCssValue *corner[4];

  corner[GTK_CSS_TOP_LEFT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS);
  corner[GTK_CSS_TOP_RIGHT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS);
  corner[GTK_CSS_BOTTOM_LEFT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS);
  corner[GTK_CSS_BOTTOM_RIGHT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS);

  _gtk_rounded_box_apply_border_radius (box, corner, junction);
}

void
_gtk_rounded_box_apply_outline_radius_for_context (GtkRoundedBox    *box,
                                                   GtkStyleContext  *context,
                                                   GtkJunctionSides  junction)
{
  GtkCssValue *corner[4];

  corner[GTK_CSS_TOP_LEFT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS);
  corner[GTK_CSS_TOP_RIGHT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS);
  corner[GTK_CSS_BOTTOM_LEFT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS);
  corner[GTK_CSS_BOTTOM_RIGHT] = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS);

  _gtk_rounded_box_apply_border_radius (box, corner, junction);
}

static void
gtk_css_border_radius_grow (GtkRoundedBoxCorner *corner,
                            double               horizontal,
                            double               vertical)
{
  if (corner->horizontal)
    corner->horizontal += horizontal;
  if (corner->vertical)
    corner->vertical += vertical;

  if (corner->horizontal <= 0 || corner->vertical <= 0)
    {
      corner->horizontal = 0;
      corner->vertical = 0;
    }
}

void
_gtk_rounded_box_grow (GtkRoundedBox *box,
                       double         top,
                       double         right,
                       double         bottom,
                       double         left)
{
  if (box->box.width + left + right < 0)
    {
      box->box.x -= left * box->box.width / (left + right);
      box->box.width = 0;
    }
  else
    {
      box->box.x -= left;
      box->box.width += left + right;
    }

  if (box->box.height + bottom + top < 0)
    {
      box->box.y -= top * box->box.height / (top + bottom);
      box->box.height = 0;
    }
  else
    {
      box->box.y -= top;
      box->box.height += top + bottom;
    }

  gtk_css_border_radius_grow (&box->corner[GTK_CSS_TOP_LEFT], left, top);
  gtk_css_border_radius_grow (&box->corner[GTK_CSS_TOP_RIGHT], right, bottom);
  gtk_css_border_radius_grow (&box->corner[GTK_CSS_BOTTOM_RIGHT], right, top);
  gtk_css_border_radius_grow (&box->corner[GTK_CSS_BOTTOM_LEFT], left, bottom);
}

void
_gtk_rounded_box_shrink (GtkRoundedBox *box,
                         double         top,
                         double         right,
                         double         bottom,
                         double         left)
{
  _gtk_rounded_box_grow (box, -top, -right, -bottom, -left);
}

void
_gtk_rounded_box_move (GtkRoundedBox *box,
                       double         dx,
                       double         dy)
{
  box->box.x += dx;
  box->box.y += dy;
}

static void
_cairo_ellipsis (cairo_t *cr,
	         double xc, double yc,
	         double xradius, double yradius,
	         double angle1, double angle2)
{
  if (xradius <= 0.0 || yradius <= 0.0)
    {
      cairo_line_to (cr, xc, yc);
      return;
    }

  cairo_save (cr);
  cairo_translate (cr, xc, yc);
  cairo_scale (cr, xradius, yradius);
  cairo_arc (cr, 0, 0, 1.0, angle1, angle2);
  cairo_restore (cr);
}

static void
_cairo_ellipsis_negative (cairo_t *cr,
                          double xc, double yc,
                          double xradius, double yradius,
                          double angle1, double angle2)
{
  if (xradius <= 0.0 || yradius <= 0.0)
    {
      cairo_line_to (cr, xc, yc);
      return;
    }

  cairo_save (cr);
  cairo_translate (cr, xc, yc);
  cairo_scale (cr, xradius, yradius);
  cairo_arc_negative (cr, 0, 0, 1.0, angle1, angle2);
  cairo_restore (cr);
}

void
_gtk_rounded_box_path (const GtkRoundedBox *box,
                       cairo_t             *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   box->box.x + box->corner[GTK_CSS_TOP_LEFT].horizontal,
                   box->box.y + box->corner[GTK_CSS_TOP_LEFT].vertical,
                   box->corner[GTK_CSS_TOP_LEFT].horizontal,
                   box->corner[GTK_CSS_TOP_LEFT].vertical,
                   G_PI, 3 * G_PI / 2);
  _cairo_ellipsis (cr, 
                   box->box.x + box->box.width - box->corner[GTK_CSS_TOP_RIGHT].horizontal,
                   box->box.y + box->corner[GTK_CSS_TOP_RIGHT].vertical,
                   box->corner[GTK_CSS_TOP_RIGHT].horizontal,
                   box->corner[GTK_CSS_TOP_RIGHT].vertical,
                   - G_PI / 2, 0);
  _cairo_ellipsis (cr,
                   box->box.x + box->box.width - box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                   box->box.y + box->box.height - box->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                   box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                   box->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                   0, G_PI / 2);
  _cairo_ellipsis (cr,
                   box->box.x + box->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                   box->box.y + box->box.height - box->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                   box->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                   box->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                   G_PI / 2, G_PI);

  cairo_close_path (cr);
}

double
_gtk_rounded_box_guess_length (const GtkRoundedBox *box,
                               GtkCssSide           side)
{
  double length;
  GtkCssSide before, after;

  before = side;
  after = (side + 1) % 4;

  if (side & 1)
    length = box->box.height
             - box->corner[before].vertical
             - box->corner[after].vertical;
  else
    length = box->box.width
             - box->corner[before].horizontal
             - box->corner[after].horizontal;

  length += G_PI * 0.125 * (box->corner[before].horizontal
                            + box->corner[before].vertical
                            + box->corner[after].horizontal
                            + box->corner[after].vertical);

  return length;
}

void
_gtk_rounded_box_path_side (const GtkRoundedBox *box,
                            cairo_t             *cr,
                            GtkCssSide           side)
{
  switch (side)
    {
    case GTK_CSS_TOP:
      _cairo_ellipsis (cr,
                       box->box.x + box->corner[GTK_CSS_TOP_LEFT].horizontal,
                       box->box.y + box->corner[GTK_CSS_TOP_LEFT].vertical,
                       box->corner[GTK_CSS_TOP_LEFT].horizontal,
                       box->corner[GTK_CSS_TOP_LEFT].vertical,
                       5 * G_PI / 4, 3 * G_PI / 2);
      _cairo_ellipsis (cr, 
                       box->box.x + box->box.width - box->corner[GTK_CSS_TOP_RIGHT].horizontal,
                       box->box.y + box->corner[GTK_CSS_TOP_RIGHT].vertical,
                       box->corner[GTK_CSS_TOP_RIGHT].horizontal,
                       box->corner[GTK_CSS_TOP_RIGHT].vertical,
                       - G_PI / 2, -G_PI / 4);
      break;
    case GTK_CSS_RIGHT:
      _cairo_ellipsis (cr, 
                       box->box.x + box->box.width - box->corner[GTK_CSS_TOP_RIGHT].horizontal,
                       box->box.y + box->corner[GTK_CSS_TOP_RIGHT].vertical,
                       box->corner[GTK_CSS_TOP_RIGHT].horizontal,
                       box->corner[GTK_CSS_TOP_RIGHT].vertical,
                       - G_PI / 4, 0);
      _cairo_ellipsis (cr,
                       box->box.x + box->box.width - box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                       box->box.y + box->box.height - box->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                       box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                       box->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                       0, G_PI / 4);
      break;
    case GTK_CSS_BOTTOM:
      _cairo_ellipsis (cr,
                       box->box.x + box->box.width - box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                       box->box.y + box->box.height - box->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                       box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                       box->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                       G_PI / 4, G_PI / 2);
      _cairo_ellipsis (cr,
                       box->box.x + box->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                       box->box.y + box->box.height - box->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                       box->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                       box->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                       G_PI / 2, 3 * G_PI / 4);
      break;
    case GTK_CSS_LEFT:
      _cairo_ellipsis (cr,
                       box->box.x + box->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                       box->box.y + box->box.height - box->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                       box->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                       box->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                       3 * G_PI / 4, G_PI);
      _cairo_ellipsis (cr,
                       box->box.x + box->corner[GTK_CSS_TOP_LEFT].horizontal,
                       box->box.y + box->corner[GTK_CSS_TOP_LEFT].vertical,
                       box->corner[GTK_CSS_TOP_LEFT].horizontal,
                       box->corner[GTK_CSS_TOP_LEFT].vertical,
                       G_PI, 5 * G_PI / 4);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

void
_gtk_rounded_box_path_top (const GtkRoundedBox *outer,
                           const GtkRoundedBox *inner,
                           cairo_t             *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->box.x + outer->corner[GTK_CSS_TOP_LEFT].horizontal,
                   outer->box.y + outer->corner[GTK_CSS_TOP_LEFT].vertical,
                   outer->corner[GTK_CSS_TOP_LEFT].horizontal,
                   outer->corner[GTK_CSS_TOP_LEFT].vertical,
                   5 * G_PI / 4, 3 * G_PI / 2);
  _cairo_ellipsis (cr, 
                   outer->box.x + outer->box.width - outer->corner[GTK_CSS_TOP_RIGHT].horizontal,
                   outer->box.y + outer->corner[GTK_CSS_TOP_RIGHT].vertical,
                   outer->corner[GTK_CSS_TOP_RIGHT].horizontal,
                   outer->corner[GTK_CSS_TOP_RIGHT].vertical,
                   - G_PI / 2, -G_PI / 4);

  _cairo_ellipsis_negative (cr, 
                            inner->box.x + inner->box.width - inner->corner[GTK_CSS_TOP_RIGHT].horizontal,
                            inner->box.y + inner->corner[GTK_CSS_TOP_RIGHT].vertical,
                            inner->corner[GTK_CSS_TOP_RIGHT].horizontal,
                            inner->corner[GTK_CSS_TOP_RIGHT].vertical,
                            -G_PI / 4, - G_PI / 2);
  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->corner[GTK_CSS_TOP_LEFT].horizontal,
                            inner->box.y + inner->corner[GTK_CSS_TOP_LEFT].vertical,
                            inner->corner[GTK_CSS_TOP_LEFT].horizontal,
                            inner->corner[GTK_CSS_TOP_LEFT].vertical,
                            3 * G_PI / 2, 5 * G_PI / 4);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_path_right (const GtkRoundedBox *outer,
                             const GtkRoundedBox *inner,
                             cairo_t             *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr, 
                   outer->box.x + outer->box.width - outer->corner[GTK_CSS_TOP_RIGHT].horizontal,
                   outer->box.y + outer->corner[GTK_CSS_TOP_RIGHT].vertical,
                   outer->corner[GTK_CSS_TOP_RIGHT].horizontal,
                   outer->corner[GTK_CSS_TOP_RIGHT].vertical,
                   - G_PI / 4, 0);
  _cairo_ellipsis (cr,
                   outer->box.x + outer->box.width - outer->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                   outer->box.y + outer->box.height - outer->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                   outer->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                   outer->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                   0, G_PI / 4);

  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->box.width - inner->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                            inner->box.y + inner->box.height - inner->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                            inner->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                            inner->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                            G_PI / 4, 0);
  _cairo_ellipsis_negative (cr, 
                            inner->box.x + inner->box.width - inner->corner[GTK_CSS_TOP_RIGHT].horizontal,
                            inner->box.y + inner->corner[GTK_CSS_TOP_RIGHT].vertical,
                            inner->corner[GTK_CSS_TOP_RIGHT].horizontal,
                            inner->corner[GTK_CSS_TOP_RIGHT].vertical,
                            0, - G_PI / 4);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_path_bottom (const GtkRoundedBox *outer,
                              const GtkRoundedBox *inner,
                              cairo_t             *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->box.x + outer->box.width - outer->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                   outer->box.y + outer->box.height - outer->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                   outer->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                   outer->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                   G_PI / 4, G_PI / 2);
  _cairo_ellipsis (cr,
                   outer->box.x + outer->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                   outer->box.y + outer->box.height - outer->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                   outer->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                   outer->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                   G_PI / 2, 3 * G_PI / 4);

  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                            inner->box.y + inner->box.height - inner->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                            inner->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                            inner->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                            3 * G_PI / 4, G_PI / 2);
  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->box.width - inner->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                            inner->box.y + inner->box.height - inner->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                            inner->corner[GTK_CSS_BOTTOM_RIGHT].horizontal,
                            inner->corner[GTK_CSS_BOTTOM_RIGHT].vertical,
                            G_PI / 2, G_PI / 4);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_path_left (const GtkRoundedBox *outer,
                            const GtkRoundedBox *inner,
                            cairo_t             *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->box.x + outer->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                   outer->box.y + outer->box.height - outer->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                   outer->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                   outer->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                   3 * G_PI / 4, G_PI);
  _cairo_ellipsis (cr,
                   outer->box.x + outer->corner[GTK_CSS_TOP_LEFT].horizontal,
                   outer->box.y + outer->corner[GTK_CSS_TOP_LEFT].vertical,
                   outer->corner[GTK_CSS_TOP_LEFT].horizontal,
                   outer->corner[GTK_CSS_TOP_LEFT].vertical,
                   G_PI, 5 * G_PI / 4);

  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->corner[GTK_CSS_TOP_LEFT].horizontal,
                            inner->box.y + inner->corner[GTK_CSS_TOP_LEFT].vertical,
                            inner->corner[GTK_CSS_TOP_LEFT].horizontal,
                            inner->corner[GTK_CSS_TOP_LEFT].vertical,
                            5 * G_PI / 4, G_PI);
  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                            inner->box.y + inner->box.height - inner->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                            inner->corner[GTK_CSS_BOTTOM_LEFT].horizontal,
                            inner->corner[GTK_CSS_BOTTOM_LEFT].vertical,
                            G_PI, 3 * G_PI / 4);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_clip_path (const GtkRoundedBox *box,
                            cairo_t             *cr)
{
  cairo_rectangle (cr,
                   box->box.x, box->box.y,
                   box->box.width, box->box.height);
}

