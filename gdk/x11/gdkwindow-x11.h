/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_WINDOW_X11_H__
#define __GDK_WINDOW_X11_H__

#include <gdk/x11/gdkdrawable-x11.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Window implementation for X11
 */

typedef struct _GdkWindowImpl GdkWindowImpl;
typedef struct _GdkWindowImplClass GdkWindowImplClass;

#define GDK_TYPE_WINDOW_IMPL              (gdk_window_impl_get_type ())
#define GDK_WINDOW_IMPL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL, GdkWindowImpl))
#define GDK_WINDOW_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL, GdkWindowImplClass))
#define GDK_IS_WINDOW_IMPL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL))
#define GDK_IS_WINDOW_IMPL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL))
#define GDK_WINDOW_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL, GdkWindowImplClass))

struct _GdkWindowImpl
{
  GdkDrawableImpl parent_instance;

  gint width;
  gint height;
  
  GdkXPositionInfo position_info;
};
 
struct _GdkWindowImplClass 
{
  GdkDrawableImplClass parent_class;

};

GType gdk_window_impl_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_WINDOW_X11_H__ */
