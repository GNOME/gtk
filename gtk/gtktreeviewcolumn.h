/* gtktreeviewcolumn.h
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

#ifndef __GTK_TREE_VIEW_COLUMN_H__
#define __GTK_TREE_VIEW_COLUMN_H__

#include <gtk/gtkobject.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreesortable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_COLUMN		     (gtk_tree_view_column_get_type ())
#define GTK_TREE_VIEW_COLUMN(obj)	     (GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_COLUMN, GtkTreeViewColumn))
#define GTK_TREE_VIEW_COLUMN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_COLUMN, GtkTreeViewColumnClass))
#define GTK_IS_TREE_VIEW_COLUMN(obj)	     (GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_COLUMN))
#define GTK_IS_TREE_VIEW_COLUMN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_COLUMN))

typedef enum
{
  GTK_TREE_VIEW_COLUMN_RESIZEABLE,
  GTK_TREE_VIEW_COLUMN_AUTOSIZE,
  GTK_TREE_VIEW_COLUMN_FIXED
} GtkTreeViewColumnSizing;

typedef struct _GtkTreeViewColumn      GtkTreeViewColumn;
typedef struct _GtkTreeViewColumnClass GtkTreeViewColumnClass;

typedef void (* CellDataFunc) (GtkTreeViewColumn *tree_column,
			       GtkCellRenderer   *cell,
			       GtkTreeModel      *tree_model,
			       GtkTreeIter       *iter,
			       gpointer           data);

  
struct _GtkTreeViewColumn
{
  GtkObject parent;

  GtkWidget *tree_view;
  GtkWidget *button;
  GtkWidget *child;  
  GtkWidget *arrow;
  GtkWidget *alignment;
  GdkWindow *window;
  gfloat xalign;

  gint id;

  gint width;
  gint min_width;
  gint max_width;
  gint displayed_width;

  CellDataFunc func;
  gpointer func_data;
  GtkDestroyNotify destroy;
  gchar *title;
  GtkCellRenderer *cell;
  GSList *attributes;
  GtkTreeViewColumnSizing column_type;
  GtkTreeSortOrder sort_order;
  guint visible       : 1;
  guint button_active : 1;
  guint dirty         : 1;
  guint show_sort_indicator : 1;
};

struct _GtkTreeViewColumnClass
{
  GtkObjectClass parent_class;

  void (*clicked) (GtkTreeViewColumn *tree_column);
};


GtkType            gtk_tree_view_column_get_type            (void);
GtkTreeViewColumn *gtk_tree_view_column_new                 (void);
GtkTreeViewColumn *gtk_tree_view_column_new_with_attributes (const gchar           *title,
							     GtkCellRenderer       *cell,
							     ...);
void               gtk_tree_view_column_set_cell_renderer   (GtkTreeViewColumn     *tree_column,
							     GtkCellRenderer       *cell);
GtkCellRenderer   *gtk_tree_view_column_get_cell_renderer   (GtkTreeViewColumn     *tree_column);
void               gtk_tree_view_column_add_attribute       (GtkTreeViewColumn     *tree_column,
							     const gchar           *attribute,
							     gint                   column);
void               gtk_tree_view_column_set_attributes      (GtkTreeViewColumn     *tree_column,
							     ...);
void               gtk_tree_view_column_set_cell_data_func  (GtkTreeViewColumn     *tree_column,
							     CellDataFunc           func,
							     gpointer               func_data,
							     GtkDestroyNotify       destroy);
void               gtk_tree_view_column_clear_attributes    (GtkTreeViewColumn     *tree_column);
void               gtk_tree_view_column_set_cell_data       (GtkTreeViewColumn     *tree_column,
							     GtkTreeModel          *tree_model,
							     GtkTreeIter           *iter);
void               gtk_tree_view_column_set_visible         (GtkTreeViewColumn     *tree_column,
							     gboolean               visible);
gboolean           gtk_tree_view_column_get_visible         (GtkTreeViewColumn     *tree_column);
void               gtk_tree_view_column_set_sizing          (GtkTreeViewColumn     *tree_column,
                                                             GtkTreeViewColumnSizing  type);
gint               gtk_tree_view_column_get_sizing          (GtkTreeViewColumn     *tree_column);
gint               gtk_tree_view_column_get_width           (GtkTreeViewColumn     *tree_column);
void               gtk_tree_view_column_set_width           (GtkTreeViewColumn     *tree_column,
							     gint                   size);
void               gtk_tree_view_column_set_min_width       (GtkTreeViewColumn     *tree_column,
							     gint                   min_width);
gint               gtk_tree_view_column_get_min_width       (GtkTreeViewColumn     *tree_column);
void               gtk_tree_view_column_set_max_width       (GtkTreeViewColumn     *tree_column,
							     gint                   max_width);
gint               gtk_tree_view_column_get_max_width       (GtkTreeViewColumn     *tree_column);


void               gtk_tree_view_column_clicked             (GtkTreeViewColumn     *tree_column);

/* Options for manipulating the column headers
 */
void                  gtk_tree_view_column_set_title          (GtkTreeViewColumn *tree_column,
                                                               const gchar       *title);
G_CONST_RETURN gchar *gtk_tree_view_column_get_title          (GtkTreeViewColumn *tree_column);
void                  gtk_tree_view_column_set_clickable      (GtkTreeViewColumn *tree_column,
                                                               gboolean           active);
gboolean              gtk_tree_view_column_get_clickable      (GtkTreeViewColumn *tree_column);
void                  gtk_tree_view_column_set_widget         (GtkTreeViewColumn *tree_column,
                                                               GtkWidget         *widget);
GtkWidget         *   gtk_tree_view_column_get_widget         (GtkTreeViewColumn *tree_column);
void                  gtk_tree_view_column_set_alignment      (GtkTreeViewColumn *tree_column,
                                                               gfloat             xalign);
gfloat                gtk_tree_view_column_get_alignment      (GtkTreeViewColumn *tree_column);
void                  gtk_tree_view_column_set_sort_indicator (GtkTreeViewColumn *tree_column,
                                                               gboolean           setting);
gboolean              gtk_tree_view_column_get_sort_indicator (GtkTreeViewColumn *tree_column);
void                  gtk_tree_view_column_set_sort_order     (GtkTreeViewColumn *tree_column,
                                                               GtkTreeSortOrder   order);
GtkTreeSortOrder      gtk_tree_view_column_get_sort_order     (GtkTreeViewColumn *tree_column);





#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_TREE_VIEW_COLUMN_H__ */
