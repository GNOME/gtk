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
 * GskRoundedRect:
 * @bounds: the bounds of the rectangle
 * @corner: the size of the 4 rounded corners
 *
 * A rectangular region with rounded corners.
 *
 * Application code should normalize rectangles using
 * [method@Gsk.RoundedRect.normalize]; this function will ensure that
 * the bounds of the rectangle are normalized and ensure that the corner
 * values are positive and the corners do not overlap.
 *
 * All functions taking a `GskRoundedRect` as an argument will internally
 * operate on a normalized copy; all functions returning a `GskRoundedRect`
 * will always return a normalized one.
 *
 * The algorithm used for normalizing corner sizes is described in
 * [the CSS specification](https://drafts.csswg.org/css-backgrounds-3/#border-radius).
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
 * @self: The `GskRoundedRect` to initialize
 * @bounds: a `graphene_rect_t` describing the bounds
 * @top_left: the rounding radius of the top left corner
 * @top_right: the rounding radius of the top right corner
 * @bottom_right: the rounding radius of the bottom right corner
 * @bottom_left: the rounding radius of the bottom left corner
 *
 * Initializes the given `GskRoundedRect` with the given values.
 *
 * This function will implicitly normalize the `GskRoundedRect`
 * before returning.
 *
 * Returns: (transfer none): the initialized rectangle
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
 * @self: a `GskRoundedRect`
 * @src: a `GskRoundedRect`
 *
 * Initializes @self using the given @src rectangle.
 *
 * This function will not normalize the `GskRoundedRect`,
 * so make sure the source is normalized.
 *
 * Returns: (transfer none): the initialized rectangle
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
 * @self: a `GskRoundedRect`
 * @bounds: a `graphene_rect_t`
 * @radius: the border radius
 *
 * Initializes @self to the given @bounds and sets the radius
 * of all four corners to @radius.
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
 * @self: a `GskRoundedRect`
 *
 * Normalizes the passed rectangle.
 *
 * This function will ensure that the bounds of the rectangle
 * are normalized and ensure that the corner values are positive
 * and the corners do not overlap.
 *
 * Returns: (transfer none): the normalized rectangle
 */
GskRoundedRect *
gsk_rounded_rect_normalize (GskRoundedRect *self)
{
  gsk_rounded_rect_normalize_in_place (self);

  return self;
}

/**
 * gsk_rounded_rect_offset:
 * @self: a `GskRoundedRect`
 * @dx: the horizontal offset
 * @dy: the vertical offset
 *
 * Offsets the bound's origin by @dx and @dy.
 *
 * The size and corners of the rectangle are unchanged.
 *
 * Returns: (transfer none): the offset rectangle
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

static inline void
border_radius_shrink (graphene_size_t       *corner,
                      double                 width,
                      double                 height,
                      const graphene_size_t *max)
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
  else
    {
      corner->width = MIN (corner->width, max->width);
      corner->height = MIN (corner->height, max->height);
    }
}

/**
 * gsk_rounded_rect_shrink:
 * @self: The `GskRoundedRect` to shrink or grow
 * @top: How far to move the top side downwards
 * @right: How far to move the right side to the left
 * @bottom: How far to move the bottom side upwards
 * @left: How far to move the left side to the right
 *
 * Shrinks (or grows) the given rectangle by moving the 4 sides
 * according to the offsets given.
 *
 * The corner radii will be changed in a way that tries to keep
 * the center of the corner circle intact. This emulates CSS behavior.
 *
 * This function also works for growing rectangles if you pass
 * negative values for the @top, @right, @bottom or @left.
 *
 * Returns: (transfer none): the resized `GskRoundedRect`
 **/
GskRoundedRect *
gsk_rounded_rect_shrink (GskRoundedRect *self,
                         float           top,
                         float           right,
                         float           bottom,
                         float           left)
{
  float width = left + right;
  float height = top + bottom;

  if (self->bounds.size.width - width < 0)
    {
      self->bounds.origin.x += left * self->bounds.size.width / width;
      self->bounds.size.width = 0;
    }
  else
    {
      self->bounds.origin.x += left;
      self->bounds.size.width -= width;
    }

  if (self->bounds.size.height - height < 0)
    {
      self->bounds.origin.y += top * self->bounds.size.height / height;
      self->bounds.size.height = 0;
    }
  else
    {
      self->bounds.origin.y += top;
      self->bounds.size.height -= height;
    }

  border_radius_shrink (&self->corner[GSK_CORNER_TOP_LEFT], left, top, &self->bounds.size);
  border_radius_shrink (&self->corner[GSK_CORNER_TOP_RIGHT], right, top, &self->bounds.size);
  border_radius_shrink (&self->corner[GSK_CORNER_BOTTOM_RIGHT], right, bottom, &self->bounds.size);
  border_radius_shrink (&self->corner[GSK_CORNER_BOTTOM_LEFT], left, bottom, &self->bounds.size);

  return self;
}

void
gsk_rounded_rect_scale_affine (GskRoundedRect       *dest,
                               const GskRoundedRect *src,
                               float                 scale_x,
                               float                 scale_y,
                               float                 dx,
                               float                 dy)
{
  guint flip = ((scale_x < 0) ? 1 : 0) + (scale_y < 0 ? 2 : 0);

  g_assert (dest != src);

  graphene_rect_scale (&src->bounds, scale_x, scale_y, &dest->bounds);
  graphene_rect_offset (&dest->bounds, dx, dy);

  scale_x = fabs (scale_x);
  scale_y = fabs (scale_y);

  for (guint i = 0; i < 4; i++)
    {
      dest->corner[i].width = src->corner[i ^ flip].width * scale_x;
      dest->corner[i].height = src->corner[i ^ flip].height * scale_y;
    }
}

/* XXX: Find a better name */
gboolean
gsk_rounded_rect_is_circular (const GskRoundedRect *self)
{
  for (guint i = 0; i < 4; i++)
    {
      if (self->corner[i].width != self->corner[i].height)
        return FALSE;
    }

  return TRUE;
}

/**
 * gsk_rounded_rect_is_rectilinear:
 * @self: the `GskRoundedRect` to check
 *
 * Checks if all corners of @self are right angles and the
 * rectangle covers all of its bounds.
 *
 * This information can be used to decide if [ctor@Gsk.ClipNode.new]
 * or [ctor@Gsk.RoundedClipNode.new] should be called.
 *
 * Returns: %TRUE if the rectangle is rectilinear
 **/
gboolean
gsk_rounded_rect_is_rectilinear (const GskRoundedRect *self)
{
  for (guint i = 0; i < 4; i++)
    {
      if (self->corner[i].width > 0 ||
          self->corner[i].height > 0)
        return FALSE;
    }

  return TRUE;
}

static inline gboolean
ellipsis_contains_point (const graphene_size_t  *ellipsis,
                         const graphene_point_t *point)
{
  return (point->x * point->x) / (ellipsis->width * ellipsis->width)
       + (point->y * point->y) / (ellipsis->height * ellipsis->height) <= 1;
}

typedef enum
{
  INSIDE,
  OUTSIDE_TOP_LEFT,
  OUTSIDE_TOP_RIGHT,
  OUTSIDE_BOTTOM_LEFT,
  OUTSIDE_BOTTOM_RIGHT,
  OUTSIDE
} Location;

static Location
gsk_rounded_rect_locate_point (const GskRoundedRect   *self,
                               const graphene_point_t *point)
{
  float px, py;
  float ox, oy;

  ox = self->bounds.origin.x + self->bounds.size.width;
  oy = self->bounds.origin.y + self->bounds.size.height;

  if (point->x < self->bounds.origin.x ||
      point->y < self->bounds.origin.y ||
      point->x > ox ||
      point->y > oy)
    return OUTSIDE;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width - point->x;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height - point->y;
  if (px > 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_LEFT], &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_TOP_LEFT;

  px = ox - self->corner[GSK_CORNER_TOP_RIGHT].width - point->x;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height - point->y;
  if (px < 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_RIGHT], &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_TOP_RIGHT;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width - point->x;
  py = oy - self->corner[GSK_CORNER_BOTTOM_LEFT].height - point->y;
  if (px > 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_LEFT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_BOTTOM_LEFT;

  px = ox - self->corner[GSK_CORNER_BOTTOM_RIGHT].width - point->x;
  py = oy - self->corner[GSK_CORNER_BOTTOM_RIGHT].height - point->y;
  if (px < 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_RIGHT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_BOTTOM_RIGHT;

  return INSIDE;
}

/**
 * gsk_rounded_rect_contains_point:
 * @self: a `GskRoundedRect`
 * @point: the point to check
 *
 * Checks if the given @point is inside the rounded rectangle.
 *
 * Returns: %TRUE if the @point is inside the rounded rectangle
 **/
gboolean
gsk_rounded_rect_contains_point (const GskRoundedRect   *self,
                                 const graphene_point_t *point)
{
  return gsk_rounded_rect_locate_point (self, point) == INSIDE;
}

/**
 * gsk_rounded_rect_contains_rect:
 * @self: a `GskRoundedRect`
 * @rect: the rectangle to check
 *
 * Checks if the given @rect is contained inside the rounded rectangle.
 *
 * Returns: %TRUE if the @rect is fully contained inside the rounded rectangle
 **/
gboolean
gsk_rounded_rect_contains_rect (const GskRoundedRect  *self,
                                const graphene_rect_t *rect)
{
  float tx, ty;
  float px, py;
  float ox, oy;

  tx = rect->origin.x + rect->size.width;
  ty = rect->origin.y + rect->size.height;
  ox = self->bounds.origin.x + self->bounds.size.width;
  oy = self->bounds.origin.y + self->bounds.size.height;

  if (rect->origin.x < self->bounds.origin.x ||
      rect->origin.y < self->bounds.origin.y ||
      tx > ox ||
      ty > oy)
    return FALSE;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width - rect->origin.x;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height - rect->origin.y;
  if (px > 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_LEFT], &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  px = ox - self->corner[GSK_CORNER_TOP_RIGHT].width - tx;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height - rect->origin.y;
  if (px < 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_RIGHT], &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width - rect->origin.x;
  py = oy - self->corner[GSK_CORNER_BOTTOM_LEFT].height - ty;
  if (px > 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_LEFT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  px = ox - self->corner[GSK_CORNER_BOTTOM_RIGHT].width - tx;
  py = oy - self->corner[GSK_CORNER_BOTTOM_RIGHT].height - ty;
  if (px < 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_RIGHT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  return TRUE;
}

/**
 * gsk_rounded_rect_intersects_rect:
 * @self: a `GskRoundedRect`
 * @rect: the rectangle to check
 *
 * Checks if part of the given @rect is contained inside the rounded rectangle.
 *
 * Returns: %TRUE if the @rect intersects with the rounded rectangle
 */
gboolean
gsk_rounded_rect_intersects_rect (const GskRoundedRect  *self,
                                  const graphene_rect_t *rect)
{
  if (!graphene_rect_intersection (&self->bounds, rect, NULL))
    return FALSE;

  /* If the bounding boxes intersect but the rectangles don't,
   * one of the rect's corners must be in the opposite corner's
   * outside region
   */
  if (gsk_rounded_rect_locate_point (self, &rect->origin) == OUTSIDE_BOTTOM_RIGHT ||
      gsk_rounded_rect_locate_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y)) == OUTSIDE_BOTTOM_LEFT ||
      gsk_rounded_rect_locate_point (self, &GRAPHENE_POINT_INIT (rect->origin.x, rect->origin.y + rect->size.height)) == OUTSIDE_TOP_RIGHT ||
      gsk_rounded_rect_locate_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y + rect->size.height)) == OUTSIDE_TOP_LEFT)
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

/*< private >
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

gboolean
gsk_rounded_rect_equal (gconstpointer rect1,
                        gconstpointer rect2)
{
  const GskRoundedRect *self1 = rect1;
  const GskRoundedRect *self2 = rect2;

  return graphene_rect_equal (&self1->bounds, &self2->bounds)
      && graphene_size_equal (&self1->corner[0], &self2->corner[0])
      && graphene_size_equal (&self1->corner[1], &self2->corner[1])
      && graphene_size_equal (&self1->corner[2], &self2->corner[2])
      && graphene_size_equal (&self1->corner[3], &self2->corner[3]);
}

char *
gsk_rounded_rect_to_string (const GskRoundedRect *self)
{
  return g_strdup_printf ("GskRoundedRect %p: Bounds: (%f, %f, %f, %f)"
                          " Corners: (%f, %f) (%f, %f) (%f, %f) (%f, %f)",
                          self,
                          self->bounds.origin.x,
                          self->bounds.origin.y,
                          self->bounds.size.width,
                          self->bounds.size.height,
                          self->corner[0].width,
                          self->corner[0].height,
                          self->corner[1].width,
                          self->corner[1].height,
                          self->corner[2].width,
                          self->corner[2].height,
                          self->corner[3].width,
                          self->corner[3].height);

}
