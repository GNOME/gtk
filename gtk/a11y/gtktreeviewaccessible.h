/* GAIL - The GNOME Accessibility Implementation Library
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

#ifndef __GTK_TREE_VIEW_ACCESSIBLE_H__
#define __GTK_TREE_VIEW_ACCESSIBLE_H__

#include "gtkcontaineraccessible.h"
#include "gtktreeprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_TREE_VIEW_ACCESSIBLE                  (_gtk_tree_view_accessible_get_type ())
#define GTK_TREE_VIEW_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_VIEW_ACCESSIBLE, GtkTreeViewAccessible))
#define GTK_TREE_VIEW_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_VIEW_ACCESSIBLE, GtkTreeViewAccessibleClass))
#define GTK_IS_TREE_VIEW_ACCESSIBLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_VIEW_ACCESSIBLE))
#define GTK_IS_TREE_VIEW_ACCESSIBLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TREE_VIEW_ACCESSIBLE))
#define GTK_TREE_VIEW_ACCESSIBLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TREE_VIEW_ACCESSIBLE, GtkTreeViewAccessibleClass))

typedef struct _GtkTreeViewAccessible        GtkTreeViewAccessible;
typedef struct _GtkTreeViewAccessibleClass   GtkTreeViewAccessibleClass;
typedef struct _GtkTreeViewAccessiblePrivate GtkTreeViewAccessiblePrivate;

struct _GtkTreeViewAccessible
{
  GtkContainerAccessible parent;

  GtkTreeViewAccessiblePrivate *priv;
};

struct _GtkTreeViewAccessibleClass
{
  GtkContainerAccessibleClass parent_class;
};

GType _gtk_tree_view_accessible_get_type (void);

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
void            _gtk_tree_view_accessible_reorder_column(GtkTreeView       *treeview,
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

#endif /* __GTK_TREE_VIEW_ACCESSIBLE_H__ */
