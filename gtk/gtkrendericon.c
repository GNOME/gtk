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

#include "gtkcssfiltervalueprivate.h"
#include "gtkcssimagebuiltinprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkiconthemeprivate.h"
#include "gtksnapshotprivate.h"

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
  graphene_matrix_t graphene_matrix;
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

  if (gtk_css_transform_value_get_matrix (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM), &graphene_matrix) &&
      graphene_matrix_is_2d (&graphene_matrix))
    {
      graphene_matrix_to_2d (&graphene_matrix,
                             &transform_matrix.xx, &transform_matrix.yx,
                             &transform_matrix.xy, &transform_matrix.yy,
                             &transform_matrix.x0, &transform_matrix.y0);
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

void
gtk_css_style_snapshot_icon (GtkCssStyle            *style,
                             GtkSnapshot            *snapshot,
                             double                  width,
                             double                  height,
                             GtkCssImageBuiltinType  builtin_type)
{
  const GtkCssValue *shadows_value, *transform_value, *filter_value;
  graphene_matrix_t transform_matrix;
  GtkCssImage *image;
  gboolean has_shadow;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (snapshot != NULL);

  if (width == 0.0 || height == 0.0)
    return;

  image = _gtk_css_image_value_get_image (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SOURCE));
  if (image == NULL)
    return;

  shadows_value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW);
  transform_value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM);
  filter_value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_FILTER);

  if (!gtk_css_transform_value_get_matrix (transform_value, &transform_matrix))
    return;

  gtk_css_filter_value_push_snapshot (filter_value, snapshot);

  has_shadow = gtk_css_shadows_value_push_snapshot (shadows_value, snapshot);

  if (graphene_matrix_is_identity (&transform_matrix))
    {
      gtk_css_image_builtin_snapshot (image, snapshot, width, height, builtin_type);
    }
  else
    {
      graphene_matrix_t m1, m2, m3;

      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      graphene_matrix_init_translate (&m1, &GRAPHENE_POINT3D_INIT (width / 2.0, height / 2.0, 0));
      graphene_matrix_multiply (&transform_matrix, &m1, &m3);
      graphene_matrix_init_translate (&m2, &GRAPHENE_POINT3D_INIT (- width / 2.0, - height / 2.0, 0));
      graphene_matrix_multiply (&m2, &m3, &m1);

      gtk_snapshot_push_transform (snapshot, &m1, "CSS Icon Transform Container");

      gtk_css_image_builtin_snapshot (image, snapshot, width, height, builtin_type);

      gtk_snapshot_pop (snapshot);
    }

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  gtk_css_filter_value_pop_snapshot (filter_value, snapshot);
}

static gboolean
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
  graphene_matrix_t graphene_matrix;
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

  if (gtk_css_transform_value_get_matrix (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM), &graphene_matrix) &&
      graphene_matrix_is_2d (&graphene_matrix))
    {
      cairo_pattern_t *pattern;

      graphene_matrix_to_2d (&graphene_matrix,
                             &transform_matrix.xx, &transform_matrix.yx,
                             &transform_matrix.xy, &transform_matrix.yy,
                             &transform_matrix.x0, &transform_matrix.y0);
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

void
gtk_css_style_render_icon_get_extents (GtkCssStyle  *style,
                                       GdkRectangle *extents,
                                       gint          x,
                                       gint          y,
                                       gint          width,
                                       gint          height)
{
  graphene_matrix_t transform_matrix, translate_matrix, matrix;
  graphene_rect_t bounds;
  GtkBorder border;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (extents != NULL);

  extents->x = x;
  extents->y = y;
  extents->width = width;
  extents->height = height;

  if (!gtk_css_transform_value_get_matrix (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM), &transform_matrix))
    return;

  graphene_matrix_init_translate (&translate_matrix, &GRAPHENE_POINT3D_INIT(x + width / 2.0, y + height / 2.0, 0));
  graphene_matrix_multiply (&transform_matrix, &translate_matrix, &matrix);
  graphene_rect_init (&bounds,
                      - width / 2.0, - height / 2.0,
                      width, height);
  /* need to round to full pixels */
  graphene_matrix_transform_bounds (&matrix, &bounds, &bounds);

  _gtk_css_shadows_value_get_extents (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW), &border);

  extents->x = floorf (bounds.origin.x) - border.left;
  extents->y = floorf (bounds.origin.y) - border.top;
  extents->width = ceilf (bounds.origin.x + bounds.size.width) - extents->x + border.right;
  extents->height = ceilf (bounds.origin.y + bounds.size.height) - extents->y + border.bottom;
}

void
gtk_css_style_snapshot_icon_texture (GtkCssStyle *style,
                                     GtkSnapshot *snapshot,
                                     GdkTexture  *texture,
                                     double       texture_scale,
                                     gboolean     recolor)
{
  const GtkCssValue *shadows_value, *transform_value, *filter_value;
  graphene_matrix_t transform_matrix;
  graphene_rect_t bounds;
  double width, height;
  gboolean has_shadow;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (texture_scale > 0);

  shadows_value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SHADOW);
  transform_value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_TRANSFORM);
  filter_value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_FILTER);
  width = gdk_texture_get_width (texture) / texture_scale;
  height = gdk_texture_get_height (texture) / texture_scale;

  if (!gtk_css_transform_value_get_matrix (transform_value, &transform_matrix))
    return;

  gtk_css_filter_value_push_snapshot (filter_value, snapshot);

  has_shadow = gtk_css_shadows_value_push_snapshot (shadows_value, snapshot);

  if (recolor)
    {
      graphene_matrix_t color_matrix;
      graphene_vec4_t color_offset;
      GdkRGBA fg, sc, wc, ec;

      gtk_icon_theme_lookup_symbolic_colors (style, &fg, &sc, &wc, &ec);

      graphene_matrix_init_from_float (&color_matrix, (float[16]) {
                                         sc.red - fg.red, sc.green - fg.green, sc.blue - fg.blue, 0,
                                         wc.red - fg.red, wc.green - fg.green, wc.blue - fg.blue, 0,
                                         ec.red - fg.red, ec.green - fg.green, ec.blue - fg.blue, 0,
                                         0, 0, 0, fg.alpha
                                       });
      graphene_vec4_init (&color_offset, fg.red, fg.green, fg.blue, 0);

      gtk_snapshot_push_color_matrix (snapshot, &color_matrix, &color_offset, "Recoloring Icon");
    }

  if (graphene_matrix_is_identity (&transform_matrix))
    {
      graphene_rect_init (&bounds, 0, 0, width, height);
      gtk_snapshot_append_texture (snapshot, texture, &bounds, "Icon");
    }
  else
    {
      graphene_matrix_t m1, m2, m3;

      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      graphene_matrix_init_translate (&m1, &GRAPHENE_POINT3D_INIT (width / 2.0, height / 2.0, 0));
      graphene_matrix_multiply (&transform_matrix, &m1, &m3);
      graphene_matrix_init_translate (&m2, &GRAPHENE_POINT3D_INIT (- width / 2.0, - height / 2.0, 0));
      graphene_matrix_multiply (&m2, &m3, &m1);
      graphene_matrix_scale (&m1, 1.0 / texture_scale, 1.0 / texture_scale, 1);

      gtk_snapshot_push_transform (snapshot, &m1, "Icon Transform");

      graphene_rect_init (&bounds, 0, 0, gdk_texture_get_width (texture), gdk_texture_get_height (texture));
      gtk_snapshot_append_texture (snapshot, texture, &bounds, "Icon");

      gtk_snapshot_pop (snapshot);
    }

  if (recolor)
    gtk_snapshot_pop (snapshot);

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  gtk_css_filter_value_pop_snapshot (filter_value, snapshot);
}
