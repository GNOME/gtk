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

#ifndef __GDK_NANOX_H__
#define __GDK_NANOX_H__

#include <microwin/nano-X.h>

#include <gdk/gdkfont.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkcursor.h>

typedef struct _GdkGCXData          GdkGCXData;
typedef struct _GdkDrawableXData    GdkDrawableXData;
typedef struct _GdkColormapPrivateX GdkColormapPrivateX;
typedef struct _GdkCursorPrivate    GdkCursorPrivate;
typedef struct _GdkFontPrivateX     GdkFontPrivateX;
typedef struct _GdkImagePrivateX    GdkImagePrivateX;
typedef struct _GdkVisualPrivate    GdkVisualPrivate;
typedef struct _GdkRegionPrivate    GdkRegionPrivate;

#define GDK_DRAWABLE_XDATA(win) ((GdkDrawableXData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_GC_XDATA(gc) ((GdkGCXData *)(((GdkGCPrivate*)(gc))->klass_data))

struct _GdkGCXData
{
  GR_GC_ID xgc;
};

struct _GdkDrawableXData
{
  GR_WINDOW_ID xid;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  int width, height;
  int hotx, hoty;
  GR_COLOR fg, bg;
  GR_BITMAP *fgb;
  GR_BITMAP *bgb;
};

struct _GdkFontPrivateX
{
  GdkFontPrivate base;
  GR_FONTID xfont;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
};

struct _GdkColormapPrivateX
{
  GdkColormapPrivate base;

};

struct _GdkImagePrivateX
{
  GdkImagePrivate base;
  
  gpointer x_shm_info;
};

struct _GdkRegionPrivate
{
  /*GdkRegion region;*/
  MWCLIPREGION* xregion;
};

GdkGC *       _gdk_nanox_gc_new          (GdkDrawable     *drawable,
					GdkGCValues     *values,
					GdkGCValuesMask  values_mask);
void          gdk_xid_table_insert     (guint             *xid,
					gpointer         data);
void          gdk_xid_table_remove     (guint              xid);
gpointer      gdk_xid_table_lookup     (guint              xid);
/*
gint          gdk_send_xevent          (Window           window,
					gboolean         propagate,
					glong            event_mask,
					XEvent          *event_send);
GdkColormap * gdk_colormap_lookup      (Colormap         xcolormap);
GdkVisual *   gdk_visual_lookup        (Visual          *xvisual);
*/
/* Please see gdkwindow.c for comments on how to use */ 
/*Window gdk_window_xid_at        (Window    base,
				 gint      bx,
				 gint      by,
				 gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);
Window gdk_window_xid_at_coords (gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);
*/

extern GdkDrawableClass  _gdk_nanox_drawable_class;
extern gboolean	         gdk_use_xshm;
extern gchar		*gdk_display_name;
extern GR_WINDOW_ID		 gdk_root_window;
extern GR_WINDOW_ID		 gdk_leader_window;
/*extern Atom		 gdk_wm_delete_window;
extern Atom		 gdk_wm_take_focus;
extern Atom		 gdk_wm_protocols;
extern Atom		 gdk_wm_window_protocols[];*/
extern guint		 gdk_selection_property;
extern GdkWindow	*selection_owner[];
extern gchar		*gdk_progclass;
extern gboolean          gdk_null_window_warnings;
extern const int         gdk_nevent_masks;
extern const int         gdk_event_mask_table[];

extern GdkWindowPrivate *gdk_xgrab_window;  /* Window that currently holds the
					     * x pointer grab
					     */

#define GDK_ROOT_WINDOW()             gdk_root_window
#define GDK_ROOT_PARENT()             ((GdkWindow *)&gdk_parent_root)
#define GDK_DISPLAY()                 NULL
#define GDK_DRAWABLE_XDISPLAY(win)    NULL
#define GDK_DRAWABLE_XID(win)         (GDK_DRAWABLE_XDATA(win)->xid)
#define GDK_IMAGE_XDISPLAY(image)     NULL
#define GDK_IMAGE_XIMAGE(image)       (((GdkImagePrivate*) image)->ximage)
#define GDK_GC_XDISPLAY(gc)           NULL
#define GDK_GC_XGC(gc)                (GDK_GC_XDATA(gc)->xgc)
#define GDK_COLORMAP_XDISPLAY(cmap)   NULL
#define GDK_COLORMAP_XCOLORMAP(cmap)  (((GdkColormapPrivateX *)cmap)->xcolormap)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate*) vis)->xvisual)
#define GDK_FONT_XDISPLAY(font)       NULL
#define GDK_FONT_XFONT(font)          (((GdkFontPrivateX *)font)->xfont)

#define GDK_WINDOW_XWINDOW            GDK_DRAWABLE_XID
#define GDK_WINDOW_XDISPLAY           GDK_DRAWABLE_XDISPLAY

/*GdkVisual*   gdkx_visual_get   (VisualID xvisualid);*/
/* XXX: Do not use this function until it is fixed. An X Colormap
 *      is useless unless we also have the visual. */
/*GdkColormap* gdkx_colormap_get (Colormap xcolormap);*/
/* Utility function in gdk.c - not sure where it belongs, but it's
   needed in more than one place, so make it public */
/*Window        gdk_get_client_window      (Display  *dpy,
                                          Window    win);*/

/* Functions to create pixmaps and windows from their X equivalents */
/*GdkPixmap    *gdk_pixmap_foreign_new (guint32 anid);
GdkWindow    *gdk_window_foreign_new (guint32 anid);*/

#endif /* __GDK_NANOX_H__ */
