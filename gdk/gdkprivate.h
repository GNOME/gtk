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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GDK_PRIVATE_H__
#define __GDK_PRIVATE_H__


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdktypes.h>

#define DND_PROTOCOL_VERSION 0

#define gdk_window_lookup(xid)     ((GdkWindow*) gdk_xid_table_lookup (xid))
#define gdk_pixmap_lookup(xid)     ((GdkPixmap*) gdk_xid_table_lookup (xid))
#define gdk_font_lookup(xid)       ((GdkFont*) gdk_xid_table_lookup (xid))


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct _GdkWindowPrivate       GdkWindowPrivate;
typedef struct _GdkWindowPrivate       GdkPixmapPrivate;
typedef struct _GdkImagePrivate        GdkImagePrivate;
typedef struct _GdkGCPrivate           GdkGCPrivate;
typedef struct _GdkColormapPrivate     GdkColormapPrivate;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;
typedef struct _GdkFontPrivate         GdkFontPrivate;
typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkColorContextPrivate GdkColorContextPrivate;


struct _GdkWindowPrivate
{
  GdkWindow window;
  GdkWindow *parent;
  Window xwindow;
  Display *xdisplay;
  gint16 x;
  gint16 y;
  guint16 width;
  guint16 height;
  guint8 resize_count;
  guint8 ref_count;
  guint8 window_type;
  guint8 destroyed : 2;
  guint8 dnd_drag_enabled : 1,
    dnd_drag_datashow : 1,
    dnd_drag_destructive_op : 1,
    dnd_drag_accepted : 1,
    dnd_drop_enabled : 1,
    dnd_drop_destructive_op : 1;
  GdkAtom dnd_drag_data_type, *dnd_drag_data_typesavail;
  guint dnd_drag_data_numtypesavail;
  /* We have to turn on MotionMask/EnterWindowMask/LeaveWindowMask
     during drags, then set it back to what it was after */
  glong dnd_drag_savedeventmask, dnd_drag_eventmask;
  GdkAtom *dnd_drop_data_typesavail;
  guint dnd_drop_data_numtypesavail;
  /* need to allow custom drag/drop cursors */

  gint extension_events;
};

struct _GdkImagePrivate
{
  GdkImage image;
  XImage *ximage;
  Display *xdisplay;
  gpointer x_shm_info;

  void (*image_put) (GdkDrawable *window,
		     GdkGC       *gc,
		     GdkImage    *image,
		     gint         xsrc,
		     gint         ysrc,
		     gint         xdest,
		     gint         ydest,
		     gint         width,
		     gint         height);
};

struct _GdkGCPrivate
{
  GdkGC gc;
  GC xgc;
  Display *xdisplay;
};

struct _GdkColormapPrivate
{
  GdkColormap colormap;
  Colormap xcolormap;
  Display *xdisplay;
  GdkVisual *visual;
  gint private_val;
  gint next_color;
  gint ref_count;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
  Visual *xvisual;
};

struct _GdkFontPrivate
{
  GdkFont font;
  /* XFontStruct *xfont; */
  /* generic pointer point to XFontStruct or XFontSet */
  gpointer xfont;
  Display *xdisplay;
  gint ref_count;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  Display *xdisplay;
};

struct _GdkDndGlobals {
  GdkAtom            gdk_XdeEnter, gdk_XdeLeave, gdk_XdeRequest;
  GdkAtom            gdk_XdeDataAvailable, gdk_XdeDataShow, gdk_XdeCancel;
  GdkAtom            gdk_XdeTypelist;
  Cursor          gdk_cursor_dragdefault, gdk_cursor_dragok;
  GdkWindow     **drag_startwindows;
  guint           drag_numwindows;
  guint8          drag_really;
  GdkPoint        drag_dropcoords;
};
typedef struct _GdkDndGlobals GdkDndGlobals;


#ifdef USE_XIM

struct _GdkICPrivate
{
  XIC xic;
  GdkIMStyle style;
};
typedef struct _GdkICPrivate GdkICPrivate;

#endif /* USE_XIM */


struct _GdkColorContextPrivate
{
  GdkColorContext color_context;
  Display *xdisplay;
  GdkVisual *visual;
  GdkColormap *colormap;

  gint num_colors;		/* available no. of colors in colormap */
  gint max_colors;		/* maximum no. of colors */
  gint num_allocated;		/* no. of allocated colors */

  GdkColorContextMode mode;
  gint need_to_free_colormap;
  GdkAtom std_cmap_atom;

  XStandardColormap std_cmap;
  gulong *clut;			/* color look-up table */
  GdkColor *cmap;		/* colormap */

  GHashTable *color_hash;	/* hash table of allocated colors */
  GdkColor *palette;		/* preallocated palette */
  gint num_palette;		/* size of palette */

  GdkColorContextDither *fast_dither;	/* fast dither matrix */

  struct
  {
    gint red;
    gint green;
    gint blue;
  } shifts;

  struct
  {
    gulong red;
    gulong green;
    gulong blue;
  } masks;


  struct {
    gint red;
    gint green;
    gint blue;
  } bits;

  gulong max_entry;

  gulong black_pixel;
  gulong white_pixel;
};


void gdk_window_init (void);
void gdk_visual_init (void);

void gdk_image_init  (void);
void gdk_image_exit (void);

GdkColormap* gdk_colormap_lookup (Colormap  xcolormap);
GdkVisual*   gdk_visual_lookup   (Visual   *xvisual);

void gdk_window_real_destroy         (GdkWindow *window);
void gdk_window_add_colormap_windows (GdkWindow *window);

void     gdk_xid_table_insert (XID      *xid,
			       gpointer  data);
void     gdk_xid_table_remove (XID       xid);
gpointer gdk_xid_table_lookup (XID       xid);


extern gint              gdk_debug_level;
extern gint              gdk_show_events;
extern gint              gdk_use_xshm;
extern gint              gdk_stack_trace;
extern gchar            *gdk_display_name;
extern Display          *gdk_display;
extern gint              gdk_screen;
extern Window            gdk_root_window;
extern Window            gdk_leader_window;
extern GdkWindowPrivate  gdk_root_parent;
extern Atom              gdk_wm_delete_window;
extern Atom              gdk_wm_take_focus;
extern Atom              gdk_wm_protocols;
extern Atom              gdk_wm_window_protocols[];
extern Atom              gdk_selection_property;
extern GdkDndGlobals     gdk_dnd;
extern GdkWindow        *selection_owner[];
extern gchar            *gdk_progname;
extern gchar            *gdk_progclass;
extern gint              gdk_error_code;
extern gint              gdk_error_warnings;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_PRIVATE_H__ */
