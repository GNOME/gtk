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

#ifndef __GTK_LIST_VIEW_H__
#define __GTK_LIST_VIEW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

/**
 * GtkListCreateWidgetFunc:
 * @user_data: (closure): user data
 *
 * Called whenever a new widget needs to be created for managing a row in
 * the list.
 *
 * The widget will later be bound to an item via the #GtkListBindWidgetFunc.
 *
 * Returns: (transfer full): a #GtkWidget
 */
typedef GtkWidget * (* GtkListCreateWidgetFunc) (gpointer user_data);

/**
 * GtkListBindWidgetFunc:
 * @widget: The #GtkWidget to bind
 * @item: (type GObject) (allow-none): item to bind or %NULL to unbind
 *     the widget.
 * @user_data: (closure): user data
 *
 * Binds a widget previously created via a #GtkListCreateWidgetFunc to
 * an @item.
 *
 * Rebinding a @widget to different @items is supported as well as
 * unbinding it by setting @item to %NULL.
 */
typedef void (* GtkListBindWidgetFunc) (GtkWidget *widget,
                                        gpointer   item,
                                        gpointer   user_data);

#define GTK_TYPE_LIST_VIEW         (gtk_list_view_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkListView, gtk_list_view, GTK, LIST_VIEW, GtkWidget)

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_list_view_new                               (void);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_list_view_get_model                         (GtkListView            *self);
GDK_AVAILABLE_IN_ALL
void            gtk_list_view_set_model                         (GtkListView            *self,
                                                                 GListModel             *model);
GDK_AVAILABLE_IN_ALL
void            gtk_list_view_set_functions                     (GtkListView            *self,
                                                                 GtkListCreateWidgetFunc create_func,
                                                                 GtkListBindWidgetFunc   bind_func,
                                                                 gpointer                user_data,
                                                                 GDestroyNotify          user_destroy);

G_END_DECLS

#endif  /* __GTK_LIST_VIEW_H__ */
