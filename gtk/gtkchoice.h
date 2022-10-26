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

G_BEGIN_DECLS

#define GTK_TYPE_CHOICE (gtk_choice_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkChoice, gtk_choice, GTK, CHOICE, GObject)

GDK_AVAILABLE_IN_4_10
GtkChoice *     gtk_choice_new (const char         *label,
                                const char * const *options);

GDK_AVAILABLE_IN_4_10
const char *    gtk_choice_get_label (GtkChoice *choice);

GDK_AVAILABLE_IN_4_10
const char * const *
                gtk_choice_get_options (GtkChoice *choice);

G_END_DECLS
