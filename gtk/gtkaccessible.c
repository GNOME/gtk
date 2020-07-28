/* gtkaccessible.c: Accessible interface
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

/**
 * SECTION:gtkaccessible
 * @Title: GtkAccessible
 * @Short_description: Accessible interface
 *
 * GtkAccessible provides an interface for describing a UI element, like a
 * #GtkWidget, in a way that can be consumed by Assistive Technologies, or
 * “AT”. Every accessible implementation has:
 *
 *  - a “role”, represented by a value of the #GtkAccessibleRole enumeration
 *  - an “attribute”, represented by a set of #GtkAccessibleState,
 *    #GtkAccessibleProperty and #GtkAccessibleRelation values
 *
 * The role cannot be changed after instantiating a #GtkAccessible
 * implementation.
 *
 * The attributes are updated every time a UI element's state changes in a way that
 * should be reflected by assistive technologies. For instance, if a #GtkWidget
 * visibility changes, the %GTK_ACCESSIBLE_STATE_HIDDEN state will also change
 * to reflect the #GtkWidget:visible property.
 */

#include "config.h"

#include "gtkaccessibleprivate.h"

#include "gtkatcontextprivate.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"

#include <stdarg.h>

G_DEFINE_INTERFACE (GtkAccessible, gtk_accessible, G_TYPE_OBJECT)

static void
gtk_accessible_default_init (GtkAccessibleInterface *iface)
{
  GParamSpec *pspec =
    g_param_spec_enum ("accessible-role",
                       "Accessible Role",
                       "The role of the accessible object",
                       GTK_TYPE_ACCESSIBLE_ROLE,
                       GTK_ACCESSIBLE_ROLE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_interface_install_property (iface, pspec);
}

/*< private >
 * gtk_accessible_get_at_context:
 * @self: a #GtkAccessible
 *
 * Retrieves the #GtkATContext for the given #GtkAccessible.
 *
 * Returns: (transfer none): the #GtkATContext
 */
GtkATContext *
gtk_accessible_get_at_context (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_at_context (self);
}

/**
 * gtk_accessible_get_accessible_role:
 * @self: a #GtkAccessible
 *
 * Retrieves the #GtkAccessibleRole for the given #GtkAccessible.
 *
 * Returns: a #GtkAccessibleRole
 */
GtkAccessibleRole
gtk_accessible_get_accessible_role (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), GTK_ACCESSIBLE_ROLE_NONE);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return GTK_ACCESSIBLE_ROLE_NONE;

  return gtk_at_context_get_accessible_role (context);
}

/**
 * gtk_accessible_update_state:
 * @self: a #GtkAccessible
 * @first_state: the first #GtkAccessibleState
 * @...: a list of state and value pairs, terminated by -1
 *
 * Updates a list of accessible states. See the #GtkAccessibleState
 * documentation for the value types of accessible states.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * state change must be communicated to assistive technologies.
 *
 * Example:
 * |[
 *   value = GTK_ACCESSIBLE_TRISTATE_MIXED;
 *   gtk_accessible_update_state (GTK_ACCESSIBLE (check_button),
 *                                GTK_ACCESSIBLE_STATE_CHECKED, value,
 *                                -1);
 * ]|
 */
void
gtk_accessible_update_state (GtkAccessible      *self,
                             GtkAccessibleState  first_state,
                             ...)
{
  GtkAccessibleState state;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_state);

  state = first_state;

  while (state != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_state (state, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect value for state “%s”: %s",
                      gtk_accessible_state_get_attribute_name (state),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_state (context, state, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      state = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);
}

/**
 * gtk_accessible_update_state_value:
 * @self: a #GtkAccessible
 * @state: a #GtkAccessibleState
 * @value: a #GValue with the value for @state
 *
 * Updates an accessible state.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * state change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_state_value (GtkAccessible      *self,
                                   GtkAccessibleState  state,
                                   const GValue       *value)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  GError *error = NULL;
  GtkAccessibleValue *real_value =
    gtk_accessible_value_collect_for_state_value (state, value, &error);

  if (error != NULL)
    {
      g_critical ("Unable to collect the value for state “%s”: %s",
                  gtk_accessible_state_get_attribute_name (state),
                  error->message);
      g_error_free (error);
      return;
    }

  gtk_at_context_set_accessible_state (context, state, real_value);

  if (real_value != NULL)
    gtk_accessible_value_unref (real_value);

  gtk_at_context_update (context);
}

/**
 * gtk_accessible_reset_state:
 * @self: a #GtkAccessible
 * @state: a #GtkAccessibleState
 *
 * Resets the accessible @state to its default value.
 */
void
gtk_accessible_reset_state (GtkAccessible      *self,
                            GtkAccessibleState  state)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_state (context, state, NULL);
  gtk_at_context_update (context);
}

/**
 * gtk_accessible_update_property:
 * @self: a #GtkAccessible
 * @first_property: the first #GtkAccessibleProperty
 * @...: a list of property and value pairs, terminated by -1
 *
 * Updates a list of accessible properties. See the #GtkAccessibleProperty
 * documentation for the value types of accessible properties.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * property change must be communicated to assistive technologies.
 *
 * Example:
 * |[
 *   value = gtk_adjustment_get_value (adjustment);
 *   gtk_accessible_update_property (GTK_ACCESSIBLE (spin_button),
                                     GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, value,
                                     -1);
 * ]|
 */
void
gtk_accessible_update_property (GtkAccessible         *self,
                                GtkAccessibleProperty  first_property,
                                ...)
{
  GtkAccessibleProperty property;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_property);

  property = first_property;

  while (property != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_property (property, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for property “%s”: %s",
                      gtk_accessible_property_get_attribute_name (property),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_property (context, property, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      property = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);
}

/**
 * gtk_accessible_update_property_value:
 * @self: a #GtkAccessible
 * @property: a #GtkAccessibleProperty
 * @value: a #GValue with the value for @property
 *
 * Updates an accessible property.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * property change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_property_value (GtkAccessible         *self,
                                      GtkAccessibleProperty  property,
                                      const GValue          *value)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  GError *error = NULL;
  GtkAccessibleValue *real_value =
    gtk_accessible_value_collect_for_property_value (property, value, &error);

  if (error != NULL)
    {
      g_critical ("Unable to collect the value for property “%s”: %s",
                  gtk_accessible_property_get_attribute_name (property),
                  error->message);
      g_error_free (error);
      return;
    }

  gtk_at_context_set_accessible_property (context, property, real_value);

  if (real_value != NULL)
    gtk_accessible_value_unref (real_value);

  gtk_at_context_update (context);
}

/**
 * gtk_accessible_reset_property:
 * @self: a #GtkAccessible
 * @property: a #GtkAccessibleProperty
 *
 * Resets the accessible @property to its default value.
 */
void
gtk_accessible_reset_property (GtkAccessible         *self,
                               GtkAccessibleProperty  property)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_property (context, property, NULL);
  gtk_at_context_update (context);
}

/**
 * gtk_accessible_update_relation:
 * @self: a #GtkAccessible
 * @first_relation: the first #GtkAccessibleRelation
 * @...: a list of relation and value pairs, terminated by -1
 *
 * Updates a list of accessible relations.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * relation change must be communicated to assistive technologies.
 */
void
gtk_accessible_update_relation (GtkAccessible         *self,
                                GtkAccessibleRelation  first_relation,
                                ...)
{
  GtkAccessibleRelation relation;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_relation);

  relation = first_relation;

  while (relation != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_relation (relation, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for relation “%s”: %s",
                      gtk_accessible_relation_get_attribute_name (relation),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_relation (context, relation, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      relation = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);
}

/**
 * gtk_accessible_update_relation_value:
 * @self: a #GtkAccessible
 * @relation: a #GtkAccessibleRelation
 * @value: a #GValue with the value for @relation
 *
 * Updates an accessible relation.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * relation change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_relation_value (GtkAccessible         *self,
                                      GtkAccessibleRelation  relation,
                                      const GValue          *value)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  GError *error = NULL;
  GtkAccessibleValue *real_value =
    gtk_accessible_value_collect_for_relation_value (relation, value, &error);

  if (error != NULL)
    {
      g_critical ("Unable to collect the value for relation “%s”: %s",
                  gtk_accessible_relation_get_attribute_name (relation),
                  error->message);
      g_error_free (error);
      return;
    }

  gtk_at_context_set_accessible_relation (context, relation, real_value);

  if (real_value != NULL)
    gtk_accessible_value_unref (real_value);

  gtk_at_context_update (context);
}

/**
 * gtk_accessible_reset_relation:
 * @self: a #GtkAccessible
 * @relation: a #GtkAccessibleRelation
 *
 * Resets the accessible @relation to its default value.
 */
void
gtk_accessible_reset_relation (GtkAccessible         *self,
                               GtkAccessibleRelation  relation)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_relation (context, relation, NULL);
  gtk_at_context_update (context);
}
