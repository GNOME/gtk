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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "gdkx.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"


GdkAtom
gdk_atom_intern (const gchar *atom_name, gboolean only_if_exists)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_display_atom instead\n"));
  return gdk_display_atom (gdk_get_default_display(), atom_name, only_if_exists);

}

gchar*
gdk_atom_name (GdkAtom atom)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_display_atom_name instead\n"));
  return gdk_display_atom_name (gdk_get_default_display(), atom);	

}

gboolean
gdk_property_get (GdkWindow   *window,
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
  Display *xdisplay;
  Window xwindow;
  Atom ret_prop_type;
  gint ret_format;
  gulong ret_nitems;
  gulong ret_bytes_after;
  gulong ret_length;
  guchar *ret_data;

  g_return_val_if_fail (!window || GDK_IS_WINDOW (window), FALSE);

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return FALSE;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
    }
  else
    {
        g_warning("gdk_property_get no GdkWindow defined\n");
	return FALSE;
    }

  ret_data = NULL;
  XGetWindowProperty (xdisplay, xwindow, property,
		      offset, (length + 3) / 4, pdelete,
		      type, &ret_prop_type, &ret_format,
		      &ret_nitems, &ret_bytes_after,
		      &ret_data);

  if ((ret_prop_type == None) && (ret_format == 0)) {
    return FALSE;
  }

  if (actual_property_type)
    *actual_property_type = ret_prop_type;
  if (actual_format_type)
    *actual_format_type = ret_format;

  if ((type != AnyPropertyType) && (ret_prop_type != type))
    {
      gchar *rn, *pn;

      XFree (ret_data);
      rn = gdk_display_atom_name (GDK_WINDOW_DISPLAY(window), ret_prop_type);

      pn = gdk_display_atom_name (GDK_WINDOW_DISPLAY(window), type);

      g_warning("Couldn't match property type %s to %s\n", rn, pn);
      g_free(rn); g_free(pn);
      return FALSE;
    }

  /* FIXME: ignoring bytes_after could have very bad effects */

  if (data)
    {
      switch (ret_format)
	{
	case 8:
	  ret_length = ret_nitems;
	  break;
	case 16:
	  ret_length = sizeof(short) * ret_nitems;
	  break;
	case 32:
	  ret_length = sizeof(long) * ret_nitems;
	  break;
	default:
	  g_warning ("unknown property return format: %d", ret_format);
	  XFree (ret_data);
	  return FALSE;
	}

      *data = g_new (guchar, ret_length);
      memcpy (*data, ret_data, ret_length);
      if (actual_length)
	*actual_length = ret_length;
    }

  XFree (ret_data);

  return TRUE;
}

void
gdk_property_change (GdkWindow    *window,
		     GdkAtom       property,
		     GdkAtom       type,
		     gint          format,
		     GdkPropMode   mode,
		     const guchar *data,
		     gint          nelements)
{
  Display *xdisplay;
  Window xwindow;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
    }
  else
    {
      g_warning("gdk_property_change no GdkWindow defined\n");
      return;
    }

  XChangeProperty (xdisplay, xwindow, property, type,
		   format, mode, (guchar *)data, nelements);
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  Display *xdisplay;
  Window xwindow;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
      XDeleteProperty (xdisplay, xwindow, property);

    }
/*  removed for multihead support,
 *  use root_window from a GdkScreen to delete a prop
 *  from the specific root window.
    else
    {
      xdisplay = gdk_display;
      xwindow = gdk_root_window;
    }
*/
 }
