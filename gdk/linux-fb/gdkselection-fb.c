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

#include <string.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate.h"
#include "gdkprivate-fb.h"


gint
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gint       send_event)
{
  return FALSE;
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  return NULL;
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  return 0;
}


void
gdk_selection_send_notify (guint32  requestor,
			   GdkAtom  selection,
			   GdkAtom  target,
			   GdkAtom  property,
			   guint32  time)
{
}

gint
gdk_text_property_to_text_list (GdkAtom encoding, gint format, 
				const guchar *text, gint length,
				gchar ***list)
{
  return 0;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);
}

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom *encoding, gint *format,
			     guchar **ctext, gint *length)
{
  return 0;
}

void gdk_free_compound_text (guchar *ctext)
{
}
