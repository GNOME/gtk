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

#include "gtkcanvaspointprivate.h"

#include "gtkcanvasbox.h"

/* {{{ Boilerplate */

struct _GtkCanvasPointClass
{
  const char *type_name;

  void                  (* copy)                (GtkCanvasPoint         *self,
                                                 const GtkCanvasPoint   *source);
  void                  (* finish)              (GtkCanvasPoint         *self);
  gboolean              (* eval)                (const GtkCanvasPoint   *self,
                                                 float                  *x,
                                                 float                  *y);
};

G_DEFINE_BOXED_TYPE (GtkCanvasPoint, gtk_canvas_point,
                     gtk_canvas_point_copy,
                     gtk_canvas_point_free)

static gpointer
gtk_canvas_point_alloc (const GtkCanvasPointClass *class)
{
  GtkCanvasPoint *self = g_slice_new (GtkCanvasPoint);

  self->class = class;

  return self;
}

void
gtk_canvas_point_init_copy (GtkCanvasPoint       *self,
                            const GtkCanvasPoint *source)
{
  self->class = source->class;
  self->class->copy (self, source);
}

void
gtk_canvas_point_finish (GtkCanvasPoint *self)
{
  self->class->finish (self);
}

/* }}} */
/* {{{ OFFSET */

static void
gtk_canvas_point_offset_copy (GtkCanvasPoint       *point,
                              const GtkCanvasPoint *source_point)
{
  GtkCanvasPointOffset *self = &point->offset;
  const GtkCanvasPointOffset *source = &source_point->offset;

  *self = *source;

  if (source->other)
    self->other = gtk_canvas_point_copy (source->other);
}

static void
gtk_canvas_point_offset_finish (GtkCanvasPoint *point)
{
  GtkCanvasPointOffset *self = &point->offset;

  if (self->other)
    gtk_canvas_point_free (self->other);
}

static gboolean
gtk_canvas_point_offset_eval (const GtkCanvasPoint *point,
                              float                *x,
                              float                *y)
{
  const GtkCanvasPointOffset *self = &point->offset;

  if (self->other != NULL)
    {
      if (!gtk_canvas_point_eval (self->other, x, y))
        return FALSE;

      *x += self->dx;
      *y += self->dy;
    }
  else
    {
      *x = self->dx;
      *y = self->dy;
    }

  return TRUE;
}

static const GtkCanvasPointClass GTK_CANVAS_POINT_OFFSET_CLASS =
{
  "GtkCanvasPointOffset",
  gtk_canvas_point_offset_copy,
  gtk_canvas_point_offset_finish,
  gtk_canvas_point_offset_eval,
};

void
gtk_canvas_point_init (GtkCanvasPoint *point,
                       float           x,
                       float           y)
{
  GtkCanvasPointOffset *self = &point->offset;

  self->class = &GTK_CANVAS_POINT_OFFSET_CLASS;

  self->other = NULL;
  self->dx = x;
  self->dy = y;
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

  self = gtk_canvas_point_alloc (&GTK_CANVAS_POINT_OFFSET_CLASS);

  gtk_canvas_point_init (self, x, y);

  return self;
}

/* }}} */
/* {{{ BOX */

static void
gtk_canvas_point_box_copy (GtkCanvasPoint       *point,
                           const GtkCanvasPoint *source_point)
{
  GtkCanvasPointBox *self = &point->box;
  const GtkCanvasPointBox *source = &source_point->box;

  *self = *source;

  self->box = gtk_canvas_box_copy (source->box);
}

static void
gtk_canvas_point_box_finish (GtkCanvasPoint *point)
{
  GtkCanvasPointBox *self = &point->box;

  gtk_canvas_box_free (self->box);
}

static gboolean
gtk_canvas_point_box_eval (const GtkCanvasPoint *point,
                           float                *x,
                           float                *y)
{
  const GtkCanvasPointBox *self = &point->box;
  graphene_rect_t rect;

  if (!gtk_canvas_box_eval (self->box, &rect))
    return FALSE;

  *x = rect.origin.x + self->offset_x + self->origin_x * rect.size.width;
  *y = rect.origin.y + self->offset_y + self->origin_y * rect.size.height;

  return TRUE;
}

static const GtkCanvasPointClass GTK_CANVAS_POINT_BOX_CLASS =
{
  "GtkCanvasPointBox",
  gtk_canvas_point_box_copy,
  gtk_canvas_point_box_finish,
  gtk_canvas_point_box_eval,
};

/**
 * gtk_canvas_point_new_from_box:
 * @box: a box
 * @origin_x: x coordinate of box origin
 * @origin_y: y coordinate of box origin
 * @offset_x: offset in x direction
 * @offset_y: offset in y direction
 *
 * Creates a point relative to the given box.
 *
 * The origin describes where in the box the point is, with
 * (0, 0) being the top left and (1, 1) being the bottom right
 * corner of the box.
 *
 * The offset is then added to the origin. It may be negative.
 *
 * Returns: a new point
 **/
GtkCanvasPoint *
gtk_canvas_point_new_from_box (const GtkCanvasBox *box,
                               float               origin_x,
                               float               origin_y,
                               float               offset_x,
                               float               offset_y)
{
  GtkCanvasPointBox *self;

  g_return_val_if_fail (box != NULL, NULL);

  self = gtk_canvas_point_alloc (&GTK_CANVAS_POINT_BOX_CLASS);

  self->box = gtk_canvas_box_copy (box);
  self->origin_x = origin_x;
  self->origin_y = origin_y;
  self->offset_x = offset_x;
  self->offset_y = offset_y;

  return (GtkCanvasPoint *) self;
}

/* }}} */
/* {{{ PUBLIC API */

GtkCanvasPoint *
gtk_canvas_point_copy (const GtkCanvasPoint *self)
{
  GtkCanvasPoint *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gtk_canvas_point_alloc (self->class);

  gtk_canvas_point_init_copy (copy, self);

  return copy;
}

void
gtk_canvas_point_free (GtkCanvasPoint *self)
{
  gtk_canvas_point_finish (self);

  g_slice_free (GtkCanvasPoint, self);
}

gboolean
gtk_canvas_point_eval (const GtkCanvasPoint *self,
                       float                *x,
                       float                *y)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  if (self->class->eval (self, x, y))
    return TRUE;

  *x = 0;
  *y = 0;
  return FALSE;
}

