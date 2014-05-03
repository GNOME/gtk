/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef _GTKPARASITE_WIDGET_TREE_H_
#define _GTKPARASITE_WIDGET_TREE_H_


#include <gtk/gtk.h>


#define PARASITE_TYPE_WIDGET_TREE            (parasite_widget_tree_get_type())
#define PARASITE_WIDGET_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PARASITE_TYPE_WIDGET_TREE, ParasiteWidgetTree))
#define PARASITE_WIDGET_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PARASITE_TYPE_WIDGET_TREE, ParasiteWidgetTreeClass))
#define PARASITE_IS_WIDGET_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PARASITE_TYPE_WIDGET_TREE))
#define PARASITE_IS_WIDGET_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PARASITE_TYPE_WIDGET_TREE))
#define PARASITE_WIDGET_TREE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PARASITE_TYPE_WIDGET_TREE, ParasiteWidgetTreeClass))


typedef struct _ParasiteWidgetTreePrivate ParasiteWidgetTreePrivate;

typedef struct _ParasiteWidgetTree {
   GtkTreeView parent;

   // Private
   ParasiteWidgetTreePrivate *priv;
} ParasiteWidgetTree;

typedef struct _ParasiteWidgetTreeClass {
   GtkTreeViewClass parent;

    void (*widget_changed)(ParasiteWidgetTree *tree);
} ParasiteWidgetTreeClass;


G_BEGIN_DECLS


GType      parasite_widget_tree_get_type            (void);
GtkWidget *parasite_widget_tree_new                 (void);

GObject   *parasite_widget_tree_get_selected_object (ParasiteWidgetTree *widget_tree);

void       parasite_widget_tree_scan                (ParasiteWidgetTree *widget_tree,
                                                     GtkWidget *window);
void       parasite_widget_tree_select_object       (ParasiteWidgetTree *widget_tree,
                                                     GObject            *object);
void       parasite_widget_tree_append_object       (ParasiteWidgetTree *widget_tree,
                                                     GObject            *object,
                                                     GtkTreeIter        *parent_iter);
gboolean   parasite_widget_tree_find_object         (ParasiteWidgetTree *widget_tree,
                                                     GObject            *object,
                                                     GtkTreeIter        *iter);

G_END_DECLS


#endif // _GTKPARASITE_WIDGETTREE_H_

// vim: set et sw=4 ts=4:
