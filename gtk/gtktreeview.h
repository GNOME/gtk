/* gtktreeview.h
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
#ifndef __GTK_TREE_VIEW_H__
#define __GTK_TREE_VIEW_H__

#include <gtk/gtkcontainer.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeviewcolumn.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_VIEW		(gtk_tree_view_get_type ())
#define GTK_TREE_VIEW(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_VIEW, GtkTreeView))
#define GTK_TREE_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_VIEW, GtkTreeViewClass))
#define GTK_IS_TREE_VIEW(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_VIEW))
#define GTK_IS_TREE_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_VIEW))

typedef struct _GtkTreeView        GtkTreeView;
typedef struct _GtkTreeViewClass   GtkTreeViewClass;
typedef struct _GtkTreeViewPrivate GtkTreeViewPrivate;

typedef struct _GtkTreeSelection      GtkTreeSelection;
typedef struct _GtkTreeSelectionClass GtkTreeSelectionClass;


struct _GtkTreeView
{
  GtkContainer parent;

  GtkTreeViewPrivate *priv;
};

struct _GtkTreeViewClass
{
  GtkContainerClass parent_class;

  void (*set_scroll_adjustments) (GtkTreeView   *tree_view,
				  GtkAdjustment *hadjustment,
				  GtkAdjustment *vadjustment);
  gint (*expand_row)             (GtkTreeView   *tree_view,
				  GtkTreeIter   *iter);
};

GtkType                gtk_tree_view_get_type            (void);
GtkWidget             *gtk_tree_view_new                 (void);
GtkWidget             *gtk_tree_view_new_with_model      (GtkTreeModel       *model);

GtkTreeModel          *gtk_tree_view_get_model           (GtkTreeView        *tree_view);
void                   gtk_tree_view_set_model           (GtkTreeView        *tree_view,
							  GtkTreeModel       *model);
GtkTreeSelection      *gtk_tree_view_get_selection       (GtkTreeView        *tree_view);
GtkAdjustment         *gtk_tree_view_get_hadjustment     (GtkTreeView        *tree_view);
void                   gtk_tree_view_set_hadjustment     (GtkTreeView        *tree_view,
							  GtkAdjustment      *adjustment);
GtkAdjustment         *gtk_tree_view_get_vadjustment     (GtkTreeView        *tree_view);
void                   gtk_tree_view_set_vadjustment     (GtkTreeView        *tree_view,
							  GtkAdjustment      *adjustment);
gboolean               gtk_tree_view_get_headers_visible (GtkTreeView        *tree_view);
void                   gtk_tree_view_set_headers_visible (GtkTreeView        *tree_view,
							  gboolean            headers_visible);
void                   gtk_tree_view_columns_autosize    (GtkTreeView        *tree_view);
void                   gtk_tree_view_set_headers_active  (GtkTreeView        *tree_view,
							  gboolean            active);
gint                   gtk_tree_view_append_column       (GtkTreeView        *tree_view,
							  GtkTreeViewColumn  *column);
gint                   gtk_tree_view_remove_column       (GtkTreeView        *tree_view,
							  GtkTreeViewColumn  *column);
gint                   gtk_tree_view_insert_column       (GtkTreeView        *tree_view,
							  GtkTreeViewColumn  *column,
							  gint                position);
GtkTreeViewColumn     *gtk_tree_view_get_column          (GtkTreeView        *tree_view,
							  gint                n);

/* Actions */
void                   gtk_tree_view_move_to             (GtkTreeView        *tree_view,
							  GtkTreePath        *path,
							  GtkTreeViewColumn  *column,
							  gfloat              row_align,
							  gfloat              col_align);
gboolean               gtk_tree_view_get_path_at_pos     (GtkTreeView        *tree_view,
							  GdkWindow          *window,
							  gint                x,
							  gint                y,
							  GtkTreePath       **path,
							  GtkTreeViewColumn **column);
void                   gtk_tree_view_expand_all          (GtkTreeView        *tree_view);
void                   gtk_tree_view_collapse_all        (GtkTreeView        *tree_view);
gboolean               gtk_tree_view_expand_row          (GtkTreeView        *tree_view,
							  GtkTreePath        *path,
							  gboolean            open_all);
gboolean               gtk_tree_view_collapse_row        (GtkTreeView        *tree_view,
							  GtkTreePath        *path);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_VIEW_H__ */

