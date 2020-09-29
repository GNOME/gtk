/* gtkatspicontext.c: AT-SPI GtkATContext implementation
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

#include "gtkatspicontextprivate.h"

struct _GtkAtSpiContext
{
  GtkATContext parent_instance;
};

G_DEFINE_TYPE (GtkAtSpiContext, gtk_at_spi_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_at_spi_context_state_change (GtkATContext                *self,
                                 GtkAccessibleStateChange     changed_states,
                                 GtkAccessiblePropertyChange  changed_properties,
                                 GtkAccessibleRelationChange  changed_relations,
                                 GtkAccessibleAttributeSet   *states,
                                 GtkAccessibleAttributeSet   *properties,
                                 GtkAccessibleAttributeSet   *relations)
{
}

static void
gtk_at_spi_context_constructed (GObject *gobject)
{
  G_OBJECT_CLASS (gtk_at_spi_context_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_context_class_init (GtkAtSpiContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_context_constructed;

  context_class->state_change = gtk_at_spi_context_state_change;
}

static void
gtk_at_spi_context_init (GtkAtSpiContext *self)
{
}

/*< private >
 * gtk_at_spi_context_new:
 * @accessible_role: the accessible role for the AT context
 * @accessible: the #GtkAccessible instance which owns the context
 *
 * Creates a new #GtkAtSpiContext instance for @accessible, using the
 * given @accessible_role.
 *
 * Returns: (transfer full): the newly created #GtkAtSpiContext
 */
GtkATContext *
gtk_at_spi_context_new (GtkAccessibleRole  accessible_role,
                        GtkAccessible *    accessible)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);

  return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                       "accessible-role", accessible_role,
                       "accessible", accessible,
                       NULL);
}
