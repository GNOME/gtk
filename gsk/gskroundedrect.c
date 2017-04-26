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
#include "gskroundedrectprivate.h"

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
 * This function will not normalize the #GskRoundedRect, so
 * make sure the source is normalized.
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
 * @dx: the horizontal offset
 * @dy: the vertical offset
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

static void
border_radius_shrink (graphene_size_t *corner,
                      double           width,
                      double           height)
{
  if (corner->width > 0)
    corner->width -= width;
  if (corner->height > 0)
    corner->height -= height;

  if (corner->width <= 0 || corner->height <= 0)
    {
      corner->width = 0;
      corner->height = 0;
    }
}

/**
 * gsk_rounded_rect_shrink:
 * @self: The #GskRoundedRect to shrink or grow
 * @top: How far to move the top side downwards
 * @right: How far to move the right side to the left
 * @bottom: How far to move the bottom side upwards
 * @left: How far to move the left side to the right
 *
 * Shrinks (or grows) the given rectangle by moving the 4 sides
 * according to the offsets given. The corner radii will be changed
 * in a way that tries to keep the center of the corner circle intact.
 * This emulates CSS behavior.
 *
 * This function also works for growing rectangles if you pass
 * negative values for the @top, @right, @bottom or @left.
 *
 * Returns: (transfer none): the resized #GskRoundedRect
 **/
GskRoundedRect *
gsk_rounded_rect_shrink (GskRoundedRect *self,
                         float           top,
                         float           right,
                         float           bottom,
                         float           left)
{
  if (self->bounds.size.width - left - right < 0)
    {
      self->bounds.origin.x += left * self->bounds.size.width / (left + right);
      self->bounds.size.width = 0;
    }
  else
    {
      self->bounds.origin.x += left;
      self->bounds.size.width -= left + right;
    }

  if (self->bounds.size.height - bottom - top < 0)
    {
      self->bounds.origin.y += top * self->bounds.size.height / (top + bottom);
      self->bounds.size.height = 0;
    }
  else
    {
      self->bounds.origin.y += top;
      self->bounds.size.height -= top + bottom;
    }

  border_radius_shrink (&self->corner[GSK_CORNER_TOP_LEFT], left, top);
  border_radius_shrink (&self->corner[GSK_CORNER_TOP_RIGHT], right, top);
  border_radius_shrink (&self->corner[GSK_CORNER_BOTTOM_RIGHT], right, bottom);
  border_radius_shrink (&self->corner[GSK_CORNER_BOTTOM_LEFT], left, bottom);

  return self;
}

/* XXX: Find a better name */
gboolean
gsk_rounded_rect_is_circular (const GskRoundedRect *self)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (self->corner[i].width != self->corner[i].height)
        return FALSE;
    }

  return TRUE;
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
gsk_rounded_rect_is_rectilinear (const GskRoundedRect *self)
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

static gboolean
ellipsis_contains_point (const graphene_size_t *ellipsis,
                         const graphene_point_t *point)
{
  return (point->x * point->x) / (ellipsis->width * ellipsis->width)
       + (point->y * point->y) / (ellipsis->height * ellipsis->height) <= 1;
}

/**
 * gsk_rounded_rect_contains_point:
 * @self: a #GskRoundedRect
 * @point: the point to check
 *
 * Checks if the given @point is inside the rounded rectangle. This function
 * returns %FALSE if the point is in the rounded corner areas.
 *
 * Returns: %TRUE if the @point is inside the rounded rectangle
 **/
gboolean
gsk_rounded_rect_contains_point (const GskRoundedRect   *self,
                                 const graphene_point_t *point)
{
  if (!graphene_rect_contains_point (&self->bounds, point))
    return FALSE;

  if (self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width > point->x &&
      self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height > point->y &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_LEFT],
                                &GRAPHENE_POINT_INIT (
                                    self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width - point->x,
                                    self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height- point->y
                                )))
    return FALSE;

  if (self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_TOP_RIGHT].width < point->x &&
      self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height > point->y &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_RIGHT],
                                &GRAPHENE_POINT_INIT (
                                    self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_TOP_RIGHT].width - point->x,
                                    self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height- point->y
                                )))
    return FALSE;

  if (self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width > point->x &&
      self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_LEFT].height > point->y &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_LEFT],
                                &GRAPHENE_POINT_INIT (
                                    self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width - point->x,
                                    self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_LEFT].height- point->y
                                )))
    return FALSE;

  if (self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_BOTTOM_RIGHT].width < point->x &&
      self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_RIGHT].height > point->y &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_RIGHT],
                                &GRAPHENE_POINT_INIT (
                                    self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_BOTTOM_RIGHT].width - point->x,
                                    self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_RIGHT].height- point->y
                                )))
    return FALSE;

  return TRUE;
}

/**
 * gsk_rounded_rect_contains_rect:
 * @self: a #GskRoundedRect
 * @rect: the rectangle to check
 *
 * Checks if the given @rect is contained inside the rounded rectangle.
 * This function returns %FALSE if @rect extends into one of the rounded
 * corner areas.
 *
 * Returns: %TRUE if the @rect is fully contained inside the rounded rectangle
 **/
gboolean
gsk_rounded_rect_contains_rect (const GskRoundedRect  *self,
                                const graphene_rect_t *rect)
{
  if (!graphene_rect_contains_rect (&self->bounds, rect))
    return FALSE;

  if (!gsk_rounded_rect_contains_point (self, &rect->origin) ||
      !gsk_rounded_rect_contains_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y)) ||
      !gsk_rounded_rect_contains_point (self, &GRAPHENE_POINT_INIT (rect->origin.x, rect->origin.y + rect->size.height)) ||
      !gsk_rounded_rect_contains_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y + rect->size.height)))
    return FALSE;

  return TRUE;
}

/**
 * gsk_rounded_rect_intersects_rect:
 * @self: a #GskRoundedRect
 * @rect: the rectangle to check
 *
 * Checks if part of the given @rect is contained inside the rounded rectangle.
 * This function returns %FALSE if @rect only extends into one of the rounded
 * corner areas but not into the rounded rectangle itself.
 *
 * Returns: %TRUE if the @rect intersects with the rounded rectangle
 **/
gboolean
gsk_rounded_rect_intersects_rect (const GskRoundedRect  *self,
                                  const graphene_rect_t *rect)
{
  if (!graphene_rect_intersection (&self->bounds, rect, NULL))
    return FALSE;

  if (!gsk_rounded_rect_contains_point (self, &rect->origin) &&
      !gsk_rounded_rect_contains_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y)) &&
      !gsk_rounded_rect_contains_point (self, &GRAPHENE_POINT_INIT (rect->origin.x, rect->origin.y + rect->size.height)) &&
      !gsk_rounded_rect_contains_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y + rect->size.height)))
    return FALSE;

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

/*
 * Converts to the format we use in our shaders:
 * vec4 rect;
 * vec4 corner_widths;
 * vec4 corner_heights;
 * rect is (x, y, width, height), the corners are the same
 * order as in the rounded rect.
 *
 * This is so that shaders can use just the first vec4 for
 * rectilinear rects, the 2nd vec4 for circular rects and
 * only look at the last vec4 if they have to.
 */
void
gsk_rounded_rect_to_float (const GskRoundedRect *self,
                           float                 rect[12])
{
  guint i;

  rect[0] = self->bounds.origin.x;
  rect[1] = self->bounds.origin.y;
  rect[2] = self->bounds.size.width;
  rect[3] = self->bounds.size.height;

  for (i = 0; i < 4; i++)
    {
      rect[4 + i] = self->corner[i].width;
      rect[8 + i] = self->corner[i].height;
    }
}

