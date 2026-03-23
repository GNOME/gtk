/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileCopyrightText: 2019 Benjamin Otte <otte@gnome.org>
 * SPDX-FileCopyrightText: 2026 Jamie Gravendeel <me@jamie.garden>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkexpression.h>
#include <gtk/gtkfilter.h>

G_BEGIN_DECLS

#define GTK_TYPE_ENUM_FILTER (gtk_enum_filter_get_type ())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkEnumFilter, gtk_enum_filter, GTK, ENUM_FILTER, GtkFilter)

GDK_AVAILABLE_IN_4_24
GtkEnumFilter *gtk_enum_filter_new            (GType          enum_type,
                                               GtkExpression *expression);
GDK_AVAILABLE_IN_4_24
GtkExpression *gtk_enum_filter_get_expression (GtkEnumFilter *self);
GDK_AVAILABLE_IN_4_24
void           gtk_enum_filter_set_expression (GtkEnumFilter *self,
                                               GtkExpression *expression);
GDK_AVAILABLE_IN_4_24
long           gtk_enum_filter_get_value      (GtkEnumFilter *self);
GDK_AVAILABLE_IN_4_24
void           gtk_enum_filter_set_value      (GtkEnumFilter *self,
                                               long           value);

G_END_DECLS
