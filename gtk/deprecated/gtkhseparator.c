/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkhseparator.h"
#include "gtkorientable.h"


/**
 * SECTION:gtkhseparator
 * @Short_description: A horizontal separator
 * @Title: GtkHSeparator
 * @See_also: #GtkSeparator
 *
 * The #GtkHSeparator widget is a horizontal separator, used to group the
 * widgets within a window. It displays a horizontal line with a shadow to
 * make it appear sunken into the interface.
 *
 * <note>
 * The #GtkHSeparator widget is not used as a separator within menus.
 * To create a separator in a menu create an empty #GtkSeparatorMenuItem
 * widget using gtk_separator_menu_item_new() and add it to the menu with
 * gtk_menu_shell_append().
 * </note>
 *
 * GtkHSeparator has been deprecated, use #GtkSeparator instead.
 */


G_DEFINE_TYPE (GtkHSeparator, gtk_hseparator, GTK_TYPE_SEPARATOR)

static void
gtk_hseparator_class_init (GtkHSeparatorClass *class)
{
}

static void
gtk_hseparator_init (GtkHSeparator *hseparator)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (hseparator),
                                  GTK_ORIENTATION_HORIZONTAL);
}

/**
 * gtk_hseparator_new:
 *
 * Creates a new #GtkHSeparator.
 *
 * Returns: a new #GtkHSeparator.
 *
 * Deprecated: 3.2: Use gtk_separator_new() with %GTK_ORIENTATION_HORIZONTAL instead
 */
GtkWidget *
gtk_hseparator_new (void)
{
  return g_object_new (GTK_TYPE_HSEPARATOR, NULL);
}
