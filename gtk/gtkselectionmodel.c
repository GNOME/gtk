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

#include "gtkselectionmodel.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"

/**
 * SECTION:gtkselectionmodel
 * @Title: GtkSelectionModel
 * @Short_description: An extension of the list model interface that handles selections
 * @See_also: #GListModel, #GtkSingleSelection
 *
 * #GtkSelectionModel is an interface that extends the #GListModel interface by adding
 * support for selections. This support is then used by widgets using list models to add
 * the ability to select and unselect various items.
 *
 * GTK provides default implementations of the mode common selection modes such as
 * #GtkSingleSelection, so you will only need to implement this interface if you want
 * detailed control about how selections should be handled.
 *
 * A #GtkSelectionModel supports a single boolean per row indicating if a row is selected
 * or not. This can be queried via gtk_selection_model_is_selected(). When the selected
 * state of one or more rows changes, the model will emit the
 * #GtkSelectionModel::selection-changed signal by calling the
 * gtk_selection_model_selection_changed() function. The positions given in that signal
 * may have their selection state changed, though that is not a requirement.  
 * If new items added to the model via the #GListModel::items-changed signal are selected
 * or not is up to the implementation.
 *
 * Additionally, the interface can expose functionality to select and unselect items.
 * If these functions are implemented, GTK's list widgets will allow users to select and
 * unselect items. However, #GtkSelectionModels are free to only implement them
 * partially or not at all. In that case the widgets will not support the unimplemented
 * operations.
 *
 * When selecting or unselecting is supported by a model, the return values of the
 * selection functions do NOT indicate if selection or unselection happened. They are
 * only meant to indicate complete failure, like when this mode of selecting is not
 * supported by the model.
 *
 * Selections may happen asynchronously, so the only reliable way to find out when an
 * item was selected is to listen to the signals that indicate selection.
 */

G_DEFINE_INTERFACE (GtkSelectionModel, gtk_selection_model, G_TYPE_LIST_MODEL)

enum {
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gboolean
gtk_selection_model_default_is_selected (GtkSelectionModel *model,
                                         guint              position)
{
  return FALSE;
}

static gboolean
gtk_selection_model_default_select_item (GtkSelectionModel *model,
                                         guint              position,
                                         gboolean           exclusive)
{
  return FALSE;
}
static gboolean
gtk_selection_model_default_unselect_item (GtkSelectionModel *model,
                                           guint              position)
{
  return FALSE;
}

static gboolean
gtk_selection_model_default_select_range (GtkSelectionModel *model,
                                          guint              position,
                                          guint              n_items,
                                          gboolean           exclusive)
{
  return FALSE;
}

static gboolean
gtk_selection_model_default_unselect_range (GtkSelectionModel *model,
                                            guint              position,
                                            guint              n_items)
{
  return FALSE;
}

static gboolean
gtk_selection_model_default_select_all (GtkSelectionModel *model)
{
  return gtk_selection_model_select_range (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)), FALSE);
}

static gboolean
gtk_selection_model_default_unselect_all (GtkSelectionModel *model)
{
  return gtk_selection_model_unselect_range (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));
}

static void
gtk_selection_model_default_query_range (GtkSelectionModel *model,
                                         guint              position,
                                         guint             *start_range,
                                         guint             *n_items,
                                         gboolean          *selected)
{
  *start_range = position;

  if (position >= g_list_model_get_n_items (G_LIST_MODEL (model)))
    {
      *n_items = 0;
      *selected = FALSE;
    }
  else
    {
      *n_items = 1;
      *selected = gtk_selection_model_is_selected (model, position);  
    }
}

static void
gtk_selection_model_default_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_selection_model_default_is_selected; 
  iface->select_item = gtk_selection_model_default_select_item; 
  iface->unselect_item = gtk_selection_model_default_unselect_item; 
  iface->select_range = gtk_selection_model_default_select_range; 
  iface->unselect_range = gtk_selection_model_default_unselect_range; 
  iface->select_all = gtk_selection_model_default_select_all; 
  iface->unselect_all = gtk_selection_model_default_unselect_all; 
  iface->query_range = gtk_selection_model_default_query_range;

  /**
   * GtkSelectionModel::selection-changed
   * @model: a #GtkSelectionModel
   * @position: The first item that may have changed
   * @n_items: number of items with changes
   *
   * Emitted when the selection state of some of the items in @model changes.
   *
   * Note that this signal does not specify the new selection state of the items,
   * they need to be queried manually.  
   * It is also not necessary for a model to change the selection state of any of
   * the items in the selection model, though it would be rather useless to emit
   * such a signal.
   */
  signals[SELECTION_CHANGED] =
    g_signal_new ("selection-changed",
                  GTK_TYPE_SELECTION_MODEL,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[SELECTION_CHANGED],
                              GTK_TYPE_SELECTION_MODEL,
                              _gtk_marshal_VOID__UINT_UINTv);
}

/**
 * gtk_selection_model_is_selected:
 * @model: a #GtkSelectionModel
 * @position: the position of the item to query
 *
 * Checks if the given item is selected.
 *
 * Returns: %TRUE if the item is selected
 **/
gboolean
gtk_selection_model_is_selected (GtkSelectionModel *model,
                                 guint              position)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->is_selected (model, position);
}

/**
 * gtk_selection_model_select_item:
 * @model: a #GtkSelectionModel
 * @position: the position of the item to select
 * @exclusive: whether previously selected items should be unselected
 *
 * Requests to select an item in the model.
 */
gboolean
gtk_selection_model_select_item (GtkSelectionModel *model,
                                 guint              position,
                                 gboolean           exclusive)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->select_item (model, position, exclusive);
}

/**
 * gtk_selection_model_unselect_item:
 * @model: a #GtkSelectionModel
 * @position: the position of the item to unselect
 *
 * Requests to unselect an item in the model.
 */
gboolean
gtk_selection_model_unselect_item (GtkSelectionModel *model,
                                   guint              position)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->unselect_item (model, position);
}

/**
 * gtk_selection_model_select_range:
 * @model: a #GtkSelectionModel
 * @position: the first item to select
 * @n_items: the number of items to select
 * @exclusive: whether previously selected items should be unselected
 *
 * Requests to select a range of items in the model.
 */
gboolean
gtk_selection_model_select_range (GtkSelectionModel *model,
                                  guint              position,
                                  guint              n_items,
                                  gboolean           exclusive)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->select_range (model, position, n_items, exclusive);
}

/**
 * gtk_selection_model_unselect_range:
 * @model: a #GtkSelectionModel
 * @position: the first item to unselect
 * @n_items: the number of items to unselect
 *
 * Requests to unselect a range of items in the model.
 */
gboolean
gtk_selection_model_unselect_range (GtkSelectionModel *model,
                                    guint              position,
                                    guint              n_items)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->unselect_range (model, position, n_items);
}

/**
 * gtk_selection_model_select_all:
 * @model: a #GtkSelectionModel
 *
 * Requests to select all items in the model.
 */
gboolean
gtk_selection_model_select_all (GtkSelectionModel *model)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->select_all (model);
}

/**
 * gtk_selection_model_unselect_all:
 * @model: a #GtkSelectionModel
 *
 * Requests to unselect all items in the model.
 */
gboolean
gtk_selection_model_unselect_all (GtkSelectionModel *model)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->unselect_all (model);
}

/**
 * gtk_selection_model_query_range:
 * @model: a #GtkSelectionModel
 * @position: the position inside the range
 * @start_range: (out): returns the position of the first element of the range
 * @n_items: (out): returns the size of the range
 * @selected: (out): returns whether items in @range are selected
 *
 * This function allows to query the selection status of multiple elements at once.
 * It is passed a position and returns a range of elements of uniform selection status.
 *
 * If @position is greater than the number of items in @model, @n_items is set to 0.
 * Otherwise the returned range is guaranteed to include the passed-in position, so
 * @n_items will be >= 1.
 *
 * Positions directly adjacent to the returned range may have the same selection
 * status as the returned range.
 *
 * This is an optimization function to make iterating over a model faster when few
 * items are selected. However, it is valid behavior for implementations to use a
 * naive implementation that only ever returns a single element.
 */
void
gtk_selection_model_query_range (GtkSelectionModel *model,
                                 guint              position,
                                 guint             *start_range,
                                 guint             *n_items,
                                 gboolean          *selected)
{
  GtkSelectionModelInterface *iface;

  g_return_if_fail (GTK_IS_SELECTION_MODEL (model));
  g_return_if_fail (start_range != NULL);
  g_return_if_fail (n_items != NULL);
  g_return_if_fail (selected != NULL);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->query_range (model, position, start_range, n_items, selected);
}

/**
 * gtk_selection_model_selection_changed:
 * @model: a #GtkSelectionModel
 * @position: the first changed item
 * @n_items: the number of changed items
 *
 * Helper function for implementations of #GtkSelectionModel.
 * Call this when a the selection changes to emit the ::selection-changed
 * signal.
 */
void
gtk_selection_model_selection_changed (GtkSelectionModel *model,
                                       guint              position,
                                       guint              n_items)
{
  g_return_if_fail (GTK_IS_SELECTION_MODEL (model));
  g_return_if_fail (n_items > 0);
  g_return_if_fail (position + n_items <= g_list_model_get_n_items (G_LIST_MODEL (model)));

  g_signal_emit (model, signals[SELECTION_CHANGED], 0, position, n_items);
}

/**
 * gtk_selection_model_user_select_item:
 * @self: a #GtkSelectionModel
 * @pos: position selected by the user. If this position is invalid
 *     no selection will be done.
 * @modify: %TRUE if the selection should be modified, %FALSE
 *     if a new selection should be done. This is usually set
 *     to %TRUE if the user keeps the <Shift> key pressed.
 * @extend_pos: the position to extend the selection from or
 *     an invalid position like #GTK_INVALID_LIST_POSITION to not
 *     extend the selection. Selections are usually extended
 *     from the last selected position if the user presses the
 *     <Ctrl> key. The last selected position is stored by the
 *     widget
 *
 * Does a selection according to how GTK list widgets modify
 * selections, both when clicking rows with the mouse or when using
 * the keyboard.
 *
 * Returns: %TRUE if the last selected position for further calls
 *     to this function should be updated to @pos, %FALSE if the
 *     last selected position should not change.
 **/
gboolean
gtk_selection_model_user_select_item (GtkSelectionModel *self,
                                      guint              pos,
                                      gboolean           modify,
                                      guint              extend_pos)
{
  gboolean success = FALSE;
  guint n_items;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (self), FALSE);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  if (pos >= n_items)
    return FALSE;

  if (extend_pos < n_items)
    {
      guint max = MAX (extend_pos, pos);
      guint min = MIN (extend_pos, pos);
      if (modify)
        {
          if (gtk_selection_model_is_selected (self, extend_pos))
            {
              success = gtk_selection_model_select_range (self,
                                                          min,
                                                          max - min + 1,
                                                          FALSE);
            }
          else
            {
              success = gtk_selection_model_unselect_range (self,
                                                            min,
                                                            max - min + 1);
            }
        }
      else
        {
          success = gtk_selection_model_select_range (self,
                                                      min,
                                                      max - min + 1,
                                                      TRUE);
        }
      /* If there's no range to select or selecting ranges isn't supported
       * by the model, fall through to normal setting.
       */
    }
  if (success)
    return FALSE;

  if (modify)
    {
      if (gtk_selection_model_is_selected (self, pos))
        success = gtk_selection_model_unselect_item (self, pos);
      else
        success = gtk_selection_model_select_item (self, pos, FALSE);
    }
  else
    {
      success = gtk_selection_model_select_item (self, pos, TRUE);
    }

  return success;
}

