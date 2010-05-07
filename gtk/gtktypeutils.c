/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "config.h"
#include <string.h> /* strcmp */

#include "gtktypeutils.h"
#include "gtkobject.h"
#include "gtkintl.h"
#include "gtkalias.h"

GType
gtk_identifier_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    {
      GTypeInfo tinfo = { 0, };
      our_type = g_type_register_static (G_TYPE_STRING, I_("GtkIdentifier"), &tinfo, 0);
    }

  return our_type;
}

#define __GTK_TYPE_UTILS_C__
#include "gtkaliasdef.c"
