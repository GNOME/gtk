/*
 * Copyright © 2018 Benjamin Otte
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

#ifndef __GSK_DIFF_PRIVATE_H__
#define __GSK_DIFF_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef void (* GskKeepFunc)   (gconstpointer elem1, gconstpointer elem2, gpointer data);
typedef void (* GskDeleteFunc) (gconstpointer elem, gsize idx, gpointer data);
typedef void (* GskInsertFunc) (gconstpointer elem, gsize idx, gpointer data);

typedef struct _GskDiffSettings GskDiffSettings;

GskDiffSettings *       gsk_diff_settings_new                   (GCompareDataFunc        compare_func,
                                                                 GskKeepFunc             keep_func,
                                                                 GskDeleteFunc           delete_func,
                                                                 GskInsertFunc           insert_func);
void                    gsk_diff_settings_free                  (GskDiffSettings        *settings);

void                    gsk_diff                                (gconstpointer          *elem1,
                                                                 gsize                   n1,
                                                                 gconstpointer          *elem2,
                                                                 gsize                   n2,
                                                                 const GskDiffSettings  *settings,
                                                                 gpointer                data);

G_END_DECLS

#endif /* __GSK_DIFF_PRIVATE_H__ */
