/*
 * Copyright (c) 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GTK_INSPECTOR_LIST_DATA_H_
#define _GTK_INSPECTOR_LIST_DATA_H_

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_LIST_DATA            (gtk_inspector_list_data_get_type())

G_DECLARE_FINAL_TYPE (GtkInspectorListData, gtk_inspector_list_data, GTK, INSPECTOR_LIST_DATA, GtkWidget)

typedef struct _GtkInspectorListData GtkInspectorListData;

G_BEGIN_DECLS

void       gtk_inspector_list_data_set_object (GtkInspectorListData *sl,
                                               GObject              *object);

G_END_DECLS

#endif // _GTK_INSPECTOR_LIST_DATA_H_

// vim: set et sw=2 ts=2:
