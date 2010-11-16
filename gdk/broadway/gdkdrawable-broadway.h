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

#ifndef __GDK_DRAWABLE_BROADWAY_H__
#define __GDK_DRAWABLE_BROADWAY_H__

#include <gdk/gdkdrawable.h>

G_BEGIN_DECLS

/* Drawable implementation for Broadway
 */

typedef struct _GdkDrawableImplBroadway GdkDrawableImplBroadway;
typedef struct _GdkDrawableImplBroadwayClass GdkDrawableImplBroadwayClass;

#define GDK_TYPE_DRAWABLE_IMPL_BROADWAY              (_gdk_drawable_impl_broadway_get_type ())
#define GDK_DRAWABLE_IMPL_BROADWAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE_IMPL_BROADWAY, GdkDrawableImplBroadway))
#define GDK_DRAWABLE_IMPL_BROADWAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWABLE_IMPL_BROADWAY, GdkDrawableImplBroadwayClass))
#define GDK_IS_DRAWABLE_IMPL_BROADWAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_BROADWAY))
#define GDK_IS_DRAWABLE_IMPL_BROADWAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWABLE_IMPL_BROADWAY))
#define GDK_DRAWABLE_IMPL_BROADWAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWABLE_IMPL_BROADWAY, GdkDrawableImplBroadwayClass))

struct _GdkDrawableImplBroadway
{
  GdkDrawable parent_instance;

  GdkDrawable *wrapper;

  GdkScreen *screen;
  cairo_surface_t *surface;
  cairo_surface_t *last_surface;
  cairo_surface_t *ref_surface;
};

struct _GdkDrawableImplBroadwayClass
{
  GdkDrawableClass parent_class;

};

GType _gdk_drawable_impl_broadway_get_type (void);

/* Note that the following take GdkDrawableImplBroadway, not the wrapper drawable */
void _gdk_broadway_drawable_finish           (GdkDrawable  *drawable);
void _gdk_broadway_drawable_update_size      (GdkDrawable  *drawable);
GdkDrawable *gdk_broadway_window_get_drawable_impl (GdkWindow *window);

G_END_DECLS

#endif /* __GDK_DRAWABLE_BROADWAY_H__ */
