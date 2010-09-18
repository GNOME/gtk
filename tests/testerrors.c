/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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


#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include "x11/gdkx.h"

gint
main (gint argc, gchar *argv[])
{
  Display *d;
  int dummy;
  int error;

  gtk_init (&argc, &argv);

  d = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  /* verify that we can catch errors */
  gdk_error_trap_push ();
  XListProperties (d, 0, &dummy);
  error = gdk_error_trap_pop ();
  g_assert (error == BadWindow);

  gdk_error_trap_push ();
  XSetCloseDownMode (d, 12345);
  XSetCloseDownMode (d, DestroyAll);
  error = gdk_error_trap_pop ();
  g_assert (error == BadValue);

  /* try the same without sync */
  gdk_error_trap_push ();
  XListProperties (d, 0, &dummy);
  gdk_error_trap_pop_ignored ();

  gdk_error_trap_push ();
  XSetCloseDownMode (d, 12345);
  XSetCloseDownMode (d, DestroyAll);
  gdk_error_trap_pop_ignored ();

  XSync (d, TRUE);

  return 0;
}

