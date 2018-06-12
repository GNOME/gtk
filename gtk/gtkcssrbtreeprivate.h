/* gtkrb_tree.h
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* A Red-Black Tree implementation used specifically by GtkTreeView.
 */
#ifndef __GTK_CSS_RB_TREE_H__
#define __GTK_CSS_RB_TREE_H__

#include <glib.h>


G_BEGIN_DECLS


typedef struct _GtkCssRbTree GtkCssRbTree;

typedef void            (* GtkCssRbTreeAugmentFunc)     (GtkCssRbTree           *tree,
                                                         gpointer                node_augment,
                                                         gpointer                node,
                                                         gpointer                left,
                                                         gpointer                right);
typedef int             (* GtkCssRbTreeFindFunc)        (GtkCssRbTree           *tree,
                                                         gpointer                node,
                                                         gpointer                user_data);

GtkCssRbTree *          gtk_css_rb_tree_new_for_size    (gsize                   element_size,
                                                         gsize                   augment_size,
                                                         GtkCssRbTreeAugmentFunc augment_func,
                                                         GDestroyNotify          clear_func,
                                                         GDestroyNotify          clear_augment_func);
#define gtk_css_rb_tree_new(type, augment_type, augment_func, clear_func, clear_augment_func) \
  gtk_css_rb_tree_new_for_size (sizeof (type), sizeof (augment_type), (augment_func), (clear_func), (clear_augment_func))

GtkCssRbTree *          gtk_css_rb_tree_ref             (GtkCssRbTree           *tree);
void                    gtk_css_rb_tree_unref           (GtkCssRbTree           *tree);

gpointer                gtk_css_rb_tree_get_first       (GtkCssRbTree           *tree);
gpointer                gtk_css_rb_tree_get_last        (GtkCssRbTree           *tree);
gpointer                gtk_css_rb_tree_get_previous    (GtkCssRbTree           *tree,
                                                         gpointer                node);
gpointer                gtk_css_rb_tree_get_next        (GtkCssRbTree           *tree,
                                                         gpointer                node);

gpointer                gtk_css_rb_tree_get_root        (GtkCssRbTree           *tree);
gpointer                gtk_css_rb_tree_get_parent      (GtkCssRbTree           *tree,
                                                         gpointer                node);
gpointer                gtk_css_rb_tree_get_left        (GtkCssRbTree           *tree,
                                                         gpointer                node);
gpointer                gtk_css_rb_tree_get_right       (GtkCssRbTree           *tree,
                                                         gpointer                node);
gpointer                gtk_css_rb_tree_get_augment     (GtkCssRbTree           *tree,
                                                         gpointer                node);

void                    gtk_css_rb_tree_mark_dirty      (GtkCssRbTree           *tree,
                                                         gpointer                node);

gpointer                gtk_css_rb_tree_insert_before   (GtkCssRbTree           *tree,
                                                         gpointer                node);
gpointer                gtk_css_rb_tree_insert_after    (GtkCssRbTree           *tree,
                                                         gpointer                node);
void                    gtk_css_rb_tree_remove          (GtkCssRbTree           *tree,
                                                         gpointer                node);

gpointer                gtk_css_rb_tree_find            (GtkCssRbTree           *tree,
                                                         gpointer               *out_before,
                                                         gpointer               *out_after,
                                                         GtkCssRbTreeFindFunc    find_func,
                                                         gpointer                user_data);



G_END_DECLS


#endif /* __GTK_CSS_RB_TREE_H__ */
