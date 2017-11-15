/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 *               2005 Imendio AB
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

#include "config.h"

#include "gdkproperty.h"

#include "gdkprivate.h"

/**
 * SECTION:properties
 * @Short_description: Functions to manipulate properties on windows
 * @Title: Properties and Atoms
 *
 * Each window under X can have any number of associated
 * “properties” attached to it.
 * Properties are arbitrary chunks of data identified by
 * “atom”s. (An “atom”
 * is a numeric index into a string table on the X server. They are used
 * to transfer strings efficiently between clients without
 * having to transfer the entire string.) A property
 * has an associated type, which is also identified
 * using an atom.
 *
 * A property has an associated “format”,
 * an integer describing how many bits are in each unit
 * of data inside the property. It must be 8, 16, or 32.
 * When data is transferred between the server and client,
 * if they are of different endianesses it will be byteswapped
 * as necessary according to the format of the property.
 * Note that on the client side, properties of format 32
 * will be stored with one unit per long,
 * even if a long integer has more than 32 bits on the platform.
 * (This decision was apparently made for Xlib to maintain
 * compatibility with programs that assumed longs were 32
 * bits, at the expense of programs that knew better.)
 *
 * The functions in this section are used to add, remove
 * and change properties on windows, to convert atoms
 * to and from strings and to manipulate some types of
 * data commonly stored in X window properties.
 */

/**
 * gdk_atom_intern:
 * @atom_name: a string.
 * @only_if_exists: if %TRUE, GDK is allowed to not create a new atom, but
 *   just return %GDK_NONE if the requested atom doesn’t already
 *   exists. Currently, the flag is ignored, since checking the
 *   existance of an atom is as expensive as creating it.
 *
 * Finds or creates an atom corresponding to a given string.
 *
 * Returns: (transfer none): the atom corresponding to @atom_name.
 */
GdkAtom
gdk_atom_intern (const gchar *atom_name,
                 gboolean     only_if_exists)
{
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  return g_intern_string (atom_name);
}

/**
 * gdk_atom_intern_static_string:
 * @atom_name: a static string
 *
 * Finds or creates an atom corresponding to a given string.
 *
 * Note that this function is identical to gdk_atom_intern() except
 * that if a new #GdkAtom is created the string itself is used rather
 * than a copy. This saves memory, but can only be used if the string
 * will always exist. It can be used with statically
 * allocated strings in the main program, but not with statically
 * allocated memory in dynamically loaded modules, if you expect to
 * ever unload the module again (e.g. do not use this function in
 * GTK+ theme engines).
 *
 * Returns: (transfer none): the atom corresponding to @atom_name
 *
 * Since: 2.10
 */
GdkAtom
gdk_atom_intern_static_string (const gchar *atom_name)
{
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  return g_intern_static_string (atom_name);
}

/**
 * gdk_atom_name:
 * @atom: a #GdkAtom.
 *
 * Determines the string corresponding to an atom.
 *
 * Returns: a newly-allocated string containing the string
 *   corresponding to @atom. When you are done with the
 *   return value, you should free it using g_free().
 */
gchar *
gdk_atom_name (GdkAtom atom)
{
  return g_strdup (_gdk_atom_name_const (atom));
}

const gchar *
_gdk_atom_name_const (GdkAtom atom)
{
  return atom;
}
