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


#ifndef __GTK_MATRIX_H__
#define __GTK_MATRIX_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <graphene.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_MATRIX (gtk_matrix_get_type ())

typedef enum
{
  GTK_MATRIX_TYPE_IDENTITY,
  GTK_MATRIX_TYPE_TRANSFORM,
  GTK_MATRIX_TYPE_TRANSLATE,
  GTK_MATRIX_TYPE_ROTATE,
  GTK_MATRIX_TYPE_SCALE
} GtkMatrixType;

GDK_AVAILABLE_IN_ALL
GType                   gtk_matrix_get_type                     (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_ref                          (GtkMatrix                      *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_matrix_unref                        (GtkMatrix                      *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_matrix_print                        (GtkMatrix                      *self,
                                                                 GString                        *string);
GDK_AVAILABLE_IN_ALL
char *                  gtk_matrix_to_string                    (GtkMatrix                      *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_matrix_compute                      (GtkMatrix                      *self,
                                                                 graphene_matrix_t              *out_matrix);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_matrix_equal                        (GtkMatrix                      *first,
                                                                 GtkMatrix                      *second) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_transition                   (GtkMatrix                      *from,
                                                                 GtkMatrix                      *to,
                                                                 double                          progress);

GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_get_identity                 (void) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_identity                     (GtkMatrix                      *next);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_transform                    (GtkMatrix                      *next,
                                                                 const graphene_matrix_t        *matrix);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_translate                    (GtkMatrix                      *next,
                                                                 const graphene_point_t         *point);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_translate_3d                 (GtkMatrix                      *next,
                                                                 const graphene_point3d_t       *point);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_rotate                       (GtkMatrix                      *next,
                                                                 float                           angle);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_rotate_3d                    (GtkMatrix                      *next,
                                                                 float                           angle,
                                                                 const graphene_vec3_t          *axis);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_scale                        (GtkMatrix                      *next,
                                                                 float                           factor_x,
                                                                 float                           factor_y);
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_scale_3d                     (GtkMatrix                      *next,
                                                                 float                           factor_x,
                                                                 float                           factor_y,
                                                                 float                           factor_z);

GDK_AVAILABLE_IN_ALL
GtkMatrixType           gtk_matrix_get_matrix_type              (GtkMatrix                      *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
GtkMatrix *             gtk_matrix_get_next                     (GtkMatrix                      *self) G_GNUC_PURE;

G_END_DECLS

#endif /* __GTK_MATRIX_H__ */
