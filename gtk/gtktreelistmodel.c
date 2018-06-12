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

#include "gtktreelistmodel.h"

#include "gtkcssrbtreeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_AUTOEXPAND,
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
    GtkTreeListModel *list;
  };

  guint empty : 1;
  guint is_root : 1;
};

struct _TreeAugment
{
  guint n_items;
  guint n_local;
};

struct _GtkTreeListModel
{
  GObject parent_instance;

  TreeNode root_node;

  GtkTreeListModelCreateModelFunc create_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  guint autoexpand : 1;
};

struct _GtkTreeListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GtkTreeListModel *
tree_node_get_tree_list_model (TreeNode *node)
{
  for (; !node->is_root; node = node->parent)
    { }

  return node->list;
}

static TreeNode *
tree_node_get_nth_child (TreeNode *node,
                         guint     position)
{
  GtkCssRbTree *tree;
  TreeNode *child, *tmp;
  TreeAugment *aug;

  tree = node->children;
  child = gtk_css_rb_tree_get_root (tree);

  while (TRUE)
    {
      tmp = gtk_css_rb_tree_get_left (tree, child);
      if (tmp)
        {
          aug = gtk_css_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_local)
            {
              child = tmp;
              continue;
            }
          position -= aug->n_local;
        }

      if (position == 0)
        return child;

      position--;

      child = gtk_css_rb_tree_get_right (tree, child);
    }

  g_return_val_if_reached (NULL);
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

static guint
tree_node_get_position (TreeNode *node)
{
  GtkCssRbTree *tree;
  TreeNode *left, *parent;
  TreeAugment *left_aug;
  guint n;
  
  for (n = 0;
       !node->is_root;
       node = node->parent, n++)
    {
      tree = node->parent->children;

      left = gtk_css_rb_tree_get_left (tree, node);
      if (left)
        {
          left_aug = gtk_css_rb_tree_get_augment (tree, left);
          n += left_aug->n_items;
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
              n += 1 + tree_node_get_n_children (parent);
              if (left)
                {
                  left_aug = gtk_css_rb_tree_get_augment (tree, left);
                  n += left_aug->n_items;
                }
            }
          node = parent;
        }
    }
  /* the root isn't visible */
  n--;

  return n;
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

static TreeNode *
gtk_tree_list_model_get_nth (GtkTreeListModel *self,
                             guint             position)
{
  GtkCssRbTree *tree;
  TreeNode *node, *tmp;
  guint n_children;

  n_children = tree_node_get_n_children (&self->root_node);
  if (n_children <= position)
    return NULL;

  tree = self->root_node.children;
  node = gtk_css_rb_tree_get_root (tree);

  while (TRUE)
    {
      tmp = gtk_css_rb_tree_get_left (tree, node);
      if (tmp)
        {
          TreeAugment *aug = gtk_css_rb_tree_get_augment (tree, tmp);
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

      n_children = tree_node_get_n_children (node);
      if (position < n_children)
        {
          tree = node->children;
          node = gtk_css_rb_tree_get_root (tree);
          continue;
        }
      position -= n_children;

      node = gtk_css_rb_tree_get_right (tree, node);
    }

  g_return_val_if_reached (NULL);
}

static GType
gtk_tree_list_model_get_item_type (GListModel *list)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (list);

  return g_list_model_get_item_type (self->root_node.model);
}

static guint
gtk_tree_list_model_get_n_items (GListModel *list)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (list);

  return tree_node_get_n_children (&self->root_node);
}

static gpointer
gtk_tree_list_model_get_item (GListModel *list,
                              guint       position)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (list);
  TreeNode *node, *parent;

  node = gtk_tree_list_model_get_nth (self, position);
  if (node == NULL)
    return NULL;

  parent = node->parent;
  return g_list_model_get_item (parent->model,
                                tree_node_get_local_position (parent->children, node));
}

static void
gtk_tree_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_tree_list_model_get_item_type;
  iface->get_n_items = gtk_tree_list_model_get_n_items;
  iface->get_item = gtk_tree_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkTreeListModel, gtk_tree_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_tree_list_model_model_init))

static void
gtk_tree_list_model_augment (GtkCssRbTree *tree,
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

static guint
gtk_tree_list_model_expand_node (GtkTreeListModel *self,
                                 TreeNode         *node);

static void
gtk_tree_list_model_items_changed_cb (GListModel *model,
                                      guint       position,
                                      guint       removed,
                                      guint       added,
                                      TreeNode   *node)
{
  GtkTreeListModel *self;
  TreeNode *child;
  guint i, tree_position, tree_removed, tree_added, n_local;

  self = tree_node_get_tree_list_model (node);
  n_local = g_list_model_get_n_items (model) - added + removed;

  if (position < n_local)
    {
      child = tree_node_get_nth_child (node, position);
      tree_position = tree_node_get_position (child);
    }
  else
    {
      child = NULL;
      tree_position = tree_node_get_position (node) + tree_node_get_n_children (node) + 1;
    }

  if (removed)
    {
      TreeNode *tmp;

      g_assert (child != NULL);
      if (position + removed < n_local)
        {
          TreeNode *end = tree_node_get_nth_child (node, position + removed);
          tree_removed = tree_node_get_position (end) - tree_position;
        }
      else
        {
          tree_removed = tree_node_get_position (node) + tree_node_get_n_children (node) + 1 - tree_position;
        }

      for (i = 0; i < removed; i++)
        {
          tmp = child;
          child = gtk_css_rb_tree_get_next (node->children, child);
          gtk_css_rb_tree_remove (node->children, tmp);
        }
    }
  else
    {
      tree_removed = 0;
    }

  tree_added = added;
  for (i = 0; i < added; i++)
    {
      child = gtk_css_rb_tree_insert_before (node->children, child);
      child->parent = node;
      if (self->autoexpand)
        tree_added += gtk_tree_list_model_expand_node (self, child);
    }

  tree_node_mark_dirty (node);

  g_list_model_items_changed (G_LIST_MODEL (self),
                              tree_position,
                              tree_removed,
                              tree_added);
}

static void
gtk_tree_list_model_clear_node (gpointer data)
{
  TreeNode *node = data;

  if (node->model)
    {
      g_signal_handlers_disconnect_by_func (node->model,
                                            gtk_tree_list_model_items_changed_cb,
                                            node);
      g_object_unref (node->model);
    }
  if (node->children)
    gtk_css_rb_tree_unref (node->children);
}

static void
gtk_tree_list_model_init_node (GtkTreeListModel *list,
                               TreeNode         *self,
                               GListModel       *model)
{
  gsize i, n;
  TreeNode *node;

  self->model = g_object_ref (model);
  g_signal_connect (model,
                    "items-changed",
                    G_CALLBACK (gtk_tree_list_model_items_changed_cb),
                    self);
  self->children = gtk_css_rb_tree_new (TreeNode,
                                        TreeAugment,
                                        gtk_tree_list_model_augment,
                                        gtk_tree_list_model_clear_node,
                                        NULL);

  n = g_list_model_get_n_items (model);
  node = NULL;
  for (i = 0; i < n; i++)
    {
      node = gtk_css_rb_tree_insert_after (self->children, node);
      node->parent = self;
      if (list->autoexpand)
        gtk_tree_list_model_expand_node (list, node);
    }
}

static void
gtk_tree_list_model_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_AUTOEXPAND:
      gtk_tree_list_model_set_autoexpand (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_tree_list_model_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_AUTOEXPAND:
      g_value_set_boolean (value, self->autoexpand);
      break;

    case PROP_ROOT_MODEL:
      g_value_set_object (value, self->root_node.model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_model_finalize (GObject *object)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (object);

  gtk_tree_list_model_clear_node (&self->root_node);
  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_tree_list_model_parent_class)->finalize (object);
};

static void
gtk_tree_list_model_class_init (GtkTreeListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_tree_list_model_set_property;
  gobject_class->get_property = gtk_tree_list_model_get_property;
  gobject_class->finalize = gtk_tree_list_model_finalize;

  /**
   * GtkTreeListModel:autoexpand:
   *
   * If all rows should be expanded by default
   */
  properties[PROP_AUTOEXPAND] =
      g_param_spec_boolean ("autoexpand",
                            P_("autoexpand"),
                            P_("If all rows should be expanded by default"),
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeListModel:root-model:
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
gtk_tree_list_model_init (GtkTreeListModel *self)
{
  self->root_node.list = self;
  self->root_node.is_root = TRUE;
}

/**
 * gtk_tree_list_model_new:
 * @root: The #GListModel to use as root
 * @autoexpand: %TRUE to set the autoexpand property and expand the @root model
 * @create_func: Function to call to create the #GListModel for the children
 *     of an item
 * @user_data: Data to pass to @create_func
 * @user_destroy: Function to call to free @user_data
 *
 * Creates a new empty #GtkTreeListModel displaying @root with all rows collapsed.
 * 
 * Returns: a newly created #GtkTreeListModel. 
 **/
GtkTreeListModel *
gtk_tree_list_model_new (GListModel                      *root,
                         gboolean                         autoexpand,
                         GtkTreeListModelCreateModelFunc  create_func,
                         gpointer                         user_data,
                         GDestroyNotify                   user_destroy)
{
  GtkTreeListModel *self;

  g_return_val_if_fail (G_IS_LIST_MODEL (root), NULL);
  g_return_val_if_fail (create_func != NULL, NULL);

  self = g_object_new (GTK_TYPE_TREE_LIST_MODEL,
                       "autoexpand", autoexpand,
                       NULL);

  self->create_func = create_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  gtk_tree_list_model_init_node (self, &self->root_node, g_object_ref (root));

  return self;
}

/**
 * gtk_tree_list_model_set_autoexpand:
 * @self: a #GtkTreeListModel
 * @autoexpand: %TRUE to make the model autoexpand its rows
 *
 * If set to %TRUE, the model will recursively expand all rows that
 * get added to the model. This can be either rows added by changes
 * to the underlying models or via gtk_tree_list_model_set_expanded().
 **/
void
gtk_tree_list_model_set_autoexpand (GtkTreeListModel *self,
                                    gboolean          autoexpand)
{
  g_return_if_fail (GTK_IS_TREE_LIST_MODEL (self));

  if (self->autoexpand == autoexpand)
    return;

  self->autoexpand = autoexpand;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTOEXPAND]);
}

/**
 * gtk_tree_list_model_get_autoexpand:
 * @self: a #GtkTreeListModel
 *
 * Gets whether the model is set to automatically expand new rows
 * that get added. This can be either rows added by changes to the
 * underlying models or via gtk_tree_list_model_set_expanded().
 *
 * Returns: %TRUE if the model is set to autoexpand
 **/
gboolean
gtk_tree_list_model_get_autoexpand (GtkTreeListModel *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), FALSE);

  return self->autoexpand;
}

guint
gtk_tree_list_model_get_depth (GtkTreeListModel *self,
                               guint             position)
{
  TreeNode *node;
  guint depth;

  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), FALSE);

  node = gtk_tree_list_model_get_nth (self, position);
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
tree_node_create_model (GtkTreeListModel *self,
                        TreeNode         *node)
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

static guint
gtk_tree_list_model_expand_node (GtkTreeListModel *self,
                                 TreeNode         *node)
{
  GListModel *model;

  if (node->empty)
    return 0;
  
  if (node->model != NULL)
    return 0;

  model = tree_node_create_model (self, node);

  if (model == NULL)
    return 0;
  
  g_assert (g_list_model_get_item_type (model) == g_list_model_get_item_type (self->root_node.model));
  gtk_tree_list_model_init_node (self, node, model);

  tree_node_mark_dirty (node);
  
  return tree_node_get_n_children (node);
}

static void
gtk_tree_list_model_collapse_node (GtkTreeListModel *self,
                                   guint             position,
                                   TreeNode         *node)
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
gtk_tree_list_model_set_expanded (GtkTreeListModel *self,
                                  guint             position,
                                  gboolean          expanded)
{
  TreeNode *node;
  guint n_items;

  g_return_if_fail (GTK_IS_TREE_LIST_MODEL (self));

  node = gtk_tree_list_model_get_nth (self, position);
  if (node == NULL)
    return;

  if (expanded)
    {
      n_items = gtk_tree_list_model_expand_node (self, node);
      if (n_items > 0)
        g_list_model_items_changed (G_LIST_MODEL (self), position + 1, 0, n_items);
    }
  else
    {
      gtk_tree_list_model_collapse_node (self, position, node);
    }
}

gboolean
gtk_tree_list_model_get_expanded (GtkTreeListModel *self,
                                  guint             position)
{
  TreeNode *node;

  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), FALSE);

  node = gtk_tree_list_model_get_nth (self, position);
  if (node == NULL)
    return FALSE;

  return node->children != NULL;
}

gboolean
gtk_tree_list_model_is_expandable (GtkTreeListModel *self,
                                   guint             position)
{
  GListModel *model;
  TreeNode *node;

  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), FALSE);

  node = gtk_tree_list_model_get_nth (self, position);
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

