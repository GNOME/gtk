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

#include "gskmaskutilsprivate.h"

#include "gsk/gskrendernodeprivate.h"

static GskRenderNode *  gsk_render_node_as_mask                  (GskRenderNode  *mask,
                                                                 GskRenderNode  *source,
                                                                 GskMaskMode     mask_mode);

static GskRenderNode *
gsk_render_node_as_mask (GskRenderNode *mask,
                         GskRenderNode *source,
                         GskMaskMode    mask_mode)
{
  switch (gsk_render_node_get_node_type (mask))
  {
    case GSK_COLOR_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_ISOLATION_NODE:
    case GSK_COPY_NODE:
    case GSK_PASTE_NODE:
    case GSK_BLUR_NODE:
    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
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
      return NULL;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

GskRenderNode *
gsk_mask_node_try_new_optimized (GskRenderNode *source,
                                 GskRenderNode *mask,
                                 GskMaskMode    mask_mode)
{
  return gsk_render_node_as_mask (mask, source, mask_mode);
}
