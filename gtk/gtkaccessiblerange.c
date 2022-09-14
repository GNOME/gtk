/* gtkaccessiblerange.c: Accessible range interface
 *
 * Copyright © 2022 Red Hat Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkAccessibleRange:
 * 
 * This interface describes range controls, e. g. controls which have a single
 * value which can optionally be changed by the user.
 * 
 * This interface is expected to be implemented by controls of the
 * %GTK_ACCESSIBLE_ROLE_METER, %GTK_ACCESSIBLE_ROLE_PROGRESS_BAR,
 * %GTK_ACCESSIBLE_ROLE_SCROLLBAR, %GTK_ACCESSIBLE_ROLE_SLIDER and %GTK_ACCESSIBLE_ROLE_SPIN_BUTTON
 * role. If that is not the case, a warning will be issued at runtime.
 * 
 * In addition to this interface, its implementors are expected to provide the
 * correct values for the following properties: %GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
 * %GTK_ACCESSIBLE_PROPERTY_VALUE_MIN and %GTK_ACCESSIBLE_PROPERTY_VALUE_NOW.
 */

#include "config.h"

#include "gtkaccessiblerange.h"

G_DEFINE_INTERFACE (GtkAccessibleRange, gtk_accessible_range, GTK_TYPE_ACCESSIBLE)

static double
gtk_accessible_range_default_get_minimum_increment (GtkAccessibleRange *accessible_range)
{
  return 0.0;
}

static void
gtk_accessible_range_default_set_current_value (GtkAccessibleRange *accessible_range, double value)
{
  /* By default, we do nothing */
}

static void
gtk_accessible_range_default_init (GtkAccessibleRangeInterface *iface)
{
  iface->get_minimum_increment = gtk_accessible_range_default_get_minimum_increment;
  iface->set_current_value = gtk_accessible_range_default_set_current_value;
}

 /*
 * gtk_accessible_range_get_minimum_increment:
 * @self: a `GtkAccessibleRange`
 *
 * Returns the minimum increment which this `GtkAccessibleRange` supports.
 *
 * Returns: the minimum increment, or 0.0 if not overridden
 */
double
gtk_accessible_range_get_minimum_increment (GtkAccessibleRange *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_RANGE (self), .0);

  return GTK_ACCESSIBLE_RANGE_GET_IFACE (self)->get_minimum_increment (self);
}

 /*
 * gtk_accessible_range_set_current_value:
 * @self: a `GtkAccessibleRange`
 *
 * Sets the current value of this `GtkAccessibleRange`.
 * Note that for some widgets implementing this interface, setting a value
 * through the accessibility API makes no sense, so calling this function may
 * in some cases do nothing.
 */
void
gtk_accessible_range_set_current_value (GtkAccessibleRange *self, double value)
{
  g_return_if_fail (GTK_IS_ACCESSIBLE_RANGE (self));

  GtkAccessibleRangeInterface *iface = GTK_ACCESSIBLE_RANGE_GET_IFACE (self);
  iface->set_current_value (self, value);
}