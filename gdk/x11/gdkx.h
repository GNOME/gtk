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

#ifndef __GDK_X_H__
#define __GDK_X_H__

#include <gdk/gdkprivate.h>
#include <gdk/gdkcursor.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_BEGIN_DECLS

extern Display		*gdk_display;

Display *gdk_x11_drawable_get_xdisplay    (GdkDrawable *drawable);
XID      gdk_x11_drawable_get_xid         (GdkDrawable *drawable);
Display *gdk_x11_image_get_xdisplay       (GdkImage    *image);
XImage  *gdk_x11_image_get_ximage         (GdkImage    *image);
Display *gdk_x11_colormap_get_xdisplay    (GdkColormap *colormap);
Colormap gdk_x11_colormap_get_xcolormap   (GdkColormap *colormap);
Display *gdk_x11_cursor_get_xdisplay      (GdkCursor   *cursor);
Cursor   gdk_x11_cursor_get_xcursor       (GdkCursor   *cursor);
Visual * gdk_x11_visual_get_xvisual       (GdkVisual   *visual);
Display *gdk_x11_gc_get_xdisplay          (GdkGC       *gc);
GC       gdk_x11_gc_get_xgc               (GdkGC       *gc);
Window   gdk_x11_get_default_root_xwindow (void);
Display *gdk_x11_get_default_xdisplay     (void);
gint     gdk_x11_get_default_screen       (void);

#define GDK_COLORMAP_XDISPLAY(cmap)   (gdk_x11_colormap_get_xdisplay (cmap))
#define GDK_COLORMAP_XCOLORMAP(cmap)  (gdk_x11_colormap_get_xcolormap (cmap))
#define GDK_CURSOR_XDISPLAY(win)      (gdk_x11_cursor_get_xdisplay (win))
#define GDK_CURSOR_XCURSOR(win)       (gdk_x11_cursor_get_xid (win))
#define GDK_DISPLAY()                 gdk_display
#define GDK_IMAGE_XDISPLAY(image)     (gdk_x11_image_get_xdisplay (image))
#define GDK_IMAGE_XIMAGE(image)       (gdk_x11_image_get_ximage (image))

#ifdef INSIDE_GDK_X11

#include "gdkprivate-x11.h"

#define GDK_ROOT_WINDOW()             _gdk_root_window
#undef GDK_ROOT_PARENT
#define GDK_ROOT_PARENT()             ((GdkWindow *)_gdk_parent_root)
#define GDK_WINDOW_XDISPLAY(win)      (GDK_DRAWABLE_IMPL_X11(((GdkWindowObject *)win)->impl)->xdisplay)
#define GDK_WINDOW_XID(win)           (GDK_DRAWABLE_IMPL_X11(((GdkWindowObject *)win)->impl)->xid)
#define GDK_PIXMAP_XDISPLAY(win)      (GDK_DRAWABLE_IMPL_X11(((GdkPixmapObject *)win)->impl)->xdisplay)
#define GDK_PIXMAP_XID(win)           (GDK_DRAWABLE_IMPL_X11(((GdkPixmapObject *)win)->impl)->xid)
#define GDK_DRAWABLE_XDISPLAY(win)    (GDK_IS_WINDOW (win) ? GDK_WINDOW_XDISPLAY (win) : GDK_PIXMAP_XDISPLAY (win))
#define GDK_DRAWABLE_XID(win)         (GDK_IS_WINDOW (win) ? GDK_WINDOW_XID (win) : GDK_PIXMAP_XID (win))
#define GDK_GC_XDISPLAY(gc)           (GDK_GC_X11(gc)->xdisplay)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate *) vis)->xvisual)
#define GDK_GC_XGC(gc)       (GDK_GC_X11(gc)->xgc)
#define GDK_GC_GET_XGC(gc)   (GDK_GC_X11(gc)->dirty_mask ? _gdk_x11_gc_flush (gc) : ((GdkGCX11 *)(gc))->xgc)
#define GDK_WINDOW_XWINDOW    GDK_DRAWABLE_XID

#else /* INSIDE_GDK_X11 */

#define GDK_ROOT_WINDOW()             (gdk_x11_get_default_root_xwindow ())
#define GDK_WINDOW_XDISPLAY(win)      (gdk_x11_drawable_get_xdisplay (win))
#define GDK_WINDOW_XID(win)           (gdk_x11_drawable_get_xid (win))
#define GDK_WINDOW_XWINDOW(win)       (gdk_x11_drawable_get_xid (win))
#define GDK_PIXMAP_XDISPLAY(win)      (gdk_x11_drawable_get_xdisplay (win))
#define GDK_PIXMAP_XID(win)           (gdk_x11_drawable_get_xid (win))
#define GDK_DRAWABLE_XDISPLAY(win)    (gdk_x11_drawable_get_xdisplay (win))
#define GDK_DRAWABLE_XID(win)         (gdk_x11_drawable_get_xid (win))
#define GDK_VISUAL_XVISUAL(visual)    (gdk_x11_visual_get_xvisual (visual))
#define GDK_GC_XDISPLAY(gc)           (gdk_x11_gc_get_xdisplay (gc))
#define GDK_GC_XGC(gc)                (gdk_x11_gc_get_xgc (gc))

#endif /* INSIDE_GDK_X11 */

GdkVisual*   gdkx_visual_get   (VisualID xvisualid);
/* XXX: Do not use this function until it is fixed. An X Colormap
 *      is useless unless we also have the visual. */
GdkColormap* gdkx_colormap_get (Colormap xcolormap);
/* Utility function in gdk.c - not sure where it belongs, but it's
   needed in more than one place, so make it public */
Window        gdk_get_client_window      (Display  *dpy,
                                          Window    win);

/* Functions to create pixmaps and windows from their X equivalents */
GdkPixmap    *gdk_pixmap_foreign_new (GdkNativeWindow anid);
GdkWindow    *gdk_window_foreign_new (GdkNativeWindow anid);

/* Return the Gdk* for a particular XID */
gpointer      gdk_xid_table_lookup     (XID              xid);

guint32       gdk_x11_get_server_time  (GdkWindow       *window);

/* FIXME should take a GdkDisplay* */
void          gdk_x11_grab_server      (void);
void          gdk_x11_ungrab_server    (void);

/* returns TRUE if we support the given WM spec feature */
gboolean      gdk_net_wm_supports      (GdkAtom property);

#define gdk_window_lookup(xid)	   ((GdkWindow*) gdk_xid_table_lookup (xid))
#define gdk_pixmap_lookup(xid)	   ((GdkPixmap*) gdk_xid_table_lookup (xid))

#ifndef GDK_DISABLE_DEPRECATED

Display *gdk_x11_font_get_xdisplay      (GdkFont     *font);
gpointer gdk_x11_font_get_xfont         (GdkFont     *font);

#define GDK_FONT_XDISPLAY(font)       (gdk_x11_font_get_xdisplay (font))
#define GDK_FONT_XFONT(font)          (gdk_x11_font_get_xfont (font))

#define gdk_font_lookup(xid)	   ((GdkFont*) gdk_xid_table_lookup (xid))

#endif /* GDK_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __GDK_X_H__ */
