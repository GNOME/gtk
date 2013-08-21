/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002,2005 Hans Breuer
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkdisplayprivate.h"
#include "gdkwin32display.h"
#include "gdkwin32screen.h"
#include "gdkwin32window.h"
#include "gdkwin32.h"

#define HAVE_MONITOR_INFO

#if defined(_MSC_VER) && (WINVER < 0x500) && (WINVER > 0x0400)
#include <multimon.h>
#elif defined(_MSC_VER) && (WINVER <= 0x0400)
#undef HAVE_MONITOR_INFO
#endif

static gulong
gdk_win32_display_get_next_serial (GdkDisplay *display)
{
	return 0;
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
  /* The struct MONITORINFOEX definition is for some reason different
   * in the winuser.h bundled with mingw64 from that in MSDN and the
   * official 32-bit mingw (the MONITORINFO part is in a separate "mi"
   * member). So to keep this easily compileable with either, repeat
   * the MSDN definition it here.
   */
  typedef struct tagMONITORINFOEXA2 {
    DWORD cbSize;
    RECT  rcMonitor;
    RECT  rcWork;
    DWORD dwFlags;
    CHAR szDevice[CCHDEVICENAME];
  } MONITORINFOEXA2;

  MONITORINFOEXA2 monitor_info;
  HDC hDC;

  gint *index = (gint *) data;
  GdkWin32Monitor *monitor;

  g_assert (*index < _gdk_num_monitors);

  monitor = _gdk_monitors + *index;

  monitor_info.cbSize = sizeof (MONITORINFOEX);
  GetMonitorInfoA (hmonitor, (MONITORINFO *) &monitor_info);

#ifndef MONITORINFOF_PRIMARY
#define MONITORINFOF_PRIMARY 1
#endif

  monitor->name = g_strdup (monitor_info.szDevice);
  hDC = CreateDCA ("DISPLAY", monitor_info.szDevice, NULL, NULL);
  monitor->width_mm = GetDeviceCaps (hDC, HORZSIZE);
  monitor->height_mm = GetDeviceCaps (hDC, VERTSIZE);
  DeleteDC (hDC);
  monitor->rect.x = monitor_info.rcMonitor.left;
  monitor->rect.y = monitor_info.rcMonitor.top;
  monitor->rect.width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  monitor->rect.height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

  if (monitor_info.dwFlags & MONITORINFOF_PRIMARY &&
      *index != 0)
    {
      /* Put primary monitor at index 0, just in case somebody needs
       * to know which one is the primary.
       */
      GdkWin32Monitor temp = *monitor;
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
  gint i, index;

  _gdk_num_monitors = 0;

  EnumDisplayMonitors (NULL, NULL, count_monitor, (LPARAM) &_gdk_num_monitors);

  _gdk_monitors = g_renew (GdkWin32Monitor, _gdk_monitors, _gdk_num_monitors);

  index = 0;
  EnumDisplayMonitors (NULL, NULL, enum_monitor, (LPARAM) &index);

  _gdk_offset_x = G_MININT;
  _gdk_offset_y = G_MININT;

  /* Calculate offset */
  for (i = 0; i < _gdk_num_monitors; i++)
    {
      _gdk_offset_x = MAX (_gdk_offset_x, -_gdk_monitors[i].rect.x);
      _gdk_offset_y = MAX (_gdk_offset_y, -_gdk_monitors[i].rect.y);
    }
  GDK_NOTE (MISC, g_print ("Multi-monitor offset: (%d,%d)\n",
			   _gdk_offset_x, _gdk_offset_y));

  /* Translate monitor coords into GDK coordinate space */
  for (i = 0; i < _gdk_num_monitors; i++)
    {
      _gdk_monitors[i].rect.x += _gdk_offset_x;
      _gdk_monitors[i].rect.y += _gdk_offset_y;
      GDK_NOTE (MISC, g_print ("Monitor %d: %dx%d@%+d%+d\n",
			       i, _gdk_monitors[i].rect.width,
			       _gdk_monitors[i].rect.height,
			       _gdk_monitors[i].rect.x,
			       _gdk_monitors[i].rect.y));
    }
#else
  HDC hDC;

  _gdk_num_monitors = 1;
  _gdk_monitors = g_renew (GdkWin32Monitor, _gdk_monitors, 1);

  _gdk_monitors[0].name = g_strdup ("DISPLAY");
  hDC = GetDC (NULL);
  _gdk_monitors[0].width_mm = GetDeviceCaps (hDC, HORZSIZE);
  _gdk_monitors[0].height_mm = GetDeviceCaps (hDC, VERTSIZE);
  ReleaseDC (NULL, hDC);
  _gdk_monitors[0].rect.x = 0;
  _gdk_monitors[0].rect.y = 0;
  _gdk_monitors[0].rect.width = GetSystemMetrics (SM_CXSCREEN);
  _gdk_monitors[0].rect.height = GetSystemMetrics (SM_CYSCREEN);
  _gdk_offset_x = 0;
  _gdk_offset_y = 0;
#endif
}

GdkDisplay *
_gdk_win32_display_open (const gchar *display_name)
{
  GDK_NOTE (MISC, g_print ("gdk_display_open: %s\n", (display_name ? display_name : "NULL")));

  if (display_name == NULL ||
      g_ascii_strcasecmp (display_name,
			  gdk_display_get_name (_gdk_display)) == 0)
    {
      if (_gdk_display != NULL)
	{
	  GDK_NOTE (MISC, g_print ("... return _gdk_display\n"));
	  return _gdk_display;
	}
    }
  else
    {
      GDK_NOTE (MISC, g_print ("... return NULL\n"));
      return NULL;
    }

  _gdk_display = g_object_new (GDK_TYPE_WIN32_DISPLAY, NULL);
  _gdk_screen = g_object_new (GDK_TYPE_WIN32_SCREEN, NULL);

  _gdk_monitor_init ();
  _gdk_visual_init (_gdk_screen);
  _gdk_windowing_window_init (_gdk_screen);
  _gdk_events_init ();
  _gdk_input_init (_gdk_display);
  _gdk_dnd_init ();

  /* Precalculate display name */
  (void) gdk_display_get_name (_gdk_display);

  g_signal_emit_by_name (_gdk_display, "opened");

  GDK_NOTE (MISC, g_print ("... _gdk_display now set up\n"));

  return _gdk_display;
}

struct _GdkWin32Display
{
  GdkDisplay display;
};

struct _GdkWin32DisplayClass
{
  GdkDisplayClass display_class;
};

G_DEFINE_TYPE (GdkWin32Display, gdk_win32_display, GDK_TYPE_DISPLAY)

static const gchar *
gdk_win32_display_get_name (GdkDisplay *display)
{
  HDESK hdesk = GetThreadDesktop (GetCurrentThreadId ());
  char dummy;
  char *desktop_name;
  HWINSTA hwinsta = GetProcessWindowStation ();
  char *window_station_name;
  DWORD n;
  DWORD session_id;
  char *display_name;
  static const char *display_name_cache = NULL;
  typedef BOOL (WINAPI *PFN_ProcessIdToSessionId) (DWORD, DWORD *);
  PFN_ProcessIdToSessionId processIdToSessionId;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display_name_cache != NULL)
    return display_name_cache;

  n = 0;
  GetUserObjectInformation (hdesk, UOI_NAME, &dummy, 0, &n);
  if (n == 0)
    desktop_name = "Default";
  else
    {
      n++;
      desktop_name = g_alloca (n + 1);
      memset (desktop_name, 0, n + 1);

      if (!GetUserObjectInformation (hdesk, UOI_NAME, desktop_name, n, &n))
	desktop_name = "Default";
    }

  n = 0;
  GetUserObjectInformation (hwinsta, UOI_NAME, &dummy, 0, &n);
  if (n == 0)
    window_station_name = "WinSta0";
  else
    {
      n++;
      window_station_name = g_alloca (n + 1);
      memset (window_station_name, 0, n + 1);

      if (!GetUserObjectInformation (hwinsta, UOI_NAME, window_station_name, n, &n))
	window_station_name = "WinSta0";
    }

  processIdToSessionId = (PFN_ProcessIdToSessionId) GetProcAddress (GetModuleHandle ("kernel32.dll"), "ProcessIdToSessionId");
  if (!processIdToSessionId || !processIdToSessionId (GetCurrentProcessId (), &session_id))
    session_id = 0;

  display_name = g_strdup_printf ("%ld\\%s\\%s",
				  session_id,
				  window_station_name,
				  desktop_name);

  GDK_NOTE (MISC, g_print ("gdk_win32_display_get_name: %s\n", display_name));

  display_name_cache = display_name;

  return display_name_cache;
}

static GdkScreen *
gdk_win32_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return _gdk_screen;
}

static GdkWindow *
gdk_win32_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  g_warning ("gdk_display_get_default_group not yet implemented");

  return NULL;
}

static gboolean
gdk_win32_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

static HWND _hwnd_next_viewer = NULL;
static int debug_indent = 0;

/*
 * maybe this should be integrated with the default message loop - or maybe not ;-)
 */
static LRESULT CALLBACK
inner_clipboard_window_procedure (HWND   hwnd,
                                  UINT   message,
                                  WPARAM wparam,
                                  LPARAM lparam)
{
  switch (message)
    {
    case WM_DESTROY: /* remove us from chain */
      {
        ChangeClipboardChain (hwnd, _hwnd_next_viewer);
        PostQuitMessage (0);
        return 0;
      }
    case WM_CHANGECBCHAIN:
      {
        HWND hwndRemove = (HWND) wparam; /* handle of window being removed */
        HWND hwndNext   = (HWND) lparam; /* handle of next window in chain */

        if (hwndRemove == _hwnd_next_viewer)
          _hwnd_next_viewer = hwndNext == hwnd ? NULL : hwndNext;
        else if (_hwnd_next_viewer != NULL)
          return SendMessage (_hwnd_next_viewer, message, wparam, lparam);

        return 0;
      }
#ifdef WM_CLIPBOARDUPDATE
    case WM_CLIPBOARDUPDATE:
#endif
    case WM_DRAWCLIPBOARD:
      {
        int success;
        HWND hwndOwner;
#ifdef G_ENABLE_DEBUG
        UINT nFormat = 0;
#endif
        GdkEvent *event;
        GdkWindow *owner;

        success = OpenClipboard (hwnd);
        g_return_val_if_fail (success, 0);
        hwndOwner = GetClipboardOwner ();
        owner = gdk_win32_window_lookup_for_display (_gdk_display, hwndOwner);
        if (owner == NULL)
          owner = gdk_win32_window_foreign_new_for_display (_gdk_display, hwndOwner);

        GDK_NOTE (DND, g_print (" drawclipboard owner: %p", hwndOwner));

#ifdef G_ENABLE_DEBUG
        if (_gdk_debug_flags & GDK_DEBUG_DND)
          {
            while ((nFormat = EnumClipboardFormats (nFormat)) != 0)
              g_print ("%s ", _gdk_win32_cf_to_string (nFormat));
          }
#endif

        GDK_NOTE (DND, g_print (" \n"));


        event = gdk_event_new (GDK_OWNER_CHANGE);
        event->owner_change.window = _gdk_root;
        event->owner_change.owner = owner;
        event->owner_change.reason = GDK_OWNER_CHANGE_NEW_OWNER;
        event->owner_change.selection = GDK_SELECTION_CLIPBOARD;
        event->owner_change.time = _gdk_win32_get_next_tick (0);
        event->owner_change.selection_time = GDK_CURRENT_TIME;
        _gdk_win32_append_event (event);

        CloseClipboard ();

        if (_hwnd_next_viewer != NULL)
          return SendMessage (_hwnd_next_viewer, message, wparam, lparam);

        /* clear error to avoid confusing SetClipboardViewer() return */
        SetLastError (0);
        return 0;
      }
    default:
      /* Otherwise call DefWindowProcW(). */
      GDK_NOTE (EVENTS, g_print (" DefWindowProcW"));
      return DefWindowProc (hwnd, message, wparam, lparam);
    }
}

static LRESULT CALLBACK
_clipboard_window_procedure (HWND   hwnd,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  LRESULT retval;

  GDK_NOTE (EVENTS, g_print ("%s%*s%s %p",
			     (debug_indent > 0 ? "\n" : ""),
			     debug_indent, "",
			     _gdk_win32_message_to_string (message), hwnd));
  debug_indent += 2;
  retval = inner_clipboard_window_procedure (hwnd, message, wparam, lparam);
  debug_indent -= 2;

  GDK_NOTE (EVENTS, g_print (" => %I64d%s", (gint64) retval, (debug_indent == 0 ? "\n" : "")));

  return retval;
}

/*
 * Creates a hidden window and adds it to the clipboard chain
 */
static HWND
_gdk_win32_register_clipboard_notification (void)
{
  WNDCLASS wclass = { 0, };
  HWND     hwnd;
  ATOM     klass;

  wclass.lpszClassName = "GdkClipboardNotification";
  wclass.lpfnWndProc   = _clipboard_window_procedure;
  wclass.hInstance     = _gdk_app_hmodule;

  klass = RegisterClass (&wclass);
  if (!klass)
    return NULL;

  hwnd = CreateWindow (MAKEINTRESOURCE (klass),
                       NULL, WS_POPUP,
                       0, 0, 0, 0, NULL, NULL,
                       _gdk_app_hmodule, NULL);
  if (!hwnd)
    goto failed;

  SetLastError (0);
  _hwnd_next_viewer = SetClipboardViewer (hwnd);

  if (_hwnd_next_viewer == NULL && GetLastError() != 0)
    goto failed;

  /* FIXME: http://msdn.microsoft.com/en-us/library/ms649033(v=VS.85).aspx */
  /* This is only supported by Vista, and not yet by mingw64 */
  /* if (AddClipboardFormatListener (hwnd) == FALSE) */
  /*   goto failed; */

  return hwnd;

failed:
  g_critical ("Failed to install clipboard viewer");
  UnregisterClass (MAKEINTRESOURCE (klass), _gdk_app_hmodule);
  return NULL;
}

static gboolean 
gdk_win32_display_request_selection_notification (GdkDisplay *display,
						  GdkAtom     selection)

{
  static HWND hwndViewer = NULL;
  gboolean ret = FALSE;

  GDK_NOTE (DND,
            g_print ("gdk_display_request_selection_notification (..., %s)",
                     gdk_atom_name (selection)));

  if (selection == GDK_SELECTION_CLIPBOARD ||
      selection == GDK_SELECTION_PRIMARY)
    {
      if (!hwndViewer)
        {
          hwndViewer = _gdk_win32_register_clipboard_notification ();
          GDK_NOTE (DND, g_print (" registered"));
        }
      ret = (hwndViewer != NULL);
    }
  else
    {
      GDK_NOTE (DND, g_print (" unsupported"));
      ret = FALSE;
    }

  GDK_NOTE (DND, g_print (" -> %s\n", ret ? "TRUE" : "FALSE"));
  return ret;
}

static gboolean
gdk_win32_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_win32_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
}

static gboolean 
gdk_win32_display_supports_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

static gboolean
gdk_win32_display_supports_input_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* Not yet implemented. See comment in
   * gdk_window_input_shape_combine_mask().
   */

  return FALSE;
}

static gboolean
gdk_win32_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_win32_display_beep (GdkDisplay *display)
{
  g_return_if_fail (display == gdk_display_get_default());
  if (!MessageBeep (-1))
    Beep(1000, 50);
}

static void
gdk_win32_display_flush (GdkDisplay * display)
{
  g_return_if_fail (display == _gdk_display);

  GdiFlush ();
}


static void
gdk_win32_display_sync (GdkDisplay * display)
{
  g_return_if_fail (display == _gdk_display);

  GdiFlush ();
}

static void
gdk_win32_display_dispose (GObject *object)
{
}

static void
gdk_win32_display_finalize (GObject *object)
{
}

static void
gdk_win32_display_init(GdkWin32Display *display)
{
}

static void
gdk_win32_display_before_process_all_updates (GdkDisplay  *display)
{
  /* nothing */
}
static void
gdk_win32_display_after_process_all_updates (GdkDisplay  *display)
{
  /* nothing */
}
static void
gdk_win32_display_notify_startup_complete (GdkDisplay  *display,
                                           const gchar *startup_id)
{
  /* nothing */
}
static void
gdk_win32_display_event_data_copy (GdkDisplay    *display,
                                   const GdkEvent *src,
                                   GdkEvent       *dst)
{
  /* nothing */
}
static void
gdk_win32_display_event_data_free (GdkDisplay *display,
                                   GdkEvent *event)
{
  /* nothing */
}
static void
gdk_win32_display_push_error_trap (GdkDisplay *display)
{
  /* nothing */
}
static gint
gdk_win32_display_pop_error_trap (GdkDisplay *display,
				  gboolean    ignored)
{
  return 0;
}
static void
gdk_win32_display_class_init (GdkWin32DisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->dispose = gdk_win32_display_dispose;
  object_class->finalize = gdk_win32_display_finalize;

  display_class->window_type = GDK_TYPE_WIN32_WINDOW;

  display_class->get_name = gdk_win32_display_get_name;
  display_class->get_default_screen = gdk_win32_display_get_default_screen;
  display_class->beep = gdk_win32_display_beep;
  display_class->sync = gdk_win32_display_sync;
  display_class->flush = gdk_win32_display_flush;
  display_class->has_pending = _gdk_win32_display_has_pending;
  display_class->queue_events = _gdk_win32_display_queue_events;
  display_class->get_default_group = gdk_win32_display_get_default_group;

  display_class->supports_selection_notification = gdk_win32_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_win32_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_win32_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_win32_display_store_clipboard;
  display_class->supports_shapes = gdk_win32_display_supports_shapes;
  display_class->supports_input_shapes = gdk_win32_display_supports_input_shapes;
  display_class->supports_composite = gdk_win32_display_supports_composite;

  display_class->list_devices = _gdk_win32_display_list_devices;
  //? display_class->get_app_launch_context = _gdk_win32_display_get_app_launch_context;
  display_class->get_cursor_for_type = _gdk_win32_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_win32_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = _gdk_win32_display_get_cursor_for_surface;
  display_class->get_default_cursor_size = _gdk_win32_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_win32_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_win32_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_win32_display_supports_cursor_color;

  display_class->before_process_all_updates = gdk_win32_display_before_process_all_updates;
  display_class->after_process_all_updates = gdk_win32_display_after_process_all_updates;
  display_class->get_next_serial = gdk_win32_display_get_next_serial;
  display_class->notify_startup_complete = gdk_win32_display_notify_startup_complete;
  display_class->event_data_copy = gdk_win32_display_event_data_copy;
  display_class->event_data_free = gdk_win32_display_event_data_free;
  display_class->create_window_impl = _gdk_win32_display_create_window_impl;

  display_class->get_keymap = _gdk_win32_display_get_keymap;
  display_class->push_error_trap = gdk_win32_display_push_error_trap;
  display_class->pop_error_trap = gdk_win32_display_pop_error_trap;
  display_class->get_selection_owner = _gdk_win32_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_win32_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_win32_display_send_selection_notify;
  display_class->get_selection_property = _gdk_win32_display_get_selection_property;
  display_class->convert_selection = _gdk_win32_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_win32_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_win32_display_utf8_to_string_target;
  
  _gdk_win32_windowing_init ();
}
