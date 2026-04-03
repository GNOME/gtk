/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined(__GTK_H_INSIDE__) && !defined(GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib-object.h>
#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_ENUM_LIST_ITEM (gtk_enum_list_item_get_type ())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkEnumListItem, gtk_enum_list_item, GTK, ENUM_LIST_ITEM, GObject)

GDK_AVAILABLE_IN_4_24
int             gtk_enum_list_item_get_value    (GtkEnumListItem *self);

GDK_AVAILABLE_IN_4_24
const char *    gtk_enum_list_item_get_name     (GtkEnumListItem *self);

GDK_AVAILABLE_IN_4_24
const char *    gtk_enum_list_item_get_nick     (GtkEnumListItem *self);

#define GTK_TYPE_ENUM_LIST (gtk_enum_list_get_type ())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkEnumList, gtk_enum_list, GTK, ENUM_LIST, GObject)

GDK_AVAILABLE_IN_4_24
GtkEnumList *   gtk_enum_list_new               (GType        enum_type) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_4_24
GType           gtk_enum_list_get_enum_type     (GtkEnumList *self);

GDK_AVAILABLE_IN_4_24
unsigned int    gtk_enum_list_find              (GtkEnumList *self,
                                                 int          value);

G_END_DECLS
