/*
 * Copyright © 2011 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_BITMASK_PRIVATE_H__
#define __GTK_BITMASK_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

#ifdef GTK_INSIDE_BITMASK_C
typedef GArray GtkBitmask;
#else
typedef struct _GtkBitmask           GtkBitmask;
#endif


GtkBitmask *   _gtk_bitmask_new                  (void);
GtkBitmask *   _gtk_bitmask_copy                 (const GtkBitmask  *mask);
void           _gtk_bitmask_free                 (GtkBitmask        *mask);

char *         _gtk_bitmask_to_string            (const GtkBitmask  *mask);
void           _gtk_bitmask_print                (const GtkBitmask  *mask,
                                                  GString           *string);

void           _gtk_bitmask_intersect            (GtkBitmask       **mask,
                                                  const GtkBitmask  *other);
void           _gtk_bitmask_union                (GtkBitmask       **mask,
                                                  const GtkBitmask  *other);
void           _gtk_bitmask_subtract             (GtkBitmask       **mask,
                                                  const GtkBitmask  *other);

gboolean       _gtk_bitmask_get                  (const GtkBitmask  *mask,
                                                  guint              index_);
void           _gtk_bitmask_set                  (GtkBitmask       **mask,
                                                  guint              index_,
                                                  gboolean           value);
void           _gtk_bitmask_clear                (GtkBitmask       **mask);
guint          _gtk_bitmask_get_uint             (const GtkBitmask  *mask,
                                                  guint              index_);
void           _gtk_bitmask_set_uint             (GtkBitmask       **mask,
                                                  guint              index_,
						  guint              bits);

void           _gtk_bitmask_invert_range         (GtkBitmask       **mask,
                                                  guint              start,
                                                  guint              end);

gboolean       _gtk_bitmask_is_empty             (const GtkBitmask  *mask);
gboolean       _gtk_bitmask_equals               (const GtkBitmask  *mask,
                                                  const GtkBitmask  *other);
gboolean       _gtk_bitmask_intersects           (const GtkBitmask  *mask,
                                                  const GtkBitmask  *other);
guint          _gtk_bitmask_hash                 (const GtkBitmask  *mask);
gboolean       _gtk_bitmask_find_next_set        (const GtkBitmask  *mask,
						  guint             *pos);

G_END_DECLS

#endif /* __GTK_BITMASK_PRIVATE_H__ */
