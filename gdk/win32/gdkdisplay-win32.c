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

#define _WIN32_WINNT 0x0600
#define VK_USE_PLATFORM_WIN32_KHR

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkcairocontext-win32.h"
#include "gdkclipboardprivate.h"
#include "gdkclipboard-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkdevicemanager-win32.h"
#include "gdkglcontext-win32.h"
#include "gdkwin32display.h"
#include "gdkwin32screen.h"
#include "gdkwin32surface.h"
#include "gdkmonitor-win32.h"
#include "gdkwin32.h"
#include "gdkvulkancontext-win32.h"

#include <dwmapi.h>

#include "gdkwin32langnotification.h"

static int debug_indent = 0;

/**
 * gdk_win32_display_add_filter:
 * @display: a #GdkWin32Display
 * @function: filter callback
 * @data: data to pass to filter callback
 *
 * Adds an event filter to @window, allowing you to intercept messages
 * before they reach GDK. This is a low-level operation and makes it
 * easy to break GDK and/or GTK+, so you have to know what you're
 * doing.
 **/
void
gdk_win32_display_add_filter (GdkWin32Display           *display,
                              GdkWin32MessageFilterFunc  function,
                              gpointer                   data)
{
  GList *tmp_list;
  GdkWin32MessageFilter *filter;

  g_return_if_fail (GDK_IS_WIN32_DISPLAY (display));

  tmp_list = display->filters;

  for (tmp_list = display->filters; tmp_list; tmp_list = tmp_list->next)
    {
      filter = (GdkWin32MessageFilter *) tmp_list->data;

      if ((filter->function == function) && (filter->data == data))
        {
          filter->ref_count++;
          return;
        }
    }

  filter = g_new (GdkWin32MessageFilter, 1);
  filter->function = function;
  filter->data = data;
  filter->ref_count = 1;
  filter->removed = FALSE;

  display->filters = g_list_append (display->filters, filter);
}

/**
 * _gdk_win32_message_filter_unref:
 * @display: A #GdkWin32Display
 * @filter: A message filter
 *
 * Release a reference to @filter.  Note this function may
 * mutate the list storage, so you need to handle this
 * if iterating over a list of filters.
 */
void
_gdk_win32_message_filter_unref (GdkWin32Display       *display,
			         GdkWin32MessageFilter *filter)
{
  GList **filters;
  GList *tmp_list;

  filters = &display->filters;

  tmp_list = *filters;
  while (tmp_list)
    {
      GdkWin32MessageFilter *iter_filter = tmp_list->data;
      GList *node;

      node = tmp_list;
      tmp_list = tmp_list->next;

      if (iter_filter != filter)
	continue;

      g_assert (iter_filter->ref_count > 0);

      filter->ref_count--;
      if (filter->ref_count != 0)
	continue;

      *filters = g_list_remove_link (*filters, node);
      g_free (filter);
      g_list_free_1 (node);
    }
}

/**
 * gdk_win32_display_remove_filter:
 * @display: A #GdkWin32Display
 * @function: previously-added filter function
 * @data: user data for previously-added filter function
 *
 * Remove a filter previously added with gdk_win32_display_add_filter().
 */
void
gdk_win32_display_remove_filter (GdkWin32Display           *display,
                                 GdkWin32MessageFilterFunc  function,
                                 gpointer                   data)
{
  GList *tmp_list;
  GdkWin32MessageFilter *filter;

  g_return_if_fail (GDK_IS_WIN32_DISPLAY (display));

  tmp_list = display->filters;

  while (tmp_list)
    {
      filter = (GdkWin32MessageFilter *) tmp_list->data;
      tmp_list = tmp_list->next;

      if ((filter->function == function) && (filter->data == data))
        {
          filter->removed = TRUE;

          _gdk_win32_message_filter_unref (display, filter);

          return;
        }
    }
}

static GdkMonitor *
_gdk_win32_display_find_matching_monitor (GdkWin32Display *win32_display,
                                          GdkMonitor      *needle)
{
  int i;

  for (i = 0; i < win32_display->monitors->len; i++)
    {
      GdkWin32Monitor *m;

      m = GDK_WIN32_MONITOR (g_ptr_array_index (win32_display->monitors, i));

      if (_gdk_win32_monitor_compare (m, GDK_WIN32_MONITOR (needle)) == 0)
        return GDK_MONITOR (m);
    }

  return NULL;
}

gboolean
_gdk_win32_display_init_monitors (GdkWin32Display *win32_display)
{
  GdkDisplay *display = GDK_DISPLAY (win32_display);
  GPtrArray *new_monitors;
  gint i;
  gboolean changed = FALSE;
  GdkWin32Monitor *primary_to_move = NULL;

  for (i = 0; i < win32_display->monitors->len; i++)
    GDK_WIN32_MONITOR (g_ptr_array_index (win32_display->monitors, i))->remove = TRUE;

  new_monitors = _gdk_win32_display_get_monitor_list (win32_display);

  for (i = 0; i < new_monitors->len; i++)
    {
      GdkWin32Monitor *w32_m;
      GdkMonitor *m;
      GdkWin32Monitor *w32_ex_monitor;
      GdkMonitor *ex_monitor;
      GdkRectangle geometry, ex_geometry;
      GdkRectangle workarea, ex_workarea;

      w32_m = GDK_WIN32_MONITOR (g_ptr_array_index (new_monitors, i));
      m = GDK_MONITOR (w32_m);
      ex_monitor = _gdk_win32_display_find_matching_monitor (win32_display, m);
      w32_ex_monitor = GDK_WIN32_MONITOR (ex_monitor);

      if (ex_monitor == NULL)
        {
          w32_m->add = TRUE;
          changed = TRUE;
          continue;
        }

      w32_ex_monitor->remove = FALSE;

      if (i == 0)
        primary_to_move = w32_ex_monitor;

      gdk_monitor_get_geometry (m, &geometry);
      gdk_monitor_get_geometry (ex_monitor, &ex_geometry);
      gdk_monitor_get_workarea (m, &workarea);
      gdk_monitor_get_workarea (ex_monitor, &ex_workarea);

      if (memcmp (&workarea, &ex_workarea, sizeof (GdkRectangle)) != 0)
        {
          w32_ex_monitor->work_rect = workarea;
          changed = TRUE;
        }

      if (memcmp (&geometry, &ex_geometry, sizeof (GdkRectangle)) != 0)
        {
          gdk_monitor_set_size (ex_monitor, geometry.width, geometry.height);
          gdk_monitor_set_position (ex_monitor, geometry.x, geometry.y);
          changed = TRUE;
        }

      if (gdk_monitor_get_width_mm (m) != gdk_monitor_get_width_mm (ex_monitor) ||
          gdk_monitor_get_height_mm (m) != gdk_monitor_get_height_mm (ex_monitor))
        {
          gdk_monitor_set_physical_size (ex_monitor,
                                         gdk_monitor_get_width_mm (m),
                                         gdk_monitor_get_height_mm (m));
          changed = TRUE;
        }

      if (g_strcmp0 (gdk_monitor_get_model (m), gdk_monitor_get_model (ex_monitor)) != 0)
        {
          gdk_monitor_set_model (ex_monitor,
                                 gdk_monitor_get_model (m));
          changed = TRUE;
        }

      if (g_strcmp0 (gdk_monitor_get_manufacturer (m), gdk_monitor_get_manufacturer (ex_monitor)) != 0)
        {
          gdk_monitor_set_manufacturer (ex_monitor,
                                        gdk_monitor_get_manufacturer (m));
          changed = TRUE;
        }

      if (gdk_monitor_get_refresh_rate (m) != gdk_monitor_get_refresh_rate (ex_monitor))
        {
          gdk_monitor_set_refresh_rate (ex_monitor, gdk_monitor_get_refresh_rate (m));
          changed = TRUE;
        }

      if (gdk_monitor_get_scale_factor (m) != gdk_monitor_get_scale_factor (ex_monitor))
        {
          gdk_monitor_set_scale_factor (ex_monitor, gdk_monitor_get_scale_factor (m));
          changed = TRUE;
        }

      if (gdk_monitor_get_subpixel_layout (m) != gdk_monitor_get_subpixel_layout (ex_monitor))
        {
          gdk_monitor_set_subpixel_layout (ex_monitor, gdk_monitor_get_subpixel_layout (m));
          changed = TRUE;
        }
    }

  for (i = win32_display->monitors->len - 1; i >= 0; i--)
    {
      GdkWin32Monitor *w32_ex_monitor;
      GdkMonitor *ex_monitor;

      w32_ex_monitor = GDK_WIN32_MONITOR (g_ptr_array_index (win32_display->monitors, i));
      ex_monitor = GDK_MONITOR (w32_ex_monitor);

      if (!w32_ex_monitor->remove)
        continue;

      changed = TRUE;
      gdk_display_monitor_removed (display, ex_monitor);
      g_ptr_array_remove_index (win32_display->monitors, i);
    }

  for (i = 0; i < new_monitors->len; i++)
    {
      GdkWin32Monitor *w32_m;
      GdkMonitor *m;

      w32_m = GDK_WIN32_MONITOR (g_ptr_array_index (new_monitors, i));
      m = GDK_MONITOR (w32_m);

      if (!w32_m->add)
        continue;

      gdk_display_monitor_added (display, m);

      if (i == 0)
        g_ptr_array_insert (win32_display->monitors, 0, g_object_ref (w32_m));
      else
        g_ptr_array_add (win32_display->monitors, g_object_ref (w32_m));
    }

  g_ptr_array_free (new_monitors, TRUE);

  if (primary_to_move)
    {
      g_ptr_array_remove (win32_display->monitors, g_object_ref (primary_to_move));
      g_ptr_array_insert (win32_display->monitors, 0, primary_to_move);
      changed = TRUE;
    }
  return changed;
}


/**
 * gdk_win32_display_set_cursor_theme:
 * @display: (type GdkWin32Display): a #GdkDisplay
 * @name: (allow-none): the name of the cursor theme to use, or %NULL to unset
 *         a previously set value
 * @size: the cursor size to use, or 0 to keep the previous size
 *
 * Sets the cursor theme from which the images for cursor
 * should be taken.
 *
 * If the windowing system supports it, existing cursors created
 * with gdk_cursor_new(), gdk_cursor_new_for_display() and
 * gdk_cursor_new_from_name() are updated to reflect the theme
 * change. Custom cursors constructed with
 * gdk_cursor_new_from_texture() will have to be handled
 * by the application (GTK+ applications can learn about
 * cursor theme changes by listening for change notification
 * for the corresponding #GtkSetting).
 */
void
gdk_win32_display_set_cursor_theme (GdkDisplay  *display,
                                    const gchar *name,
                                    gint         size)
{
  gint cursor_size;
  gint w, h;
  Win32CursorTheme *theme;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  g_assert (win32_display);

  if (name == NULL)
    name = "system";

  w = GetSystemMetrics (SM_CXCURSOR);
  h = GetSystemMetrics (SM_CYCURSOR);

  /* We can load cursors of any size, but SetCursor() will scale them back
   * to this value. It's possible to break that restrictions with SetSystemCursor(),
   * but that will override cursors for the whole desktop session.
   */
  cursor_size = (w == h) ? w : size;

  if (win32_display->cursor_theme_name != NULL &&
      g_strcmp0 (name, win32_display->cursor_theme_name) == 0 &&
      win32_display->cursor_theme_size == cursor_size)
    return;

  theme = win32_cursor_theme_load (name, cursor_size);
  if (theme == NULL)
    {
      g_warning ("Failed to load cursor theme %s", name);
      return;
    }

  if (win32_display->cursor_theme)
    {
      win32_cursor_theme_destroy (win32_display->cursor_theme);
      win32_display->cursor_theme = NULL;
    }

  win32_display->cursor_theme = theme;
  g_free (win32_display->cursor_theme_name);
  win32_display->cursor_theme_name = g_strdup (name);
  win32_display->cursor_theme_size = cursor_size;

  _gdk_win32_display_update_cursors (win32_display);
}

Win32CursorTheme *
_gdk_win32_display_get_cursor_theme (GdkWin32Display *win32_display)
{
  Win32CursorTheme *theme;

  g_assert (win32_display->cursor_theme_name);

  theme = win32_display->cursor_theme;
  if (!theme)
    {
      theme = win32_cursor_theme_load (win32_display->cursor_theme_name,
                                       win32_display->cursor_theme_size);
      if (theme == NULL)
        {
          g_warning ("Failed to load cursor theme %s",
                     win32_display->cursor_theme_name);
          return NULL;
        }
      win32_display->cursor_theme = theme;
    }

  return theme;
}

static gulong
gdk_win32_display_get_next_serial (GdkDisplay *display)
{
	return 0;
}

static LRESULT CALLBACK
inner_display_change_window_procedure (HWND   hwnd,
                                       UINT   message,
                                       WPARAM wparam,
                                       LPARAM lparam)
{
  switch (message)
    {
    case WM_DESTROY:
      {
        PostQuitMessage (0);
        return 0;
      }
    case WM_DISPLAYCHANGE:
      {
        GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (_gdk_display);

        _gdk_win32_screen_on_displaychange_event (GDK_WIN32_SCREEN (win32_display->screen));
        return 0;
      }
    default:
      /* Otherwise call DefWindowProcW(). */
      GDK_NOTE (EVENTS, g_print (" DefWindowProcW"));
      return DefWindowProc (hwnd, message, wparam, lparam);
    }
}

static LRESULT CALLBACK
display_change_window_procedure (HWND   hwnd,
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
  retval = inner_display_change_window_procedure (hwnd, message, wparam, lparam);
  debug_indent -= 2;

  GDK_NOTE (EVENTS, g_print (" => %" G_GINT64_FORMAT "%s", (gint64) retval, (debug_indent == 0 ? "\n" : "")));

  return retval;
}

/* Use a hidden window to be notified about display changes */
static void
register_display_change_notification (GdkDisplay *display)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  WNDCLASS wclass = { 0, };
  ATOM klass;

  wclass.lpszClassName = "GdkDisplayChange";
  wclass.lpfnWndProc = display_change_window_procedure;
  wclass.hInstance = _gdk_app_hmodule;

  klass = RegisterClass (&wclass);
  if (klass)
    {
      display_win32->hwnd = CreateWindow (MAKEINTRESOURCE (klass),
                                          NULL, WS_POPUP,
                                          0, 0, 0, 0, NULL, NULL,
                                          _gdk_app_hmodule, NULL);
      if (!display_win32->hwnd)
        {
          UnregisterClass (MAKEINTRESOURCE (klass), _gdk_app_hmodule);
        }
    }
}

GdkDisplay *
_gdk_win32_display_open (const gchar *display_name)
{
  GdkWin32Display *win32_display;

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
  win32_display = GDK_WIN32_DISPLAY (_gdk_display);

  win32_display->screen = g_object_new (GDK_TYPE_WIN32_SCREEN, NULL);

  _gdk_events_init (_gdk_display);

  _gdk_input_ignore_core = 0;

  _gdk_device_manager = g_object_new (GDK_TYPE_DEVICE_MANAGER_WIN32,
                                      NULL);
  _gdk_device_manager->display = _gdk_display;

  _gdk_win32_lang_notification_init ();
  _gdk_drag_init ();
  _gdk_drop_init ();

  _gdk_display->clipboard = gdk_win32_clipboard_new (_gdk_display);
  _gdk_display->primary_clipboard = gdk_clipboard_new (_gdk_display);

  /* Precalculate display name */
  (void) gdk_display_get_name (_gdk_display);

  register_display_change_notification (_gdk_display);

  g_signal_emit_by_name (_gdk_display, "opened");

  GDK_NOTE (MISC, g_print ("... _gdk_display now set up\n"));

  return _gdk_display;
}

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

static GdkSurface *
gdk_win32_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  g_warning ("gdk_display_get_default_group not yet implemented");

  return NULL;
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

  /* Partially supported, see WM_NCHITTEST handler. */
  return TRUE;
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
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (object);

  if (display_win32->hwnd != NULL)
    {
      DestroyWindow (display_win32->hwnd);
      display_win32->hwnd = NULL;
    }

  if (display_win32->have_at_least_win81)
    {
      if (display_win32->shcore_funcs.hshcore != NULL)
        {
          FreeLibrary (display_win32->shcore_funcs.hshcore);
          display_win32->shcore_funcs.hshcore = NULL;
        }
    }

  G_OBJECT_CLASS (gdk_win32_display_parent_class)->dispose (object);
}

static void
gdk_win32_display_finalize (GObject *object)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (object);

  _gdk_win32_display_finalize_cursors (display_win32);
  _gdk_win32_dnd_exit ();
  _gdk_win32_lang_notification_exit ();

  g_ptr_array_free (display_win32->monitors, TRUE);

  while (display_win32->filters)
    _gdk_win32_message_filter_unref (display_win32, display_win32->filters->data);

  G_OBJECT_CLASS (gdk_win32_display_parent_class)->finalize (object);
}

static void
_gdk_win32_enable_hidpi (GdkWin32Display *display)
{
  gboolean check_for_dpi_awareness = FALSE;
  gboolean have_hpi_disable_envvar = FALSE;

  enum dpi_aware_status {
    DPI_STATUS_PENDING,
    DPI_STATUS_SUCCESS,
    DPI_STATUS_DISABLED,
    DPI_STATUS_FAILED
  } status = DPI_STATUS_PENDING;

  if (g_win32_check_windows_version (6, 3, 0, G_WIN32_OS_ANY))
    {
      /* If we are on Windows 8.1 or later, cache up functions from shcore.dll, by all means */
      display->have_at_least_win81 = TRUE;
      display->shcore_funcs.hshcore = LoadLibraryW (L"shcore.dll");

      if (display->shcore_funcs.hshcore != NULL)
        {
          display->shcore_funcs.setDpiAwareFunc =
            (funcSetProcessDpiAwareness) GetProcAddress (display->shcore_funcs.hshcore,
                                                         "SetProcessDpiAwareness");
          display->shcore_funcs.getDpiAwareFunc =
            (funcGetProcessDpiAwareness) GetProcAddress (display->shcore_funcs.hshcore,
                                                         "GetProcessDpiAwareness");

          display->shcore_funcs.getDpiForMonitorFunc =
            (funcGetDpiForMonitor) GetProcAddress (display->shcore_funcs.hshcore,
                                                   "GetDpiForMonitor");
        }
    }
  else
    {
      /* Windows Vista through 8: use functions from user32.dll directly */
      HMODULE user32;

      display->have_at_least_win81 = FALSE;
      user32 = GetModuleHandleW (L"user32.dll");

      if (user32 != NULL)
        {
          display->user32_dpi_funcs.setDpiAwareFunc =
            (funcSetProcessDPIAware) GetProcAddress (user32, "SetProcessDPIAware");
          display->user32_dpi_funcs.isDpiAwareFunc =
            (funcIsProcessDPIAware) GetProcAddress (user32, "IsProcessDPIAware");
        }
    }

  if (g_getenv ("GDK_WIN32_DISABLE_HIDPI") == NULL)
    {
      /* For Windows 8.1 and later, use SetProcessDPIAwareness() */
      if (display->have_at_least_win81)
        {
          /* then make the GDK-using app DPI-aware */
          if (display->shcore_funcs.setDpiAwareFunc != NULL)
            {
              GdkWin32ProcessDpiAwareness hidpi_mode;

              /* TODO: See how per-monitor DPI awareness is done by the Wayland backend */
              if (g_getenv ("GDK_WIN32_PER_MONITOR_HIDPI") != NULL)
                hidpi_mode = PROCESS_PER_MONITOR_DPI_AWARE;
              else
                hidpi_mode = PROCESS_SYSTEM_DPI_AWARE;

              switch (display->shcore_funcs.setDpiAwareFunc (hidpi_mode))
                {
                  case S_OK:
                    display->dpi_aware_type = hidpi_mode;
                    status = DPI_STATUS_SUCCESS;
                    break;
                  case E_ACCESSDENIED:
                    /* This means the app used a manifest to set DPI awareness, or a
                       DPI compatibility setting is used.
                       The manifest is the trump card in this game of bridge here.  The
                       same applies if one uses the control panel or program properties to
                       force system DPI awareness */
                    check_for_dpi_awareness = TRUE;
                    break;
                  default:
                    display->dpi_aware_type = PROCESS_DPI_UNAWARE;
                    status = DPI_STATUS_FAILED;
                    break;
                }
            }
          else
            {
              check_for_dpi_awareness = TRUE;
            }
        }
      else
        {
          /* For Windows Vista through 8, use SetProcessDPIAware() */
          display->have_at_least_win81 = FALSE;
          if (display->user32_dpi_funcs.setDpiAwareFunc != NULL)
            {
              if (display->user32_dpi_funcs.setDpiAwareFunc () != 0)
                {
                  display->dpi_aware_type = PROCESS_SYSTEM_DPI_AWARE;
                  status = DPI_STATUS_SUCCESS;
                }
              else
                {
                  check_for_dpi_awareness = TRUE;
                }
            }
          else
            {
              display->dpi_aware_type = PROCESS_DPI_UNAWARE;
              status = DPI_STATUS_FAILED;
            }
        }
    }
  else
    {
      /* if GDK_WIN32_DISABLE_HIDPI is set, check for any DPI
       * awareness settings done via manifests or user settings
       */
      check_for_dpi_awareness = TRUE;
      have_hpi_disable_envvar = TRUE;
    }

  if (check_for_dpi_awareness)
    {
      if (display->have_at_least_win81)
        {
          if (display->shcore_funcs.getDpiAwareFunc != NULL)
            {
              display->shcore_funcs.getDpiAwareFunc (NULL, &display->dpi_aware_type);

              if (display->dpi_aware_type != PROCESS_DPI_UNAWARE)
                status = DPI_STATUS_SUCCESS;
              else
                /* This means the DPI awareness setting was forcefully disabled */
                status = DPI_STATUS_DISABLED;
            }
          else
            {
              display->dpi_aware_type = PROCESS_DPI_UNAWARE;
              status = DPI_STATUS_FAILED;
            }
        }
      else
        {
          if (display->user32_dpi_funcs.isDpiAwareFunc != NULL)
            {
              /* This most probably means DPI awareness is set through
                 the manifest, or a DPI compatibility setting is used. */
              display->dpi_aware_type = display->user32_dpi_funcs.isDpiAwareFunc () ?
                                        PROCESS_SYSTEM_DPI_AWARE :
                                        PROCESS_DPI_UNAWARE;

              if (display->dpi_aware_type == PROCESS_SYSTEM_DPI_AWARE)
                status = DPI_STATUS_SUCCESS;
              else
                status = DPI_STATUS_DISABLED;
            }
          else
            {
              display->dpi_aware_type = PROCESS_DPI_UNAWARE;
              status = DPI_STATUS_FAILED;
            }
        }
      if (have_hpi_disable_envvar &&
          status == DPI_STATUS_SUCCESS)
        {
          /* The user setting or application manifest trumps over GDK_WIN32_DISABLE_HIDPI */
          g_print ("Note: GDK_WIN32_DISABLE_HIDPI is ignored due to preset\n"
                   "      DPI awareness settings in user settings or application\n"
                   "      manifest, DPI awareness is still enabled.");
        }
    }

  switch (status)
    {
    case DPI_STATUS_SUCCESS:
      GDK_NOTE (MISC, g_message ("HiDPI support enabled, type: %s",
                                 display->dpi_aware_type == PROCESS_PER_MONITOR_DPI_AWARE ? "per-monitor" : "system"));
      break;
    case DPI_STATUS_DISABLED:
      GDK_NOTE (MISC, g_message ("HiDPI support disabled via manifest"));
      break;
    case DPI_STATUS_FAILED:
      g_warning ("Failed to enable HiDPI support.");
      break;
    case DPI_STATUS_PENDING:
      g_assert_not_reached ();
      break;
    }
}

static void
gdk_win32_display_init (GdkWin32Display *display)
{
  const gchar *scale_str = g_getenv ("GDK_SCALE");

  display->monitors = g_ptr_array_new_with_free_func (g_object_unref);

  _gdk_win32_enable_hidpi (display);

  /* if we have DPI awareness, set up fixed scale if set */
  if (display->dpi_aware_type != PROCESS_DPI_UNAWARE &&
      scale_str != NULL)
    {
      display->surface_scale = atol (scale_str);

      if (display->surface_scale <= 0)
        display->surface_scale = 1;

      display->has_fixed_scale = TRUE;
    }
  else
    display->surface_scale = _gdk_win32_display_get_monitor_scale_factor (display, NULL, NULL, NULL);

  _gdk_win32_display_init_cursors (display);
  gdk_win32_display_check_composited (display);
}

void
gdk_win32_display_check_composited (GdkWin32Display *display)
{
  gboolean composited;

  /* On Windows 8 and later, DWM (composition) is always enabled */
  if (g_win32_check_windows_version (6, 2, 0, G_WIN32_OS_ANY))
    {
      composited = TRUE;
    }
  else
    {
      if (DwmIsCompositionEnabled (&composited) != S_OK)
        composited = FALSE;
    }

  gdk_display_set_composited (GDK_DISPLAY (display), composited);
}

static void
gdk_win32_display_notify_startup_complete (GdkDisplay  *display,
                                           const gchar *startup_id)
{
  /* nothing */
}

static int
gdk_win32_display_get_n_monitors (GdkDisplay *display)
{
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  return win32_display->monitors->len;
}


static GdkMonitor *
gdk_win32_display_get_monitor (GdkDisplay *display,
                               int         monitor_num)
{
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  if (monitor_num < 0 || monitor_num >= win32_display->monitors->len)
    return NULL;

  return (GdkMonitor *) g_ptr_array_index (win32_display->monitors, monitor_num);
}

static GdkMonitor *
gdk_win32_display_get_primary_monitor (GdkDisplay *display)
{
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  /* We arrange for the first monitor in the array to also be the primiary monitor */
  if (win32_display->monitors->len > 0)
    return (GdkMonitor *) g_ptr_array_index (win32_display->monitors, 0);

  return NULL;
}

guint
_gdk_win32_display_get_monitor_scale_factor (GdkWin32Display *win32_display,
                                             HMONITOR         hmonitor,
                                             HWND             hwnd,
                                             gint            *dpi)
{
  gboolean is_scale_acquired = FALSE;
  gboolean use_dpi_for_monitor = FALSE;
  guint dpix, dpiy;

  if (win32_display->have_at_least_win81)
    {
      if (hmonitor != NULL)
        use_dpi_for_monitor = TRUE;

      else
        {
          if (hwnd != NULL)
            {
              hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
              use_dpi_for_monitor = TRUE;
            }
        }
    }

  if (use_dpi_for_monitor)
    {
      /* Use GetDpiForMonitor() for Windows 8.1+, when we have a HMONITOR */
      if (win32_display->shcore_funcs.hshcore != NULL &&
          win32_display->shcore_funcs.getDpiForMonitorFunc != NULL)
        {
          if (win32_display->shcore_funcs.getDpiForMonitorFunc (hmonitor,
                                                                MDT_EFFECTIVE_DPI,
                                                                &dpix,
                                                                &dpiy) == S_OK)
            {
              is_scale_acquired = TRUE;
            }
        }
    }
  else
    {
      /* Go back to GetDeviceCaps() for Windows 8 and earler, or when we don't
       * have a HMONITOR nor a HWND
       */
      HDC hdc = GetDC (hwnd);

      /* in case we can't get the DC for the window, return 1 for the scale */
      if (hdc == NULL)
        {
          if (dpi != NULL)
            *dpi = USER_DEFAULT_SCREEN_DPI;

          return 1;
        }

      dpix = GetDeviceCaps (hdc, LOGPIXELSX);
      dpiy = GetDeviceCaps (hdc, LOGPIXELSY);
      ReleaseDC (hwnd, hdc);

      is_scale_acquired = TRUE;
    }

  if (is_scale_acquired)
    /* USER_DEFAULT_SCREEN_DPI = 96, in winuser.h */
    {
      if (dpi != NULL)
        *dpi = dpix;

      if (win32_display->has_fixed_scale)
        return win32_display->surface_scale;
      else
        return dpix / USER_DEFAULT_SCREEN_DPI > 1 ? dpix / USER_DEFAULT_SCREEN_DPI : 1;
    }
  else
    {
      if (dpi != NULL)
        *dpi = USER_DEFAULT_SCREEN_DPI;

      return 1;
  }
}

static gboolean
gdk_win32_display_get_setting (GdkDisplay  *display,
                               const gchar *name,
                               GValue      *value)
{
  return _gdk_win32_get_setting (name, value);
}

static guint32
gdk_win32_display_get_last_seen_time (GdkDisplay *display)
{
  return GetMessageTime ();
}

static void
gdk_win32_display_class_init (GdkWin32DisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->dispose = gdk_win32_display_dispose;
  object_class->finalize = gdk_win32_display_finalize;

  display_class->cairo_context_type = GDK_TYPE_WIN32_CAIRO_CONTEXT;

  display_class->get_name = gdk_win32_display_get_name;
  display_class->beep = gdk_win32_display_beep;
  display_class->sync = gdk_win32_display_sync;
  display_class->flush = gdk_win32_display_flush;
  display_class->has_pending = _gdk_win32_display_has_pending;
  display_class->queue_events = _gdk_win32_display_queue_events;
  display_class->get_default_group = gdk_win32_display_get_default_group;

  display_class->supports_shapes = gdk_win32_display_supports_shapes;
  display_class->supports_input_shapes = gdk_win32_display_supports_input_shapes;

  //? display_class->get_app_launch_context = _gdk_win32_display_get_app_launch_context;

  display_class->get_next_serial = gdk_win32_display_get_next_serial;
  display_class->notify_startup_complete = gdk_win32_display_notify_startup_complete;
  display_class->create_surface = _gdk_win32_display_create_surface;

  display_class->get_keymap = _gdk_win32_display_get_keymap;
  display_class->text_property_to_utf8_list = _gdk_win32_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_win32_display_utf8_to_string_target;
  display_class->make_gl_context_current = _gdk_win32_display_make_gl_context_current;

  display_class->get_n_monitors = gdk_win32_display_get_n_monitors;
  display_class->get_monitor = gdk_win32_display_get_monitor;
  display_class->get_primary_monitor = gdk_win32_display_get_primary_monitor;

#ifdef GDK_RENDERING_VULKAN
  display_class->vk_context_type = GDK_TYPE_WIN32_VULKAN_CONTEXT;
  display_class->vk_extension_name = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif

  display_class->get_setting = gdk_win32_display_get_setting;
  display_class->get_last_seen_time = gdk_win32_display_get_last_seen_time;
  display_class->set_cursor_theme = gdk_win32_display_set_cursor_theme;

  _gdk_win32_surfaceing_init ();
}
