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

#include "recordingrenderops.h"

#include "gtkrenderoperationbackground.h"
#include "gtkrenderoperationborder.h"
#include "gtkrenderoperationcairo.h"
#include "gtkrenderoperationoutline.h"
#include "gtkrenderoperationwidget.h"
#include "gtkwidget.h"

G_DEFINE_TYPE (GtkRecordingRenderOps, gtk_recording_render_ops, GTK_TYPE_RENDER_OPS)

static cairo_t *
gtk_recording_render_ops_cairo_create (GtkRecordingRenderOps *ops,
                                       GtkWidget             *widget)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_rectangle_t extents;
  GtkAllocation clip, allocation;

  gtk_widget_get_clip (widget, &clip);
  gtk_widget_get_allocation (widget, &allocation);

  extents.x = allocation.x - clip.x;
  extents.y = allocation.y - clip.y;
  extents.width = clip.width;
  extents.height = clip.height;

  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, &extents);
  cairo_surface_set_device_offset (surface, -extents.x, -extents.y);
  cr = cairo_create (surface);
  cairo_surface_destroy (surface);

  gtk_cairo_set_render_ops (cr, GTK_RENDER_OPS (ops));

  return cr;
}

static void
gtk_recording_render_ops_save_snapshot (GtkRecordingRenderOps *record,
                                        cairo_t               *cr)
{
  GtkRenderOperation *oper;
  cairo_surface_t *target, *snapshot;
  cairo_rectangle_t extents, real_extents;
  cairo_t *copy;

  target = cairo_get_target (cr);
  g_assert (cairo_surface_get_type (target) == CAIRO_SURFACE_TYPE_RECORDING);
  cairo_recording_surface_ink_extents (target, &extents.x, &extents.y, &extents.width, &extents.height);
  
  /* no snapshot necessary */
  if (extents.width <= 0 || extents.height <= 0)
    return;

  /* create snapshot */
  g_print ("saving snapshot of %g %g %g %g\n", extents.x, extents.y, extents.width, extents.height);
  real_extents = extents;
  real_extents.x = real_extents.y = 0;
  snapshot = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, &real_extents);
  cairo_surface_set_device_offset (snapshot, -extents.x, -extents.y);
  copy = cairo_create (snapshot);
  cairo_set_source_surface (copy, target, 0, 0);
  cairo_paint (copy);
  cairo_destroy (copy);

  if (extents.x > 10)
    cairo_surface_write_to_png (snapshot, "foo.png");

  /* enqueue snapshot */
  oper = gtk_render_operation_cairo_new (snapshot);
  cairo_surface_destroy (snapshot);
  gtk_render_operation_widget_add_operation (record->widgets->data, oper);
  g_object_unref (oper);

  /* clear original_surface */
  cairo_save (cr);
  cairo_reset_clip (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);
}

static void
gtk_recording_render_ops_draw_background (GtkRenderOps     *ops,
                                          GtkCssStyle      *style,
                                          cairo_t          *cr,
                                          gdouble           x,
                                          gdouble           y,
                                          gdouble           width,
                                          gdouble           height,
                                          GtkJunctionSides  junction)
{
  GtkRecordingRenderOps *record = GTK_RECORDING_RENDER_OPS (ops);
  GtkRenderOperation *oper;

  gtk_recording_render_ops_save_snapshot (record, cr);

  oper = gtk_render_operation_background_new (style, x, y, width, height, junction);
  gtk_render_operation_widget_add_operation (record->widgets->data, oper);
  g_object_unref (oper);
}

static void
gtk_recording_render_ops_draw_border (GtkRenderOps     *ops,
                                      GtkCssStyle      *style,
                                      cairo_t          *cr,
                                      gdouble           x,
                                      gdouble           y,
                                      gdouble           width,
                                      gdouble           height,
                                      guint             hidden_side,
                                      GtkJunctionSides  junction)
{
  GtkRecordingRenderOps *record = GTK_RECORDING_RENDER_OPS (ops);
  GtkRenderOperation *oper;

  gtk_recording_render_ops_save_snapshot (record, cr);

  oper = gtk_render_operation_border_new (style, x, y, width, height, hidden_side, junction);
  gtk_render_operation_widget_add_operation (record->widgets->data, oper);
  g_object_unref (oper);
}

static void
gtk_recording_render_ops_draw_outline (GtkRenderOps     *ops,
                                       GtkCssStyle      *style,
                                       cairo_t          *cr,
                                       gdouble           x,
                                       gdouble           y,
                                       gdouble           width,
                                       gdouble           height)
{
  GtkRecordingRenderOps *record = GTK_RECORDING_RENDER_OPS (ops);
  GtkRenderOperation *oper;

  gtk_recording_render_ops_save_snapshot (record, cr);

  oper = gtk_render_operation_outline_new (style, x, y, width, height);
  gtk_render_operation_widget_add_operation (record->widgets->data, oper);
  g_object_unref (oper);
}

static cairo_t *
gtk_recording_render_ops_begin_draw_widget (GtkRenderOps *ops,
                                            GtkWidget    *widget,
                                            cairo_t      *cr)
{
  GtkRecordingRenderOps *record = GTK_RECORDING_RENDER_OPS (ops);
  GtkRenderOperation *oper;
  cairo_matrix_t matrix;

  if (record->widgets)
    gtk_recording_render_ops_save_snapshot (record, cr);

  g_print ("begin drawing widget %s\n", gtk_widget_get_name (widget));

  cairo_get_matrix (cr, &matrix);

  oper = gtk_render_operation_widget_new (widget, &matrix);
  
  if (record->widgets)
    gtk_render_operation_widget_add_operation (record->widgets->data, oper);
  record->widgets = g_list_prepend (record->widgets, oper);

  return gtk_recording_render_ops_cairo_create (record, widget);
}

static void
gtk_recording_render_ops_end_draw_widget (GtkRenderOps *ops,
                                          GtkWidget    *widget,
                                          cairo_t      *draw_cr,
                                          cairo_t      *original_cr)
{
  GtkRecordingRenderOps *record = GTK_RECORDING_RENDER_OPS (ops);

  gtk_recording_render_ops_save_snapshot (record, draw_cr);

  g_print ("ended drawing widget %s\n", gtk_widget_get_name (widget));

  if (record->widgets->next)
    {
      g_object_unref (record->widgets->data);
      record->widgets = g_list_remove (record->widgets, record->widgets->data);
    }

  cairo_destroy (draw_cr);
}

static void
gtk_recording_render_ops_class_init (GtkRecordingRenderOpsClass *klass)
{
  GtkRenderOpsClass *ops_class = GTK_RENDER_OPS_CLASS (klass);

  ops_class->begin_draw_widget = gtk_recording_render_ops_begin_draw_widget;
  ops_class->end_draw_widget = gtk_recording_render_ops_end_draw_widget;
  ops_class->draw_background = gtk_recording_render_ops_draw_background;
  ops_class->draw_border = gtk_recording_render_ops_draw_border;
  ops_class->draw_outline = gtk_recording_render_ops_draw_outline;
}

static void
gtk_recording_render_ops_init (GtkRecordingRenderOps *image)
{
}

GtkRenderOps *
gtk_recording_render_ops_new (void)
{
  return g_object_new (GTK_TYPE_RECORDING_RENDER_OPS, NULL);
}

GtkRenderOperation *
gtk_recording_render_ops_run_for_widget (GtkRecordingRenderOps *ops,
                                         GtkWidget             *widget)
{
  GtkRenderOperation *result;
  cairo_t *cr;

  g_return_val_if_fail (GTK_IS_RECORDING_RENDER_OPS (ops), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  cr = gtk_recording_render_ops_cairo_create (ops, widget);
  gtk_widget_draw (widget, cr);
  cairo_destroy (cr);

  result = ops->widgets->data;
  g_list_free (ops->widgets);
  ops->widgets = NULL;

  return result;
}

