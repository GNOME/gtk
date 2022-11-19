/*
 * Copyright © 2021 Benjamin Otte
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

#ifndef __GTK_FLIPPER_H__
#define __GTK_FLIPPER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_FLIPPER         (gtk_flipper_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkFlipper, gtk_flipper, GTK, FLIPPER, GtkWidget)

GDK_AVAILABLE_IN_4_10
GtkWidget *     gtk_flipper_new                                 (GtkWidget              *child);

GDK_AVAILABLE_IN_4_10
GtkWidget *     gtk_flipper_get_child                           (GtkFlipper             *self);
GDK_AVAILABLE_IN_4_10
void            gtk_flipper_set_child                           (GtkFlipper             *self,
                                                                 GtkWidget              *child);
GDK_AVAILABLE_IN_4_10
gboolean        gtk_flipper_get_flip_horizontal                 (GtkFlipper             *self);
GDK_AVAILABLE_IN_4_10
void            gtk_flipper_set_flip_horizontal                 (GtkFlipper             *self,
                                                                 gboolean                flip_horizontal);
GDK_AVAILABLE_IN_4_10
gboolean        gtk_flipper_get_flip_vertical                   (GtkFlipper             *self);
GDK_AVAILABLE_IN_4_10
void            gtk_flipper_set_flip_vertical                   (GtkFlipper             *self,
                                                                 gboolean                flip_vertical);
GDK_AVAILABLE_IN_4_10
gboolean        gtk_flipper_get_rotate                          (GtkFlipper             *self);
GDK_AVAILABLE_IN_4_10
void            gtk_flipper_set_rotate                          (GtkFlipper             *self,
                                                                 gboolean                rotate);


G_END_DECLS

#endif  /* __GTK_FLIPPER_H__ */
