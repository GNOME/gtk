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

#include "gtkslicelistmodel.h"

#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkslicelistmodel
 * @title: GtkSliceListModel
 * @short_description: A list model that presents a slice out of a larger list
 * @see_also: #GListModel
 *
 * #GtkSliceListModel is a list model that takes a list model and presents a slice of
 * that model.
 *
 * This is useful when implementing paging by setting the size to the number of elements
 * per page and updating the offset whenever a different page is opened.
 */

#define DEFAULT_SIZE 10

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  PROP_OFFSET,
  PROP_SIZE,
  NUM_PROPERTIES
};

struct _GtkSliceListModel
{
  GObject parent_instance;

  GType item_type;
  GListModel *model;
  guint offset;
  guint size;

  guint n_items;
};

struct _GtkSliceListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_slice_list_model_get_item_type (GListModel *list)
{
  GtkSliceListModel *self = GTK_SLICE_LIST_MODEL (list);

  return self->item_type;
}

static guint
gtk_slice_list_model_get_n_items (GListModel *list)
{
  GtkSliceListModel *self = GTK_SLICE_LIST_MODEL (list);
  guint n_items;
  
  if (self->model == NULL)
    return 0;

  /* XXX: This can be done without calling g_list_model_get_n_items() on the parent model
   * by checking if model.get_item(offset + size) != NULL */
  n_items = g_list_model_get_n_items (self->model);
  if (n_items <= self->offset)
    return 0;

  n_items -= self->offset;
  return MIN (n_items, self->size);
}

static gpointer
gtk_slice_list_model_get_item (GListModel *list,
                               guint       position)
{
  GtkSliceListModel *self = GTK_SLICE_LIST_MODEL (list);

  if (self->model == NULL)
    return NULL;

  if (position >= self->size)
    return NULL;

  return g_list_model_get_item (self->model, position + self->offset);
}

static void
gtk_slice_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_slice_list_model_get_item_type;
  iface->get_n_items = gtk_slice_list_model_get_n_items;
  iface->get_item = gtk_slice_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkSliceListModel, gtk_slice_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_slice_list_model_model_init))

static void
gtk_slice_list_model_items_changed_cb (GListModel        *model,
                                       guint              position,
                                       guint              removed,
                                       guint              added,
                                       GtkSliceListModel *self)
{
  if (position >= self->offset + self->size)
    return;

  if (position < self->offset)
    {
      guint skip = MIN (removed, added);
      skip = MIN (skip, position - self->offset);

      position += skip;
      removed -= skip;
      added -= skip;
    }

  if (removed == added)
    {
      guint changed = removed;

      if (changed == 0)
        return;

      g_assert (position >= self->offset);
      position -= self->offset;
      changed = MIN (changed, self->size) - position;

      g_list_model_items_changed (G_LIST_MODEL (self), position, changed, changed);
    }
  else
    {
      guint n_after, n_before;
      guint skip;

      if (position > self->offset)
        skip = position - self->offset;
      else
        skip = 0;

      n_after = g_list_model_get_n_items (self->model);
      n_before = n_after - added + removed;
      n_after = CLAMP (n_after, self->offset, self->offset + self->size) - self->offset;
      n_before = CLAMP (n_before, self->offset, self->offset + self->size) - self->offset;

      g_list_model_items_changed (G_LIST_MODEL (self), skip, n_before - skip, n_after - skip);
    }
}

static void
gtk_slice_list_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkSliceListModel *self = GTK_SLICE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      self->item_type = g_value_get_gtype (value);
      break;

    case PROP_MODEL:
      gtk_slice_list_model_set_model (self, g_value_get_object (value));
      break;

    case PROP_OFFSET:
      gtk_slice_list_model_set_offset (self, g_value_get_uint (value));
      break;

    case PROP_SIZE:
      gtk_slice_list_model_set_size (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_slice_list_model_get_property (GObject     *object,
                                   guint        prop_id,
                                   GValue      *value,
                                   GParamSpec  *pspec)
{
  GtkSliceListModel *self = GTK_SLICE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, self->item_type);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_OFFSET:
      g_value_set_uint (value, self->offset);
      break;

    case PROP_SIZE:
      g_value_set_uint (value, self->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_slice_list_model_clear_model (GtkSliceListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_slice_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
}

static void
gtk_slice_list_model_dispose (GObject *object)
{
  GtkSliceListModel *self = GTK_SLICE_LIST_MODEL (object);

  gtk_slice_list_model_clear_model (self);

  G_OBJECT_CLASS (gtk_slice_list_model_parent_class)->dispose (object);
};

static void
gtk_slice_list_model_class_init (GtkSliceListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_slice_list_model_set_property;
  gobject_class->get_property = gtk_slice_list_model_get_property;
  gobject_class->dispose = gtk_slice_list_model_dispose;

  /**
   * GtkSliceListModel:item-type:
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
   * GtkSliceListModel:model:
   *
   * Child model to take slice from
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("Child model to take slice from"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSliceListModel:offset:
   *
   * Offset of slice
   */
  properties[PROP_OFFSET] =
      g_param_spec_uint ("offset",
                         P_("Offset"),
                         P_("Offset of slice"),
                         0, G_MAXUINT, 0,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSliceListModel:size:
   *
   * Maximum size of slice
   */
  properties[PROP_SIZE] =
      g_param_spec_uint ("size",
                         P_("Size"),
                         P_("Maximum size of slice"),
                         0, G_MAXUINT, DEFAULT_SIZE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_slice_list_model_init (GtkSliceListModel *self)
{
  self->size = DEFAULT_SIZE;
}

/**
 * gtk_slice_list_model_new:
 * @model: (transfer none): The model to use
 * @offset: the offset of the slice
 * @size: maximum size of the slice
 *
 * Creates a new slice model that presents the slice from @offset to
 * @offset + @size our of the given @model.
 *
 * Returns: A new #GtkSliceListModel
 **/
GtkSliceListModel *
gtk_slice_list_model_new (GListModel *model,
                          guint       offset,
                          guint       size)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_SLICE_LIST_MODEL,
                       "item-type", g_list_model_get_item_type (model),
                       "model", model,
                       "offset", offset,
                       "size", size,
                       NULL);
}

/**
 * gtk_slice_list_model_new_for_type:
 * @item_type: the type of items
 *
 * Creates a new empty #GtkSliceListModel for the given @item_type that
 * can be set up later.
 *
 * Returns: a new empty #GtkSliceListModel
 **/
GtkSliceListModel *
gtk_slice_list_model_new_for_type (GType item_type)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (GTK_TYPE_SLICE_LIST_MODEL,
                       "item-type", item_type,
                       NULL);
}

/**
 * gtk_slice_list_model_set_model:
 * @self: a #GtkSliceListModel
 * @model: (allow-none): The model to be sliced
 *
 * Sets the model to show a slice of. The model's item type must conform
 * to @self's item type.
 *
 **/
void
gtk_slice_list_model_set_model (GtkSliceListModel *self,
                                GListModel      *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_SLICE_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_slice_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_slice_list_model_items_changed_cb), self);
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
 * gtk_slice_list_model_get_model:
 * @self: a #GtkSliceListModel
 *
 * Gets the model that is curently being used or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_slice_list_model_get_model (GtkSliceListModel *self)
{
  g_return_val_if_fail (GTK_IS_SLICE_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * gtk_slice_list_model_set_offset:
 * @self: a #GtkSliceListModel
 * @offset: the new offset to use
 *
 * Sets the offset into the original model for this slice.
 *
 * If the offset is too large for the sliced model,
 * @self will end up empty. 
 **/
void
gtk_slice_list_model_set_offset (GtkSliceListModel *self,
                                 guint              offset)
{
  guint before, after;

  g_return_if_fail (GTK_IS_SLICE_LIST_MODEL (self));

  if (self->offset == offset)
    return;

  before = g_list_model_get_n_items (G_LIST_MODEL (self));

  self->offset = offset;

  after = g_list_model_get_n_items (G_LIST_MODEL (self));

  if (before > 0 || after > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, before, after);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OFFSET]);
}

/**
 * gtk_slice_list_model_get_offset:
 * @self: a #GtkSliceListModel
 *
 * Gets the offset set via gtk_slice_list_model_set_offset()
 *
 * Returns: The offset
 **/
guint
gtk_slice_list_model_get_offset (GtkSliceListModel *self)
{
  g_return_val_if_fail (GTK_IS_SLICE_LIST_MODEL (self), 0);

  return self->offset;
}

/**
 * gtk_slice_list_model_set_size:
 * @self: a #GtkSliceListModel
 * @size: the maximum size
 *
 * Sets the maximum size. @self will never have more items
 * than @size.
 *
 * It can however have fewer items if the offset is too large or
 * the model sliced from doesn't have enough items.
 */
void
gtk_slice_list_model_set_size (GtkSliceListModel *self,
                               guint              size)
{
  guint before, after;

  g_return_if_fail (GTK_IS_SLICE_LIST_MODEL (self));

  if (self->size == size)
    return;

  before = g_list_model_get_n_items (G_LIST_MODEL (self));

  self->size = size;

  after = g_list_model_get_n_items (G_LIST_MODEL (self));

  if (before > after)
    g_list_model_items_changed (G_LIST_MODEL (self), after, before - after, 0);
  else if (before < after)
    g_list_model_items_changed (G_LIST_MODEL (self), before, 0, after - before);
  /* else nothing */

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIZE]);
}

/**
 * gtk_slice_list_model_get_size:
 * @self: a #GtkSliceListModel
 *
 * Gets the size set via gtk_slice_list_model_set_size().
 *
 * Returns: The size
 **/
guint
gtk_slice_list_model_get_size (GtkSliceListModel *self)
{
  g_return_val_if_fail (GTK_IS_SLICE_LIST_MODEL (self), DEFAULT_SIZE);

  return self->size;
}


