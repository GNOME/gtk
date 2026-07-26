/*
 * Copyright © 2018 Benjamin Otte
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


#pragma once

#include <gio/gio.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_LIST_LIST_MODEL         (gtk_list_list_model_get_type ())
GDK_DECLARE_INTERNAL_TYPE (GtkListListModel, gtk_list_list_model, GTK, LIST_LIST_MODEL, GObject);

GtkListListModel *      gtk_list_list_model_new                 (gpointer                (* get_first) (gpointer),
                                                                 gpointer                (* get_next) (gpointer, gpointer),
                                                                 gpointer                (* get_previous) (gpointer, gpointer),
                                                                 gpointer                (* get_last) (gpointer),
                                                                 gpointer                (* get_item) (gpointer, gpointer),
                                                                 gpointer                data,
                                                                 GDestroyNotify          notify);

GtkListListModel *      gtk_list_list_model_new_with_size       (guint                   n_items,
                                                                 gpointer                (* get_first) (gpointer),
                                                                 gpointer                (* get_next) (gpointer, gpointer),
                                                                 gpointer                (* get_previous) (gpointer, gpointer),
                                                                 gpointer                (* get_last) (gpointer),
                                                                 gpointer                (* get_item) (gpointer, gpointer),
                                                                 gpointer                data,
                                                                 GDestroyNotify          notify);

void                    gtk_list_list_model_item_added          (GtkListListModel       *self,
                                                                 gpointer                item);
void                    gtk_list_list_model_item_added_at       (GtkListListModel       *self,
                                                                 guint                   position);
void                    gtk_list_list_model_item_removed        (GtkListListModel       *self,
                                                                 gpointer                previous);
void                    gtk_list_list_model_item_replaced       (GtkListListModel       *self,
                                                                 gpointer                new_item);
void                    gtk_list_list_model_item_removed_at     (GtkListListModel       *self,
                                                                 guint                   position);
void                    gtk_list_list_model_item_moved          (GtkListListModel       *self,
                                                                 gpointer                item,
                                                                 gpointer                previous_previous);

void                    gtk_list_list_model_clear               (GtkListListModel       *self);

G_END_DECLS
