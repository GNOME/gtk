/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtktreelist.h"

#include "gtkcssrbtreeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_ROOT_MODEL,
  NUM_PROPERTIES
};

typedef struct _TreeNode TreeNode;
typedef struct _TreeAugment TreeAugment;

struct _TreeNode
{
  GListModel *model;
  GtkCssRbTree *children;
  union {
    TreeNode *parent;
    GtkTreeList *list;
  };

  guint empty : 1;
  guint is_root : 1;
};

struct _TreeAugment
{
  guint n_items;
  guint n_local;
};

struct _GtkTreeList
{
  GObject parent_instance;

  TreeNode root_node;

  GtkTreeListCreateModelFunc create_func;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

struct _GtkTreeListClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static TreeNode *
gtk_tree_list_get_nth (GtkTreeList *self,
                       guint        position)
{
  GtkCssRbTree *tree;
  TreeNode *node, *tmp;
  TreeAugment *aug;

  tree = self->root_node.children;
  node = gtk_css_rb_tree_get_root (tree);
  aug = gtk_css_rb_tree_get_augment (tree, node);
  if (aug->n_items <= position)
    return NULL;

  while (TRUE)
    {
      tmp = gtk_css_rb_tree_get_left (tree, node);
      if (tmp)
        {
          aug = gtk_css_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_items)
            {
              node = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      if (position == 0)
        return node;

      position--;

      if (node->children)
        {
          tmp = gtk_css_rb_tree_get_root (node->children);
          aug = gtk_css_rb_tree_get_augment (node->children, tmp);
          if (position < aug->n_items)
            {
              tree = node->children;
              node = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      node = gtk_css_rb_tree_get_right (tree, node);
    }

  g_return_val_if_reached (NULL);
}

static guint
tree_node_get_local_position (GtkCssRbTree *tree,
                              TreeNode     *node)
{
  TreeNode *left, *parent;
  TreeAugment *left_aug;
  guint n;
  
  left = gtk_css_rb_tree_get_left (tree, node);
  if (left)
    {
      left_aug = gtk_css_rb_tree_get_augment (tree, left);
      n = left_aug->n_local;
    }
  else
    {
      n = 0;
    }

  for (parent = gtk_css_rb_tree_get_parent (tree, node);
       parent;
       parent = gtk_css_rb_tree_get_parent (tree, node))
    {
      left = gtk_css_rb_tree_get_left (tree, parent);
      if (left == node)
        {
          /* we are the left node, nothing changes */
        }
      else
        {
          /* we are the right node */
          n++;
          if (left)
            {
              left_aug = gtk_css_rb_tree_get_augment (tree, left);
              n += left_aug->n_local;
            }
        }
      node = parent;
    }

  return n;
}

static GType
gtk_tree_list_get_item_type (GListModel *list)
{
  GtkTreeList *self = GTK_TREE_LIST (list);

  return g_list_model_get_item_type (self->root_node.model);
}

static void
tree_node_mark_dirty (TreeNode *node)
{
  for (;
       !node->is_root;
       node = node->parent)
    {
      gtk_css_rb_tree_mark_dirty (node->parent->children, node);
    }
}

static guint
tree_node_get_n_children (TreeNode *node)
{
  TreeAugment *child_aug;
  TreeNode *child_node;

  if (node->children == NULL)
    return 0;

  child_node = gtk_css_rb_tree_get_root (node->children);
  if (child_node == NULL)
    return 0;

  child_aug = gtk_css_rb_tree_get_augment (node->children, child_node);

  return child_aug->n_items;
}

static guint
gtk_tree_list_get_n_items (GListModel *list)
{
  GtkTreeList *self = GTK_TREE_LIST (list);

  return tree_node_get_n_children (&self->root_node);
}

static gpointer
gtk_tree_list_get_item (GListModel *list,
                        guint       position)
{
  GtkTreeList *self = GTK_TREE_LIST (list);
  TreeNode *node, *parent;

  node = gtk_tree_list_get_nth (self, position);
  if (node == NULL)
    return NULL;

  parent = node->parent;
  return g_list_model_get_item (parent->model,
                                tree_node_get_local_position (parent->children, node));
}

static void
gtk_tree_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_tree_list_get_item_type;
  iface->get_n_items = gtk_tree_list_get_n_items;
  iface->get_item = gtk_tree_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkTreeList, gtk_tree_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_tree_list_model_init))

static void
gtk_tree_list_augment (GtkCssRbTree *tree,
                       gpointer      _aug,
                       gpointer      _node,
                       gpointer      left,
                       gpointer      right)
{
  TreeAugment *aug = _aug;

  aug->n_items = 1;
  aug->n_items += tree_node_get_n_children (_node);
  aug->n_local = 1;

  if (left)
    {
      TreeAugment *left_aug = gtk_css_rb_tree_get_augment (tree, left);
      aug->n_items += left_aug->n_items;
      aug->n_local += left_aug->n_local;
    }
  if (right)
    {
      TreeAugment *right_aug = gtk_css_rb_tree_get_augment (tree, right);
      aug->n_items += right_aug->n_items;
      aug->n_local += right_aug->n_local;
    }
}

static void
gtk_tree_list_clear_node (gpointer data)
{
  TreeNode *node = data;

  if (node->model)
    g_object_unref (node->model);
  if (node->children)
    gtk_css_rb_tree_unref (node->children);
}

static void
gtk_tree_list_init_node (GtkTreeList *list,
                         TreeNode    *self,
                         GListModel  *model)
{
  gsize i, n;
  TreeNode *node;

  self->model = g_object_ref (model);
  self->children = gtk_css_rb_tree_new (TreeNode,
                                        TreeAugment,
                                        gtk_tree_list_augment,
                                        gtk_tree_list_clear_node,
                                        NULL);

  n = g_list_model_get_n_items (model);
  node = NULL;
  for (i = 0; i < n; i++)
    {
      node = gtk_css_rb_tree_insert_after (self->children, node);
      node->parent = self;
    }
}

static void
gtk_tree_list_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  /* GtkTreeList *self = GTK_TREE_LIST (object); */

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_tree_list_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  GtkTreeList *self = GTK_TREE_LIST (object);

  switch (prop_id)
    {
    case PROP_ROOT_MODEL:
      g_value_set_object (value, self->root_node.model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_finalize (GObject *object)
{
  GtkTreeList *self = GTK_TREE_LIST (object);

  gtk_tree_list_clear_node (&self->root_node);
  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_tree_list_parent_class)->finalize (object);
};

static void
gtk_tree_list_class_init (GtkTreeListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_tree_list_set_property;
  gobject_class->get_property = gtk_tree_list_get_property;
  gobject_class->finalize = gtk_tree_list_finalize;

  /**
   * GtkTreeList:root-model:
   *
   * The root model displayed
   */
  properties[PROP_ROOT_MODEL] =
      g_param_spec_object ("root-model",
                           P_("Root model"),
                           P_("The root model displayed"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_tree_list_init (GtkTreeList *self)
{
  self->root_node.list = self;
  self->root_node.is_root = TRUE;
}

/**
 * gtk_tree_list_new:
 * @root: The #GListModel to use as root
 * @create_func: Function to call to create the #GListModel for the children
 *     of an item
 * @user_data: Data to pass to @create_func
 * @user_destroy: Function to call to free @user_data
 *
 * Creates a new empty #GtkTreeList displaying @root with all rows collapsed.
 * 
 * Returns: a newly created #GtkTreeList. 
 **/
GListModel *
gtk_tree_list_new (GListModel                 *root,
                   GtkTreeListCreateModelFunc  create_func,
                   gpointer                    user_data,
                   GDestroyNotify              user_destroy)
{
  GtkTreeList *self;

  g_return_val_if_fail (G_IS_LIST_MODEL (root), NULL);
  g_return_val_if_fail (create_func != NULL, NULL);

  self = g_object_new (GTK_TYPE_TREE_LIST,
                       NULL);

  gtk_tree_list_init_node (self, &self->root_node, g_object_ref (root));
  self->create_func = create_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  return G_LIST_MODEL (self);
}

guint
gtk_tree_list_get_depth (GtkTreeList *self,
                         guint        position)
{
  TreeNode *node;
  guint depth;

  g_return_val_if_fail (GTK_IS_TREE_LIST (self), FALSE);

  node = gtk_tree_list_get_nth (self, position);
  if (node == NULL)
    return 0;

  depth = 0;
  for (node = node->parent;
       !node->is_root;
       node = node->parent)
    depth++;

  return depth;
}

static GListModel *
tree_node_create_model (GtkTreeList *self,
                        TreeNode    *node)
{
  TreeNode *parent = node->parent;
  GListModel *model;
  GObject *item;

  item = g_list_model_get_item (parent->model,
                                tree_node_get_local_position (parent->children, node));
  model = self->create_func (item, self->user_data);
  g_object_unref (item);
  if (model == NULL)
    node->empty = TRUE;

  return model;
}

static void
gtk_tree_list_expand_node (GtkTreeList *self,
                           guint        position,
                           TreeNode    *node)
{
  GListModel *model;
  guint n_items;

  if (node->empty)
    return;
  
  if (node->model != NULL)
    return;

  model = tree_node_create_model (self, node);

  if (model == NULL)
    return;
  
  g_assert (g_list_model_get_item_type (model) == g_list_model_get_item_type (self->root_node.model));
  gtk_tree_list_init_node (self, node, model);

  tree_node_mark_dirty (node);
  n_items = tree_node_get_n_children (node);

  if (n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), position + 1, 0, n_items);
}

static void
gtk_tree_list_collapse_node (GtkTreeList *self,
                             guint        position,
                             TreeNode    *node)
{
  guint n_items;

  if (node->model == NULL)
    return;

  n_items = tree_node_get_n_children (node);

  g_clear_pointer (&node->children, gtk_css_rb_tree_unref);
  g_clear_object (&node->model);

  tree_node_mark_dirty (node);

  if (n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), position + 1, n_items, 0);
}

void
gtk_tree_list_set_expanded (GtkTreeList *self,
                            guint        position,
                            gboolean     expanded)
{
  TreeNode *node;

  g_return_if_fail (GTK_IS_TREE_LIST (self));

  node = gtk_tree_list_get_nth (self, position);
  if (node == NULL)
    return;

  if (expanded)
    gtk_tree_list_expand_node (self, position, node);
  else
    gtk_tree_list_collapse_node (self, position, node);
}

gboolean
gtk_tree_list_get_expanded (GtkTreeList *self,
                            guint        position)
{
  TreeNode *node;

  g_return_val_if_fail (GTK_IS_TREE_LIST (self), FALSE);

  node = gtk_tree_list_get_nth (self, position);
  if (node == NULL)
    return FALSE;

  return node->children != NULL;
}

gboolean
gtk_tree_list_is_expandable (GtkTreeList *self,
                             guint        position)
{
  GListModel *model;
  TreeNode *node;

  g_return_val_if_fail (GTK_IS_TREE_LIST (self), FALSE);

  node = gtk_tree_list_get_nth (self, position);
  if (node == NULL)
    return FALSE;

  if (node->empty)
    return FALSE;

  if (node->model)
    return TRUE;

  model = tree_node_create_model (self, node);
  if (model)
    {
      g_object_unref (model);
      return TRUE;
    }

  return FALSE;
}

