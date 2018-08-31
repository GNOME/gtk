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

#include "gtkfilterlistmodel.h"

#include "gtkcssrbtreeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_HAS_FILTER,
  PROP_MODEL,
  NUM_PROPERTIES
};

typedef struct _FilterNode FilterNode;
typedef struct _FilterAugment FilterAugment;

struct _FilterNode
{
  guint visible : 1;
};

struct _FilterAugment
{
  guint n_items;
  guint n_visible;
};

struct _GtkFilterListModel
{
  GObject parent_instance;

  GListModel *model;
  GtkFilterListModelFilterFunc filter_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GtkCssRbTree *items; /* NULL if filter_func == NULL */
};

struct _GtkFilterListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static FilterNode *
gtk_filter_list_model_get_nth_filtered (GtkCssRbTree *tree,
                                        guint         position,
                                        guint        *out_unfiltered)
{
  FilterNode *node, *tmp;
  guint unfiltered;

  node = gtk_css_rb_tree_get_root (tree);
  unfiltered = 0;

  while (node)
    {
      tmp = gtk_css_rb_tree_get_left (tree, node);
      if (tmp)
        {
          FilterAugment *aug = gtk_css_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_visible)
            {
              node = tmp;
              continue;
            }
          position -= aug->n_visible;
          unfiltered += aug->n_items;
        }

      if (node->visible)
        {
          if (position == 0)
            break;
          position--;
        }

      unfiltered++;

      node = gtk_css_rb_tree_get_right (tree, node);
    }

  if (out_unfiltered)
    *out_unfiltered = unfiltered;

  return node;
}

static FilterNode *
gtk_filter_list_model_get_nth (GtkCssRbTree *tree,
                               guint         position,
                               guint        *out_filtered)
{
  FilterNode *node, *tmp;
  guint filtered;

  node = gtk_css_rb_tree_get_root (tree);
  filtered = 0;

  while (node)
    {
      tmp = gtk_css_rb_tree_get_left (tree, node);
      if (tmp)
        {
          FilterAugment *aug = gtk_css_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_items)
            {
              node = tmp;
              continue;
            }
          position -= aug->n_items;
          filtered += aug->n_visible;
        }

      if (position == 0)
        break;

      position--;
      if (node->visible)
        filtered++;

      node = gtk_css_rb_tree_get_right (tree, node);
    }

  if (out_filtered)
    *out_filtered = filtered;

  return node;
}

static GType
gtk_filter_list_model_get_item_type (GListModel *list)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_filter_list_model_get_n_items (GListModel *list)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);
  FilterAugment *aug;
  FilterNode *node;

  if (!self->items)
    return g_list_model_get_n_items (self->model);

  node = gtk_css_rb_tree_get_root (self->items);
  if (node == NULL)
    return 0;

  aug = gtk_css_rb_tree_get_augment (self->items, node);
  return aug->n_visible;
}

static gpointer
gtk_filter_list_model_get_item (GListModel *list,
                                guint       position)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);
  guint unfiltered;

  if (self->items)
    gtk_filter_list_model_get_nth_filtered (self->items, position, &unfiltered);
  else
    unfiltered = position;

  return g_list_model_get_item (self->model, unfiltered);
}

static void
gtk_filter_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_filter_list_model_get_item_type;
  iface->get_n_items = gtk_filter_list_model_get_n_items;
  iface->get_item = gtk_filter_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkFilterListModel, gtk_filter_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_filter_list_model_model_init))

static gboolean
gtk_filter_list_model_run_filter (GtkFilterListModel *self,
                                  guint               position)
{
  gpointer item;
  gboolean visible;

  item = g_list_model_get_item (self->model, position);
  visible = self->filter_func (item, self->user_data);
  g_object_unref (item);

  return visible;
}

static guint
gtk_filter_list_model_add_items (GtkFilterListModel *self,
                                 FilterNode         *after,
                                 guint               position,
                                 guint               n_items)
{
  FilterNode *node;
  guint i, n_visible;

  n_visible = 0;
  
  for (i = 0; i < n_items; i++)
    {
      node = gtk_css_rb_tree_insert_before (self->items, after);
      node->visible = gtk_filter_list_model_run_filter (self, position + i);
      if (node->visible)
        n_visible++;
    }

  return n_visible;
}

static void
gtk_filter_list_model_items_changed_cb (GListModel         *model,
                                        guint               position,
                                        guint               removed,
                                        guint               added,
                                        GtkFilterListModel *self)
{
  FilterNode *node;
  guint i, filter_position, filter_removed, filter_added;

  if (self->items == NULL)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      return;
    }

  node = gtk_filter_list_model_get_nth (self->items, position, &filter_position);

  filter_removed = 0;
  for (i = 0; i < removed; i++)
    {
      FilterNode *next = gtk_css_rb_tree_get_next (self->items, node);
      if (node->visible)
        filter_removed++;
      gtk_css_rb_tree_remove (self->items, node);
      node = next;
    }

  filter_added = gtk_filter_list_model_add_items (self, node, position, added);

  if (filter_removed > 0 || filter_added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), filter_position, filter_removed, filter_added);
}

static void
gtk_filter_list_model_set_model (GtkFilterListModel *self,
                                 GListModel         *model)
{
  g_return_if_fail (G_IS_LIST_MODEL (model));

  self->model = g_object_ref (model);
  g_signal_connect (model, "items-changed", G_CALLBACK (gtk_filter_list_model_items_changed_cb), self);
  
  /* no filter func has been set yet */
  g_assert (self->items == NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

static void
gtk_filter_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_filter_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_filter_list_model_get_property (GObject     *object,
                                    guint        prop_id,
                                    GValue      *value,
                                    GParamSpec  *pspec)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_HAS_FILTER:
      g_value_set_boolean (value, self->items != NULL);
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
gtk_filter_list_model_finalize (GObject *object)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  g_signal_handlers_disconnect_by_func (self->model, gtk_filter_list_model_items_changed_cb, self);
  g_object_unref (self->model);
  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_filter_list_model_parent_class)->finalize (object);
};

static void
gtk_filter_list_model_class_init (GtkFilterListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_filter_list_model_set_property;
  gobject_class->get_property = gtk_filter_list_model_get_property;
  gobject_class->finalize = gtk_filter_list_model_finalize;

  /**
   * GtkFilterListModel:has-filter:
   *
   * If a filter is set for this model
   */
  properties[PROP_HAS_FILTER] =
      g_param_spec_boolean ("has-filter",
                            P_("has filter"),
                            P_("If a filter is set for this model"),
                            FALSE,
                            GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFilterListModel:model:
   *
   * The model being filtered
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The model being filtered"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_filter_list_model_init (GtkFilterListModel *self)
{
}


static void
gtk_filter_list_model_augment (GtkCssRbTree *filter,
                               gpointer      _aug,
                               gpointer      _node,
                               gpointer      left,
                               gpointer      right)
{
  FilterNode *node= _node;
  FilterAugment *aug = _aug;

  aug->n_items = 1;
  aug->n_visible = node->visible ? 1 : 0;

  if (left)
    {
      FilterAugment *left_aug = gtk_css_rb_tree_get_augment (filter, left);
      aug->n_items += left_aug->n_items;
      aug->n_visible += left_aug->n_visible;
    }
  if (right)
    {
      FilterAugment *right_aug = gtk_css_rb_tree_get_augment (filter, right);
      aug->n_items += right_aug->n_items;
      aug->n_visible += right_aug->n_visible;
    }
}

GtkFilterListModel *
gtk_filter_list_model_new (GListModel                   *model,
                           GtkFilterListModelFilterFunc  filter_func,
                           gpointer                      data,
                           GDestroyNotify                data_destroy)
{
  GtkFilterListModel *result;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  result = g_object_new (GTK_TYPE_FILTER_LIST_MODEL,
                         "model", model,
                         NULL);

  if (filter_func)
    gtk_filter_list_model_set_filter_func (result, filter_func, data, data_destroy);

  return result;
}

void
gtk_filter_list_model_set_filter_func (GtkFilterListModel     *self,
                                       GtkFilterListModelFilterFunc filter_func,
                                       gpointer                user_data,
                                       GDestroyNotify          user_destroy)
{
  gboolean was_filtered, will_be_filtered;

  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  g_return_if_fail (filter_func != NULL || (user_data == NULL && !user_destroy));

  was_filtered = self->filter_func != NULL;
  will_be_filtered = filter_func != NULL;

  if (!was_filtered && !will_be_filtered)
    return;

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  self->filter_func = filter_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;
  
  if (!will_be_filtered)
    {
      g_clear_pointer (&self->items, gtk_css_rb_tree_unref);
    }
  else if (!was_filtered)
    {
      guint i, n_items;

      self->items = gtk_css_rb_tree_new (FilterNode,
                                         FilterAugment, 
                                         gtk_filter_list_model_augment,
                                         NULL, NULL);
      n_items =g_list_model_get_n_items (self->model);
      for (i = 0; i < n_items; i++)
        {
          FilterNode *node = gtk_css_rb_tree_insert_before (self->items, NULL);
          node->visible = TRUE;
        }
    }

  gtk_filter_list_model_refilter (self);

  if (was_filtered != will_be_filtered)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_FILTER]);
}

GListModel *
gtk_filter_list_model_get_model (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), NULL);

  return self->model;
}

gboolean
gtk_filter_list_model_has_filter (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), FALSE);

  return self->items != NULL;
}

void
gtk_filter_list_model_refilter (GtkFilterListModel *self)
{
  FilterNode *node;
  guint i, first_change, last_change;
  guint n_is_visible, n_was_visible;
  gboolean visible;

  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  
  if (self->items == NULL)
    return;

  first_change = G_MAXUINT;
  last_change = 0;
  n_is_visible = 0;
  n_was_visible = 0;
  for (i = 0, node = gtk_css_rb_tree_get_first (self->items);
       node != NULL;
       i++, node = gtk_css_rb_tree_get_next (self->items, node))
    {
      visible = gtk_filter_list_model_run_filter (self, i);
      if (visible == node->visible)
        {
          if (visible)
            {
              n_is_visible++;
              n_was_visible++;
            }
          continue;
        }

      node->visible = visible;
      first_change = MIN (n_is_visible, first_change);
      if (visible)
        n_is_visible++;
      else
        n_was_visible++;
      last_change = MAX (n_is_visible, last_change);
    }

  if (first_change <= last_change)
    {
      g_list_model_items_changed (G_LIST_MODEL (self),
                                  first_change,
                                  last_change - first_change + n_was_visible - n_is_visible,
                                  last_change - first_change);
    }
}

void
gtk_filter_list_model_dump (GtkFilterListModel *self)
{
  FilterNode *node;

  if (self->items == NULL)
    {
      g_print ("unfiltered\n");
      return;
    }

  for (node = gtk_css_rb_tree_get_first (self->items);
       node != NULL;
       node = gtk_css_rb_tree_get_next (self->items, node))
    {
      FilterAugment *aug = gtk_css_rb_tree_get_augment (self->items, node);
      g_print ("%p %s %u %u\n", node, node->visible ? "X" : " ", aug->n_items, aug->n_visible);
    }
}
