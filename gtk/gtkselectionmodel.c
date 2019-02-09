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
 * GtkSelectionModel::selection-changed signal by calling the
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
  return gtk_selection_model_unselect_range (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));;
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

  g_object_interface_install_property (iface,
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("List managed by this selection"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE
                         | G_PARAM_CONSTRUCT_ONLY
                         | G_PARAM_EXPLICIT_NOTIFY
                         | G_PARAM_STATIC_STRINGS));

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

gboolean
gtk_selection_model_unselect_item (GtkSelectionModel *model,
                                   guint              position)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->unselect_item (model, position);
}

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

gboolean
gtk_selection_model_select_all (GtkSelectionModel *model)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->select_all (model);
}

gboolean
gtk_selection_model_unselect_all (GtkSelectionModel *model)
{
  GtkSelectionModelInterface *iface;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), 0);

  iface = GTK_SELECTION_MODEL_GET_IFACE (model);
  return iface->unselect_all (model);
}

/**
 * gtk_selection_model_get_model:
 * @model: a #GtkSelectionModel
 *
 * Gets the underlying model of @model.
 *
 * Returns: (transfer none): the underlying model
 */
GListModel *
gtk_selection_model_get_model (GtkSelectionModel *model)
{
  GListModel *child;

  g_object_get (model, "model", &child, NULL);
  if (child)
    g_object_unref (child);

  return child;
}

void
gtk_selection_model_selection_changed (GtkSelectionModel *model,
                                       guint              position,
                                       guint              n_items)
{
  g_return_if_fail (GTK_IS_SELECTION_MODEL (model));

  g_signal_emit (model, signals[SELECTION_CHANGED], 0, position, n_items);
}

