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
#include <gtk/gtkdnd.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum
{
  /* drop before/after this row */
  GTK_TREE_VIEW_DROP_BEFORE,
  GTK_TREE_VIEW_DROP_AFTER,
  /* drop as a child of this row (with fallback to before or after
   * if into is not possible)
   */
  GTK_TREE_VIEW_DROP_INTO_OR_BEFORE,
  GTK_TREE_VIEW_DROP_INTO_OR_AFTER
} GtkTreeViewDropPosition;

#define GTK_TYPE_TREE_VIEW		(gtk_tree_view_get_type ())
#define GTK_TREE_VIEW(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_VIEW, GtkTreeView))
#define GTK_TREE_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_VIEW, GtkTreeViewClass))
#define GTK_IS_TREE_VIEW(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_VIEW))
#define GTK_IS_TREE_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_VIEW))
#define GTK_TREE_VIEW_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TREE_VIEW, GtkTreeViewClass))

typedef struct _GtkTreeView           GtkTreeView;
typedef struct _GtkTreeViewClass      GtkTreeViewClass;
typedef struct _GtkTreeViewPrivate    GtkTreeViewPrivate;
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

  void     (* set_scroll_adjustments) (GtkTreeView       *tree_view,
				       GtkAdjustment     *hadjustment,
				       GtkAdjustment     *vadjustment);
  void     (* row_activated)          (GtkTreeView       *tree_view,
				       GtkTreePath       *path,
				       GtkTreeViewColumn *column);
  gboolean (* expand_row)             (GtkTreeView       *tree_view,
				       GtkTreeIter       *iter,
				       GtkTreePath       *path);
  gboolean (* collapse_row)           (GtkTreeView       *tree_view,
				       GtkTreeIter       *iter,
				       GtkTreePath       *path);
  void     (* columns_changed)        (GtkTreeView       *tree_view);
};


typedef gboolean (* GtkTreeViewColumnDropFunc) (GtkTreeView             *tree_view,
						GtkTreeViewColumn       *column,
						GtkTreeViewColumn       *prev_column,
						GtkTreeViewColumn       *next_column,
						gpointer                 data);
typedef gboolean (* GtkTreeViewDraggableFunc)  (GtkTreeView             *tree_view,
                                                GdkDragContext          *context,
                                                GtkTreePath             *path,
						gpointer                user_data);
typedef void     (* GtkTreeViewMappingFunc)    (GtkTreeView             *tree_view,
						GtkTreePath             *path,
						gpointer                 user_data);
typedef gboolean (* GtkTreeViewDroppableFunc)  (GtkTreeView             *tree_view,
						GdkDragContext          *context,
						GtkTreePath             *path,
						GtkTreeViewDropPosition *pos,
						gpointer                 user_data);


/* Creators */
GtkType                gtk_tree_view_get_type                      (void);
GtkWidget             *gtk_tree_view_new                           (void);
GtkWidget             *gtk_tree_view_new_with_model                (GtkTreeModel              *model);

/* Accessors */
GtkTreeModel          *gtk_tree_view_get_model                     (GtkTreeView               *tree_view);
void                   gtk_tree_view_set_model                     (GtkTreeView               *tree_view,
								    GtkTreeModel              *model);
GtkTreeSelection      *gtk_tree_view_get_selection                 (GtkTreeView               *tree_view);
GtkAdjustment         *gtk_tree_view_get_hadjustment               (GtkTreeView               *tree_view);
void                   gtk_tree_view_set_hadjustment               (GtkTreeView               *tree_view,
								    GtkAdjustment             *adjustment);
GtkAdjustment         *gtk_tree_view_get_vadjustment               (GtkTreeView               *tree_view);
void                   gtk_tree_view_set_vadjustment               (GtkTreeView               *tree_view,
								    GtkAdjustment             *adjustment);
gboolean               gtk_tree_view_get_headers_visible           (GtkTreeView               *tree_view);
void                   gtk_tree_view_set_headers_visible           (GtkTreeView               *tree_view,
								    gboolean                   headers_visible);
void                   gtk_tree_view_columns_autosize              (GtkTreeView               *tree_view);
void                   gtk_tree_view_set_headers_clickable         (GtkTreeView               *tree_view,
								    gboolean                   setting);
void                   gtk_tree_view_set_rules_hint                (GtkTreeView               *tree_view,
								    gboolean                   setting);
gboolean               gtk_tree_view_get_rules_hint                (GtkTreeView               *tree_view);

/* Column funtions */
gint                   gtk_tree_view_append_column                 (GtkTreeView               *tree_view,
								    GtkTreeViewColumn         *column);
gint                   gtk_tree_view_remove_column                 (GtkTreeView               *tree_view,
								    GtkTreeViewColumn         *column);
gint                   gtk_tree_view_insert_column                 (GtkTreeView               *tree_view,
								    GtkTreeViewColumn         *column,
								    gint                       position);
gint                   gtk_tree_view_insert_column_with_attributes (GtkTreeView               *tree_view,
								    gint                       position,
								    gchar                     *title,
								    GtkCellRenderer           *cell,
								    ...);
gint                   gtk_tree_view_insert_column_with_data_func  (GtkTreeView               *tree_view,
								    gint                       position,
								    gchar                     *title,
								    GtkCellRenderer           *cell,
                                                                    GtkCellDataFunc            func,
                                                                    gpointer                   data,
                                                                    GDestroyNotify             dnotify);
GtkTreeViewColumn     *gtk_tree_view_get_column                    (GtkTreeView               *tree_view,
								    gint                       n);
GList                 *gtk_tree_view_get_columns                   (GtkTreeView               *tree_view);
void                   gtk_tree_view_move_column_after             (GtkTreeView               *tree_view,
								    GtkTreeViewColumn         *column,
								    GtkTreeViewColumn         *base_column);
void                   gtk_tree_view_set_expander_column           (GtkTreeView               *tree_view,
								    gint                       col);
gint                   gtk_tree_view_get_expander_column           (GtkTreeView               *tree_view);
void                   gtk_tree_view_set_column_drag_function      (GtkTreeView               *tree_view,
								    GtkTreeViewColumnDropFunc  func,
								    gpointer                   user_data,
								    GtkDestroyNotify           destroy);

/* Actions */
void                   gtk_tree_view_scroll_to_point               (GtkTreeView               *tree_view,
								    gint                       tree_x,
								    gint                       tree_y);
void                   gtk_tree_view_scroll_to_cell                (GtkTreeView               *tree_view,
								    GtkTreePath               *path,
								    GtkTreeViewColumn         *column,
								    gfloat                     row_align,
								    gfloat                     col_align);
void                   gtk_tree_view_row_activated                 (GtkTreeView               *tree_view,
								    GtkTreePath               *path,
								    GtkTreeViewColumn         *column);
void                   gtk_tree_view_expand_all                    (GtkTreeView               *tree_view);
void                   gtk_tree_view_collapse_all                  (GtkTreeView               *tree_view);
gboolean               gtk_tree_view_expand_row                    (GtkTreeView               *tree_view,
								    GtkTreePath               *path,
								    gboolean                   open_all);
gboolean               gtk_tree_view_collapse_row                  (GtkTreeView               *tree_view,
								    GtkTreePath               *path);
void                   gtk_tree_view_map_expanded_rows             (GtkTreeView               *tree_view,
								    GtkTreeViewMappingFunc     func,
								    gpointer                   data);
void                   gtk_tree_view_set_reorderable               (GtkTreeView               *tree_view,
								    gboolean                   reorderable);


/* Layout information */
gboolean               gtk_tree_view_get_path_at_pos               (GtkTreeView               *tree_view,
								    GdkWindow                 *window,
								    gint                       x,
								    gint                       y,
								    GtkTreePath              **path,
								    GtkTreeViewColumn        **column,
								    gint                      *cell_x,
								    gint                      *cell_y);
void                   gtk_tree_view_get_cell_area                 (GtkTreeView               *tree_view,
								    GtkTreePath               *path,
								    GtkTreeViewColumn         *column,
								    GdkRectangle              *rect);
void                   gtk_tree_view_get_background_area           (GtkTreeView               *tree_view,
								    GtkTreePath               *path,
								    GtkTreeViewColumn         *column,
								    GdkRectangle              *rect);
void                   gtk_tree_view_get_visible_rect              (GtkTreeView               *tree_view,
								    GdkRectangle              *visible_rect);
void                   gtk_tree_view_widget_to_tree_coords         (GtkTreeView               *tree_view,
								    gint                       wx,
								    gint                       wy,
								    gint                      *tx,
								    gint                      *ty);
void                   gtk_tree_view_tree_to_widget_coords         (GtkTreeView               *tree_view,
								    gint                       tx,
								    gint                       ty,
								    gint                      *wx,
								    gint                      *wy);

/* Drag-and-Drop support */
void                   gtk_tree_view_set_rows_drag_source          (GtkTreeView               *tree_view,
								    GdkModifierType            start_button_mask,
								    const GtkTargetEntry      *targets,
								    gint                       n_targets,
								    GdkDragAction              actions,
								    GtkTreeViewDraggableFunc   row_draggable_func,
								    gpointer                   user_data);
void                   gtk_tree_view_set_rows_drag_dest            (GtkTreeView               *tree_view,
								    const GtkTargetEntry      *targets,
								    gint                       n_targets,
								    GdkDragAction              actions,
								    GtkTreeViewDroppableFunc   location_droppable_func,
								    gpointer                   user_data);
void                   gtk_tree_view_unset_rows_drag_source        (GtkTreeView               *tree_view);
void                   gtk_tree_view_unset_rows_drag_dest          (GtkTreeView               *tree_view);


/* These are useful to implement your own custom stuff. */
void                   gtk_tree_view_set_drag_dest_row             (GtkTreeView               *tree_view,
								    GtkTreePath               *path,
								    GtkTreeViewDropPosition    pos);
void                   gtk_tree_view_get_drag_dest_row             (GtkTreeView               *tree_view,
								    GtkTreePath              **path,
								    GtkTreeViewDropPosition   *pos);
gboolean               gtk_tree_view_get_dest_row_at_pos           (GtkTreeView               *tree_view,
								    gint                       drag_x,
								    gint                       drag_y,
								    GtkTreePath              **path,
								    GtkTreeViewDropPosition   *pos);
GdkPixmap             *gtk_tree_view_create_row_drag_icon          (GtkTreeView               *tree_view,
								    GtkTreePath               *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_VIEW_H__ */

