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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GDK_PRIVATE_WIN32_H__
#define __GDK_PRIVATE_WIN32_H__

#include <gdk/gdkprivate.h>
#include <gdk/win32/gdkwindow-win32.h>
#include <gdk/win32/gdkpixmap-win32.h>

#include "gdkinternals.h"

#include <config.h>

/* Make up for some minor w32api header lossage */

/* PS_JOIN_MASK is missing */
#ifndef PS_JOIN_MASK
#define PS_JOIN_MASK (PS_JOIN_BEVEL|PS_JOIN_MITER|PS_JOIN_ROUND)
#endif

/* CLR_INVALID is missing */
#ifndef CLR_INVALID
#define CLR_INVALID CLR_NONE
#endif

/* Some charsets are missing from MSVC 5 headers */
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
#ifndef WM_SYNCPAINT
#define WM_SYNCPAINT 0x88
#endif
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

/* Define corresponding Windows types for some X11 types, just for laziness. */
typedef PALETTEENTRY XColor;

/* Some structs are somewhat useful to emulate internally. */
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

#define GDK_TYPE_GC_WIN32              (_gdk_gc_win32_get_type ())
#define GDK_GC_WIN32(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_WIN32, GdkGCWin32))
#define GDK_GC_WIN32_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_WIN32, GdkGCWin32Class))
#define GDK_IS_GC_WIN32(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_WIN32))
#define GDK_IS_GC_WIN32_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_WIN32))
#define GDK_GC_WIN32_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_WIN32, GdkGCWin32Class))

typedef struct _GdkColormapPrivateWin32 GdkColormapPrivateWin32;
typedef struct _GdkCursorPrivate        GdkCursorPrivate;
typedef struct _GdkWin32SingleFont      GdkWin32SingleFont;
typedef struct _GdkFontPrivateWin32     GdkFontPrivateWin32;
typedef struct _GdkImagePrivateWin32    GdkImagePrivateWin32;
typedef struct _GdkVisualPrivate        GdkVisualPrivate;
typedef struct _GdkGCWin32      GdkGCWin32;
typedef struct _GdkGCWin32Class GdkGCWin32Class;

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

struct _GdkVisualClass
{
  GObjectClass parent_class;
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

GType _gdk_gc_win32_get_type (void);

/* Routines from gdkgeometry-win32.c */
void
_gdk_window_init_position (GdkWindow *window);
void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height);
void _gdk_window_process_expose    (GdkWindow     *window,
                                    gulong         serial,
                                    GdkRectangle  *area);

/* gdkdrawable-win32.c, background draw helper */
void _gdk_win32_draw_tiles (GdkDrawable *drawable,
                            GdkGC       *gc,
                            GdkPixmap   *tile,
                            gint        x, 
                            gint        y, 
                            gint        width, 
                            gint        height);

void _gdk_win32_selection_init (void);
void _gdk_win32_dnd_exit (void);

void	 gdk_win32_handle_table_insert    (HANDLE   *handle,
					   gpointer data);
void	 gdk_win32_handle_table_remove    (HANDLE handle);

GdkGC *  _gdk_win32_gc_new       (GdkDrawable        *drawable,
				  GdkGCValues        *values,
				  GdkGCValuesMask     values_mask);

GdkImage* _gdk_win32_get_image (GdkDrawable    *drawable,
				gint            x,
				gint            y,
				gint            width,
				gint            height);

GdkImage *_gdk_win32_copy_to_image       (GdkDrawable *drawable,
					  GdkImage    *image,
					  gint         src_x,
					  gint         src_y,
					  gint         dest_x,
					  gint         dest_y,
					  gint         width,
					  gint         height);

COLORREF _gdk_win32_colormap_color      (GdkColormap        *colormap,
				          gulong              pixel);

HRGN	 BitmapToRegion          (HBITMAP hBmp);

gchar  *gdk_font_full_name_get   (GdkFont *font);

void    _gdk_selection_property_store (GdkWindow *owner,
                                       GdkAtom    type,
                                       gint       format,
                                       guchar    *data,
                                       gint       length);

void    _gdk_selection_property_delete (GdkWindow *);

gint    _gdk_win32_nmbstowchar_ts (wchar_t     *dest,
                                   const gchar *src,
                                   gint         src_len,
                                   gint         dest_max);

void    _gdk_wchar_text_handle    (GdkFont       *font,
				   const wchar_t *wcstr,
				   int            wclen,
				   void         (*handler)(GdkWin32SingleFont *,
							   const wchar_t *,
							   int,
							   void *),
				   void          *arg);

#ifdef G_ENABLE_DEBUG
gchar *gdk_win32_color_to_string      (const        GdkColor *color);
gchar *gdk_win32_cap_style_to_string  (GdkCapStyle  cap_style);
gchar *gdk_win32_fill_style_to_string (GdkFill      fill);
gchar *gdk_win32_function_to_string   (GdkFunction  function);
gchar *gdk_win32_join_style_to_string (GdkJoinStyle join_style);
gchar *gdk_win32_line_style_to_string (GdkLineStyle line_style);
gchar *gdk_win32_message_name         (UINT         msg);

#define PING() printf(G_STRLOC),fflush(stdout)
#else
#define PING()
#endif

gchar  *gdk_win32_last_error_string (void);
void    gdk_win32_api_failed        (const gchar *where,
				     gint line,
				     const gchar *api);
void    gdk_other_api_failed        (const gchar *where,
				     gint line,
				     const gchar *api);
void    gdk_win32_gdi_failed        (const gchar *where,
				     gint line,
				     const gchar *api);

#ifdef __GNUC__
#define WIN32_API_FAILED(api) gdk_win32_api_failed (__FILE__ ":" __PRETTY_FUNCTION__, __LINE__, api)
#define WIN32_GDI_FAILED(api) gdk_win32_gdi_failed (__FILE__ ":" __PRETTY_FUNCTION__, __LINE__, api)
#define OTHER_API_FAILED(api) gdk_other_api_failed (__FILE__ ":" __PRETTY_FUNCTION__, __LINE__, api)
#else
#define WIN32_API_FAILED(api) gdk_win32_api_failed (__FILE__, __LINE__, api)
#define WIN32_GDI_FAILED(api) gdk_win32_gdi_failed (__FILE__, __LINE__, api)
#define OTHER_API_FAILED(api) gdk_other_api_failed (__FILE__, __LINE__, api)
#endif
 
extern LRESULT CALLBACK _gdk_win32_window_procedure (HWND, UINT, WPARAM, LPARAM);

extern HWND		 gdk_root_window;
extern gboolean		 gdk_event_func_from_window_proc;

extern HDC		 gdk_display_hdc;
extern HINSTANCE	 gdk_dll_hinstance;
extern HINSTANCE	 gdk_app_hmodule;

extern UINT		 gdk_selection_notify_msg;
extern UINT		 gdk_selection_request_msg;
extern UINT		 gdk_selection_clear_msg;
extern GdkAtom		 gdk_clipboard_atom;
extern GdkAtom		 gdk_win32_dropfiles_atom;
extern GdkAtom		 gdk_ole2_dnd_atom;

extern DWORD		 windows_version;
#define IS_WIN_NT()      (windows_version < 0x80000000)

extern gint		 gdk_input_ignore_wintab;

extern GdkAtom		 _gdk_selection_property;

#define IMAGE_PRIVATE_DATA(image) ((GdkImagePrivateWin32 *) GDK_IMAGE (image)->windowing_data)

#endif /* __GDK_PRIVATE_WIN32_H__ */
