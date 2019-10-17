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
 * @GSK_COLOR_NODE: A node drawing a single color rectangle
 * @GSK_LINEAR_GRADIENT_NODE: A node drawing a linear gradient
 * @GSK_REPEATING_LINEAR_GRADIENT_NODE: A node drawing a repeating linear gradient
 * @GSK_BORDER_NODE: A node stroking a border around an area
 * @GSK_TEXTURE_NODE: A node drawing a #GdkTexture
 * @GSK_INSET_SHADOW_NODE: A node drawing an inset shadow
 * @GSK_OUTSET_SHADOW_NODE: A node drawing an outset shadow
 * @GSK_TRANSFORM_NODE: A node that renders its child after applying a matrix transform
 * @GSK_OPACITY_NODE: A node that changes the opacity of its child
 * @GSK_COLOR_MATRIX_NODE: A node that applies a color matrix to every pixel
 * @GSK_REPEAT_NODE: A node that repeats the child's contents
 * @GSK_CLIP_NODE: A node that clips its child to a rectangular area
 * @GSK_ROUNDED_CLIP_NODE: A node that clips its child to a rounded rectangle
 * @GSK_SHADOW_NODE: A node that draws a shadow below its child
 * @GSK_BLEND_NODE: A node that blends two children together
 * @GSK_CROSS_FADE_NODE: A node that cross-fades between two children
 * @GSK_TEXT_NODE: A node containing a glyph string
 * @GSK_BLUR_NODE: A node that applies a blur
 *
 * The type of a node determines what the node is rendering.
 **/
typedef enum {
  GSK_NOT_A_RENDER_NODE = 0,
  GSK_CONTAINER_NODE,
  GSK_CAIRO_NODE,
  GSK_COLOR_NODE,
  GSK_LINEAR_GRADIENT_NODE,
  GSK_REPEATING_LINEAR_GRADIENT_NODE,
  GSK_BORDER_NODE,
  GSK_TEXTURE_NODE,
  GSK_INSET_SHADOW_NODE,
  GSK_OUTSET_SHADOW_NODE,
  GSK_TRANSFORM_NODE,
  GSK_OPACITY_NODE,
  GSK_COLOR_MATRIX_NODE,
  GSK_REPEAT_NODE,
  GSK_CLIP_NODE,
  GSK_ROUNDED_CLIP_NODE,
  GSK_SHADOW_NODE,
  GSK_BLEND_NODE,
  GSK_CROSS_FADE_NODE,
  GSK_TEXT_NODE,
  GSK_BLUR_NODE,
  GSK_DEBUG_NODE
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
 * @GSK_BLEND_MODE_COLOR: ...
 * @GSK_BLEND_MODE_HUE: ...
 * @GSK_BLEND_MODE_SATURATION: ...
 * @GSK_BLEND_MODE_LUMINOSITY: ...
 *
 * The blend modes available for render nodes.
 *
 * The implementation of each blend mode is deferred to the
 * rendering pipeline.
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
  GSK_BLEND_MODE_EXCLUSION,
  GSK_BLEND_MODE_COLOR,
  GSK_BLEND_MODE_HUE,
  GSK_BLEND_MODE_SATURATION,
  GSK_BLEND_MODE_LUMINOSITY
} GskBlendMode;

/**
 * GskCorner:
 * @GSK_CORNER_TOP_LEFT: The top left corner
 * @GSK_CORNER_TOP_RIGHT: The top right corner
 * @GSK_CORNER_BOTTOM_RIGHT: The bottom right corner
 * @GSK_CORNER_BOTTOM_LEFT: The bottom left corner
 *
 * The corner indices used by #GskRoundedRect.
 */
typedef enum {
  GSK_CORNER_TOP_LEFT,
  GSK_CORNER_TOP_RIGHT,
  GSK_CORNER_BOTTOM_RIGHT,
  GSK_CORNER_BOTTOM_LEFT
} GskCorner;

/**
 * GskSerializationError:
 * @GSK_SERIALIZATION_UNSUPPORTED_FORMAT: The format can not be
 *     identified
 * @GSK_SERIALIZATION_UNSUPPORTED_VERSION: The version of the data
 *     is not understood
 * @GSK_SERIALIZATION_INVALID_DATA: The given data may not exist in
 *     a proper serialization
 *
 * Errors that can happen during (de)serialization.
 */
typedef enum {
  GSK_SERIALIZATION_UNSUPPORTED_FORMAT,
  GSK_SERIALIZATION_UNSUPPORTED_VERSION,
  GSK_SERIALIZATION_INVALID_DATA
} GskSerializationError;

/**
 * GskTransformCategory:
 * @GSK_TRANSFORM_CATEGORY_UNKNOWN: The category of the matrix has not been
 *     determined.
 * @GSK_TRANSFORM_CATEGORY_ANY: Analyzing the matrix concluded that it does
 *     not fit in any other category.
 * @GSK_TRANSFORM_CATEGORY_3D: The matrix is a 3D matrix. This means that
 *     the w column (the last column) has the values (0, 0, 0, 1).
 * @GSK_TRANSFORM_CATEGORY_2D: The matrix is a 2D matrix. This is equivalent
 *     to graphene_matrix_is_2d() returning %TRUE. In particular, this
 *     means that Cairo can deal with the matrix.
 * @GSK_TRANSFORM_CATEGORY_2D_AFFINE: The matrix is a combination of 2D scale
 *     and 2D translation operations. In particular, this means that any
 *     rectangle can be transformed exactly using this matrix.
 * @GSK_TRANSFORM_CATEGORY_2D_TRANSLATE: The matrix is a 2D translation.
 * @GSK_TRANSFORM_CATEGORY_IDENTITY: The matrix is the identity matrix.
 *
 * The categories of matrices relevant for GSK and GTK. Note that any
 * category includes matrices of all later categories. So if you want
 * to for example check if a matrix is a 2D matrix,
 * `category >= GSK_TRANSFORM_CATEGORY_2D` is the way to do this.
 *
 * Also keep in mind that rounding errors may cause matrices to not
 * conform to their categories. Otherwise, matrix operations done via
 * mutliplication will not worsen categories. So for the matrix
 * multiplication `C = A * B`, `category(C) = MIN (category(A), category(B))`.
 */
typedef enum
{
  GSK_TRANSFORM_CATEGORY_UNKNOWN,
  GSK_TRANSFORM_CATEGORY_ANY,
  GSK_TRANSFORM_CATEGORY_3D,
  GSK_TRANSFORM_CATEGORY_2D,
  GSK_TRANSFORM_CATEGORY_2D_AFFINE,
  GSK_TRANSFORM_CATEGORY_2D_TRANSLATE,
  GSK_TRANSFORM_CATEGORY_IDENTITY
} GskTransformCategory;

#endif /* __GSK_TYPES_H__ */
