/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gtkdndprivate.h"

#include "gtkdragdestprivate.h"
#include "gtkimageprivate.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkpicture.h"
#include "gtkselectionprivate.h"
#include "gtksettingsprivate.h"
#include "gtkstylecontext.h"
#include "gtktooltipprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowgroup.h"
#include "gtkwindowprivate.h"
#include "gtknative.h"
#include "gtkdragiconprivate.h"

#include "gdk/gdkcontentformatsprivate.h"
#include "gdk/gdktextureprivate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


/**
 * SECTION:gtkdnd
 * @Short_description: Functions for controlling drag and drop handling
 * @Title: Drag and Drop
 *
 * GTK+ has a rich set of functions for doing inter-process communication
 * via the drag-and-drop metaphor.
 *
 * As well as the functions listed here, applications may need to use some
 * facilities provided for [Selections][gtk3-Selections]. Also, the Drag and
 * Drop API makes use of signals in the #GtkWidget class.
 */

/*
 * gtk_drag_dest_handle_event:
 * @toplevel: Toplevel widget that received the event
 * @event: the event to handle
 *
 * Called from widget event handling code on Drag events
 * for destinations. For drag-motion and drop-start events,
 * this function is only called if no event handler has
 * handled the event.
 */
void
gtk_drag_dest_handle_event (GtkWidget *toplevel,
                            GdkEvent  *event)
{
  GtkDropTarget *dest;
  GdkDrop *drop;
  GdkEventType event_type;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  event_type = gdk_event_get_event_type (event);
  drop = gdk_event_get_drop (event);

  /* Find the widget for the event */
  switch ((guint) event_type)
    {
    case GDK_DRAG_ENTER:
      break;
      
    case GDK_DRAG_LEAVE:
      dest = gtk_drop_get_current_dest (drop);
      if (dest)
        {
          gtk_drop_target_emit_drag_leave (dest, drop);
          gtk_drop_set_current_dest (drop, NULL);
        }
      break;

    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      gdk_drop_status (drop, 0);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
clear_current_dest (gpointer data, GObject *former_object)
{
  g_object_set_data (G_OBJECT (data), "current-dest", NULL);
}

void
gtk_drop_set_current_dest (GdkDrop       *drop,
                           GtkDropTarget *dest)
{
  GtkDropTarget *old_dest;

  old_dest = g_object_get_data (G_OBJECT (drop), "current-dest");

  if (old_dest)
    g_object_weak_unref (G_OBJECT (old_dest), clear_current_dest, drop);

  g_object_set_data (G_OBJECT (drop), "current-dest", dest);

  if (dest)
    g_object_weak_ref (G_OBJECT (dest), clear_current_dest, drop);
}

GtkDropTarget *
gtk_drop_get_current_dest (GdkDrop *drop)
{
  return g_object_get_data (G_OBJECT (drop), "current-dest");
}
