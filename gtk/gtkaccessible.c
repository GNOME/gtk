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
 *  - a “state”, represented by a set of #GtkAccessibleState and
 *    #GtkAccessibleProperty values
 *
 * The role cannot be changed after instantiating a #GtkAccessible
 * implementation.
 *
 * The state is updated every time a UI element's state changes in a way that
 * should be reflected by assistive technologies. For instance, if a #GtkWidget
 * visibility changes, the %GTK_ACCESSIBLE_STATE_HIDDEN state will also change
 * to reflect the #GtkWidget:visible property.
 */

#include "config.h"

#include "gtkaccessibleprivate.h"

#include "gtkatcontextprivate.h"
#include "gtkenums.h"

#include <stdarg.h>

G_DEFINE_INTERFACE (GtkAccessible, gtk_accessible, G_TYPE_OBJECT)

static void
gtk_accessible_default_init (GtkAccessibleInterface *iface)
{
}

GtkATContext *
gtk_accessible_get_at_context (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_at_context (self);
}

/**
 * gtk_accessible_update_state:
 * @self: a #GtkAccessible
 * @first_state: the first #GtkAccessibleState
 * @...: a list of state and value pairs, terminated by %GTK_ACCESSIBLE_STATE_NONE
 *
 * Updates a list of accessible states.
 *
 * This function should be called by #GtkWidget types whenever an accessible
 * state change must be communicated to assistive technologies.
 */
void
gtk_accessible_update_state (GtkAccessible *self,
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

  while (state != GTK_ACCESSIBLE_STATE_NONE)
    {
      GtkAccessibleValue *value = gtk_accessible_value_collect_for_state (state, &args);

      if (value == NULL)
        goto out;

      gtk_at_context_set_state (context, state, value);
      gtk_accessible_value_unref (value);

      state = va_arg (args, int);
    }

  gtk_at_context_update_state (context);

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

  GtkAccessibleValue *real_value =
    gtk_accessible_value_collect_for_state_value (state, value);

  if (real_value == NULL)
    return;

  gtk_at_context_set_state (context, state, real_value);
  gtk_accessible_value_unref (real_value);
  gtk_at_context_update_state (context);
}
