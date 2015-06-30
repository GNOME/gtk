/*
 * Copyright © 2011 Red Hat Inc.
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

#include "config.h"

#include "gtkrenderoperationcairo.h"

#include <math.h>

G_DEFINE_TYPE (GtkRenderOperationCairo, gtk_render_operation_cairo, GTK_TYPE_RENDER_OPERATION)

static void
gtk_render_operation_cairo_get_clip (GtkRenderOperation    *operation,
                                     cairo_rectangle_int_t *clip)
{
  GtkRenderOperationCairo *oper = GTK_RENDER_OPERATION_CAIRO (operation);
  cairo_rectangle_t extents;
  double off_x, off_y;

  g_assert (cairo_surface_get_type (oper->surface) == CAIRO_SURFACE_TYPE_RECORDING);
  cairo_recording_surface_ink_extents (oper->surface, &extents.x, &extents.y, &extents.width, &extents.height);
  cairo_surface_get_device_offset (oper->surface, &off_x, &off_y);
  extents.x -= off_x;
  extents.y -= off_y;

  clip->x = floor (extents.x);
  clip->y = floor (extents.y);
  clip->width = ceil (extents.x + extents.width) - clip->x;
  clip->height = ceil (extents.y + extents.height) - clip->y;
}

static void
gtk_render_operation_cairo_draw (GtkRenderOperation *operation,
                                 cairo_t            *cr)
{
  GtkRenderOperationCairo *oper = GTK_RENDER_OPERATION_CAIRO (operation);

  cairo_set_source_surface (cr, oper->surface, 0, 0);
  cairo_paint (cr);
}

static void
gtk_render_operation_cairo_finalize (GObject *object)
{
  GtkRenderOperationCairo *oper = GTK_RENDER_OPERATION_CAIRO (object);

  cairo_surface_destroy (oper->surface);

  G_OBJECT_CLASS (gtk_render_operation_cairo_parent_class)->finalize (object);
}

static void
gtk_render_operation_cairo_class_init (GtkRenderOperationCairoClass *klass)
{
  GtkRenderOperationClass *operation_class = GTK_RENDER_OPERATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_render_operation_cairo_finalize;

  operation_class->get_clip = gtk_render_operation_cairo_get_clip;
  operation_class->draw = gtk_render_operation_cairo_draw;
}

static void
gtk_render_operation_cairo_init (GtkRenderOperationCairo *image)
{
}

GtkRenderOperation *
gtk_render_operation_cairo_new (cairo_surface_t *surface)
{
  GtkRenderOperationCairo *result;

  g_return_val_if_fail (surface != NULL, NULL);

  result = g_object_new (GTK_TYPE_RENDER_OPERATION_CAIRO, NULL);
  
  result->surface = cairo_surface_reference (surface);
 
  return GTK_RENDER_OPERATION (result);
}

