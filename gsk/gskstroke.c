/*
 * Copyright © 2020 Benjamin Otte
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

#include "gskstrokeprivate.h"

/**
 * GskStroke:
 *
 * A `GskStroke` describes the attributes that are used when stroking a path.
 */

G_DEFINE_BOXED_TYPE (GskStroke, gsk_stroke,
                     gsk_stroke_copy,
                     gsk_stroke_free)


/**
 * gsk_stroke_new:
 * @line_width: line width of the stroke. Must be > 0
 *
 * Creates a new `GskStroke` with the given @line_width.
 *
 * Returns: a new `GskStroke`
 **/
GskStroke *
gsk_stroke_new (float line_width)
{
  GskStroke *self;

  g_return_val_if_fail (line_width > 0, NULL);

  self = g_new0 (GskStroke, 1);

  self->line_width = line_width;
  self->line_cap = GSK_LINE_CAP_BUTT;
  self->line_join = GSK_LINE_JOIN_MITER;
  self->miter_limit = 4.f; /* following svg */

  return self;
}

/**
 * gsk_stroke_copy:
 * @other: `GskStroke` to copy
 *
 * Creates a copy of the given @other stroke.
 *
 * Returns: a new `GskStroke`. Use [method@Gsk.Stroke.free] to free it.
 **/
GskStroke *
gsk_stroke_copy (const GskStroke *other)
{
  GskStroke *self;

  g_return_val_if_fail (other != NULL, NULL);

  self = g_new (GskStroke, 1);

  gsk_stroke_init_copy (self, other);

  return self;
}

/**
 * gsk_stroke_free:
 * @self: a `GskStroke`
 *
 * Frees a `GskStroke`.
 **/
void
gsk_stroke_free (GskStroke *self)
{
  if (self == NULL)
    return;

  gsk_stroke_clear (self);

  g_free (self);
}

void
gsk_stroke_to_cairo (const GskStroke *self,
                     cairo_t         *cr)
{
  cairo_set_line_width (cr, self->line_width);

  /* gcc can optimize that to a direct case. This catches later additions to the enum */
  switch (self->line_cap)
    {
      case GSK_LINE_CAP_BUTT:
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
        break;
      case GSK_LINE_CAP_ROUND:
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
        break;
      case GSK_LINE_CAP_SQUARE:
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  /* gcc can optimize that to a direct case. This catches later additions to the enum */
  switch (self->line_join)
    {
      case GSK_LINE_JOIN_MITER:
      case GSK_LINE_JOIN_MITER_CLIP:
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
        break;
      case GSK_LINE_JOIN_ROUND:
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
        break;
      case GSK_LINE_JOIN_BEVEL:
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  cairo_set_miter_limit (cr, self->miter_limit);
}

/**
 * gsk_stroke_equal:
 * @stroke1: the first `GskStroke`
 * @stroke2: the second `GskStroke`
 *
 * Checks if two strokes are identical.
 *
 * Returns: %TRUE if the two strokes are equal, %FALSE otherwise
 **/
gboolean
gsk_stroke_equal (gconstpointer stroke1,
                  gconstpointer stroke2)
{
  const GskStroke *self1 = stroke1;
  const GskStroke *self2 = stroke2;

  if (self1->line_width != self2->line_width ||
      self1->line_cap != self2->line_cap ||
      self1->line_join != self2->line_join ||
      self1->miter_limit != self2->miter_limit)
    return FALSE;

  return TRUE;
}

/**
 * gsk_stroke_set_line_width:
 * @self: a `GskStroke`
 * @line_width: width of the line in pixels
 *
 * Sets the line width to be used when stroking.
 *
 * The line width must be > 0.
 **/
void
gsk_stroke_set_line_width (GskStroke *self,
                           float      line_width)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (line_width > 0);

  self->line_width = line_width;
}

/**
 * gsk_stroke_get_line_width:
 * @self: a `GskStroke`
 *
 * Gets the line width used.
 *
 * Returns: The line width
 **/
float
gsk_stroke_get_line_width (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->line_width;
}

/**
 * gsk_stroke_set_line_cap:
 * @self: a `GskStroke`
 * @line_cap: the `GskLineCap`
 *
 * Sets the line cap to be used when stroking.
 *
 * See `GskLineCap` for details.
 **/
void
gsk_stroke_set_line_cap (GskStroke  *self,
                         GskLineCap  line_cap)
{
  g_return_if_fail (self != NULL);

  self->line_cap = line_cap;
}

/**
 * gsk_stroke_get_line_cap:
 * @self: a `GskStroke`
 *
 * Gets the line cap used.
 *
 * See `GskLineCap` for details.
 *
 * Returns: The line cap
 **/
GskLineCap
gsk_stroke_get_line_cap (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->line_cap;
}

/**
 * gsk_stroke_set_line_join:
 * @self: a `GskStroke`
 * @line_join: The line join to use
 *
 * Sets the line join to be used when stroking.
 *
 * See `GskLineJoin` for details.
 **/
void
gsk_stroke_set_line_join (GskStroke   *self,
                          GskLineJoin  line_join)
{
  g_return_if_fail (self != NULL);

  self->line_join = line_join;
}

/**
 * gsk_stroke_get_line_join:
 * @self: a `GskStroke`
 *
 * Gets the line join used.
 *
 * See `GskLineJoin` for details.
 *
 * Returns: The line join
 **/
GskLineJoin
gsk_stroke_get_line_join (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->line_join;
}

/**
 * gsk_stroke_set_miter_limit:
 * @self: a `GskStroke`
 * @limit: the miter limit, must be non-negative
 *
 * Sets the limit for the distance from the corner where sharp
 * turns of joins get cut off. The miter limit is in units of
 * line width.
 *
 * For joins of type `GSK_LINE_JOIN_MITER` that exceed the miter
 * limit, the join gets rendered as if it was of type
 * `GSK_LINE_JOIN_BEVEL`. For joins of type `GSK_LINE_JOIN_MITER_CLIP`,
 * the miter is clipped at a distance of half the miter limit.
 */
void
gsk_stroke_set_miter_limit (GskStroke  *self,
                            float       limit)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (limit >= 0);

  self->miter_limit = limit;
}

/**
 * gsk_stroke_get_miter_limit:
 * @self: a `GskStroke`
 *
 * Returns the miter limit of a `GskStroke`.
 */
float
gsk_stroke_get_miter_limit (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 4.f);

  return self->miter_limit;
}
