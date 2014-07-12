/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#ifndef _GTK_INSPECTOR_DATA_LIST_H_
#define _GTK_INSPECTOR_DATA_LIST_H_

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_DATA_LIST            (gtk_inspector_data_list_get_type())
#define GTK_INSPECTOR_DATA_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_DATA_LIST, GtkInspectorDataList))
#define GTK_INSPECTOR_DATA_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_DATA_LIST, GtkInspectorDataListClass))
#define GTK_INSPECTOR_IS_DATA_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_DATA_LIST))
#define GTK_INSPECTOR_IS_DATA_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_DATA_LIST))
#define GTK_INSPECTOR_DATA_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_DATA_LIST, GtkInspectorDataListClass))


typedef struct _GtkInspectorDataListPrivate GtkInspectorDataListPrivate;

typedef struct _GtkInspectorDataList
{
  GtkBox parent;
  GtkInspectorDataListPrivate *priv;
} GtkInspectorDataList;

typedef struct _GtkInspectorDataListClass
{
  GtkBoxClass parent;
} GtkInspectorDataListClass;

G_BEGIN_DECLS

GType      gtk_inspector_data_list_get_type   (void);
void       gtk_inspector_data_list_set_object (GtkInspectorDataList *sl,
                                               GObject              *object);

G_END_DECLS

#endif // _GTK_INSPECTOR_DATA_LIST_H_

// vim: set et sw=2 ts=2:
