/* GSK - The GTK Scene Kit
 *
 * Copyright 2026  Benjamin Otte
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

#include "gskblurutilsprivate.h"

#include <gsk/gsk.h>

#include "gpu/gskgputransformprivate.h"
#include "gskpathprivate.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskroundedrectprivate.h"

#define BACKGROUND_BLUR_THRESHOLD 20.0

typedef struct {
  GskGpuTransform transform;
  /* these are in device pixels */
  cairo_region_t *unblurred_bg;
  cairo_region_t *blurred_bg;
} Render;

static void
render_clear (Render *render)
{
  g_clear_pointer (&render->unblurred_bg, cairo_region_destroy);
  g_clear_pointer (&render->blurred_bg, cairo_region_destroy);
}

static void
render_copy (Render *dest,
             Render *src)
{
  dest->transform = src->transform;

  if (src->unblurred_bg)
    dest->unblurred_bg = cairo_region_copy (src->unblurred_bg);
  else
    dest->unblurred_bg = NULL;

  if (src->blurred_bg)
    dest->blurred_bg = cairo_region_copy (src->blurred_bg);
  else
    dest->blurred_bg = NULL;
}

static void
render_clip_region (Render               *render,
                    const cairo_region_t *region)
{
  if (render->unblurred_bg)
    cairo_region_intersect (render->unblurred_bg, region);
  if (render->blurred_bg)
    cairo_region_intersect (render->blurred_bg, region);
}

static void
render_clip_rect (Render                *render,
                  cairo_rectangle_int_t *rect)
{
  if (render->unblurred_bg)
    cairo_region_intersect_rectangle (render->unblurred_bg, rect);
  if (render->blurred_bg)
    cairo_region_intersect_rectangle (render->blurred_bg, rect);
}

static void
render_subtract_region (Render               *render,
                        const cairo_region_t *region)
{
  if (render->unblurred_bg)
    cairo_region_subtract (render->unblurred_bg, region);
  if (render->blurred_bg)
    cairo_region_subtract (render->blurred_bg, region);
}

static void
render_subtract_rect (Render                *render,
                      cairo_rectangle_int_t *rect)
{
  if (render->unblurred_bg)
    cairo_region_subtract_rectangle (render->unblurred_bg, rect);
  if (render->blurred_bg)
    cairo_region_subtract_rectangle (render->blurred_bg, rect);
}

static void
render_merge (Render *render,
              Render *child)
{
  if (child->unblurred_bg)
    {
      if (render->unblurred_bg)
        cairo_region_union (render->unblurred_bg, child->unblurred_bg);
      else
        render->unblurred_bg = cairo_region_copy (child->unblurred_bg);
      if (render->blurred_bg)
        cairo_region_subtract (render->blurred_bg, child->unblurred_bg);
    }
  if (child->blurred_bg)
    {
      if (render->unblurred_bg)
        cairo_region_subtract (render->unblurred_bg, child->blurred_bg);
      if (render->blurred_bg)
        cairo_region_union (render->blurred_bg, child->blurred_bg);
      else
        render->blurred_bg = cairo_region_copy (child->blurred_bg);
    }
}

/*<private>
 * region_create_from_bitmap:
 * @data: bitmap data
 * @width: width of the bitmap
 * @height: height of the bitmap
 * @stride: stride of the bitmap
 *
 * Creates region for the given bitmap.
 *
 * Returns: (transfer full): a #cairo_region_t
 */
static cairo_region_t *
region_create_from_bitmap (const guchar *data,
                           gsize         width,
                           gsize         height,
                           gsize         stride)
{
  cairo_region_t *region;
  GdkRectangle rect;
  gsize x, y;

  region = cairo_region_create ();

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          /* Search for a continuous range of "non transparent pixels"*/
          gint x0 = x;
          while (x < width)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              if (((data[x / 8] >> (x%8)) & 1) == 0)
#else
              if (((data[x / 8] >> (7-(x%8))) & 1) == 0)
#endif
                /* This pixel is "transparent"*/
                break;
              x++;
            }

          if (x > x0)
            {
              /* Add the pixels (x0, y) to (x, y+1) as a new rectangle
               * in the region
               */
              rect.x = x0;
              rect.width = x - x0;
              rect.y = y;
              rect.height = 1;

              cairo_region_union_rectangle (region, &rect);
            }
        }
      data += stride;
    }

  return region;
}

static void
compute_background_blur (GskRenderNode *node,
                         Render        *render,
                         GSList         *copies);

static void
render_clipped_with_cairo (Render                *render,
                           GSList                *copies,
                           const graphene_rect_t *clip_bounds,
                           GskRenderNode         *node,
                           void                   (* clip_func) (cairo_t *, gpointer),
                           gpointer               clip_data)
{
  cairo_surface_t *bitmap;
  cairo_t *cr;
  cairo_region_t *region;
  graphene_rect_t transformed;
  cairo_rectangle_int_t clip;
  cairo_matrix_t matrix;
  Render child_render;

  render_copy (&child_render, render);
  compute_background_blur (node, &child_render, copies);

  gsk_gpu_transform_transform_rect (&render->transform, clip_bounds, &transformed);
  gsk_rect_to_cairo_grow (&transformed, &clip);
  bitmap = cairo_image_surface_create (CAIRO_FORMAT_A1, clip.width, clip.height);
  cr = cairo_create (bitmap);
  cairo_translate (cr, - clip.x, - clip.y);
  gsk_gpu_transform_to_cairo_matrix (&render->transform, &matrix);
  cairo_transform (cr, &matrix);

  clip_func (cr, clip_data);

  cairo_destroy (cr);

  cairo_surface_flush (bitmap);
  region = region_create_from_bitmap (cairo_image_surface_get_data (bitmap),
                                      clip.width, clip.height,
                                      cairo_image_surface_get_stride (bitmap));
  cairo_surface_destroy (bitmap);

  cairo_region_translate (region, clip.x, clip.y);

  render_clip_region (&child_render, region);
  render_subtract_region (render, region);
  render_merge (render, &child_render);
  render_clear (&child_render);
  cairo_region_destroy (region);
}

static cairo_region_t *
region_create_for_rounded_rect (const GskRoundedRect *rect)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_rectangle_int_t clip;
  cairo_region_t *result;

  gsk_rect_to_cairo_grow (&rect->bounds, &clip);
  surface = cairo_image_surface_create (CAIRO_FORMAT_A1, clip.width, clip.height);
  cr = cairo_create (surface);
  cairo_translate (cr, - clip.x, - clip.y);
  gsk_rounded_rect_path (rect, cr);
  cairo_fill (cr);
  cairo_destroy (cr);

  cairo_surface_flush (surface);

  result = region_create_from_bitmap (cairo_image_surface_get_data (surface),
                                      clip.width, clip.height,
                                      cairo_image_surface_get_stride (surface));
  cairo_surface_destroy (surface);

  cairo_region_translate (result, clip.x, clip.y);

  return result;
}

static void
compute_isolation_node_blur (GskRenderNode *node,
                             Render        *render,
                             GSList        *copies)
{
  GskIsolation isolations;
  GskRenderNode *child;

  isolations = gsk_isolation_node_get_isolations (node);
  child = gsk_isolation_node_get_child (node);

  if (isolations & GSK_ISOLATION_COPY_PASTE)
    copies = NULL;

  if (isolations & GSK_ISOLATION_BACKGROUND)
    {
      Render child_render = { render->transform, NULL, NULL };
      compute_background_blur (child, &child_render, copies);
      render_merge (render, &child_render);
      render_clear (&child_render);
    }
  else
    {
      compute_background_blur (child, render, copies);
    }
}

static void
compute_copy_node_blur (GskRenderNode *node,
                        Render        *render,
                        GSList        *copies)
{
  Render copy_render = {
    .transform = GSK_GPU_TRANSFORM_IDENTITY,
    .unblurred_bg = render->unblurred_bg ? cairo_region_copy (render->unblurred_bg) : NULL,
    .blurred_bg = render->blurred_bg ? cairo_region_copy (render->blurred_bg) : NULL,
  };
  GSList copy = { &copy_render, copies };

  compute_background_blur (gsk_copy_node_get_child (node), render, &copy);

  render_clear (&copy_render);
}

static void
compute_paste_node_blur (GskRenderNode *node,
                         Render        *render,
                         GSList        *copies)
{
  GSList *copy;
  Render paste;
  graphene_rect_t bounds;
  cairo_rectangle_int_t clip;

  copy = g_slist_nth (copies, gsk_paste_node_get_depth (node));
  if (copy == NULL)
    return;

  render_copy (&paste, copy->data);

  gsk_render_node_get_bounds (node, &bounds);
  gsk_gpu_transform_transform_rect (&render->transform,
                                    &bounds,
                                    &bounds);
  if (!gsk_rect_snap (&bounds, gsk_paste_node_get_snap (node), &bounds))
    return;
  gsk_rect_to_cairo_grow (&bounds, &clip);
  render_clip_rect (&paste, &clip);
  
  render_merge (render, &paste);
  render_clear (&paste);
}

static void
compute_blur_node_blur (GskRenderNode *node,
                        Render        *render,
                        GSList        *copies)
{
  Render child_render = { render->transform, NULL, };

  compute_background_blur (gsk_blur_node_get_child (node), &child_render, copies);

  if (gsk_blur_node_get_radius (node) >= BACKGROUND_BLUR_THRESHOLD)
    {
      if (child_render.unblurred_bg)
        {
          if (child_render.blurred_bg)
            {
              cairo_region_union (child_render.blurred_bg, child_render.unblurred_bg);
              g_clear_pointer (&child_render.unblurred_bg, cairo_region_destroy);
            }
          else
            {
              child_render.blurred_bg = g_steal_pointer (&child_render.unblurred_bg);
            }
        }
    }
  render_merge (render, &child_render);
  render_clear (&child_render);
}

static void
compute_transform_node_blur (GskRenderNode *node,
                             Render        *render,
                             GSList        *copies)
{
  GskGpuTransform save;

  save = render->transform;

  /* give up if the transform cannot be represented */
  if (!gsk_gpu_transform_transform (&render->transform, gsk_transform_node_get_transform (node)))
    return;

  compute_background_blur (gsk_transform_node_get_child (node), render, copies);
  
  render->transform = save;
}

static void
compute_clip_node_blur (GskRenderNode *node,
                        Render        *render,
                        GSList        *copies)
{
  Render child_render;
  graphene_rect_t transformed;
  cairo_rectangle_int_t clip;

  render_copy (&child_render, render);
  compute_background_blur (gsk_clip_node_get_child (node), &child_render, copies);

  gsk_gpu_transform_transform_rect (&render->transform,
                                    gsk_clip_node_get_clip (node),
                                    &transformed);
  if (!gsk_rect_snap (&transformed, gsk_clip_node_get_snap (node), &transformed))
    return;

  gsk_rect_to_cairo_grow (&transformed, &clip);
  render_clip_rect (&child_render, &clip);
  render_subtract_rect (render, &clip);
  render_merge (render, &child_render);
  render_clear (&child_render);
}

static void
compute_rounded_clip_node_blur (GskRenderNode *node,
                                Render        *render,
                                GSList        *copies)
{
  Render child_render;
  GskRoundedRect transformed;
  cairo_region_t *clip;

  render_copy (&child_render, render);
  compute_background_blur (gsk_rounded_clip_node_get_child (node), &child_render, copies);

  gsk_gpu_transform_transform_rounded_rect (&render->transform,
                                            gsk_rounded_clip_node_get_clip (node),
                                            &transformed);
  if (!gsk_rect_snap (&transformed.bounds,
                      gsk_rounded_clip_node_get_snap (node),
                      &transformed.bounds))
    return;

  clip = region_create_for_rounded_rect (&transformed);

  render_clip_region (&child_render, clip);
  render_subtract_region (render, clip);
  render_merge (render, &child_render);
  render_clear (&child_render);
  cairo_region_destroy (clip);
}

static void
render_fill_node (cairo_t  *cr,
                  gpointer  node)
{
  switch (gsk_fill_node_get_fill_rule (node))
  {
    case GSK_FILL_RULE_WINDING:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
      break;
    case GSK_FILL_RULE_EVEN_ODD:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  gsk_path_to_cairo (gsk_fill_node_get_path (node), cr);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill (cr);
}

static void
compute_fill_node_blur (GskRenderNode *node,
                        Render        *render,
                        GSList        *copies)
{
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);
  render_clipped_with_cairo (render,
                             copies,
                             &bounds,
                             gsk_fill_node_get_child (node),
                             render_fill_node,
                             node);
}

static void
render_stroke_node (cairo_t  *cr,
                    gpointer  node)
{
  cairo_set_source_rgb (cr, 1, 1, 1);
  gsk_cairo_stroke_path (cr,
                         gsk_stroke_node_get_path (node),
                         gsk_stroke_node_get_stroke (node));
}

static void
compute_stroke_node_blur (GskRenderNode *node,
                          Render        *render,
                          GSList        *copies)
{
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);
  render_clipped_with_cairo (render,
                             copies,
                             &bounds,
                             gsk_stroke_node_get_child (node),
                             render_stroke_node,
                             node);
}

static void
compute_background_blur (GskRenderNode *node,
                         Render        *render,
                         GSList        *copies)
{
  GskRenderNode **children;
  gsize i, n_children;

  if (gsk_render_node_get_copy_mode (node) == GSK_COPY_NONE &&
      copies == NULL)
    return;

  switch (gsk_render_node_get_node_type (node))
  {
    case GSK_ISOLATION_NODE:
      compute_isolation_node_blur (node, render, copies);
      break;

    case GSK_COPY_NODE:
      compute_copy_node_blur (node, render, copies);
      break;

    case GSK_PASTE_NODE:
      compute_paste_node_blur (node, render, copies);
      break;

    case GSK_BLUR_NODE:
      compute_blur_node_blur (node, render, copies);
      break;

    case GSK_CLIP_NODE:
      compute_clip_node_blur (node, render, copies);
      break;

    case GSK_ROUNDED_CLIP_NODE:
      compute_rounded_clip_node_blur (node, render, copies);
      break;

    case GSK_TRANSFORM_NODE:
      compute_transform_node_blur (node, render, copies);
      break;

    case GSK_FILL_NODE:
      compute_fill_node_blur (node, render, copies);
      break;

    case GSK_STROKE_NODE:
      compute_stroke_node_blur (node, render, copies);
      break;

    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_TEXT_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_MASK_NODE:
    case GSK_DEBUG_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COMPOSITE_NODE:
    case GSK_DISPLACEMENT_NODE:
    case GSK_ARITHMETIC_NODE:
    case GSK_TURBULENCE_NODE:
      children = gsk_render_node_get_children (node, &n_children);
      for (i = 0; i < n_children; i++)
        {
          if (gsk_render_node_isolates_background (node))
            {
              Render child_render = { render->transform, NULL, NULL };
              compute_background_blur (children[i], &child_render, copies);
              render_merge (render, &child_render);
              render_clear (&child_render);
            }
          else
            {
              compute_background_blur (children[i], render, copies);
            }
        }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      break;
  }
}

cairo_region_t *
gsk_render_node_compute_background_blur (GskRenderNode *node)
{
  Render render;
  graphene_rect_t bounds;
  cairo_rectangle_int_t cairo_bounds;

  gsk_render_node_get_bounds (node, &bounds);
  gsk_rect_to_cairo_grow (&bounds, &cairo_bounds);
  render.transform = GSK_GPU_TRANSFORM_IDENTITY;
  render.blurred_bg = NULL;
  render.unblurred_bg = cairo_region_create_rectangle (&cairo_bounds);

  compute_background_blur (node, &render, NULL);

  cairo_region_destroy (render.unblurred_bg);
  
  return render.blurred_bg;
}
