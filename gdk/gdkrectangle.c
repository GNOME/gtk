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

#include <config.h>
#include <gdk/gdk.h>
#include "gdkalias.h"

/**
 * gdk_rectangle_union:
 * @src1: a #GdkRectangle
 * @src2: a #GdkRectangle
 * @dest: return location for the union of @src1 and @src2
 *
 * Calculates the union of two rectangles.
 * The union of rectangles @src1 and @src2 is the smallest rectangle which
 * includes both @src1 and @src2 within it.
 * It is allowed for @dest to be the same as either @src1 or @src2.
 */
void
gdk_rectangle_union (GdkRectangle *src1,
		     GdkRectangle *src2,
		     GdkRectangle *dest)
{
  gint dest_x, dest_y;
  
  g_return_if_fail (src1 != NULL);
  g_return_if_fail (src2 != NULL);
  g_return_if_fail (dest != NULL);

  dest_x = MIN (src1->x, src2->x);
  dest_y = MIN (src1->y, src2->y);
  dest->width = MAX (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest->height = MAX (src1->y + src1->height, src2->y + src2->height) - dest_y;
  dest->x = dest_x;
  dest->y = dest_y;
}

/**
 * gdk_rectangle_intersect:
 * @src1: a #GdkRectangle
 * @src2: a #GdkRectangle
 * @dest: return location for the intersection of @src1 and @src2
 *
 * Calculates the intersection of two rectangles.
 * It is allowed for @dest to be the same as either @src1 or @src2.
 *
 * Returns: %TRUE if the rectangles intersect.
 */
gboolean
gdk_rectangle_intersect (GdkRectangle *src1,
			 GdkRectangle *src2,
			 GdkRectangle *dest)
{
  gint dest_x, dest_y;
  gint dest_w, dest_h;
  gint return_val;

  g_return_val_if_fail (src1 != NULL, FALSE);
  g_return_val_if_fail (src2 != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  return_val = FALSE;

  dest_x = MAX (src1->x, src2->x);
  dest_y = MAX (src1->y, src2->y);
  dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;

  if (dest_w > 0 && dest_h > 0)
    {
      dest->x = dest_x;
      dest->y = dest_y;
      dest->width = dest_w;
      dest->height = dest_h;
      return_val = TRUE;
    }
  else
    {
      dest->width = 0;
      dest->height = 0;
    }

  return return_val;
}

static GdkRectangle *
gdk_rectangle_copy (const GdkRectangle *rectangle)
{
  GdkRectangle *result = g_new (GdkRectangle, 1);
  *result = *rectangle;

  return result;
}

GType
gdk_rectangle_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (g_intern_static_string ("GdkRectangle"),
					     (GBoxedCopyFunc)gdk_rectangle_copy,
					     (GBoxedFreeFunc)g_free);
  return our_type;
}


#define __GDK_RECTANGLE_C__
#include "gdkaliasdef.c"
