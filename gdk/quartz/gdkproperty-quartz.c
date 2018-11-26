/* gdkproperty-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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
 /* Below here, these are our additions. Increment N_CUSTOM_PREDEFINED
   * if you add any.
   */
  "CLIPBOARD\0"			/* = 69 */;

static const gint xatoms_offset[] = {
    0,   5,  13,  23,  27,  32,  39,  48,  57,  64,  76,  88, 
  100, 112, 124, 136, 148, 160, 169, 174, 182, 189, 195, 205, 
  222, 236, 249, 262, 278, 291, 305, 317, 324, 333, 340, 351, 
  360, 378, 391, 404, 412, 428, 442, 456, 466, 477, 487, 497, 
  511, 525, 537, 549, 568, 588, 605, 623, 636, 645, 656, 663, 
  674, 685, 695, 702, 712, 724, 734, 745, 754, 771
};

#define N_CUSTOM_PREDEFINED 1

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

void
_gdk_quartz_window_delete_property (GdkWindow *window,
                                    GdkAtom    property)
{
  /* FIXME: Implement */
}

gint
_gdk_quartz_window_get_property (GdkWindow   *window,
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
  /* FIXME: Implement */
  return 0;
}

void
_gdk_quartz_window_change_property (GdkWindow   *window,
                                    GdkAtom      property,
                                    GdkAtom      type,
                                    gint         format,
                                    GdkPropMode  mode,
                                    const guchar *data,
                                    gint         nelements)
{
  /* FIXME: Implement */
}
