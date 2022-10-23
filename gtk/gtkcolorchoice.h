/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2022 Red Hat, Inc.
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_CHOICE (gtk_color_choice_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkColorChoice, gtk_color_choice, GTK, COLOR_CHOICE, GObject)

GDK_AVAILABLE_IN_4_10
GtkColorChoice * gtk_color_choice_new                 (void);

GDK_AVAILABLE_IN_4_10
GtkWindow *      gtk_color_choice_get_parent          (GtkColorChoice       *self);

GDK_AVAILABLE_IN_4_10
void             gtk_color_choice_set_parent          (GtkColorChoice       *self,
                                                       GtkWindow            *window);

GDK_AVAILABLE_IN_4_10
const char *     gtk_color_choice_get_title           (GtkColorChoice       *self);

GDK_AVAILABLE_IN_4_10
void             gtk_color_choice_set_title           (GtkColorChoice       *self,
                                                       const char           *title);

GDK_AVAILABLE_IN_4_10
void             gtk_color_choice_set_color           (GtkColorChoice       *self,
                                                       GdkRGBA              *color);

GDK_AVAILABLE_IN_4_10
void             gtk_color_choice_get_color           (GtkColorChoice       *self,
                                                       GdkRGBA              *color);

GDK_AVAILABLE_IN_4_10
gboolean         gtk_color_choice_get_use_alpha       (GtkColorChoice       *self);

GDK_AVAILABLE_IN_4_10
void             gtk_color_choice_set_use_alpha       (GtkColorChoice       *self,
                                                       gboolean              use_alpha);

GDK_AVAILABLE_IN_4_10
void             gtk_color_choice_present             (GtkColorChoice       *self,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

GDK_AVAILABLE_IN_4_10
GdkRGBA *        gtk_color_choice_present_finish     (GtkColorChoice        *self,
                                                      GAsyncResult          *result,
                                                      GError               **error);

G_END_DECLS
