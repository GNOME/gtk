/* gtktreestore.h
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_TREE_MODEL_RENDER_NODE_H__
#define __GTK_TREE_MODEL_RENDER_NODE_H__

#include <gsk/gsk.h>

#include <gtk/gtktreemodel.h>


G_BEGIN_DECLS


#define GTK_TYPE_TREE_MODEL_RENDER_NODE			(gtk_tree_model_render_node_get_type ())
#define GTK_TREE_MODEL_RENDER_NODE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_MODEL_RENDER_NODE, GtkTreeModelRenderNode))
#define GTK_TREE_MODEL_RENDER_NODE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_MODEL_RENDER_NODE, GtkTreeModelRenderNodeClass))
#define GTK_IS_TREE_MODEL_RENDER_NODE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_MODEL_RENDER_NODE))
#define GTK_IS_TREE_MODEL_RENDER_NODE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TREE_MODEL_RENDER_NODE))
#define GTK_TREE_MODEL_RENDER_NODE_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TREE_MODEL_RENDER_NODE, GtkTreeModelRenderNodeClass))

typedef struct _GtkTreeModelRenderNode        GtkTreeModelRenderNode;
typedef struct _GtkTreeModelRenderNodeClass   GtkTreeModelRenderNodeClass;
typedef struct _GtkTreeModelRenderNodePrivate GtkTreeModelRenderNodePrivate;

typedef void (* GtkTreeModelRenderNodeGetFunc)          (GtkTreeModelRenderNode *model,
                                                         GskRenderNode          *node,
                                                         int                     column,
                                                         GValue                 *value);

struct _GtkTreeModelRenderNode
{
  GObject parent;

  GtkTreeModelRenderNodePrivate *priv;
};

struct _GtkTreeModelRenderNodeClass
{
  GObjectClass parent_class;
};


GType         gtk_tree_model_render_node_get_type               (void) G_GNUC_CONST;

GtkTreeModel *gtk_tree_model_render_node_new                    (GtkTreeModelRenderNodeGetFunc get_func,
                                                                 gint                    n_columns,
                                                                 ...);
GtkTreeModel *gtk_tree_model_render_node_newv                   (GtkTreeModelRenderNodeGetFunc get_func,
                                                                 gint                    n_columns,
                                                                 GType                  *types);

void          gtk_tree_model_render_node_set_root_node          (GtkTreeModelRenderNode *model,
                                                                 GskRenderNode          *node);
GskRenderNode*gtk_tree_model_render_node_get_root_node          (GtkTreeModelRenderNode *model);
GskRenderNode*gtk_tree_model_render_node_get_node_from_iter     (GtkTreeModelRenderNode *model,
                                                                 GtkTreeIter            *iter);


G_END_DECLS


#endif /* __GTK_TREE_MODEL_RENDER_NODE_H__ */
