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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GDK_PRIVATE_H__
#define __GDK_PRIVATE_H__


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdktypes.h>

#define DND_PROTOCOL_VERSION 0

#define gdk_window_lookup(xid)	   ((GdkWindow*) gdk_xid_table_lookup (xid))
#define gdk_pixmap_lookup(xid)	   ((GdkPixmap*) gdk_xid_table_lookup (xid))
#define gdk_font_lookup(xid)	   ((GdkFont*) gdk_xid_table_lookup (xid))


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct _GdkWindowPrivate       GdkWindowPrivate;
typedef struct _GdkWindowPrivate       GdkPixmapPrivate;
typedef struct _GdkImagePrivate	       GdkImagePrivate;
typedef struct _GdkGCPrivate	       GdkGCPrivate;
typedef struct _GdkColormapPrivate     GdkColormapPrivate;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;
typedef struct _GdkFontPrivate	       GdkFontPrivate;
typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkEventFilter	       GdkEventFilter;
typedef struct _GdkColorContextPrivate GdkColorContextPrivate;
typedef struct _GdkRegionPrivate       GdkRegionPrivate;


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
  guint8 window_type;
  guint ref_count;
  guint destroyed : 2;
  guint dnd_drag_enabled : 1,
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

  GList *filters;
  GdkColormap *colormap;
  GList *children;
};

struct _GdkImagePrivate
{
  GdkImage image;
  XImage *ximage;
  Display *xdisplay;
  gpointer x_shm_info;

  void (*image_put) (GdkDrawable *window,
		     GdkGC	 *gc,
		     GdkImage	 *image,
		     gint	  xsrc,
		     gint	  ysrc,
		     gint	  xdest,
		     gint	  ydest,
		     gint	  width,
		     gint	  height);
};

struct _GdkGCPrivate
{
  GdkGC gc;
  GC xgc;
  Display *xdisplay;
  guint ref_count;
};

struct _GdkColormapPrivate
{
  GdkColormap colormap;
  Colormap xcolormap;
  Display *xdisplay;
  GdkVisual *visual;
  gint private_val;
  gint next_color;
  guint ref_count;
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
  guint ref_count;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  Display *xdisplay;
};

struct _GdkDndCursorInfo {
  Cursor	  gdk_cursor_dragdefault, gdk_cursor_dragok;
  GdkWindow	 *drag_pm_default, *drag_pm_ok;
  GdkPoint	  default_hotspot, ok_hotspot;
  GList *xids;
};
typedef struct _GdkDndCursorInfo GdkDndCursorInfo;

struct _GdkDndGlobals {
  GdkAtom	     gdk_XdeEnter, gdk_XdeLeave, gdk_XdeRequest;
  GdkAtom	     gdk_XdeDataAvailable, gdk_XdeDataShow, gdk_XdeCancel;
  GdkAtom	     gdk_XdeTypelist;

  GdkDndCursorInfo  *c;
  GdkWindow	**drag_startwindows;
  guint		  drag_numwindows;
  gboolean	  drag_really, drag_perhaps, dnd_grabbed;
  Window	  dnd_drag_target;
  GdkPoint	  drag_dropcoords;

  GdkPoint dnd_drag_start, dnd_drag_oldpos;
  GdkRectangle dnd_drag_dropzone;
  GdkWindowPrivate *real_sw;
  Window dnd_drag_curwin;
  Time last_drop_time; /* An incredible hack, sosumi miguel */
};
typedef struct _GdkDndGlobals GdkDndGlobals;

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
};

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
  XStandardColormap std_cmap;
};

struct _GdkRegionPrivate
{
  GdkRegion region;
  Region xregion;
};

typedef enum {
  GDK_DEBUG_MISC = 1<<0,
  GDK_DEBUG_EVENTS = 1 << 1,
  GDK_DEBUG_DND = 1<<2,
  GDK_DEBUG_COLOR_CONTEXT = 1<<3,
  GDK_DEBUG_XIM = 1<<4
} GdkDebugFlag;

void gdk_window_init (void);
void gdk_visual_init (void);

void gdk_image_init  (void);
void gdk_image_exit (void);

GdkColormap* gdk_colormap_lookup (Colormap  xcolormap);
GdkVisual*   gdk_visual_lookup	 (Visual   *xvisual);

void gdk_window_add_colormap_windows (GdkWindow *window);
void gdk_window_destroy_notify	     (GdkWindow *window);

void	 gdk_xid_table_insert (XID	*xid,
			       gpointer	 data);
void	 gdk_xid_table_remove (XID	 xid);
gpointer gdk_xid_table_lookup (XID	 xid);

gint gdk_send_xevent (Window window, gboolean propagate, glong event_mask,
		      XEvent *event_send);

/* If you pass x = y = -1, it queries the pointer
   to find out where it currently is.
   If you pass x = y = -2, it does anything necessary
   to know that the drag is ending.
*/
void gdk_dnd_display_drag_cursor(gint x,
				 gint y,
				 gboolean drag_ok,
				 gboolean change_made);

/* Please see gdkwindow.c for comments on how to use */ 
Window gdk_window_xid_at(Window base, gint bx, gint by, gint x, gint y, GList *excludes, gboolean excl_child);
Window gdk_window_xid_at_coords(gint x, gint y, GList *excludes, gboolean excl_child);

extern gint		 gdk_debug_level;
extern gint		 gdk_show_events;
extern gint		 gdk_use_xshm;
extern gint		 gdk_stack_trace;
extern gchar		*gdk_display_name;
extern Display		*gdk_display;
extern gint		 gdk_screen;
extern Window		 gdk_root_window;
extern Window		 gdk_leader_window;
extern GdkWindowPrivate	 gdk_root_parent;
extern Atom		 gdk_wm_delete_window;
extern Atom		 gdk_wm_take_focus;
extern Atom		 gdk_wm_protocols;
extern Atom		 gdk_wm_window_protocols[];
extern Atom		 gdk_selection_property;
extern GdkDndGlobals	 gdk_dnd;
extern GdkWindow	*selection_owner[];
extern gchar		*gdk_progname;
extern gchar		*gdk_progclass;
extern gint		 gdk_error_code;
extern gint		 gdk_error_warnings;
extern gint              gdk_null_window_warnings;
extern GList            *gdk_default_filters;

/* Debugging support */

#ifdef G_ENABLE_DEBUG

#define GDK_NOTE(type,action)		     G_STMT_START { \
    if (gdk_debug_flags & GDK_DEBUG_##type)		    \
       { action; };			     } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GDK_NOTE(type,action)
      
#endif /* G_ENABLE_DEBUG */

extern guint gdk_debug_flags;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_PRIVATE_H__ */
