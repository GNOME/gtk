/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_TREE_H__
#define __GTK_TREE_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TREE(obj)          GTK_CHECK_CAST (obj, gtk_tree_get_type (), GtkTree)
#define GTK_TREE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_tree_get_type (), GtkTreeClass)
#define GTK_IS_TREE(obj)       GTK_CHECK_TYPE (obj, gtk_tree_get_type ())


typedef struct _GtkTree       GtkTree;
typedef struct _GtkTreeClass  GtkTreeClass;

struct _GtkTree
{
  GtkContainer container;

  GList *children;
};

struct _GtkTreeClass
{
  GtkContainerClass parent_class;
};


guint      gtk_tree_get_type   (void);
GtkWidget* gtk_tree_new        (void);
void       gtk_tree_append     (GtkTree   *tree,
				GtkWidget *child);
void       gtk_tree_prepend    (GtkTree   *tree,
				GtkWidget *child);
void       gtk_tree_insert     (GtkTree   *tree,
				GtkWidget *child,
				gint       position);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_H__ */
