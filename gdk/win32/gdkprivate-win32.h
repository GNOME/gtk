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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_PRIVATE_WIN32_H__
#define __GDK_PRIVATE_WIN32_H__

#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif

#include <gdk/gdkprivate.h>
#include <gdk/gdkcursorprivate.h>
#include <gdk/win32/gdkwindow-win32.h>

#include "gdkinternals.h"

#include "config.h"

/* Make up for some minor w32api or MSVC6 header lossage */

#ifndef PS_JOIN_MASK
#define PS_JOIN_MASK (PS_JOIN_BEVEL|PS_JOIN_MITER|PS_JOIN_ROUND)
#endif

#ifndef FS_VIETNAMESE
#define FS_VIETNAMESE 0x100
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
#ifndef WM_NCMOUSEHOVER
#define WM_NCMOUSEHOVER 0x2A0
#endif
#ifndef WM_NCMOUSELEAVE
#define WM_NCMOUSELEAVE 0x2A2
#endif
#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND 0x319
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x20E
#endif

#ifndef CF_DIBV5
#define CF_DIBV5 17
#endif


/* Define some combinations of GdkDebugFlags */
#define GDK_DEBUG_EVENTS_OR_INPUT (GDK_DEBUG_EVENTS|GDK_DEBUG_INPUT)
#define GDK_DEBUG_MISC_OR_EVENTS (GDK_DEBUG_MISC|GDK_DEBUG_EVENTS)

GdkScreen *GDK_WINDOW_SCREEN(GObject *win);

#define GDK_WINDOW_IS_WIN32(win)        (GDK_IS_WINDOW_IMPL_WIN32 (win->impl))

typedef struct _GdkColormapPrivateWin32 GdkColormapPrivateWin32;
typedef struct _GdkWin32SingleFont      GdkWin32SingleFont;

struct _GdkWin32Cursor
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

GType _gdk_gc_win32_get_type (void);

gulong _gdk_win32_get_next_tick (gulong suggested_tick);

void _gdk_window_init_position     (GdkWindow *window);
void _gdk_window_move_resize_child (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height);

gboolean _gdk_win32_window_enable_transparency (GdkWindow *window);


/* GdkWindowImpl methods */
void _gdk_win32_window_scroll (GdkWindow *window,
			       gint       dx,
			       gint       dy);
void _gdk_win32_window_move_region (GdkWindow       *window,
				    const cairo_region_t *region,
				    gint             dx,
				    gint             dy);


void _gdk_win32_selection_init (void);
void _gdk_win32_dnd_exit (void);

GdkDragProtocol _gdk_win32_window_get_drag_protocol (GdkWindow *window,
						     GdkWindow **target);

void	 gdk_win32_handle_table_insert  (HANDLE   *handle,
					 gpointer data);
void	 gdk_win32_handle_table_remove  (HANDLE handle);

HRGN	  _gdk_win32_cairo_region_to_hrgn (const cairo_region_t *region,
					   gint                  x_origin,
					   gint                  y_origin);

cairo_region_t *_gdk_win32_hrgn_to_region    (HRGN hrgn);

void	_gdk_win32_adjust_client_rect   (GdkWindow *window,
					 RECT      *RECT);

void    _gdk_selection_property_delete (GdkWindow *);

void    _gdk_dropfiles_store (gchar *data);

void       _gdk_push_modal_window   (GdkWindow *window);
void       _gdk_remove_modal_window (GdkWindow *window);
GdkWindow *_gdk_modal_current       (void);
gboolean   _gdk_modal_blocked       (GdkWindow *window);

#ifdef G_ENABLE_DEBUG
gchar *_gdk_win32_color_to_string      (const GdkColor *color);
void   _gdk_win32_print_paletteentries (const PALETTEENTRY *pep,
					const int           nentries);
void   _gdk_win32_print_system_palette (void);
void   _gdk_win32_print_hpalette       (HPALETTE     hpal);
void   _gdk_win32_print_dc             (HDC          hdc);

gchar *_gdk_win32_drag_protocol_to_string (GdkDragProtocol protocol);
gchar *_gdk_win32_window_state_to_string (GdkWindowState state);
gchar *_gdk_win32_window_style_to_string (LONG style);
gchar *_gdk_win32_window_exstyle_to_string (LONG style);
gchar *_gdk_win32_window_pos_bits_to_string (UINT flags);
gchar *_gdk_win32_drag_action_to_string (GdkDragAction actions);
gchar *_gdk_win32_window_description (GdkWindow *d);

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
gchar *_gdk_win32_cairo_region_to_string (const cairo_region_t    *box);

void   _gdk_win32_print_event            (const GdkEvent     *event);

#endif

gchar  *_gdk_win32_last_error_string (void);
void    _gdk_win32_api_failed        (const gchar *where,
				     const gchar *api);
void    _gdk_other_api_failed        (const gchar *where,
				     const gchar *api);

#define WIN32_API_FAILED(api) _gdk_win32_api_failed (G_STRLOC , api)
#define WIN32_GDI_FAILED(api) WIN32_API_FAILED (api)
#define OTHER_API_FAILED(api) _gdk_other_api_failed (G_STRLOC, api)
 
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
typedef struct _GdkWin32Monitor GdkWin32Monitor;
struct _GdkWin32Monitor
{
  gchar *name;
  gint width_mm, height_mm;
  GdkRectangle rect;
};
extern GdkWin32Monitor  *_gdk_monitors;

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

/* GdkAtoms: properties, targets and types */
extern GdkAtom		 _gdk_selection;
extern GdkAtom		 _wm_transient_for;
extern GdkAtom		 _targets;
extern GdkAtom		 _delete;
extern GdkAtom		 _save_targets;
extern GdkAtom           _utf8_string;
extern GdkAtom		 _text;
extern GdkAtom		 _compound_text;
extern GdkAtom		 _text_uri_list;
extern GdkAtom		 _text_html;
extern GdkAtom		 _image_png;
extern GdkAtom		 _image_jpeg;
extern GdkAtom		 _image_bmp;
extern GdkAtom		 _image_gif;

/* DND selections */
extern GdkAtom           _local_dnd;
extern GdkAtom		 _gdk_win32_dropfiles;
extern GdkAtom		 _gdk_ole2_dnd;

/* Clipboard formats */
extern UINT		 _cf_png;
extern UINT		 _cf_jfif;
extern UINT		 _cf_gif;
extern UINT		 _cf_url;
extern UINT		 _cf_html_format;
extern UINT		 _cf_text_html;

/* OLE-based DND state */
typedef enum {
  GDK_WIN32_DND_NONE,
  GDK_WIN32_DND_PENDING,
  GDK_WIN32_DND_DROPPED,
  GDK_WIN32_DND_FAILED,
  GDK_WIN32_DND_DRAGGING,
} GdkWin32DndState;

extern GdkWin32DndState  _dnd_target_state;
extern GdkWin32DndState  _dnd_source_state;

void _gdk_win32_dnd_do_dragdrop (void);
void _gdk_win32_ole2_dnd_property_change (GdkAtom       type,
					  gint          format,
					  const guchar *data,
					  gint          nelements);

void  _gdk_win32_begin_modal_call (void);
void  _gdk_win32_end_modal_call (void);


/* Options */
extern gboolean		 _gdk_input_ignore_wintab;
extern gint		 _gdk_max_colors;

#define GDK_WIN32_COLORMAP_DATA(cmap) ((GdkColormapPrivateWin32 *) GDK_COLORMAP (cmap)->windowing_data)

/* TRUE while a modal sizing, moving, or dnd operation is in progress */
extern gboolean		_modal_operation_in_progress;

extern HWND		_modal_move_resize_window;

/* TRUE when we are emptying the clipboard ourselves */
extern gboolean		_ignore_destroy_clipboard;

/* Mapping from registered clipboard format id (native) to
 * corresponding GdkAtom
 */
extern GHashTable	*_format_atom_table;

/* Hold the result of a delayed rendering */
extern HGLOBAL		_delayed_rendering_data;

extern HCURSOR _gdk_win32_grab_cursor;

HGLOBAL _gdk_win32_selection_convert_to_dib (HGLOBAL  hdata,
					     GdkAtom  target);

/* Convert a pixbuf to an HICON (or HCURSOR).  Supports alpha under
 * Windows XP, thresholds alpha otherwise.
 */
HICON _gdk_win32_pixbuf_to_hicon   (GdkPixbuf *pixbuf);
HICON _gdk_win32_pixbuf_to_hcursor (GdkPixbuf *pixbuf,
				    gint       x_hotspot,
				    gint       y_hotspot);

/* GdkDisplay member functions */
GdkCursor *_gdk_win32_display_get_cursor_for_type (GdkDisplay   *display,
						   GdkCursorType cursor_type);
GdkCursor *_gdk_win32_display_get_cursor_for_name (GdkDisplay  *display,
						   const gchar *name);
GdkCursor *_gdk_win32_display_get_cursor_for_surface (GdkDisplay *display,
						     cairo_surface_t  *surface,
						     gdouble          x,
						     gdouble          y);
void     _gdk_win32_display_get_default_cursor_size (GdkDisplay  *display,
						     guint       *width,
						     guint       *height);
void     _gdk_win32_display_get_maximal_cursor_size (GdkDisplay  *display,
						     guint       *width,
						     guint       *height);
gboolean _gdk_win32_display_supports_cursor_alpha (GdkDisplay    *display);
gboolean _gdk_win32_display_supports_cursor_color (GdkDisplay    *display);

GList *_gdk_win32_display_list_devices (GdkDisplay *dpy);

gboolean _gdk_win32_display_has_pending (GdkDisplay *display);
void _gdk_win32_display_queue_events (GdkDisplay *display);

gboolean _gdk_win32_selection_owner_set_for_display (GdkDisplay *display,
						     GdkWindow  *owner,
						     GdkAtom     selection,
						     guint32     time,
						     gboolean    send_event);
GdkWindow *_gdk_win32_display_get_selection_owner   (GdkDisplay *display,
						     GdkAtom     selection);
gboolean   _gdk_win32_display_set_selection_owner   (GdkDisplay *display,
						     GdkWindow  *owner,
						     GdkAtom     selection,
						     guint32     time,
						     gboolean    send_event);
void       _gdk_win32_display_send_selection_notify (GdkDisplay      *display,
						     GdkWindow       *requestor,
						     GdkAtom   	      selection,
						     GdkAtom          target,
						     GdkAtom          property,
						     guint32          time);
gint      _gdk_win32_display_get_selection_property (GdkDisplay *display,
						     GdkWindow  *requestor,
						     guchar    **data,
						     GdkAtom    *ret_type,
						     gint       *ret_format);
void      _gdk_win32_display_convert_selection (GdkDisplay *display,
						GdkWindow *requestor,
						GdkAtom    selection,
						GdkAtom    target,
						guint32    time);
gint      _gdk_win32_display_text_property_to_utf8_list (GdkDisplay    *display,
							 GdkAtom        encoding,
							 gint           format,
							 const guchar  *text,
							 gint           length,
							 gchar       ***list);
gchar     *_gdk_win32_display_utf8_to_string_target (GdkDisplay *display, const gchar *str);

GdkKeymap *_gdk_win32_display_get_keymap (GdkDisplay *display);

void       _gdk_win32_display_create_window_impl   (GdkDisplay    *display,
                                                    GdkWindow     *window,
                                                    GdkWindow     *real_parent,
                                                    GdkScreen     *screen,
                                                    GdkEventMask   event_mask,
                                                    GdkWindowAttr *attributes,
                                                    gint           attributes_mask);

/* stray GdkWindowImplWin32 members */
void _gdk_win32_window_register_dnd (GdkWindow *window);
GdkDragContext *_gdk_win32_window_drag_begin (GdkWindow *window, GdkDevice *device, GList *targets);
gboolean _gdk_win32_window_simulate_key (GdkWindow      *window,
				  gint            x,
				  gint            y,
				  guint           keyval,
				  GdkModifierType modifiers,
				  GdkEventType    key_pressrelease);
gboolean _gdk_win32_window_simulate_button (GdkWindow      *window,
				     gint            x,
				     gint            y,
				     guint           button, /*1..3*/
				     GdkModifierType modifiers,
				     GdkEventType    button_pressrelease);

gint _gdk_win32_window_get_property (GdkWindow   *window,
				     GdkAtom      property,
				     GdkAtom      type,
				     gulong       offset,
				     gulong       length,
				     gint         pdelete,
				     GdkAtom     *actual_property_type,
				     gint        *actual_format_type,
				     gint        *actual_length,
				     guchar     **data);
void _gdk_win32_window_change_property (GdkWindow    *window,
					GdkAtom       property,
					GdkAtom       type,
					gint          format,
					GdkPropMode   mode,
					const guchar *data,
					gint          nelements);
void _gdk_win32_window_delete_property (GdkWindow *window, GdkAtom    property);

/* Stray GdkWin32Screen members */
GdkVisual *_gdk_win32_screen_get_system_visual (GdkScreen *screen);
GdkVisual *_gdk_win32_screen_get_rgba_visual (GdkScreen *screen);
gboolean _gdk_win32_screen_get_setting (GdkScreen   *screen, const gchar *name, GValue *value);
gint _gdk_win32_screen_visual_get_best_depth (GdkScreen *screen);
GdkVisualType _gdk_win32_screen_visual_get_best_type (GdkScreen *screen);
GdkVisual *_gdk_win32_screen_visual_get_best (GdkScreen *screen);
GdkVisual *_gdk_win32_screen_visual_get_best_with_depth (GdkScreen *screen, gint depth);
GdkVisual *_gdk_win32_screen_visual_get_best_with_type (GdkScreen *screen, GdkVisualType visual_type);
GdkVisual *_gdk_win32_screen_visual_get_best_with_both (GdkScreen *screen, gint depth, GdkVisualType visual_type);
void _gdk_win32_screen_query_depths  (GdkScreen *screen, gint **depths, gint  *count);
void _gdk_win32_screen_query_visual_types (GdkScreen      *screen,
				           GdkVisualType **visual_types,
				           gint           *count);
GList *_gdk_win32_screen_list_visuals (GdkScreen *screen);

/* Distributed display manager implementation */
GdkDisplay *_gdk_win32_display_open (const gchar *display_name);
GdkAtom _gdk_win32_display_manager_atom_intern (GdkDisplayManager *manager,
						const gchar *atom_name,
						gint         only_if_exists);
gchar *_gdk_win32_display_manager_get_atom_name (GdkDisplayManager *manager, 
					         GdkAtom            atom);
void _gdk_win32_append_event (GdkEvent *event);
void _gdk_win32_emit_configure_event (GdkWindow *window);

/* Initialization */
void _gdk_win32_windowing_init (void);
void _gdk_windowing_window_init (GdkScreen *screen);
void _gdk_root_window_size_init (void);
void _gdk_monitor_init(void);
void _gdk_visual_init (GdkScreen *screen);
void _gdk_dnd_init    (void);
void _gdk_events_init (void);
void _gdk_input_init  (GdkDisplay *display);
void _gdk_input_wintab_init_check (GdkDeviceManager *device_manager);

extern gboolean _is_win8_or_later;

#endif /* __GDK_PRIVATE_WIN32_H__ */
