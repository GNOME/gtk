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

#ifndef __GDK_SURFACE_BROADWAY_H__
#define __GDK_SURFACE_BROADWAY_H__

#include <gdk/gdksurfaceprivate.h>
#include "gdkbroadwaysurface.h"

G_BEGIN_DECLS

/* Surface implementation for Broadway
 */

struct _GdkBroadwaySurface
{
  GdkSurface parent_instance;

  GdkCursor *cursor;

  int id;

  gboolean visible;
  gboolean maximized;
  int transient_for;

  int pre_maximize_x;
  int pre_maximize_y;
  int pre_maximize_width;
  int pre_maximize_height;

  gint64 pending_frame_counter;

  gboolean dirty;
  gboolean last_synced;

  GdkGeometry geometry_hints;
  GdkSurfaceHints geometry_hints_mask;

  GArray *node_data;
  GPtrArray *node_data_textures;

  int offset_x;
  int offset_y;
};

struct _GdkBroadwaySurfaceClass
{
  GdkSurfaceClass parent_class;
};

GType gdk_surface_broadway_get_type (void);

G_END_DECLS

#endif /* __GDK_SURFACE_BROADWAY_H__ */
