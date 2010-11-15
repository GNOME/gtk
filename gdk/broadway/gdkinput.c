/* GDK - The GIMP Drawing Kit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkscreen-broadway.h"
#include "gdkdisplay-broadway.h"
#include "gdkwindow.h"

#include <stdlib.h>


/* Addition used for extension_events mask */
#define GDK_ALL_DEVICES_MASK (1<<30)

struct _GdkInputWindow
{
  GList *windows; /* GdkWindow:s with extension_events set */

  /* gdk window */
  GdkWindow *impl_window; /* an impl window */
};


GList *
gdk_devices_list (void)
{
  return gdk_display_list_devices (gdk_display_get_default ());
}

void
gdk_input_set_extension_events (GdkWindow        *window,
                                gint              mask,
				GdkExtensionMode  mode)
{
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
}

void
_gdk_input_check_extension_events (GdkDevice *device)
{
}
