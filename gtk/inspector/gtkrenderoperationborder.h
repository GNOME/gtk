/*
 * Copyright © 2015 Red Hat Inc.
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

#ifndef __GTK_RENDER_OPERATION_BORDER_PRIVATE_H__
#define __GTK_RENDER_OPERATION_BORDER_PRIVATE_H__

#include "gtkrenderoperation.h"

#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_RENDER_OPERATION_BORDER           (gtk_render_operation_border_get_type ())
#define GTK_RENDER_OPERATION_BORDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_RENDER_OPERATION_BORDER, GtkRenderOperationBorder))
#define GTK_RENDER_OPERATION_BORDER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_RENDER_OPERATION_BORDER, GtkRenderOperationBorderClass))
#define GTK_IS_RENDER_OPERATION_BORDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_RENDER_OPERATION_BORDER))
#define GTK_IS_RENDER_OPERATION_BORDER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_RENDER_OPERATION_BORDER))
#define GTK_RENDER_OPERATION_BORDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RENDER_OPERATION_BORDER, GtkRenderOperationBorderClass))

typedef struct _GtkRenderOperationBorder           GtkRenderOperationBorder;
typedef struct _GtkRenderOperationBorderClass      GtkRenderOperationBorderClass;

struct _GtkRenderOperationBorder
{
  GtkRenderOperation parent;
  
  GtkCssStyle      *style;
  double            x;
  double            y;
  double            width;
  double            height;
  guint             hidden_side;
  GtkJunctionSides  junction;
};

struct _GtkRenderOperationBorderClass
{
  GtkRenderOperationClass parent_class;
};

GType                   gtk_render_operation_border_get_type         (void) G_GNUC_CONST;

GtkRenderOperation *    gtk_render_operation_border_new              (GtkCssStyle            *style,
                                                                      double                  x,
                                                                      double                  y,
                                                                      double                  width,
                                                                      double                  height,
                                                                      guint                   hidden_side,
                                                                      GtkJunctionSides        junction);

G_END_DECLS

#endif /* __GTK_RENDER_OPERATION_BORDER_PRIVATE_H__ */
