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
#ifndef VK_OEM_PLUS
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

/* Define some combinations of GdkDebugFlags */
#define GDK_DEBUG_EVENTS_OR_COLORMAP (GDK_DEBUG_EVENTS|GDK_DEBUG_COLORMAP)
#define GDK_DEBUG_EVENTS_OR_INPUT (GDK_DEBUG_EVENTS|GDK_DEBUG_INPUT)
#define GDK_DEBUG_PIXMAP_OR_COLORMAP (GDK_DEBUG_PIXMAP|GDK_DEBUG_COLORMAP)
#define GDK_DEBUG_MISC_OR_COLORMAP (GDK_DEBUG_MISC|GDK_DEBUG_COLORMAP)

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
typedef struct _GdkGCWin32		GdkGCWin32;
typedef struct _GdkGCWin32Class		GdkGCWin32Class;

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

typedef enum {
  GDK_WIN32_PE_STATIC,
  GDK_WIN32_PE_AVAILABLE,
  GDK_WIN32_PE_INUSE
} GdkWin32PalEntryState;

struct _GdkColormapPrivateWin32
{
  HPALETTE hpal;
  gint current_size;		/* Current size of hpal */
  GdkWin32PalEntryState *use;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
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

  HRGN hcliprgn;

  GdkGCValuesMask values_mask;

  GdkFont *font;
  gint rop2;
  GdkSubwindowMode subwindow_mode;
  gint graphics_exposures;
  gint pen_width;
  DWORD pen_style;
  GdkLineStyle line_style;
  GdkCapStyle cap_style;
  GdkJoinStyle join_style;
  DWORD *pen_dashes;		/* use for PS_USERSTYLE or step-by-step rendering */
  gint pen_num_dashes;
  gint pen_dash_offset;
  HBRUSH pen_hbrbg;

  /* Following fields are valid while the GC exists as a Windows DC */
  HDC hdc;
  int saved_dc;

  HPALETTE holdpal;
};

struct _GdkGCWin32Class
{
  GdkGCClass parent_class;
};

GType _gdk_gc_win32_get_type (void);

gulong _gdk_win32_get_next_tick (gulong suggested_tick);

void _gdk_window_init_position     (GdkWindow *window);
void _gdk_window_move_resize_child (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height);
void _gdk_window_process_expose    (GdkWindow *window,
                                    GdkRegion *invalidate_region);

void _gdk_win32_selection_init (void);
void _gdk_win32_dnd_exit (void);

void	 gdk_win32_handle_table_insert  (HANDLE   *handle,
					 gpointer data);
void	 gdk_win32_handle_table_remove  (HANDLE handle);

GdkGC    *_gdk_win32_gc_new             (GdkDrawable        *drawable,
					 GdkGCValues        *values,
					 GdkGCValuesMask     values_mask);

GdkImage *_gdk_win32_get_image 		(GdkDrawable *drawable,
					 gint         x,
					 gint         y,
					 gint         width,
					 gint         height);

GdkImage *_gdk_win32_copy_to_image      (GdkDrawable *drawable,
					 GdkImage    *image,
					 gint         src_x,
					 gint         src_y,
					 gint         dest_x,
					 gint         dest_y,
					 gint         width,
					 gint         height);

void      _gdk_win32_blit               (gboolean              use_fg_bg,
					 GdkDrawableImplWin32 *drawable,
					 GdkGC       	       *gc,
					 GdkDrawable   	       *src,
					 gint        	    	xsrc,
					 gint        	    	ysrc,
					 gint        	    	xdest,
					 gint        	    	ydest,
					 gint        	    	width,
					 gint        	    	height);

COLORREF  _gdk_win32_colormap_color     (GdkColormap *colormap,
				         gulong       pixel);

HRGN	  _gdk_win32_bitmap_to_hrgn     (GdkPixmap   *bitmap);

HRGN	  _gdk_win32_gdkregion_to_hrgn  (GdkRegion   *region,
					 gint         x_origin,
					 gint         y_origin);

GdkRegion *_gdk_win32_hrgn_to_region    (HRGN hrgn);

void	_gdk_win32_adjust_client_rect   (GdkWindow *window,
					 RECT      *RECT);

void    _gdk_selection_property_delete (GdkWindow *);

void    _gdk_dropfiles_store (gchar *data);

void    _gdk_wchar_text_handle    (GdkFont       *font,
				   const wchar_t *wcstr,
				   int            wclen,
				   void         (*handler)(GdkWin32SingleFont *,
							   const wchar_t *,
							   int,
							   void *),
				   void          *arg);

#ifdef G_ENABLE_DEBUG
gchar *_gdk_win32_color_to_string      (const GdkColor *color);
void   _gdk_win32_print_paletteentries (const PALETTEENTRY *pep,
					const int           nentries);
void   _gdk_win32_print_system_palette (void);
void   _gdk_win32_print_hpalette       (HPALETTE     hpal);
void   _gdk_win32_print_dc             (HDC          hdc);

gchar *_gdk_win32_cap_style_to_string  (GdkCapStyle  cap_style);
gchar *_gdk_win32_fill_style_to_string (GdkFill      fill);
gchar *_gdk_win32_function_to_string   (GdkFunction  function);
gchar *_gdk_win32_join_style_to_string (GdkJoinStyle join_style);
gchar *_gdk_win32_line_style_to_string (GdkLineStyle line_style);
gchar *_gdk_win32_gcvalues_mask_to_string (GdkGCValuesMask mask);
gchar *_gdk_win32_window_state_to_string (GdkWindowState state);
gchar *_gdk_win32_drawable_description (GdkDrawable *d);

gchar *_gdk_win32_rop2_to_string       (int          rop2);
gchar *_gdk_win32_lbstyle_to_string    (UINT         brush_style);
gchar *_gdk_win32_pstype_to_string     (DWORD        pen_style);
gchar *_gdk_win32_psstyle_to_string    (DWORD        pen_style);
gchar *_gdk_win32_psendcap_to_string   (DWORD        pen_style);
gchar *_gdk_win32_psjoin_to_string     (DWORD        pen_style);
gchar *_gdk_win32_message_to_string    (UINT         msg);
gchar *_gdk_win32_key_to_string        (LONG         lParam);
gchar *_gdk_win32_cf_to_string         (UINT         format);
gchar *_gdk_win32_data_to_string       (const guchar*data,
					int          nbytes);
gchar *_gdk_win32_rect_to_string       (const RECT  *rect);

gchar *_gdk_win32_gdkrectangle_to_string (const GdkRectangle *rect);
gchar *_gdk_win32_gdkregion_to_string    (const GdkRegion    *box);

#endif

gchar  *_gdk_win32_last_error_string (void);
void    _gdk_win32_api_failed        (const gchar *where,
				     gint line,
				     const gchar *api);
void    _gdk_other_api_failed        (const gchar *where,
				     gint line,
				     const gchar *api);
void    _gdk_win32_gdi_failed        (const gchar *where,
				     gint line,
				     const gchar *api);

#if defined(__GNUC__) && (__GNUC__ < 3)
#define WIN32_API_FAILED(api) _gdk_win32_api_failed (__FILE__ ":" __PRETTY_FUNCTION__, __LINE__, api)
#define WIN32_GDI_FAILED(api) _gdk_win32_gdi_failed (__FILE__ ":" __PRETTY_FUNCTION__, __LINE__, api)
#define OTHER_API_FAILED(api) _gdk_other_api_failed (__FILE__ ":" __PRETTY_FUNCTION__, __LINE__, api)
#else
#define WIN32_API_FAILED(api) _gdk_win32_api_failed (__FILE__, __LINE__, api)
#define WIN32_GDI_FAILED(api) _gdk_win32_gdi_failed (__FILE__, __LINE__, api)
#define OTHER_API_FAILED(api) _gdk_other_api_failed (__FILE__, __LINE__, api)
#endif
 
/* These two macros call a GDI or other Win32 API and if the return
 * value is zero or NULL, print a warning message. The majority of GDI
 * calls return zero or NULL on failure. The value of the macros is nonzero
 * if the call succeeded, zero otherwise.
 */

#define GDI_CALL(api, arglist) (api arglist ? 1 : (WIN32_GDI_FAILED (#api), 0))
#define API_CALL(api, arglist) (api arglist ? 1 : (WIN32_API_FAILED (#api), 0))
 
extern LRESULT CALLBACK _gdk_win32_window_procedure (HWND, UINT, WPARAM, LPARAM);

extern GdkWindow        *_gdk_root;

extern GdkDisplay       *_gdk_display;
extern GdkScreen        *_gdk_screen;

extern gint		 _gdk_num_monitors;
extern GdkRectangle     *_gdk_monitors;

/* Offsets to add to Windows coordinates (which are relative to the
 * primary monitor's origin, and thus might be negative for monitors
 * to the left and/or above the primary monitor) to get GDK
 * coordinates, which should be non-negative on the whole screen.
 */
extern gint		 _gdk_offset_x, _gdk_offset_y;

extern HDC		 _gdk_display_hdc;
extern HINSTANCE	 _gdk_dll_hinstance;
extern HINSTANCE	 _gdk_app_hmodule;

/* These are thread specific, but GDK/win32 works OK only when invoked
 * from a single thread anyway.
 */
extern HKL		 _gdk_input_locale;
extern gboolean		 _gdk_input_locale_is_ime;
extern UINT		 _gdk_input_codepage;

extern guint		 _gdk_keymap_serial;
extern gboolean		 _gdk_keyboard_has_altgr;
extern guint		 _scancode_rshift;

/* Registered clipboard formats */
extern WORD		 _cf_rtf;
extern WORD		 _cf_utf8_string;
extern WORD		 _cf_image_bmp;

/* GdkAtoms: properties, targets and types */
extern GdkAtom		 _gdk_selection_property;
extern GdkAtom		 _wm_transient_for;
extern GdkAtom		 _targets;
extern GdkAtom		 _save_targets;
extern GdkAtom           _utf8_string;
extern GdkAtom		 _text;
extern GdkAtom		 _compound_text;
extern GdkAtom		 _text_uri_list;
extern GdkAtom		 _image_bmp;

/* DND selections */
extern GdkAtom           _local_dnd;
extern GdkAtom		 _gdk_win32_dropfiles;
extern GdkAtom		 _gdk_ole2_dnd;

/* Options */
extern gboolean		 _gdk_input_ignore_wintab;
extern gint		 _gdk_max_colors;

#define GDK_WIN32_COLORMAP_DATA(cmap) ((GdkColormapPrivateWin32 *) GDK_COLORMAP (cmap)->windowing_data)

/* TRUE while a user-initiated window move or resize operation is in progress */
extern gboolean		 _sizemove_in_progress;

/* TRUE when we are emptying the clipboard ourselves */
extern gboolean		_ignore_destroy_clipboard;

/* Mapping from registered clipboard format id (native) to
 * corresponding GdkAtom
 */
extern GHashTable	*_format_atom_table;

/* Hold the result of a delayed rendering */
extern HGLOBAL		_delayed_rendering_data;

HGLOBAL _gdk_win32_selection_convert_to_dib (HGLOBAL  hdata,
					     GdkAtom  target);

/* Convert a pixbuf to an HICON (or HCURSOR).  Supports alpha under
 * Windows XP, thresholds alpha otherwise.
 */
HICON _gdk_win32_pixbuf_to_hicon   (GdkPixbuf *pixbuf);
HICON _gdk_win32_pixbuf_to_hcursor (GdkPixbuf *pixbuf,
				    gint       x_hotspot,
				    gint       y_hotspot);
gboolean _gdk_win32_pixbuf_to_hicon_supports_alpha (void);

/* Initialization */
void _gdk_windowing_window_init (void);
void _gdk_root_window_size_init (void);
void _gdk_monitor_init(void);
void _gdk_visual_init (void);
void _gdk_dnd_init    (void);
void _gdk_windowing_image_init  (void);
void _gdk_events_init (void);
void _gdk_input_init  (GdkDisplay *display);

#endif /* __GDK_PRIVATE_WIN32_H__ */
