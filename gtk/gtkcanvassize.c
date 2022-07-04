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
 * GtkCanvasSize:
 *
 * `GtkCanvasSize` describes a size in a `GtkCanvas`.
 */

#include "config.h"

#include "gtkcanvassize.h"

#include "gtkcanvasboxprivate.h"
#include "gtkcanvasitemprivate.h"
#include "gtkcanvasvector.h"
#include "gtkcanvasvectorprivate.h"

G_DEFINE_BOXED_TYPE (GtkCanvasSize, gtk_canvas_size,
                     gtk_canvas_size_copy,
                     gtk_canvas_size_free)

struct _GtkCanvasSize
{
  GtkCanvasVector vec2;
};

static GtkCanvasSize *
gtk_canvas_size_alloc (void)
{
  return g_slice_new (GtkCanvasSize);
}

/**
 * gtk_canvas_size_new:
 * @width: width of the size
 * @height: height of the size
 *
 * Creates a new size with the given dimensions
 *
 * Returns: a new size
 **/
GtkCanvasSize *
gtk_canvas_size_new (float width,
                     float height)
{
  GtkCanvasSize *self;

  self = gtk_canvas_size_alloc ();
  gtk_canvas_vector_init_constant (&self->vec2, width, height);

  return self;
}

/**
 * gtk_canvas_size_new_from_box:
 * @box: a box
 *
 * Creates a size for the given box.
 *
 * Returns: a new size
 **/
GtkCanvasSize *
gtk_canvas_size_new_from_box (const GtkCanvasBox *box)
{
  GtkCanvasSize *self;

  self = gtk_canvas_size_alloc ();
  gtk_canvas_vector_init_copy (&self->vec2, &box->size);

  return self;
}

/**
 * gtk_canvas_size_new_distance:
 * @from: point from where to compute the distance
 * @to: point to where to compute the distance
 *
 * Creates a size for the given distance. Note that both width and height
 * can be negative if the coordinate of @to is smaller than @from in the
 * corresponding dimension.
 *
 * Returns: a new size
 **/
GtkCanvasSize *
gtk_canvas_size_new_distance (const GtkCanvasVector *from,
                              const GtkCanvasVector *to)
{
  GtkCanvasSize *self;
  graphene_vec2_t minus_one;

  g_return_val_if_fail (from != NULL, NULL);
  g_return_val_if_fail (to != NULL, NULL);

  graphene_vec2_init (&minus_one, -1.f, -1.f);

  self = gtk_canvas_size_alloc ();
  gtk_canvas_vector_init_sum (&self->vec2,
                              graphene_vec2_one (),
                              from,
                              &minus_one,
                              to,
                              NULL);

  return self;
}

/**
 * gtk_canvas_size_new_measure_item:
 * @item: the item
 * @measure: how to measure the item
 *
 * Measures the widget of @item with the given method to determine
 * a size.
 *
 * Returns: a new size
 **/
GtkCanvasSize *
gtk_canvas_size_new_measure_item (GtkCanvasItem            *item,
                                  GtkCanvasItemMeasurement  measure)
{
  GtkCanvasSize *self;

  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (item), NULL);

  self = gtk_canvas_size_alloc ();
  gtk_canvas_vector_init_copy (&self->vec2,
                             gtk_canvas_item_get_measure_vec2 (item, measure));

  return self;
}

GtkCanvasSize *
gtk_canvas_size_copy (const GtkCanvasSize *self)
{
  GtkCanvasSize *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gtk_canvas_size_alloc ();
  gtk_canvas_vector_init_copy (&copy->vec2, &self->vec2);

  return copy;
}

void
gtk_canvas_size_free (GtkCanvasSize *self)
{
  g_return_if_fail (self != NULL);

  gtk_canvas_vector_finish (&self->vec2);

  g_slice_free (GtkCanvasSize, self);
}

gboolean
gtk_canvas_size_eval (const GtkCanvasSize *self,
                      float                *width,
                      float                *height)
{
  graphene_vec2_t vec2;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (width != NULL, FALSE);
  g_return_val_if_fail (height != NULL, FALSE);

  if (!gtk_canvas_vector_eval (&self->vec2, &vec2))
    {
      *width = 0;
      *height = 0;
      return FALSE;
    }

  *width = graphene_vec2_get_x (&vec2);
  *height = graphene_vec2_get_y (&vec2);
  return TRUE;
}

