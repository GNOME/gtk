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

#define GDK_ARRAY_TYPE_NAME Sections
#define GDK_ARRAY_NAME sections
#define GDK_ARRAY_ELEMENT_TYPE guint
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
  GTK_LIST_VALIDATION_SECTION_CHANGES = (1 << 1),
  GTK_LIST_VALIDATION_MINIMAL_CHANGES = (1 << 2),
  GTK_LIST_VALIDATION_N_ITEMS = (1 << 3),
  GTK_LIST_VALIDATION_N_ITEMS_MINIMAL_NOTIFY = (1 << 4),
} GtkListValidationFlags;

struct _GtkListModelValidator
{
  GObject parent_instance;

  GListModel *model;
  GtkListValidationFlags flags;

  guint notified_n_items;
  Items items;
  Sections sections;
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
gtk_list_model_validator_self_check_sections (GtkListModelValidator *self)
{
  guint sum, i;

  sum = 0;
  for (i = 0; i < sections_get_size (&self->sections); i++)
    {
      g_assert (sections_get (&self->sections, i) > 0);
      sum += sections_get (&self->sections, i);
    }

  g_assert (sum == items_get_size (&self->items));
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

static gsize
gtk_list_model_validator_find_section_index (GtkListModelValidator *self,
                                             guint                  position,
                                             guint                 *offset)
{
  gsize i;

  for (i = 0; i < sections_get_size (&self->sections); i++)
    {
      guint items = sections_get (&self->sections, i);
      if (position < items)
        {
          if (offset)
            *offset = position;
          return i;
        }
      position -= items;
    }

  if (offset)
    *offset = position;
  return i;
}

static void
gtk_list_model_validator_validate_section_range (GtkListModelValidator *self,
                                                 guint                  position,
                                                 guint                  n_items)
{
  guint section, section_items, offset, start, end, i;
  
  if (n_items == 0)
    return;

  section = gtk_list_model_validator_find_section_index (self, position, &offset);
  section_items = sections_get (&self->sections, section);

  for (i = 0; i < n_items; i++)
    {
      gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), position + i, &start, &end);
      if (start != position + i - offset ||
          end != position + i - offset + section_items)
        {
          gtk_list_model_validator_error (self, GTK_LIST_VALIDATION_SECTION_CHANGES,
                                          "item at %u reports wrong section: [%u, %u) but should be [%u, %u)",
                                          position + i,
                                          start, end,
                                          position + i - offset, position + i - offset + section_items);
        }
      offset++;
      if (offset == section_items)
        {
          section++;
          if (section < sections_get_size (&self->sections))
            section_items = sections_get (&self->sections, section);
          else
            section_items = 0;
          offset = 0;
        }
    }
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
gtk_list_model_validator_invalidate_sections (GtkListModelValidator *self,
                                              guint                  position,
                                              guint                  removed,
                                              guint                  added)
{
  guint section, offset, remaining;
  
  section = gtk_list_model_validator_find_section_index (self, position, &offset);
  
  /* first, delete all the removed items from the sections */
  remaining = removed;
  while (remaining > 0)
    {
      guint section_items = sections_get (&self->sections, section);
      if (remaining >= section_items - offset)
        {
          if (offset)
            {
              remaining -= section_items - offset;
              *sections_index (&self->sections, section) = offset;
              section++;
              offset = 0;
            }
          else
            {
              remaining -= section_items;
              sections_splice (&self->sections, section, 1, FALSE, NULL, 0);
            }
        }
      else
        {
          *sections_index (&self->sections, section) -= remaining;
          remaining = 0;
        }
    }

  /* now add all the new items into their sections */
  if (offset > 0 && sections_get (&self->sections, section) > offset)
    {
      *sections_index (&self->sections, section) += added;
    }
  else if (added > 0)
    {
      guint start, end;
      guint pos = position;

      remaining = added;
      if (offset == 0 && section > 0)
        {
          section--;
          offset = sections_get (&self->sections, section);
        }

      if (offset)
        g_assert (offset == sections_get (&self->sections, section));
      else
        g_assert (section == 0);

      gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), pos, &start, &end);
      if (start < pos)
        {
          g_assert (remaining >= end - start - offset);
          remaining -= end - start - offset;
          *sections_index (&self->sections, section) = end - start;
          section++;
          pos = end;
        }
      else if (offset)
        section++;

      while (remaining > 0)
        {
          gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), pos, &start, &end);
          if (end - start <= remaining)
            {
              sections_splice (&self->sections, section, 0, FALSE, (guint[1]) { end - start }, 1);
              section++;
              remaining -= end - start;
              pos = end;
            }
          else
            {
              g_assert (end - start == remaining + sections_get (&self->sections, section));
              *sections_index (&self->sections, section) = end - start;
              section++;
              remaining = 0;
            }
        }
    }

  gtk_list_model_validator_self_check_sections (self);
}

static void
gtk_list_model_validator_sections_changed_cb (GtkSectionModel       *model,
                                              guint                  position,
                                              guint                  n_items,
                                              GtkListModelValidator *self)
{
  gtk_list_model_validator_invalidate_sections (self, position, n_items, n_items);

  gtk_list_model_validator_validate_section_range (self, 0, items_get_size (&self->items));
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

  if (GTK_IS_SECTION_MODEL (model))
    {
      gtk_list_model_validator_invalidate_sections (self, position, removed, added);

      /* Should we only validate unchanged items here?
       * Because this also validates the newly added ones */
      gtk_list_model_validator_validate_section_range (self, 0, items_get_size (&self->items));
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
  sections_set_size (&self->sections, 0);
  g_signal_handlers_disconnect_by_func (self->model, gtk_list_model_validator_items_changed_cb, self);
  g_signal_handlers_disconnect_by_func (self->model, gtk_list_model_validator_notify_n_items_cb, self);
  if (GTK_IS_SECTION_MODEL (self->model))
    g_signal_handlers_disconnect_by_func (self->model, gtk_list_model_validator_sections_changed_cb, self);
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
  items_init (&self->items);
  sections_init (&self->sections);
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
  guint i;

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
      if (GTK_IS_SECTION_MODEL (model))
        g_signal_connect (model, "sections-changed", G_CALLBACK (gtk_list_model_validator_sections_changed_cb), self);
      g_object_weak_ref (G_OBJECT (model), gtk_list_model_validator_weak_unref_cb, self);

      self->notified_n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
      items_set_size (&self->items, self->notified_n_items);
      for (i = 0; i < self->notified_n_items; i++)
        {
          *items_index (&self->items, i) = g_list_model_get_item (model, i);
        }
      if (GTK_IS_SECTION_MODEL (self->model))
        {
          guint start, end;

          for (i = 0; i < self->notified_n_items; i = end)
            {
              gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), i, &start, &end);
              sections_append (&self->sections, end - start);
            }
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
