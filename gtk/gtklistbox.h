/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef __GTK_LIST_BOX_H__
#define __GTK_LIST_BOX_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkscrolledwindow.h>

G_BEGIN_DECLS


#define GTK_TYPE_LIST_BOX (gtk_list_box_get_type ())
#define GTK_LIST_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LIST_BOX, GtkListBox))
#define GTK_LIST_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LIST_BOX, GtkListBoxClass))
#define GTK_IS_LIST_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LIST_BOX))
#define GTK_IS_LIST_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LIST_BOX))
#define GTK_LIST_BOX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LIST_BOX, GtkListBoxClass))

typedef struct _GtkListBox GtkListBox;
typedef struct _GtkListBoxClass GtkListBoxClass;
typedef struct _GtkListBoxPrivate GtkListBoxPrivate;

typedef struct _GtkListBoxRow GtkListBoxRow;
typedef struct _GtkListBoxRowClass GtkListBoxRowClass;
typedef struct _GtkListBoxRowPrivate GtkListBoxRowPrivate;

struct _GtkListBox
{
  GtkContainer parent_instance;

  GtkListBoxPrivate * priv;
};

struct _GtkListBoxClass
{
  GtkContainerClass parent_class;

  void (*row_selected) (GtkListBox* list_box, GtkListBoxRow* row);
  void (*row_activated) (GtkListBox* list_box, GtkListBoxRow* row);
  void (*activate_cursor_row) (GtkListBox* list_box);
  void (*toggle_cursor_row) (GtkListBox* list_box);
  void (*move_cursor) (GtkListBox* list_box, GtkMovementStep step, gint count);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
};

#define GTK_TYPE_LIST_BOX_ROW (gtk_list_box_row_get_type ())
#define GTK_LIST_BOX_ROW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LIST_BOX_ROW, GtkListBoxRow))
#define GTK_LIST_BOX_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LIST_BOX_ROW, GtkListBoxRowClass))
#define GTK_IS_LIST_BOX_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LIST_BOX_ROW))
#define GTK_IS_LIST_BOX_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LIST_BOX_ROW))
#define GTK_LIST_BOX_ROW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LIST_BOX_ROW, GtkListBoxRowClass))

struct _GtkListBoxRow
{
  GtkBin parent_instance;

  GtkListBoxRowPrivate * priv;
};

struct _GtkListBoxRowClass
{
  GtkBinClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
};

/**
 * GtkListBoxFilterFunc:
 * @row: The row that may be filtered.
 * @user_data: (closure): user data.
 *
 * Will be called whenever the row changes or is added and lets you control
 * if the row should be visible or not.
 *
 * Returns: %TRUE if the row should be visible, %FALSE otherwise
 *
 * Since: 3.10
 */
typedef gboolean (*GtkListBoxFilterFunc) (GtkListBoxRow * row,
                                          gpointer        user_data);
/**
 * GtkListBoxSortFunc:
 * @row1: The first row.
 * @row2: The second row.
 * @user_data: (closure): user data.
 *
 * Compare two rows to determin which should be first.
 *
 * Returns: < 0 if @row1 should be before @row2, 0 if they are equal and > 0 otherwise
 *
 * Since: 3.10
 */
typedef gint (*GtkListBoxSortFunc) (GtkListBoxRow *row1,
                                    GtkListBoxRow *row2,
                                    gpointer       user_data);
/**
 * GtkListBoxUpdateHeaderFunc:
 * @row: The row to update
 * @before: The row before @row, or %NULL if it is first.
 * @user_data: (closure): user data.
 *
 * Whenever @row changes or which row is before @row changes this is called, which
 * lets you update the header on @row. You may remove or set a new one
 * via gtk_list_box_row_set_header() or just change the state of the current
 * header widget.
 *
 * Since: 3.10
 */
typedef void (*GtkListBoxUpdateHeaderFunc) (GtkListBoxRow *row,
                                            GtkListBoxRow *before,
                                            gpointer       user_data);

GDK_AVAILABLE_IN_3_10
GType      gtk_list_box_row_get_type      (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_10
GtkWidget* gtk_list_box_row_new           (void);
GDK_AVAILABLE_IN_3_10
GtkWidget* gtk_list_box_row_get_header    (GtkListBoxRow *row);
GDK_AVAILABLE_IN_3_10
void       gtk_list_box_row_set_header    (GtkListBoxRow *row,
                                           GtkWidget     *header);
GDK_AVAILABLE_IN_3_10
void       gtk_list_box_row_changed       (GtkListBoxRow *row);


GDK_AVAILABLE_IN_3_10
GType          gtk_list_box_get_type                     (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_10
GtkListBoxRow* gtk_list_box_get_selected_row             (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
GtkListBoxRow* gtk_list_box_get_row_at_index             (GtkListBox                    *list_box,
                                                          int                           index);
GDK_AVAILABLE_IN_3_10
GtkListBoxRow* gtk_list_box_get_row_at_y                 (GtkListBox                    *list_box,
                                                          gint                           y);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_select_row                   (GtkListBox                    *list_box,
                                                          GtkListBoxRow                 *row);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_placeholder              (GtkListBox                    *list_box,
                                                          GtkWidget                     *placeholder);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_adjustment               (GtkListBox                    *list_box,
                                                          GtkAdjustment                 *adjustment);
GDK_AVAILABLE_IN_3_10
GtkAdjustment *gtk_list_box_get_adjustment               (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_selection_mode           (GtkListBox                    *list_box,
                                                          GtkSelectionMode               mode);
GDK_AVAILABLE_IN_3_10
GtkSelectionMode gtk_list_box_get_selection_mode         (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_filter_func              (GtkListBox                    *list_box,
                                                          GtkListBoxFilterFunc           filter_func,
                                                          gpointer                       user_data,
                                                          GDestroyNotify                 destroy);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_header_func              (GtkListBox                    *list_box,
                                                          GtkListBoxUpdateHeaderFunc     update_header,
                                                          gpointer                       user_data,
                                                          GDestroyNotify                 destroy);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_refilter                     (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_resort                       (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_reseparate                   (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_sort_func                (GtkListBox                    *list_box,
                                                          GtkListBoxSortFunc             sort_func,
                                                          gpointer                       user_data,
                                                          GDestroyNotify                 destroy);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_set_activate_on_single_click (GtkListBox                    *list_box,
                                                          gboolean                       single);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_drag_unhighlight_row         (GtkListBox                    *list_box);
GDK_AVAILABLE_IN_3_10
void           gtk_list_box_drag_highlight_row           (GtkListBox                    *list_box,
                                                          GtkListBoxRow                 *row);
GDK_AVAILABLE_IN_3_10
GtkWidget*     gtk_list_box_new                          (void);


G_END_DECLS

#endif
