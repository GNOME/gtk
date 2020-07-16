/* gtkaccessiblepropertyset.c: Accessible properties
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkaccessiblepropertysetprivate.h"

#include "gtkbitmaskprivate.h"
#include "gtkenums.h"

/* Keep in sync with GtkAccessibleProperty in gtkenums.h */
#define LAST_PROPERTY GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT

struct _GtkAccessiblePropertySet
{
  GtkBitmask *property_set;

  GtkAccessibleValue **property_values;
};

static GtkAccessiblePropertySet *
gtk_accessible_property_set_init (GtkAccessiblePropertySet *self)
{
  self->property_set = _gtk_bitmask_new ();
  self->property_values = g_new (GtkAccessibleValue *, LAST_PROPERTY);

  /* Initialize all property values, so we can always get the full set */
  for (int i = 0; i < LAST_PROPERTY; i++)
    self->property_values[i] = gtk_accessible_value_get_default_for_property (i);

  return self;
}

GtkAccessiblePropertySet *
gtk_accessible_property_set_new (void)
{
  GtkAccessiblePropertySet *set = g_rc_box_new0 (GtkAccessiblePropertySet);

  return gtk_accessible_property_set_init (set);
}

GtkAccessiblePropertySet *
gtk_accessible_property_set_ref (GtkAccessiblePropertySet *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_rc_box_acquire (self);
}

static void
gtk_accessible_property_set_free (gpointer data)
{
  GtkAccessiblePropertySet *self = data;

  for (int i = 0; i < LAST_PROPERTY; i++)
    {
      if (self->property_values[i] != NULL)
        gtk_accessible_value_unref (self->property_values[i]);
    }

  g_free (self->property_values);

  _gtk_bitmask_free (self->property_set);
}

void
gtk_accessible_property_set_unref (GtkAccessiblePropertySet *self)
{
  g_rc_box_release_full (self, gtk_accessible_property_set_free);
}

void
gtk_accessible_property_set_add (GtkAccessiblePropertySet *self,
                                 GtkAccessibleProperty     property,
                                 GtkAccessibleValue       *value)
{
  g_return_if_fail (property >= GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE &&
                    property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT);

  if (gtk_accessible_property_set_contains (self, property))
    gtk_accessible_value_unref (self->property_values[property]);
  else
    self->property_set = _gtk_bitmask_set (self->property_set, property, TRUE);

  self->property_values[property] = gtk_accessible_value_ref (value);
}

void
gtk_accessible_property_set_remove (GtkAccessiblePropertySet *self,
                                    GtkAccessibleProperty     property)
{
  g_return_if_fail (property >= GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE &&
                    property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT);

  if (gtk_accessible_property_set_contains (self, property))
    {
      g_clear_pointer (&(self->property_values[property]), gtk_accessible_value_unref);
      self->property_set = _gtk_bitmask_set (self->property_set, property, FALSE);
    }
}

gboolean
gtk_accessible_property_set_contains (GtkAccessiblePropertySet *self,
                                      GtkAccessibleProperty     property)
{
  g_return_val_if_fail (property >= GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE &&
                        property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT,
                        FALSE);

  return _gtk_bitmask_get (self->property_set, property);
}

GtkAccessibleValue *
gtk_accessible_property_set_get_value (GtkAccessiblePropertySet *self,
                                       GtkAccessibleProperty     property)
{
  g_return_val_if_fail (property >= GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE &&
                        property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT,
                        NULL);

  return self->property_values[property];
}

static const char *property_names[] = {
  [GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE]     = "autocomplete",
  [GTK_ACCESSIBLE_PROPERTY_DESCRIPTION]      = "description",
  [GTK_ACCESSIBLE_PROPERTY_HAS_POPUP]        = "haspopup",
  [GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS]    = "keyshortcuts",
  [GTK_ACCESSIBLE_PROPERTY_LABEL]            = "label",
  [GTK_ACCESSIBLE_PROPERTY_LEVEL]            = "level",
  [GTK_ACCESSIBLE_PROPERTY_MODAL]            = "modal",
  [GTK_ACCESSIBLE_PROPERTY_MULTI_LINE]       = "multiline",
  [GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE] = "multiselectable",
  [GTK_ACCESSIBLE_PROPERTY_ORIENTATION]      = "orientation",
  [GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER]      = "placeholder",
  [GTK_ACCESSIBLE_PROPERTY_READ_ONLY]        = "readonly",
  [GTK_ACCESSIBLE_PROPERTY_REQUIRED]         = "required",
  [GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION] = "roledescription",
  [GTK_ACCESSIBLE_PROPERTY_SORT]             = "sort",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_MAX]        = "valuemax",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_MIN]        = "valuemin",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_NOW]        = "valuenow",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT]       = "valuetext",
};

/*< private >
 * gtk_accessible_property_set_print:
 * @self: a #GtkAccessiblePropertySet
 * @only_set: %TRUE if only the set properties should be printed
 * @buffer: a #GString
 *
 * Prints the contents of the #GtkAccessiblePropertySet into @buffer.
 */
void
gtk_accessible_property_set_print (GtkAccessiblePropertySet *self,
                                   gboolean               only_set,
                                   GString               *buffer)
{
  if (only_set && _gtk_bitmask_is_empty (self->property_set))
    {
      g_string_append (buffer, "{}");
      return;
    }

  g_string_append (buffer, "{\n");

  for (int i = 0; i < G_N_ELEMENTS (property_names); i++)
    {
      if (only_set && !_gtk_bitmask_get (self->property_set, i))
        continue;

      g_string_append (buffer, "    ");
      g_string_append (buffer, property_names[i]);
      g_string_append (buffer, ": ");

      gtk_accessible_value_print (self->property_values[i], buffer);

      g_string_append (buffer, ",\n");
    }

  g_string_append (buffer, "}");
}

/*< private >
 * gtk_accessible_property_set_to_string:
 * @self: a #GtkAccessiblePropertySet
 *
 * Prints the contents of a #GtkAccessiblePropertySet into a string.
 *
 * Returns: (transfer full): a newly allocated string with the contents
 *   of the #GtkAccessiblePropertySet
 */
char *
gtk_accessible_property_set_to_string (GtkAccessiblePropertySet *self)
{
  GString *buf = g_string_new (NULL);

  gtk_accessible_property_set_print (self, TRUE, buf);

  return g_string_free (buf, FALSE);
}
