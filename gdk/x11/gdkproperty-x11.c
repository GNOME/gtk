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

#include "gdkinternals.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

static void
insert_atom_pair (GdkDisplay *display,
		  GdkAtom     virtual_atom,
		  Atom        xatom)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);  
  
  if (!display_x11->atom_from_virtual)
    {
      display_x11->atom_from_virtual = g_hash_table_new (g_direct_hash, NULL);
      display_x11->atom_to_virtual = g_hash_table_new (g_direct_hash, NULL);
    }
  
  g_hash_table_insert (display_x11->atom_from_virtual, (gpointer)virtual_atom, 
		       GUINT_TO_POINTER (xatom));
  g_hash_table_insert (display_x11->atom_to_virtual,
		       GUINT_TO_POINTER (xatom), (gpointer)virtual_atom);
}

static Atom
lookup_cached_xatom (GdkDisplay *display,
		     GdkAtom     atom)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->atom_from_virtual)
    return GPOINTER_TO_UINT (g_hash_table_lookup (display_x11->atom_from_virtual, atom));

  return None;
}

/**
 * gdk_x11_atom_to_xatom_for_display:
 * @display: (type GdkX11Display): A #GdkDisplay
 * @atom: A #GdkAtom, or %NULL
 *
 * Converts from a #GdkAtom to the X atom for a #GdkDisplay
 * with the same string value. The special value %NULL
 * is converted to %None.
 *
 * Returns: the X atom corresponding to @atom, or %None
 **/
Atom
gdk_x11_atom_to_xatom_for_display (GdkDisplay *display,
				   GdkAtom     atom)
{
  Atom xatom = None;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);

  if (atom == NULL)
    return None;

  if (gdk_display_is_closed (display))
    return None;

  xatom = lookup_cached_xatom (display, atom);

  if (!xatom)
    {
      const char *name = (const char *)atom;

      xatom = XInternAtom (GDK_DISPLAY_XDISPLAY (display), name, FALSE);
      insert_atom_pair (display, atom, xatom);
    }

  return xatom;
}

void
_gdk_x11_precache_atoms (GdkDisplay          *display,
			 const gchar * const *atom_names,
			 gint                 n_atoms)
{
  Atom *xatoms;
  GdkAtom *atoms;
  const gchar **xatom_names;
  gint n_xatoms;
  gint i;

  xatoms = g_new (Atom, n_atoms);
  xatom_names = g_new (const gchar *, n_atoms);
  atoms = g_new (GdkAtom, n_atoms);

  n_xatoms = 0;
  for (i = 0; i < n_atoms; i++)
    {
      GdkAtom atom = g_intern_static_string (atom_names[i]);
      if (lookup_cached_xatom (display, atom) == None)
	{
	  atoms[n_xatoms] = atom;
	  xatom_names[n_xatoms] = atom_names[i];
	  n_xatoms++;
	}
    }

  if (n_xatoms)
    XInternAtoms (GDK_DISPLAY_XDISPLAY (display),
                  (char **)xatom_names, n_xatoms, False, xatoms);

  for (i = 0; i < n_xatoms; i++)
    insert_atom_pair (display, atoms[i], xatoms[i]);

  g_free (xatoms);
  g_free (xatom_names);
  g_free (atoms);
}

/**
 * gdk_x11_xatom_to_atom_for_display:
 * @display: (type GdkX11Display): A #GdkDisplay
 * @xatom: an X atom 
 * 
 * Convert from an X atom for a #GdkDisplay to the corresponding
 * #GdkAtom.
 * 
 * Returns: (transfer none): the corresponding #GdkAtom.
 **/
GdkAtom
gdk_x11_xatom_to_atom_for_display (GdkDisplay *display,
				   Atom	       xatom)
{
  GdkX11Display *display_x11;
  GdkAtom virtual_atom = NULL;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (xatom == None)
    return NULL;

  if (gdk_display_is_closed (display))
    return NULL;

  display_x11 = GDK_X11_DISPLAY (display);
  
  if (display_x11->atom_to_virtual)
    virtual_atom = g_hash_table_lookup (display_x11->atom_to_virtual,
				     GUINT_TO_POINTER (xatom));
  
  if (!virtual_atom)
    {
      /* If this atom doesn't exist, we'll die with an X error unless
       * we take precautions
       */
      char *name;
      gdk_x11_display_error_trap_push (display);
      name = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
      if (gdk_x11_display_error_trap_pop (display))
	{
	  g_warning (G_STRLOC " invalid X atom: %ld", xatom);
	}
      else
	{
	  virtual_atom = g_intern_string (name);
	  XFree (name);
	  
	  insert_atom_pair (display, virtual_atom, xatom);
	}
    }

  return virtual_atom;
}

/**
 * gdk_x11_get_xatom_by_name_for_display:
 * @display: (type GdkX11Display): a #GdkDisplay
 * @atom_name: a string
 * 
 * Returns the X atom for a #GdkDisplay corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than XInternAtom(), which is a round trip to the server each time.
 * 
 * Returns: a X atom for a #GdkDisplay
 **/
Atom
gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
				       const gchar *atom_name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);
  return gdk_x11_atom_to_xatom_for_display (display, g_intern_string (atom_name));
}

Atom
_gdk_x11_get_xatom_for_display_printf (GdkDisplay    *display,
                                       const gchar   *format,
                                       ...)
{
  va_list args;
  char *atom_name;
  Atom atom;

  va_start (args, format);
  atom_name = g_strdup_vprintf (format, args);
  va_end (args);

  atom = gdk_x11_get_xatom_by_name_for_display (display, atom_name);

  g_free (atom_name);

  return atom;
}

/**
 * gdk_x11_get_xatom_name_for_display:
 * @display: (type GdkX11Display): the #GdkDisplay where @xatom is defined
 * @xatom: an X atom 
 * 
 * Returns the name of an X atom for its display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * XAtomName() and the result doesn’t need to
 * be freed. 
 *
 * Returns: name of the X atom; this string is owned by GDK,
 *   so it shouldn’t be modifed or freed. 
 **/
const gchar *
gdk_x11_get_xatom_name_for_display (GdkDisplay *display,
				    Atom        xatom)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return (const char *)gdk_x11_xatom_to_atom_for_display (display, xatom);
}

