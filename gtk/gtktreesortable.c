/* gtktreesortable.c
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


#include "gtktreesortable.h"
#include "gtksignal.h"

static void gtk_tree_sortable_base_init (gpointer g_class);

GtkType
gtk_tree_sortable_get_type (void)
{
  static GtkType tree_sortable_type = 0;

  if (! tree_sortable_type)
    {
      static const GTypeInfo tree_sortable_info =
      {
	sizeof (GtkTreeSortableIface), /* class_size */
	gtk_tree_sortable_base_init,   /* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,
	NULL
      };
      tree_sortable_type = g_type_register_static (G_TYPE_INTERFACE, "GtkTreeSortable", &tree_sortable_info, 0);
      g_type_interface_add_prerequisite (tree_sortable_type, G_TYPE_OBJECT);
    }

  return tree_sortable_type;
}

static void
gtk_tree_sortable_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      g_signal_new ("sort_column_changed",
                    GTK_TYPE_TREE_SORTABLE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeSortableIface, sort_column_changed),
                    NULL, NULL,
                    gtk_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
      initialized = TRUE;
    }
}

void
gtk_tree_sortable_sort_column_changed (GtkTreeSortable *sortable)
{
  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));

  g_signal_emit_by_name (G_OBJECT (sortable),
			 "sort_column_changed");
}

gboolean
gtk_tree_sortable_get_sort_column_id (GtkTreeSortable  *sortable,
				      gint             *sort_column_id,
				      GtkTreeSortOrder *order)
{
  GtkTreeSortableIface *iface;

  g_return_val_if_fail (sortable != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_SORTABLE (sortable), FALSE);

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->get_sort_column_id != NULL, FALSE);

  return (* iface->get_sort_column_id) (sortable, sort_column_id, order);
}

void
gtk_tree_sortable_set_sort_column_id (GtkTreeSortable  *sortable,
				      gint              sort_column_id,
				      GtkTreeSortOrder  order)
{
  GtkTreeSortableIface *iface;

  g_return_if_fail (sortable != NULL);
  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->set_sort_column_id != NULL);
  
  (* iface->set_sort_column_id) (sortable, sort_column_id, order);

}

void
gtk_tree_sortable_set_sort_func (GtkTreeSortable        *sortable,
				 gint                    sort_column_id,
				 GtkTreeIterCompareFunc  func,
				 gpointer                data,
				 GtkDestroyNotify        destroy)
{
  GtkTreeSortableIface *iface;

  g_return_if_fail (sortable != NULL);
  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->set_sort_func != NULL);
  
  (* iface->set_sort_func) (sortable, sort_column_id, func, data, destroy);
}


