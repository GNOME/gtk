/* gtkaccessiblestateset.c: Accessible state
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

#include "gtkaccessiblestatesetprivate.h"

#include "gtkbitmaskprivate.h"
#include "gtkenums.h"

/* Keep in sync with GtkAccessibleState in gtkenums.h */
#define LAST_STATE GTK_ACCESSIBLE_STATE_SELECTED

struct _GtkAccessibleStateSet
{
  GtkBitmask *states_set;

  GtkAccessibleValue **state_values;
};

static GtkAccessibleStateSet *
gtk_accessible_state_set_init (GtkAccessibleStateSet *self)
{
  self->states_set = _gtk_bitmask_new ();
  self->state_values = g_new (GtkAccessibleValue*, LAST_STATE);

  /* Initialize all state values, so we can always get the full state */
  for (int i = 0; i < LAST_STATE; i++)
    self->state_values[i] = gtk_accessible_value_get_default_for_state (i);

  return self;
}

GtkAccessibleStateSet *
gtk_accessible_state_set_new (void)
{
  GtkAccessibleStateSet *set = g_rc_box_new0 (GtkAccessibleStateSet);

  return gtk_accessible_state_set_init (set);
}

GtkAccessibleStateSet *
gtk_accessible_state_set_ref (GtkAccessibleStateSet *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_rc_box_acquire (self);
}

static void
gtk_accessible_state_set_free (gpointer data)
{
  GtkAccessibleStateSet *self = data;

  for (int i = 0; i < LAST_STATE; i++)
    {
      if (self->state_values[i] != NULL)
        gtk_accessible_value_unref (self->state_values[i]);
    }

  g_free (self->state_values);

  _gtk_bitmask_free (self->states_set);
}

void
gtk_accessible_state_set_unref (GtkAccessibleStateSet *self)
{
  g_rc_box_release_full (self, gtk_accessible_state_set_free);
}

void
gtk_accessible_state_set_add (GtkAccessibleStateSet *self,
                              GtkAccessibleState     state,
                              GtkAccessibleValue    *value)
{
  g_return_if_fail (state >= GTK_ACCESSIBLE_STATE_BUSY &&
                    state <= GTK_ACCESSIBLE_STATE_SELECTED);

  if (gtk_accessible_state_set_contains (self, state))
    gtk_accessible_value_unref (self->state_values[state]);
  else
    self->states_set = _gtk_bitmask_set (self->states_set, state, TRUE);

  self->state_values[state] = gtk_accessible_value_ref (value);
}

void
gtk_accessible_state_set_remove (GtkAccessibleStateSet *self,
                                 GtkAccessibleState     state)
{
  g_return_if_fail (state >= GTK_ACCESSIBLE_STATE_BUSY &&
                    state <= GTK_ACCESSIBLE_STATE_SELECTED);

  if (gtk_accessible_state_set_contains (self, state))
    {
      g_clear_pointer (&(self->state_values[state]), gtk_accessible_value_unref);
      self->states_set = _gtk_bitmask_set (self->states_set, state, FALSE);
    }
}

gboolean
gtk_accessible_state_set_contains (GtkAccessibleStateSet *self,
                                   GtkAccessibleState     state)
{
  g_return_val_if_fail (state >= GTK_ACCESSIBLE_STATE_BUSY &&
                        state <= GTK_ACCESSIBLE_STATE_SELECTED,
                        FALSE);

  return _gtk_bitmask_get (self->states_set, state);
}

GtkAccessibleValue *
gtk_accessible_state_set_get_value (GtkAccessibleStateSet *self,
                                    GtkAccessibleState     state)
{
  g_return_val_if_fail (state >= GTK_ACCESSIBLE_STATE_BUSY &&
                        state <= GTK_ACCESSIBLE_STATE_SELECTED,
                        NULL);

  return self->state_values[state];
}

static const char *state_names[] = {
  [GTK_ACCESSIBLE_STATE_BUSY]     = "busy",
  [GTK_ACCESSIBLE_STATE_CHECKED]  = "checked",
  [GTK_ACCESSIBLE_STATE_DISABLED] = "disabled",
  [GTK_ACCESSIBLE_STATE_EXPANDED] = "expanded",
  [GTK_ACCESSIBLE_STATE_GRABBED]  = "grabbed",
  [GTK_ACCESSIBLE_STATE_HIDDEN]   = "hidden",
  [GTK_ACCESSIBLE_STATE_INVALID]  = "invalid",
  [GTK_ACCESSIBLE_STATE_PRESSED]  = "pressed",
  [GTK_ACCESSIBLE_STATE_SELECTED] = "selected",
};

/*< private >
 * gtk_accessible_state_set_print:
 * @self: a #GtkAccessibleStateSet
 * @only_set: %TRUE if only the set states should be printed
 * @buffer: a #GString
 *
 * Prints the contents of the #GtkAccessibleStateSet into @buffer.
 */
void
gtk_accessible_state_set_print (GtkAccessibleStateSet *self,
                                gboolean               only_set,
                                GString               *buffer)
{
  if (only_set && _gtk_bitmask_is_empty (self->states_set))
    {
      g_string_append (buffer, "{}");
      return;
    }

  g_string_append (buffer, "{\n");

  for (int i = 0; i < G_N_ELEMENTS (state_names); i++)
    {
      if (only_set && !_gtk_bitmask_get (self->states_set, i))
        continue;

      g_string_append (buffer, "    ");
      g_string_append (buffer, state_names[i]);
      g_string_append (buffer, ": ");

      gtk_accessible_value_print (self->state_values[i], buffer);

      g_string_append (buffer, ",\n");
    }

  g_string_append (buffer, "}");
}

/*< private >
 * gtk_accessible_state_set_to_string:
 * @self: a #GtkAccessibleStateSet
 *
 * Prints the contents of a #GtkAccessibleStateSet into a string.
 *
 * Returns: (transfer full): a newly allocated string with the contents
 *   of the #GtkAccessibleStateSet
 */
char *
gtk_accessible_state_set_to_string (GtkAccessibleStateSet *self)
{
  GString *buf = g_string_new (NULL);

  gtk_accessible_state_set_print (self, TRUE, buf);

  return g_string_free (buf, FALSE);
}
