/* gtkaccessiblerange.h: Accessible range interface
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib-object.h>
#include <gtk/gtkaccessible.h>

G_BEGIN_DECLS
#define GTK_TYPE_ACCESSIBLE_RANGE (gtk_accessible_range_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkAccessibleRange, gtk_accessible_range, GTK, ACCESSIBLE_RANGE, GtkAccessible)

struct _GtkAccessibleRangeInterface
{
  GTypeInterface g_iface;

  double (* get_minimum_increment) (GtkAccessibleRange *self);
  void (* set_current_value) (GtkAccessibleRange *self, double value);
};

GDK_AVAILABLE_IN_ALL
double gtk_accessible_range_get_minimum_increment (GtkAccessibleRange *self);

GDK_AVAILABLE_IN_ALL
void gtk_accessible_range_set_current_value (GtkAccessibleRange *self, double value);

G_END_DECLS