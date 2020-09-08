/*
 * Copyright © 2020 Red Hat, Inc
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
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "gtkreverselistmodel.h"

#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkreverselistmodel
 * @title: GtkReverseListModel
 * @short_description: A list model that reverses the order of a list model
 * @see_also: #GListModel
 *
 * #GtkReverseListModel is a list model that takes a list model and reverses
 * the order of that model.
 */

enum {
  PROP_0,
  PROP_MODEL,
  NUM_PROPERTIES
};

struct _GtkReverseListModel
{
  GObject parent_instance;

  GListModel *model;
};

struct _GtkReverseListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_reverse_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_reverse_list_model_get_n_items (GListModel *list)
{
  GtkReverseListModel *self = GTK_REVERSE_LIST_MODEL (list);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_reverse_list_model_get_item (GListModel *list,
                                 guint       position)
{
  GtkReverseListModel *self = GTK_REVERSE_LIST_MODEL (list);
  guint n_items;

  if (self->model == NULL)
    return NULL;

  n_items = g_list_model_get_n_items (self->model);

  if (position >= n_items)
    return NULL;

  return g_list_model_get_item (self->model, n_items - 1 - position);
}

static void
gtk_reverse_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_reverse_list_model_get_item_type;
  iface->get_n_items = gtk_reverse_list_model_get_n_items;
  iface->get_item = gtk_reverse_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkReverseListModel, gtk_reverse_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_reverse_list_model_model_init))

static void
gtk_reverse_list_model_items_changed_cb (GListModel          *model,
                                         guint                position,
                                         guint                removed,
                                         guint                added,
                                         GtkReverseListModel *self)
{
  guint n_items;
  guint position_before;

  n_items = g_list_model_get_n_items (model);

  position_before = (n_items - added + removed) - 1 - (position + removed - 1);

  g_list_model_items_changed (G_LIST_MODEL (self), position_before, removed, added);
}

static void
gtk_reverse_list_model_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkReverseListModel *self = GTK_REVERSE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_reverse_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_reverse_list_model_get_property (GObject     *object,
                                     guint        prop_id,
                                     GValue      *value,
                                     GParamSpec  *pspec)
{
  GtkReverseListModel *self = GTK_REVERSE_LIST_MODEL (object);

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
gtk_reverse_list_model_clear_model (GtkReverseListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_reverse_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
}

static void
gtk_reverse_list_model_dispose (GObject *object)
{
  GtkReverseListModel *self = GTK_REVERSE_LIST_MODEL (object);

  gtk_reverse_list_model_clear_model (self);

  G_OBJECT_CLASS (gtk_reverse_list_model_parent_class)->dispose (object);
};

static void
gtk_reverse_list_model_class_init (GtkReverseListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_reverse_list_model_set_property;
  gobject_class->get_property = gtk_reverse_list_model_get_property;
  gobject_class->dispose = gtk_reverse_list_model_dispose;

  /**
   * GtkReverseListModel:model:
   *
   * Child model to revert
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("Child model to revert"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_reverse_list_model_init (GtkReverseListModel *self)
{
}

/**
 * gtk_reverse_list_model_new:
 * @model: (transfer full) (allow-none): The model to use, or %NULL
 *
 * Creates a new model that presents the items from @model
 * in reverse order
 *
 * Returns: A new #GtkReverseListModel
 **/
GtkReverseListModel *
gtk_reverse_list_model_new (GListModel *model)
{
  GtkReverseListModel *self;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (GTK_TYPE_REVERSE_LIST_MODEL,
                       "model", model,
                       NULL);

  /* consume the reference */
  g_clear_object (&model);

  return self;
}

/**
 * gtk_reverse_list_model_set_model:
 * @self: a #GtkReverseListModel
 * @model: (allow-none): The model to be sliced
 *
 * Sets the model to revert. The model's item type must conform
 * to @self's item type.
 *
 **/
void
gtk_reverse_list_model_set_model (GtkReverseListModel *self,
                                  GListModel          *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_REVERSE_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_reverse_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_reverse_list_model_items_changed_cb), self);
      added = g_list_model_get_n_items (G_LIST_MODEL (self));
    }
  else
    {
      added = 0;
    }

  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_reverse_list_model_get_model:
 * @self: a #GtkReverseListModel
 *
 * Gets the model that is currently being used or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_reverse_list_model_get_model (GtkReverseListModel *self)
{
  g_return_val_if_fail (GTK_IS_REVERSE_LIST_MODEL (self), NULL);

  return self->model;
}
