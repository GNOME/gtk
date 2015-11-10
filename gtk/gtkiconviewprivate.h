/* gtkiconview.h
 * Copyright (C) 2002, 2004  Anders Carlsson <andersca@gnome.org>
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

#include "gtk/gtkiconview.h"
#include "gtk/gtkcssnodeprivate.h"

#ifndef __GTK_ICON_VIEW_PRIVATE_H__
#define __GTK_ICON_VIEW_PRIVATE_H__

typedef struct _GtkIconViewItem GtkIconViewItem;
struct _GtkIconViewItem
{
  GdkRectangle cell_area;

  gint index;
  
  gint row, col;

  guint selected : 1;
  guint selected_before_rubberbanding : 1;

};

struct _GtkIconViewPrivate
{
  GtkCellArea        *cell_area;
  GtkCellAreaContext *cell_area_context;

  gulong              add_editable_id;
  gulong              remove_editable_id;
  gulong              context_changed_id;

  GPtrArray          *row_contexts;

  gint width, height;

  GtkSelectionMode selection_mode;

  GdkWindow *bin_window;

  GList *children;

  GtkTreeModel *model;

  GList *items;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  gint rubberband_x1, rubberband_y1;
  gint rubberband_x2, rubberband_y2;
  GdkDevice *rubberband_device;
  GtkCssNode *rubberband_node;

  guint scroll_timeout_id;
  gint scroll_value_diff;
  gint event_last_x, event_last_y;

  GtkIconViewItem *anchor_item;
  GtkIconViewItem *cursor_item;

  GtkIconViewItem *last_single_clicked;
  GtkIconViewItem *last_prelight;

  GtkOrientation item_orientation;

  gint columns;
  gint item_width;
  gint spacing;
  gint row_spacing;
  gint column_spacing;
  gint margin;
  gint item_padding;

  gint text_column;
  gint markup_column;
  gint pixbuf_column;
  gint tooltip_column;

  GtkCellRenderer *pixbuf_cell;
  GtkCellRenderer *text_cell;

  /* Drag-and-drop. */
  GdkModifierType start_button_mask;
  gint pressed_button;
  gint press_start_x;
  gint press_start_y;

  GdkDragAction source_actions;
  GdkDragAction dest_actions;

  GtkTreeRowReference *dest_item;
  GtkIconViewDropPosition dest_pos;

  /* scroll to */
  GtkTreeRowReference *scroll_to_path;
  gfloat scroll_to_row_align;
  gfloat scroll_to_col_align;
  guint scroll_to_use_align : 1;

  guint source_set : 1;
  guint dest_set : 1;
  guint reorderable : 1;
  guint empty_view_drop :1;
  guint activate_on_single_click : 1;

  guint modify_selection_pressed : 1;
  guint extend_selection_pressed : 1;

  guint draw_focus : 1;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;

  guint doing_rubberband : 1;

};

void                 _gtk_icon_view_set_cell_data                  (GtkIconView            *icon_view,
                                                                    GtkIconViewItem        *item);
void                 _gtk_icon_view_set_cursor_item                (GtkIconView            *icon_view,
                                                                    GtkIconViewItem        *item,
                                                                    GtkCellRenderer        *cursor_cell);
GtkIconViewItem *    _gtk_icon_view_get_item_at_coords             (GtkIconView            *icon_view,
                                                                    gint                    x,
                                                                    gint                    y,
                                                                    gboolean                only_in_cell,
                                                                    GtkCellRenderer       **cell_at_pos);
void                 _gtk_icon_view_select_item                    (GtkIconView            *icon_view,
                                                                    GtkIconViewItem        *item);
void                 _gtk_icon_view_unselect_item                  (GtkIconView            *icon_view,
                                                                    GtkIconViewItem        *item);

G_END_DECLS

#endif /* __GTK_ICON_VIEW_PRIVATE_H__ */
