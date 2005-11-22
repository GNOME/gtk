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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "gdkproperty.h"

GdkAtom
gdk_atom_intern (const gchar *atom_name,
		 gboolean     only_if_exists)
{
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  /* FIXME: Implement */
  return GDK_NONE;
}

GdkAtom
gdk_atom_intern_static_string (const gchar *atom_name)
{
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);
  
  /* FIXME: Implement */
  return GDK_NONE;
}


gchar *
gdk_atom_name (GdkAtom atom)
{
  /* FIXME: Implement */
  return NULL;
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  /* FIXME: Implement */
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
  /* FIXME: Implement */
  return 0;
}

void
gdk_property_change (GdkWindow   *window,
		     GdkAtom      property,
		     GdkAtom      type,
		     gint         format,
		     GdkPropMode  mode,
		     const guchar *data,
		     gint         nelements)
{
  /* FIXME: Implement */
}
