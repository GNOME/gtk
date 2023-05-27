/*
 * Copyright © 2023 Benjamin Otte
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

#include "gtklistmodelvalidator.h"

#include "gtkprivate.h"

#define GDK_ARRAY_TYPE_NAME Items
#define GDK_ARRAY_NAME items
#define GDK_ARRAY_ELEMENT_TYPE gpointer
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_NO_MEMSET 1

#include "gdk/gdkarrayimpl.c"

/*<private>
 * GtkListModelValidator:
 *
 * `GtkListModelValidator` is an object that can be attached to a given list model
 * and ten checks that it conforms to the listmodel API guarantees.
 *
 * This is useful when implementing a new listmodel or when writing a testsuite.
 *
 * Note that these checks are expensive and can cause signficant slow downs. So it
 * is recommended to only use them when testing.
 */

enum {
  PROP_0,
  PROP_MODEL,
  NUM_PROPERTIES
};

typedef enum 
{
  GTK_LIST_VALIDATION_CHANGES = (1 << 0),
  GTK_LIST_VALIDATION_MINIMAL_CHANGES = (1 << 1),
  GTK_LIST_VALIDATION_N_ITEMS = (1 << 2),
  GTK_LIST_VALIDATION_N_ITEMS_MINIMAL_NOTIFY = (1 << 3),
} GtkListValidationFlags;

struct _GtkListModelValidator
{
  GObject parent_instance;

  GListModel *model;
  GtkListValidationFlags flags;

  guint notified_n_items;
  Items items;
};

struct _GtkListModelValidatorClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkListModelValidator, gtk_list_model_validator, G_TYPE_OBJECT)

static void
gtk_list_model_validator_error (GtkListModelValidator  *self,
                                GtkListValidationFlags  flags,
                                const char             *format,
                                ...) G_GNUC_PRINTF(3, 4);
static void
gtk_list_model_validator_error (GtkListModelValidator  *self,
                                GtkListValidationFlags  flags,
                                const char             *format,
                                ...)
{
  va_list args;

  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, format, args);
  va_end (args);
}

static void
gtk_list_model_validator_validate_different (GtkListModelValidator *self,
                                             guint                  self_position,
                                             guint                  model_position)
{
  gpointer o = g_list_model_get_item (self->model, model_position);
  if (o == items_get (&self->items, self_position))
    gtk_list_model_validator_error (self, GTK_LIST_VALIDATION_MINIMAL_CHANGES,
                                     "item at position %u did not change but was part of items-changed",
                                     self_position);
  g_object_unref (o);
}

static void
gtk_list_model_validator_validate_range (GtkListModelValidator *self,
                                         guint                  self_position,
                                         guint                  model_position,
                                         guint                  n_items)
{
  guint i;

  if (n_items == 0)
    return;

  for (i = 0; i < n_items; i++)
    {
      gpointer o = g_list_model_get_item (self->model, model_position + i);
      if (o != items_get (&self->items, self_position + i))
        gtk_list_model_validator_error (self, GTK_LIST_VALIDATION_CHANGES,
                                        "item at %u did change but was not included in items-changed",
                                        self_position + i);
      g_object_unref (o);
    }
}

static void
gtk_list_model_validator_items_changed_cb (GListModel            *model,
                                           guint                  position,
                                           guint                  removed,
                                           guint                  added,
                                           GtkListModelValidator *self)
{
  guint i;

  if (self->notified_n_items != items_get_size (&self->items))
    gtk_list_model_validator_error (self, GTK_LIST_VALIDATION_N_ITEMS,
                                     "notify::n-items wasn't emitted before new modifications: %u, should be %zu",
                                     self->notified_n_items, items_get_size (&self->items));

  gtk_list_model_validator_validate_range (self, 0, 0, position);
  gtk_list_model_validator_validate_range (self,
                                           position + removed,
                                           position + added, 
                                           items_get_size (&self->items) - removed - position);
  if (removed > 0 && added > 0)
    {
      gtk_list_model_validator_validate_different (self, position, position);
      gtk_list_model_validator_validate_different (self, position + removed - 1, position + added - 1);
    }

  items_splice (&self->items, position, removed, FALSE, NULL, added);
  for (i = 0; i < added; i++)
    {
      *items_index (&self->items, position + i) = g_list_model_get_item (model, position + i);
    }
}

static void
gtk_list_model_validator_notify_n_items_cb (GListModel            *model,
                                            GParamSpec            *pspec,
                                            GtkListModelValidator *self)
{
  guint new_n_items = g_list_model_get_n_items (model);

  if (items_get_size (&self->items) != new_n_items)
    gtk_list_model_validator_error (self, GTK_LIST_VALIDATION_N_ITEMS,
                                     "notify::n-items reports wrong item count: %u, should be %zu",
                                     new_n_items, items_get_size (&self->items));

  if (self->notified_n_items == new_n_items)
    gtk_list_model_validator_error (self, GTK_LIST_VALIDATION_N_ITEMS_MINIMAL_NOTIFY,
                                     "notify::n-items unchanged from last emission: %u items",
                                     new_n_items);

  self->notified_n_items = new_n_items;
}

static void
gtk_list_model_validator_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkListModelValidator *self = GTK_LIST_MODEL_VALIDATOR (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_list_model_validator_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_list_model_validator_get_property (GObject     *object,
                                       guint        prop_id,
                                       GValue      *value,
                                       GParamSpec  *pspec)
{
  GtkListModelValidator *self = GTK_LIST_MODEL_VALIDATOR (object);

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
gtk_list_model_validator_weak_unref_cb (gpointer  data,
                                        GObject  *not_the_model_anymore)
{
  GtkListModelValidator *self = data;

  g_assert (G_OBJECT (self->model) == not_the_model_anymore);

  items_set_size (&self->items, 0);
  self->model = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);

  g_object_unref (self);
}

static void
gtk_list_model_validator_clear_model (GtkListModelValidator *self)
{
  if (self->model == NULL)
    return;

  items_set_size (&self->items, 0);
  g_signal_handlers_disconnect_by_func (self->model, gtk_list_model_validator_items_changed_cb, self);
  g_signal_handlers_disconnect_by_func (self->model, gtk_list_model_validator_notify_n_items_cb, self);
  g_object_weak_unref (G_OBJECT (self->model), gtk_list_model_validator_weak_unref_cb, self);
  self->model = NULL;
}

static void
gtk_list_model_validator_dispose (GObject *object)
{
  GtkListModelValidator *self = GTK_LIST_MODEL_VALIDATOR (object);

  g_assert (self->model == NULL);

  G_OBJECT_CLASS (gtk_list_model_validator_parent_class)->dispose (object);
};

static void
gtk_list_model_validator_class_init (GtkListModelValidatorClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_list_model_validator_set_property;
  gobject_class->get_property = gtk_list_model_validator_get_property;
  gobject_class->dispose = gtk_list_model_validator_dispose;

  /*<private>
   * GtkListModelValidator:model: (attributes org.gtk.Property.get=gtk_list_model_validator_get_model org.gtk.Property.set=gtk_list_model_validator_set_model)
   *
   * Child model to validate.
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_list_model_validator_init (GtkListModelValidator *self)
{
}

/*<private>
 * gtk_list_model_validator_new:
 * @model: (transfer full) (nullable): The model to use
 *
 * Creates a new validation model.
 *
 * Returns: A new `GtkListModelValidator`
 */
GtkListModelValidator *
gtk_list_model_validator_new (GListModel *model)
{
  GtkListModelValidator *self;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (GTK_TYPE_LIST_MODEL_VALIDATOR,
                       "model", model,
                       NULL);

  /* consume the reference */
  g_clear_object (&model);

  return self;
}

/*<private>
 * gtk_list_model_validator_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkListModelValidator`
 * @model: (nullable): The model to be validated
 *
 * Sets the model to validate.
 */
void
gtk_list_model_validator_set_model (GtkListModelValidator *self,
                                    GListModel            *model)
{
  guint i, added;

  g_return_if_fail (GTK_IS_LIST_MODEL_VALIDATOR (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  if (self->model)
    gtk_list_model_validator_clear_model (self);
  else
    g_object_ref (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_list_model_validator_items_changed_cb), self);
      g_signal_connect (model, "notify::n-items", G_CALLBACK (gtk_list_model_validator_notify_n_items_cb), self);
      g_object_weak_ref (G_OBJECT (model), gtk_list_model_validator_weak_unref_cb, self);

      added = g_list_model_get_n_items (G_LIST_MODEL (model));
      items_set_size (&self->items, added);
      for (i = 0; i < added; i++)
        {
          *items_index (&self->items, i) = g_list_model_get_item (model, i);
        }
    }
  else
    {
      g_object_unref (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/*<private>
 * gtk_list_model_validator_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkListModelValidator`
 *
 * Gets the model that is currently being used or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model in use
 */
GListModel *
gtk_list_model_validator_get_model (GtkListModelValidator *self)
{
  g_return_val_if_fail (GTK_IS_LIST_MODEL_VALIDATOR (self), NULL);

  return self->model;
}
