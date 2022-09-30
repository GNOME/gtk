/* gtkaccessiblerange.h: Accessible range interface
 *
 * SPDX-FileCopyrightText: 2022 Red Hat Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib-object.h>
#include <gtk/gtkaccessible.h>

G_BEGIN_DECLS
#define GTK_TYPE_ACCESSIBLE_RANGE (gtk_accessible_range_get_type())

GDK_AVAILABLE_IN_4_10
G_DECLARE_INTERFACE (GtkAccessibleRange, gtk_accessible_range, GTK, ACCESSIBLE_RANGE, GtkAccessible)

struct _GtkAccessibleRangeInterface
{
  GTypeInterface g_iface;

  /**
   * GtkAccessibleRangeInterface::get_minimum_increment:
   * @self: a `GtkAccessibleRange`
   *
   * Returns the minimum increment for this `GtkAccessibleRange`.
   * The default implementation returns 0.0, which indicates that a minimum
   * increment does not make sense for this implementation.
   * Returns: the minimum increment
   *
   * Since: 4.10
   */
  double (* get_minimum_increment) (GtkAccessibleRange *self);
  /**
   * GtkAccessibleRangeInterface::set_current_value:
   * @self: a `GtkAccessibleRange`
   * @value: the value to set
   *
   * Sets the current value of @self to @value.
   *
   * This operation should behave similarly as if the user performed the
   * action.
   *
   * Returns: true if the operation was performed, false otherwise
   *
   * Since: 4.10
   */
  gboolean (* set_current_value) (GtkAccessibleRange *self,
                                  double              value);
};

GDK_AVAILABLE_IN_4_10
double    gtk_accessible_range_get_minimum_increment  (GtkAccessibleRange *self);

GDK_AVAILABLE_IN_4_10
gboolean  gtk_accessible_range_set_current_value      (GtkAccessibleRange *self,
                                                       double              value);

G_END_DECLS
