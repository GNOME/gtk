/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>
 *
 * GtkCTree widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

#ifndef __GTK_CTREE_H__
#define __GTK_CTREE_H__

#include <gtk/gtkclist.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_TYPE_CTREE            (gtk_ctree_get_type ())
#define GTK_CTREE(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_CTREE, GtkCTree))
#define GTK_CTREE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CTREE, GtkCTreeClass))
#define GTK_IS_CTREE(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_CTREE))
#define GTK_IS_CTREE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CTREE))

#define GTK_CTREE_ROW(_glist_) ((GtkCTreeRow *)((_glist_)->data))
#define GTK_CTREE_TREE(_ctree_, _glist_) \
  ((GtkCellTree *) &(((GtkCTreeRow *)((_glist_)->data))->cell[(_ctree_)->tree_col]))

#define GTK_CTREE_FUNC(_func_) ((GtkCTreeFunc)(_func_))

typedef enum
{
  GTK_CTREE_POS_BEFORE,
  GTK_CTREE_POS_AS_CHILD,
  GTK_CTREE_POS_AFTER
} GtkCTreePos;

typedef enum
{
  GTK_CTREE_LINES_SOLID,
  GTK_CTREE_LINES_DOTTED,
  GTK_CTREE_LINES_TABBED,
  GTK_CTREE_LINES_NONE
} GtkCTreeLineStyle;

typedef enum
{
  GTK_CTREE_EXPANSION_EXPAND,
  GTK_CTREE_EXPANSION_EXPAND_RECURSIVE,
  GTK_CTREE_EXPANSION_COLLAPSE,
  GTK_CTREE_EXPANSION_COLLAPSE_RECURSIVE,
  GTK_CTREE_EXPANSION_TOGGLE,
  GTK_CTREE_EXPANSION_TOGGLE_RECURSIVE
} GtkCTreeExpansionType;

typedef struct _GtkCTree      GtkCTree;
typedef struct _GtkCTreeClass GtkCTreeClass;
typedef struct _GtkCTreeRow   GtkCTreeRow;

typedef void (*GtkCTreeFunc) (GtkCTree *ctree,
			      GList    *node,
			      gpointer  data);

typedef gint (*GtkCTreeCompareFunc) (GtkCTree    *ctree,
				     const GList *node1,
				     const GList *node2);

struct _GtkCTree
{
  GtkCList clist;

  GdkGC *xor_gc;
  GdkGC *lines_gc;
  GdkWindow *drag_icon;
  gint icon_width;
  gint icon_height;

  gint tree_indent;
  gint tree_column;
  gint drag_row;
  GList *drag_source;
  GList *drag_target;
  gint insert_pos;
  GtkCTreeCompareFunc node_compare;

  guint auto_sort   : 1;
  guint reorderable : 1;
  guint use_icons   : 1;
  guint in_drag     : 1;
  guint drag_rect   : 1;
  guint line_style  : 2;
};

struct _GtkCTreeClass
{
  GtkCListClass parent_class;

  void (*tree_select_row)   (GtkCTree *ctree,
			     GList    *row,
			     gint      column);
  void (*tree_unselect_row) (GtkCTree *ctree,
			     GList    *row,
			     gint      column);
  void (*tree_expand)       (GtkCTree *ctree,
			     GList    *node);
  void (*tree_collapse)     (GtkCTree *ctree,
			     GList    *node);
  void (*tree_move)         (GtkCTree *ctree,
			     GList    *node,
			     GList    *new_parent,
			     GList    *new_sibling);
  void (*change_focus_row_expansion) (GtkCTree *ctree,
				      GtkCTreeExpansionType action);
};

struct _GtkCTreeRow
{
  GtkCListRow row;

  GList *parent;
  GList *sibling;
  GList *children;

  GdkPixmap *pixmap_closed;
  GdkBitmap *mask_closed;
  GdkPixmap *pixmap_opened;
  GdkBitmap *mask_opened;

  guint16 level;

  guint is_leaf  : 1;
  guint expanded : 1;
};


/***********************************************************
 *           Creation, insertion, deletion                 *
 ***********************************************************/

GtkType     gtk_ctree_get_type              (void);
void        gtk_ctree_construct             (GtkCTree     *ctree,
					     gint          columns, 
					     gint          tree_column,
					     gchar        *titles[]);
GtkWidget * gtk_ctree_new_with_titles       (gint          columns, 
					     gint          tree_column,
					     gchar        *titles[]);
GtkWidget * gtk_ctree_new                   (gint          columns, 
					     gint          tree_column);
GList *     gtk_ctree_insert                (GtkCTree     *ctree,
					     GList        *parent, 
					     GList        *sibling,
					     gchar        *text[],
					     guint8        spacing,
					     GdkPixmap    *pixmap_closed,
					     GdkBitmap    *mask_closed,
					     GdkPixmap    *pixmap_opened,
					     GdkBitmap    *mask_opened,
					     gboolean      is_leaf,
					     gboolean      expanded);
void       gtk_ctree_remove                 (GtkCTree     *ctree, 
					     GList        *node);

/***********************************************************
 *  Generic recursive functions, querying / finding tree   *
 *  information                                            *
 ***********************************************************/

void       gtk_ctree_post_recursive          (GtkCTree     *ctree, 
					      GList        *node,
					      GtkCTreeFunc  func,
					      gpointer      data);
void       gtk_ctree_post_recursive_to_depth (GtkCTree     *ctree, 
					      GList        *node,
					      gint          depth,
					      GtkCTreeFunc  func,
					      gpointer      data);
void       gtk_ctree_pre_recursive           (GtkCTree     *ctree, 
					      GList        *node,
					      GtkCTreeFunc  func,
					      gpointer      data);
void       gtk_ctree_pre_recursive_to_depth  (GtkCTree     *ctree, 
					      GList        *node,
					      gint          depth,
					      GtkCTreeFunc  func,
					      gpointer      data);
gboolean   gtk_ctree_is_visible              (GtkCTree     *ctree, 
					      GList        *node);
GList *    gtk_ctree_last                    (GtkCTree     *ctree,
					      GList        *node);
GList *    gtk_ctree_find_glist_ptr          (GtkCTree     *ctree,
					      GtkCTreeRow  *ctree_row);
gint       gtk_ctree_find                    (GtkCTree     *ctree,
					      GList        *node,
					      GList        *child);
gboolean   gtk_ctree_is_ancestor             (GtkCTree     *ctree,
					      GList        *node,
					      GList        *child);
GList *    gtk_ctree_find_by_row_data        (GtkCTree     *ctree,
					      GList        *node,
					      gpointer      data);
gboolean   gtk_ctree_is_hot_spot             (GtkCTree     *ctree,
					      gint          x,
					      gint          y);

/***********************************************************
 *   Tree signals : move, expand, collapse, (un)select     *
 ***********************************************************/

void       gtk_ctree_move                   (GtkCTree     *ctree,
					     GList        *node,
					     GList        *new_parent, 
					     GList        *new_sibling);
void       gtk_ctree_expand                 (GtkCTree     *ctree,
					     GList        *node);
void       gtk_ctree_expand_recursive       (GtkCTree     *ctree,
					     GList        *node);
void       gtk_ctree_expand_to_depth        (GtkCTree     *ctree,
					     GList        *node,
					     gint          depth);
void       gtk_ctree_collapse               (GtkCTree     *ctree,
					     GList        *node);
void       gtk_ctree_collapse_recursive     (GtkCTree     *ctree,
					     GList        *node);
void       gtk_ctree_collapse_to_depth      (GtkCTree     *ctree,
					     GList        *node,
					     gint          depth);
void       gtk_ctree_toggle_expansion       (GtkCTree     *ctree,
					     GList        *node);
void       gtk_ctree_toggle_expansion_recursive (GtkCTree *ctree,
						 GList    *node);
void       gtk_ctree_select                 (GtkCTree     *ctree, 
					     GList        *node);
void       gtk_ctree_select_recursive       (GtkCTree     *ctree, 
					     GList        *node);
void       gtk_ctree_unselect               (GtkCTree     *ctree, 
					     GList        *node);
void       gtk_ctree_unselect_recursive     (GtkCTree     *ctree, 
					     GList        *node);
void       gtk_ctree_real_select_recursive  (GtkCTree     *ctree, 
					     GList        *node, 
					     gint          state);

/***********************************************************
 *           Analogons of GtkCList functions               *
 ***********************************************************/

void       gtk_ctree_set_text               (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     gchar        *text);
void       gtk_ctree_set_pixmap             (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     GdkPixmap    *pixmap,
					     GdkBitmap    *mask);
void       gtk_ctree_set_pixtext            (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     gchar        *text,
					     guint8        spacing,
					     GdkPixmap    *pixmap,
					     GdkBitmap    *mask);
void       gtk_ctree_set_node_info          (GtkCTree     *ctree,
					     GList        *node,
					     gchar        *text,
					     guint8        spacing,
					     GdkPixmap    *pixmap_closed,
					     GdkBitmap    *mask_closed,
					     GdkPixmap    *pixmap_opened,
					     GdkBitmap    *mask_opened,
					     gboolean      is_leaf,
					     gboolean      expanded);
void       gtk_ctree_set_shift              (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     gint          vertical,
					     gint          horizontal);
GtkCellType gtk_ctree_get_cell_type         (GtkCTree     *ctree,
					     GList        *node,
					     gint          column);
gint       gtk_ctree_get_text               (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     gchar       **text);
gint       gtk_ctree_get_pixmap             (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     GdkPixmap   **pixmap,
					     GdkBitmap   **mask);
gint       gtk_ctree_get_pixtext            (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     gchar       **text,
					     guint8       *spacing,
					     GdkPixmap   **pixmap,
					     GdkBitmap   **mask);
gint       gtk_ctree_get_node_info          (GtkCTree     *ctree,
					     GList        *node,
					     gchar       **text,
					     guint8       *spacing,
					     GdkPixmap   **pixmap_closed,
					     GdkBitmap   **mask_closed,
					     GdkPixmap   **pixmap_opened,
					     GdkBitmap   **mask_opened,
					     gboolean     *is_leaf,
					     gboolean     *expanded);
void       gtk_ctree_set_foreground         (GtkCTree     *ctree,
					     GList        *node,
					     GdkColor     *color);
void       gtk_ctree_set_background         (GtkCTree     *ctree,
					     GList        *node,
					     GdkColor     *color);
void       gtk_ctree_set_row_data           (GtkCTree     *ctree,
					     GList        *node,
					     gpointer      data);
void       gtk_ctree_set_row_data_full      (GtkCTree     *ctree,
					     GList        *node,
					     gpointer      data,
					     GtkDestroyNotify destroy);
gpointer   gtk_ctree_get_row_data           (GtkCTree     *ctree,
					     GList        *node);
void       gtk_ctree_moveto                 (GtkCTree     *ctree,
					     GList        *node,
					     gint          column,
					     gfloat        row_align,
					     gfloat        col_align);

/***********************************************************
 *             GtkCTree specific functions                 *
 ***********************************************************/

void       gtk_ctree_set_indent             (GtkCTree     *ctree, 
                                             gint          indent);
void       gtk_ctree_set_reorderable        (GtkCTree     *ctree,
					     gboolean      reorderable);
void       gtk_ctree_set_use_drag_icons     (GtkCTree     *ctree,
					     gboolean      use_icons);
void       gtk_ctree_set_line_style         (GtkCTree     *ctree, 
					     GtkCTreeLineStyle line_style);

/***********************************************************
 *             Tree sorting functions                      *
 ***********************************************************/

void       gtk_ctree_set_auto_sort          (GtkCTree     *ctree,
					     gboolean      auto_sort);
void       gtk_ctree_set_compare_func       (GtkCTree     *ctree,
					     GtkCTreeCompareFunc cmp_func);
void       gtk_ctree_sort                   (GtkCTree     *ctree, 
					     GList        *node);
void       gtk_ctree_sort_recursive         (GtkCTree     *ctree, 
					     GList        *node);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_CTREE_H__ */
