/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/ or
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
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"

#include <string.h>

#include "gdkproperty.h"

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"

#include "gdkalias.h"

static GHashTable *names_to_atoms;
static GPtrArray  *atoms_to_names;

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
;

static const gint xatoms_offset[] = {
    0,   5,  13,  23,  27,  32,  39,  48,  57,  64,  76,  88,
  100, 112, 124, 136, 148, 160, 169, 174, 182, 189, 195, 205,
  222, 236, 249, 262, 278, 291, 305, 317, 324, 333, 340, 351,
  360, 378, 391, 404, 412, 428, 442, 456, 466, 477, 487, 497,
  511, 525, 537, 549, 568, 588, 605, 623, 636, 645, 656, 663,
  674, 685, 695, 702, 712, 724, 734, 745, 754
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
      g_hash_table_insert (names_to_atoms,
                           (gchar *)xatoms_string + xatoms_offset[i],
                           GINT_TO_POINTER (i));
      g_ptr_array_add (atoms_to_names,
                       (gchar *)xatoms_string + xatoms_offset[i]);
    }
}

static GdkAtom
intern_atom_internal (const gchar *atom_name, gboolean allocate)
{
  gpointer  result;
  gchar    *name;

  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  ensure_atom_tables ();

  if (g_hash_table_lookup_extended (names_to_atoms, atom_name, NULL, &result))
    return result;

  result = GINT_TO_POINTER (atoms_to_names->len);
  name   = allocate ? g_strdup (atom_name) : (gchar *)atom_name;
  g_hash_table_insert (names_to_atoms, name, result);
  g_ptr_array_add (atoms_to_names, name);

  return result;
}

GdkAtom
gdk_atom_intern (const gchar *atom_name,
		 gboolean     only_if_exists)
{
  return intern_atom_internal (atom_name, TRUE);
}

GdkAtom
gdk_atom_intern_static_string (const gchar *atom_name)
{
  return intern_atom_internal (atom_name, FALSE);
}


gchar *
gdk_atom_name (GdkAtom atom)
{
  if (!atoms_to_names)
    return NULL;

  if (GPOINTER_TO_INT (atom) >= atoms_to_names->len)
    return NULL;

  return g_strdup (g_ptr_array_index (atoms_to_names, GPOINTER_TO_INT (atom)));
}


static void
gdk_property_delete_2 (GdkWindow         *window,
                       GdkAtom            property,
                       GdkWindowProperty *prop)
{
  GdkWindowImplDirectFB *impl;
  GdkEvent              *event;
  GdkWindow             *event_window;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  g_hash_table_remove (impl->properties, GUINT_TO_POINTER (property));
  g_free (prop);

  event_window = gdk_directfb_other_event_window (window, GDK_PROPERTY_NOTIFY);

  if (event_window)
    {
      event                 = gdk_directfb_event_make (event_window, GDK_PROPERTY_NOTIFY);
      event->property.atom  = property;
      event->property.state = GDK_PROPERTY_DELETE;
    }
}

void
gdk_property_delete (GdkWindow *window,
                     GdkAtom    property)
{
  GdkWindowImplDirectFB *impl;
  GdkWindowProperty     *prop;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (!impl->properties)
    return;

  prop = g_hash_table_lookup (impl->properties, GUINT_TO_POINTER (property));
  if (!prop)
    return;

  gdk_property_delete_2 (window, property, prop);
}

gboolean
gdk_property_get (GdkWindow  *window,
                  GdkAtom     property,
                  GdkAtom     type,
                  gulong      offset,
                  gulong      length,
                  gint        pdelete,
                  GdkAtom    *actual_property_type,
                  gint       *actual_format_type,
                  gint       *actual_length,
                  guchar    **data)
{
  GdkWindowImplDirectFB *impl;
  GdkWindowProperty     *prop;
  gint                   nbytes = 0;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (!window)
    window = _gdk_parent_root;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (!impl->properties)
    return FALSE;

  prop = g_hash_table_lookup (impl->properties, GUINT_TO_POINTER (property));
  if (!prop)
    {
      if (actual_property_type)
        *actual_property_type = GDK_NONE;
      return FALSE;
    }

  nbytes = CLAMP (length, 0, prop->length - offset * 4);

  if (nbytes > 0 &&
      (prop->type == 0 /* AnyPropertyType */ || prop->type == type))
    {
      *data = g_malloc (nbytes + 1);
      memcpy (*data, prop->data + offset, nbytes);
      (*data)[nbytes] = 0;
    }
  else
    {
      *data = NULL;
    }

  if (actual_length)
    *actual_length        = nbytes;
  if (actual_property_type)
    *actual_property_type = prop->type;
  if (actual_format_type)
    *actual_format_type   = prop->format;

  /* only delete the property if it was completely retrieved */
  if (pdelete && length >= *actual_length && *data != NULL)
    {
      gdk_property_delete_2 (window, property, prop);
    }

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
  GdkWindowImplDirectFB *impl;
  GdkWindowProperty     *prop;
  GdkWindowProperty     *new_prop;
  gint                   new_size = 0;
  GdkEvent              *event;
  GdkWindow             *event_window;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (!window)
    window = _gdk_parent_root;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (!impl->properties)
    impl->properties = g_hash_table_new (NULL, NULL);

  prop = g_hash_table_lookup (impl->properties, GUINT_TO_POINTER (property));

  switch (mode)
    {
    case GDK_PROP_MODE_REPLACE:
      new_size = nelements * (format >> 3);
      break;

    case GDK_PROP_MODE_PREPEND:
    case GDK_PROP_MODE_APPEND:
      new_size = nelements * (format >> 3);
      if (prop)
        {
          if (type != prop->type || format != prop->format)
            return;
          new_size += prop->length;
        }
      break;
    }

  new_prop         = g_malloc (G_STRUCT_OFFSET (GdkWindowProperty, data) + new_size);
  new_prop->length = new_size;
  new_prop->type   = type;
  new_prop->format = format;

  switch (mode)
    {
    case GDK_PROP_MODE_REPLACE:
      memcpy (new_prop->data, data, new_size);
      break;

    case GDK_PROP_MODE_APPEND:
      if (prop)
        memcpy (new_prop->data, prop->data, prop->length);
      memcpy (new_prop->data + new_prop->length,
              data, (nelements * (format >> 3)));
      break;

    case GDK_PROP_MODE_PREPEND:
      memcpy (new_prop->data, data, (nelements * (format >> 3)));
      if (prop)
        memcpy (new_prop->data + (nelements * (format >> 3)),
                prop->data, prop->length);
      break;
    }

  g_hash_table_insert (impl->properties,
                       GUINT_TO_POINTER (property), new_prop);
  g_free (prop);

  event_window = gdk_directfb_other_event_window (window, GDK_PROPERTY_NOTIFY);

  if (event_window)
    {
      event                 = gdk_directfb_event_make (event_window, GDK_PROPERTY_NOTIFY);
      event->property.atom  = property;
      event->property.state = GDK_PROPERTY_NEW_VALUE;
    }
}

#define __GDK_PROPERTY_X11_C__
#include "gdkaliasdef.c"
