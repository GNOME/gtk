/*
 * Copyright © 2019 Benjamin Otte
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


#ifndef __GTK_TRANSFORM_H__
#define __GTK_TRANSFORM_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <graphene.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_MATRIX (gtk_transform_get_type ())

typedef enum
{
  GTK_TRANSFORM_TYPE_IDENTITY,
  GTK_TRANSFORM_TYPE_TRANSFORM,
  GTK_TRANSFORM_TYPE_TRANSLATE,
  GTK_TRANSFORM_TYPE_ROTATE,
  GTK_TRANSFORM_TYPE_SCALE
} GtkTransformType;

GDK_AVAILABLE_IN_ALL
GType                   gtk_transform_get_type                  (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_ref                       (GtkTransform                   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_transform_unref                     (GtkTransform                   *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_transform_print                     (GtkTransform                   *self,
                                                                 GString                        *string);
GDK_AVAILABLE_IN_ALL
char *                  gtk_transform_to_string                 (GtkTransform                   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_transform_to_matrix                 (GtkTransform                   *self,
                                                                 graphene_matrix_t              *out_matrix);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_transform_equal                     (GtkTransform                   *first,
                                                                 GtkTransform                   *second) G_GNUC_PURE;

GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_new                       (void);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_identity                  (GtkTransform                   *next);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_transform                 (GtkTransform                   *next,
                                                                 GtkTransform                   *other);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_matrix                    (GtkTransform                   *next,
                                                                 const graphene_matrix_t        *matrix);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_translate                 (GtkTransform                   *next,
                                                                 const graphene_point_t         *point);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_translate_3d              (GtkTransform                   *next,
                                                                 const graphene_point3d_t       *point);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_rotate                    (GtkTransform                   *next,
                                                                 float                           angle);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_rotate_3d                 (GtkTransform                   *next,
                                                                 float                           angle,
                                                                 const graphene_vec3_t          *axis);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_scale                     (GtkTransform                   *next,
                                                                 float                           factor_x,
                                                                 float                           factor_y);
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_scale_3d                  (GtkTransform                   *next,
                                                                 float                           factor_x,
                                                                 float                           factor_y,
                                                                 float                           factor_z);

GDK_AVAILABLE_IN_ALL
GtkTransformType        gtk_transform_get_transform_type        (GtkTransform                   *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
GtkTransform *          gtk_transform_get_next                  (GtkTransform                   *self) G_GNUC_PURE;

G_END_DECLS

#endif /* __GTK_TRANSFORM_H__ */
