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

#ifndef __GTK_RENDER_OPERATION_PRIVATE_H__
#define __GTK_RENDER_OPERATION_PRIVATE_H__

#include <cairo.h>

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_RENDER_OPERATION           (gtk_render_operation_get_type ())
#define GTK_RENDER_OPERATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_RENDER_OPERATION, GtkRenderOperation))
#define GTK_RENDER_OPERATION_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_RENDER_OPERATION, GtkRenderOperationClass))
#define GTK_IS_RENDER_OPERATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_RENDER_OPERATION))
#define GTK_IS_RENDER_OPERATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_RENDER_OPERATION))
#define GTK_RENDER_OPERATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RENDER_OPERATION, GtkRenderOperationClass))

typedef struct _GtkRenderOperation           GtkRenderOperation;
typedef struct _GtkRenderOperationClass      GtkRenderOperationClass;

struct _GtkRenderOperation
{
  GObject parent;
};

struct _GtkRenderOperationClass
{
  GObjectClass parent_class;

  void          (* get_clip)                            (GtkRenderOperation      *operation,
                                                         cairo_rectangle_int_t   *clip);
  void          (* get_matrix)                          (GtkRenderOperation      *operation,
                                                         cairo_matrix_t          *matrix);

  char *        (* describe)                            (GtkRenderOperation      *operation);
  void          (* draw)                                (GtkRenderOperation      *operation,
                                                         cairo_t                 *cr);
};

GType           gtk_render_operation_get_type           (void) G_GNUC_CONST;

void            gtk_render_operation_get_clip           (GtkRenderOperation      *operation,
                                                         cairo_rectangle_int_t   *clip);
void            gtk_render_operation_get_matrix         (GtkRenderOperation      *operation,
                                                         cairo_matrix_t          *matrix);

char *          gtk_render_operation_describe           (GtkRenderOperation      *operation);
void            gtk_render_operation_draw               (GtkRenderOperation      *operation,
                                                         cairo_t                 *cr);

G_END_DECLS

#endif /* __GTK_RENDER_OPERATION_PRIVATE_H__ */
