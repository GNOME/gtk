/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include "gdk.h"
#include "gdkprivate.h"


GdkAtom
gdk_atom_intern (const gchar *atom_name,
		 gint         only_if_exists)
{
  return XInternAtom (gdk_display, atom_name, only_if_exists);
}

gchar *
gdk_atom_name (GdkAtom atom)
{
  gchar *t;
  gchar *name;

  /* If this atom doesn't exist, we'll die with an X error unless
     we take precautions */

  gdk_error_warnings = 0;
  gdk_error_code = 0;
  t = XGetAtomName (gdk_display, atom);
  gdk_error_warnings = 1;

  if (gdk_error_code == -1)
    {
      return NULL;
    }
  else
    {
      name = g_strdup (t);
      XFree (t);
      
      return name;
    }
}

gint
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

  if (window)
    {
      GdkWindowPrivate *private;

      private = (GdkWindowPrivate*) window;
      if (private->destroyed)
	return FALSE;

      xdisplay = private->xdisplay;
      xwindow = private->xwindow;
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = gdk_root_window;
    }

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

  if (ret_prop_type != type)
    {
      gchar *rn, *pn;
      XFree (ret_data);
      rn = gdk_atom_name(ret_prop_type);
      pn = gdk_atom_name(type);
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
	  ret_length = 2 * ret_nitems;
	  break;
	case 32:
	  ret_length = 4 * ret_nitems;
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
gdk_property_change (GdkWindow   *window,
		     GdkAtom      property,
		     GdkAtom      type,
		     gint         format,
		     GdkPropMode  mode,
		     guchar      *data,
		     gint         nelements)
{
  Display *xdisplay;
  Window xwindow;

  if (window)
    {
      GdkWindowPrivate *private;

      private = (GdkWindowPrivate*) window;
      if (private->destroyed)
	return;

      xdisplay = private->xdisplay;
      xwindow = private->xwindow;
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = gdk_root_window;
    }

  XChangeProperty (xdisplay, xwindow, property, type,
		   format, mode, data, nelements);
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  Display *xdisplay;
  Window xwindow;

  if (window)
    {
      GdkWindowPrivate *private;

      private = (GdkWindowPrivate*) window;
      if (private->destroyed)
	return;

      xdisplay = private->xdisplay;
      xwindow = private->xwindow;
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = gdk_root_window;
    }

  XDeleteProperty (xdisplay, xwindow, property);
}
