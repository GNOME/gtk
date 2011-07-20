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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkroundedboxprivate.h"

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
  box->box.x = x;
  box->box.y = y;
  box->box.width = width;
  box->box.height = height;
  memset (&box->border_radius, 0, sizeof (GtkCssBorderRadius));
}

/* clamp border radius, following CSS specs */
static void
gtk_rounded_box_clamp_border_radius (GtkRoundedBox *box)
{
  gdouble factor = 1.0;

  /* note: division by zero leads to +INF, which is > factor, so will be ignored */
  factor = MIN (factor, box->box.width / (box->border_radius.top_left.horizontal +
                                          box->border_radius.top_right.horizontal));
  factor = MIN (factor, box->box.height / (box->border_radius.top_right.vertical +
                                           box->border_radius.bottom_right.vertical));
  factor = MIN (factor, box->box.width / (box->border_radius.bottom_right.horizontal +
                                          box->border_radius.bottom_left.horizontal));
  factor = MIN (factor, box->box.height / (box->border_radius.top_left.vertical +
                                           box->border_radius.bottom_left.vertical));

  box->border_radius.top_left.horizontal *= factor;
  box->border_radius.top_left.vertical *= factor;
  box->border_radius.top_right.horizontal *= factor;
  box->border_radius.top_right.vertical *= factor;
  box->border_radius.bottom_right.horizontal *= factor;
  box->border_radius.bottom_right.vertical *= factor;
  box->border_radius.bottom_left.horizontal *= factor;
  box->border_radius.bottom_left.vertical *= factor;
}

void
_gtk_rounded_box_apply_border_radius (GtkRoundedBox    *box,
                                      GtkThemingEngine *engine,
                                      GtkStateFlags     state,
                                      GtkJunctionSides  junction)
{
  GtkCssBorderCornerRadius *top_left_radius, *top_right_radius;
  GtkCssBorderCornerRadius *bottom_left_radius, *bottom_right_radius;

  gtk_theming_engine_get (engine, state,
                          /* Can't use border-radius as it's an int for
                           * backwards compat */
                          "border-top-left-radius", &top_left_radius,
                          "border-top-right-radius", &top_right_radius,
                          "border-bottom-right-radius", &bottom_right_radius,
                          "border-bottom-left-radius", &bottom_left_radius,
                          NULL);

  if (top_left_radius && (junction & GTK_JUNCTION_CORNER_TOPLEFT) == 0)
    box->border_radius.top_left = *top_left_radius;
  if (top_right_radius && (junction & GTK_JUNCTION_CORNER_TOPRIGHT) == 0)
    box->border_radius.top_right = *top_right_radius;
  if (bottom_right_radius && (junction & GTK_JUNCTION_CORNER_BOTTOMRIGHT) == 0)
    box->border_radius.bottom_right = *bottom_right_radius;
  if (bottom_left_radius && (junction & GTK_JUNCTION_CORNER_BOTTOMLEFT) == 0)
    box->border_radius.bottom_left = *bottom_left_radius;

  gtk_rounded_box_clamp_border_radius (box);

  g_free (top_left_radius);
  g_free (top_right_radius);
  g_free (bottom_right_radius);
  g_free (bottom_left_radius);
}

static void
gtk_css_border_radius_grow (GtkCssBorderCornerRadius *corner,
                            double                    horizontal,
                            double                    vertical)
{
  corner->horizontal += horizontal;
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

  if (box->box.height + bottom + right < 0)
    {
      box->box.y -= top * box->box.height / (top + bottom);
      box->box.height = 0;
    }
  else
    {
      box->box.y -= top;
      box->box.height += top + bottom;
    }

  gtk_css_border_radius_grow (&box->border_radius.top_left, left, top);
  gtk_css_border_radius_grow (&box->border_radius.top_right, right, bottom);
  gtk_css_border_radius_grow (&box->border_radius.bottom_right, right, top);
  gtk_css_border_radius_grow (&box->border_radius.bottom_left, left, bottom);
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
                   box->box.x + box->border_radius.top_left.horizontal,
                   box->box.y + box->border_radius.top_left.vertical,
                   box->border_radius.top_left.horizontal,
                   box->border_radius.top_left.vertical,
                   G_PI, 3 * G_PI / 2);
  _cairo_ellipsis (cr, 
                   box->box.x + box->box.width - box->border_radius.top_right.horizontal,
                   box->box.y + box->border_radius.top_right.vertical,
                   box->border_radius.top_right.horizontal,
                   box->border_radius.top_right.vertical,
                   - G_PI / 2, 0);
  _cairo_ellipsis (cr,
                   box->box.x + box->box.width - box->border_radius.bottom_right.horizontal,
                   box->box.y + box->box.height - box->border_radius.bottom_right.vertical,
                   box->border_radius.bottom_right.horizontal,
                   box->border_radius.bottom_right.vertical,
                   0, G_PI / 2);
  _cairo_ellipsis (cr,
                   box->box.x + box->border_radius.bottom_left.horizontal,
                   box->box.y + box->box.height - box->border_radius.bottom_left.vertical,
                   box->border_radius.bottom_left.horizontal,
                   box->border_radius.bottom_left.vertical,
                   G_PI / 2, G_PI);
}

void
_gtk_rounded_box_path_top (const GtkRoundedBox *outer,
                           const GtkRoundedBox *inner,
                           cairo_t             *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->box.x + outer->border_radius.top_left.horizontal,
                   outer->box.y + outer->border_radius.top_left.vertical,
                   outer->border_radius.top_left.horizontal,
                   outer->border_radius.top_left.vertical,
                   5 * G_PI / 4, 3 * G_PI / 2);
  _cairo_ellipsis (cr, 
                   outer->box.x + outer->box.width - outer->border_radius.top_right.horizontal,
                   outer->box.y + outer->border_radius.top_right.vertical,
                   outer->border_radius.top_right.horizontal,
                   outer->border_radius.top_right.vertical,
                   - G_PI / 2, -G_PI / 4);

  _cairo_ellipsis_negative (cr, 
                            inner->box.x + inner->box.width - inner->border_radius.top_right.horizontal,
                            inner->box.y + inner->border_radius.top_right.vertical,
                            inner->border_radius.top_right.horizontal,
                            inner->border_radius.top_right.vertical,
                            -G_PI / 4, - G_PI / 2);
  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->border_radius.top_left.horizontal,
                            inner->box.y + inner->border_radius.top_left.vertical,
                            inner->border_radius.top_left.horizontal,
                            inner->border_radius.top_left.vertical,
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
                   outer->box.x + outer->box.width - outer->border_radius.top_right.horizontal,
                   outer->box.y + outer->border_radius.top_right.vertical,
                   outer->border_radius.top_right.horizontal,
                   outer->border_radius.top_right.vertical,
                   - G_PI / 4, 0);
  _cairo_ellipsis (cr,
                   outer->box.x + outer->box.width - outer->border_radius.bottom_right.horizontal,
                   outer->box.y + outer->box.height - outer->border_radius.bottom_right.vertical,
                   outer->border_radius.bottom_right.horizontal,
                   outer->border_radius.bottom_right.vertical,
                   0, G_PI / 4);

  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->box.width - inner->border_radius.bottom_right.horizontal,
                            inner->box.y + inner->box.height - inner->border_radius.bottom_right.vertical,
                            inner->border_radius.bottom_right.horizontal,
                            inner->border_radius.bottom_right.vertical,
                            G_PI / 4, 0);
  _cairo_ellipsis_negative (cr, 
                            inner->box.x + inner->box.width - inner->border_radius.top_right.horizontal,
                            inner->box.y + inner->border_radius.top_right.vertical,
                            inner->border_radius.top_right.horizontal,
                            inner->border_radius.top_right.vertical,
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
                   outer->box.x + outer->box.width - outer->border_radius.bottom_right.horizontal,
                   outer->box.y + outer->box.height - outer->border_radius.bottom_right.vertical,
                   outer->border_radius.bottom_right.horizontal,
                   outer->border_radius.bottom_right.vertical,
                   G_PI / 4, G_PI / 2);
  _cairo_ellipsis (cr,
                   outer->box.x + outer->border_radius.bottom_left.horizontal,
                   outer->box.y + outer->box.height - outer->border_radius.bottom_left.vertical,
                   outer->border_radius.bottom_left.horizontal,
                   outer->border_radius.bottom_left.vertical,
                   G_PI / 2, 3 * G_PI / 4);

  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->border_radius.bottom_left.horizontal,
                            inner->box.y + inner->box.height - inner->border_radius.bottom_left.vertical,
                            inner->border_radius.bottom_left.horizontal,
                            inner->border_radius.bottom_left.vertical,
                            3 * G_PI / 4, G_PI / 2);
  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->box.width - inner->border_radius.bottom_right.horizontal,
                            inner->box.y + inner->box.height - inner->border_radius.bottom_right.vertical,
                            inner->border_radius.bottom_right.horizontal,
                            inner->border_radius.bottom_right.vertical,
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
                   outer->box.x + outer->border_radius.bottom_left.horizontal,
                   outer->box.y + outer->box.height - outer->border_radius.bottom_left.vertical,
                   outer->border_radius.bottom_left.horizontal,
                   outer->border_radius.bottom_left.vertical,
                   3 * G_PI / 4, G_PI);
  _cairo_ellipsis (cr,
                   outer->box.x + outer->border_radius.top_left.horizontal,
                   outer->box.y + outer->border_radius.top_left.vertical,
                   outer->border_radius.top_left.horizontal,
                   outer->border_radius.top_left.vertical,
                   G_PI, 5 * G_PI / 4);

  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->border_radius.top_left.horizontal,
                            inner->box.y + inner->border_radius.top_left.vertical,
                            inner->border_radius.top_left.horizontal,
                            inner->border_radius.top_left.vertical,
                            5 * G_PI / 4, G_PI);
  _cairo_ellipsis_negative (cr,
                            inner->box.x + inner->border_radius.bottom_left.horizontal,
                            inner->box.y + inner->box.height - inner->border_radius.bottom_left.vertical,
                            inner->border_radius.bottom_left.horizontal,
                            inner->border_radius.bottom_left.vertical,
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

