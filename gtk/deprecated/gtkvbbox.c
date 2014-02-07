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

#include "gtkvbbox.h"
#include "gtkorientable.h"
#include "gtkintl.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * SECTION:gtkvbbox
 * @Short_description: A container for arranging buttons vertically
 * @Title: GtkVButtonBox
 * @See_also: #GtkBox, #GtkButtonBox, #GtkHButtonBox
 *
 * A button box should be used to provide a consistent layout of buttons
 * throughout your application. The layout/spacing can be altered by the
 * programmer, or if desired, by the user to alter the “feel” of a
 * program to a small degree.
 *
 * A #GtkVButtonBox is created with gtk_vbutton_box_new(). Buttons are
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
 * GtkVButtonBox has been deprecated, use #GtkButtonBox instead.
 */

G_DEFINE_TYPE (GtkVButtonBox, gtk_vbutton_box, GTK_TYPE_BUTTON_BOX)

static void
gtk_vbutton_box_class_init (GtkVButtonBoxClass *class)
{
}

static void
gtk_vbutton_box_init (GtkVButtonBox *vbutton_box)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (vbutton_box),
                                  GTK_ORIENTATION_VERTICAL);
}

/**
 * gtk_vbutton_box_new:
 *
 * Creates a new vertical button box.
 *
 * Returns: a new button box #GtkWidget.
 *
 * Deprecated: 3.2: Use gtk_button_box_new() with %GTK_ORIENTATION_VERTICAL instead
 */
GtkWidget *
gtk_vbutton_box_new (void)
{
  return g_object_new (GTK_TYPE_VBUTTON_BOX, NULL);
}
