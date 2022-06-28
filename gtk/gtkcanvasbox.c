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

#include "gtkcanvaspointprivate.h"
#include "gtkcanvassizeprivate.h"

/* {{{ Boilerplate */

G_DEFINE_BOXED_TYPE (GtkCanvasBox, gtk_canvas_box,
                     gtk_canvas_box_copy,
                     gtk_canvas_box_free)

void
gtk_canvas_box_init (GtkCanvasBox         *self,
                     const GtkCanvasPoint *point,
                     const GtkCanvasSize  *size,
                     float                 origin_x,
                     float                 origin_y)
{
  gtk_canvas_point_init_copy (&self->point, point);
  gtk_canvas_size_init_copy (&self->size, size);
  self->origin_x = origin_x;
  self->origin_y = origin_y;
}

void
gtk_canvas_box_init_copy (GtkCanvasBox       *self,
                          const GtkCanvasBox *source)
{
  gtk_canvas_point_init_copy (&self->point, &source->point);
  gtk_canvas_size_init_copy (&self->size, &source->size);
  self->origin_x = source->origin_x;
  self->origin_y = source->origin_y;
}

void
gtk_canvas_box_finish (GtkCanvasBox *self)
{
  gtk_canvas_point_finish (&self->point);
  gtk_canvas_size_finish (&self->size);
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
  GtkCanvasSize size;
  GtkCanvasBox *result;

  g_return_val_if_fail (point1 != NULL, NULL);
  g_return_val_if_fail (point2 != NULL, NULL);

  gtk_canvas_size_init_distance (&size, point1, point2);
  result = gtk_canvas_box_new (point1, &size, 0, 0);
  gtk_canvas_size_finish (&size);

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

  g_return_val_if_fail (point != NULL, NULL);
  g_return_val_if_fail (size != NULL, NULL);

  self = g_slice_new (GtkCanvasBox);

  gtk_canvas_box_init (self, point, size, origin_x, origin_y);

  return self;
}

GtkCanvasBox *
gtk_canvas_box_copy (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return gtk_canvas_box_new (&self->point, &self->size, self->origin_x, self->origin_y);
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
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  if (gtk_canvas_point_eval (&self->point, &rect->origin.x, &rect->origin.y) &&
      gtk_canvas_size_eval (&self->size, &rect->size.width, &rect->size.height))
    return TRUE;

  *rect = *graphene_rect_zero ();
  return FALSE;
}

const GtkCanvasPoint *
gtk_canvas_box_get_point (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->point;
}

const GtkCanvasSize *
gtk_canvas_box_get_size (const GtkCanvasBox *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->size;
}

void
gtk_canvas_box_get_origin (const GtkCanvasBox *self,
                           float              *x,
                           float              *y)
{
  g_return_if_fail (self != NULL);

  if (x)
    *x = self->origin_x;
  if (y)
    *y = self->origin_y;
}
