/* gtktreeselection.h
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

#ifndef __GTK_TREE_SELECTION_H__
#define __GTK_TREE_SELECTION_H__

#include <gtk/gtkobject.h>
#include <gtk/gtktreeview.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_SELECTION			(gtk_tree_selection_get_type ())
#define GTK_TREE_SELECTION(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_SELECTION, GtkTreeSelection))
#define GTK_TREE_SELECTION_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_SELECTION, GtkTreeSelectionClass))
#define GTK_IS_TREE_SELECTION(obj)         	(GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_SELECTION))
#define GTK_IS_TREE_SELECTION_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_SELECTION))
#define GTK_TREE_SELECTION_GET_CLASS(obj)       (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TREE_SELECTION, GtkTreeSelectionClass))

typedef enum
{
  GTK_TREE_SELECTION_SINGLE,
  GTK_TREE_SELECTION_MULTI
} GtkTreeSelectionMode;

typedef gboolean (* GtkTreeSelectionFunc)    (GtkTreeSelection  *selection,
					      GtkTreeModel      *model,
					      GtkTreePath       *path,
					      gpointer           data);
typedef void (* GtkTreeSelectionForeachFunc) (GtkTreeModel      *model,
					      GtkTreePath       *path,
					      GtkTreeIter       *iter,
					      gpointer           data);

struct _GtkTreeSelection
{
  GObject parent;

  /*< private >*/
  
  GtkTreeView *tree_view;
  GtkTreeSelectionMode type;
  GtkTreeSelectionFunc user_func;
  gpointer user_data;
  GtkDestroyNotify destroy;
};

struct _GtkTreeSelectionClass
{
  GObjectClass parent_class;

  void (* changed) (GtkTreeSelection *selection);
};


GtkType          gtk_tree_selection_get_type            (void);

void             gtk_tree_selection_set_mode            (GtkTreeSelection            *selection,
							 GtkTreeSelectionMode         type);
GtkTreeSelectionMode gtk_tree_selection_get_mode        (GtkTreeSelection            *selection);
void             gtk_tree_selection_set_select_function (GtkTreeSelection            *selection,
							 GtkTreeSelectionFunc         func,
							 gpointer                     data,
							 GtkDestroyNotify             destroy);
gpointer         gtk_tree_selection_get_user_data       (GtkTreeSelection            *selection);
GtkTreeView*     gtk_tree_selection_get_tree_view       (GtkTreeSelection            *selection);

/* Only meaningful if GTK_TREE_SELECTION_SINGLE is set */
/* Use selected_foreach for GTK_TREE_SELECTION_MULTI */
gboolean         gtk_tree_selection_get_selected        (GtkTreeSelection            *selection,
							 GtkTreeModel               **model,
							 GtkTreeIter                 *iter);

/* FIXME: Get a more convenient get_selection function????  one returning GSList?? */
void             gtk_tree_selection_selected_foreach    (GtkTreeSelection            *selection,
							 GtkTreeSelectionForeachFunc  func,
							 gpointer                     data);
void             gtk_tree_selection_select_path         (GtkTreeSelection            *selection,
							 GtkTreePath                 *path);
void             gtk_tree_selection_unselect_path       (GtkTreeSelection            *selection,
							 GtkTreePath                 *path);
void             gtk_tree_selection_select_iter         (GtkTreeSelection            *selection,
							 GtkTreeIter                 *iter);
void             gtk_tree_selection_unselect_iter       (GtkTreeSelection            *selection,
							 GtkTreeIter                 *iter);
void             gtk_tree_selection_select_all          (GtkTreeSelection            *selection);
void             gtk_tree_selection_unselect_all        (GtkTreeSelection            *selection);
void             gtk_tree_selection_select_range        (GtkTreeSelection            *selection,
							 GtkTreePath                 *start_path,
							 GtkTreePath                 *end_path);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_SELECTION_H__ */

