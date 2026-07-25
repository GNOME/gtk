/*
 * Copyright © 2026 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */


#pragma once

#include <gio/gio.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_ARRAY_LIST_MODEL         (gtk_array_list_model_get_type ())
GDK_DECLARE_INTERNAL_TYPE (GtkArrayListModel, gtk_array_list_model, GTK, ARRAY_LIST_MODEL, GObject);

GtkArrayListModel * gtk_array_list_model_new             (GPtrArray         *array,
                                                          gpointer           data,
                                                          GDestroyNotify     notify);

void                gtk_array_list_model_item_added      (GtkArrayListModel *self);
void                gtk_array_list_model_item_inserted   (GtkArrayListModel *self,
                                                          unsigned int       position);
void                gtk_array_list_model_item_removed    (GtkArrayListModel *self,
                                                          unsigned int       position);
void                gtk_array_list_model_item_moved      (GtkArrayListModel *self,
                                                          unsigned int       previous_position,
                                                          unsigned int       position);
void                gtk_array_list_model_sorted          (GtkArrayListModel *self);

void                gtk_array_list_model_clear           (GtkArrayListModel *self);

G_END_DECLS
