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

#include "gtkcanvassizeprivate.h"

#include "gtkcanvasbox.h"
#include "gtkcanvasitem.h"
#include "gtkwidget.h"

/* {{{ Boilerplate */

struct _GtkCanvasSizeClass
{
  const char *type_name;

  void                  (* copy)                (GtkCanvasSize          *self,
                                                 const GtkCanvasSize    *source);
  void                  (* finish)              (GtkCanvasSize          *self);
  gboolean              (* eval)                (const GtkCanvasSize    *self,
                                                 float                  *width,
                                                 float                  *height);
};

G_DEFINE_BOXED_TYPE (GtkCanvasSize, gtk_canvas_size,
                     gtk_canvas_size_copy,
                     gtk_canvas_size_free)

static gpointer
gtk_canvas_size_alloc (const GtkCanvasSizeClass *class)
{
  GtkCanvasSize *self = g_slice_new (GtkCanvasSize);

  self->class = class;

  return self;
}

void
gtk_canvas_size_init_copy (GtkCanvasSize       *self,
                           const GtkCanvasSize *source)
{
  self->class = source->class;
  self->class->copy (self, source);
}

void
gtk_canvas_size_finish (GtkCanvasSize *self)
{
  self->class->finish (self);
}

/* }}} */
/* {{{ ABSOLUTE */

static void
gtk_canvas_size_absolute_copy (GtkCanvasSize       *size,
                               const GtkCanvasSize *source_size)
{
  GtkCanvasSizeAbsolute *self = &size->absolute;
  const GtkCanvasSizeAbsolute *source = &source_size->absolute;

  *self = *source;
}

static void
gtk_canvas_size_absolute_finish (GtkCanvasSize *size)
{
}

static gboolean
gtk_canvas_size_absolute_eval (const GtkCanvasSize *size,
                               float               *width,
                               float               *height)
{
  const GtkCanvasSizeAbsolute *self = &size->absolute;

  *width = self->width;
  *height = self->height;

  return TRUE;
}

static const GtkCanvasSizeClass GTK_CANVAS_SIZE_ABSOLUTE_CLASS =
{
  "GtkCanvasSizeAbsolute",
  gtk_canvas_size_absolute_copy,
  gtk_canvas_size_absolute_finish,
  gtk_canvas_size_absolute_eval,
};

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
  GtkCanvasSizeAbsolute *self;

  self = gtk_canvas_size_alloc (&GTK_CANVAS_SIZE_ABSOLUTE_CLASS);
  self->width = width;
  self->height = height;

  return (GtkCanvasSize *) self;
}

/* }}} */
/* {{{ BOX */

static void
gtk_canvas_size_box_copy (GtkCanvasSize       *size,
                          const GtkCanvasSize *source_size)
{
  GtkCanvasSizeBox *self = &size->box;
  const GtkCanvasSizeBox *source = &source_size->box;

  *self = *source;

  self->box = gtk_canvas_box_copy (source->box);
}

static void
gtk_canvas_size_box_finish (GtkCanvasSize *size)
{
  GtkCanvasSizeBox *self = &size->box;

  gtk_canvas_box_free (self->box);
}

static gboolean
gtk_canvas_size_box_eval (const GtkCanvasSize *size,
                          float               *width,
                          float               *height)
{
  const GtkCanvasSizeBox *self = &size->box;
  graphene_rect_t rect;

  if (!gtk_canvas_box_eval (self->box, &rect))
    return FALSE;

  *width = rect.size.width;
  *height = rect.size.height;

  return TRUE;
}

static const GtkCanvasSizeClass GTK_CANVAS_SIZE_BOX_CLASS =
{
  "GtkCanvasSizeBox",
  gtk_canvas_size_box_copy,
  gtk_canvas_size_box_finish,
  gtk_canvas_size_box_eval,
};

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
  GtkCanvasSizeBox *self;

  g_return_val_if_fail (box != NULL, NULL);

  self = gtk_canvas_size_alloc (&GTK_CANVAS_SIZE_BOX_CLASS);

  /* FIXME: We could potentially just copy the box's size here */
  self->box = gtk_canvas_box_copy (box);

  return (GtkCanvasSize *) self;
}

/* }}} */
/* {{{ MEASURE */

static void
gtk_canvas_size_measure_copy (GtkCanvasSize       *size,
                              const GtkCanvasSize *source_size)
{
  const GtkCanvasSizeMeasure *source = &source_size->measure;

  gtk_canvas_size_init_measure_item (size, source->item, source->measure);
}

static void
gtk_canvas_size_measure_finish (GtkCanvasSize *size)
{
}

static gboolean
gtk_canvas_size_measure_eval (const GtkCanvasSize *size,
                              float               *width,
                              float               *height)
{
  const GtkCanvasSizeMeasure *self = &size->measure;
  GtkWidget *widget;
  int w, h;

  if (self->item == NULL)
    return FALSE;

  widget = gtk_canvas_item_get_widget (self->item);
  if (widget == NULL)
    {
      *width = 0;
      *height = 0;
      return TRUE;
    }

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      switch (self->measure)
      {
        case GTK_CANVAS_ITEM_MEASURE_MIN_FOR_MIN:
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &w, NULL, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, w, &h, NULL, NULL, NULL);
          break;
        case GTK_CANVAS_ITEM_MEASURE_MIN_FOR_NAT:
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &w, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, w, &h, NULL, NULL, NULL);
          break;
        case GTK_CANVAS_ITEM_MEASURE_NAT_FOR_MIN:
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &w, NULL, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, w, NULL, &h, NULL, NULL);
          break;
        case GTK_CANVAS_ITEM_MEASURE_NAT_FOR_NAT:
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &w, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, w, NULL, &h, NULL, NULL);
          break;
        default:
          g_assert_not_reached ();
          w = h = 0;
          break;
      }
    }
  else
    {
      switch (self->measure)
      {
        case GTK_CANVAS_ITEM_MEASURE_MIN_FOR_MIN:
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, &h, NULL, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, h, &w, NULL, NULL, NULL);
          break;
        case GTK_CANVAS_ITEM_MEASURE_MIN_FOR_NAT:
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, NULL, &h, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, h, &w, NULL, NULL, NULL);
          break;
        case GTK_CANVAS_ITEM_MEASURE_NAT_FOR_MIN:
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, &h, NULL, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, h, NULL, &w, NULL, NULL);
          break;
        case GTK_CANVAS_ITEM_MEASURE_NAT_FOR_NAT:
          gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, NULL, &h, NULL, NULL);
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, h, NULL, &w, NULL, NULL);
          break;
        default:
          g_assert_not_reached ();
          w = h = 0;
          break;
      }
    }

  *width = w;
  *height = h;

  return TRUE;
}

static const GtkCanvasSizeClass GTK_CANVAS_SIZE_MEASURE_CLASS =
{
  "GtkCanvasSizeMeasure",
  gtk_canvas_size_measure_copy,
  gtk_canvas_size_measure_finish,
  gtk_canvas_size_measure_eval,
};

void
gtk_canvas_size_init_measure_item (GtkCanvasSize            *size,
                                   GtkCanvasItem            *item,
                                   GtkCanvasItemMeasurement  measure)
{
  GtkCanvasSizeMeasure *self = &size->measure;

  self->class = &GTK_CANVAS_SIZE_MEASURE_CLASS;
  
  self->item = item;
  self->measure = measure;
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

  self = gtk_canvas_size_alloc (&GTK_CANVAS_SIZE_MEASURE_CLASS);

  gtk_canvas_size_init_measure_item (self, item, measure);

  return self;
}

/* }}} */
/* {{{ PUBLIC API */

GtkCanvasSize *
gtk_canvas_size_copy (const GtkCanvasSize *self)
{
  GtkCanvasSize *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gtk_canvas_size_alloc (self->class);

  gtk_canvas_size_init_copy (copy, self);

  return copy;
}

void
gtk_canvas_size_free (GtkCanvasSize *self)
{
  gtk_canvas_size_finish (self);

  g_slice_free (GtkCanvasSize, self);
}

gboolean
gtk_canvas_size_eval (const GtkCanvasSize *self,
                      float                *width,
                      float                *height)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (width != NULL, FALSE);
  g_return_val_if_fail (height != NULL, FALSE);

  if (self->class->eval (self, width, height))
    return TRUE;

  *width = 0;
  *height = 0;
  return FALSE;
}

