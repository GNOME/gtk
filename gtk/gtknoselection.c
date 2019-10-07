/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtknoselection.h"

#include "gtkintl.h"
#include "gtkselectionmodel.h"

/**
 * SECTION:gtknoselection
 * @Short_description: A selection model that does not allow selecting anything
 * @Title: GtkNoSelection
 * @see_also: #GtkSelectionModel
 *
 * GtkNoSelection is an implementation of the #GtkSelectionModel interface 
 * that does not allow selecting anything.
 *
 * This model is meant to be used as a simple wrapper to #GListModels when a
 * #GtkSelectionModel is required.
 */
struct _GtkNoSelection
{
  GObject parent_instance;

  GListModel *model;
};

struct _GtkNoSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_no_selection_get_item_type (GListModel *list)
{
  GtkNoSelection *self = GTK_NO_SELECTION (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_no_selection_get_n_items (GListModel *list)
{
  GtkNoSelection *self = GTK_NO_SELECTION (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_no_selection_get_item (GListModel *list,
                               guint       position)
{
  GtkNoSelection *self = GTK_NO_SELECTION (list);

  return g_list_model_get_item (self->model, position);
}

static void
gtk_no_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_no_selection_get_item_type;
  iface->get_n_items = gtk_no_selection_get_n_items;
  iface->get_item = gtk_no_selection_get_item;
}

static gboolean
gtk_no_selection_is_selected (GtkSelectionModel *model,
                              guint              position)
{
  return FALSE;
}

static void
gtk_no_selection_query_range (GtkSelectionModel *model,
                              guint              position,
                              guint             *start_range,
                              guint             *n_range,
                              gboolean          *selected)
{
  GtkNoSelection *self = GTK_NO_SELECTION (model);

  *start_range = 0;
  *n_range = g_list_model_get_n_items (self->model);
  *selected = FALSE;
}

static void
gtk_no_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_no_selection_is_selected; 
  iface->query_range = gtk_no_selection_query_range;
}

G_DEFINE_TYPE_EXTENDED (GtkNoSelection, gtk_no_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_no_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_no_selection_selection_model_init))

static void
gtk_no_selection_clear_model (GtkNoSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, 
                                        g_list_model_items_changed,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_no_selection_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkNoSelection *self = GTK_NO_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_no_selection_clear_model (self);
      self->model = g_value_dup_object (value);
      g_signal_connect_swapped (self->model, "items-changed",
                                G_CALLBACK (g_list_model_items_changed), self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_no_selection_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkNoSelection *self = GTK_NO_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_no_selection_dispose (GObject *object)
{
  GtkNoSelection *self = GTK_NO_SELECTION (object);

  gtk_no_selection_clear_model (self);

  G_OBJECT_CLASS (gtk_no_selection_parent_class)->dispose (object);
}

static void
gtk_no_selection_class_init (GtkNoSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_no_selection_get_property;
  gobject_class->set_property = gtk_no_selection_set_property;
  gobject_class->dispose = gtk_no_selection_dispose;

  /**
   * GtkNoSelection:model:
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
gtk_no_selection_init (GtkNoSelection *self)
{
}

/**
 * gtk_no_selection_new:
 * @model: (transfer none): the #GListModel to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkNoSelection): a new #GtkNoSelection
 **/
GtkNoSelection *
gtk_no_selection_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_NO_SELECTION,
                       "model", model,
                       NULL);
}

/**
 * gtk_no_selection_get_model:
 * @self: a #GtkNoSelection
 *
 * Gets the model that @self is wrapping.
 *
 * Returns: (transfer none): The model being wrapped
 **/
GListModel *
gtk_no_selection_get_model (GtkNoSelection *self)
{
  g_return_val_if_fail (GTK_IS_NO_SELECTION (self), NULL);

  return self->model;
}

