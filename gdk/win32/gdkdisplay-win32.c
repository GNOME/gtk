/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002 Hans Breuer
 * Copyright (C) 2003 Tor Lillqvist
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

#include <config.h>
#include "gdk.h"
#include "gdkprivate-win32.h"

#define HAVE_MONITOR_INFO

#if defined(_MSC_VER) && (WINVER < 0x500) && (WINVER > 0x0400)
#include <multimon.h>
#elif defined(_MSC_VER) && (WINVER <= 0x0400)
#undef HAVE_MONITOR_INFO
#endif

#ifdef HAVE_MONITOR_INFO
typedef BOOL (WINAPI *t_EnumDisplayMonitors)(HDC, LPCRECT, MONITORENUMPROC, LPARAM);
typedef BOOL (WINAPI *t_GetMonitorInfoA)(HMONITOR, LPMONITORINFO);

static t_EnumDisplayMonitors p_EnumDisplayMonitors = NULL;
static t_GetMonitorInfoA p_GetMonitorInfoA = NULL;
#endif

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  g_assert (_gdk_display == display);
}

#ifdef HAVE_MONITOR_INFO
static BOOL CALLBACK
count_monitor (HMONITOR hmonitor,
	       HDC      hdc,
	       LPRECT   rect,
	       LPARAM   data)
{
  gint *n = (gint *) data;

  (*n)++;

  return TRUE;
}

static BOOL CALLBACK
enum_monitor (HMONITOR hmonitor,
	      HDC      hdc,
	      LPRECT   rect,
	      LPARAM   data)
{
  MONITORINFOEX monitor_info;

  gint *index = (gint *) data;
  GdkRectangle *monitor;

  g_assert (*index < _gdk_num_monitors);

  monitor = _gdk_monitors + *index;

  monitor_info.cbSize = sizeof (MONITORINFOEX);
  (*p_GetMonitorInfoA) (hmonitor, (MONITORINFO *) &monitor_info);

#ifndef MONITORINFOF_PRIMARY
#define MONITORINFOF_PRIMARY 1
#endif

  monitor->x = monitor_info.rcMonitor.left;
  monitor->y = monitor_info.rcMonitor.top;
  monitor->width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  monitor->height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

  if (monitor_info.dwFlags & MONITORINFOF_PRIMARY &&
      *index != 0)
    {
      /* Put primary monitor at index 0, just in case somebody needs
       * to know which one is the primary.
       */
      GdkRectangle temp = *monitor;
      *monitor = _gdk_monitors[0];
      _gdk_monitors[0] = temp;
    }

  (*index)++;

  return TRUE;
}
#endif /* HAVE_MONITOR_INFO */

void
_gdk_monitor_init (void)
{
#ifdef HAVE_MONITOR_INFO
  static HMODULE user32 = NULL;

  if (user32 == NULL)
    {
      user32 = GetModuleHandle ("user32.dll");

      g_assert (user32 != NULL);

      p_EnumDisplayMonitors = (t_EnumDisplayMonitors) GetProcAddress (user32, "EnumDisplayMonitors");
      p_GetMonitorInfoA = (t_GetMonitorInfoA) GetProcAddress (user32, "GetMonitorInfoA");
    }

  if (p_EnumDisplayMonitors != NULL && p_GetMonitorInfoA != NULL)
    {
      gint i, index;

      _gdk_num_monitors = 0;

      (*p_EnumDisplayMonitors) (NULL, NULL, count_monitor, (LPARAM) &_gdk_num_monitors);

      _gdk_monitors = g_renew (GdkRectangle, _gdk_monitors, _gdk_num_monitors);

      index = 0;
      (*p_EnumDisplayMonitors) (NULL, NULL, enum_monitor, (LPARAM) &index);

      _gdk_offset_x = G_MININT;
      _gdk_offset_y = G_MININT;

      /* Calculate offset */
      for (i = 0; i < _gdk_num_monitors; i++)
	{
	  _gdk_offset_x = MAX (_gdk_offset_x, -_gdk_monitors[i].x);
	  _gdk_offset_y = MAX (_gdk_offset_y, -_gdk_monitors[i].y);
	}
      GDK_NOTE (MISC, g_print ("Multi-monitor offset: (%d,%d)\n",
			       _gdk_offset_x, _gdk_offset_y));

      /* Translate monitor coords into GDK coordinate space */
      for (i = 0; i < _gdk_num_monitors; i++)
	{
	  _gdk_monitors[i].x += _gdk_offset_x;
	  _gdk_monitors[i].y += _gdk_offset_y;
	  GDK_NOTE (MISC, g_print ("Monitor %d: %dx%d@+%d+%d\n",
				   i, _gdk_monitors[i].width,
				   _gdk_monitors[i].height,
				   _gdk_monitors[i].x, _gdk_monitors[i].y));
	}
    }
  else
#endif /* HAVE_MONITOR_INFO */
    {
      unsigned int width, height;

      _gdk_num_monitors = 1;
      _gdk_monitors = g_renew (GdkRectangle, _gdk_monitors, 1);

      width = GetSystemMetrics (SM_CXSCREEN);
      height = GetSystemMetrics (SM_CYSCREEN);

      _gdk_monitors[0].x = 0;
      _gdk_monitors[0].y = 0;
      _gdk_monitors[0].width = width;
      _gdk_monitors[0].height = height;
      _gdk_offset_x = 0;
      _gdk_offset_y = 0;
    }

}

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL; /* single display only */

  _gdk_display = g_object_new (GDK_TYPE_DISPLAY, NULL);
  _gdk_screen = g_object_new (GDK_TYPE_SCREEN, NULL);

  _gdk_monitor_init ();
  _gdk_visual_init ();
  gdk_screen_set_default_colormap (_gdk_screen,
                                   gdk_screen_get_system_colormap (_gdk_screen));
  _gdk_windowing_window_init ();
  _gdk_windowing_image_init ();
  _gdk_events_init ();
  _gdk_input_init (_gdk_display);
  _gdk_dnd_init ();

  g_signal_emit_by_name (gdk_display_manager_get (),
			 "display_opened", _gdk_display);

  return _gdk_display;
}

G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay *display)
{
  return gdk_get_display_arg_name ();
}

gint
gdk_display_get_n_screens (GdkDisplay *display)
{
  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  return _gdk_screen;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
}

GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  g_warning ("gdk_display_get_default_group not yet implemented");

  return NULL;
}

gboolean 
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

static HWND _hwnd_next_viewer = NULL;

/*
 * maybe this should be integrated with the default message loop - or maybe not ;-)
 */
static LRESULT CALLBACK
_win32_on_clipboard_change (HWND   hwnd,
                            UINT   message,
                            WPARAM wparam,
                            LPARAM lparam)
{
  switch (message)
    {
    case WM_DESTROY : /* remove us from chain */
      {
        ChangeClipboardChain (hwnd, _hwnd_next_viewer);
        return 0; 
      }
    case WM_CHANGECBCHAIN :
      {
        HWND hwndRemove = (HWND) wparam; /* handle of window being removed */
        HWND hwndNext   = (HWND) lparam; /* handle of next window in chain */
        if (hwndRemove == _hwnd_next_viewer)
          _hwnd_next_viewer = hwndNext == hwnd ? NULL : hwndNext;
        return 0;
      }
    case WM_DRAWCLIPBOARD :
      {
        /* create the appropriate gdk events */
        HWND hwndOwner = GetClipboardOwner ();
        UINT nFormat = 0;
        int n = 0;

        if (OpenClipboard (hwnd))
          { 
            for (; 0 != (nFormat = EnumClipboardFormats (nFormat)); )
              {
                char sFormat[80];
                if (GetClipboardFormatName (nFormat, sFormat, 80) > 0)
                  g_print ("%s ", sFormat);
                n++; /* do something useful ? */
              }
            GDK_NOTE (DND, 
                      g_print ("WM_DRAWCLIPBOARD :  formats %d owner %#lx\n", n, hwndOwner));

            CloseClipboard ();
          }
        /* XXX: generate the apropriate GdkEventOwnerChange ... */

        /* don't break the chain */
        return PostMessage (_hwnd_next_viewer, message, wparam, lparam);
      }
    default :
      return DefWindowProc (hwnd, message, wparam, lparam);
    }
}

/*
 * Creates a hidden window and adds it to the clipboard chain
 */
HWND
_gdk_win32_register_clipboard_notification (void)
{
  WNDCLASS wclass;
  HWND     hwnd;
  ATOM     klass;

  memset (&wclass, 0, sizeof(WNDCLASS));
  wclass.lpszClassName = "GdkClipboardNotification";
  wclass.lpfnWndProc   = _win32_on_clipboard_change;
  wclass.hInstance     = _gdk_app_hmodule;

  klass = RegisterClass (&wclass);
  if (!klass)
    return NULL;

  hwnd = CreateWindow (MAKEINTRESOURCE(klass),
                       NULL, WS_POPUP,
                       0, 0, 0, 0, NULL, NULL,
                       _gdk_app_hmodule, NULL);
  if (!hwnd)
    {
      UnregisterClass (MAKEINTRESOURCE(klass), _gdk_app_hmodule);
      return NULL;
    }
  _hwnd_next_viewer = SetClipboardViewer (hwnd);
  return hwnd;
}

/*
 * The whole function would only make sense if the gdk/win32 clipboard
 * model is rewritten to do delayed rendering. Currently this is only
 * testcode and as noted in
 * http://mail.gnome.org/archives/gtk-devel-list/2004-May/msg00113.html
 * probably not worth bothering ;)
 */
gboolean 
gdk_display_request_selection_notification (GdkDisplay *display,
                                            GdkAtom     selection)

{
  static HWND hwndViewer = NULL;
  gboolean ret = FALSE;

  GDK_NOTE (DND, 
            g_print ("gdk_display_request_selection_notification (..., %s)",
                     gdk_atom_name (selection)));

  if (GDK_SELECTION_CLIPBOARD == selection)
    {
      if (!hwndViewer)
        {
          hwndViewer = _gdk_win32_register_clipboard_notification ();
          GDK_NOTE (DND, g_print (" registered"));
        }
      ret = (hwndViewer != NULL);
    }
  else if (GDK_SELECTION_PRIMARY == selection)
    {
      /* seems to work by default ? */
      GDK_NOTE (DND, g_print (" by default"));
      ret = TRUE;
    }
  GDK_NOTE (DND, g_print (" -> %s\n", ret ? "TRUE" : "FALSE"));
  return ret;
}

gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

void
gdk_display_store_clipboard (GdkDisplay *display,
			     GdkWindow  *clipboard_window,
			     guint32     time_,
			     GdkAtom    *targets,
			     gint        n_targets)
{
  /* XXX: implement it (or maybe not as long as we don't support delayed rendering?) */
}
