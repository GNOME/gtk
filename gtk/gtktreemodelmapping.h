/* gtktreemodelmapping.h
 * Copyright (C) 2000  Red Hat, Inc.,  Alexander Larsson <alexl@redhat.com>
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

#ifndef __GTK_TREE_MODEL_MAPPING_H__
#define __GTK_TREE_MODEL_MAPPING_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_MODEL_MAPPING		(gtk_tree_model_mapping_get_type ())
#define GTK_TREE_MODEL_MAPPING(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_MODEL_MAPPING, GtkTreeModelMapping))
#define GTK_TREE_MODEL_MAPPING_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_MODEL_MAPPING, GtkTreeModelMappingClass))
#define GTK_IS_TREE_MODEL_MAPPING(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_MODEL_MAPPING))
#define GTK_IS_TREE_MODEL_MAPPING_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_MODEL_MAPPING))

typedef struct _GtkTreeModelMapping       GtkTreeModelMapping;
typedef struct _GtkTreeModelMappingClass  GtkTreeModelMappingClass;
typedef struct _GtkTreeModelMappingMap    GtkTreeModelMappingMap;

typedef void (* GValueMapFunc) (const GValue *a,
				GValue *b,
				gpointer user_data);

  
struct _GtkTreeModelMappingMap
{
  gint src_column;
  GType col_type;
  GValueMapFunc map_func;
  gpointer user_data;
};
  
struct _GtkTreeModelMapping
{
  GtkObject parent;

  /* < private > */
  GtkTreeModel *child_model;
  gint n_columns;
  GtkTreeModelMappingMap *column_maps;
};

struct _GtkTreeModelMappingClass
{
  GtkObjectClass parent_class;

  /* signals */
  /* Will be moved into the GtkTreeModelIface eventually */
  void (* changed)       (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter);
  void (* inserted)      (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter);
  void (* child_toggled) (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter);
  void (* deleted)       (GtkTreeModel *tree_model,
			  GtkTreePath  *path);
};


GtkType       gtk_tree_model_mapping_get_type           (void);
GtkTreeModel *gtk_tree_model_mapping_new                (void);
GtkTreeModel *gtk_tree_model_mapping_new_with_model     (GtkTreeModel        *child_model);
void          gtk_tree_model_mapping_set_n_columns      (GtkTreeModelMapping *tree_model_mapping,
							 gint                 n_columns);
void          gtk_tree_model_mapping_set_column_mapping (GtkTreeModelMapping *tree_model_mapping,
							 gint                 column,
							 gint                 src_column,
							 GType                col_type,
							 GValueMapFunc        map_func,
							 gpointer             user_data);
void          gtk_tree_model_mapping_set_model          (GtkTreeModelMapping *tree_model_mapping,
							 GtkTreeModel        *child_model);
GtkTreeModel *gtk_tree_model_mapping_get_model          (GtkTreeModelMapping *tree_model);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_MODEL_MAPPING_H__ */
