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

#include "gdk.h"          /* For gdk_error_trap_push/pop() */
#include "gdkx.h"
#include "gdkproperty.h"
#include "gdkprivate.h"

static GPtrArray *virtual_atom_array;
static GHashTable *virtual_atom_hash;
static GHashTable *atom_to_virtual;
static GHashTable *atom_from_virtual;

static gchar *XAtomsStrings[] = {
  /* These are all the standard predefined X atoms */
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
  "MAX_SPACE",  "END_SPACE",
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
  "WM_TRANSIENT_FOR",
  /* Below here, these our our additions. Increment N_CUSTOM_PREDEFINED
   * if you add any.
   */
  "CLIPBOARD"			/* = 69 */
};

#define N_CUSTOM_PREDEFINED 1

#define ATOM_TO_INDEX(atom) (GPOINTER_TO_UINT(atom))
#define INDEX_TO_ATOM(atom) ((GdkAtom)GUINT_TO_POINTER(atom))

static void
insert_atom_pair (GdkAtom     virtual_atom,
		  Atom        xatom)
{
  if (!atom_from_virtual)
    {
      atom_from_virtual = g_hash_table_new (g_direct_hash, NULL);
      atom_to_virtual = g_hash_table_new (g_direct_hash, NULL);
    }
  
  g_hash_table_insert (atom_from_virtual, 
		       GDK_ATOM_TO_POINTER (virtual_atom), GUINT_TO_POINTER (xatom));
  g_hash_table_insert (atom_to_virtual,
		       GUINT_TO_POINTER (xatom), GDK_ATOM_TO_POINTER (virtual_atom));
}

/**
 * gdk_x11_atom_to_xatom:
 * @atom: A #GdkAtom 
 * 
 * Converts from a #GdkAtom to the X atom for the default GDK display
 * with the same string value.
 * 
 * Return value: the X atom corresponding to @atom.
 **/
Atom
gdk_x11_atom_to_xatom (GdkAtom atom)
{
  Atom xatom = None;
  
  if (ATOM_TO_INDEX (atom) < G_N_ELEMENTS (XAtomsStrings) - N_CUSTOM_PREDEFINED)
    return ATOM_TO_INDEX (atom);
  
  if (atom_from_virtual)
    xatom = GPOINTER_TO_UINT (g_hash_table_lookup (atom_from_virtual,
						   GDK_ATOM_TO_POINTER (atom)));
  if (!xatom)
    {
      char *name;
      
      g_return_val_if_fail (ATOM_TO_INDEX (atom) < virtual_atom_array->len, None);

      name = g_ptr_array_index (virtual_atom_array, ATOM_TO_INDEX (atom));
      
      xatom = XInternAtom (gdk_display, name, FALSE);
      insert_atom_pair (atom, xatom);
    }

  return xatom;
}

/**
 * gdk_x11_xatom_to_atom:
 * @xatom: an X atom for the default GDK display
 * 
 * Converts from an X atom for the default display to the corresponding
 * #GdkAtom.
 * 
 * Return value: the corresponding #GdkAtom.
 **/
GdkAtom
gdk_x11_xatom_to_atom (Atom xatom)
{
  GdkAtom virtual_atom = GDK_NONE;
  
  if (xatom < G_N_ELEMENTS (XAtomsStrings) - N_CUSTOM_PREDEFINED)
    return INDEX_TO_ATOM (xatom);
  
  if (atom_to_virtual)
    virtual_atom = GDK_POINTER_TO_ATOM (g_hash_table_lookup (atom_to_virtual,
							     GUINT_TO_POINTER (xatom)));
  
  if (!virtual_atom)
    {
      /* If this atom doesn't exist, we'll die with an X error unless
       * we take precautions
       */
      char *name;
      gdk_error_trap_push ();
      name = XGetAtomName (gdk_display, xatom);
      if (gdk_error_trap_pop ())
	{
	  g_warning (G_STRLOC " invalid X atom: %ld", xatom);
	}
      else
	{
	  virtual_atom = gdk_atom_intern (name, FALSE);
	  XFree (name);
	  
	  insert_atom_pair (virtual_atom, xatom);
	}
    }

  return virtual_atom;
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
  
  result = GDK_POINTER_TO_ATOM (g_hash_table_lookup (virtual_atom_hash, atom_name));
  if (!result)
    {
      result = INDEX_TO_ATOM (virtual_atom_array->len);
      
      g_ptr_array_add (virtual_atom_array, g_strdup (atom_name));
      g_hash_table_insert (virtual_atom_hash, 
			   g_ptr_array_index (virtual_atom_array,
					      ATOM_TO_INDEX (result)),
			   GDK_ATOM_TO_POINTER (result));
    }

  return result;
}

static G_CONST_RETURN char *
get_atom_name (GdkAtom atom)
{
  virtual_atom_check_init ();

  if (ATOM_TO_INDEX (atom) < virtual_atom_array->len)
    return g_ptr_array_index (virtual_atom_array, ATOM_TO_INDEX (atom));
  else
    return NULL;
}

gchar *
gdk_atom_name (GdkAtom atom)
{
  return g_strdup (get_atom_name (atom));
}

/**
 * gdk_x11_get_xatom_by_name:
 * @atom_name: a string
 * 
 * Returns the X atom for GDK's default display corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than <function>XInternAtom()</function>, which is a round trip to 
 * the server each time.
 * 
 * Return value: a X atom for GDK's default display.
 **/
Atom
gdk_x11_get_xatom_by_name (const gchar *atom_name)
{
  return gdk_x11_atom_to_xatom (gdk_atom_intern (atom_name, FALSE));
}

/**
 * gdk_x11_get_xatom_name:
 * @xatom: an X atom for GDK's default display
 * 
 * Returns the name of an X atom for GDK's default display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * <function>XAtomName()</function> and gdk_atom_name(), the result 
 * doesn't need to be freed. Also, this function will never return %NULL, 
 * even if @xatom is invalid.
 * 
 * Return value: name of the X atom; this string is owned by GTK+,
 *   so it shouldn't be modifed or freed. 
 **/
G_CONST_RETURN gchar *
gdk_x11_get_xatom_name (Atom xatom)
{
  return get_atom_name (gdk_x11_xatom_to_atom (xatom));
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
  Atom xproperty;
  Atom xtype;
  Atom ret_prop_type;
  gint ret_format;
  gulong ret_nitems;
  gulong ret_bytes_after;
  gulong ret_length;
  guchar *ret_data;

  g_return_val_if_fail (!window || GDK_IS_WINDOW (window), FALSE);

  xproperty = gdk_x11_atom_to_xatom (property);
  xtype = gdk_x11_atom_to_xatom (type);

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return FALSE;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = _gdk_root_window;
    }

  ret_data = NULL;
  XGetWindowProperty (xdisplay, xwindow, xproperty,
		      offset, (length + 3) / 4, pdelete,
		      xtype, &ret_prop_type, &ret_format,
		      &ret_nitems, &ret_bytes_after,
		      &ret_data);

  if ((ret_prop_type == None) && (ret_format == 0)) {
    return FALSE;
  }

  if (actual_property_type)
    *actual_property_type = gdk_x11_xatom_to_atom (ret_prop_type);
  if (actual_format_type)
    *actual_format_type = ret_format;

  if ((type != AnyPropertyType) && (ret_prop_type != xtype))
    {
      XFree (ret_data);
      
      g_warning("Couldn't match property type %s to %s\n", 
		gdk_x11_get_xatom_name (ret_prop_type),
		gdk_x11_get_xatom_name (xtype));
      return FALSE;
    }

  /* FIXME: ignoring bytes_after could have very bad effects */

  if (data)
    {
      if (ret_prop_type == XA_ATOM ||
	  ret_prop_type == gdk_x11_get_xatom_by_name ("ATOM_PAIR"))
	{
	  /*
	   * data is an array of X atom, we need to convert it
	   * to an array of GDK Atoms
	   */
	  gint i;
	  GdkAtom *ret_atoms = g_new (GdkAtom, ret_nitems);
	  Atom *xatoms = (Atom *)ret_data;

	  *data = (guchar *)ret_atoms;

	  for (i = 0; i < ret_nitems; i++)
	    ret_atoms[i] = gdk_x11_xatom_to_atom (xatoms[i]);

	  if (actual_length)
	    *actual_length = ret_nitems * sizeof (GdkAtom);
	}
      else
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
  Atom xproperty;
  Atom xtype;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  xproperty = gdk_x11_atom_to_xatom (property);
  xtype = gdk_x11_atom_to_xatom (type);

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = _gdk_root_window;
    }

  if (xtype == XA_ATOM || xtype == gdk_x11_get_xatom_by_name ("ATOM_PAIR"))
    {
      /*
       * data is an array of GdkAtom, we need to convert it
       * to an array of X Atoms
       */
      gint i;
      GdkAtom *atoms = (GdkAtom*) data;
      Atom *xatoms;

      xatoms = g_new (Atom, nelements);
      for (i = 0; i < nelements; i++)
	xatoms[i] = gdk_x11_atom_to_xatom (atoms[i]);

      XChangeProperty (xdisplay, xwindow, xproperty, xtype,
                      format, mode, (guchar *)xatoms, nelements);
      g_free (xatoms);
    }
  else
    XChangeProperty (xdisplay, xwindow, xproperty, xtype,
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
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = _gdk_root_window;
    }

  XDeleteProperty (xdisplay, xwindow, gdk_x11_atom_to_xatom (property));
}
