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

#ifndef __GDK_WIN32_H__
#define __GDK_WIN32_H__

#include <gdk/gdkprivate.h>
#include <gdk/gdkcursor.h>

#define STRICT			/* We want strict type checks */
#include <windows.h>
#include <commctrl.h>

/* Make up for some minor w32api header lossage */

/* PS_JOIN_MASK is missing */
#ifndef PS_JOIN_MASK
#define PS_JOIN_MASK (PS_JOIN_BEVEL|PS_JOIN_MITER|PS_JOIN_ROUND)
#endif

/* CLR_INVALID is missing */
#ifndef CLR_INVALID
#define CLR_INVALID CLR_NONE
#endif

/* Some charsets are missing */
#ifndef JOHAB_CHARSET
#define JOHAB_CHARSET 130
#endif
#ifndef VIETNAMESE_CHARSET
#define VIETNAMESE_CHARSET 163
#endif

#ifndef FS_VIETNAMESE
#define FS_VIETNAMESE 0x100
#endif

#ifndef VM_OEM_PLUS
#define VK_OEM_PLUS 0xBB
#endif


/* Missing messages */
#ifndef WM_GETOBJECT
#define WM_GETOBJECT 0x3D
#endif
#ifndef WM_NCXBUTTONDOWN
#define WM_NCXBUTTONDOWN 0xAB
#endif
#ifndef WM_NCXBUTTONUP
#define WM_NCXBUTTONUP 0xAC
#endif
#ifndef WM_NCXBUTTONDBLCLK
#define WM_NCXBUTTONDBLCLK 0xAD
#endif
#ifndef WM_MENURBUTTONUP
#define WM_MENURBUTTONUP 0x122
#endif
#ifndef WM_MENUDRAG
#define WM_MENUDRAG 0x123
#endif
#ifndef WM_MENUGETOBJECT
#define WM_MENUGETOBJECT 0x124
#endif
#ifndef WM_UNINITMENUPOPUP
#define WM_UNINITMENUPOPUP 0x125
#endif
#ifndef WM_MENUCOMMAND
#define WM_MENUCOMMAND 0x126
#endif
#ifndef WM_CHANGEUISTATE
#define WM_CHANGEUISTATE 0x127
#endif
#ifndef WM_UPDATEUISTATE
#define WM_UPDATEUISTATE 0x128
#endif
#ifndef WM_QUERYUISTATE
#define WM_QUERYUISTATE 0x129
#endif
#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN 0x20B
#endif
#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP 0x20C
#endif
#ifndef WM_XBUTTONDBLCLK
#define WM_XBUTTONDBLCLK 0x20D
#endif
#ifndef WM_IME_REQUEST
#define WM_IME_REQUEST 0x288
#endif
#ifndef WM_MOUSEHOVER
#define WM_MOUSEHOVER 0x2A1
#endif
#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE 0x2A3
#endif
#ifndef WM_NCMOUSEHOVER
#define WM_NCMOUSEHOVER 0x2A0
#endif
#ifndef WM_NCMOUSELEAVE
#define WM_NCMOUSELEAVE 0x2A2
#endif
#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND 0x319
#endif
#ifndef WM_HANDHELDFIRST
#define WM_HANDHELDFIRST 0x358
#endif
#ifndef WM_HANDHELDLAST
#define WM_HANDHELDLAST 0x35F
#endif
#ifndef WM_AFXFIRST
#define WM_AFXFIRST 0x360
#endif
#ifndef WM_AFXLAST
#define WM_AFXLAST 0x37F
#endif

#include <gdk/gdkprivate.h>

/* Define corresponding Windows types for some X11 types, just for laziness. */
typedef PALETTEENTRY XColor;

/* Define some of the X11 constants also here, again just for laziness */

/* Error codes */
#define Success            0

/* Grabbing status */
#define GrabSuccess	   0
#define AlreadyGrabbed	   2

/* Some structs are somewhat useful to emulate internally, just to
 * keep the code less #ifdefed.
 */
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

typedef struct _GdkGCWin32Data          GdkGCWin32Data;
typedef struct _GdkDrawableWin32Data    GdkDrawableWin32Data;
typedef struct _GdkWindowWin32Data      GdkWindowWin32Data;
typedef struct _GdkWin32PositionInfo    GdkWin32PositionInfo;
typedef struct _GdkColormapPrivateWin32 GdkColormapPrivateWin32;
typedef struct _GdkCursorPrivate        GdkCursorPrivate;
typedef struct _GdkWin32SingleFont      GdkWin32SingleFont;
typedef struct _GdkFontPrivateWin32     GdkFontPrivateWin32;
typedef struct _GdkImagePrivateWin32    GdkImagePrivateWin32;
typedef struct _GdkVisualPrivate        GdkVisualPrivate;
typedef struct _GdkRegionPrivate        GdkRegionPrivate;
typedef struct _GdkICPrivate            GdkICPrivate;

#define GDK_DRAWABLE_WIN32DATA(win) ((GdkDrawableWin32Data *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_WINDOW_WIN32DATA(win) ((GdkWindowWin32Data *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_GC_WIN32DATA(gc) ((GdkGCWin32Data *)(((GdkGCPrivate*)(gc))->klass_data))

struct _GdkGCWin32Data
{
  GdkRegion *clip_region;

  /* A Windows Device Context (DC) is not equivalent to an X11
   * GC. We can use a DC only in the window for which it was
   * allocated, or (in the case of a memory DC) with the bitmap that
   * has been selected into it. Thus, we have to release and
   * reallocate a DC each time the GdkGC is used to paint into a new
   * window or pixmap. We thus keep all the necessary values in the
   * GdkGCWin32Data struct.
   */
  HDC xgc;
  HRGN hcliprgn;
  GdkGCValuesMask values_mask;
  gulong foreground;		/* Pixel values from GdkColor, */
  gulong background;		/* not Win32 COLORREFs */
  GdkFont *font;
  gint rop2;
  GdkFill fill_style;
  GdkPixmap *tile;
  GdkPixmap *stipple;
  GdkSubwindowMode subwindow_mode;
  gint graphics_exposures;
  gint pen_width;
  DWORD pen_style;
  HANDLE hwnd;			/* If a DC is allocated, for which window
				 * or what bitmap is selected into it
				 */
  int saved_dc;
};

struct _GdkDrawableWin32Data
{
  HANDLE xid;
};

struct _GdkWin32PositionInfo
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x_offset;		/* Offsets to add to Win32 coordinates */
  gint y_offset;		/* within window to get GDK coodinates */
  gboolean big : 1;
  gboolean mapped : 1;
  gboolean no_bg : 1;	        /* Set when the window background is 
				 * temporarily unset during resizing
				 * and scaling */
  GdkRectangle clip_rect;	/* visible rectangle of window */
};

struct _GdkWindowWin32Data
{
  GdkDrawableWin32Data drawable;

  GdkWin32PositionInfo position_info;

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
  gulong bg_pixel;
  GdkPixmap *bg_pixmap;

  HCURSOR xcursor;

  /* Window size hints */
  gint hint_flags;
  gint hint_x, hint_y;
  gint hint_min_width, hint_min_height;
  gint hint_max_width, hint_max_height;

  gboolean extension_events_selected;

  HKL input_locale;
  CHARSETINFO charset_info;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  HCURSOR xcursor;
};

struct _GdkWin32SingleFont
{
  HFONT xfont;
  UINT charset;
  UINT codepage;
  FONTSIGNATURE fs;
};

struct _GdkFontPrivateWin32
{
  GdkFontPrivate base;
  GSList *fonts;		/* List of GdkWin32SingleFonts */
  GSList *names;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
  Visual *xvisual;
};

struct _GdkColormapPrivateWin32
{
  GdkColormapPrivate base;
  Colormap xcolormap;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  DWORD last_sync_time;
};

struct _GdkImagePrivateWin32
{
  GdkImagePrivate base;
  HBITMAP ximage;
};

#define GDK_ROOT_WINDOW()             ((guint32) HWND_DESKTOP)
#define GDK_ROOT_PARENT()             ((GdkWindow *) gdk_parent_root)
#define GDK_DISPLAY()                 NULL
#define GDK_DRAWABLE_XID(win)         (GDK_DRAWABLE_WIN32DATA(win)->xid)
#define GDK_IMAGE_XIMAGE(image)       (((GdkImagePrivate *) image)->ximage)
#define GDK_COLORMAP_XDISPLAY(cmap)   NULL
#define GDK_COLORMAP_WIN32COLORMAP(cmap)(((GdkColormapPrivateWin32 *) cmap)->xcolormap)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate *) vis)->xvisual)

#define GDK_WINDOW_XWINDOW	      GDK_DRAWABLE_XID
#define GDK_WINDOW_XDISPLAY	      GDK_DRAWABLE_XDISPLAY

GDKVAR gchar		*gdk_progclass;
GDKVAR ATOM		 gdk_selection_property;

/* Functions to create GDK pixmaps and windows from their native equivalents */
GdkPixmap    *gdk_pixmap_foreign_new (guint32     anid);
GdkWindow    *gdk_window_foreign_new (guint32     anid);

/* Return the Gdk* for a particular HANDLE */
gpointer      gdk_xid_table_lookup     (HANDLE handle);

#endif /* __GDK_WIN32_H__ */
