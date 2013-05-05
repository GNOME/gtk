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

#include "gtkorientable.h"

#include "gtkvpaned.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * SECTION:gtkvpaned
 * @Short_description: A container with two panes arranged vertically
 * @Title: GtkVPaned
 *
 * The VPaned widget is a container widget with two
 * children arranged vertically. The division between
 * the two panes is adjustable by the user by dragging
 * a handle. See #GtkPaned for details.
 *
 * GtkVPaned has been deprecated, use #GtkPaned instead.
 */

G_DEFINE_TYPE (GtkVPaned, gtk_vpaned, GTK_TYPE_PANED)

static void
gtk_vpaned_class_init (GtkVPanedClass *class)
{
}

static void
gtk_vpaned_init (GtkVPaned *vpaned)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (vpaned),
                                  GTK_ORIENTATION_VERTICAL);
}

/**
 * gtk_vpaned_new:
 *
 * Create a new #GtkVPaned
 *
 * Returns: the new #GtkVPaned
 *
 * Deprecated: 3.2: Use gtk_paned_new() with %GTK_ORIENTATION_VERTICAL instead
 */
GtkWidget *
gtk_vpaned_new (void)
{
  return g_object_new (GTK_TYPE_VPANED, NULL);
}
