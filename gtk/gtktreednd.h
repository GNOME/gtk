/* gtktreednd.h
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

#ifndef __GTK_TREE_DND_H__
#define __GTK_TREE_DND_H__

#include <gtk/gtktreemodel.h>
#include <gtk/gtkdnd.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_DRAG_SOURCE            (gtk_tree_drag_source_get_type ())
#define GTK_TREE_DRAG_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_DRAG_SOURCE, GtkTreeDragSource))
#define GTK_IS_TREE_DRAG_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_DRAG_SOURCE))
#define GTK_TREE_DRAG_SOURCE_GET_IFACE(obj)  ((GtkTreeDragSourceIface *)g_type_interface_peek (((GTypeInstance *)GTK_TREE_DRAG_SOURCE (obj))->g_class, GTK_TYPE_TREE_DRAG_SOURCE))

typedef struct _GtkTreeDragSource      GtkTreeDragSource; /* Dummy typedef */
typedef struct _GtkTreeDragSourceIface GtkTreeDragSourceIface;

struct _GtkTreeDragSourceIface
{
  GTypeInterface g_iface;

  /* VTable - not signals */

  gboolean     (* drag_data_get)        (GtkTreeDragSource   *dragsource,
                                         GtkTreePath         *path,
                                         GtkSelectionData    *selection_data);

  gboolean     (* drag_data_delete)     (GtkTreeDragSource *dragsource,
                                         GtkTreePath       *path);
};

GType           gtk_tree_drag_source_get_type   (void) G_GNUC_CONST;

/* Deletes the given row, or returns FALSE if it can't */
gboolean gtk_tree_drag_source_drag_data_delete (GtkTreeDragSource *drag_source,
                                                GtkTreePath       *path);

/* Fills in selection_data with type selection_data->target based on
 * the row denoted by path, returns TRUE if it does anything
 */
gboolean gtk_tree_drag_source_drag_data_get    (GtkTreeDragSource *drag_source,
                                                GtkTreePath       *path,
                                                GtkSelectionData  *selection_data);

#define GTK_TYPE_TREE_DRAG_DEST            (gtk_tree_drag_dest_get_type ())
#define GTK_TREE_DRAG_DEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_DRAG_DEST, GtkTreeDragDest))
#define GTK_IS_TREE_DRAG_DEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_DRAG_DEST))
#define GTK_TREE_DRAG_DEST_GET_IFACE(obj)  ((GtkTreeDragDestIface *)g_type_interface_peek (((GTypeInstance *)GTK_TREE_DRAG_DEST (obj))->g_class, GTK_TYPE_TREE_DRAG_DEST))

typedef struct _GtkTreeDragDest      GtkTreeDragDest; /* Dummy typedef */
typedef struct _GtkTreeDragDestIface GtkTreeDragDestIface;

struct _GtkTreeDragDestIface
{
  GTypeInterface g_iface;

  /* VTable - not signals */

  gboolean     (* drag_data_received) (GtkTreeDragDest   *dragdest,
                                       GtkTreePath       *dest,
                                       GtkSelectionData  *selection_data);

};

GType           gtk_tree_drag_dest_get_type   (void) G_GNUC_CONST;

/* Inserts a row before dest which contains data in selection_data,
 * or returns FALSE if it can't
 */
gboolean gtk_tree_drag_dest_drag_data_received (GtkTreeDragDest  *drag_dest,
                                                GtkTreePath      *dest,
                                                GtkSelectionData *selection_data);

/* The selection data would normally have target type GTK_TREE_MODEL_ROW in this
 * case. If the target is wrong these functions return FALSE.
 */
gboolean gtk_selection_data_set_tree_row     (GtkSelectionData         *selection_data,
                                              GtkTreeModel             *tree_model,
                                              GtkTreePath              *path);
gboolean gtk_selection_data_get_tree_row     (GtkSelectionData         *selection_data,
                                              GtkTreeModel           **tree_model,
                                              GtkTreePath             **path);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_DND_H__ */
