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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "gdkdisplay-broadway.h"
#include "gdkinternals.h"
#include "gdkprivate-broadway.h"
#include "gdkintl.h"
#include "gdkdeviceprivate.h"

#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

typedef struct _GdkPredicate        GdkPredicate;
typedef struct _GdkGlobalErrorTrap  GdkGlobalErrorTrap;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

/* Private variable declarations
 */
const GOptionEntry _gdk_windowing_args[] = {
  { NULL }
};

void
_gdk_windowing_init (void)
{
  _gdk_x11_initialize_locale ();

  _gdk_selection_property = gdk_atom_intern_static_string ("GDK_SELECTION");
}

GdkGrabStatus
_gdk_windowing_device_grab (GdkDevice    *device,
                            GdkWindow    *window,
                            GdkWindow    *native,
                            gboolean      owner_events,
                            GdkEventMask  event_mask,
                            GdkWindow    *confine_to,
                            GdkCursor    *cursor,
                            guint32       time)
{
  return GDK_GRAB_NOT_VIEWABLE;
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					 const gchar *sm_client_id)
{
  if (display->closed)
    return;
 }

/* Close all open displays
 */
void
_gdk_windowing_exit (void)
{
}

void
gdk_error_trap_push (void)
{
}

void
gdk_error_trap_pop_ignored (void)
{
}

gint
gdk_error_trap_pop (void)
{
  return 0;
}

gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}

void
_gdk_windowing_event_data_copy (const GdkEvent *src,
                                GdkEvent       *dst)
{
}

void
_gdk_windowing_event_data_free (GdkEvent *event)
{
}
