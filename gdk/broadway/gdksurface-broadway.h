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

#include <gdk/gdksurfaceimpl.h>

G_BEGIN_DECLS

typedef struct _GdkSurfaceImplBroadway GdkSurfaceImplBroadway;
typedef struct _GdkSurfaceImplBroadwayClass GdkSurfaceImplBroadwayClass;

/* Surface implementation for Broadway
 */

#define GDK_TYPE_SURFACE_IMPL_BROADWAY              (gdk_surface_impl_broadway_get_type ())
#define GDK_SURFACE_IMPL_BROADWAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SURFACE_IMPL_BROADWAY, GdkSurfaceImplBroadway))
#define GDK_SURFACE_IMPL_BROADWAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SURFACE_IMPL_BROADWAY, GdkSurfaceImplBroadwayClass))
#define GDK_IS_SURFACE_IMPL_BROADWAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SURFACE_IMPL_BROADWAY))
#define GDK_IS_SURFACE_IMPL_BROADWAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SURFACE_IMPL_BROADWAY))
#define GDK_SURFACE_IMPL_BROADWAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SURFACE_IMPL_BROADWAY, GdkSurfaceImplBroadwayClass))

struct _GdkSurfaceImplBroadway
{
  GdkSurfaceImpl parent_instance;

  GdkSurface *wrapper;

  GdkCursor *cursor;

  int id;

  gboolean visible;
  gboolean maximized;
  int transient_for;

  int pre_maximize_x;
  int pre_maximize_y;
  int pre_maximize_width;
  int pre_maximize_height;

  gboolean dirty;
  gboolean last_synced;

  GdkGeometry geometry_hints;
  GdkSurfaceHints geometry_hints_mask;

  GArray *node_data;
  GPtrArray *node_data_textures;
};

struct _GdkSurfaceImplBroadwayClass
{
  GdkSurfaceImplClass parent_class;
};

GType gdk_surface_impl_broadway_get_type (void);

G_END_DECLS

#endif /* __GDK_SURFACE_BROADWAY_H__ */
