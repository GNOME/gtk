/* gtktestatcontext.c: Test AT context
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

#include "gtktestatcontextprivate.h"

#include "gtkatcontextprivate.h"
#include "gtkenums.h"

struct _GtkTestATContext
{
  GtkATContext parent_instance;
};

G_DEFINE_TYPE (GtkTestATContext, gtk_test_at_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_test_at_context_state_change (GtkATContext             *self,
                                  GtkAccessibleStateChange  change,
                                  GtkAccessibleStateSet    *states)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkAccessibleRole role = gtk_at_context_get_accessible_role (self);
  char *state = gtk_accessible_state_set_to_string (states);

  g_print ("Accessible state changed for accessible “%s”, with role %d: %s\n",
           G_OBJECT_TYPE_NAME (accessible),
           role,
           state);

  g_free (state);
}

static void
gtk_test_at_context_class_init (GtkTestATContextClass *klass)
{
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  context_class->state_change = gtk_test_at_context_state_change;
}

static void
gtk_test_at_context_init (GtkTestATContext *self)
{
}

GtkATContext *
gtk_test_at_context_new (GtkAccessibleRole  accessible_role,
                         GtkAccessible     *accessible)
{
  return g_object_new (GTK_TYPE_TEST_AT_CONTEXT,
                       "accessible-role", accessible_role,
                       "accessible", accessible,
                       NULL);
}
