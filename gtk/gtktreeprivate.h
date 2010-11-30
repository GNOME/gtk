/* gtktreeprivate.h
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

#ifndef __GTK_TREE_PRIVATE_H__
#define __GTK_TREE_PRIVATE_H__


G_BEGIN_DECLS


#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkrbtree.h>

#define TREE_VIEW_DRAG_WIDTH 6

typedef enum
{
  GTK_TREE_SELECT_MODE_TOGGLE = 1 << 0,
  GTK_TREE_SELECT_MODE_EXTEND = 1 << 1
}
GtkTreeSelectMode;

typedef struct _GtkTreeViewColumnReorder GtkTreeViewColumnReorder;
struct _GtkTreeViewColumnReorder
{
  gint left_align;
  gint right_align;
  GtkTreeViewColumn *left_column;
  GtkTreeViewColumn *right_column;
};

struct _GtkTreeViewPrivate
{
  GtkTreeModel *model;

  guint flags;
  /* tree information */
  GtkRBTree *tree;

  /* Container info */
  GList *children;
  gint width;
  gint height;

  /* Adjustments */
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  gint           min_display_width;
  gint           min_display_height;

  /* Sub windows */
  GdkWindow *bin_window;
  GdkWindow *header_window;

  /* Scroll position state keeping */
  GtkTreeRowReference *top_row;
  gint top_row_dy;
  /* dy == y pos of top_row + top_row_dy */
  /* we cache it for simplicity of the code */
  gint dy;

  guint presize_handler_timer;
  guint validate_rows_timer;
  guint scroll_sync_timer;

  /* Indentation and expander layout */
  gint expander_size;
  GtkTreeViewColumn *expander_column;

  gint level_indentation;

  /* Key navigation (focus), selection */
  gint cursor_offset;

  GtkTreeRowReference *anchor;
  GtkTreeRowReference *cursor;

  GtkTreeViewColumn *focus_column;

  /* Current pressed node, previously pressed, prelight */
  GtkRBNode *button_pressed_node;
  GtkRBTree *button_pressed_tree;

  gint pressed_button;
  gint press_start_x;
  gint press_start_y;

  gint event_last_x;
  gint event_last_y;

  guint last_button_time;
  gint last_button_x;
  gint last_button_y;

  GtkRBNode *prelight_node;
  GtkRBTree *prelight_tree;

  /* Cell Editing */
  GtkTreeViewColumn *edited_column;

  /* The node that's currently being collapsed or expanded */
  GtkRBNode *expanded_collapsed_node;
  GtkRBTree *expanded_collapsed_tree;
  guint expand_collapse_timeout;

  /* Auto expand/collapse timeout in hover mode */
  guint auto_expand_timeout;

  /* Selection information */
  GtkTreeSelection *selection;

  /* Header information */
  gint n_columns;
  GList *columns;
  gint header_height;

  GtkTreeViewColumnDropFunc column_drop_func;
  gpointer column_drop_func_data;
  GDestroyNotify column_drop_func_data_destroy;
  GList *column_drag_info;
  GtkTreeViewColumnReorder *cur_reorder;

  gint prev_width_before_expander;

  /* Interactive Header reordering */
  GdkWindow *drag_window;
  GdkWindow *drag_highlight_window;
  GtkTreeViewColumn *drag_column;
  gint drag_column_x;

  /* Interactive Header Resizing */
  gint drag_pos;
  gint x_drag;

  /* Non-interactive Header Resizing, expand flag support */
  gint prev_width;

  gint last_extra_space;
  gint last_extra_space_per_column;
  gint last_number_of_expand_columns;

  /* ATK Hack */
  GtkTreeDestroyCountFunc destroy_count_func;
  gpointer destroy_count_data;
  GDestroyNotify destroy_count_destroy;

  /* Scroll timeout (e.g. during dnd, rubber banding) */
  guint scroll_timeout;

  /* Row drag-and-drop */
  GtkTreeRowReference *drag_dest_row;
  GtkTreeViewDropPosition drag_dest_pos;
  guint open_dest_timeout;

  /* Rubber banding */
  gint rubber_band_status;
  gint rubber_band_x;
  gint rubber_band_y;
  gint rubber_band_shift;
  gint rubber_band_ctrl;

  GtkRBNode *rubber_band_start_node;
  GtkRBTree *rubber_band_start_tree;

  GtkRBNode *rubber_band_end_node;
  GtkRBTree *rubber_band_end_tree;

  /* fixed height */
  gint fixed_height;

  /* Scroll-to functionality when unrealized */
  GtkTreeRowReference *scroll_to_path;
  GtkTreeViewColumn *scroll_to_column;
  gfloat scroll_to_row_align;
  gfloat scroll_to_col_align;

  /* Interactive search */
  gint selected_iter;
  gint search_column;
  GtkTreeViewSearchPositionFunc search_position_func;
  GtkTreeViewSearchEqualFunc search_equal_func;
  gpointer search_user_data;
  GDestroyNotify search_destroy;
  gpointer search_position_user_data;
  GDestroyNotify search_position_destroy;
  GtkWidget *search_window;
  GtkWidget *search_entry;
  gulong search_entry_changed_id;
  guint typeselect_flush_timeout;

  /* Grid and tree lines */
  GtkTreeViewGridLines grid_lines;
  double grid_line_dashes[2];
  int grid_line_width;

  gboolean tree_lines_enabled;
  double tree_line_dashes[2];
  int tree_line_width;

  /* Row separators */
  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer row_separator_data;
  GDestroyNotify row_separator_destroy;

  /* Tooltip support */
  gint tooltip_column;

  /* Here comes the bitfield */
  guint scroll_to_use_align : 1;

  guint fixed_height_mode : 1;
  guint fixed_height_check : 1;

  guint reorderable : 1;
  guint header_has_focus : 1;
  guint drag_column_window_state : 3;
  /* hint to display rows in alternating colors */
  guint has_rules : 1;
  guint mark_rows_col_dirty : 1;

  /* for DnD */
  guint empty_view_drop : 1;

  guint ctrl_pressed : 1;
  guint shift_pressed : 1;

  guint init_hadjust_value : 1;

  guint in_top_row_to_dy : 1;

  /* interactive search */
  guint enable_search : 1;
  guint disable_popdown : 1;
  guint search_custom_entry_set : 1;
  
  guint hover_selection : 1;
  guint hover_expand : 1;
  guint imcontext_changed : 1;

  guint rubber_banding_enable : 1;

  guint in_grab : 1;

  guint post_validation_flag : 1;

  /* Whether our key press handler is to avoid sending an unhandled binding to the search entry */
  guint search_entry_avoid_unhandled_binding : 1;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
};

/* functions that shouldn't be exported */
void         _gtk_tree_selection_internal_select_node (GtkTreeSelection  *selection,
						       GtkRBNode         *node,
						       GtkRBTree         *tree,
						       GtkTreePath       *path,
                                                       GtkTreeSelectMode  mode,
						       gboolean           override_browse_mode);
void         _gtk_tree_selection_emit_changed         (GtkTreeSelection  *selection);
gboolean     _gtk_tree_view_find_node                 (GtkTreeView       *tree_view,
						       GtkTreePath       *path,
						       GtkRBTree        **tree,
						       GtkRBNode        **node);
GtkTreePath *_gtk_tree_view_find_path                 (GtkTreeView       *tree_view,
						       GtkRBTree         *tree,
						       GtkRBNode         *node);
void         _gtk_tree_view_child_move_resize         (GtkTreeView       *tree_view,
						       GtkWidget         *widget,
						       gint               x,
						       gint               y,
						       gint               width,
						       gint               height);
void         _gtk_tree_view_queue_draw_node           (GtkTreeView       *tree_view,
						       GtkRBTree         *tree,
						       GtkRBNode         *node,
						       const GdkRectangle *clip_rect);

void         _gtk_tree_view_add_editable              (GtkTreeView       *tree_view,
                                                       GtkTreeViewColumn *column,
                                                       GtkTreePath       *path,
                                                       GtkCellEditable   *cell_editable,
                                                       GdkRectangle      *cell_area);
void         _gtk_tree_view_remove_editable           (GtkTreeView       *tree_view,
                                                       GtkTreeViewColumn *column,
                                                       GtkCellEditable   *cell_editable);

void       _gtk_tree_view_install_mark_rows_col_dirty (GtkTreeView *tree_view);
void         _gtk_tree_view_column_autosize           (GtkTreeView       *tree_view,
						       GtkTreeViewColumn *column);
gint         _gtk_tree_view_get_header_height         (GtkTreeView       *tree_view);

GtkTreeSelection* _gtk_tree_selection_new                (void);
GtkTreeSelection* _gtk_tree_selection_new_with_tree_view (GtkTreeView      *tree_view);
void              _gtk_tree_selection_set_tree_view      (GtkTreeSelection *selection,
                                                          GtkTreeView      *tree_view);
gboolean          _gtk_tree_selection_row_is_selectable  (GtkTreeSelection *selection,
							  GtkRBNode        *node,
							  GtkTreePath      *path);


void _gtk_tree_view_column_realize_button   (GtkTreeViewColumn *column);
void _gtk_tree_view_column_unrealize_button (GtkTreeViewColumn *column);
void _gtk_tree_view_column_set_tree_view    (GtkTreeViewColumn *column,
					     GtkTreeView       *tree_view);
void _gtk_tree_view_column_set_width        (GtkTreeViewColumn *column,
                                             int                width,
					     int                internal_width);
void _gtk_tree_view_column_unset_model      (GtkTreeViewColumn *column,
					     GtkTreeModel      *old_model);
void _gtk_tree_view_column_unset_tree_view  (GtkTreeViewColumn *column);
void _gtk_tree_view_column_start_drag       (GtkTreeView       *tree_view,
					     GtkTreeViewColumn *column,
                                             GdkDevice         *device);
gboolean _gtk_tree_view_column_cell_event   (GtkTreeViewColumn  *tree_column,
					     GtkCellEditable   **editable_widget,
					     GdkEvent           *event,
					     gchar              *path_string,
					     const GdkRectangle *background_area,
					     const GdkRectangle *cell_area,
					     guint               flags);
gboolean          _gtk_tree_view_column_has_editable_cell(GtkTreeViewColumn  *column);
GtkCellRenderer  *_gtk_tree_view_column_get_edited_cell  (GtkTreeViewColumn  *column);
GtkCellRenderer  *_gtk_tree_view_column_get_cell_at_pos  (GtkTreeViewColumn  *column,
							  gint                x);

void		  _gtk_tree_view_column_cell_render      (GtkTreeViewColumn  *tree_column,
							  cairo_t            *cr,
							  const GdkRectangle *background_area,
							  const GdkRectangle *cell_area,
							  guint               flags,
                                                          gboolean            draw_focus);
void		  _gtk_tree_view_column_get_focus_area   (GtkTreeViewColumn  *tree_column,
							  const GdkRectangle *background_area,
							  const GdkRectangle *cell_area,
							  GdkRectangle       *focus_area);
gboolean	  _gtk_tree_view_column_cell_focus       (GtkTreeViewColumn  *tree_column,
							  gint                count,
							  gboolean            left,
							  gboolean            right);
void		  _gtk_tree_view_column_cell_set_dirty	 (GtkTreeViewColumn  *tree_column,
							  gboolean            install_handler);
gboolean          _gtk_tree_view_column_cell_get_dirty   (GtkTreeViewColumn  *tree_column);
GdkWindow        *_gtk_tree_view_column_get_window       (GtkTreeViewColumn  *column);

void              _gtk_tree_view_column_set_requested_width   (GtkTreeViewColumn  *column,
							       gint                width);
gint              _gtk_tree_view_column_get_requested_width   (GtkTreeViewColumn  *column);
void              _gtk_tree_view_column_set_resized_width     (GtkTreeViewColumn  *column,
							       gint                width);
gint              _gtk_tree_view_column_get_resized_width     (GtkTreeViewColumn  *column);
void              _gtk_tree_view_column_set_use_resized_width (GtkTreeViewColumn  *column,
							       gboolean            use_resized_width);
gboolean          _gtk_tree_view_column_get_use_resized_width (GtkTreeViewColumn  *column);
gint              _gtk_tree_view_column_get_drag_x            (GtkTreeViewColumn  *column);
GtkCellAreaContext *_gtk_tree_view_column_get_context         (GtkTreeViewColumn  *column);

G_END_DECLS


#endif /* __GTK_TREE_PRIVATE_H__ */

