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

#ifndef _GTK_INSPECTOR_MISC_INFO_H_
#define _GTK_INSPECTOR_MISC_INFO_H_

#include "gtkscrolledwindow.h"

#define GTK_TYPE_INSPECTOR_MISC_INFO            (gtk_inspector_misc_info_get_type())
#define GTK_INSPECTOR_MISC_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_MISC_INFO, GtkInspectorMiscInfo))
#define GTK_INSPECTOR_MISC_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_MISC_INFO, GtkInspectorMiscInfoClass))
#define GTK_INSPECTOR_IS_MISC_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_MISC_INFO))
#define GTK_INSPECTOR_IS_MISC_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_MISC_INFO))
#define GTK_INSPECTOR_MISC_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_MISC_INFO, GtkInspectorMiscInfoClass))


typedef struct _GtkInspectorMiscInfoPrivate GtkInspectorMiscInfoPrivate;

typedef struct _GtkInspectorMiscInfo
{
  GtkScrolledWindow parent;
  GtkInspectorMiscInfoPrivate *priv;
} GtkInspectorMiscInfo;

typedef struct _GtkInspectorMiscInfoClass
{
  GtkScrolledWindowClass parent;
} GtkInspectorMiscInfoClass;

G_BEGIN_DECLS

GType gtk_inspector_misc_info_get_type   (void);
void  gtk_inspector_misc_info_set_object (GtkInspectorMiscInfo *sl,
                                          GObject              *object);

G_END_DECLS

#endif // _GTK_INSPECTOR_MISC_INFO_H_

// vim: set et sw=2 ts=2:
