/* gtktreesortable.h
 * Copyright (C) 2001  Red Hat, Inc.
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

#ifndef __GTK_TREE_SORTABLE_H__
#define __GTK_TREE_SORTABLE_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum
{
  GTK_TREE_SORT_NONE,
  GTK_TREE_SORT_ASCENDING,
  GTK_TREE_SORT_DESCENDING
} GtkTreeSortOrder;

#define GTK_TYPE_TREE_SORTABLE            (gtk_tree_sortable_get_type ())
#define GTK_TREE_SORTABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_SORTABLE, GtkTreeSortable))
#define GTK_TREE_SORTABLE_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), GTK_TYPE_TREE_SORTABLE, GtkTreeSortableIface))
#define GTK_IS_TREE_SORTABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_SORTABLE))
#define GTK_TREE_SORTABLE_GET_IFACE(obj)  ((GtkTreeSortableIface *)g_type_interface_peek (((GTypeInstance *)GTK_TREE_SORTABLE (obj))->g_class, GTK_TYPE_TREE_SORTABLE))


typedef struct _GtkTreeSortable      GtkTreeSortable; /* Dummy typedef */
typedef struct _GtkTreeSortableIface GtkTreeSortableIface;

typedef gint (* GtkTreeIterCompareFunc) (GtkTreeModel *model,
					 GtkTreeIter  *a,
					 GtkTreeIter  *b,
					 gpointer      user_data);


struct _GtkTreeSortableIface
{
  GTypeInterface g_iface;

  /* signals */
  void (* sort_column_changed) (GtkTreeSortable  *sortable);

  /* virtual table */
  gboolean (* get_sort_column_id)  (GtkTreeSortable  *sortable,
				    gint             *sort_column_id,
				    GtkTreeSortOrder *order);
  void (* set_sort_column_id)      (GtkTreeSortable  *sortable,
				    gint              sort_column_id,
				    GtkTreeSortOrder  order);
  void (* sort_column_id_set_func) (GtkTreeSortable  *sortable,
				    gint              sort_column_id,
				    GtkTreeIterCompareFunc func,
				    gpointer          data,
				    GtkDestroyNotify  destroy);
};


GType    gtk_tree_sortable_get_type         (void) G_GNUC_CONST;

gboolean gtk_tree_sortable_get_sort_id      (GtkTreeSortable        *sortable,
					     gint                   *sort_column_id,
					     GtkTreeSortOrder       *order);
void     gtk_tree_sortable_set_sort_id      (GtkTreeSortable        *sortable,
					     gint                    sort_column_id,
					     GtkTreeSortOrder        order);
void     gtk_tree_sortable_sort_id_set_func (GtkTreeSortable        *sortable,
					     gint                    sort_column_id,
					     GtkTreeIterCompareFunc  func,
					     gpointer                data,
					     GtkDestroyNotify        destroy);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_SORTABLE_H__ */
