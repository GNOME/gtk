/* gtkmodelsimple.h
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

#ifndef __GTK_MODEL_SIMPLE_H__
#define __GTK_MODEL_SIMPLE_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if 0

#define GTK_TYPE_MODEL_SIMPLE			(gtk_model_simple_get_type ())
#define GTK_MODEL_SIMPLE(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_MODEL_SIMPLE, GtkModelSimple))
#define GTK_MODEL_SIMPLE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MODEL_SIMPLE, GtkModelSimpleClass))
#define GTK_IS_MODEL_SIMPLE(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_MODEL_SIMPLE))
#define GTK_IS_MODEL_SIMPLE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_MODEL_SIMPLE))

typedef struct _GtkModelSimple       GtkModelSimple;
typedef struct _GtkModelSimpleClass  GtkModelSimpleClass;

struct _GtkModelSimple
{
  GtkObject parent;
};

struct _GtkModelSimpleClass
{
  GtkObjectClass parent_class;

  /* signals */
  /* Will be moved into the GtkTreeModelIface eventually */
  void       (* changed)         (GtkTreeModel *tree_model,
				       GtkTreePath  *path,
				       GtkTreeNode   node);
  void       (* inserted)        (GtkTreeModel *tree_model,
				       GtkTreePath  *path,
				       GtkTreeNode   node);
  void       (* child_toggled)   (GtkTreeModel *tree_model,
				       GtkTreePath  *path,
				       GtkTreeNode   node);
  void       (* deleted)         (GtkTreeModel *tree_model,
				       GtkTreePath  *path);
};


GtkType    gtk_model_simple_get_type           (void);
GtkObject *gtk_model_simple_new                (void);

void       gtk_model_simple_node_changed       (GtkModelSimple *simple,
						GtkTreePath    *path,
						GtkTreeNode     node);
void       gtk_model_simple_node_inserted      (GtkModelSimple *simple,
						GtkTreePath    *path,
						GtkTreeNode     node);
void       gtk_model_simple_node_child_toggled (GtkModelSimple *simple,
						GtkTreePath    *path,
						GtkTreeNode     node);
void       gtk_model_simple_node_deleted       (GtkModelSimple *simple,
						GtkTreePath    *path,
						GtkTreeNode     node);


#endif
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MODEL_SIMPLE_H__ */
