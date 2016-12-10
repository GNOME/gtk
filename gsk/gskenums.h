/* GSK - The GTK Scene Kit
 * Copyright 2016  Endless 
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

#ifndef __GSK_ENUMS_H__
#define __GSK_ENUMS_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

/**
 * GskRenderNodeType:
 * @GSK_NOT_A_RENDER_NODE: Error type. No node will ever have this type.
 * @GSK_CONTAINER_NODE: A node containing a stack of children
 * @GSK_CAIRO_NODE: A node drawing a #cairo_surface_t
 * @GSK_TEXTURE_NODE: A node drawing a #GskTexture
 *
 * The type of a node determines what the node is rendering.
 *
 * Since: 3.90
 **/
typedef enum {
  GSK_NOT_A_RENDER_NODE = 0,
  GSK_CONTAINER_NODE,
  GSK_CAIRO_NODE,
  GSK_TEXTURE_NODE
} GskRenderNodeType;

/**
 * GskScalingFilter:
 * @GSK_SCALING_FILTER_LINEAR: linear interpolation filter
 * @GSK_SCALING_FILTER_NEAREST: nearest neighbor interpolation filter
 * @GSK_SCALING_FILTER_TRILINEAR: linear interpolation along each axis,
 *   plus mipmap generation, with linear interpolation along the mipmap
 *   levels
 *
 * The filters used when scaling texture data.
 *
 * The actual implementation of each filter is deferred to the
 * rendering pipeline.
 *
 * Since: 3.90
 */
typedef enum {
  GSK_SCALING_FILTER_LINEAR,
  GSK_SCALING_FILTER_NEAREST,
  GSK_SCALING_FILTER_TRILINEAR
} GskScalingFilter;

/**
 * GskBlendMode:
 * @GSK_BLEND_MODE_DEFAULT: The default blend mode, which specifies no blending
 * @GSK_BLEND_MODE_MULTIPLY: The source color is multiplied by the destination
 *   and replaces the destination
 * @GSK_BLEND_MODE_SCREEN: ...
 * @GSK_BLEND_MODE_OVERLAY: ...
 * @GSK_BLEND_MODE_DARKEN: ...
 * @GSK_BLEND_MODE_LIGHTEN: ...
 * @GSK_BLEND_MODE_COLOR_DODGE: ...
 * @GSK_BLEND_MODE_COLOR_BURN: ...
 * @GSK_BLEND_MODE_HARD_LIGHT: ...
 * @GSK_BLEND_MODE_SOFT_LIGHT: ...
 * @GSK_BLEND_MODE_DIFFERENCE: ...
 * @GSK_BLEND_MODE_EXCLUSION: ...
 *
 * The blend modes available for render nodes.
 *
 * The implementation of each blend mode is deferred to the
 * rendering pipeline.
 *
 * Since: 3.90
 */
typedef enum {
  GSK_BLEND_MODE_DEFAULT = 0,

  GSK_BLEND_MODE_MULTIPLY,
  GSK_BLEND_MODE_SCREEN,
  GSK_BLEND_MODE_OVERLAY,
  GSK_BLEND_MODE_DARKEN,
  GSK_BLEND_MODE_LIGHTEN,
  GSK_BLEND_MODE_COLOR_DODGE,
  GSK_BLEND_MODE_COLOR_BURN,
  GSK_BLEND_MODE_HARD_LIGHT,
  GSK_BLEND_MODE_SOFT_LIGHT,
  GSK_BLEND_MODE_DIFFERENCE,
  GSK_BLEND_MODE_EXCLUSION
} GskBlendMode;

#endif /* __GSK_TYPES_H__ */
