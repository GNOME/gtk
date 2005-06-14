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

#ifndef __GDK_PIXMAP_MACOSX_H__
#define __GDK_PIXMAP_MACOSX_H__

#include <gdk/macosx/gdkdrawable-macosx.h>
#include <gdk/gdkpixmap.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Pixmap implementation for MacOSX
 */

typedef struct _GdkPixmapImplMacOSX GdkPixmapImplMacOSX;
typedef struct _GdkPixmapImplMacOSXClass GdkPixmapImplMacOSXClass;

#define GDK_TYPE_PIXMAP_IMPL_MacOSX              (gdk_pixmap_impl_macosx_get_type ())
#define GDK_PIXMAP_IMPL_MACOSX(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXMAP_IMPL_MacOSX, GdkPixmapImplMacOSX))
#define GDK_PIXMAP_IMPL_MACOSX_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXMAP_IMPL_MACOSX, GdkPixmapImplMacOSXClass))
#define GDK_IS_PIXMAP_IMPL_MACOSX(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXMAP_IMPL_MacOSX))
#define GDK_IS_PIXMAP_IMPL_MACOSX_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXMAP_IMPL_MACOSX))
#define GDK_PIXMAP_IMPL_MACOSX_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXMAP_IMPL_MACOSX, GdkPixmapImplMacOSXClass))

struct _GdkPixmapImplMacOSX
{
  GdkDrawableImplMacOSX parent_instance;

  gint width;
  gint height;

  guint is_foreign : 1;
};
 
struct _GdkPixmapImplMacOSXClass 
{
  GdkDrawableImplMacOSXClass parent_class;

};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_PIXMAP_MacOSX_H__ */
