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
 * <firstterm>properties</firstterm> attached to it.
 * Properties are arbitrary chunks of data identified by
 * <firstterm>atom</firstterm>s. (An <firstterm>atom</firstterm>
 * is a numeric index into a string table on the X server. They are used
 * to transfer strings efficiently between clients without
 * having to transfer the entire string.) A property
 * has an associated type, which is also identified
 * using an atom.
 *
 * A property has an associated <firstterm>format</firstterm>,
 * an integer describing how many bits are in each unit
 * of data inside the property. It must be 8, 16, or 32.
 * When data is transferred between the server and client,
 * if they are of different endianesses it will be byteswapped
 * as necessary according to the format of the property.
 * Note that on the client side, properties of format 32
 * will be stored with one unit per <emphasis>long</emphasis>,
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

static GHashTable *names_to_atoms;
static GPtrArray *atoms_to_names;

static const gchar xatoms_string[] = 
  /* These are all the standard predefined X atoms */
  "NONE\0"
  "PRIMARY\0"
  "SECONDARY\0"
  "ARC\0"
  "ATOM\0"
  "BITMAP\0"
  "CARDINAL\0"
  "COLORMAP\0"
  "CURSOR\0"
  "CUT_BUFFER0\0"
  "CUT_BUFFER1\0"
  "CUT_BUFFER2\0"
  "CUT_BUFFER3\0"
  "CUT_BUFFER4\0"
  "CUT_BUFFER5\0"
  "CUT_BUFFER6\0"
  "CUT_BUFFER7\0"
  "DRAWABLE\0"
  "FONT\0"
  "INTEGER\0"
  "PIXMAP\0"
  "POINT\0"
  "RECTANGLE\0"
  "RESOURCE_MANAGER\0"
  "RGB_COLOR_MAP\0"
  "RGB_BEST_MAP\0"
  "RGB_BLUE_MAP\0"
  "RGB_DEFAULT_MAP\0"
  "RGB_GRAY_MAP\0"
  "RGB_GREEN_MAP\0"
  "RGB_RED_MAP\0"
  "STRING\0"
  "VISUALID\0"
  "WINDOW\0"
  "WM_COMMAND\0"
  "WM_HINTS\0"
  "WM_CLIENT_MACHINE\0"
  "WM_ICON_NAME\0"
  "WM_ICON_SIZE\0"
  "WM_NAME\0"
  "WM_NORMAL_HINTS\0"
  "WM_SIZE_HINTS\0"
  "WM_ZOOM_HINTS\0"
  "MIN_SPACE\0"
  "NORM_SPACE\0"
  "MAX_SPACE\0"
  "END_SPACE\0"
  "SUPERSCRIPT_X\0"
  "SUPERSCRIPT_Y\0"
  "SUBSCRIPT_X\0"
  "SUBSCRIPT_Y\0"
  "UNDERLINE_POSITION\0"
  "UNDERLINE_THICKNESS\0"
  "STRIKEOUT_ASCENT\0"
  "STRIKEOUT_DESCENT\0"
  "ITALIC_ANGLE\0"
  "X_HEIGHT\0"
  "QUAD_WIDTH\0"
  "WEIGHT\0"
  "POINT_SIZE\0"
  "RESOLUTION\0"
  "COPYRIGHT\0"
  "NOTICE\0"
  "FONT_NAME\0"
  "FAMILY_NAME\0"
  "FULL_NAME\0"
  "CAP_HEIGHT\0"
  "WM_CLASS\0"
  "WM_TRANSIENT_FOR\0"
  "CLIPBOARD\0"			/* = 69 */;

static const gint xatoms_offset[] = {
    0,   5,  13,  23,  27,  32,  39,  48,  57,  64,  76,  88, 
  100, 112, 124, 136, 148, 160, 169, 174, 182, 189, 195, 205, 
  222, 236, 249, 262, 278, 291, 305, 317, 324, 333, 340, 351, 
  360, 378, 391, 404, 412, 428, 442, 456, 466, 477, 487, 497, 
  511, 525, 537, 549, 568, 588, 605, 623, 636, 645, 656, 663, 
  674, 685, 695, 702, 712, 724, 734, 745, 754, 771
};

static void
ensure_atom_tables (void)
{
  int i;
  
  if (names_to_atoms)
    return;

  names_to_atoms = g_hash_table_new (g_str_hash, g_str_equal);
  atoms_to_names = g_ptr_array_sized_new (G_N_ELEMENTS (xatoms_offset));

  for (i = 0; i < G_N_ELEMENTS (xatoms_offset); i++)
    {
      g_hash_table_insert(names_to_atoms, (gchar *)xatoms_string + xatoms_offset[i], GINT_TO_POINTER (i));
      g_ptr_array_add(atoms_to_names, (gchar *)xatoms_string + xatoms_offset[i]);
    }
}

static GdkAtom
intern_atom_internal (const gchar *atom_name, gboolean allocate)
{
  gpointer result;
  gchar *name;

  ensure_atom_tables ();
  
  if (g_hash_table_lookup_extended (names_to_atoms, atom_name, NULL, &result))
    return result;
  
  result = GINT_TO_POINTER (atoms_to_names->len);
  name = allocate ? g_strdup (atom_name) : (gchar *)atom_name;
  g_hash_table_insert(names_to_atoms, name, result);
  g_ptr_array_add(atoms_to_names, name);
  
  return result;  
}

/**
 * gdk_atom_intern:
 * @atom_name: a string.
 * @only_if_exists: if %TRUE, GDK is allowed to not create a new atom, but
 *   just return %GDK_NONE if the requested atom doesn't already
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

  return intern_atom_internal (atom_name, TRUE);
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
 * will <emphasis>always</emphasis> exist. It can be used with statically
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

  return intern_atom_internal (atom_name, FALSE);
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
  ensure_atom_tables ();

  if (GPOINTER_TO_INT (atom) >= atoms_to_names->len)
    return NULL;

  return g_ptr_array_index (atoms_to_names, GPOINTER_TO_INT (atom));
}
