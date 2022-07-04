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
                     const GtkCanvasVector *point,
                     const GtkCanvasVector *size,
                     const GtkCanvasVector *origin)
{
  gtk_canvas_vector_init_copy (&self->point, point);
  gtk_canvas_vector_init_copy (&self->size, size);
  gtk_canvas_vector_init_copy (&self->origin, origin);
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
  gtk_canvas_vector_finish (&self->point);
  gtk_canvas_vector_finish (&self->size);
  gtk_canvas_vector_finish (&self->origin);
}

void
gtk_canvas_box_init_variable (GtkCanvasBox *self)
{
  gtk_canvas_vector_init_variable (&self->point);
  gtk_canvas_vector_init_variable (&self->size);
  gtk_canvas_vector_init_variable (&self->origin);
}

void
gtk_canvas_box_update_variable (GtkCanvasBox       *self,
                                const GtkCanvasBox *other)
{
  gtk_canvas_vector_init_copy (gtk_canvas_vector_get_variable (&self->point), &other->point);
  gtk_canvas_vector_init_copy (gtk_canvas_vector_get_variable (&self->size), &other->size);
  gtk_canvas_vector_init_copy (gtk_canvas_vector_get_variable (&self->origin), &other->origin);
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
gtk_canvas_box_new_points (const GtkCanvasVector *point1,
                           const GtkCanvasVector *point2)
{
  GtkCanvasVector size, origin;
  GtkCanvasBox *result;
  graphene_vec2_t minus_one;

  g_return_val_if_fail (point1 != NULL, NULL);
  g_return_val_if_fail (point2 != NULL, NULL);

  graphene_vec2_init (&minus_one, -1.f, -1.f);
  gtk_canvas_vector_init_sum (&size,
                              graphene_vec2_one (),
                              point2,
                              &minus_one,
                              point1,
                              NULL);
  gtk_canvas_vector_init_constant (&origin, 0, 0);
  result = g_slice_new (GtkCanvasBox);
  gtk_canvas_box_init (result, (GtkCanvasVector *) point1, &size, &origin);
  gtk_canvas_vector_finish (&size);
  gtk_canvas_vector_finish (&origin);

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
gtk_canvas_box_new (const GtkCanvasVector *point,
                    const GtkCanvasVector *size,
                    float                  origin_x,
                    float                  origin_y)
{
  GtkCanvasBox *self;
  GtkCanvasVector origin;

  g_return_val_if_fail (point != NULL, NULL);
  g_return_val_if_fail (size != NULL, NULL);

  gtk_canvas_vector_init_constant (&origin, origin_x, origin_y);

  self = g_slice_new (GtkCanvasBox);
  gtk_canvas_box_init (self, point, size, &origin);

  gtk_canvas_vector_finish (&origin);

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
  graphene_vec2_t point, size, origin;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  if (!gtk_canvas_vector_eval (&self->point, &point) ||
      !gtk_canvas_vector_eval (&self->size, &size) ||
      !gtk_canvas_vector_eval (&self->origin, &origin))
    {
      *rect = *graphene_rect_zero ();
      return FALSE;
    }

  graphene_vec2_multiply (&origin, &size, &origin);
  graphene_vec2_subtract (&point, &origin, &point);

  *rect = GRAPHENE_RECT_INIT (graphene_vec2_get_x (&point),
                              graphene_vec2_get_y (&point),
                              graphene_vec2_get_x (&size),
                              graphene_vec2_get_y (&size));

  return TRUE;
}

const GtkCanvasVector *
gtk_canvas_box_get_point (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->point;
}

const GtkCanvasVector *
gtk_canvas_box_get_size (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->size;
}

const GtkCanvasVector *
gtk_canvas_box_get_origin (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->origin;
}
