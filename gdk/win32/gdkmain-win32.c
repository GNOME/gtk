/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <io.h>

#include "gdk.h"
#include "gdkkeysyms.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

#include <objbase.h>

static gboolean gdk_synchronize = FALSE;

GdkArgDesc _gdk_windowing_args[] = {
  { "sync",          GDK_ARG_BOOL, &gdk_synchronize, (GdkArgFunc) NULL},
  { "no-wintab",     GDK_ARG_BOOL, &gdk_input_ignore_wintab,
						     (GdkArgFunc) NULL},
  { "ignore-wintab", GDK_ARG_BOOL, &gdk_input_ignore_wintab,
						     (GdkArgFunc) NULL},
  { "event-func-from-window-proc",
		     GDK_ARG_BOOL, &gdk_event_func_from_window_proc,
						     (GdkArgFunc) NULL},
  { NULL }
};

int __stdcall
DllMain(HINSTANCE hinstDLL,
	DWORD     dwReason,
	LPVOID    reserved)
{
  gdk_dll_hinstance = hinstDLL;

  return TRUE;
}

gboolean
_gdk_windowing_init_check (int    argc,
			   char **argv)
{
#ifdef HAVE_WINTAB
  if (getenv ("GDK_IGNORE_WINTAB") != NULL)
    gdk_input_ignore_wintab = TRUE;
#endif
  if (getenv ("GDK_EVENT_FUNC_FROM_WINDOW_PROC") != NULL)
    gdk_event_func_from_window_proc = TRUE;

  if (gdk_synchronize)
    GdiSetBatchLimit (1);

  gdk_app_hmodule = GetModuleHandle (NULL);
  gdk_display_hdc = CreateDC ("DISPLAY", NULL, NULL, NULL);
  gdk_root_window = GetDesktopWindow ();
  windows_version = GetVersion ();

  CoInitialize (NULL);

  gdk_selection_request_msg = RegisterWindowMessage ("gdk-selection-request");
  gdk_selection_notify_msg = RegisterWindowMessage ("gdk-selection-notify");
  gdk_selection_clear_msg = RegisterWindowMessage ("gdk-selection-clear");

  _gdk_selection_property = gdk_atom_intern ("GDK_SELECTION", FALSE);
  gdk_clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
  gdk_win32_dropfiles_atom = gdk_atom_intern ("DROPFILES_DND", FALSE);
  gdk_ole2_dnd_atom = gdk_atom_intern ("OLE2_DND", FALSE);

  _gdk_win32_selection_init ();

  return TRUE;
}

void
gdk_win32_api_failed (const gchar *where,
		      gint         line,
		      const gchar *api)
{
  gchar *msg = g_win32_error_message (GetLastError ());
  g_warning ("%s:%d: %s failed: %s", where, line, api, msg);
  g_free (msg);
}

void
gdk_other_api_failed (const gchar *where,
		      gint         line,
		      const gchar *api)
{
  g_warning ("%s:%d: %s failed", where, line, api);
}

void
gdk_win32_gdi_failed (const gchar *where,
		      gint         line,
		      const gchar *api)
{
  /* On Win9x GDI calls are implemented in 16-bit code and thus
   * don't set the 32-bit error code, sigh.
   */
  if (IS_WIN_NT ())
    gdk_win32_api_failed (where, line, api);
  else
    gdk_other_api_failed (where, line, api);
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
  /* Always on */
}

gboolean
gdk_get_use_xshm (void)
{
  return TRUE;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width
 *
 *   Return the width of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width (void)
{
  return GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (_gdk_parent_root)->impl)->width;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height (void)
{
  return GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (_gdk_parent_root)->impl)->height;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width_mm
 *
 *   Return the width of the screen in millimetres.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width_mm (void)
{
  return GetDeviceCaps (gdk_display_hdc, HORZSIZE);
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen in millimetres.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height_mm (void)
{
  return GetDeviceCaps (gdk_display_hdc, VERTSIZE);
}

void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
  g_warning("gdk_set_sm_client_id %s", sm_client_id ? sm_client_id : "NULL");
}

void
gdk_beep (void)
{
  Beep(1000, 50);
}

void
_gdk_windowing_exit (void)
{
  _gdk_win32_dnd_exit ();
  CoUninitialize ();
  DeleteDC (gdk_display_hdc);
  gdk_display_hdc = NULL;
}

gchar *
gdk_get_display (void)
{
  return "Win32";
}
