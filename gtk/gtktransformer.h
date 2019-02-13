/*
 * Copyright © 2019 Timm Bäder
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

#ifndef __GTK_TRANSFORMER_H__
#define __GTK_TRANSFORMER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_TRANSFORMER         (gtk_transformer_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkTransformer, gtk_transformer, GTK, TRANSFORMER, GtkWidget);

GDK_AVAILABLE_IN_ALL
GtkWidget *           gtk_transformer_new              (GtkWidget               *child);

GDK_AVAILABLE_IN_ALL
void                  gtk_transformer_set_transform    (GtkTransformer          *self,
                                                        const graphene_matrix_t *transform);


G_END_DECLS

#endif  /* __GTK_TRANSFORMER_H__ */
