/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_CHOOSER_WINDOW (gtk_color_chooser_window_get_type ())

G_DECLARE_FINAL_TYPE (GtkColorChooserWindow, gtk_color_chooser_window, GTK, COLOR_CHOOSER_WINDOW, GtkWindow)

GtkWidget * gtk_color_chooser_window_new      (const char *title,
                                               GtkWindow   *parent);
void gtk_color_chooser_window_save_color (GtkColorChooserWindow *self);
GtkWidget *gtk_color_chooser_window_get_ok_button (GtkColorChooserWindow *self);
GtkWidget *gtk_color_chooser_window_get_cancel_button (GtkColorChooserWindow *self);

G_END_DECLS
