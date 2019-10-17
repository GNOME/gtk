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

#include "gtkrbtreeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtktreelistmodel
 * @Short_description: A list model that can create child models on demand
 * @Title: GtkTreeListModel
 * @See_also: #GListModel
 *
 * #GtkTreeListModel is a #GListModel implementation that can expand rows
 * by creating new child list models on demand.
 */

enum {
  PROP_0,
  PROP_AUTOEXPAND,
  PROP_MODEL,
  PROP_PASSTHROUGH,
  NUM_PROPERTIES
};

typedef struct _TreeNode TreeNode;
typedef struct _TreeAugment TreeAugment;

struct _TreeNode
{
  GListModel *model;
  GtkTreeListRow *row;
  GtkRbTree *children;
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
  guint passthrough : 1;
};

struct _GtkTreeListModelClass
{
  GObjectClass parent_class;
};

struct _GtkTreeListRow
{
  GObject parent_instance;

  TreeNode *node; /* NULL when the row has been destroyed */
};

struct _GtkTreeListRowClass
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
  GtkRbTree *tree;
  TreeNode *child, *tmp;
  TreeAugment *aug;

  tree = node->children;
  child = gtk_rb_tree_get_root (tree);

  while (child)
    {
      tmp = gtk_rb_tree_node_get_left (child);
      if (tmp)
        {
          aug = gtk_rb_tree_get_augment (tree, tmp);
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

      child = gtk_rb_tree_node_get_right (child);
    }

  return NULL;
}

static guint
tree_node_get_n_children (TreeNode *node)
{
  TreeAugment *child_aug;
  TreeNode *child_node;

  if (node->children == NULL)
    return 0;

  child_node = gtk_rb_tree_get_root (node->children);
  if (child_node == NULL)
    return 0;

  child_aug = gtk_rb_tree_get_augment (node->children, child_node);

  return child_aug->n_items;
}

static guint
tree_node_get_local_position (GtkRbTree *tree,
                              TreeNode  *node)
{
  TreeNode *left, *parent;
  TreeAugment *left_aug;
  guint n;
  
  left = gtk_rb_tree_node_get_left (node);
  if (left)
    {
      left_aug = gtk_rb_tree_get_augment (tree, left);
      n = left_aug->n_local;
    }
  else
    {
      n = 0;
    }

  for (parent = gtk_rb_tree_node_get_parent (node);
       parent;
       parent = gtk_rb_tree_node_get_parent (node))
    {
      left = gtk_rb_tree_node_get_left (parent);
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
              left_aug = gtk_rb_tree_get_augment (tree, left);
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
  GtkRbTree *tree;
  TreeNode *left, *parent;
  TreeAugment *left_aug;
  guint n;
  
  for (n = 0;
       !node->is_root;
       node = node->parent, n++)
    {
      tree = node->parent->children;

      left = gtk_rb_tree_node_get_left (node);
      if (left)
        {
          left_aug = gtk_rb_tree_get_augment (tree, left);
          n += left_aug->n_items;
        }

      for (parent = gtk_rb_tree_node_get_parent (node);
           parent;
           parent = gtk_rb_tree_node_get_parent (node))
        {
          left = gtk_rb_tree_node_get_left (parent);
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
                  left_aug = gtk_rb_tree_get_augment (tree, left);
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
      gtk_rb_tree_node_mark_dirty (node);
    }
}

static TreeNode *
gtk_tree_list_model_get_nth (GtkTreeListModel *self,
                             guint             position)
{
  GtkRbTree *tree;
  TreeNode *node, *tmp;
  guint n_children;

  n_children = tree_node_get_n_children (&self->root_node);
  if (n_children <= position)
    return NULL;

  tree = self->root_node.children;
  node = gtk_rb_tree_get_root (tree);

  while (TRUE)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          TreeAugment *aug = gtk_rb_tree_get_augment (tree, tmp);
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
          node = gtk_rb_tree_get_root (tree);
          continue;
        }
      position -= n_children;

      node = gtk_rb_tree_node_get_right (node);
    }

  g_return_val_if_reached (NULL);
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

static gpointer
tree_node_get_item (TreeNode *node)
{
  TreeNode *parent;

  parent = node->parent;
  return g_list_model_get_item (parent->model,
                                tree_node_get_local_position (parent->children, node));
}

static GtkTreeListRow *
tree_node_get_row (TreeNode *node)
{
  if (node->row)
    {
      return g_object_ref (node->row);
    }
  else
    {
      node->row = g_object_new (GTK_TYPE_TREE_LIST_ROW, NULL);
      node->row->node = node;

      return node->row;
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
          child = gtk_rb_tree_node_get_next (child);
          gtk_rb_tree_remove (node->children, tmp);
        }
    }
  else
    {
      tree_removed = 0;
    }

  tree_added = added;
  for (i = 0; i < added; i++)
    {
      child = gtk_rb_tree_insert_before (node->children, child);
      child->parent = node;
    }
  if (self->autoexpand)
    {
      for (i = 0; i < added; i++)
        {
          tree_added += gtk_tree_list_model_expand_node (self, child);
          child = gtk_rb_tree_node_get_next (child);
        }
    }

  tree_node_mark_dirty (node);

  g_list_model_items_changed (G_LIST_MODEL (self),
                              tree_position,
                              tree_removed,
                              tree_added);
}

static void gtk_tree_list_row_destroy (GtkTreeListRow *row);

static void
gtk_tree_list_model_clear_node (gpointer data)
{
  TreeNode *node = data;

  if (node->row)
    gtk_tree_list_row_destroy (node->row);

  if (node->model)
    {
      g_signal_handlers_disconnect_by_func (node->model,
                                            gtk_tree_list_model_items_changed_cb,
                                            node);
      g_object_unref (node->model);
    }
  if (node->children)
    gtk_rb_tree_unref (node->children);
}

static void
gtk_tree_list_model_augment (GtkRbTree *tree,
                             gpointer   _aug,
                             gpointer   _node,
                             gpointer   left,
                             gpointer   right)
{
  TreeAugment *aug = _aug;

  aug->n_items = 1;
  aug->n_items += tree_node_get_n_children (_node);
  aug->n_local = 1;

  if (left)
    {
      TreeAugment *left_aug = gtk_rb_tree_get_augment (tree, left);
      aug->n_items += left_aug->n_items;
      aug->n_local += left_aug->n_local;
    }
  if (right)
    {
      TreeAugment *right_aug = gtk_rb_tree_get_augment (tree, right);
      aug->n_items += right_aug->n_items;
      aug->n_local += right_aug->n_local;
    }
}

static void
gtk_tree_list_model_init_node (GtkTreeListModel *list,
                               TreeNode         *self,
                               GListModel       *model)
{
  gsize i, n;
  TreeNode *node;

  self->model = model;
  g_signal_connect (model,
                    "items-changed",
                    G_CALLBACK (gtk_tree_list_model_items_changed_cb),
                    self);
  self->children = gtk_rb_tree_new (TreeNode,
                                    TreeAugment,
                                    gtk_tree_list_model_augment,
                                    gtk_tree_list_model_clear_node,
                                    NULL);

  n = g_list_model_get_n_items (model);
  node = NULL;
  for (i = 0; i < n; i++)
    {
      node = gtk_rb_tree_insert_after (self->children, node);
      node->parent = self;
      if (list->autoexpand)
        gtk_tree_list_model_expand_node (list, node);
    }
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
  
  if (!g_type_is_a (g_list_model_get_item_type (model), g_list_model_get_item_type (self->root_node.model)))
    {
      g_critical ("The GtkTreeListModelCreateModelFunc for %p returned a model with item type \"%s\" "
                  "but \"%s\" is required.",
                  self,
                  g_type_name (g_list_model_get_item_type (model)),
                  g_type_name (g_list_model_get_item_type (self->root_node.model)));
      return 0;
    }

  gtk_tree_list_model_init_node (self, node, model);

  tree_node_mark_dirty (node);
  
  return tree_node_get_n_children (node);
}

static guint
gtk_tree_list_model_collapse_node (GtkTreeListModel *self,
                                   TreeNode         *node)
{      
  guint n_items;

  if (node->model == NULL)
    return 0;

  n_items = tree_node_get_n_children (node);

  g_clear_pointer (&node->children, gtk_rb_tree_unref);
  g_clear_object (&node->model);

  tree_node_mark_dirty (node);

  return n_items;
}


static GType
gtk_tree_list_model_get_item_type (GListModel *list)
{
  GtkTreeListModel *self = GTK_TREE_LIST_MODEL (list);

  if (self->passthrough)
    return g_list_model_get_item_type (self->root_node.model);
  else
    return GTK_TYPE_TREE_LIST_ROW;
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
  TreeNode *node;

  node = gtk_tree_list_model_get_nth (self, position);
  if (node == NULL)
    return NULL;

  if (self->passthrough)
    {
      return tree_node_get_item (node);
    }
  else
    {
      return tree_node_get_row (node);
    }
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

    case PROP_PASSTHROUGH:
      self->passthrough = g_value_get_boolean (value);
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

    case PROP_MODEL:
      g_value_set_object (value, self->root_node.model);
      break;

    case PROP_PASSTHROUGH:
      g_value_set_boolean (value, self->passthrough);
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
}

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
   * GtkTreeListModel:model:
   *
   * The root model displayed
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The root model displayed"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeListModel:passthrough:
   *
   * If %FALSE, the #GListModel functions for this object return custom
   * #GtkTreeListRow objects.
   * If %TRUE, the values of the child models are pass through unmodified.
   */
  properties[PROP_PASSTHROUGH] =
      g_param_spec_boolean ("passthrough",
                            P_("passthrough"),
                            P_("If child model values are passed through"),
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

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
 * @passthrough: %TRUE to pass through items from the models
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
gtk_tree_list_model_new (gboolean                         passthrough,
                         GListModel                      *root,
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
                       "passthrough", passthrough,
                       NULL);

  self->create_func = create_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  gtk_tree_list_model_init_node (self, &self->root_node, g_object_ref (root));

  return self;
}

/**
 * gtk_tree_list_model_get_model:
 * @self: a #GtkTreeListModel
 *
 * Gets the root model that @self was created with.
 *
 * Returns: (transfer none): the root model
 **/
GListModel *
gtk_tree_list_model_get_model (GtkTreeListModel *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), NULL);

  return self->root_node.model;
}

/**
 * gtk_tree_list_model_get_passthrough:
 * @self: a #GtkTreeListModel
 *
 * If this function returns %FALSE, the #GListModel functions for @self
 * return custom #GtkTreeListRow objects. You need to call
 * gtk_tree_list_row_get_item() on these objects to get the original
 * item.
 *
 * If %TRUE, the values of the child models are passed through in their
 * original state. You then need to call gtk_tree_list_model_get_row()
 * to get the custom #GtkTreeListRows.
 *
 * Returns: %TRUE if the model is passing through original row items
 **/
gboolean
gtk_tree_list_model_get_passthrough (GtkTreeListModel *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), FALSE);

  return self->passthrough;
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

/**
 * gtk_tree_list_model_get_row:
 * @self: a #GtkTreeListModel
 * @position: the position of the row to fetch
 *
 * Gets the row object for the given row. If @position is greater than
 * the number of items in @self, %NULL is returned.
 *
 * The row object can be used to expand and collapse rows as well as
 * to inspect its position in the tree. See its documentation for details.
 *
 * This row object is persistent and will refer to the current item as
 * long as the row is present in @self, independent of other rows being
 * added or removed.
 *
 * If @self is set to not be passthrough, this function is equivalent
 * to calling g_list_model_get_item().
 *
 * Do not confuse this function with gtk_tree_list_model_get_child_row().
 *
 * Returns: (nullable) (transfer full): The row item 
 **/
GtkTreeListRow *
gtk_tree_list_model_get_row (GtkTreeListModel *self,
                             guint             position)
{
  TreeNode *node;

  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), NULL);

  node = gtk_tree_list_model_get_nth (self, position);
  if (node == NULL)
    return NULL;

  return tree_node_get_row (node);
}

/**
 * gtk_tree_list_model_get_child_row:
 * @self: a #GtkTreeListModel
 * @position: position of the child to get
 *
 * Gets the row item corresponding to the child at index @position for
 * @self's root model.
 *
 * If @position is greater than the number of children in the root model,
 * %NULL is returned.
 *
 * Do not confuse this function with gtk_tree_list_model_get_row().
 *
 * Returns: (nullable) (transfer full): the child in @position
 **/
GtkTreeListRow *
gtk_tree_list_model_get_child_row (GtkTreeListModel *self,
                                   guint             position)
{
  TreeNode *child;

  g_return_val_if_fail (GTK_IS_TREE_LIST_MODEL (self), NULL);

  child = tree_node_get_nth_child (&self->root_node, position);
  if (child == NULL)
    return NULL;

  return tree_node_get_row (child);
}

/***   ROW   ***/

enum {
  ROW_PROP_0,
  ROW_PROP_CHILDREN,
  ROW_PROP_DEPTH,
  ROW_PROP_EXPANDABLE,
  ROW_PROP_EXPANDED,
  ROW_PROP_ITEM,
  NUM_ROW_PROPERTIES
};

static GParamSpec *row_properties[NUM_ROW_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkTreeListRow, gtk_tree_list_row, G_TYPE_OBJECT)

static void
gtk_tree_list_row_destroy (GtkTreeListRow *self)
{
  g_object_freeze_notify (G_OBJECT (self));

  /* FIXME: We could check some properties to avoid excess notifies */
  g_object_notify_by_pspec (G_OBJECT (self), row_properties[ROW_PROP_DEPTH]);
  g_object_notify_by_pspec (G_OBJECT (self), row_properties[ROW_PROP_EXPANDABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), row_properties[ROW_PROP_EXPANDED]);
  g_object_notify_by_pspec (G_OBJECT (self), row_properties[ROW_PROP_ITEM]);

  self->node = NULL;
  g_object_thaw_notify (G_OBJECT (self));
}

static void
gtk_tree_list_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkTreeListRow *self = GTK_TREE_LIST_ROW (object);

  switch (prop_id)
    {
    case ROW_PROP_EXPANDED:
      gtk_tree_list_row_set_expanded (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_tree_list_row_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkTreeListRow *self = GTK_TREE_LIST_ROW (object);

  switch (prop_id)
    {
    case ROW_PROP_CHILDREN:
      g_value_set_object (value, gtk_tree_list_row_get_children (self));
      break;

    case ROW_PROP_DEPTH:
      g_value_set_uint (value, gtk_tree_list_row_get_depth (self));
      break;

    case ROW_PROP_EXPANDABLE:
      g_value_set_boolean (value, gtk_tree_list_row_is_expandable (self));
      break;

    case ROW_PROP_EXPANDED:
      g_value_set_boolean (value, gtk_tree_list_row_get_expanded (self));
      break;

    case ROW_PROP_ITEM:
      g_value_take_object (value, gtk_tree_list_row_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_row_dispose (GObject *object)
{
  GtkTreeListRow *self = GTK_TREE_LIST_ROW (object);

  if (self->node)
    self->node->row = NULL;

  G_OBJECT_CLASS (gtk_tree_list_row_parent_class)->dispose (object);
}

static void
gtk_tree_list_row_class_init (GtkTreeListRowClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_tree_list_row_set_property;
  gobject_class->get_property = gtk_tree_list_row_get_property;
  gobject_class->dispose = gtk_tree_list_row_dispose;

  /**
   * GtkTreeListRow:children:
   *
   * The model holding the row's children.
   */
  row_properties[ROW_PROP_CHILDREN] =
      g_param_spec_object ("children",
                           P_("Children"),
                           P_("Model holding the row’s children"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READABLE);

  /**
   * GtkTreeListRow:depth:
   *
   * The depth in the tree of this row
   */
  row_properties[ROW_PROP_DEPTH] =
      g_param_spec_uint ("depth",
                         P_("Depth"),
                         P_("Depth in the tree"),
                         0, G_MAXUINT, 0,
                         GTK_PARAM_READABLE);

  /**
   * GtkTreeListRow:expandable:
   *
   * If this row can ever be expanded
   */
  row_properties[ROW_PROP_EXPANDABLE] =
      g_param_spec_boolean ("expandable",
                            P_("Expandable"),
                            P_("If this row can ever be expanded"),
                            FALSE,
                            GTK_PARAM_READABLE);

  /**
   * GtkTreeListRow:expanded:
   *
   * If this row is currently expanded
   */
  row_properties[ROW_PROP_EXPANDED] =
      g_param_spec_boolean ("expanded",
                            P_("Expanded"),
                            P_("If this row is currently expanded"),
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTreeListRow:item:
   *
   * The item held in this row
   */
  row_properties[ROW_PROP_ITEM] =
      g_param_spec_object ("item",
                           P_("Item"),
                           P_("The item held in this row"),
                           G_TYPE_OBJECT,
                           GTK_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, NUM_ROW_PROPERTIES, row_properties);
}

static void
gtk_tree_list_row_init (GtkTreeListRow *self)
{
}

/**
 * gtk_tree_list_row_get_position:
 * @self: a #GtkTreeListRow
 *
 * Returns the position in the #GtkTreeListModel that @self occupies
 * at the moment.
 *
 * Returns: The position in the model
 **/
guint
gtk_tree_list_row_get_position (GtkTreeListRow *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), 0);

  if (self->node == NULL)
    return 0;

  return tree_node_get_position (self->node);
}

/**
 * gtk_tree_list_row_get_depth:
 * @self: a #GtkTreeListRow
 *
 * Gets the depth of this row. Rows that correspond to items in
 * the root model have a depth of zero, rows corresponding to items
 * of models of direct children of the root model have a depth of
 * 1 and so on.
 *
 * The depth of a row never changes until the row is destroyed.
 *
 * Returns: The depth of this row
 **/
guint
gtk_tree_list_row_get_depth (GtkTreeListRow *self)
{
  TreeNode *node;
  guint depth;

  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), 0);

  if (self->node == NULL)
    return 0;

  depth = 0;
  for (node = self->node->parent;
       !node->is_root;
       node = node->parent)
    depth++;

  return depth;
}

/**
 * gtk_tree_list_row_set_expanded:
 * @self: a #GtkTreeListRow
 * @expanded: %TRUE if the row should be expanded
 *
 * Expands or collapses a row.
 *
 * If a row is expanded, the model of calling the 
 * #GtkTreeListModelCreateModelFunc for the row's item will
 * be inserted after this row. If a row is collapsed, those
 * items will be removed from the model.
 *
 * If the row is not expandable, this function does nothing.
 **/
void
gtk_tree_list_row_set_expanded (GtkTreeListRow *self,
                                gboolean        expanded)
{
  GtkTreeListModel *list;
  gboolean was_expanded;
  guint n_items;

  g_return_if_fail (GTK_IS_TREE_LIST_ROW (self));

  if (self->node == NULL)
    return;

  was_expanded = self->node->children != NULL;
  if (was_expanded == expanded)
    return;

  list = tree_node_get_tree_list_model (self->node);

  if (expanded)
    {
      n_items = gtk_tree_list_model_expand_node (list, self->node);
      if (n_items > 0)
        g_list_model_items_changed (G_LIST_MODEL (list), tree_node_get_position (self->node) + 1, 0, n_items);
    }
  else
    {
      n_items = gtk_tree_list_model_collapse_node (list, self->node);
      if (n_items > 0)
        g_list_model_items_changed (G_LIST_MODEL (list), tree_node_get_position (self->node) + 1, n_items, 0);
    }

  g_object_notify_by_pspec (G_OBJECT (self), row_properties[ROW_PROP_EXPANDED]);
  g_object_notify_by_pspec (G_OBJECT (self), row_properties[ROW_PROP_CHILDREN]);
}

/**
 * gtk_tree_list_row_get_expanded:
 * @self: a #GtkTreeListRow
 *
 * Gets if a row is currently expanded.
 *
 * Returns: %TRUE if the row is expanded
 **/
gboolean
gtk_tree_list_row_get_expanded (GtkTreeListRow *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), FALSE);

  if (self->node == NULL)
    return FALSE;

  return self->node->children != NULL;
}

/**
 * gtk_tree_list_row_is_expandable:
 * @self: a #GtkTreeListRow
 *
 * Checks if a row can be expanded. This does not mean that the
 * row is actually expanded, this can be checked with
 * gtk_tree_list_row_get_expanded()
 * 
 * If a row is expandable never changes until the row is destroyed.
 *
 * Returns: %TRUE if the row is expandable
 **/
gboolean
gtk_tree_list_row_is_expandable (GtkTreeListRow *self)
{
  GtkTreeListModel *list;
  GListModel *model;

  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), FALSE);

  if (self->node == NULL)
    return FALSE;

  if (self->node->empty)
    return FALSE;

  if (self->node->model)
    return TRUE;

  list = tree_node_get_tree_list_model (self->node);
  model = tree_node_create_model (list, self->node);
  if (model)
    {
      g_object_unref (model);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_tree_list_row_get_item:
 * @self: a #GtkTreeListRow
 *
 * Gets the item corresponding to this row,
 *
 * The value returned by this function never changes until the
 * row is destroyed.
 *
 * Returns: (nullable) (type GObject) (transfer full): The item of this row
 *    or %NULL when the row was destroyed
 **/
gpointer
gtk_tree_list_row_get_item (GtkTreeListRow *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), NULL);

  if (self->node == NULL)
    return NULL;

  return tree_node_get_item (self->node);
}

/**
 * gtk_tree_list_row_get_children:
 * @self: a #GtkTreeListRow
 *
 * If the row is expanded, gets the model holding the children of @self.
 *
 * This model is the model created by the #GtkTreeListModelCreateModelFunc
 * and contains the original items, no matter what value
 * #GtkTreeListModel:passthrough is set to.
 *
 * Returns: (nullable) (transfer none): The model containing the children
 **/
GListModel *
gtk_tree_list_row_get_children (GtkTreeListRow *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), NULL);

  if (self->node == NULL)
    return NULL;

  return self->node->model;
}

/**
 * gtk_tree_list_row_get_parent:
 * @self: a #GtkTreeListRow
 *
 * Gets the row representing the parent for @self. That is the row that would
 * need to be collapsed to make this row disappear.
 *
 * If @self is a row corresponding to the root model, %NULL is returned.
 *
 * The value returned by this function never changes until the
 * row is destroyed.
 *
 * Returns: (nullable) (transfer full): The parent of @self
 **/
GtkTreeListRow *
gtk_tree_list_row_get_parent (GtkTreeListRow *self)
{
  TreeNode *parent;

  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), NULL);

  if (self->node == NULL)
    return NULL;

  parent = self->node->parent;
  if (parent->is_root)
    return NULL;

  return tree_node_get_row (parent);
}

/**
 * gtk_tree_list_row_get_child_row:
 * @self: a #GtkTreeListRow
 * @position: position of the child to get
 *
 * If @self is not expanded or @position is greater than the number of
 * children, %NULL is returned.
 *
 * Returns: (nullable) (transfer full): the child in @position
 **/
GtkTreeListRow *
gtk_tree_list_row_get_child_row (GtkTreeListRow *self,
                                 guint           position)
{
  TreeNode *child;

  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (self), NULL);

  if (self->node == NULL)
    return NULL;

  if (self->node->children == NULL)
    return NULL;

  child = tree_node_get_nth_child (self->node, position);
  if (child == NULL)
    return NULL;

  return tree_node_get_row (child);
}

