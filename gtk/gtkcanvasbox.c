/*
 * Copyright © 2022 Benjamin Otte
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


/**
 * GtkCanvasBox:
 *
 * `GtkCanvasBox` describes an axis-aligned rectangular box inside
 * a `GtkCanvas`.
 *
 * A box can have no size and be just a single point.
 */

#include "config.h"

#include "gtkcanvasboxprivate.h"

/* {{{ Boilerplate */

G_DEFINE_BOXED_TYPE (GtkCanvasBox, gtk_canvas_box,
                     gtk_canvas_box_copy,
                     gtk_canvas_box_free)

void
gtk_canvas_box_init (GtkCanvasBox          *self,
                     const GtkCanvasVec2   *point,
                     const GtkCanvasVec2   *size,
                     const graphene_vec2_t *origin)
{
  gtk_canvas_vec2_init_copy (&self->point, point);
  gtk_canvas_vec2_init_copy (&self->size, size);
  graphene_vec2_init_from_vec2 (&self->origin, origin);
}

void
gtk_canvas_box_init_copy (GtkCanvasBox       *self,
                          const GtkCanvasBox *source)
{
  gtk_canvas_box_init (self, &source->point, &source->size, &source->origin);
}

void
gtk_canvas_box_finish (GtkCanvasBox *self)
{
  gtk_canvas_vec2_finish (&self->point);
  gtk_canvas_vec2_finish (&self->size);
}

void
gtk_canvas_box_init_variable (GtkCanvasBox *self)
{
  gtk_canvas_vec2_init_variable (&self->point);
  gtk_canvas_vec2_init_variable (&self->size);
  graphene_vec2_init (&self->origin, 0, 0);
}

void
gtk_canvas_box_update_variable (GtkCanvasBox       *self,
                                const GtkCanvasBox *other)
{
  gtk_canvas_vec2_init_copy (gtk_canvas_vec2_get_variable (&self->point), &other->point);
  gtk_canvas_vec2_init_copy (gtk_canvas_vec2_get_variable (&self->size), &other->size);
  graphene_vec2_init_from_vec2 (&self->origin, &other->origin);
}

/**
 * gtk_canvas_box_new_points:
 * @point1: the first point
 * @point2: the second point
 *
 * Creates a new box describing the rectangle between the two
 * points
 *
 * Returns: a new box
 **/
GtkCanvasBox *
gtk_canvas_box_new_points (const GtkCanvasPoint *point1,
                           const GtkCanvasPoint *point2)
{
  GtkCanvasVec2 size;
  GtkCanvasBox *result;
  graphene_vec2_t minus_one;

  g_return_val_if_fail (point1 != NULL, NULL);
  g_return_val_if_fail (point2 != NULL, NULL);

  graphene_vec2_init (&minus_one, -1.f, -1.f);
  gtk_canvas_vec2_init_sum (&size,
                            graphene_vec2_one (),
                            point2,
                            &minus_one,
                            point1,
                            NULL);
  result = g_slice_new (GtkCanvasBox);
  gtk_canvas_box_init (result, (GtkCanvasVec2 *) point1, &size, graphene_vec2_zero ());
  gtk_canvas_vec2_finish (&size);

  return result;
}

/**
 * gtk_canvas_box_new:
 * @point: the origin point of the box
 * @size: size of the box
 * @origin_x: x coordinate of origin
 * @origin_y: y coordinate of origin
 *
 * Creates a new box of the given size relative to the given point.
 * The origin describes where in the box the point is located.
 * (0, 0) means the point describes the top left of the box, (1, 1)
 * describes the bottom right, and (0.5, 0.5) is the center.
 *
 * Returns: a new box
 **/
GtkCanvasBox *
gtk_canvas_box_new (const GtkCanvasPoint *point,
                    const GtkCanvasSize  *size,
                    float                 origin_x,
                    float                 origin_y)
{
  GtkCanvasBox *self;
  graphene_vec2_t origin;

  g_return_val_if_fail (point != NULL, NULL);
  g_return_val_if_fail (size != NULL, NULL);

  graphene_vec2_init (&origin, origin_x, origin_y);

  self = g_slice_new (GtkCanvasBox);
  gtk_canvas_box_init (self,
                       (GtkCanvasVec2 *) point,
                       (GtkCanvasVec2 *) size,
                       &origin);

  return self;
}

GtkCanvasBox *
gtk_canvas_box_copy (const GtkCanvasBox *self)
{
  GtkCanvasBox *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = g_slice_new (GtkCanvasBox);
  gtk_canvas_box_init_copy (copy, self);

  return copy;
}

void
gtk_canvas_box_free (GtkCanvasBox *self)
{
  gtk_canvas_box_finish (self);

  g_slice_free (GtkCanvasBox, self);
}

gboolean
gtk_canvas_box_eval (const GtkCanvasBox *self,
                     graphene_rect_t    *rect)
{
  graphene_vec2_t point, size, tmp;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  if (!gtk_canvas_vec2_eval (&self->point, &point) ||
      !gtk_canvas_vec2_eval (&self->size, &size))
    {
      *rect = *graphene_rect_zero ();
      return FALSE;
    }

  graphene_vec2_multiply (&self->origin, &size, &tmp);
  graphene_vec2_subtract (&point, &tmp, &point);

  *rect = GRAPHENE_RECT_INIT (graphene_vec2_get_x (&point),
                              graphene_vec2_get_y (&point),
                              graphene_vec2_get_x (&size),
                              graphene_vec2_get_y (&size));

  return TRUE;
}

const GtkCanvasPoint *
gtk_canvas_box_get_point (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return (GtkCanvasPoint *) &self->point;
}

const GtkCanvasSize *
gtk_canvas_box_get_size (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return (GtkCanvasSize *) &self->size;
}

void
gtk_canvas_box_get_origin (const GtkCanvasBox *self,
                           float              *x,
                           float              *y)
{
  g_return_if_fail (self != NULL);

  if (x)
    *x = graphene_vec2_get_x (&self->origin);
  if (y)
    *y = graphene_vec2_get_y (&self->origin);
}
