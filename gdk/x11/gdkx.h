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
#include <gdk/gdkcursor.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _GdkGCXData          GdkGCXData;
typedef struct _GdkDrawableXData    GdkDrawableXData;
typedef struct _GdkWindowXData      GdkWindowXData;
typedef struct _GdkXPositionInfo    GdkXPositionInfo;
typedef struct _GdkColormapPrivateX GdkColormapPrivateX;
typedef struct _GdkCursorPrivate    GdkCursorPrivate;
typedef struct _GdkFontPrivateX     GdkFontPrivateX;
typedef struct _GdkImagePrivateX    GdkImagePrivateX;
typedef struct _GdkVisualPrivate    GdkVisualPrivate;

#ifdef USE_XIM
typedef struct _GdkICPrivate        GdkICPrivate;
#endif /* USE_XIM */

#define GDK_DRAWABLE_XDATA(win) ((GdkDrawableXData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_WINDOW_XDATA(win) ((GdkWindowXData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_GC_XDATA(gc) ((GdkGCXData *)(((GdkGCPrivate*)(gc))->klass_data))

struct _GdkGCXData
{
  GC xgc;
  Display *xdisplay;
  GdkRegion *clip_region;
  guint dirty_mask;
};

struct _GdkDrawableXData
{
  Window xid;
  Display *xdisplay;
};

struct _GdkXPositionInfo
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x_offset;		/* Offsets to add to X coordinates within window */
  gint y_offset;		/*   to get GDK coodinates within window */
  gboolean big : 1;
  gboolean mapped : 1;
  gboolean no_bg : 1;	        /* Set when the window background is temporarily
				 * unset during resizing and scaling */
  GdkRectangle clip_rect;	/* visible rectangle of window */
};

struct _GdkWindowXData
{
  GdkDrawableXData drawable_data;
  GdkXPositionInfo position_info;
};

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

struct _GdkColormapPrivateX
{
  GdkColormapPrivate base;

  Colormap xcolormap;
  Display *xdisplay;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  time_t last_sync_time;
};

struct _GdkImagePrivateX
{
  GdkImagePrivate base;
  
  XImage *ximage;
  Display *xdisplay;
  gpointer x_shm_info;
};


#ifdef USE_XIM

struct _GdkICPrivate
{
  XIC xic;
  GdkICAttr *attr;
  GdkICAttributesType mask;
};

#endif /* USE_XIM */

#define GDK_ROOT_WINDOW()             gdk_root_window
#define GDK_ROOT_PARENT()             ((GdkWindow *)gdk_parent_root)
#define GDK_DISPLAY()                 gdk_display
#define GDK_DRAWABLE_XDISPLAY(win)    (GDK_DRAWABLE_XDATA(win)->xdisplay)
#define GDK_DRAWABLE_XID(win)         (GDK_DRAWABLE_XDATA(win)->xid)
#define GDK_IMAGE_XDISPLAY(image)     (((GdkImagePrivate*) image)->xdisplay)
#define GDK_IMAGE_XIMAGE(image)       (((GdkImagePrivate*) image)->ximage)
#define GDK_GC_XDISPLAY(gc)           (GDK_GC_XDATA(gc)->xdisplay)
#define GDK_COLORMAP_XDISPLAY(cmap)   (((GdkColormapPrivateX *)cmap)->xdisplay)
#define GDK_COLORMAP_XCOLORMAP(cmap)  (((GdkColormapPrivateX *)cmap)->xcolormap)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate*) vis)->xvisual)
#define GDK_FONT_XDISPLAY(font)       (((GdkFontPrivate*) font)->xdisplay)
#define GDK_FONT_XFONT(font)          (((GdkFontPrivateX *)font)->xfont)

#define GDK_GC_XGC(gc)       (GDK_GC_XDATA(gc)->xgc)
#define GDK_GC_GET_XGC(gc)   (GDK_GC_XDATA(gc)->dirty_mask ? _gdk_x11_gc_flush (gc) : GDK_GC_XGC (gc))

#define GDK_WINDOW_XWINDOW            GDK_DRAWABLE_XID
#define GDK_WINDOW_XDISPLAY           GDK_DRAWABLE_XDISPLAY

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

#define gdk_window_lookup(xid)	   ((GdkWindow*) gdk_xid_table_lookup (xid))
#define gdk_pixmap_lookup(xid)	   ((GdkPixmap*) gdk_xid_table_lookup (xid))
#define gdk_font_lookup(xid)	   ((GdkFont*) gdk_xid_table_lookup (xid))

GC _gdk_x11_gc_flush (GdkGC *gc);

#endif /* __GDK_X_H__ */
