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

#include <gdk/x11/gdkwindow-x11.h>
#include <gdk/x11/gdkpixmap-x11.h>

typedef struct _GdkColormapPrivateX11  GdkColormapPrivateX11;
typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkFontPrivateX        GdkFontPrivateX;
typedef struct _GdkImagePrivateX11     GdkImagePrivateX11;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  Display *xdisplay;
};

struct _GdkFontPrivateX
{
  GdkFontPrivate base;
  /* XFontStruct *xfont; */
  /* generic pointer point to XFontStruct or XFontSet */
  gpointer xfont;
  Display *xdisplay;

  GSList *names;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
  Visual *xvisual;
};

struct _GdkVisualClass
{
  GObjectClass parent_class;
};

struct _GdkColormapPrivateX11
{
  Colormap xcolormap;
  Display *xdisplay;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  time_t last_sync_time;
};

struct _GdkImagePrivateX11
{
  XImage *ximage;
  Display *xdisplay;
  gpointer x_shm_info;
};


typedef struct _GdkGCX11      GdkGCX11;
typedef struct _GdkGCX11Class GdkGCX11Class;

#define GDK_TYPE_GC_X11              (gdk_gc_x11_get_type ())
#define GDK_GC_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_X11, GdkGCX11))
#define GDK_GC_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_X11, GdkGCX11Class))
#define GDK_IS_GC_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_X11))
#define GDK_IS_GC_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_X11))
#define GDK_GC_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_X11, GdkGCX11Class))

struct _GdkGCX11
{
  GdkGC parent_instance;
  
  GC xgc;
  Display *xdisplay;
  GdkRegion *clip_region;
  guint dirty_mask;

  /* We can't conditionalize on HAVE_XFT here, so we simply always
   * have this here as a gpointer.
   */
  gpointer xft_draw;
  gulong fg_pixel;
};

struct _GdkGCX11Class
{
  GdkGCClass parent_class;

};

GType gdk_gc_x11_get_type (void);

#define GDK_ROOT_WINDOW()             gdk_root_window
#define GDK_ROOT_PARENT()             ((GdkWindow *)gdk_parent_root)
#define GDK_DISPLAY()                 gdk_display
#define GDK_WINDOW_XDISPLAY(win)      (GDK_DRAWABLE_IMPL_X11(((GdkWindowObject *)win)->impl)->xdisplay)
#define GDK_WINDOW_XID(win)           (GDK_DRAWABLE_IMPL_X11(((GdkWindowObject *)win)->impl)->xid)
#define GDK_PIXMAP_XDISPLAY(win)      (GDK_DRAWABLE_IMPL_X11(((GdkPixmapObject *)win)->impl)->xdisplay)
#define GDK_PIXMAP_XID(win)           (GDK_DRAWABLE_IMPL_X11(((GdkPixmapObject *)win)->impl)->xid)
#define GDK_DRAWABLE_XDISPLAY(win)    (GDK_IS_WINDOW (win) ? GDK_WINDOW_XDISPLAY (win) : GDK_PIXMAP_XDISPLAY (win))
#define GDK_DRAWABLE_XID(win)         (GDK_IS_WINDOW (win) ? GDK_WINDOW_XID (win) : GDK_PIXMAP_XID (win))
#define GDK_IMAGE_XDISPLAY(image)     (((GdkImagePrivateX11 *) GDK_IMAGE (image)->windowing_data)->xdisplay)
#define GDK_IMAGE_XIMAGE(image)       (((GdkImagePrivateX11 *) GDK_IMAGE (image)->windowing_data)->ximage)
#define GDK_GC_XDISPLAY(gc)           (GDK_GC_X11(gc)->xdisplay)
#define GDK_COLORMAP_XDISPLAY(cmap)   (((GdkColormapPrivateX11 *)GDK_COLORMAP (cmap)->windowing_data)->xdisplay)
#define GDK_COLORMAP_XCOLORMAP(cmap)  (((GdkColormapPrivateX11 *)GDK_COLORMAP (cmap)->windowing_data)->xcolormap)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate *) vis)->xvisual)
#define GDK_FONT_XDISPLAY(font)       (((GdkFontPrivate *) font)->xdisplay)
#define GDK_FONT_XFONT(font)          (((GdkFontPrivateX *)font)->xfont)

#define GDK_GC_XGC(gc)       (GDK_GC_X11(gc)->xgc)
#define GDK_GC_GET_XGC(gc)   (GDK_GC_X11(gc)->dirty_mask ? _gdk_x11_gc_flush (gc) : GDK_GC_XGC (gc))
#define GDK_WINDOW_XWINDOW    GDK_DRAWABLE_XID

extern Display		*gdk_display;
extern Window		 gdk_root_window;
extern gint		 gdk_screen;
extern gchar		*gdk_display_name;
extern Window		 gdk_leader_window;

extern Atom		 gdk_selection_property;

extern gchar		*gdk_progclass;

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

void gdk_x11_grab_server (void);
void gdk_x11_ungrab_server (void);

#define gdk_window_lookup(xid)	   ((GdkWindow*) gdk_xid_table_lookup (xid))
#define gdk_pixmap_lookup(xid)	   ((GdkPixmap*) gdk_xid_table_lookup (xid))
#define gdk_font_lookup(xid)	   ((GdkFont*) gdk_xid_table_lookup (xid))

GC _gdk_x11_gc_flush (GdkGC *gc);

#endif /* __GDK_X_H__ */
