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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkproperty.h"

#include "gdkmain.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay-broadway.h"
#include "gdkscreen-broadway.h"
#include "gdkselection.h"

#include <string.h>

gboolean
_gdk_broadway_window_get_property (GdkWindow   *window,
				   GdkAtom      property,
				   GdkAtom      type,
				   gulong       offset,
				   gulong       length,
				   gint         pdelete,
				   GdkAtom     *actual_property_type,
				   gint        *actual_format_type,
				   gint        *actual_length,
				   guchar     **data)
{
  return FALSE;
}

void
_gdk_broadway_window_change_property (GdkWindow    *window,
				      GdkAtom       property,
				      GdkAtom       type,
				      gint          format,
				      GdkPropMode   mode,
				      const guchar *data,
				      gint          nelements)
{
  g_return_if_fail (!window || GDK_WINDOW_IS_BROADWAY (window));
}

void
_gdk_broadway_window_delete_property (GdkWindow *window,
				      GdkAtom    property)
{
  g_return_if_fail (!window || GDK_WINDOW_IS_BROADWAY (window));
}
