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
#include "gdkwin32.h"

/* Routines from gdkgeometry-win32.c */
void
_gdk_window_init_position (GdkWindow *window);
void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height);

void gdk_win32_selection_init (void);
void gdk_win32_dnd_exit (void);

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

COLORREF gdk_colormap_color      (GdkColormap        *colormap,
				  gulong              pixel);

HRGN	 BitmapToRegion          (HBITMAP hBmp);

gchar  *gdk_font_full_name_get   (GdkFont *font);

void    gdk_sel_prop_store       (GdkWindow *owner,
				  GdkAtom    type,
				  gint       format,
				  guchar    *data,
				  gint       length);

gint    gdk_nmbstowcs            (GdkWChar    *dest,
				  const gchar *src,
				  gint         src_len,
				  gint         dest_max);
gint    gdk_nmbstowchar_ts       (wchar_t     *dest,
				  const gchar *src,
				  gint         src_len,
				  gint         dest_max);

void    gdk_wchar_text_handle    (GdkFont       *font,
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
 
extern LRESULT CALLBACK gdk_window_procedure (HWND, UINT, WPARAM, LPARAM);

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

#define IMAGE_PRIVATE_DATA(image) ((GdkImagePrivateWin32 *) GDK_IMAGE (image)->windowing_data)

#endif /* __GDK_PRIVATE_WIN32_H__ */
