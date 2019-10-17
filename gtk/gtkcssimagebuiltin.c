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

#include "gtkcssimagebuiltinprivate.h"

#include "gtkcssenumvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkhslaprivate.h"

#include <math.h>

#include "fallback-c89.c"

G_DEFINE_TYPE (GtkCssImageBuiltin, gtk_css_image_builtin, GTK_TYPE_CSS_IMAGE)

static GtkCssImage *the_one_true_image;

static void
gtk_css_image_builtin_draw_check (GtkCssImage *image,
                                  cairo_t     *cr,
                                  double       width,
                                  double       height,
                                  gboolean     checked,
                                  gboolean     inconsistent)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  gint x, y, exterior_size, interior_size, pad;

  exterior_size = MIN (width, height);

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  pad = 1 + MAX (1, (exterior_size - 2) / 9);
  interior_size = MAX (1, exterior_size - 2 * pad);

  if (interior_size < 7)
    pad = MAX (0, (exterior_size - interior_size) / 2);

  x = - (1 + exterior_size - (gint) width) / 2;
  y = - (1 + exterior_size - (gint) height) / 2;

  gdk_cairo_set_source_rgba (cr, &builtin->fg_color);

  if (inconsistent)
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
      if (checked)
        {
          cairo_save (cr);
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
          cairo_restore (cr);
        }
    }
}

static void
gtk_css_image_builtin_draw_option (GtkCssImage *image,
                                   cairo_t     *cr,
                                   double       width,
                                   double       height,
                                   gboolean     checked,
                                   gboolean     inconsistent)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  gint x, y, exterior_size, interior_size, pad;

  exterior_size = MIN (width, height);

  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  x = - (1 + exterior_size - width) / 2;
  y = - (1 + exterior_size - height) / 2;

  gdk_cairo_set_source_rgba (cr, &builtin->fg_color);

  pad = 1 + MAX (1, 2 * (exterior_size - 2) / 9);
  interior_size = MAX (1, exterior_size - 2 * pad);

  if (interior_size < 7)
    pad = MAX (0, (exterior_size - interior_size) / 2);

  if (inconsistent)
    {
      gint line_thickness;

      line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
                       x + pad,
                       y + pad + (interior_size - line_thickness) / 2.,
                       interior_size,
                       line_thickness);
      cairo_fill (cr);
    }
  else if (checked)
    {
      cairo_new_sub_path (cr);
      cairo_arc (cr,
                 x + pad + interior_size / 2.,
                 y + pad + interior_size / 2.,
                 interior_size / 2.,
                 0, 2 * G_PI);
      cairo_fill (cr);
    }
}

static void
gtk_css_image_builtin_draw_arrow (GtkCssImage            *image,
                                  cairo_t                *cr,
                                  double                  width,
                                  double                  height,
                                  GtkCssImageBuiltinType  image_type)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  double line_width;
  double size;

  size = MIN (width, height);

  cairo_translate (cr, width / 2.0, height / 2.0);
  switch ((guint) image_type)
  {
    case GTK_CSS_IMAGE_BUILTIN_ARROW_UP:
      break;
    case GTK_CSS_IMAGE_BUILTIN_ARROW_DOWN:
      cairo_rotate (cr, G_PI);
      break;
    case GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT:
      cairo_rotate (cr, 3 * G_PI / 2);
      break;
    case GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT:
      cairo_rotate (cr, G_PI / 2);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  line_width = size / 3.0 / sqrt (2);
  cairo_set_line_width (cr, line_width);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  cairo_scale (cr,
               (size / (size + line_width)),
               (size / (size + line_width)));

  cairo_move_to (cr, -size / 2.0, size / 4.0);
  cairo_rel_line_to (cr, size / 2.0, -size / 2.0);
  cairo_rel_line_to (cr, size / 2.0, size / 2.0);

  gdk_cairo_set_source_rgba (cr, &builtin->fg_color);
  cairo_stroke (cr);
}

static void
gtk_css_image_builtin_draw_expander (GtkCssImage *image,
                                     cairo_t     *cr,
                                     double       width,
                                     double       height,
                                     gboolean     horizontal,
                                     gboolean     is_rtl,
                                     gboolean     expanded)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  double vertical_overshoot;
  int diameter;
  double radius;
  double interp;		/* interpolation factor for center position */
  double x_double_horz, y_double_horz;
  double x_double_vert, y_double_vert;
  double x_double, y_double;
  gdouble angle;
  gint line_width;
  gdouble progress;

  line_width = 1;
  progress = expanded ? 1 : 0;

  if (!horizontal)
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
  x_double_vert = floor ((width / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;
  y_double_vert = (height / 2) - 0.5;

  x_double_horz = (width / 2) - 0.5;
  y_double_horz = floor ((height / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;

  x_double = x_double_vert * (1 - interp) + x_double_horz * interp;
  y_double = y_double_vert * (1 - interp) + y_double_horz * interp;

  cairo_translate (cr, x_double, y_double);
  cairo_rotate (cr, angle);

  cairo_move_to (cr, - radius / 2., - radius);
  cairo_line_to (cr,   radius / 2.,   0);
  cairo_line_to (cr, - radius / 2.,   radius);
  cairo_close_path (cr);

  cairo_set_line_width (cr, line_width);

  gdk_cairo_set_source_rgba (cr, &builtin->fg_color);

  cairo_fill (cr);
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
gtk_css_image_builtin_draw_pane_separator (GtkCssImage *image,
                                           cairo_t     *cr,
                                           double       width,
                                           double       height)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  GdkRGBA lighter, darker;
  gint xx, yy;

  cairo_set_line_width (cr, 1.0);

  color_shade (&builtin->bg_color, 0.7, &darker);
  color_shade (&builtin->bg_color, 1.3, &lighter);

  if (width > height)
    for (xx = width / 2 - 15; xx <= width / 2 + 15; xx += 5)
      render_dot (cr, &lighter, &darker, xx, height / 2 - 1, 3);
  else
    for (yy = height / 2 - 15; yy <= height / 2 + 15; yy += 5)
      render_dot (cr, &lighter, &darker, width / 2 - 1, yy, 3);
}

static void
gtk_css_image_builtin_draw_handle (GtkCssImage *image,
                                   cairo_t     *cr,
                                   double       width,
                                   double       height)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  GdkRGBA lighter, darker;
  gint xx, yy;

  cairo_set_line_width (cr, 1.0);

  color_shade (&builtin->bg_color, 0.7, &darker);
  color_shade (&builtin->bg_color, 1.3, &lighter);

  for (yy = 0; yy < height; yy += 3)
    for (xx = 0; xx < width; xx += 6)
      {
        render_dot (cr, &lighter, &darker, xx, yy, 2);
        render_dot (cr, &lighter, &darker, xx + 3, yy + 1, 2);
      }
}

static void
gtk_css_image_builtin_draw_spinner (GtkCssImage *image,
                                    cairo_t     *cr,
                                    double       width,
                                    double       height)
{
  GtkCssImageBuiltin *builtin = GTK_CSS_IMAGE_BUILTIN (image);
  guint num_steps;
  gdouble radius;
  gdouble half;
  gint i;

  radius = MIN (width / 2, height / 2);

  cairo_translate (cr, width / 2, height / 2);

  num_steps = 12;

  cairo_set_line_width (cr, 2.0);

  half = num_steps / 2;

  for (i = 0; i < num_steps; i++)
    {
      gint inset = 0.7 * radius;
      /* transparency is a function of time and intial value */
      gdouble t = 1.0 - (gdouble) i / num_steps;
      gdouble xscale = - sin (i * G_PI / half);
      gdouble yscale = - cos (i * G_PI / half);

      cairo_move_to (cr,
                     (radius - inset) * xscale,
                     (radius - inset) * yscale);
      cairo_line_to (cr,
                     radius * xscale,
                     radius * yscale);

      cairo_set_source_rgba (cr,
                             builtin->fg_color.red,
                             builtin->fg_color.green,
                             builtin->fg_color.blue,
                             builtin->fg_color.alpha * t);

      cairo_stroke (cr);
    }
}

static void
gtk_css_image_builtin_real_snapshot (GtkCssImage *image,
                                     GtkSnapshot *snapshot,
                                     double       width,
                                     double       height)
{
  /* It's a builtin image, other code will draw things */
}


static gboolean
gtk_css_image_builtin_parse (GtkCssImage  *image,
                             GtkCssParser *parser)
{
  if (!gtk_css_parser_try_ident (parser, "builtin"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'builtin'");
      return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_builtin_print (GtkCssImage *image,
                             GString     *string)
{
  g_string_append (string, "builtin");
}

static GtkCssImage *
gtk_css_image_builtin_compute (GtkCssImage      *image,
                               guint             property_id,
                               GtkStyleProvider *provider,
                               GtkCssStyle      *style,
                               GtkCssStyle      *parent_style)
{
  GtkCssImageBuiltin *result;

  result = g_object_new (GTK_TYPE_CSS_IMAGE_BUILTIN, NULL);

  result->fg_color = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_COLOR));
  result->bg_color = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_COLOR));

  return GTK_CSS_IMAGE (result);
}

static gboolean
gtk_css_image_builtin_equal (GtkCssImage *image1,
                             GtkCssImage *image2)
{
  GtkCssImageBuiltin *builtin1 = (GtkCssImageBuiltin *) image1;
  GtkCssImageBuiltin *builtin2 = (GtkCssImageBuiltin *) image2;

  return gdk_rgba_equal (&builtin1->fg_color, &builtin2->fg_color)
      && gdk_rgba_equal (&builtin1->bg_color, &builtin2->bg_color);
}

static void
gtk_css_image_builtin_dispose (GObject *object)
{
  if (the_one_true_image == GTK_CSS_IMAGE (object))
    the_one_true_image = NULL;

  G_OBJECT_CLASS (gtk_css_image_builtin_parent_class)->dispose (object);
}

static void
gtk_css_image_builtin_class_init (GtkCssImageBuiltinClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->snapshot = gtk_css_image_builtin_real_snapshot;
  image_class->parse = gtk_css_image_builtin_parse;
  image_class->print = gtk_css_image_builtin_print;
  image_class->compute = gtk_css_image_builtin_compute;
  image_class->equal = gtk_css_image_builtin_equal;

  object_class->dispose = gtk_css_image_builtin_dispose;
}

static void
gtk_css_image_builtin_init (GtkCssImageBuiltin *builtin)
{
  /* white background */
  builtin->bg_color.red = builtin->bg_color.green = builtin->bg_color.blue = builtin->bg_color.alpha = 1.0;
  /* black foreground */
  builtin->fg_color.alpha = 1.0;
}

GtkCssImage *
gtk_css_image_builtin_new (void)
{
  if (the_one_true_image == NULL)
    the_one_true_image = g_object_new (GTK_TYPE_CSS_IMAGE_BUILTIN, NULL);
  else
    g_object_ref (the_one_true_image);

  return the_one_true_image;
}

void
gtk_css_image_builtin_draw (GtkCssImage            *image,
                            cairo_t                *cr,
                            double                  width,
                            double                  height,
                            GtkCssImageBuiltinType  image_type)
{
  if (!GTK_IS_CSS_IMAGE_BUILTIN (image))
    {
      _gtk_css_image_draw (image, cr, width, height);
      return;
    }

  switch (image_type)
  {
  default:
    g_assert_not_reached ();
    break;
  case GTK_CSS_IMAGE_BUILTIN_NONE:
    break;
  case GTK_CSS_IMAGE_BUILTIN_CHECK:
  case GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT:
    gtk_css_image_builtin_draw_check (image, cr,
                                      width, height,
                                      image_type == GTK_CSS_IMAGE_BUILTIN_CHECK,
                                      image_type == GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT);
    break;
  case GTK_CSS_IMAGE_BUILTIN_OPTION:
  case GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT:
    gtk_css_image_builtin_draw_option (image, cr,
                                       width, height,
                                       image_type == GTK_CSS_IMAGE_BUILTIN_OPTION,
                                       image_type == GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT);
    break;
  case GTK_CSS_IMAGE_BUILTIN_ARROW_UP:
  case GTK_CSS_IMAGE_BUILTIN_ARROW_DOWN:
  case GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT:
  case GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT:
    gtk_css_image_builtin_draw_arrow (image, cr,
                                      width, height,
                                      image_type);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         TRUE, FALSE, FALSE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         FALSE, FALSE, FALSE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         TRUE, TRUE, FALSE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         FALSE, TRUE, FALSE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT_EXPANDED:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         TRUE, FALSE, TRUE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT_EXPANDED:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         FALSE, FALSE, TRUE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT_EXPANDED:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         TRUE, TRUE, TRUE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT_EXPANDED:
    gtk_css_image_builtin_draw_expander (image, cr,
                                         width, height,
                                         FALSE, TRUE, TRUE);
    break;
  case GTK_CSS_IMAGE_BUILTIN_PANE_SEPARATOR:
    gtk_css_image_builtin_draw_pane_separator (image, cr,
                                               width, height);
    break;
  case GTK_CSS_IMAGE_BUILTIN_HANDLE:
    gtk_css_image_builtin_draw_handle (image, cr,
                                       width, height);
    break;
  case GTK_CSS_IMAGE_BUILTIN_SPINNER:
    gtk_css_image_builtin_draw_spinner (image, cr,
                                        width, height);
    break;
  }
}

void
gtk_css_image_builtin_snapshot (GtkCssImage            *image,
                                GtkSnapshot            *snapshot,
                                double                  width,
                                double                  height,
                                GtkCssImageBuiltinType  image_type)
{
  g_return_if_fail (GTK_IS_CSS_IMAGE (image));
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  if (!GTK_IS_CSS_IMAGE_BUILTIN (image))
    {
      gtk_css_image_snapshot (image, snapshot, width, height);
      return;
    }

  if (image_type != GTK_CSS_IMAGE_BUILTIN_NONE)
    {
      cairo_t *cr = gtk_snapshot_append_cairo (snapshot,
                                      &GRAPHENE_RECT_INIT (0, 0, width, height));
      gtk_css_image_builtin_draw (image, cr, width, height, image_type);
      cairo_destroy (cr);
    }
}
