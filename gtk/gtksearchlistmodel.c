/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksearchlistmodel.h"

#include "gtkintl.h"
#include "gtkselectionmodel.h"

/**
 * SECTION:gtksearchlistmodel
 * @Short_description: A selection model that allows incremental searching
 * @Title: GtkSearchListModel
 * @see_also: #GtkSelectionModel
 *
 * GtkSearchListModel is an implementation of the #GtkSelectionModel interface 
 * that allows selecting a single element. The selected element can be determined
 * interactively via a filter.
 */
struct _GtkSearchListModel
{
  GObject parent_instance;

  GListModel *model;
  guint selected;
  gpointer selected_item;

  GtkFilter *filter;
};

struct _GtkSearchListModelClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_FILTER,
  PROP_SELECTED,
  PROP_SELECTED_ITEM,

  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_search_list_model_get_item_type (GListModel *list)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_search_list_model_get_n_items (GListModel *list)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_search_list_model_get_item (GListModel *list,
                                guint       position)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (list);

  return g_list_model_get_item (self->model, position);
}

static void
gtk_search_list_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_search_list_model_get_item_type;
  iface->get_n_items = gtk_search_list_model_get_n_items;
  iface->get_item = gtk_search_list_model_get_item;
}

static gboolean
gtk_search_list_model_is_selected (GtkSelectionModel *model,
                                   guint              position)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (model);

  return self->selected == position;
}

static void
gtk_search_list_model_query_range (GtkSelectionModel *model,
                                   guint              position,
                                   guint             *start_range,
                                   guint             *n_range,
                                   gboolean          *selected)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (model);
  guint n_items;

  n_items = g_list_model_get_n_items (self->model);

  if (position >= n_items)
    {
      *start_range = position;
      *n_range = 0;
      *selected = FALSE;
    }
  else if (self->selected == GTK_INVALID_LIST_POSITION)
    {
      *start_range = 0;
      *n_range = n_items;
      *selected = FALSE;
    }
  else if (position < self->selected)
    {
      *start_range = 0;
      *n_range = self->selected;
      *selected = FALSE;
    }
  else if (position > self->selected)
    {
      *start_range = self->selected + 1;
      *n_range = n_items - *start_range;
      *selected = FALSE;
    }
  else
    {
      *start_range = self->selected;
      *n_range = 1;
      *selected = TRUE;
    }
}

static void
gtk_search_list_model_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_search_list_model_is_selected; 
  iface->query_range = gtk_search_list_model_query_range;
}

G_DEFINE_TYPE_EXTENDED (GtkSearchListModel, gtk_search_list_model, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_search_list_model_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_search_list_model_selection_model_init))

static void
gtk_search_list_model_items_changed_cb (GListModel         *model,
                                        guint               position,
                                        guint               removed,
                                        guint               added,
                                        GtkSearchListModel *self)
{
  g_object_freeze_notify (G_OBJECT (self));

  if (self->selected_item == NULL)
    {
      /* nothing to do */
    }
  else if (self->selected < position)
    {
      /* unchanged */
    }
  else if (self->selected >= position + removed)
    {
      self->selected += added - removed;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
    }
  else
    {
      guint i;

      for (i = 0; i < added; i++)
        {
          gpointer item = g_list_model_get_item (model, position + i);
          if (item == self->selected_item)
            {
              /* the item moved */
              //TODO refilter
              if (self->selected != position + i)
                {
                  self->selected = position + i;
                  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
                  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
                }
              break;
            }
        }
      if (i == added)
        {
          /* the item really was deleted */
          g_clear_object (&self->selected_item);
          self->selected = GTK_INVALID_LIST_POSITION;
          //TODO refilter
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
        }
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
set_selected (GtkSearchListModel *self,
              guint               position)
{
  gpointer new_selected = NULL;
  guint old_position;

  if (self->selected == position)
    return;

  new_selected = g_list_model_get_item (self->model, position);

  if (new_selected == NULL)
    position = GTK_INVALID_LIST_POSITION;

  if (self->selected == position)
    return;

  old_position = self->selected;
  self->selected = position;
  g_clear_object (&self->selected_item);
  self->selected_item = new_selected;

  if (old_position == GTK_INVALID_LIST_POSITION)
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), position, 1);
  else if (position == GTK_INVALID_LIST_POSITION)
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), old_position, 1);
  else if (position < old_position)
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), position, old_position - position + 1);
  else
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), old_position, position - old_position + 1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
}

static gboolean
match_item (GtkSearchListModel *self,
            guint               position)
{
  gpointer item;
  gboolean result;

  item = g_list_model_get_item (self->model, position);
  result = gtk_filter_match (self->filter, item);
  g_object_unref (item);

  return result;
}

static guint
find_next_match (GtkSearchListModel *self,
                 guint               position,
                 gboolean            forward)
{
  guint i;

  if (position == GTK_INVALID_LIST_POSITION)
    position = 0;

  g_print ("search %s from %u\n", forward ? "forward" : "backward", position); 
  if (forward)
    for (i = position; i < g_list_model_get_n_items (self->model); i++)
      {
        if (match_item (self, i))
          return i;
      }
  else
    for (i = position; ; i--)
      {
        if (match_item (self, i))
          return i;
        if (i == 0)
          break;
      }

  return GTK_INVALID_LIST_POSITION;
}

static void
gtk_search_list_model_filter_changed_cb (GtkFilter          *filter,
                                         GtkFilterChange     change,
                                         GtkSearchListModel *self)
{
  guint position;

g_print ("filter changed: change %d, strictness %d\n", change, gtk_filter_get_strictness (self->filter));

  if (gtk_filter_get_strictness (self->filter) == GTK_FILTER_MATCH_NONE)
    position = GTK_INVALID_LIST_POSITION;
  else
    switch (change)
      {
      case GTK_FILTER_CHANGE_DIFFERENT:
      case GTK_FILTER_CHANGE_LESS_STRICT:
        position = find_next_match (self, 0, TRUE);
        break;
      case GTK_FILTER_CHANGE_MORE_STRICT:
        position = find_next_match (self, self->selected, TRUE);
        break;
      default:
        g_assert_not_reached ();
      }

  g_print ("select %u\n", position);
  set_selected (self, position);
}

static void
gtk_search_list_model_clear_model (GtkSearchListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, 
                                        gtk_search_list_model_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_search_list_model_clear_filter (GtkSearchListModel *self)
{
  if (self->filter == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->filter, 
                                        gtk_search_list_model_filter_changed_cb,
                                        self);

  g_clear_object (&self->filter);
}

static void
gtk_search_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)

{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      self->model = g_value_dup_object (value);
      g_signal_connect (self->model, "items-changed",
                        G_CALLBACK (gtk_search_list_model_items_changed_cb), self);
      break;

    case PROP_FILTER:
      self->filter = g_value_dup_object (value);
      g_signal_connect (self->filter, "changed",
                        G_CALLBACK (gtk_search_list_model_filter_changed_cb), self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_list_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;

    case PROP_SELECTED:
      g_value_set_uint (value, self->selected);
      break;

    case PROP_SELECTED_ITEM:
      g_value_set_object (value, self->selected_item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_list_model_dispose (GObject *object)
{
  GtkSearchListModel *self = GTK_SEARCH_LIST_MODEL (object);

  gtk_search_list_model_clear_model (self);
  gtk_search_list_model_clear_filter (self);

  self->selected = GTK_INVALID_LIST_POSITION;
  g_clear_object (&self->selected_item);

  G_OBJECT_CLASS (gtk_search_list_model_parent_class)->dispose (object);
}

static void
gtk_search_list_model_class_init (GtkSearchListModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_search_list_model_get_property;
  gobject_class->set_property = gtk_search_list_model_set_property;
  gobject_class->dispose = gtk_search_list_model_dispose;

  /**
   * GtkSearchListModel:selected:
   *
   * Position of the selected item
   */
  properties[PROP_SELECTED] =
    g_param_spec_uint ("selected",
                       P_("Selected"),
                       P_("Position of the selected item"),
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSearchListModel:selected-item:
   *
   * The selected item
   */
  properties[PROP_SELECTED_ITEM] =
    g_param_spec_object ("selected-item",
                       P_("Selected Item"),
                       P_("The selected item"),
                       G_TYPE_OBJECT,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSearchListModel:model:
   *
   * The model being managed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("The model"),
                         P_("The model being managed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSearchListModel:filter:
   *
   * The filter determining the selected item
   */
  properties[PROP_FILTER] =
    g_param_spec_object ("filter",
                         P_("The filter"),
                         P_("The filter being used"),
                         GTK_TYPE_FILTER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_search_list_model_init (GtkSearchListModel *self)
{
  self->selected = GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_search_list_model_new:
 * @model: (transfer none): the #GListModel to manage
 * @filter: (transfer none): the #GtkFilter to use
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkSearchListModel): a new #GtkSearchListModel
 **/
GtkSearchListModel *
gtk_search_list_model_new (GListModel *model,
                           GtkFilter  *filter)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_SEARCH_LIST_MODEL,
                       "model", model,
                       "filter", filter,
                       NULL);
}

gboolean
gtk_search_list_model_next_match (GtkSearchListModel *self)
{
  guint position;

  position = find_next_match (self, self->selected, TRUE);
  if (position == GTK_INVALID_LIST_POSITION)
    return FALSE;

  set_selected (self, position);

  return TRUE;
}

gboolean
gtk_search_list_model_previous_match (GtkSearchListModel *self)
{
  guint position;

  position = find_next_match (self, self->selected, FALSE);
  if (position == GTK_INVALID_LIST_POSITION)
    return FALSE;

  set_selected (self, position);

  return TRUE;
}
