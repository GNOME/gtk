/*
 * Copyright © 2019 Red Hat, Inc.
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

#ifndef __GTK_SEARCH_LIST_MODEL_H__
#define __GTK_SEARCH_LIST_MODEL_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
#include <gtk/gtkfilter.h>


G_BEGIN_DECLS

#define GTK_TYPE_SEARCH_LIST_MODEL (gtk_search_list_model_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSearchListModel, gtk_search_list_model, GTK, SEARCH_LIST_MODEL, GObject)

GDK_AVAILABLE_IN_ALL
GtkSearchListModel *    gtk_search_list_model_new               (GListModel             *model,
                                                                 GtkFilter              *filter);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_search_list_model_next_match        (GtkSearchListModel     *self);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_search_list_model_previous_match    (GtkSearchListModel     *self);

G_END_DECLS

#endif /* __GTK_SEARCH_LIST_MODEL_H__ */
