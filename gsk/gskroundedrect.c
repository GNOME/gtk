/* GSK - The GTK Scene Kit
 *
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

/**
 * SECTION:GskRoundedRect
 * @Title: GskRoundedRect
 * @Short_description: A rounded rectangle
 *
 * #GskRoundedRect defines a rectangle with rounded corners, as is commonly
 * used in drawing.
 *
 * Operations on a #GskRoundedRect will normalize the rectangle, to
 * ensure that the bounds are normalized and that the corner sizes don't exceed
 * the size of the rectangle. The algorithm used for normalizing corner sizes
 * is described in [the CSS specification](https://drafts.csswg.org/css-backgrounds-3/#border-radius).
 */

#include "config.h"

#include "gskroundedrect.h"

#include "gskdebugprivate.h"

#include <math.h>

static void
gsk_rounded_rect_normalize_in_place (GskRoundedRect *self)
{
  float factor = 1.0;
  float corners;
  guint i;

  graphene_rect_normalize (&self->bounds);

  for (i = 0; i < 4; i++)
    {
      self->corner[i].width = MAX (self->corner[i].width, 0);
      self->corner[i].height = MAX (self->corner[i].height, 0);
    }

  /* clamp border radius, following CSS specs */
  corners = self->corner[GSK_CORNER_TOP_LEFT].width + self->corner[GSK_CORNER_TOP_RIGHT].width;
  if (corners > self->bounds.size.width)
    factor = MIN (factor, self->bounds.size.width / corners);

  corners = self->corner[GSK_CORNER_TOP_RIGHT].height + self->corner[GSK_CORNER_BOTTOM_RIGHT].height;
  if (corners > self->bounds.size.height)
    factor = MIN (factor, self->bounds.size.height / corners);

  corners = self->corner[GSK_CORNER_BOTTOM_RIGHT].width + self->corner[GSK_CORNER_BOTTOM_LEFT].width;
  if (corners > self->bounds.size.width)
    factor = MIN (factor, self->bounds.size.width / corners);

  corners = self->corner[GSK_CORNER_TOP_LEFT].height + self->corner[GSK_CORNER_BOTTOM_LEFT].height;
  if (corners > self->bounds.size.height)
    factor = MIN (factor, self->bounds.size.height / corners);

  for (i = 0; i < 4; i++)
    graphene_size_scale (&self->corner[i], factor, &self->corner[i]);
}

/**
 * gsk_rounded_rect_init:
 * @self: The #GskRoundedRect to initialize
 * @bounds: a #graphene_rect_t describing the bounds
 * @top_left: the rounding radius of the top left corner
 * @top_right: the rounding radius of the top right corner
 * @bottom_right: the rounding radius of the bottom right corner
 * @bottom_left: the rounding radius of the bottom left corner
 *
 * Initializes the given #GskRoundedRect with the given values.
 *
 * This function will implicitly normalize the #GskRoundedRect
 * before returning.
 *
 * Returns: (transfer none): the initialized rectangle
 *
 * Since: 3.90
 */
GskRoundedRect *
gsk_rounded_rect_init (GskRoundedRect        *self,
                       const graphene_rect_t *bounds,
                       const graphene_size_t *top_left,
                       const graphene_size_t *top_right,
                       const graphene_size_t *bottom_right,
                       const graphene_size_t *bottom_left)
{
  graphene_rect_init_from_rect (&self->bounds, bounds);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_TOP_LEFT], top_left);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_TOP_RIGHT], top_right);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_BOTTOM_RIGHT], bottom_right);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_BOTTOM_LEFT], bottom_left);

  gsk_rounded_rect_normalize_in_place (self);

  return self;
}

/**
 * gsk_rounded_rect_init_copy:
 * @self: a #GskRoundedRect
 * @src: a #GskRoundedRect
 *
 * Initializes @self using the given @src rectangle.
 *
 * This function will implicitly normalize the #GskRoundedRect
 * before returning.
 *
 * Returns: (transfer none): the initialized rectangle
 *
 * Since: 3.90
 */
GskRoundedRect *
gsk_rounded_rect_init_copy (GskRoundedRect       *self,
                            const GskRoundedRect *src)
{
  *self = *src;

  gsk_rounded_rect_normalize_in_place (self);

  return self;
}

/**
 * gsk_rounded_rect_init_from_rect:
 * @self: a #GskRoundedRect
 * @bounds: a #graphene_rect_t
 * @radius: the border radius
 *
 * Initializes @self to the given @bounds and sets the radius of all
 * four corners to @radius.
 *
 * Returns: (transfer none): the initialized rectangle
 **/
GskRoundedRect *
gsk_rounded_rect_init_from_rect (GskRoundedRect        *self,
                                 const graphene_rect_t *bounds,
                                 float                  radius)
{
  graphene_size_t corner = GRAPHENE_SIZE_INIT(radius, radius);

  return gsk_rounded_rect_init (self, bounds, &corner, &corner, &corner, &corner);
}

/**
 * gsk_rounded_rect_normalize:
 * @self: a #GskRoundedRect
 *
 * Normalizes the passed rectangle.
 *
 * this function will ensure that the bounds of the rectanlge are normalized
 * and ensure that the corner values are positive and the corners do not overlap.
 *
 * Returns: (transfer none): the normalized rectangle
 *
 * Since: 3.90
 */
GskRoundedRect *
gsk_rounded_rect_normalize (GskRoundedRect *self)
{
  gsk_rounded_rect_normalize_in_place (self);

  return self;
}

/**
 * gsk_rounded_rect_offset:
 * @self: a #GskRoundedRect
 * @d_x: the horizontal offset
 * @d_y: the vertical offset
 *
 * Offsets the bound's origin by @dx and @dy.
 *
 * The size and corners of the rectangle are unchanged.
 *
 * Returns: (transfer none): the offset rectangle
 *
 * Since: 3.90
 */
GskRoundedRect *
gsk_rounded_rect_offset (GskRoundedRect *self,
                         float           dx,
                         float           dy)
{
  gsk_rounded_rect_normalize (self);

  self->bounds.origin.x += dx;
  self->bounds.origin.y += dy;

  return self;
}

/**
 * gsk_rounded_rect_is_rectilinear:
 * @self: the #GskRoundedRect to check
 *
 * Checks if all corners of @self are right angles and the
 * rectangle covers all of its bounds.
 *
 * This information can be used to decide if gsk_clip_node_new()
 * or gsk_rounded_clip_node_new() should be called.
 *
 * Returns: %TRUE if the rectangle is rectilinear
 **/
gboolean
gsk_rounded_rect_is_rectilinear (GskRoundedRect *self)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (self->corner[i].width > 0 ||
          self->corner[i].height > 0)
        return FALSE;
    }

  return TRUE;
}

static void
append_arc (cairo_t *cr, double angle1, double angle2, gboolean negative)
{
  if (negative)
    cairo_arc_negative (cr, 0.0, 0.0, 1.0, angle1, angle2);
  else
    cairo_arc (cr, 0.0, 0.0, 1.0, angle1, angle2);
}

static void
_cairo_ellipsis (cairo_t *cr,
	         double xc, double yc,
	         double xradius, double yradius,
	         double angle1, double angle2)
{
  cairo_matrix_t save;

  if (xradius <= 0.0 || yradius <= 0.0)
    {
      cairo_line_to (cr, xc, yc);
      return;
    }

  cairo_get_matrix (cr, &save);
  cairo_translate (cr, xc, yc);
  cairo_scale (cr, xradius, yradius);
  append_arc (cr, angle1, angle2, FALSE);
  cairo_set_matrix (cr, &save);
}

void
gsk_rounded_rect_path (const GskRoundedRect *self,
                       cairo_t              *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width,
                   self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height,
                   self->corner[GSK_CORNER_TOP_LEFT].width,
                   self->corner[GSK_CORNER_TOP_LEFT].height,
                   G_PI, 3 * G_PI_2);
  _cairo_ellipsis (cr, 
                   self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_TOP_RIGHT].width,
                   self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height,
                   self->corner[GSK_CORNER_TOP_RIGHT].width,
                   self->corner[GSK_CORNER_TOP_RIGHT].height,
                   - G_PI_2, 0);
  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   self->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   self->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   0, G_PI_2);
  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   self->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   self->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   G_PI_2, G_PI);

  cairo_close_path (cr);
}

