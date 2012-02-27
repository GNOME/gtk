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

#include "gtkhbbox.h"
#include "gtkorientable.h"
#include "gtkintl.h"


/**
 * SECTION:gtkhbbox
 * @Short_description: A container for arranging buttons horizontally
 * @Title: GtkHButtonBox
 * @See_also: #GtkBox, #GtkButtonBox, #GtkVButtonBox
 *
 * A button box should be used to provide a consistent layout of buttons
 * throughout your application. The layout/spacing can be altered by the
 * programmer, or if desired, by the user to alter the 'feel' of a
 * program to a small degree.
 *
 * A #GtkHButtonBox is created with gtk_hbutton_box_new(). Buttons are
 * packed into a button box the same way widgets are added to any other
 * container, using gtk_container_add(). You can also use
 * gtk_box_pack_start() or gtk_box_pack_end(), but for button boxes both
 * these functions work just like gtk_container_add(), ie., they pack the
 * button in a way that depends on the current layout style and on
 * whether the button has had gtk_button_box_set_child_secondary() called
 * on it.
 *
 * The spacing between buttons can be set with gtk_box_set_spacing(). The
 * arrangement and layout of the buttons can be changed with
 * gtk_button_box_set_layout().
 *
 * GtkHButtonBox has been deprecated, use #GtkButtonBox instead.
 */


G_DEFINE_TYPE (GtkHButtonBox, gtk_hbutton_box, GTK_TYPE_BUTTON_BOX)

static void
gtk_hbutton_box_class_init (GtkHButtonBoxClass *class)
{
}

static void
gtk_hbutton_box_init (GtkHButtonBox *hbutton_box)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (hbutton_box),
                                  GTK_ORIENTATION_HORIZONTAL);
}

/**
 * gtk_hbutton_box_new:
 *
 * Creates a new horizontal button box.
 *
 * Returns: a new button box #GtkWidget.
 *
 * Deprecated: 3.2: Use gtk_button_box_new() with %GTK_ORIENTATION_HORIZONTAL instead
 */
GtkWidget *
gtk_hbutton_box_new (void)
{
  return g_object_new (GTK_TYPE_HBUTTON_BOX, NULL);
}

