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

#ifndef __GDK_X_H__
#define __GDK_X_H__

#include <gdk/gdkprivate.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
  

#define GDK_ROOT_WINDOW()             gdk_root_window
#define GDK_ROOT_PARENT()             ((GdkWindow *)&gdk_root_parent)
#define GDK_DISPLAY()                 gdk_display
#define GDK_WINDOW_XDISPLAY(win)      (((GdkWindowPrivate*) win)->xdisplay)
#define GDK_WINDOW_XWINDOW(win)       (((GdkWindowPrivate*) win)->xwindow)
#define GDK_IMAGE_XDISPLAY(image)     (((GdkImagePrivate*) image)->xdisplay)
#define GDK_IMAGE_XIMAGE(image)       (((GdkImagePrivate*) image)->ximage)
#define GDK_GC_XDISPLAY(gc)           (((GdkGCPrivate*) gc)->xdisplay)
#define GDK_GC_XGC(gc)                (((GdkGCPrivate*) gc)->xgc)
#define GDK_COLORMAP_XDISPLAY(cmap)   (((GdkColormapPrivate*) cmap)->xdisplay)
#define GDK_COLORMAP_XCOLORMAP(cmap)  (((GdkColormapPrivate*) cmap)->xcolormap)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate*) vis)->xvisual)
#define GDK_FONT_XDISPLAY(font)       (((GdkFontPrivate*) font)->xdisplay)
#define GDK_FONT_XFONT(font)          (((GdkFontPrivate*) font)->xfont)


GdkVisual*   gdkx_visual_get   (VisualID xvisualid);
/* XXX: Do not use this function until it is fixed. An X Colormap
 *      is useless unless we also have the visual. */
GdkColormap* gdkx_colormap_get (Colormap xcolormap);
/* Utility function in gdk.c - not sure where it belongs, but it's
   needed in more than one place, so make it public */
Window        gdk_get_client_window      (Display  *dpy,
                                          Window    win);

/* Functions to create pixmaps and windows from their X equivalents */
GdkPixmap    *gdk_pixmap_foreign_new (guint32     anid);
GdkWindow    *gdk_window_foreign_new (guint32	     anid);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_X_H__ */
