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
 * GtkCanvasVector:
 *
 * `GtkCanvasVector` describes a point, size or scale in the coordinate
 * system of a `GtkCanvas`.
 *
 * Vectors are automatically-updating expressions that can track other vectors
 * in the canvas, and constructing the vectors to place `GtkCanvasItem`s on the
 * canvas is the main thing about `GtkCanvas`.
 */

#include "config.h"

#include "gtkcanvasvectorprivate.h"

#include "gtkcanvasboxprivate.h"

G_DEFINE_BOXED_TYPE (GtkCanvasVector, gtk_canvas_vector,
                     gtk_canvas_vector_copy,
                     gtk_canvas_vector_free)

static GtkCanvasVector *
gtk_canvas_vector_alloc (void)
{
  return g_slice_new (GtkCanvasVector);
}

/**
 * gtk_canvas_vector_new:
 * @x: x coordinate
 * @y: y coordinate
 *
 * Creates a new vector at the given coordinate.
 *
 * Returns: a new vector
 **/
GtkCanvasVector *
gtk_canvas_vector_new (float x,
                       float y)
{
  GtkCanvasVector *self;

  self = gtk_canvas_vector_alloc ();
  gtk_canvas_vector_init_constant (self, x, y);

  return self;
}

/**
 * gtk_canvas_vector_new_from_box:
 * @box: a box
 * @origin_x: x coordinate of box origin
 * @origin_y: y coordinate of box origin
 *
 * Creates a vector relative to the given box.
 *
 * The origin describes where in the box the vector is, with
 * (0, 0) being the top left and (1, 1) being the bottom right
 * corner of the box.
 *
 * Returns: a new vector
 **/
GtkCanvasVector *
gtk_canvas_vector_new_from_box (const GtkCanvasBox *box,
                                float               origin_x,
                                float               origin_y)
{
  GtkCanvasVector *self;
  GtkCanvasVector mult;
  graphene_vec2_t origin, minus_one;
 
  g_return_val_if_fail (box != NULL, NULL);
 
  graphene_vec2_init (&origin, origin_x, origin_y);
  graphene_vec2_init (&minus_one, -1, -1);
  gtk_canvas_vector_init_multiply (&mult, &box->origin, &box->size);
 
  self = gtk_canvas_vector_alloc ();
  gtk_canvas_vector_init_sum (self,
                              graphene_vec2_one (),
                              &box->point,
                              &origin,
                              &box->size,
                              &minus_one,
                              &mult,
                              NULL);

  gtk_canvas_vector_finish (&mult);

  return self;
}

/**
 * gtk_canvas_vector_new_distance:
 * @from: point from where to compute the distance
 * @to: point to where to compute the distance
 *
 * Creates a size for the given distance. Note that both width and height
 * can be negative if the coordinate of @to is smaller than @from in the
 * corresponding dimension.
 *
 * Returns: a new size
 **/
GtkCanvasVector *
gtk_canvas_vector_new_distance (const GtkCanvasVector *from,
                                const GtkCanvasVector *to)
{
  GtkCanvasVector *self;
  graphene_vec2_t minus_one;

  g_return_val_if_fail (from != NULL, NULL);
  g_return_val_if_fail (to != NULL, NULL);

  graphene_vec2_init (&minus_one, -1.f, -1.f);

  self = gtk_canvas_vector_alloc ();
  gtk_canvas_vector_init_sum (self,
                              graphene_vec2_one (),
                              from,
                              &minus_one,
                              to,
                              NULL);

  return self;
}

GtkCanvasVector *
gtk_canvas_vector_copy (const GtkCanvasVector *self)
{
  GtkCanvasVector *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gtk_canvas_vector_alloc ();
  gtk_canvas_vector_init_copy (copy, self);

  return copy;
}

void
gtk_canvas_vector_free (GtkCanvasVector *self)
{
  gtk_canvas_vector_finish (self);

  g_slice_free (GtkCanvasVector, self);
}

