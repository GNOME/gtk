/* GTK - The GIMP Toolkit
 * gtkpapersize.c: Paper Size
 * Copyright (C) 2006, Red Hat, Inc.
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

#include "config.h"
#include "gtkprintutils.h"
#include "gtkalias.h"

gdouble
_gtk_print_convert_to_mm (gdouble len, 
			  GtkUnit unit)
{
  switch (unit)
    {
    case GTK_UNIT_MM:
      return len;
    case GTK_UNIT_INCH:
      return len * MM_PER_INCH;
    default:
    case GTK_UNIT_PIXEL:
      g_warning ("Unsupported unit");
      /* Fall through */
    case GTK_UNIT_POINTS:
      return len * (MM_PER_INCH / POINTS_PER_INCH);
      break;
    }
}

gdouble
_gtk_print_convert_from_mm (gdouble len, 
			    GtkUnit unit)
{
  switch (unit)
    {
    case GTK_UNIT_MM:
      return len;
    case GTK_UNIT_INCH:
      return len / MM_PER_INCH;
    default:
    case GTK_UNIT_PIXEL:
      g_warning ("Unsupported unit");
      /* Fall through */
    case GTK_UNIT_POINTS:
      return len / (MM_PER_INCH / POINTS_PER_INCH);
      break;
    }
}
