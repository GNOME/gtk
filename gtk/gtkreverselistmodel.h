/*
 * Copyright © 2020 Red Hat, Inc
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
 * Authors: Matthias Clasen
 */

#ifndef __GTK_REVERSE_LIST_MODEL_H__
#define __GTK_REVERSE_LIST_MODEL_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_REVERSE_LIST_MODEL (gtk_reverse_list_model_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkReverseListModel, gtk_reverse_list_model, GTK, REVERSE_LIST_MODEL, GObject)

GDK_AVAILABLE_IN_ALL
GtkReverseListModel *   gtk_reverse_list_model_new              (GListModel             *model);

GDK_AVAILABLE_IN_ALL
void                    gtk_reverse_list_model_set_model        (GtkReverseListModel    *self,
                                                                 GListModel             *model);
GDK_AVAILABLE_IN_ALL
GListModel *            gtk_reverse_list_model_get_model        (GtkReverseListModel    *self);

G_END_DECLS

#endif /* __GTK_REVERSE_LIST_MODEL_H__ */
