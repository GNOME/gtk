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

struct _GtkCanvasBoxClass
{
  const char *type_name;

  void                  (* copy)                (GtkCanvasBox         *self,
                                                 const GtkCanvasBox   *source);
  void                  (* finish)              (GtkCanvasBox         *self);
  gboolean              (* eval)                (const GtkCanvasBox   *self,
                                                 graphene_rect_t      *rect);
};

G_DEFINE_BOXED_TYPE (GtkCanvasBox, gtk_canvas_box,
                     gtk_canvas_box_copy,
                     gtk_canvas_box_free)

static gpointer
gtk_canvas_box_alloc (const GtkCanvasBoxClass *class)
{
  GtkCanvasBox *self = g_slice_new (GtkCanvasBox);

  self->class = class;

  return self;
}

void
gtk_canvas_box_init_copy (GtkCanvasBox       *self,
                          const GtkCanvasBox *source)
{
  self->class = source->class;
  self->class->copy (self, source);
}

void
gtk_canvas_box_finish (GtkCanvasBox *self)
{
  self->class->finish (self);
}

/* }}} */
/* {{{ POINTS */

static void
gtk_canvas_box_points_copy (GtkCanvasBox       *box,
                            const GtkCanvasBox *source_box)
{
  GtkCanvasBoxPoints *self = &box->points;
  const GtkCanvasBoxPoints *source = &source_box->points;

  gtk_canvas_point_init_copy (&self->point1, &source->point1);
  gtk_canvas_point_init_copy (&self->point2, &source->point2);
}

static void
gtk_canvas_box_points_finish (GtkCanvasBox *box)
{
  GtkCanvasBoxPoints *self = &box->points;

  gtk_canvas_point_finish (&self->point1);
  gtk_canvas_point_finish (&self->point2);
}

static gboolean
gtk_canvas_box_points_eval (const GtkCanvasBox *box,
                            graphene_rect_t    *rect)
{
  const GtkCanvasBoxPoints *self = &box->points;
  float x1, x2, y1, y2;

  if (!gtk_canvas_point_eval (&self->point1, &x1, &y1) ||
      !gtk_canvas_point_eval (&self->point2, &x2, &y2))
    return FALSE;

  graphene_rect_init (rect, x1, y1, x2 - x1, y2 - y1);
  return TRUE;
}

static const GtkCanvasBoxClass GTK_CANVAS_BOX_POINTS_CLASS =
{
  "GtkCanvasBoxPoints",
  gtk_canvas_box_points_copy,
  gtk_canvas_box_points_finish,
  gtk_canvas_box_points_eval,
};

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
  GtkCanvasBoxPoints *self;

  g_return_val_if_fail (point1 != NULL, NULL);
  g_return_val_if_fail (point2 != NULL, NULL);

  self = gtk_canvas_box_alloc (&GTK_CANVAS_BOX_POINTS_CLASS);

  gtk_canvas_point_init_copy (&self->point1, point1);
  gtk_canvas_point_init_copy (&self->point2, point2);

  return (GtkCanvasBox *) self;
}

/* }}} */
/* {{{ SIZE */

static void
gtk_canvas_box_size_copy (GtkCanvasBox       *box,
                          const GtkCanvasBox *source_box)
{
  const GtkCanvasBoxSize *source = &source_box->size;

  gtk_canvas_box_init (box, &source->point, &source->size, source->origin_x, source->origin_y);
}

static void
gtk_canvas_box_size_finish (GtkCanvasBox *box)
{
  GtkCanvasBoxSize *self = &box->size;

  gtk_canvas_point_finish (&self->point);
  gtk_canvas_size_finish (&self->size);
}

static gboolean
gtk_canvas_box_size_eval (const GtkCanvasBox *box,
                          graphene_rect_t    *rect)
{
  const GtkCanvasBoxSize *self = &box->size;
  float x, y, width, height;

  if (!gtk_canvas_point_eval (&self->point, &x, &y) ||
      !gtk_canvas_size_eval (&self->size, &width, &height))
    return FALSE;

  graphene_rect_init (rect,
                      x - width * self->origin_x,
                      y - height * self->origin_y,
                      width, height);

  return TRUE;
}

static const GtkCanvasBoxClass GTK_CANVAS_BOX_SIZE_CLASS =
{
  "GtkCanvasBoxSize",
  gtk_canvas_box_size_copy,
  gtk_canvas_box_size_finish,
  gtk_canvas_box_size_eval,
};

void
gtk_canvas_box_init (GtkCanvasBox         *box,
                     const GtkCanvasPoint *point,
                     const GtkCanvasSize  *size,
                     float                 origin_x,
                     float                 origin_y)
{
  GtkCanvasBoxSize *self = &box->size;

  self->class = &GTK_CANVAS_BOX_SIZE_CLASS;

  gtk_canvas_point_init_copy (&self->point, point);
  gtk_canvas_size_init_copy (&self->size, size);
  self->origin_x = origin_x;
  self->origin_y = origin_y;
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

  self = gtk_canvas_box_alloc (&GTK_CANVAS_BOX_SIZE_CLASS);

  gtk_canvas_box_init (self, point, size, origin_x, origin_y);

  return self;
}

/* }}} */
/* {{{ PUBLIC API */

GtkCanvasBox *
gtk_canvas_box_copy (const GtkCanvasBox *self)
{
  GtkCanvasBox *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gtk_canvas_box_alloc (self->class);

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
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  if (self->class->eval (self, rect))
    return TRUE;

  *rect = *graphene_rect_zero ();
  return FALSE;
}

