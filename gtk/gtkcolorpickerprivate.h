/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2018 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_COLOR_PICKER_H__
#define __GTK_COLOR_PICKER_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS


#define GTK_TYPE_COLOR_PICKER (gtk_color_picker_get_type ())
G_DECLARE_INTERFACE (GtkColorPicker, gtk_color_picker, GTK, COLOR_PICKER, GObject)

struct _GtkColorPickerInterface {
  GTypeInterface g_iface;

  void (* pick)             (GtkColorPicker      *picker,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data);

  GdkRGBA * (* pick_finish) (GtkColorPicker      *picker,
                             GAsyncResult        *res,
                             GError             **error);
};

GtkColorPicker * gtk_color_picker_new         (void);
void             gtk_color_picker_pick        (GtkColorPicker       *picker,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
GdkRGBA *        gtk_color_picker_pick_finish (GtkColorPicker       *picker,
                                               GAsyncResult         *res,
                                               GError              **error);

G_END_DECLS

#endif  /* __GTK_COLOR_PICKER_H__ */
