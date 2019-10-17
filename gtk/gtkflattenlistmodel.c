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

#include "gtkflattenlistmodel.h"

#include "gtkrbtreeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkflattenlistmodel
 * @title: GtkFlattenListModel
 * @short_description: A list model that flattens a list of lists
 * @see_also: #GListModel
 *
 * #GtkFlattenListModel is a list model that takes a list model containing
 * list models and flattens it into a single model.
 *
 * Another term for this is concatenation: #GtkFlattenListModel takes a
 * list of lists and concatenates them into a single list.
 */

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  NUM_PROPERTIES
};

typedef struct _FlattenNode FlattenNode;
typedef struct _FlattenAugment FlattenAugment;

struct _FlattenNode
{
  GListModel *model;
  GtkFlattenListModel *list;
};

struct _FlattenAugment
{
  guint n_items;
  guint n_models;
};

struct _GtkFlattenListModel
{
  GObject parent_instance;

  GType item_type;
  GListModel *model;
  GtkRbTree *items; /* NULL if model == NULL */
};

struct _GtkFlattenListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static FlattenNode *
gtk_flatten_list_model_get_nth (GtkRbTree *tree,
                                guint         position,
                                guint        *model_position)
{
  FlattenNode *node, *tmp;
  guint model_n_items;

  node = gtk_rb_tree_get_root (tree);

  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          FlattenAugment *aug = gtk_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_items)
            {
              node = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      model_n_items = g_list_model_get_n_items (node->model);
      if (position < model_n_items)
        break;
      position -= model_n_items;

      node = gtk_rb_tree_node_get_right (node);
    }

  if (model_position)
    *model_position = node ? position : 0;

  return node;
}

static FlattenNode *
gtk_flatten_list_model_get_nth_model (GtkRbTree *tree,
                                      guint         position,
                                      guint        *items_before)
{
  FlattenNode *node, *tmp;
  guint before;

  node = gtk_rb_tree_get_root (tree);
  before = 0;

  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          FlattenAugment *aug = gtk_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_models)
            {
              node = tmp;
              continue;
            }
          position -= aug->n_models;
          before += aug->n_items;
        }

      if (position == 0)
        break;
      position--;
      before += g_list_model_get_n_items (node->model);

      node = gtk_rb_tree_node_get_right (node);
    }

  if (items_before)
    *items_before = before;

  return node;
}

static GType
gtk_flatten_list_model_get_item_type (GListModel *list)
{
  GtkFlattenListModel *self = GTK_FLATTEN_LIST_MODEL (list);

  return self->item_type;
}

static guint
gtk_flatten_list_model_get_n_items (GListModel *list)
{
  GtkFlattenListModel *self = GTK_FLATTEN_LIST_MODEL (list);
  FlattenAugment *aug;
  FlattenNode *node;

  if (!self->items)
    return 0;

  node = gtk_rb_tree_get_root (self->items);
  if (node == NULL)
    return 0;

  aug = gtk_rb_tree_get_augment (self->items, node);
  return aug->n_items;
}

static gpointer
gtk_flatten_list_model_get_item (GListModel *list,
                                 guint       position)
{
  GtkFlattenListModel *self = GTK_FLATTEN_LIST_MODEL (list);
  FlattenNode *node;
  guint model_pos;

  if (!self->items)
    return NULL;

  node = gtk_flatten_list_model_get_nth (self->items, position, &model_pos);
  if (node == NULL)
    return NULL;

  return g_list_model_get_item (node->model, model_pos);
}

static void
gtk_flatten_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_flatten_list_model_get_item_type;
  iface->get_n_items = gtk_flatten_list_model_get_n_items;
  iface->get_item = gtk_flatten_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkFlattenListModel, gtk_flatten_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_flatten_list_model_model_init))

static void
gtk_flatten_list_model_items_changed_cb (GListModel          *model,
                                         guint                position,
                                         guint                removed,
                                         guint                added,
                                         gpointer             _node)
{
  FlattenNode *node = _node, *parent, *left;
  GtkFlattenListModel *self = node->list;
  guint real_position;

  gtk_rb_tree_node_mark_dirty (node);
  real_position = position;

  left = gtk_rb_tree_node_get_left (node);
  if (left)
    {
      FlattenAugment *aug = gtk_rb_tree_get_augment (self->items, left);
      real_position += aug->n_items;
    }

  for (;
       (parent = gtk_rb_tree_node_get_parent (node)) != NULL;
       node = parent)
    {
      left = gtk_rb_tree_node_get_left (parent);
      if (left != node)
        {
          if (left)
            {
              FlattenAugment *aug = gtk_rb_tree_get_augment (self->items, left);
              real_position += aug->n_items;
            }
          real_position += g_list_model_get_n_items (parent->model);
        }
    }

  g_list_model_items_changed (G_LIST_MODEL (self), real_position, removed, added);
}

static void
gtk_flatten_list_model_clear_node (gpointer _node)
{
  FlattenNode *node= _node;

  g_signal_handlers_disconnect_by_func (node->model, gtk_flatten_list_model_items_changed_cb, node);
  g_object_unref (node->model);
}

static void
gtk_flatten_list_model_augment (GtkRbTree *flatten,
                                gpointer   _aug,
                                gpointer   _node,
                                gpointer   left,
                                gpointer   right)
{
  FlattenNode *node = _node;
  FlattenAugment *aug = _aug;

  aug->n_items = g_list_model_get_n_items (node->model);
  aug->n_models = 1;

  if (left)
    {
      FlattenAugment *left_aug = gtk_rb_tree_get_augment (flatten, left);
      aug->n_items += left_aug->n_items;
      aug->n_models += left_aug->n_models;
    }
  if (right)
    {
      FlattenAugment *right_aug = gtk_rb_tree_get_augment (flatten, right);
      aug->n_items += right_aug->n_items;
      aug->n_models += right_aug->n_models;
    }
}

static guint
gtk_flatten_list_model_add_items (GtkFlattenListModel *self,
                                  FlattenNode         *after,
                                  guint                position,
                                  guint                n)
{
  FlattenNode *node;
  guint added, i;

  added = 0;
  for (i = 0; i < n; i++)
    {
      node = gtk_rb_tree_insert_before (self->items, after);
      node->model = g_list_model_get_item (self->model, position + i);
      g_warn_if_fail (g_type_is_a (g_list_model_get_item_type (node->model), self->item_type));
      g_signal_connect (node->model,
                        "items-changed",
                        G_CALLBACK (gtk_flatten_list_model_items_changed_cb),
                        node);
      node->list = self;
      added +=g_list_model_get_n_items (node->model);
    }

  return added;
}

static void
gtk_flatten_list_model_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkFlattenListModel *self = GTK_FLATTEN_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      self->item_type = g_value_get_gtype (value);
      break;

    case PROP_MODEL:
      gtk_flatten_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_flatten_list_model_get_property (GObject     *object,
                                     guint        prop_id,
                                     GValue      *value,
                                     GParamSpec  *pspec)
{
  GtkFlattenListModel *self = GTK_FLATTEN_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, self->item_type);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_flatten_list_model_model_items_changed_cb (GListModel          *model,
                                               guint                position,
                                               guint                removed,
                                               guint                added,
                                               GtkFlattenListModel *self)
{
  FlattenNode *node;
  guint i, real_position, real_removed, real_added;

  node = gtk_flatten_list_model_get_nth_model (self->items, position, &real_position);

  real_removed = 0;
  for (i = 0; i < removed; i++)
    {
      FlattenNode *next = gtk_rb_tree_node_get_next (node);
      real_removed += g_list_model_get_n_items (node->model);
      gtk_rb_tree_remove (self->items, node);
      node = next;
    }

  real_added = gtk_flatten_list_model_add_items (self, node, position, added);

  if (real_removed > 0 || real_added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), real_position, real_removed, real_added);
}

static void
gtk_flatten_list_clear_model (GtkFlattenListModel *self)
{
  if (self->model)
    {
      g_signal_handlers_disconnect_by_func (self->model, gtk_flatten_list_model_model_items_changed_cb, self);
      g_clear_object (&self->model);
      g_clear_pointer (&self->items, gtk_rb_tree_unref);
    }
}

static void
gtk_flatten_list_model_dispose (GObject *object)
{
  GtkFlattenListModel *self = GTK_FLATTEN_LIST_MODEL (object);

  gtk_flatten_list_clear_model (self);

  G_OBJECT_CLASS (gtk_flatten_list_model_parent_class)->dispose (object);
}

static void
gtk_flatten_list_model_class_init (GtkFlattenListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_flatten_list_model_set_property;
  gobject_class->get_property = gtk_flatten_list_model_get_property;
  gobject_class->dispose = gtk_flatten_list_model_dispose;

  /**
   * GtkFlattenListModel:item-type:
   *
   * The #GTpe for elements of this object
   */
  properties[PROP_ITEM_TYPE] =
      g_param_spec_gtype ("item-type",
                          P_("Item type"),
                          P_("The type of elements of this object"),
                          G_TYPE_OBJECT,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFlattenListModel:model:
   *
   * The model being flattened
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The model being flattened"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_flatten_list_model_init (GtkFlattenListModel *self)
{
}

/**
 * gtk_flatten_list_model_new:
 * @item_type: The type of items in the to-be-flattened models
 * @model: (nullable) (transfer none): the item to be flattened
 *
 * Creates a new #GtkFlattenListModel that flattens @list. The
 * models returned by @model must conform to the given @item_type,
 * either by having an identical type or a subtype.
 *
 * Returns: a new #GtkFlattenListModel
 **/
GtkFlattenListModel *
gtk_flatten_list_model_new (GType       item_type,
                            GListModel *model)
{
  GtkFlattenListModel *result;

  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  result = g_object_new (GTK_TYPE_FLATTEN_LIST_MODEL,
                         "item-type", item_type,
                         "model", model,
                         NULL);

  return result;
}

/**
 * gtk_flatten_list_model_set_model:
 * @self: a #GtkFlattenListModel
 * @model: (nullable) (transfer none): the new model or %NULL
 *
 * Sets a new model to be flattened. The model must contain items of
 * #GtkListModel that conform to the item type of @self.
 **/
void
gtk_flatten_list_model_set_model (GtkFlattenListModel *self,
                                  GListModel          *model)
{
  guint removed, added = 0;

  g_return_if_fail (GTK_IS_FLATTEN_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  if (model)
    {
      g_return_if_fail (g_type_is_a (g_list_model_get_item_type (model), G_TYPE_LIST_MODEL));
    }

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self)); 
  gtk_flatten_list_clear_model (self);

  self->model = model;

  if (model)
    {
      g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_flatten_list_model_model_items_changed_cb), self);
      self->items = gtk_rb_tree_new (FlattenNode,
                                     FlattenAugment,
                                     gtk_flatten_list_model_augment,
                                     gtk_flatten_list_model_clear_node,
                                     NULL);

      added = gtk_flatten_list_model_add_items (self, NULL, 0, g_list_model_get_n_items (model));
    }

  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_flatten_list_model_get_model:
 * @self: a #GtkFlattenListModel
 *
 * Gets the model set via gtk_flatten_list_model_set_model().
 *
 * Returns: (nullable) (transfer none): The model flattened by @self
 **/
GListModel *
gtk_flatten_list_model_get_model (GtkFlattenListModel *self)
{
  g_return_val_if_fail (GTK_IS_FLATTEN_LIST_MODEL (self), NULL);

  return self->model;
}
