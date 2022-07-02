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
 * GtkCanvasPoint:
 *
 * `GtkCanvasPoint` describes a point in a `GtkCanvas`.
 */

#include "config.h"

#include "gtkcanvaspoint.h"

#include "gtkcanvasboxprivate.h"
#include "gtkcanvasvec2private.h"

G_DEFINE_BOXED_TYPE (GtkCanvasPoint, gtk_canvas_point,
                     gtk_canvas_point_copy,
                     gtk_canvas_point_free)

struct _GtkCanvasPoint
{
  GtkCanvasVec2 vec2;
};

static GtkCanvasPoint *
gtk_canvas_point_alloc (void)
{
  return g_slice_new (GtkCanvasPoint);
}

/**
 * gtk_canvas_point_new:
 * @x: x coordinate
 * @y: y coordinate
 *
 * Creates a new point at the given coordinate.
 *
 * Returns: a new point
 **/
GtkCanvasPoint *
gtk_canvas_point_new (float x,
                      float y)
{
  GtkCanvasPoint *self;

  self = gtk_canvas_point_alloc ();
  gtk_canvas_vec2_init_constant (&self->vec2, x, y);

  return self;
}

/**
 * gtk_canvas_point_new_from_box:
 * @box: a box
 * @origin_x: x coordinate of box origin
 * @origin_y: y coordinate of box origin
 *
 * Creates a point relative to the given box.
 *
 * The origin describes where in the box the point is, with
 * (0, 0) being the top left and (1, 1) being the bottom right
 * corner of the box.
 *
 * Returns: a new point
 **/
GtkCanvasPoint *
gtk_canvas_point_new_from_box (const GtkCanvasBox *box,
                               float               origin_x,
                               float               origin_y)
{
  GtkCanvasPoint *self;
  graphene_vec2_t origin;
 
  g_return_val_if_fail (box != NULL, NULL);
 
  graphene_vec2_init (&origin, origin_x, origin_y);
  graphene_vec2_subtract (&origin, &box->origin, &origin);
 
  self = gtk_canvas_point_alloc ();
  gtk_canvas_vec2_init_sum (&self->vec2,
                            graphene_vec2_one (),
                            &box->point,
                            &origin,
                            &box->size,
                            NULL);
  return self;
}

GtkCanvasPoint *
gtk_canvas_point_copy (const GtkCanvasPoint *self)
{
  GtkCanvasPoint *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gtk_canvas_point_alloc ();
  gtk_canvas_vec2_init_copy (&copy->vec2, &self->vec2);

  return copy;
}

void
gtk_canvas_point_free (GtkCanvasPoint *self)
{
  gtk_canvas_vec2_finish (&self->vec2);

  g_slice_free (GtkCanvasPoint, self);
}

gboolean
gtk_canvas_point_eval (const GtkCanvasPoint *self,
                       float                *x,
                       float                *y)
{
  graphene_vec2_t vec2;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  if (!gtk_canvas_vec2_eval (&self->vec2, &vec2))
    {
      *x = 0;
      *y = 0;
      return FALSE;
    }

  *x = graphene_vec2_get_x (&vec2);
  *y = graphene_vec2_get_y (&vec2);
  return TRUE;
}

