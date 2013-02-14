/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TREE_VIEW_ACCESSIBLE_PRIVATE_H__
#define __GTK_TREE_VIEW_ACCESSIBLE_PRIVATE_H__

#include <gtk/a11y/gtktreeviewaccessible.h>

G_BEGIN_DECLS

/* called by treeview code */
void            _gtk_tree_view_accessible_reorder       (GtkTreeView       *treeview);
void            _gtk_tree_view_accessible_add           (GtkTreeView       *treeview,
                                                         GtkRBTree         *tree,
                                                         GtkRBNode         *node);
void            _gtk_tree_view_accessible_remove        (GtkTreeView       *treeview,
                                                         GtkRBTree         *tree,
                                                         GtkRBNode         *node);
void            _gtk_tree_view_accessible_changed       (GtkTreeView       *treeview,
                                                         GtkRBTree         *tree,
                                                         GtkRBNode         *node);

void            _gtk_tree_view_accessible_add_column    (GtkTreeView       *treeview,
                                                         GtkTreeViewColumn *column,
                                                         guint              id);
void            _gtk_tree_view_accessible_remove_column (GtkTreeView       *treeview,
                                                         GtkTreeViewColumn *column,
                                                         guint              id);
void            _gtk_tree_view_accessible_reorder_column
                                                        (GtkTreeView       *treeview,
                                                         GtkTreeViewColumn *column);
void            _gtk_tree_view_accessible_toggle_visibility
                                                        (GtkTreeView       *treeview,
                                                         GtkTreeViewColumn *column);
void            _gtk_tree_view_accessible_update_focus_column
                                                        (GtkTreeView       *treeview,
                                                         GtkTreeViewColumn *old_focus,
                                                         GtkTreeViewColumn *new_focus);

void            _gtk_tree_view_accessible_add_state     (GtkTreeView       *treeview,
                                                         GtkRBTree         *tree,
                                                         GtkRBNode         *node,
                                                         GtkCellRendererState state);
void            _gtk_tree_view_accessible_remove_state  (GtkTreeView       *treeview,
                                                         GtkRBTree         *tree,
                                                         GtkRBNode         *node,
                                                         GtkCellRendererState state);

G_END_DECLS

#endif /* __GTK_TREE_VIEW_ACCESSIBLE_PRIVATE_H__ */
