/*
 * Copyright © 2014 Red Hat Inc.
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

#ifndef __GTK_RENDER_OPERATION_WIDGET_PRIVATE_H__
#define __GTK_RENDER_OPERATION_WIDGET_PRIVATE_H__

#include "gtkrenderoperation.h"

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_RENDER_OPERATION_WIDGET           (gtk_render_operation_widget_get_type ())
#define GTK_RENDER_OPERATION_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_RENDER_OPERATION_WIDGET, GtkRenderOperationWidget))
#define GTK_RENDER_OPERATION_WIDGET_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_RENDER_OPERATION_WIDGET, GtkRenderOperationWidgetClass))
#define GTK_IS_RENDER_OPERATION_WIDGET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_RENDER_OPERATION_WIDGET))
#define GTK_IS_RENDER_OPERATION_WIDGET_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_RENDER_OPERATION_WIDGET))
#define GTK_RENDER_OPERATION_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RENDER_OPERATION_WIDGET, GtkRenderOperationWidgetClass))

typedef struct _GtkRenderOperationWidget           GtkRenderOperationWidget;
typedef struct _GtkRenderOperationWidgetClass      GtkRenderOperationWidgetClass;

struct _GtkRenderOperationWidget
{
  GtkRenderOperation parent;
  
  GtkAllocation          widget_allocation;
  GtkAllocation          widget_clip;
  cairo_matrix_t         matrix;
  GList                 *operations;
};

struct _GtkRenderOperationWidgetClass
{
  GtkRenderOperationClass parent_class;
};

GType                   gtk_render_operation_widget_get_type    (void) G_GNUC_CONST;

GtkRenderOperation *    gtk_render_operation_widget_new         (GtkWidget                      *widget,
                                                                 cairo_matrix_t                 *matrix);

void                    gtk_render_operation_widget_add_operation(GtkRenderOperationWidget      *widget,
                                                                 GtkRenderOperation             *oper);

G_END_DECLS

#endif /* __GTK_RENDER_OPERATION_WIDGET_PRIVATE_H__ */
