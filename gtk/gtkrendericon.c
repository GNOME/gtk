/* GTK - The GIMP Toolkit
 * Copyright (C) 2014,2015 Benjamin Otte
 * 
 * Authors: Benjamin Otte <otte@gnome.org>
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

#include "gtkrendericonprivate.h"

#include "gtkcssimagebuiltinprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcsstransformvalueprivate.h"

#include <math.h>

void
gtk_css_style_render_icon (GtkCssStyle            *style,
                           cairo_t                *cr,
                           double                  x,
                           double                  y,
                           double                  width,
                           double                  height,
                           GtkCssImageBuiltinType  builtin_type)
{
  const GtkCssValue *shadows;
  cairo_matrix_t matrix, transform_matrix, saved_matrix;
  GtkCssImage *image;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (cr != NULL);

  image = _gtk_css_image_value_get_image (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SOURCE));
  if (image == NULL)
    return;

  cairo_get_matrix (cr, &saved_matrix);

  shadows = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW);

  cairo_translate (cr, x, y);

  if (_gtk_css_transform_value_get_matrix (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM), &transform_matrix))
    {
      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      cairo_matrix_init_translate (&matrix, width / 2, height / 2);
      cairo_matrix_multiply (&matrix, &transform_matrix, &matrix);
      cairo_matrix_translate (&matrix, - width / 2, - height / 2);

      if (_gtk_css_shadows_value_is_none (shadows))
        {
          cairo_transform (cr, &matrix);
          gtk_css_image_builtin_draw (image, cr, width, height, builtin_type);
        }
      else
        {
          cairo_push_group (cr);
          cairo_transform (cr, &matrix);
          gtk_css_image_builtin_draw (image, cr, width, height, builtin_type);
          cairo_pop_group_to_source (cr);
          _gtk_css_shadows_value_paint_icon (shadows, cr);
          cairo_paint (cr);
        }
    }

  cairo_set_matrix (cr, &saved_matrix);
}

gboolean
get_surface_extents (cairo_surface_t *surface,
                     GdkRectangle    *out_extents)
{
  cairo_t *cr;
  gboolean result;

  cr = cairo_create (surface);
  result = gdk_cairo_get_clip_rectangle (cr, out_extents);
  cairo_destroy (cr);

  return result;
}

void
gtk_css_style_render_icon_surface (GtkCssStyle            *style,
                                   cairo_t                *cr,
                                   cairo_surface_t        *surface,
                                   double                  x,
                                   double                  y)
{
  const GtkCssValue *shadows;
  cairo_matrix_t matrix, transform_matrix, saved_matrix;
  GdkRectangle extents;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (surface != NULL);

  shadows = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW);

  if (!get_surface_extents (surface, &extents))
    {
      /* weird infinite surface, no special magic for you */
      cairo_set_source_surface (cr, surface, x, y);
      _gtk_css_shadows_value_paint_icon (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW), cr);
      cairo_paint (cr);
      return;
    }

  cairo_get_matrix (cr, &saved_matrix);
  cairo_translate (cr, x + extents.x, y + extents.y);

  if (_gtk_css_transform_value_get_matrix (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM), &transform_matrix))
    {
      cairo_pattern_t *pattern;

      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      cairo_matrix_init_translate (&matrix, extents.width / 2, extents.height / 2);
      cairo_matrix_multiply (&matrix, &transform_matrix, &matrix);
      cairo_matrix_translate (&matrix, - extents.width / 2, - extents.height / 2);
      if (cairo_matrix_invert (&matrix) != CAIRO_STATUS_SUCCESS)
        {
          g_assert_not_reached ();
        }
      cairo_matrix_translate (&matrix, extents.x, extents.y);

      pattern = cairo_pattern_create_for_surface (surface);
      cairo_pattern_set_matrix (pattern, &matrix);
      cairo_set_source (cr, pattern);
      cairo_pattern_destroy (pattern);

      _gtk_css_shadows_value_paint_icon (shadows, cr);
      cairo_paint (cr);
    }

  cairo_set_matrix (cr, &saved_matrix);
}

static void
gtk_cairo_rectangle_transform (cairo_rectangle_int_t       *dest,
                               const cairo_rectangle_int_t *src,
                               const cairo_matrix_t        *matrix)
{
  double x1, x2, x3, x4;
  double y1, y2, y3, y4;

  g_return_if_fail (dest != NULL);
  g_return_if_fail (src != NULL);
  g_return_if_fail (matrix != NULL);

  x1 = src->x;
  y1 = src->y;
  x2 = src->x + src->width;
  y2 = src->y;
  x3 = src->x + src->width;
  y3 = src->y + src->height;
  x4 = src->x;
  y4 = src->y + src->height;

  cairo_matrix_transform_point (matrix, &x1, &y1);
  cairo_matrix_transform_point (matrix, &x2, &y2);
  cairo_matrix_transform_point (matrix, &x3, &y3);
  cairo_matrix_transform_point (matrix, &x4, &y4);

  dest->x = floor (MIN (MIN (x1, x2), MIN (x3, x4)));
  dest->y = floor (MIN (MIN (y1, y2), MIN (y3, y4)));
  dest->width = ceil (MAX (MAX (x1, x2), MAX (x3, x4))) - dest->x;
  dest->height = ceil (MAX (MAX (y1, y2), MAX (y3, y4))) - dest->y;
}

void
gtk_css_style_render_icon_get_extents (GtkCssStyle  *style,
                                       GdkRectangle *extents,
                                       gint          x,
                                       gint          y,
                                       gint          width,
                                       gint          height)
{
  cairo_matrix_t transform_matrix, matrix;
  GtkBorder border;
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (extents != NULL);

  extents->x = x;
  extents->y = y;
  extents->width = width;
  extents->height = height;

  if (!_gtk_css_transform_value_get_matrix (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM), &transform_matrix))
    return;
  
  cairo_matrix_init_translate (&matrix, x + width / 2.0, y + height / 2.0);
  cairo_matrix_multiply (&matrix, &transform_matrix, &matrix);
  /* need to round to full pixels */
  rect.x = - (width + 1) / 2;
  rect.y = - (height + 1) / 2;
  rect.width = (width + 1) & ~1;
  rect.height = (height + 1) & ~1;
  gtk_cairo_rectangle_transform (extents, &rect, &matrix);

  _gtk_css_shadows_value_get_extents (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW), &border);

  extents->x -= border.left;
  extents->y -= border.top;
  extents->width += border.left + border.right;
  extents->height += border.top + border.bottom;
}

