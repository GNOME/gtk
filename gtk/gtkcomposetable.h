/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Takao Fujiwara <takao.fujiwara1@gmail.com>
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

#ifndef __GTK_COMPOSETABLE_H__
#define __GTK_COMPOSETABLE_H__

#include <glib.h>


G_BEGIN_DECLS

typedef struct _GtkComposeTable GtkComposeTable;
typedef struct _GtkComposeTableCompact GtkComposeTableCompact;

struct _GtkComposeTable
{
  guint16 *data;
  gint max_seq_len;
  gint n_seqs;
  guint32 id;
};

struct _GtkComposeTableCompact
{
  const guint16 *data;
  gint max_seq_len;
  gint n_index_size;
  gint n_index_stride;
};

GtkComposeTable * gtk_compose_table_new_with_file (const gchar   *compose_file);
GSList *gtk_compose_table_list_add_array          (GSList        *compose_tables,
                                                   const guint16 *data,
                                                   gint           max_seq_len,
                                                   gint           n_seqs);
GSList *gtk_compose_table_list_add_file           (GSList        *compose_tables,
                                                   const gchar   *compose_file);

G_END_DECLS

#endif /* __GTK_COMPOSETABLE_H__ */
