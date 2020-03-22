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

#include "gtksingleselection.h"

#include "gtkintl.h"
#include "gtkselectionmodel.h"

/**
 * SECTION:gtksingleselection
 * @Short_description: A selection model that allows selecting a single item
 * @Title: GtkSingleSelection
 * @see_also: #GtkSelectionModel
 *
 * GtkSingleSelection is an implementation of the #GtkSelectionModel interface 
 * that allows selecting a single element. It is the default selection method
 * used by list widgets in GTK.
 */
struct _GtkSingleSelection
{
  GObject parent_instance;

  GListModel *model;
  guint selected;
  gpointer selected_item;

  guint autoselect : 1;
  guint can_unselect : 1;
};

struct _GtkSingleSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_AUTOSELECT,
  PROP_CAN_UNSELECT,
  PROP_SELECTED,
  PROP_SELECTED_ITEM,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_single_selection_get_item_type (GListModel *list)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_single_selection_get_n_items (GListModel *list)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_single_selection_get_item (GListModel *list,
                               guint       position)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (list);

  return g_list_model_get_item (self->model, position);
}

static void
gtk_single_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_single_selection_get_item_type;
  iface->get_n_items = gtk_single_selection_get_n_items;
  iface->get_item = gtk_single_selection_get_item;
}

static gboolean
gtk_single_selection_is_selected (GtkSelectionModel *model,
                                  guint              position)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (model);

  return self->selected == position;
}

static gboolean
gtk_single_selection_select_item (GtkSelectionModel *model,
                                  guint              position,
                                  gboolean           exclusive)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (model);

  /* XXX: Should we check that position < n_items here? */
  gtk_single_selection_set_selected (self, position);

  return TRUE;
}

static gboolean
gtk_single_selection_unselect_item (GtkSelectionModel *model,
                                    guint              position)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (model);

  if (!self->can_unselect)
    return FALSE;

  if (self->selected == position)
    gtk_single_selection_set_selected (self, GTK_INVALID_LIST_POSITION);

  return TRUE;
}

static void
gtk_single_selection_query_range (GtkSelectionModel *model,
                                  guint              position,
                                  guint             *start_range,
                                  guint             *n_range,
                                  gboolean          *selected)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (model);
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
gtk_single_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_single_selection_is_selected; 
  iface->select_item = gtk_single_selection_select_item; 
  iface->unselect_item = gtk_single_selection_unselect_item; 
  iface->query_range = gtk_single_selection_query_range;
}

G_DEFINE_TYPE_EXTENDED (GtkSingleSelection, gtk_single_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_single_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_single_selection_selection_model_init))

static void
gtk_single_selection_items_changed_cb (GListModel         *model,
                                       guint               position,
                                       guint               removed,
                                       guint               added,
                                       GtkSingleSelection *self)
{
  g_object_freeze_notify (G_OBJECT (self));

  if (self->selected_item == NULL)
    {
      if (self->autoselect)
        {
          self->selected_item = g_list_model_get_item (self->model, 0);
          if (self->selected_item)
            {
              self->selected = 0;
              g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
              g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
            }
        }
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
          g_object_unref (item);
          if (item == self->selected_item)
            {
              /* the item moved */
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
          if (self->autoselect)
            {
              self->selected = position + (self->selected - position) * added / removed;
              self->selected_item = g_list_model_get_item (self->model, self->selected);
              if (self->selected_item == NULL)
                {
                  if (position > 0)
                    {
                      self->selected = position - 1;
                      self->selected_item = g_list_model_get_item (self->model, self->selected);
                      g_assert (self->selected_item);
                      /* We pretend the newly selected item was part of the original model change.
                       * This way we get around inconsistent state (no item selected) during
                       * the items-changed emission. */
                      position--;
                      removed++;
                      added++;
                    }
                  else
                    self->selected = GTK_INVALID_LIST_POSITION;
                }
              else
                {
                  if (self->selected == position + added)
                    {
                      /* We pretend the newly selected item was part of the original model change.
                       * This way we get around inconsistent state (no item selected) during
                       * the items-changed emission. */
                      removed++;
                      added++;
                    }
                }
            }
          else
            {
              g_clear_object (&self->selected_item);
              self->selected = GTK_INVALID_LIST_POSITION;
            }
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
        }
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
gtk_single_selection_clear_model (GtkSingleSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, 
                                        gtk_single_selection_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_single_selection_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_AUTOSELECT:
      gtk_single_selection_set_autoselect (self, g_value_get_boolean (value));
      break;

    case PROP_CAN_UNSELECT:
      gtk_single_selection_set_can_unselect (self, g_value_get_boolean (value));
      break;

    case PROP_MODEL:
      gtk_single_selection_clear_model (self);
      self->model = g_value_dup_object (value);
      g_signal_connect (self->model, "items-changed",
                        G_CALLBACK (gtk_single_selection_items_changed_cb), self);
      if (self->autoselect)
        gtk_single_selection_set_selected (self, 0);
      break;

    case PROP_SELECTED:
      gtk_single_selection_set_selected (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_single_selection_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_AUTOSELECT:
      g_value_set_boolean (value, self->autoselect);
      break;

    case PROP_CAN_UNSELECT:
      g_value_set_boolean (value, self->can_unselect);
      break;
    case PROP_MODEL:
      g_value_set_object (value, self->model);
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
gtk_single_selection_dispose (GObject *object)
{
  GtkSingleSelection *self = GTK_SINGLE_SELECTION (object);

  gtk_single_selection_clear_model (self);

  self->selected = GTK_INVALID_LIST_POSITION;
  g_clear_object (&self->selected_item);

  G_OBJECT_CLASS (gtk_single_selection_parent_class)->dispose (object);
}

static void
gtk_single_selection_class_init (GtkSingleSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_single_selection_get_property;
  gobject_class->set_property = gtk_single_selection_set_property;
  gobject_class->dispose = gtk_single_selection_dispose;

  /**
   * GtkSingleSelection:autoselect:
   *
   * If the selection will always select an item
   */
  properties[PROP_AUTOSELECT] =
    g_param_spec_boolean ("autoselect",
                          P_("Autoselect"),
                          P_("If the selection will always select an item"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSingleSelection:can-unselect:
   *
   * If unselecting the selected item is allowed
   */
  properties[PROP_CAN_UNSELECT] =
    g_param_spec_boolean ("can-unselect",
                          P_("Can unselect"),
                          P_("If unselecting the selected item is allowed"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSingleSelection:selected:
   *
   * Position of the selected item
   */
  properties[PROP_SELECTED] =
    g_param_spec_uint ("selected",
                       P_("Selected"),
                       P_("Position of the selected item"),
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSingleSelection:selected-item:
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
   * GtkSingleSelection:model:
   *
   * The model being managed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                       P_("The model"),
                       P_("The model being managed"),
                       G_TYPE_LIST_MODEL,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_single_selection_init (GtkSingleSelection *self)
{
  self->selected = GTK_INVALID_LIST_POSITION;
  self->autoselect = TRUE;
}

/**
 * gtk_single_selection_new:
 * @model: (transfer none): the #GListModel to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkSingleSelection): a new #GtkSingleSelection
 **/
GtkSingleSelection *
gtk_single_selection_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_SINGLE_SELECTION,
                       "model", model,
                       NULL);
}

/**
 * gtk_single_selection_get_model:
 * @self: a #GtkSingleSelection
 *
 * Gets the model that @self is wrapping.
 *
 * Returns: (transfer none): The model being wrapped
 **/
GListModel *
gtk_single_selection_get_model (GtkSingleSelection *self)
{
  g_return_val_if_fail (GTK_IS_SINGLE_SELECTION (self), NULL);

  return self->model;
}

/**
 * gtk_single_selection_get_selected:
 * @self: a #GtkSingleSelection
 *
 * Gets the position of the selected item. If no item is selected,
 * #GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The position of the selected item
 **/
guint
gtk_single_selection_get_selected (GtkSingleSelection *self)
{
  g_return_val_if_fail (GTK_IS_SINGLE_SELECTION (self), GTK_INVALID_LIST_POSITION);

  return self->selected;
}

/**
 * gtk_single_selection_set_selected:
 * @self: a #GtkSingleSelection
 * @position: the item to select or #GTK_INVALID_LIST_POSITION
 *
 * Selects the item at the given position. If the list does not have an item at
 * @position or #GTK_INVALID_LIST_POSITION is given, the behavior depends on the
 * value of the GtkSingleSelection:autoselect property: If it is set, no change
 * will occur and the old item will stay selected. If it is unset, the selection
 * will be unset and no item will be selected.
 **/
void
gtk_single_selection_set_selected (GtkSingleSelection *self,
                                   guint               position)
{
  gpointer new_selected = NULL;
  guint old_position;

  g_return_if_fail (GTK_IS_SINGLE_SELECTION (self));

  if (self->selected == position)
    return;

  if (self->model)
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

/**
 * gtk_single_selection_get_selected_item:
 * @self: a #GtkSingleSelection
 * 
 * Gets the selected item. If no item is selected, %NULL is returned.
 *
 * Returns: (transfer none): The selected item
 */
gpointer
gtk_single_selection_get_selected_item (GtkSingleSelection *self)
{
  g_return_val_if_fail (GTK_IS_SINGLE_SELECTION (self), NULL);

  return self->selected_item;
}

/**
 * gtk_single_selection_get_autoselect:
 * @self: a #GtkSingleSelection
 *
 * Checks if autoselect has been enabled or disabled via
 * gtk_single_selection_set_autoselect().
 *
 * Returns: %TRUE if autoselect is enabled
 **/
gboolean
gtk_single_selection_get_autoselect (GtkSingleSelection *self)
{
  g_return_val_if_fail (GTK_IS_SINGLE_SELECTION (self), TRUE);

  return self->autoselect;
}

/**
 * gtk_single_selection_set_autoselect:
 * @self: a #GtkSingleSelection
 * @autoselect: %TRUE to always select an item
 *
 * If @autoselect is %TRUE, @self will enforce that an item is always
 * selected. It will select a new item when the currently selected
 * item is deleted and it will disallow unselecting the current item.
 **/
void
gtk_single_selection_set_autoselect (GtkSingleSelection *self,
                                     gboolean            autoselect)
{
  g_return_if_fail (GTK_IS_SINGLE_SELECTION (self));

  if (self->autoselect == autoselect)
    return;

  self->autoselect = autoselect;

  g_object_freeze_notify (G_OBJECT (self));
  
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTOSELECT]);

  if (self->autoselect && !self->selected_item)
    gtk_single_selection_set_selected (self, 0);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_single_selection_get_can_unselect:
 * @self: a #GtkSingleSelection
 *
 * If %TRUE, gtk_selection_model_unselect_item() is supported and allows
 * unselecting the selected item.
 *
 * Returns: %TRUE to support unselecting
 **/
gboolean
gtk_single_selection_get_can_unselect (GtkSingleSelection *self)
{
  g_return_val_if_fail (GTK_IS_SINGLE_SELECTION (self), FALSE);

  return self->can_unselect;
}

/**
 * gtk_single_selection_set_can_unselect:
 * @self: a #GtkSingleSelection
 * @can_unselect: %TRUE to allow unselecting
 *
 * If %TRUE, unselecting the current item via
 * gtk_selection_model_unselect_item() is supported.
 *
 * Note that setting GtkSingleSelection:autoselect will cause the
 * unselecting to not work, so it practically makes no sense to set
 * both at the same time the same time..
 **/
void
gtk_single_selection_set_can_unselect (GtkSingleSelection *self,
                                       gboolean            can_unselect)
{
  g_return_if_fail (GTK_IS_SINGLE_SELECTION (self));

  if (self->can_unselect == can_unselect)
    return;

  self->can_unselect = can_unselect;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_UNSELECT]);
}
