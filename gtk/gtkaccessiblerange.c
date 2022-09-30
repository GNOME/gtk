/* gtkaccessiblerange.c: Accessible range interface
 *
 * SPDX-FileCopyrightText: 2022 Red Hat Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * GtkAccessibleRange:
 *
 * This interface describes ranged controls, e.g. controls which have a single
 * value within an allowed range and that can optionally be changed by the user.
 *
 * This interface is expected to be implemented by controls using the following
 * roles:
 *
 * - `GTK_ACCESSIBLE_ROLE_METER`
 * - `GTK_ACCESSIBLE_ROLE_PROGRESS_BAR`
 * - `GTK_ACCESSIBLE_ROLE_SCROLLBAR`
 * - `GTK_ACCESSIBLE_ROLE_SLIDER`
 * - `GTK_ACCESSIBLE_ROLE_SPIN_BUTTON`
 *
 * If that is not the case, a warning will be issued at run time.
 *
 * In addition to this interface, its implementors are expected to provide the
 * correct values for the following properties:
 *
 * - `GTK_ACCESSIBLE_PROPERTY_VALUE_MAX`
 * - `GTK_ACCESSIBLE_PROPERTY_VALUE_MIN`
 * - `GTK_ACCESSIBLE_PROPERTY_VALUE_NOW`
 * - `GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT`
 *
 * For controls where a minimum increment makes no sense and which do not allow
 * setting the current value from the user, the default implementation of this
 * interface suffices.
 *
 * Since: 4.10
 */

#include "config.h"

#include "gtkaccessiblerange.h"

G_DEFINE_INTERFACE (GtkAccessibleRange, gtk_accessible_range, GTK_TYPE_ACCESSIBLE)

static double
gtk_accessible_range_default_get_minimum_increment (GtkAccessibleRange *accessible_range)
{
  return 0.0;
}

static gboolean
gtk_accessible_range_default_set_current_value (GtkAccessibleRange *accessible_range, double value)
{
  return FALSE;
}

static void
gtk_accessible_range_default_init (GtkAccessibleRangeInterface *iface)
{
  iface->get_minimum_increment = gtk_accessible_range_default_get_minimum_increment;
  iface->set_current_value = gtk_accessible_range_default_set_current_value;
}

/**
 * gtk_accessible_range_get_minimum_increment:
 * @self: a `GtkAccessibleRange`
 *
 * Returns the minimum increment which this `GtkAccessibleRange` supports.
 *
 * Returns: the minimum increment, or 0.0 if not overridden
 *
 * Since: 4.10
 */
double
gtk_accessible_range_get_minimum_increment (GtkAccessibleRange *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_RANGE (self), .0);

  return GTK_ACCESSIBLE_RANGE_GET_IFACE (self)->get_minimum_increment (self);
}

/**
 * gtk_accessible_range_set_current_value:
 * @self: a `GtkAccessibleRange`
 *
 * Sets the current value of this `GtkAccessibleRange` to the given value
 *
 * Note that for some widgets implementing this interface, setting a value
 * through the accessibility API makes no sense, so calling this function
 * may in some cases do nothing
 *
 * Returns: true if the call changed the value, and false otherwise
 *
 * Since: 4.10
 */
gboolean
gtk_accessible_range_set_current_value (GtkAccessibleRange *self, double value)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_RANGE (self), FALSE);

  GtkAccessibleRangeInterface *iface = GTK_ACCESSIBLE_RANGE_GET_IFACE (self);

  return iface->set_current_value (self, value);
}
