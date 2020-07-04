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

#ifndef __GTK_ARRAY_STORE2_H__
#define __GTK_ARRAY_STORE2_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ARRAY_STORE2 (gtk_array_store2_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE(GtkArrayStore2, gtk_array_store2, GTK, ARRAY_STORE2, GObject)

GDK_AVAILABLE_IN_ALL
GtkArrayStore2 *         gtk_array_store2_new                             (GType           item_type);

GDK_AVAILABLE_IN_ALL
void                    gtk_array_store2_append                          (GtkArrayStore2  *store,
                                                                         gpointer        item);

GDK_AVAILABLE_IN_ALL
void                    gtk_array_store2_remove_all                      (GtkArrayStore2  *store);

GDK_AVAILABLE_IN_ALL
void                    gtk_array_store2_splice                          (GtkArrayStore2  *store,
                                                                         guint           position,
                                                                         guint           n_removals,
                                                                         gpointer       *additions,
                                                                         guint           n_additions);

G_END_DECLS

#endif /* __GTK_ARRAY_STORE2_H__ */
