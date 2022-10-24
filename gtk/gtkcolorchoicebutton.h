/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_CHOICE_BUTTON (gtk_color_choice_button_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkColorChoiceButton, gtk_color_choice_button, GTK, COLOR_CHOICE_BUTTON, GtkWidget)

GDK_AVAILABLE_IN_4_10
GtkColorChoiceButton *
                gtk_color_choice_button_new             (void);

GDK_AVAILABLE_IN_4_10
void            gtk_color_choice_button_set_color       (GtkColorChoiceButton *self,
                                                         const GdkRGBA        *color);

GDK_AVAILABLE_IN_4_10
const GdkRGBA * gtk_color_choice_button_get_color       (GtkColorChoiceButton *self);

GDK_AVAILABLE_IN_4_10
void            gtk_color_choice_button_set_use_alpha   (GtkColorChoiceButton *self,
                                                         gboolean              use_alpha);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_color_choice_button_get_use_alpha   (GtkColorChoiceButton *self);

GDK_AVAILABLE_IN_4_10
void            gtk_color_choice_button_set_title       (GtkColorChoiceButton *self,
                                                         const char           *title);

GDK_AVAILABLE_IN_4_10
const char *    gtk_color_choice_button_get_title       (GtkColorChoiceButton *self);

G_END_DECLS
