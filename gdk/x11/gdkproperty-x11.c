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
#include "gdkselection.h"	/* only from predefined atom */

static GPtrArray *virtual_atom_array;
static GHashTable *virtual_atom_hash;

static gchar *XAtomsStrings[] = {
  "NONE",
  "PRIMARY",
  "SECONDARY",
  "ARC",
  "ATOM",
  "BITMAP",
  "CARDINAL",
  "COLORMAP",
  "CURSOR",
  "CUT_BUFFER0",
  "CUT_BUFFER1",
  "CUT_BUFFER2",
  "CUT_BUFFER3",
  "CUT_BUFFER4",
  "CUT_BUFFER5",
  "CUT_BUFFER6",
  "CUT_BUFFER7",
  "DRAWABLE",
  "FONT",
  "INTEGER",
  "PIXMAP",
  "POINT",
  "RECTANGLE",
  "RESOURCE_MANAGER",
  "RGB_COLOR_MAP",
  "RGB_BEST_MAP",
  "RGB_BLUE_MAP",
  "RGB_DEFAULT_MAP",
  "RGB_GRAY_MAP",
  "RGB_GREEN_MAP",
  "RGB_RED_MAP",
  "STRING",
  "VISUALID",
  "WINDOW",
  "WM_COMMAND",
  "WM_HINTS",
  "WM_CLIENT_MACHINE",
  "WM_ICON_NAME",
  "WM_ICON_SIZE",
  "WM_NAME",
  "WM_NORMAL_HINTS",
  "WM_SIZE_HINTS",
  "WM_ZOOM_HINTS",
  "MIN_SPACE",
  "NORM_SPACE",
  "MAX_SPACE",
  "END_SPACE",
  "SUPERSCRIPT_X",
  "SUPERSCRIPT_Y",
  "SUBSCRIPT_X",
  "SUBSCRIPT_Y",
  "UNDERLINE_POSITION",
  "UNDERLINE_THICKNESS",
  "STRIKEOUT_ASCENT",
  "STRIKEOUT_DESCENT",
  "ITALIC_ANGLE",
  "X_HEIGHT",
  "QUAD_WIDTH",
  "WEIGHT",
  "POINT_SIZE",
  "RESOLUTION",
  "COPYRIGHT",
  "NOTICE",
  "FONT_NAME",
  "FAMILY_NAME",
  "FULL_NAME",
  "CAP_HEIGHT",
  "WM_CLASS",
  "WM_TRANSIENT_FOR"
};

void
insert_atom_pair (GdkDisplay *display,
		  GdkAtom     virtual_atom,
		  GdkAtom     xatom)
{
  GdkDisplayImplX11 *display_impl;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  display_impl = GDK_DISPLAY_IMPL_X11 (display);

  if (!display_impl->atom_from_virtual)
    {
      display_impl->atom_from_virtual = g_hash_table_new (g_direct_hash, NULL);
      display_impl->atom_to_virtual = g_hash_table_new (g_direct_hash, NULL);
    }
  
  g_hash_table_insert (display_impl->atom_from_virtual, 
		       GUINT_TO_POINTER (virtual_atom), GUINT_TO_POINTER (xatom));
  g_hash_table_insert (display_impl->atom_to_virtual,
		       GUINT_TO_POINTER (xatom), GUINT_TO_POINTER (virtual_atom));
}

GdkAtom
gdk_x11_get_real_atom (GdkDisplay *display, 
		       GdkAtom     virtual_atom)
{
  GdkDisplayImplX11 *display_impl;
  GdkAtom xatom = GDK_NONE;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), GDK_NONE);

  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  
  if (virtual_atom < G_N_ELEMENTS (XAtomsStrings))
    return virtual_atom;
  
  if (display_impl->atom_from_virtual)
    xatom = GPOINTER_TO_UINT (g_hash_table_lookup (display_impl->atom_from_virtual,
						   GUINT_TO_POINTER (virtual_atom)));
  if (!xatom)
    {
      char *name;
      
      g_return_val_if_fail (virtual_atom < virtual_atom_array->len, GDK_NONE);

      name = g_ptr_array_index (virtual_atom_array, virtual_atom);
      
      xatom = XInternAtom (GDK_DISPLAY_XDISPLAY (display), name, FALSE);
      insert_atom_pair (display, virtual_atom, xatom);
    }

  return xatom;
}

GdkAtom
gdk_x11_get_virtual_atom (GdkDisplay *display, 
			  GdkAtom     xatom)
{
  GdkDisplayImplX11 *display_impl;
  GdkAtom virtual_atom = GDK_NONE;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), GDK_NONE);

  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  
  if (xatom < G_N_ELEMENTS (XAtomsStrings))
    return xatom;
  
  if (display_impl->atom_to_virtual)
    virtual_atom = GPOINTER_TO_UINT (g_hash_table_lookup (display_impl->atom_to_virtual,
							  GUINT_TO_POINTER (xatom)));
  
  if (!virtual_atom)
    {
      char *name = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
      virtual_atom = gdk_atom_intern (name, FALSE);
      XFree (name);

      insert_atom_pair (display, virtual_atom, xatom);
    }

  return virtual_atom;
}

GdkAtom
gdk_x11_get_real_atom_by_name (GdkDisplay  *display,
			       const gchar *atom_name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), GDK_NONE);
  
  return gdk_x11_get_real_atom (display,
				gdk_atom_intern (atom_name, FALSE));
}

gchar *
gdk_x11_get_real_atom_name (GdkDisplay *display,
			    GdkAtom     xatom)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return gdk_atom_name (gdk_x11_get_virtual_atom (display, xatom));
}

static void
virtual_atom_check_init (void)
{
  if (!virtual_atom_hash)
    {
      gint i;
      
      virtual_atom_hash = g_hash_table_new (g_str_hash, g_str_equal);
      virtual_atom_array = g_ptr_array_new ();
      
      for (i = 0; i < G_N_ELEMENTS (XAtomsStrings); i++)
	{
	  g_ptr_array_add (virtual_atom_array, XAtomsStrings[i]);
	  g_hash_table_insert (virtual_atom_hash, XAtomsStrings[i],
			       GUINT_TO_POINTER (i));
	}
    }
}

GdkAtom
gdk_atom_intern (const gchar *atom_name, 
		 gboolean     only_if_exists)
{
  GdkAtom result;

  virtual_atom_check_init ();
  
  result = GPOINTER_TO_UINT (g_hash_table_lookup (virtual_atom_hash, atom_name));
  if (!result)
    {
      result = virtual_atom_array->len;
      g_ptr_array_add (virtual_atom_array, g_strdup (atom_name));
      g_hash_table_insert (virtual_atom_hash, g_ptr_array_index (virtual_atom_array, result),
			   GUINT_TO_POINTER (result));
    }

  return result;
}

gchar *
gdk_atom_name (GdkAtom atom)
{
  virtual_atom_check_init ();

  if (atom < virtual_atom_array->len)
    return g_strdup (g_ptr_array_index (virtual_atom_array, atom));
  else
    return NULL;
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
  GdkDisplay *display;
  Atom ret_prop_type;
  gint ret_format;
  gulong ret_nitems;
  gulong ret_bytes_after;
  gulong ret_length;
  guchar *ret_data;
  Atom xproperty;
  Atom xtype;

  g_return_val_if_fail (!window || GDK_IS_WINDOW (window), FALSE);

  if (!window)
    {
      GdkScreen *screen = gdk_get_default_screen ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_get(): window is NULL\n"));
    }

  display = gdk_drawable_get_display (window);
  xproperty = gdk_x11_get_real_atom (display, property);
  xtype = gdk_x11_get_real_atom (display, type);

  ret_data = NULL;
  
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
		      GDK_WINDOW_XWINDOW (window), xproperty,
		      offset, (length + 3) / 4, pdelete,
		      xtype, &ret_prop_type, &ret_format,
		      &ret_nitems, &ret_bytes_after, &ret_data);

  if ((ret_prop_type == None) && (ret_format == 0))
    {
      return FALSE;
    }

  if (actual_property_type)
    *actual_property_type = gdk_x11_get_virtual_atom (display, ret_prop_type);
  if (actual_format_type)
    *actual_format_type = ret_format;

  if ((xtype != AnyPropertyType) && (ret_prop_type != xtype))
    {
      gchar *rn, *pn;

      XFree (ret_data);
      rn = gdk_x11_get_real_atom_name (GDK_WINDOW_DISPLAY (window),
				       ret_prop_type);
      pn = gdk_x11_get_real_atom_name (GDK_WINDOW_DISPLAY (window), type);

      g_warning ("Couldn't match property type %s to %s\n", rn, pn);
      g_free (rn);
      g_free (pn);
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
  GdkDisplay *display;
  Atom xproperty;
  Atom xtype;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (!window)
    {
      GdkScreen *screen = gdk_get_default_screen ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_delete(): window is NULL\n"));
    }

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);
  
  xproperty = gdk_x11_get_real_atom (display, property);
  xtype = gdk_x11_get_real_atom (display, type);
  
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_WINDOW_XWINDOW (window),
		   xproperty, xtype,
		   format, mode, (guchar *) data, nelements);
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  GdkDisplay *display;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (!window)
    {
      GdkScreen *screen = gdk_get_default_screen ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_delete(): window is NULL\n"));
    }

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);

  XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_WINDOW_XWINDOW (window),
		   gdk_x11_get_real_atom (display, property));
}
