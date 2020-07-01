/*
 * Copyright © 2020 Red Hat, Inc.
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

#ifndef __GTK_STRING_LIST2_H__
#define __GTK_STRING_LIST2_H__

#include <gio/gio.h>
/* for GDK_AVAILABLE_IN_ALL */
#include <gdk/gdk.h>


G_BEGIN_DECLS

#define GTK_TYPE_STRING_LIST2 (gtk_string_list2_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkStringList2, gtk_string_list2, GTK, STRING_LIST2, GObject)

GDK_AVAILABLE_IN_ALL
GtkStringList2 * gtk_string_list2_new             (const char * const    *strings);

GDK_AVAILABLE_IN_ALL
void            gtk_string_list2_append          (GtkStringList2         *self,
                                                 const char            *string);

GDK_AVAILABLE_IN_ALL
void            gtk_string_list2_take            (GtkStringList2         *self,
                                                 char                  *string);

GDK_AVAILABLE_IN_ALL
void            gtk_string_list2_remove          (GtkStringList2         *self,
                                                 guint                  position);

GDK_AVAILABLE_IN_ALL
void            gtk_string_list2_splice          (GtkStringList2         *self,
                                                 guint                  position,
                                                 guint                  n_removals,
                                                 const char * const    *additions);

GDK_AVAILABLE_IN_ALL
const char *    gtk_string_list2_get_string      (GtkStringList2         *self,
                                                 guint                  position);

G_END_DECLS

#endif /* __GTK_STRING_LIST2_H__ */
