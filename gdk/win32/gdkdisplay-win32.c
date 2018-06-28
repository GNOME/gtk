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

#ifdef HAVE_HARFBUZZ
# include <initguid.h>
# include <pango/pangowin32.h>
extern GType _pango_win32_font_get_type (void) G_GNUC_CONST;

# define PANGO_TYPE_WIN32_FONT            (_pango_win32_font_get_type ())
# define PANGO_WIN32_FONT(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), PANGO_TYPE_WIN32_FONT, PangoWin32Font))
# define PANGO_WIN32_FONT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANGO_TYPE_WIN32_FONT, PangoWin32FontClass))
# define PANGO_WIN32_IS_FONT(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), PANGO_TYPE_WIN32_FONT))
# define PANGO_WIN32_IS_FONT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANGO_TYPE_WIN32_FONT))
# define PANGO_WIN32_FONT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANGO_TYPE_WIN32_FONT, PangoWin32FontClass))
#endif

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

#ifdef HAVE_HARFBUZZ
  g_hash_table_destroy (display_win32->ht_pango_dwrite_fonts);
  g_hash_table_destroy (display_win32->ht_pango_logfonts);
  g_hash_table_destroy (display_win32->ht_pango_win32_ft_faces);
  g_hash_table_destroy (display_win32->ht_pango_win32_ft_data);
  if (display_win32->ft_lib != NULL)
    FT_Done_FreeType (display_win32->ft_lib);

  if (display_win32->dwrite_gdi_interop != NULL)
    IDWriteGdiInterop_Release (display_win32->dwrite_gdi_interop);

  if (display_win32->dwrite_factory != NULL)
    IDWriteFactory_Release (display_win32->dwrite_factory);
#endif
  G_OBJECT_CLASS (gdk_win32_display_parent_class)->dispose (object);
}

static void
gdk_win32_display_finalize (GObject *object)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (object);

  _gdk_win32_display_finalize_cursors (display_win32);
  _gdk_win32_dnd_exit ();

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

#ifdef HAVE_HARFBUZZ
/*
 * We need to release the IDWriteFonts using IDWriteFont_Release(),
 * but since it is a macro that depends on what we feed to it, we
 * need a small C-function that calls that for us, so that we can pass
 * it to g_hash_table_new_full()
 */
static void
gdk_win32_release_dwrite_font (IDWriteFont *dwrite_font)
{
  IDWriteFont_Release (dwrite_font);
}
#endif

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

#ifdef HAVE_HARFBUZZ
  display->ht_pango_dwrite_fonts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, gdk_win32_release_dwrite_font);
  display->ht_pango_logfonts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  display->ht_pango_win32_ft_faces = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, FT_Done_Face);
  display->ht_pango_win32_ft_data = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
#endif
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

#ifdef HAVE_HARFBUZZ
static IDWriteGdiInterop *
gdk_win32_display_get_dwrite_gdi_interop (GdkWin32Display *display)
{
  HRESULT hr = S_OK;

  if (display->dwrite_factory == NULL)
    hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
                              &IID_IDWriteFactory,
                              (IUnknown **) &display->dwrite_factory);

  if (SUCCEEDED (hr) && display->dwrite_gdi_interop == NULL)
    hr = IDWriteFactory_GetGdiInterop (display->dwrite_factory,
                                       &display->dwrite_gdi_interop);

  if (FAILED (hr))
    {
      g_critical ("Failed to initialize DirectWrite, font tweaking will likely not work!");
      display->dwrite_factory = NULL;
      display->dwrite_gdi_interop = NULL;
    }

  return display->dwrite_gdi_interop;
}

static FT_Library
gdk_win32_display_get_ft_lib (GdkWin32Display *display)
{
  FT_Error ft_err = FT_Err_Ok;

  if (display->ft_lib == NULL)
    ft_err = FT_Init_FreeType (&display->ft_lib);

  if (ft_err != FT_Err_Ok)
    {
      g_critical ("Failed to initialize FreeType, font tweaking will likely not work!");
      display->ft_lib = NULL;
    }

  return (display->ft_lib);
}

IDWriteFont *
gdk_win32_get_dwrite_font_from_pango_font (GdkDisplay *display,
                                           PangoFont  *pango_font)
{
  IDWriteFont *dwrite_font = NULL;
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);

  dwrite_font = g_hash_table_lookup (display_win32->ht_pango_dwrite_fonts,
                                     pango_font);

  /* If we are using PangoWin32, we need to turn the LOGFONTW to a IDWriteFont */
  if (dwrite_font == NULL && PANGO_WIN32_IS_FONT (pango_font))
    {
      /* first acquire the LOGFONTW from the pango_font that we are using, so that we can feed it into DirectWrite */
      LOGFONTW *logfont = pango_win32_font_logfontw (pango_font);
	  HRESULT hr = S_OK;
      gboolean succeeded = TRUE;

      IDWriteGdiInterop *dwrite_gdi_interop = NULL;

      dwrite_gdi_interop = gdk_win32_display_get_dwrite_gdi_interop (display_win32);

      if (dwrite_gdi_interop == NULL)
        {
          g_critical ("Failed to initialize DirectWrite.  Font tweaking will likely not work!");
          succeeded = FALSE;
        }

      if (FAILED (IDWriteGdiInterop_CreateFontFromLOGFONT (dwrite_gdi_interop,
                                                           logfont,
                                                           &dwrite_font)))
        {
          g_warning ("Could not acquire DirectWrite Font from LOGFONTW!");
          succeeded = FALSE;
        }

      /* cache up the LOGFONTW/IDWriteFont */
      if (!g_hash_table_insert (display_win32->ht_pango_logfonts, pango_font, logfont))
        g_warning ("Could not cache LOGFONTW!");
      if (!g_hash_table_insert (display_win32->ht_pango_dwrite_fonts, pango_font, dwrite_font))
        g_warning ("Could not cache DirectWrite Font!");

      if (!succeeded)
        dwrite_font = NULL;
    }

  return dwrite_font;
}

FT_Face
gdk_win32_get_ft_face_from_pango_font (GdkDisplay *display,
                                       PangoFont  *pango_font)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  FT_Face ft_face = NULL;

  ft_face = g_hash_table_lookup (display_win32->ht_pango_win32_ft_faces, pango_font);

  if (ft_face == NULL && PANGO_WIN32_IS_FONT (pango_font))
    {
	  HRESULT hr = S_OK;

      IDWriteFont *dwrite_font = NULL;
      IDWriteFontFace *dwrite_font_face = NULL;
      IDWriteFontFile *dwrite_font_file = NULL;
      IDWriteFontFileLoader *dwrite_font_file_loader = NULL;
      IDWriteFontFileStream *dwrite_font_file_stream = NULL;
      IDWriteLocalizedStrings *lstrings = NULL;

      /*
      IDWriteTextFormat *text_format = NULL;
      IDWriteFontFamily *family = NULL;
      DWRITE_FONT_WEIGHT font_weight;
      DWRITE_FONT_STYLE font_style;
      DWRITE_FONT_STRETCH font_stretch; */
      void *dwrite_ref_key;
      FT_Library ft_lib;
      FT_Byte *font_file_data = NULL;
      FT_Error ft_err;

      BOOL property_found = FALSE;
      UINT32 count, i, size, dwrite_ref_key_size;
      UINT64 font_file_size;
      const void *font_fragment_start;
      void *font_fragment_ctx;
      gboolean succeeded = TRUE;
      gboolean release_fragment_ctx = FALSE;
	  gchar *font_name = NULL;
      wchar_t *wstr = NULL;

      /*
      wchar_t *family_name = NULL;
      wchar_t locale[LOCALE_NAME_MAX_LENGTH];
      wchar_t *font_file_path = NULL;
      gchar *str = NULL;
      UINT32 nlangs = 0;*/

      ft_lib = gdk_win32_display_get_ft_lib (display_win32);

      if (ft_lib == NULL)
        {
          succeeded = FALSE;
          goto dwrite_fail;
        }

      dwrite_font = gdk_win32_get_dwrite_font_from_pango_font (display, pango_font);

      if (dwrite_font == NULL)
        {
          succeeded = FALSE;
          goto dwrite_fail;
        }
      /*
      font_weight = IDWriteFont_GetWeight (dwrite_font);
      font_style = IDWriteFont_GetStyle (dwrite_font);
      font_stretch = IDWriteFont_GetStretch (dwrite_font); */

      hr = IDWriteFont_GetInformationalStrings (dwrite_font,
                                                DWRITE_INFORMATIONAL_STRING_FULL_NAME,
                                                &lstrings,
                                                &property_found);

      if (SUCCEEDED (hr) && property_found)
        {
          if (SUCCEEDED (IDWriteLocalizedStrings_GetStringLength (lstrings, 0, &size)))
            {
              wstr = g_new0 (wchar_t, size + 1);
              IDWriteLocalizedStrings_GetString (lstrings, 0, wstr, size + 1);
              font_name = g_utf16_to_utf8 (wstr, -1, NULL, NULL, NULL);
              g_free (wstr);
              IDWriteLocalizedStrings_Release (lstrings);
            }
        }

      if (SUCCEEDED (hr) && !property_found)
        g_print ("Could not find full name of DirectWriteFont %p!\n", dwrite_font);

      if (FAILED (IDWriteFont_CreateFontFace (dwrite_font,
                                              &dwrite_font_face)))
        {
          g_warning ("Could not create Font Face for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

      if (FAILED (IDWriteFontFace_GetFiles (dwrite_font_face, &size, NULL)) ||
          FAILED (IDWriteFontFace_GetFiles (dwrite_font_face, &size, &dwrite_font_file)))
        {
          g_warning ("Could not obtain files from Font Face for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

      if (FAILED (IDWriteFontFile_GetReferenceKey (dwrite_font_file,
                                                   &dwrite_ref_key,
                                                   &dwrite_ref_key_size)))
        {
          g_warning ("Could not obtain reference key from Font Face for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

      if (FAILED (IDWriteFontFile_GetLoader (dwrite_font_file, &dwrite_font_file_loader)))
        {
          g_warning ("Could not obtain loader from Font Face for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

      if (FAILED (IDWriteFontFileLoader_CreateStreamFromKey (dwrite_font_file_loader,
                                                             dwrite_ref_key,
                                                             dwrite_ref_key_size,
                                                             &dwrite_font_file_stream)))
        {
          g_warning ("Could not obtain Font Face file stream for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

      if (FAILED (IDWriteFontFileStream_GetFileSize (dwrite_font_file_stream, &font_file_size)))
        {
          g_warning ("Could not acquire font file size for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

      if (FAILED (IDWriteFontFileStream_ReadFileFragment (dwrite_font_file_stream,
                                                          &font_fragment_start,
                                                          0,
                                                          font_file_size,
                                                          &font_fragment_ctx)))
        {
          g_warning ("Could not acquire raw font data for font for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }
      else
        release_fragment_ctx = TRUE;

      font_file_data = g_new0 (FT_Byte, font_file_size);
      memcpy (font_file_data, font_fragment_start, (size_t) font_file_size);
      ft_err = FT_New_Memory_Face (ft_lib, font_file_data, font_file_size, 0, &ft_face);

      if (ft_err != FT_Err_Ok)
        {
          g_warning ("Unable to obtain FreeType Face for font %s", font_name);
          succeeded = FALSE;
          goto dwrite_fail;
        }

dwrite_fail:
      if (release_fragment_ctx)
        IDWriteFontFileStream_ReleaseFileFragment (dwrite_font_file_stream, &font_fragment_ctx);
      if (dwrite_font_file_stream != NULL)
        IDWriteFontFileStream_Release (dwrite_font_file_stream);
      if (dwrite_font_file_loader != NULL)
        IDWriteFontFileLoader_Release (dwrite_font_file_loader);
      if (dwrite_font_file != NULL)
        IDWriteFontFile_Release (dwrite_font_file);
      if (dwrite_font_face != NULL)
        IDWriteFontFace_Release (dwrite_font_face);

      if (font_name != NULL)
        g_free (font_name);

      if (succeeded)
        {
          g_hash_table_insert (display_win32->ht_pango_win32_ft_data, pango_font, font_file_data);
          g_hash_table_insert (display_win32->ht_pango_win32_ft_faces, pango_font, ft_face);
        }
      else
        ft_face = NULL;
#if 0
      /* Use FreeType & HarfBuzz for now */
      hr = IDWriteFont_GetInformationalStrings (dwrite_font,
                                                DWRITE_INFORMATIONAL_STRING_FULL_NAME,
                                                &lstrings,
                                                &property_found);

      if (SUCCEEDED (hr) && property_found)
        {
          count = IDWriteLocalizedStrings_GetCount (lstrings);
          g_print ("DWRITE_INFORMATIONAL_STRING_FULL_NAME strings: %d\n", count);

          for (i = 0; i < count; i++)
            {
              hr = IDWriteLocalizedStrings_GetStringLength (lstrings, i, &size);
              if (SUCCEEDED (hr))
                {
                  wstr = g_new0 (wchar_t, size + 1);
                  hr = IDWriteLocalizedStrings_GetString (lstrings, i, wstr, size + 1);
                  str = g_utf16_to_utf8 (wstr, -1, NULL, NULL, NULL);
                  g_print ("index[%d]: %s\n", i, str);
                  g_free (str);
                  g_free (wstr);
                }
              else
                break;
            }
        }

      if (SUCCEEDED (hr) && !property_found)
        g_print ("Could not find full name of font!\n");

      if (FAILED (hr))
        {
          g_warning ("Failed to acquire full name of font!\n");
          return;
        }

      if (SUCCEEDED (hr) && property_found)
        IDWriteLocalizedStrings_Release (lstrings);

      /* Get the family of the font to get the locales/languages that it supports */
      if (SUCCEEDED (hr))
        hr = IDWriteFont_GetFontFamily (dwrite_font, &family);

      if (SUCCEEDED (hr))
        hr = IDWriteFontFamily_GetFamilyNames (family, &lstrings);

      if (SUCCEEDED (hr))
        nlangs = IDWriteLocalizedStrings_GetCount (lstrings);

      IDWriteLocalizedStrings_GetLocaleName (lstrings, 0, locale, LOCALE_NAME_MAX_LENGTH);
      wstr = g_new0 (wchar_t, LOCALE_NAME_MAX_LENGTH);

      for (i = 0; i < nlangs; i++)
        {
          IDWriteLocalizedStrings_GetLocaleName (lstrings, i, wstr, LOCALE_NAME_MAX_LENGTH);

          if (SUCCEEDED (hr))
            {
               wchar_t *disp_strw = NULL;
               LCTYPE disp_type = g_win32_check_windows_version (6, 1, 0, G_WIN32_OS_ANY) ?
                                  LOCALE_SLOCALIZEDDISPLAYNAME :
                                  LOCALE_SLOCALIZEDLANGUAGENAME;
               size = GetLocaleInfoEx (wstr, disp_type, disp_strw, 0);
               if (size != 0)
                 disp_strw = g_new0 (wchar_t, size);
               if ((size != 0 && GetLocaleInfoEx (wstr, disp_type, disp_strw, size)) != 0)
                 {
                   gchar *localestr = g_utf16_to_utf8 (wstr, -1, NULL, NULL, NULL);
                   gchar *disp_str = g_utf16_to_utf8 (disp_strw, size - 1, NULL, NULL, NULL);
                   g_print ("locale %d: %s\n", i, localestr);
                   g_free (disp_strw);
                   g_free (localestr);
                   g_free (disp_str);
                 }
               else
                 {
                   g_print ("Uh-oh, we could not acquire the localized locale name, %d\n", GetLastError ());
                   return;
                 }
             }
           else
             {
                g_warning ("Unable to acquire font locale with DirectWrite\n");
                return;
             }
         }
      g_free (wstr);

      if (SUCCEEDED (hr))
        hr = IDWriteFont_GetInformationalStrings (dwrite_font,
                                                  DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
                                                  &lstrings,
                                                  &property_found);

      if (SUCCEEDED (hr))
        {
          if (property_found)
            {
              count = IDWriteLocalizedStrings_GetCount (lstrings);
              g_print ("DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES strings: %d\n", count);

              hr = IDWriteLocalizedStrings_GetStringLength (lstrings, 0, &size);
              if (SUCCEEDED (hr))
                {
                  family_name = g_new0 (wchar_t, size + 1);
                  hr = IDWriteLocalizedStrings_GetString (lstrings, 0, family_name, size + 1);
                  str = g_utf16_to_utf8 (family_name, -1, NULL, NULL, NULL);
                  g_print ("Win32 Family Name: %s\n", str);
                  g_free (str);
                  IDWriteLocalizedStrings_Release (lstrings);
                }
            }
          else
            g_print ("Could not find language/script tag of font!\n");
        }

	  if (FAILED (hr))
        {
          g_warning ("Failed to acquire the supported script/language tags!\n");
          if (family_name != NULL)
            g_free (family_name);
          return;
        }

      if (SUCCEEDED (hr))
        hr = IDWriteFont_GetInformationalStrings (dwrite_font,
                                                  DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG,
                                                  &lstrings,
                                                  &property_found);

      if (SUCCEEDED (hr))
        {
          if (property_found)
            {
              count = IDWriteLocalizedStrings_GetCount (lstrings);
              g_print ("DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG strings: %d\n", count);
              for (i = 0; i < count; i++)
                {
                  hr = IDWriteLocalizedStrings_GetStringLength (lstrings, i, &size);
                  if (SUCCEEDED (hr))
                    {
                      gchar * localestr = NULL;

                      wstr = g_new0 (wchar_t, size + 1);
                      hr = IDWriteLocalizedStrings_GetString (lstrings, i, wstr, size + 1);
                      str = g_utf16_to_utf8 (wstr, -1, NULL, NULL, NULL);
                      g_print ("index[%d]: %s\n", i, str);
                      g_free (str);
                      g_free (wstr);
                    }
                  else
                    break;
                }

              IDWriteLocalizedStrings_Release (lstrings);
            }
          else
            g_print ("Could not find language/script tag of font!\n");
        }
      else
        {
          g_warning ("Failed to acquire the supported script/language tags!\n");
          return;
        }

      hr = IDWriteFactory_CreateTextFormat (priv->dwrite_factory,
                                            family_name,
                                            NULL,
                                            font_weight,
                                            font_style,
                                            font_stretch,
                                            72.0f,
                                            locale,
                                            &text_format);

      g_print ("Text format: %s\n", SUCCEEDED (hr) ? "yes" : "no");
      if (family_name != NULL)
        g_free (family_name);

      IDWriteTextFormat_Release (text_format);
      IDWriteFontFamily_Release (family);
#endif /* if 0 */
    }

  return ft_face;
}
#endif

static void
gdk_win32_display_class_init (GdkWin32DisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->dispose = gdk_win32_display_dispose;
  object_class->finalize = gdk_win32_display_finalize;

  display_class->surface_type = GDK_TYPE_WIN32_SURFACE;
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
  display_class->create_surface_impl = _gdk_win32_display_create_surface_impl;

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
