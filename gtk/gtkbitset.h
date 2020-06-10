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


#ifndef __GTK_BITSET_H__
#define __GTK_BITSET_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_BITSET (gtk_bitset_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gtk_bitset_get_type                     (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkBitset *             gtk_bitset_ref                          (GtkBitset              *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_unref                        (GtkBitset              *self);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_bitset_contains                     (GtkBitset              *self,
                                                                 guint                   value);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_bitset_is_empty                     (GtkBitset              *self);

GDK_AVAILABLE_IN_ALL
GtkBitset *             gtk_bitset_new_empty                    (void);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_bitset_add                          (GtkBitset              *self,
                                                                 guint                   value);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_bitset_remove                       (GtkBitset              *self,
                                                                 guint                   value);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_add_range                    (GtkBitset              *self,
                                                                 guint                   start,
                                                                 guint                   n_items);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_remove_range                 (GtkBitset              *self,
                                                                 guint                   start,
                                                                 guint                   n_items);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_add_range_closed             (GtkBitset              *self,
                                                                 guint                   first,
                                                                 guint                   last);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_remove_range_closed          (GtkBitset              *self,
                                                                 guint                   first,
                                                                 guint                   last);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_add_rectangle                (GtkBitset              *self,
                                                                 guint                   start,
                                                                 guint                   width,
                                                                 guint                   height,
                                                                 guint                   stride);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_remove_rectangle             (GtkBitset              *self,
                                                                 guint                   start,
                                                                 guint                   width,
                                                                 guint                   height,
                                                                 guint                   stride);

GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_union                        (GtkBitset              *self,
                                                                 const GtkBitset        *other);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_intersect                    (GtkBitset              *self,
                                                                 const GtkBitset        *other);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_subtract                     (GtkBitset              *self,
                                                                 const GtkBitset        *other);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_difference                   (GtkBitset              *self,
                                                                 const GtkBitset        *other);

G_END_DECLS

#endif /* __GTK_BITSET_H__ */
