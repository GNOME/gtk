/* gtktreedatalist.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GTK_TREE_DATA_LIST_H__
#define __GTK_TREE_DATA_LIST_H__

#include <glib.h>
#include <gobject/gobject.h>

typedef struct _GtkTreeDataList GtkTreeDataList;
struct _GtkTreeDataList
{
  GtkTreeDataList *next;

  union {
    gint	   v_int;
    gint8          v_char;
    guint8         v_uchar;
    guint	   v_uint;
    gfloat	   v_float;
    gpointer	   v_pointer;
  } data;
};

/* FIXME: s/gtk/_gtk/g to make internal */
void             gtk_tree_data_list_push_allocator (GAllocator      *allocator);
void             gtk_tree_data_list_pop_allocator  (void);
GtkTreeDataList *gtk_tree_data_list_alloc          (void);
void             gtk_tree_data_list_free           (GtkTreeDataList *list);

void             gtk_tree_data_list_node_to_value  (GtkTreeDataList   *list,
						    GType              type,
						    GValue            *value);
void             gtk_tree_data_list_value_to_node  (GtkTreeDataList   *list,
						    GValue            *value);


#endif /* __GTK_TREE_DATA_LIST_H__ */
