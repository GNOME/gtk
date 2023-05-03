/*
 * Copyright © 2023 Benjamin Otte
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

#include "rendernodeutilsprivate.h"

static inline double
double_interpolate (double start,
                    double end,
                    double progress)
{
  return start + (end - start) * progress;
}

static inline float
float_interpolate (float  start,
                   float  end,
                   double progress)
{
  return start + (end - start) * progress;
}

static void
rgba_interpolate (GdkRGBA       *result,
                  const GdkRGBA *start,
                  const GdkRGBA *end,
                  double         progress)
{
  result->alpha = CLAMP (double_interpolate (start->alpha, end->alpha, progress), 0, 1);

  if (result->alpha <= 0.0)
    {
      result->red = result->green = result->blue = 0.0;
    }
  else
    {
      result->red   = CLAMP (double_interpolate (start->red * start->alpha, end->red * end->alpha, progress), 0,  1) / result->alpha;
      result->green = CLAMP (double_interpolate (start->green * start->alpha, end->green * end->alpha, progress), 0,  1) / result->alpha;
      result->blue  = CLAMP (double_interpolate (start->blue * start->alpha, end->blue * end->alpha, progress), 0,  1) / result->alpha;
    }
}

static void
rounded_rect_interpolate (GskRoundedRect       *dest,
                          const GskRoundedRect *start,
                          const GskRoundedRect *end,
                          double                progress)
{
  guint i;

  graphene_rect_interpolate (&start->bounds, &end->bounds, progress, &dest->bounds);
  for (i = 0; i < 4; i++)
    graphene_size_interpolate (&start->corner[i], &end->corner[i], progress, &dest->corner[i]);
}

static GskTransform *
transform_interpolate (GskTransform *start,
                       GskTransform *end,
                       double        progress)
{
  switch (MIN (gsk_transform_get_category (start), gsk_transform_get_category (end)))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      return NULL;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float startx, starty, endx, endy;

        gsk_transform_to_translate (start, &startx, &starty);
        gsk_transform_to_translate (end, &endx, &endy);
        return gsk_transform_translate (NULL,
                                        &GRAPHENE_POINT_INIT (float_interpolate (startx, endx, progress),
                                                              float_interpolate (starty, endy, progress)));
      }

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float start_skew_x, start_skew_y, start_scale_x, start_scale_y, start_angle, start_dx, start_dy;
        float end_skew_x, end_skew_y, end_scale_x, end_scale_y, end_angle, end_dx, end_dy;
        GskTransform *transform;

        gsk_transform_to_2d_components (start,
                                        &start_skew_x, &start_skew_y,
                                        &start_scale_x, &start_scale_y,
                                        &start_angle,
                                        &start_dx, &start_dy);
        gsk_transform_to_2d_components (end,
                                        &end_skew_x, &end_skew_y,
                                        &end_scale_x, &end_scale_y,
                                        &end_angle,
                                        &end_dx, &end_dy);
        transform = gsk_transform_translate (NULL,
                                             &GRAPHENE_POINT_INIT (float_interpolate (start_dx, end_dx, progress),
                                                                   float_interpolate (start_dy, end_dy, progress)));
        transform = gsk_transform_rotate (transform,
                                          float_interpolate (start_angle, end_angle, progress));
        transform = gsk_transform_scale (transform,
                                         float_interpolate (start_scale_x, end_scale_x, progress),
                                         float_interpolate (start_scale_y, end_scale_y, progress));
        transform = gsk_transform_skew (transform,
                                        float_interpolate (start_skew_x, end_skew_x, progress),
                                        float_interpolate (start_skew_y, end_skew_y, progress));
        return transform;
      }

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      {
        graphene_matrix_t start_matrix, end_matrix, matrix;

        gsk_transform_to_matrix (start, &start_matrix);
        gsk_transform_to_matrix (end, &end_matrix);
        graphene_matrix_interpolate (&start_matrix, &end_matrix, progress, &matrix);
        return gsk_transform_matrix (NULL, &matrix);
      }

    default:
      g_return_val_if_reached (NULL);
    }
}

static void
color_stops_interpolate (GskColorStop       *stops,
                         const GskColorStop *start_stops,
                         const GskColorStop *end_stops,
                         gsize               n_stops,
                         double              progress)
{
  guint i;

  for (i = 0; i < n_stops; i++)
    {
      stops[i].offset = float_interpolate (start_stops[i].offset, end_stops[i].offset, progress);
      rgba_interpolate (&stops[i].color, &start_stops[i].color, &end_stops[i].color, progress);
    }
}

GskRenderNode *
render_node_interpolate (GskRenderNode *start,
                         GskRenderNode *end,
                         double         progress)
{
  GskRenderNodeType start_type, end_type;

  if (progress <= 0)
    return gsk_render_node_ref (start);
  else if (progress >= 1)
    return gsk_render_node_ref (end);

  start_type = gsk_render_node_get_node_type (start);
  end_type = gsk_render_node_get_node_type (end);

  if (start_type == end_type)
    {
      switch (start_type)
        {
          case GSK_COLOR_NODE:
            {
              GdkRGBA rgba;
              graphene_rect_t start_bounds, end_bounds, bounds;

              rgba_interpolate (&rgba, gsk_color_node_get_color (start), gsk_color_node_get_color (end), progress);
              gsk_render_node_get_bounds (start, &start_bounds);
              gsk_render_node_get_bounds (end, &end_bounds);
              graphene_rect_interpolate (&start_bounds, &end_bounds, progress, &bounds);
              return gsk_color_node_new (&rgba, &bounds);
            }

          case GSK_DEBUG_NODE:
            {
              GskRenderNode *result, *child;

              /* xxx: do we need to interpolate the message somehow? */
              child = render_node_interpolate (gsk_debug_node_get_child (start),
                                               gsk_debug_node_get_child (end),
                                               progress);
              result = gsk_debug_node_new (child, g_strdup_printf ("progress %g", progress));
              gsk_render_node_unref (child);
              return result;
            }

          case GSK_CONTAINER_NODE:
            {
              GskRenderNode **nodes;
              GskRenderNode *result;
              gsize i, n_nodes;

              if (gsk_container_node_get_n_children (start) != gsk_container_node_get_n_children (end))
                break;

              n_nodes = gsk_container_node_get_n_children (start);
              nodes = g_new (GskRenderNode *, n_nodes);

              for (i = 0; i < n_nodes; i++)
                {
                  nodes[i] = render_node_interpolate (gsk_container_node_get_child (start, i),
                                                      gsk_container_node_get_child (end, i),
                                                      progress);
                }

              result = gsk_container_node_new (nodes, n_nodes);
              for (i = 0; i < n_nodes; i++)
                gsk_render_node_unref (nodes[i]);
              g_free (nodes);
              return result;
            }

          case GSK_TEXTURE_NODE:
            {
              graphene_rect_t start_bounds, end_bounds, bounds;

              gsk_render_node_get_bounds (start, &start_bounds);
              gsk_render_node_get_bounds (end, &end_bounds);
              graphene_rect_interpolate (&start_bounds, &end_bounds, progress, &bounds);
              return gsk_texture_node_new (gsk_texture_node_get_texture (progress > 0.5 ? end : start),
                                           &bounds);
            }

          case GSK_TEXTURE_SCALE_NODE:
            {
              graphene_rect_t start_bounds, end_bounds, bounds;

              gsk_render_node_get_bounds (start, &start_bounds);
              gsk_render_node_get_bounds (end, &end_bounds);
              graphene_rect_interpolate (&start_bounds, &end_bounds, progress, &bounds);
              return gsk_texture_scale_node_new (gsk_texture_scale_node_get_texture (progress > 0.5 ? end : start),
                                                 &bounds,
                                                 gsk_texture_scale_node_get_filter (progress > 0.5 ? end : start));
            }

          case GSK_TRANSFORM_NODE:
            {
              GskRenderNode *result, *child;
              GskTransform *transform;

              child = render_node_interpolate (gsk_transform_node_get_child (start),
                                               gsk_transform_node_get_child (end),
                                               progress);
              transform = transform_interpolate (gsk_transform_node_get_transform (start),
                                                 gsk_transform_node_get_transform (end),
                                                 progress);
              result = gsk_transform_node_new (child, transform);
              gsk_transform_unref (transform);
              gsk_render_node_unref (child);
              return result;
            }

          case GSK_CLIP_NODE:
            {
              GskRenderNode *result, *child;
              graphene_rect_t clip;

              graphene_rect_interpolate (gsk_clip_node_get_clip (start),
                                         gsk_clip_node_get_clip (end),
                                         progress,
                                         &clip);
              child = render_node_interpolate (gsk_clip_node_get_child (start),
                                               gsk_clip_node_get_child (end),
                                               progress);
              result = gsk_clip_node_new (child, &clip);
              gsk_render_node_unref (child);
              return result;
            }

          case GSK_ROUNDED_CLIP_NODE:
            {
              GskRenderNode *result, *child;
              GskRoundedRect clip;

              rounded_rect_interpolate (&clip,
                                        gsk_rounded_clip_node_get_clip (start),
                                        gsk_rounded_clip_node_get_clip (end),
                                        progress);
              child = render_node_interpolate (gsk_rounded_clip_node_get_child (start),
                                               gsk_rounded_clip_node_get_child (end),
                                               progress);
              result = gsk_rounded_clip_node_new (child, &clip);
              gsk_render_node_unref (child);
              return result;
            }

          case GSK_BORDER_NODE:
            {
              GdkRGBA color[4];
              const GdkRGBA *start_color, *end_color;
              float width[4];
              const float *start_width, *end_width;
              GskRoundedRect outline;
              guint i;

              rounded_rect_interpolate (&outline,
                                        gsk_border_node_get_outline (start),
                                        gsk_border_node_get_outline (end),
                                        progress);
              start_color = gsk_border_node_get_colors (start);
              end_color = gsk_border_node_get_colors (end);
              start_width = gsk_border_node_get_widths (start);
              end_width = gsk_border_node_get_widths (end);
              for (i = 0; i < 4; i++)
                {
                  rgba_interpolate (&color[i], &start_color[i], &end_color[i], progress);
                  width[i] = float_interpolate (start_width[i], end_width[i], progress);
                }
              return gsk_border_node_new (&outline, width, color);
            }

          case GSK_MASK_NODE:
            {
              GskRenderNode *source, *mask, *result;

              source = render_node_interpolate (gsk_mask_node_get_source (start),
                                                gsk_mask_node_get_source (end),
                                                progress);
              mask = render_node_interpolate (gsk_mask_node_get_mask (start),
                                              gsk_mask_node_get_mask (end),
                                              progress);
              result = gsk_mask_node_new (source,
                                          mask,
                                          gsk_mask_node_get_mask_mode (progress > 0.5 ? end : start));
              gsk_render_node_unref (source);
              gsk_render_node_unref (mask);
              return result;
            }

          case GSK_CONIC_GRADIENT_NODE:
            {
              graphene_rect_t start_bounds, end_bounds, bounds;
              graphene_point_t center;
              float rotation;
              GskColorStop *color_stops;
              gsize n_color_stops;
              GskRenderNode *result;

              if (gsk_conic_gradient_node_get_n_color_stops (start) != gsk_conic_gradient_node_get_n_color_stops (end))
                break;

              n_color_stops = gsk_conic_gradient_node_get_n_color_stops (start);
              color_stops = g_new (GskColorStop, n_color_stops);

              gsk_render_node_get_bounds (start, &start_bounds);
              gsk_render_node_get_bounds (end, &end_bounds);
              graphene_rect_interpolate (&start_bounds, &end_bounds, progress, &bounds);
              graphene_point_interpolate (gsk_conic_gradient_node_get_center (start),
                                          gsk_conic_gradient_node_get_center (end),
                                          progress,
                                          &center);
              rotation = float_interpolate (gsk_conic_gradient_node_get_rotation (start),
                                            gsk_conic_gradient_node_get_rotation (end),
                                            progress);
              color_stops_interpolate (color_stops,
                                       gsk_conic_gradient_node_get_color_stops (start, NULL),
                                       gsk_conic_gradient_node_get_color_stops (end, NULL),
                                       n_color_stops,
                                       progress);
              result = gsk_conic_gradient_node_new (&bounds,
                                                    &center,
                                                    rotation,
                                                    color_stops,
                                                    n_color_stops);
              g_free (color_stops);
              return result;
            }

          case GSK_REPEAT_NODE:
            {
              graphene_rect_t start_bounds, end_bounds, bounds, child_bounds;
              GskRenderNode *child, *result;

              gsk_render_node_get_bounds (start, &start_bounds);
              gsk_render_node_get_bounds (end, &end_bounds);
              graphene_rect_interpolate (&start_bounds, &end_bounds, progress, &bounds);
              graphene_rect_interpolate (gsk_repeat_node_get_child_bounds (start),
                                         gsk_repeat_node_get_child_bounds (end),
                                         progress,
                                         &child_bounds);
              child = render_node_interpolate (gsk_repeat_node_get_child (start),
                                               gsk_repeat_node_get_child (end),
                                               progress);
              result = gsk_repeat_node_new (&bounds, child, &child_bounds);
              gsk_render_node_unref (child);
              return result;
            }

          case GSK_LINEAR_GRADIENT_NODE:
          case GSK_REPEATING_LINEAR_GRADIENT_NODE:
          case GSK_RADIAL_GRADIENT_NODE:
          case GSK_REPEATING_RADIAL_GRADIENT_NODE:
          case GSK_INSET_SHADOW_NODE:
          case GSK_OUTSET_SHADOW_NODE:
          case GSK_OPACITY_NODE:
          case GSK_COLOR_MATRIX_NODE:
          case GSK_BLUR_NODE:
          case GSK_SHADOW_NODE:
          case GSK_BLEND_NODE:
          case GSK_CROSS_FADE_NODE:
          case GSK_FILL_NODE:
          case GSK_STROKE_NODE:
            g_warning ("FIXME: not implemented");
            break;

          case GSK_GL_SHADER_NODE:
          case GSK_CAIRO_NODE:
          case GSK_TEXT_NODE:
            break;

          case GSK_NOT_A_RENDER_NODE:
          default:
            g_assert_not_reached ();
            break;
        }
    }
  
  return gsk_cross_fade_node_new (start, end, progress);
}

