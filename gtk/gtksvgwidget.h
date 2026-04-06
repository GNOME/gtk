/*
 * Copyright © 2026 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#pragma once

#include <gtk/gtkwidget.h>
#include <gtk/gtkeventcontroller.h>

G_BEGIN_DECLS

#define GTK_TYPE_SVG_WIDGET (gtk_svg_widget_get_type ())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkSvgWidget, gtk_svg_widget, GTK, SVG_WIDGET, GtkWidget)

GDK_AVAILABLE_IN_4_24
GtkSvgWidget * gtk_svg_widget_new               (void);

GDK_AVAILABLE_IN_4_24
void           gtk_svg_widget_load_from_bytes   (GtkSvgWidget       *self,
                                                 GBytes             *bytes);

GDK_AVAILABLE_IN_4_24
void           gtk_svg_widget_set_stylesheet    (GtkSvgWidget       *self,
                                                 GBytes             *bytes);

GDK_AVAILABLE_IN_4_24
GBytes *       gtk_svg_widget_get_stylesheet    (GtkSvgWidget       *self);

GDK_AVAILABLE_IN_4_24
void             gtk_svg_widget_set_state       (GtkSvgWidget       *self,
                                                 unsigned int        state);

GDK_AVAILABLE_IN_4_24
unsigned int     gtk_svg_widget_get_state       (GtkSvgWidget       *self);

G_END_DECLS
