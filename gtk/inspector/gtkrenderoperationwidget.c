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

#include "gtkrenderoperationwidget.h"

G_DEFINE_TYPE (GtkRenderOperationWidget, gtk_render_operation_widget, GTK_TYPE_RENDER_OPERATION)

static void
gtk_render_operation_widget_get_clip (GtkRenderOperation    *operation,
                                      cairo_rectangle_int_t *clip)
{
  GtkRenderOperationWidget *oper = GTK_RENDER_OPERATION_WIDGET (operation);

  *clip = oper->widget_clip;
}

static void
gtk_render_operation_widget_get_matrix (GtkRenderOperation *operation,
                                        cairo_matrix_t     *matrix)
{
  GtkRenderOperationWidget *oper = GTK_RENDER_OPERATION_WIDGET (operation);

  *matrix = oper->matrix;
}

static char *
gtk_render_operation_widget_describe (GtkRenderOperation *operation)
{
  GtkRenderOperationWidget *oper = GTK_RENDER_OPERATION_WIDGET (operation);

  return g_strdup (g_type_name (oper->widget_type));
}

static void
gtk_render_operation_widget_draw (GtkRenderOperation *operation,
                                  cairo_t            *cr)
{
  GtkRenderOperationWidget *oper = GTK_RENDER_OPERATION_WIDGET (operation);
  cairo_matrix_t matrix;
  GList *l;

  for (l = oper->operations; l; l = l->next)
    {
      cairo_save (cr);

      gtk_render_operation_get_matrix (l->data, &matrix);
      cairo_transform (cr, &matrix);
      gtk_render_operation_draw (l->data, cr);

      cairo_restore (cr);
    }
}

static void
gtk_render_operation_widget_finalize (GObject *object)
{
  GtkRenderOperationWidget *oper = GTK_RENDER_OPERATION_WIDGET (object);

  g_list_free_full (oper->operations, g_object_unref);

  G_OBJECT_CLASS (gtk_render_operation_widget_parent_class)->finalize (object);
}

static void
gtk_render_operation_widget_class_init (GtkRenderOperationWidgetClass *klass)
{
  GtkRenderOperationClass *operation_class = GTK_RENDER_OPERATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_render_operation_widget_finalize;

  operation_class->get_clip = gtk_render_operation_widget_get_clip;
  operation_class->get_matrix = gtk_render_operation_widget_get_matrix;
  operation_class->describe = gtk_render_operation_widget_describe;
  operation_class->draw = gtk_render_operation_widget_draw;
}

static void
gtk_render_operation_widget_init (GtkRenderOperationWidget *image)
{
}

GtkRenderOperation *
gtk_render_operation_widget_new (GtkWidget      *widget,
                                 cairo_matrix_t *matrix)

{
  GtkRenderOperationWidget *result;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  result = g_object_new (GTK_TYPE_RENDER_OPERATION_WIDGET, NULL);
  
  result->widget_type = G_OBJECT_TYPE (widget);
  gtk_widget_get_allocation (widget, &result->widget_allocation);
  gtk_widget_get_clip (widget, &result->widget_clip);
  result->widget_clip.x -= result->widget_allocation.x;
  result->widget_clip.y -= result->widget_allocation.y;
  result->matrix = *matrix;
 
  return GTK_RENDER_OPERATION (result);
}

void
gtk_render_operation_widget_add_operation (GtkRenderOperationWidget *widget,
                                           GtkRenderOperation       *oper)
{
  g_return_if_fail (GTK_IS_RENDER_OPERATION_WIDGET (widget));
  g_return_if_fail (GTK_IS_RENDER_OPERATION (oper));

  g_object_ref (oper);

  widget->operations = g_list_append (widget->operations, oper);
}

