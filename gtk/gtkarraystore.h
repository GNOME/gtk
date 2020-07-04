/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_ARRAY_STORE_H__
#define __GTK_ARRAY_STORE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ARRAY_STORE (gtk_array_store_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE(GtkArrayStore, gtk_array_store, GTK, ARRAY_STORE, GObject)

GDK_AVAILABLE_IN_ALL
GtkArrayStore *         gtk_array_store_new                             (void);

GDK_AVAILABLE_IN_ALL
void                    gtk_array_store_append                          (GtkArrayStore  *store,
                                                                         gpointer        item);

GDK_AVAILABLE_IN_ALL
void                    gtk_array_store_remove_all                      (GtkArrayStore  *store);

GDK_AVAILABLE_IN_ALL
void                    gtk_array_store_splice                          (GtkArrayStore  *store,
                                                                         guint           position,
                                                                         guint           n_removals,
                                                                         gpointer       *additions,
                                                                         guint           n_additions);

G_END_DECLS

#endif /* __GTK_ARRAY_STORE_H__ */
