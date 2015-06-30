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

#ifndef __GTK_RENDER_OPERATION_BACKGROUND_PRIVATE_H__
#define __GTK_RENDER_OPERATION_BACKGROUND_PRIVATE_H__

#include "gtkrenderoperation.h"

#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_RENDER_OPERATION_BACKGROUND           (gtk_render_operation_background_get_type ())
#define GTK_RENDER_OPERATION_BACKGROUND(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_RENDER_OPERATION_BACKGROUND, GtkRenderOperationBackground))
#define GTK_RENDER_OPERATION_BACKGROUND_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_RENDER_OPERATION_BACKGROUND, GtkRenderOperationBackgroundClass))
#define GTK_IS_RENDER_OPERATION_BACKGROUND(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_RENDER_OPERATION_BACKGROUND))
#define GTK_IS_RENDER_OPERATION_BACKGROUND_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_RENDER_OPERATION_BACKGROUND))
#define GTK_RENDER_OPERATION_BACKGROUND_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RENDER_OPERATION_BACKGROUND, GtkRenderOperationBackgroundClass))

typedef struct _GtkRenderOperationBackground           GtkRenderOperationBackground;
typedef struct _GtkRenderOperationBackgroundClass      GtkRenderOperationBackgroundClass;

struct _GtkRenderOperationBackground
{
  GtkRenderOperation parent;
  
  GtkCssStyle      *style;
  double            x;
  double            y;
  double            width;
  double            height;
  GtkJunctionSides  junction;
};

struct _GtkRenderOperationBackgroundClass
{
  GtkRenderOperationClass parent_class;
};

GType                   gtk_render_operation_background_get_type     (void) G_GNUC_CONST;

GtkRenderOperation *    gtk_render_operation_background_new          (GtkCssStyle            *style,
                                                                      double                  x,
                                                                      double                  y,
                                                                      double                  width,
                                                                      double                  height,
                                                                      GtkJunctionSides        junction);

G_END_DECLS

#endif /* __GTK_RENDER_OPERATION_BACKGROUND_PRIVATE_H__ */
