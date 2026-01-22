/*
 * Copyright © Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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

#define GTK_TYPE_SAVE (gtk_save_get_type ())

GDK_AVAILABLE_IN_4_22
G_DECLARE_FINAL_TYPE (GtkSave, gtk_save, GTK, SAVE, GObject)

GDK_AVAILABLE_IN_4_22
void             gtk_save_insert               (GtkSave    *save,
                                                const char *key,
                                                const char *format_string,
                                                ...);
GDK_AVAILABLE_IN_4_22
void             gtk_save_insert_value         (GtkSave    *save,
                                                const char *key,
                                                GVariant   *value);
GDK_AVAILABLE_IN_4_22
GVariantDict*    gtk_save_get_dict             (GtkSave    *save);

GDK_AVAILABLE_IN_4_22
void             gtk_save_defer                (GtkSave    *save);
GDK_AVAILABLE_IN_4_22
void             gtk_save_complete             (GtkSave    *save);

G_END_DECLS

