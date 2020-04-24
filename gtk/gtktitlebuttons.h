/*
 * Copyright (c) 2020 Alexander Mikhaylenko <alexm@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_TITLE_BUTTONS (gtk_title_buttons_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkTitleButtons, gtk_title_buttons, GTK, TITLE_BUTTONS, GtkWidget)

GDK_AVAILABLE_IN_ALL
GtkWidget *   gtk_title_buttons_new                   (GtkPackType pack_type);

GDK_AVAILABLE_IN_ALL
GtkPackType   gtk_title_buttons_get_pack_type         (GtkTitleButtons *self);

GDK_AVAILABLE_IN_ALL
void          gtk_title_buttons_set_pack_type         (GtkTitleButtons *self,
                                                       GtkPackType      pack_type);

GDK_AVAILABLE_IN_ALL
const gchar * gtk_title_buttons_get_decoration_layout (GtkTitleButtons *self);

GDK_AVAILABLE_IN_ALL
void          gtk_title_buttons_set_decoration_layout (GtkTitleButtons *self,
                                                       const gchar     *layout);

GDK_AVAILABLE_IN_ALL
gboolean      gtk_title_buttons_get_empty             (GtkTitleButtons *self);

G_END_DECLS
