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

#ifndef __GDK_WIN32_H__
#define __GDK_WIN32_H__

#include <gdk/gdkprivate.h>
#include <gdk/gdkcursor.h>

#ifndef STRICT
#define STRICT			/* We want strict type checks */
#endif
#include <windows.h>
#include <commctrl.h>

#include <gdk/win32/gdkwindow-win32.h>
#include <gdk/win32/gdkpixmap-win32.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

/* Some virtual keycodes are missing */
#ifndef VM_OEM_PLUS
#define VK_OEM_PLUS 0xBB
#endif

#ifndef VK_OEM_COMMA
#define VK_OEM_COMMA 0xBC
#endif

#ifndef VK_OEM_MINUS
#define VK_OEM_MINUS 0xBD
#endif

#ifndef VK_OEM_PERIOD
#define VK_OEM_PERIOD 0xBE
#endif

#ifndef VK_OEM_1
#define VK_OEM_1 0xBA
#endif
#ifndef VK_OEM_2
#define VK_OEM_2 0xBF
#endif
#ifndef VK_OEM_3
#define VK_OEM_3 0xC0
#endif
#ifndef VK_OEM_4
#define VK_OEM_4 0xDB
#endif
#ifndef VK_OEM_5
#define VK_OEM_5 0xDC
#endif
#ifndef VK_OEM_6
#define VK_OEM_6 0xDD
#endif
#ifndef VK_OEM_7
#define VK_OEM_7 0xDE
#endif
#ifndef VK_OEM_8
#define VK_OEM_8 0xDF
#endif

/* Missing messages */
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0X20A
#endif
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

#ifndef CopyCursor
#define CopyCursor(pcur) ((HCURSOR)CopyIcon((HICON)(pcur)))
#endif

#include <gdk/gdkprivate.h>

/* Define corresponding Windows types for some X11 types, just for laziness. */
typedef PALETTEENTRY XColor;

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

typedef struct _GdkColormapPrivateWin32 GdkColormapPrivateWin32;
typedef struct _GdkCursorPrivate        GdkCursorPrivate;
typedef struct _GdkWin32SingleFont      GdkWin32SingleFont;
typedef struct _GdkFontPrivateWin32     GdkFontPrivateWin32;
typedef struct _GdkImagePrivateWin32    GdkImagePrivateWin32;
typedef struct _GdkVisualPrivate        GdkVisualPrivate;

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  HCURSOR hcursor;
};

struct _GdkWin32SingleFont
{
  HFONT hfont;
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
  Colormap xcolormap;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  DWORD last_sync_time;
};

struct _GdkImagePrivateWin32
{
  HBITMAP hbitmap;
};

typedef struct _GdkGCWin32      GdkGCWin32;
typedef struct _GdkGCWin32Class GdkGCWin32Class;

#define GDK_TYPE_GC_WIN32              (gdk_gc_win32_get_type ())
#define GDK_GC_WIN32(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_WIN32, GdkGCWin32))
#define GDK_GC_WIN32_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_WIN32, GdkGCWin32Class))
#define GDK_IS_GC_WIN32(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_WIN32))
#define GDK_IS_GC_WIN32_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_WIN32))
#define GDK_GC_WIN32_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_WIN32, GdkGCWin32Class))

struct _GdkGCWin32
{
  GdkGC parent_instance;

  /* A Windows Device Context (DC) is not equivalent to an X11
   * GC. We can use a DC only in the window for which it was
   * allocated, or (in the case of a memory DC) with the bitmap that
   * has been selected into it. Thus, we have to release and
   * reallocate a DC each time the GdkGC is used to paint into a new
   * window or pixmap. We thus keep all the necessary values in the
   * GdkGCWin32 object.
   */
  HDC hdc;

  GdkRegion *clip_region;
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
  HANDLE hwnd;			/* If a HDC is allocated, for which window,
				 * or what bitmap is selected into it
				 */
  int saved_dc;
};

struct _GdkGCWin32Class
{
  GdkGCClass parent_class;
};

GType gdk_gc_win32_get_type (void);

#define GDK_ROOT_WINDOW()             ((guint32) HWND_DESKTOP)
#define GDK_ROOT_PARENT()             ((GdkWindow *) gdk_parent_root)
#define GDK_DISPLAY()                 NULL
#define GDK_WINDOW_HWND(win)          (GDK_DRAWABLE_IMPL_WIN32(((GdkWindowObject *)win)->impl)->handle)
#define GDK_PIXMAP_HBITMAP(pixmap)    (GDK_DRAWABLE_IMPL_WIN32(((GdkPixmapObject *)pixmap)->impl)->handle)
#define GDK_DRAWABLE_HANDLE(win)      (GDK_IS_WINDOW (win) ? (GDK_WINDOW_HWND (win)) : (GDK_PIXMAP_HBITMAP (win)))
#define GDK_IMAGE_HBM(image)          (((GdkImagePrivateWin32 *) GDK_IMAGE (image)->windowing_data)->hbm)
#define GDK_COLORMAP_PRIVATE_DATA(cmap) ((GdkColormapPrivateWin32 *) cmap->windowing_data)
#define GDK_COLORMAP_WIN32COLORMAP(cmap) (((GdkColormapPrivateWin32 *)GDK_COLORMAP (cmap)->windowing_data)->xcolormap)
#define GDK_VISUAL_XVISUAL(vis)       (((GdkVisualPrivate *) vis)->xvisual)

GDKVAR gchar		*gdk_progclass;
GDKVAR ATOM		 gdk_selection_property;

/* Functions to create GDK pixmaps and windows from their native equivalents */
GdkPixmap    *gdk_pixmap_foreign_new (GdkNativeWindow anid);
GdkWindow    *gdk_window_foreign_new (GdkNativeWindow anid);

/* Return the Gdk* for a particular HANDLE */
gpointer      gdk_win32_handle_table_lookup (GdkNativeWindow handle);

#define gdk_window_lookup(hwnd) (GdkWindow*) gdk_win32_handle_table_lookup (hwnd)
#define gdk_pixmap_lookup(hbm)	(GdkPixmap*) gdk_win32_handle_table_lookup (hbm)

/* Return a device context to draw in a drawable, given a GDK GC,
 * and a mask indicating which GC values might be used (for efficiency,
 * no need to muck around with text-related stuff if we aren't going
 * to output text, for instance).
 */
HDC           gdk_win32_hdc_get      (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkGCValuesMask usage);

/* Each HDC returned from gdk_win32_hdc_get must be released with
 * this function
 */
void          gdk_win32_hdc_release  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkGCValuesMask usage);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_WIN32_H__ */
