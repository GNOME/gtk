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

#include "gtkcanvasbox.h"

G_DEFINE_BOXED_TYPE (GtkCanvasBox, gtk_canvas_box,
                     gtk_canvas_box_copy,
                     gtk_canvas_box_free)

GtkCanvasBox *
gtk_canvas_box_copy (const GtkCanvasBox *self)
{
  GtkCanvasBox *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = g_slice_dup (GtkCanvasBox, self);

  return copy;
}

void
gtk_canvas_box_free (GtkCanvasBox *self)
{
  g_slice_free (GtkCanvasBox, self);
}

void
gtk_canvas_box_init (GtkCanvasBox *self,
                     float         point_x,
                     float         point_y,
                     float         width,
                     float         height,
                     float         origin_horizontal,
                     float         origin_vertical)
{
  self->point.x = point_x;
  self->point.y = point_y;
  self->size.width = width;
  self->size.height = height;
  self->origin.horizontal = origin_horizontal;
  self->origin.vertical = origin_vertical;
}

void
gtk_canvas_box_to_rect (const GtkCanvasBox *self,
                        graphene_rect_t    *rect)
{
  rect->size = self->size;
  rect->origin = GRAPHENE_POINT_INIT (self->point.x - self->origin.horizontal * self->size.width,
                                      self->point.y - self->origin.vertical * self->size.height);   
  graphene_rect_normalize (rect);
}
