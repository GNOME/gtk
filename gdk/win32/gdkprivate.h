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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_PRIVATE_H__
#define __GDK_PRIVATE_H__

#define STRICT			/* We want strict type checks */
#include <windows.h>

/* Make up for some minor mingw32 lossage */

/* PS_JOIN_MASK is missing from the mingw32 headers */
#ifndef PS_JOIN_MASK
#define PS_JOIN_MASK (PS_JOIN_BEVEL|PS_JOIN_MITER|PS_JOIN_ROUND)
#endif

/* CLR_INVALID is missing */
#ifndef CLR_INVALID
#define CLR_INVALID CLR_NONE
#endif

/* MB_CUR_MAX is missing */
#ifndef MB_CUR_MAX
extern int *__imp___mb_cur_max;
#define MB_CUR_MAX (*__imp___mb_cur_max)
#endif

#include <time.h>

#include <gdk/gdktypes.h>

#include <gdk/gdkcursor.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkfont.h>
#include <gdk/gdkgc.h>
#include <gdk/gdkim.h>
#include <gdk/gdkimage.h>
#include <gdk/gdkregion.h>
#include <gdk/gdkvisual.h>
#include <gdk/gdkwindow.h>

#define GDK_DRAWABLE_TYPE(d) (((GdkDrawablePrivate *)d)->window_type)
#define GDK_IS_WINDOW(d) (GDK_DRAWABLE_TYPE(d) <= GDK_WINDOW_TEMP || \
                          GDK_DRAWABLE_TYPE(d) == GDK_WINDOW_FOREIGN)
#define GDK_IS_PIXMAP(d) (GDK_DRAWABLE_TYPE(d) == GDK_DRAWABLE_PIXMAP)
#define GDK_DRAWABLE_DESTROYED(d) (((GdkDrawablePrivate *)d)->destroyed)

#define gdk_window_lookup(xid)	   ((GdkWindow*) gdk_xid_table_lookup (xid))
#define gdk_pixmap_lookup(xid)	   ((GdkPixmap*) gdk_xid_table_lookup (xid))

/* HFONTs clash with HWNDs, so add dithering to HFONTs... (hack) */
#define HFONT_DITHER 43
#define gdk_font_lookup(xid)	   ((GdkFont*) gdk_xid_table_lookup ((HANDLE) ((guint) xid + HFONT_DITHER)))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Define corresponding Windows types for some X11 types, just for laziness.
 */

typedef HANDLE XID;
typedef PALETTEENTRY XColor;
typedef HDC GC;
typedef ATOM Atom;
typedef HCURSOR Cursor;
typedef guint VisualID;
typedef DWORD KeySym;
typedef int Status;

/* Define some of the X11 constants also here, again just for laziness */

/* Generic null resource */
#define None 0

/* Error codes */
#define Success            0

/* Grabbing status */
#define GrabSuccess	   0
#define AlreadyGrabbed	   2

/* For CreateColormap */
#define AllocNone 0
#define AllocAll 1

/* Notify modes */
#define NotifyNormal 0
#define NotifyHint 1

/* Some structs are somewhat useful to emulate internally, just to
   keep the code less #ifdefed.  */
typedef struct {
  HPALETTE palette;		/* Palette handle used when drawing. */
  guint size;			/* Number of entries in the palette. */
  gboolean stale;		/* 1 if palette needs to be realized,
				 * otherwise 0. */
  gboolean *in_use;
  gboolean rc_palette;		/* If RC_PALETTE is on in the RASTERCAPS */
  gulong sizepalette;		/* SIZEPALETTE if rc_palette */
} ColormapStruct, *Colormap;
  
typedef struct {
  gint map_entries;
  guint visualid;
  guint bitspixel;
} Visual;

typedef struct {
  Colormap colormap;
  unsigned long red_max;
  unsigned long red_mult;
  unsigned long green_max;
  unsigned long green_mult;
  unsigned long blue_max;
  unsigned long blue_mult;
  unsigned long base_pixel;
} XStandardColormap;

typedef struct _GdkDrawablePrivate     GdkDrawablePrivate;
/* typedef struct _GdkDrawablePrivate     GdkPixmapPrivate; */
typedef struct _GdkWindowPrivate       GdkWindowPrivate;
typedef struct _GdkImagePrivate	       GdkImagePrivate;
typedef struct _GdkGCPrivate	       GdkGCPrivate;
typedef struct _GdkColormapPrivate     GdkColormapPrivate;
typedef struct _GdkColorInfo           GdkColorInfo;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;
typedef struct _GdkFontPrivate	       GdkFontPrivate;
typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkEventFilter	       GdkEventFilter;
typedef struct _GdkClientFilter	       GdkClientFilter;
typedef struct _GdkRegionPrivate       GdkRegionPrivate;

struct _GdkDrawablePrivate
{
  GdkDrawable drawable;

  guint8 window_type;
  guint ref_count;

  guint16 width;
  guint16 height;

  HANDLE xwindow;
  GdkColormap *colormap;

  guint destroyed : 2;
};

struct _GdkWindowPrivate
{
  GdkDrawablePrivate drawable;

  GdkWindow *parent;
  gint16 x;
  gint16 y;
  guint8 resize_count;
  guint mapped : 1;
  guint guffaw_gravity : 1;

  /* We must keep the event mask here to filter them ourselves */
  gint event_mask;

  /* Values for bg_type */
#define GDK_WIN32_BG_NORMAL 0
#define GDK_WIN32_BG_PIXEL 1
#define GDK_WIN32_BG_PIXMAP 2
#define GDK_WIN32_BG_PARENT_RELATIVE 3
#define GDK_WIN32_BG_TRANSPARENT 4

  /* We draw the background ourselves at WM_ERASEBKGND  */
  guchar bg_type;
  GdkColor bg_pixel;
  GdkPixmap *bg_pixmap;

  HCURSOR xcursor;

  /* Window size hints */
  gint hint_flags;
  gint hint_x, hint_y;
  gint hint_min_width, hint_min_height;
  gint hint_max_width, hint_max_height;

  gint extension_events;
  gboolean extension_events_selected;

  GList *filters;
  GList *children;
};

struct _GdkImagePrivate
{
  GdkImage image;
  HBITMAP ximage;
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
  /* A Windows Device Context (DC) is not equivalent to an X11
   * GC. We can use a DC only in the window for which it was
   * allocated, or (in the case of a memory DC) with the bitmap that
   * has been selected into it. Thus, we have to release and
   * reallocate a DC each time the GdkGC is used to paint into a new
   * window or pixmap. We thus keep all the necessary values in the
   * GdkGCPrivate struct.
   */
  GdkGCValuesMask values_mask;
  GdkColor foreground;
  GdkColor background;
  HFONT font;
  gint rop2;
  GdkFill fill_style;
  GdkPixmap *tile;
  GdkPixmap *stipple;
  HRGN clip_region;
  GdkSubwindowMode subwindow_mode;
  gint ts_x_origin;
  gint ts_y_origin;
  gint clip_x_origin;
  gint clip_y_origin;
  gint graphics_exposures;
  gint pen_width;
  DWORD pen_style;
  HANDLE hwnd;			/* If a DC is allocated, for which window
				   or what bitmap is selected into it */
  int saved_dc;
  guint ref_count;
};

typedef enum {
  GDK_COLOR_WRITEABLE = 1 << 0
} GdkColorInfoFlags;

struct _GdkColorInfo
{
  GdkColorInfoFlags flags;
  guint ref_count;
};

struct _GdkColormapPrivate
{
  GdkColormap colormap;
  Colormap xcolormap;
  GdkVisual *visual;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  time_t last_sync_time;
  
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
  HFONT xfont;
  guint ref_count;

  GSList *names;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
};

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
};

struct _GdkClientFilter {
  GdkAtom       type;
  GdkFilterFunc function;
  gpointer      data;
};

struct _GdkRegionPrivate
{
  GdkRegion region;
  HRGN xregion;
};

typedef enum {
  GDK_DEBUG_MISC          = 1 << 0,
  GDK_DEBUG_EVENTS        = 1 << 1,
  GDK_DEBUG_DND           = 1 << 2,
  GDK_DEBUG_COLOR_CONTEXT = 1 << 3,
  GDK_DEBUG_XIM           = 1 << 4,
  GDK_DEBUG_SELECTION	  = 1 << 5
} GdkDebugFlag;

void gdk_events_init (void);
void gdk_window_init (void);
void gdk_visual_init (void);
void gdk_selection_init (void);
void gdk_dnd_init    (void);
void gdk_dnd_exit    (void);
void gdk_image_init  (void);
void gdk_image_exit  (void);

GdkColormap* gdk_colormap_lookup (Colormap  xcolormap);
GdkVisual*   gdk_visual_lookup	 (Visual   *xvisual);

void gdk_window_add_colormap_windows (GdkWindow *window);
void gdk_window_destroy_notify	     (GdkWindow *window);

void	 gdk_xid_table_insert (XID	*xid,
			       gpointer	 data);
void	 gdk_xid_table_remove (XID	 xid);
gpointer gdk_xid_table_lookup (XID	 xid);

/* Internal functions */

HDC	gdk_gc_predraw  (GdkDrawablePrivate *drawable_private,
			 GdkGCPrivate       *gc_private);
void	gdk_gc_postdraw (GdkDrawablePrivate *drawable_private,
			 GdkGCPrivate       *gc_private);
HRGN	BitmapToRegion  (HBITMAP hBmp);

void    gdk_sel_prop_store (GdkWindow *owner,
			    GdkAtom    type,
			    gint       format,
			    guchar    *data,
			    gint       length);

void       gdk_event_queue_append (GdkEvent *event);

/* Please see gdkwindow.c for comments on how to use */ 
HWND gdk_window_xid_at(HWND base, gint bx, gint by, gint x, gint y, GList *excludes, gboolean excl_child);
HWND gdk_window_xid_at_coords(gint x, gint y, GList *excludes, gboolean excl_child);

extern gint		 gdk_debug_level;
extern gint		 gdk_show_events;
extern gint		 gdk_stack_trace;
extern HWND		 gdk_root_window;
extern HWND		 gdk_leader_window;
GDKVAR GdkWindowPrivate	 gdk_root_parent;
GDKVAR Atom		 gdk_selection_property;
extern GdkWindow	*selection_owner[];
GDKVAR gchar		*gdk_progclass;
GDKVAR gint		 gdk_error_code;
GDKVAR gint		 gdk_error_warnings;
GDKVAR gint              gdk_null_window_warnings;
extern GList            *gdk_default_filters;
extern gint		 gdk_event_func_from_window_proc;

extern HDC		 gdk_DC;
extern HINSTANCE	 gdk_DLLInstance;
extern HINSTANCE	 gdk_ProgInstance;

extern UINT		 gdk_selection_notify_msg;
extern UINT		 gdk_selection_request_msg;
extern UINT		 gdk_selection_clear_msg;
extern GdkAtom		 gdk_clipboard_atom;
extern GdkAtom		 gdk_win32_dropfiles_atom;
extern GdkAtom		 gdk_ole2_dnd_atom;

extern LRESULT CALLBACK gdk_WindowProc (HWND, UINT, WPARAM, LPARAM);

/* Debugging support */

#ifdef G_ENABLE_DEBUG

#define GDK_NOTE(type,action)		     G_STMT_START { \
    if (gdk_debug_flags & GDK_DEBUG_##type)		    \
       { action; };			     } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GDK_NOTE(type,action)
      
#endif /* G_ENABLE_DEBUG */

GDKVAR guint gdk_debug_flags;

/* Internal functions for debug output etc. */

char   *gdk_color_to_string (GdkColor *);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_PRIVATE_H__ */
