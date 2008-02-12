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

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"


static GHashTable *window_id_ht = NULL;


void
gdk_directfb_window_id_table_insert (DFBWindowID  dfb_id,
                                     GdkWindow   *window)
{
  if (!window_id_ht)
    window_id_ht = g_hash_table_new (g_direct_hash, g_direct_equal);

  g_hash_table_insert (window_id_ht, GUINT_TO_POINTER (dfb_id), window);
}

void
gdk_directfb_window_id_table_remove (DFBWindowID dfb_id)
{
  if (window_id_ht)
    g_hash_table_remove (window_id_ht, GUINT_TO_POINTER (dfb_id));
}

GdkWindow *
gdk_directfb_window_id_table_lookup (DFBWindowID dfb_id)
{
  GdkWindow *window = NULL;

  if (window_id_ht)
    window = (GdkWindow *) g_hash_table_lookup (window_id_ht,
                                                GUINT_TO_POINTER (dfb_id));

  return window;
}
