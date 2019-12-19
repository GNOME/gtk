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

#include "gtkrbtreeprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkfilterlistmodel
 * @title: GtkFilterListModel
 * @short_description: A list model that filters its items
 * @see_also: #GListModel, #GtkFilter
 *
 * #GtkFilterListModel is a list model that filters a given other
 * listmodel.
 * It hides some elements from the other model according to
 * criteria given by a #GtkFilter.
 */

enum {
  PROP_0,
  PROP_FILTER,
  PROP_ITEM_TYPE,
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

  GType item_type;
  GListModel *model;
  GtkFilter *filter;
  GtkFilterMatch strictness;

  GtkRbTree *items; /* NULL if strictness != GTK_FILTER_MATCH_SOME */
};

struct _GtkFilterListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void
gtk_filter_list_model_augment (GtkRbTree *filter,
                               gpointer   _aug,
                               gpointer   _node,
                               gpointer   left,
                               gpointer   right)
{
  FilterNode *node = _node;
  FilterAugment *aug = _aug;

  aug->n_items = 1;
  aug->n_visible = node->visible ? 1 : 0;

  if (left)
    {
      FilterAugment *left_aug = gtk_rb_tree_get_augment (filter, left);
      aug->n_items += left_aug->n_items;
      aug->n_visible += left_aug->n_visible;
    }
  if (right)
    {
      FilterAugment *right_aug = gtk_rb_tree_get_augment (filter, right);
      aug->n_items += right_aug->n_items;
      aug->n_visible += right_aug->n_visible;
    }
}

static FilterNode *
gtk_filter_list_model_get_nth_filtered (GtkRbTree *tree,
                                        guint      position,
                                        guint     *out_unfiltered)
{
  FilterNode *node, *tmp;
  guint unfiltered;

  node = gtk_rb_tree_get_root (tree);
  unfiltered = 0;

  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          FilterAugment *aug = gtk_rb_tree_get_augment (tree, tmp);
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

      node = gtk_rb_tree_node_get_right (node);
    }

  if (out_unfiltered)
    *out_unfiltered = unfiltered;

  return node;
}

static FilterNode *
gtk_filter_list_model_get_nth (GtkRbTree *tree,
                               guint      position,
                               guint     *out_filtered)
{
  FilterNode *node, *tmp;
  guint filtered;

  node = gtk_rb_tree_get_root (tree);
  filtered = 0;

  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          FilterAugment *aug = gtk_rb_tree_get_augment (tree, tmp);
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

      node = gtk_rb_tree_node_get_right (node);
    }

  if (out_filtered)
    *out_filtered = filtered;

  return node;
}

static GType
gtk_filter_list_model_get_item_type (GListModel *list)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);

  return self->item_type;
}

static guint
gtk_filter_list_model_get_n_items (GListModel *list)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);
  FilterAugment *aug;
  FilterNode *node;

  switch (self->strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      return 0;

    case GTK_FILTER_MATCH_ALL:
      return g_list_model_get_n_items (self->model);

    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;
    case GTK_FILTER_MATCH_SOME:
      break;
    }

  node = gtk_rb_tree_get_root (self->items);
  if (node == NULL)
    return 0;

  aug = gtk_rb_tree_get_augment (self->items, node);
  return aug->n_visible;
}

static gpointer
gtk_filter_list_model_get_item (GListModel *list,
                                guint       position)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);
  guint unfiltered;

  switch (self->strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      return NULL;

    case GTK_FILTER_MATCH_ALL:
      unfiltered = position;
      break;

    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;
    case GTK_FILTER_MATCH_SOME:
      gtk_filter_list_model_get_nth_filtered (self->items, position, &unfiltered);
      break;
    }

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

  /* all other cases should have beeen optimized away */
  g_assert (self->strictness == GTK_FILTER_MATCH_SOME);

  item = g_list_model_get_item (self->model, position);
  visible = gtk_filter_match (self->filter, item);
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
      node = gtk_rb_tree_insert_before (self->items, after);
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

  switch (self->strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      return;

    case GTK_FILTER_MATCH_ALL:
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      return;

    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;
    case GTK_FILTER_MATCH_SOME:
      break;
    }

  node = gtk_filter_list_model_get_nth (self->items, position, &filter_position);

  filter_removed = 0;
  for (i = 0; i < removed; i++)
    {
      FilterNode *next = gtk_rb_tree_node_get_next (node);
      if (node->visible)
        filter_removed++;
      gtk_rb_tree_remove (self->items, node);
      node = next;
    }

  filter_added = gtk_filter_list_model_add_items (self, node, position, added);

  if (filter_removed > 0 || filter_added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), filter_position, filter_removed, filter_added);
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
    case PROP_FILTER:
      gtk_filter_list_model_set_filter (self, g_value_get_object (value));
      break;

    case PROP_ITEM_TYPE:
      self->item_type = g_value_get_gtype (value);
      break;

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
    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;

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
gtk_filter_list_model_clear_model (GtkFilterListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_filter_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
  if (self->items)
    gtk_rb_tree_remove_all (self->items);
}

/*<private>
 * gtk_filter_list_model_find_filtered:
 * @self: a #GtkFilterListModel
 * @start: (out) (caller-allocates): number of unfiltered items
 *     at start of list
 * @end: (out) (caller-allocates): number of unfiltered items
 *     at end of list
 * @n_items: (out) (caller-allocates): number of unfiltered items in
 *     list
 *
 * Checks if elements in self->items are filtered out and returns
 * the range that they occupy.  
 * This function is intended to be used for GListModel::items-changed
 * emissions, so it is called in an intermediate state for @self.
 *
 * Returns: %TRUE if elements are filtered out, %FALSE if none are
 **/
static gboolean
gtk_filter_list_model_find_filtered (GtkFilterListModel *self,
                                     guint              *start,
                                     guint              *end,
                                     guint              *n_items)
{
  FilterNode *root, *node, *tmp;
  FilterAugment *aug;

  if (self->items == NULL || self->model == NULL)
    return FALSE;

  root = gtk_rb_tree_get_root (self->items);
  if (root == NULL)
    return FALSE; /* empty parent model */

  aug = gtk_rb_tree_get_augment (self->items, root);
  if (aug->n_items == aug->n_visible)
    return FALSE; /* all items visible */

  /* find first filtered */
  *start = 0;
  *end = 0;
  *n_items = aug->n_visible;

  node = root;
  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          aug = gtk_rb_tree_get_augment (self->items, tmp);
          if (aug->n_visible < aug->n_items)
            {
              node = tmp;
              continue;
            }
          *start += aug->n_items;
        }

      if (!node->visible)
        break;

      (*start)++;

      node = gtk_rb_tree_node_get_right (node);
    }

  /* find last filtered by doing everything the opposite way */
  node = root;
  while (node)
    {
      tmp = gtk_rb_tree_node_get_right (node);
      if (tmp)
        {
          aug = gtk_rb_tree_get_augment (self->items, tmp);
          if (aug->n_visible < aug->n_items)
            {
              node = tmp;
              continue;
            }
          *end += aug->n_items;
        }

      if (!node->visible)
        break;

      (*end)++;

      node = gtk_rb_tree_node_get_left (node);
    }

  return TRUE;
}

static void
gtk_filter_list_model_refilter (GtkFilterListModel *self);

static void
gtk_filter_list_model_update_strictness_and_refilter (GtkFilterListModel *self)
{
  GtkFilterMatch new_strictness;

  if (self->model == NULL)
    new_strictness = GTK_FILTER_MATCH_NONE;
  else if (self->filter == NULL)
    new_strictness = GTK_FILTER_MATCH_ALL;
  else
    new_strictness = gtk_filter_get_strictness (self->filter);

  /* don't set self->strictness yet so get_n_items() and friends return old values */

  switch (new_strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      {
        guint n_before = g_list_model_get_n_items (G_LIST_MODEL (self));
        g_clear_pointer (&self->items, gtk_rb_tree_unref);
        self->strictness = new_strictness;
        if (n_before > 0)
          g_list_model_items_changed (G_LIST_MODEL (self), 0, n_before, 0);
      }
      break;

    case GTK_FILTER_MATCH_ALL:
      switch (self->strictness)
        {
        case GTK_FILTER_MATCH_NONE:
          self->strictness = new_strictness;
          g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, g_list_model_get_n_items (self->model));
          break;
        case GTK_FILTER_MATCH_ALL:
          self->strictness = new_strictness;
          break;
        default:
        case GTK_FILTER_MATCH_SOME:
          {
            guint start, end, n_before, n_after;
            self->strictness = new_strictness;
            if (gtk_filter_list_model_find_filtered (self, &start, &end, &n_before))
              {
                n_after = g_list_model_get_n_items (G_LIST_MODEL (self));
                g_clear_pointer (&self->items, gtk_rb_tree_unref);
                g_list_model_items_changed (G_LIST_MODEL (self), start, n_before - end - start, n_after - end - start);
              }
            else
              {
                g_clear_pointer (&self->items, gtk_rb_tree_unref);
              }
          }
          break;
        }
      break;
    
    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;
    case GTK_FILTER_MATCH_SOME:
      switch (self->strictness)
        {
        case GTK_FILTER_MATCH_NONE:
          {
            guint n_after;
            self->strictness = new_strictness;
            self->items = gtk_rb_tree_new (FilterNode,
                                           FilterAugment,
                                           gtk_filter_list_model_augment,
                                           NULL, NULL);
            n_after = gtk_filter_list_model_add_items (self, NULL, 0, g_list_model_get_n_items (self->model));
            if (n_after > 0)
              g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, n_after);
          }
          break;
        case GTK_FILTER_MATCH_ALL:
          {
            guint start, end, n_before, n_after;
            self->strictness = new_strictness;
            self->items = gtk_rb_tree_new (FilterNode,
                                           FilterAugment,
                                           gtk_filter_list_model_augment,
                                           NULL, NULL);
            n_before = g_list_model_get_n_items (self->model);
            gtk_filter_list_model_add_items (self, NULL, 0, n_before);
            if (gtk_filter_list_model_find_filtered (self, &start, &end, &n_after))
              g_list_model_items_changed (G_LIST_MODEL (self), start, n_before - end - start, n_after - end - start);
          }
          break;
        default:
        case GTK_FILTER_MATCH_SOME:
          gtk_filter_list_model_refilter (self);
          break;
        }
    }
}

static void
gtk_filter_list_model_filter_changed_cb (GtkFilter          *filter,
                                         GtkFilterChange     change,
                                         GtkFilterListModel *self)
{
  gtk_filter_list_model_update_strictness_and_refilter (self);
}

static void
gtk_filter_list_model_clear_filter (GtkFilterListModel *self)
{
  if (self->filter == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->filter, gtk_filter_list_model_filter_changed_cb, self);
  g_clear_object (&self->filter);
}

static void
gtk_filter_list_model_dispose (GObject *object)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  gtk_filter_list_model_clear_model (self);
  gtk_filter_list_model_clear_filter (self);
  g_clear_pointer (&self->items, gtk_rb_tree_unref);

  G_OBJECT_CLASS (gtk_filter_list_model_parent_class)->dispose (object);
}

static void
gtk_filter_list_model_class_init (GtkFilterListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_filter_list_model_set_property;
  gobject_class->get_property = gtk_filter_list_model_get_property;
  gobject_class->dispose = gtk_filter_list_model_dispose;

  /**
   * GtkFilterListModel:filter:
   *
   * The filter for this model
   */
  properties[PROP_FILTER] =
      g_param_spec_object ("filter",
                           P_("Filter"),
                           P_("The filter set for this model"),
                           GTK_TYPE_FILTER,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFilterListModel:item-type:
   *
   * The #GType for elements of this object
   */
  properties[PROP_ITEM_TYPE] =
      g_param_spec_gtype ("item-type",
                          P_("Item type"),
                          P_("The type of elements of this object"),
                          G_TYPE_OBJECT,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

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
  self->strictness = GTK_FILTER_MATCH_NONE;
}

/**
 * gtk_filter_list_model_new:
 * @model: the model to sort
 * @filter: (allow-none): filter or %NULL to not filter items
 *
 * Creates a new #GtkFilterListModel that will filter @model using the given
 * @filter.
 *
 * Returns: a new #GtkFilterListModel
 **/
GtkFilterListModel *
gtk_filter_list_model_new (GListModel *model,
                           GtkFilter  *filter)
{
  GtkFilterListModel *result;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  result = g_object_new (GTK_TYPE_FILTER_LIST_MODEL,
                         "item-type", g_list_model_get_item_type (model),
                         "model", model,
                         "filter", filter,
                         NULL);

  return result;
}

/**
 * gtk_filter_list_model_new_for_type:
 * @item_type: the type of the items that will be returned
 *
 * Creates a new empty filter list model set up to return items of type @item_type.
 * It is up to the application to set a proper filter and model to ensure
 * the item type is matched.
 *
 * Returns: a new #GtkFilterListModel
 **/
GtkFilterListModel *
gtk_filter_list_model_new_for_type (GType item_type)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (GTK_TYPE_FILTER_LIST_MODEL,
                       "item-type", item_type,
                       NULL);
}

/**
 * gtk_filter_list_model_set_filter:
 * @self: a #GtkFilterListModel
 * @filter: (allow-none) (transfer none): filter to use or %NULL to not filter items
 *
 * Sets the filter used to filter items.
 **/
void
gtk_filter_list_model_set_filter (GtkFilterListModel *self,
                                  GtkFilter          *filter)
{
  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  g_return_if_fail (filter == NULL || GTK_IS_FILTER (filter));

  if (self->filter == filter)
    return;

  gtk_filter_list_model_clear_filter (self);

  if (filter)
    {
      self->filter = g_object_ref (filter);
      g_signal_connect (filter, "changed", G_CALLBACK (gtk_filter_list_model_filter_changed_cb), self);
      gtk_filter_list_model_filter_changed_cb (filter, GTK_FILTER_CHANGE_DIFFERENT, self);
    }
  else
    {
      gtk_filter_list_model_update_strictness_and_refilter (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTER]);
}

/**
 * gtk_filter_list_model_get_filter:
 * @self: a #GtkFilterListModel
 *
 * Gets the #GtkFilter currently set on @self.
 *
 * Returns: (nullable) (transfer none): The filter currently in use
 *     or %NULL if the list isn't filtered
 **/
GtkFilter *
gtk_filter_list_model_get_filter (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), FALSE);

  return self->filter;
}

/**
 * gtk_filter_list_model_set_model:
 * @self: a #GtkFilterListModel
 * @model: (allow-none): The model to be filtered
 *
 * Sets the model to be filtered.
 *
 * Note that GTK makes no effort to ensure that @model conforms to
 * the item type of @self. It assumes that the caller knows what they
 * are doing and have set up an appropriate filter to ensure that item
 * types match.
 **/
void
gtk_filter_list_model_set_model (GtkFilterListModel *self,
                                 GListModel         *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  /* Note: We don't check for matching item type here, we just assume the
   * filter func takes care of filtering wrong items. */

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_filter_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_filter_list_model_items_changed_cb), self);
      if (removed == 0)
        {
          self->strictness = GTK_FILTER_MATCH_NONE;
          gtk_filter_list_model_update_strictness_and_refilter (self);
          added = 0;
        }
      else if (self->items)
        added = gtk_filter_list_model_add_items (self, NULL, 0, g_list_model_get_n_items (model));
      else
        added = g_list_model_get_n_items (model);
    }
  else
    {
      self->strictness = GTK_FILTER_MATCH_NONE;
      added = 0;
    }

  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_filter_list_model_get_model:
 * @self: a #GtkFilterListModel
 *
 * Gets the model currently filtered or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets filtered
 **/
GListModel *
gtk_filter_list_model_get_model (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), NULL);

  return self->model;
}

static void
gtk_filter_list_model_refilter (GtkFilterListModel *self)
{
  FilterNode *node;
  guint i, first_change, last_change;
  guint n_is_visible, n_was_visible;
  gboolean visible;

  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  
  if (self->items == NULL || self->model == NULL)
    return;

  first_change = G_MAXUINT;
  last_change = 0;
  n_is_visible = 0;
  n_was_visible = 0;
  for (i = 0, node = gtk_rb_tree_get_first (self->items);
       node != NULL;
       i++, node = gtk_rb_tree_node_get_next (node))
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
      gtk_rb_tree_node_mark_dirty (node);
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

