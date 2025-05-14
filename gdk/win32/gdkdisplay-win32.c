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

#define VK_USE_PLATFORM_WIN32_KHR

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkcairocontext-win32.h"
#include "gdkclipboardprivate.h"
#include "gdkclipboard-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkdevicemanager-win32.h"
#include "gdkglcontext-win32.h"
#include "gdkinput-dmanipulation.h"
#include "gdksurface-win32.h"
#include "gdkwin32display.h"
#include "gdkwin32surface.h"
#include "gdkmonitor-win32.h"
#include "gdkwin32.h"
#include "gdkvulkancontext-win32.h"

#include <dwmapi.h>
#include <shellscalingapi.h>

#ifdef HAVE_EGL
# include <epoxy/egl.h>
#endif

#ifndef IMAGE_FILE_MACHINE_ARM64
# define IMAGE_FILE_MACHINE_ARM64 0xAA64
#endif

/**
 * gdk_win32_display_add_filter:
 * @display: a `GdkWin32Display`
 * @function: (scope notified): filter callback
 * @data: (closure): data to pass to filter callback
 *
 * Adds an event filter to @window, allowing you to intercept messages
 * before they reach GDK. This is a low-level operation and makes it
 * easy to break GDK and/or GTK, so you have to know what you're
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
 * @display: A `GdkWin32Display`
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
 * @display: A `GdkWin32Display`
 * @function: (scope notified): previously-added filter function
 * @data: (closure): user data for previously-added filter function
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

  for (i = 0; i < g_list_model_get_n_items (win32_display->monitors); i++)
    {
      GdkWin32Monitor *m;

      m = g_list_model_get_item (win32_display->monitors, i);
      g_object_unref (m);

      if (_gdk_win32_monitor_compare (m, GDK_WIN32_MONITOR (needle)) == 0)
        return GDK_MONITOR (m);
    }

  return NULL;
}

static void
gdk_win32_display_init_monitors (GdkWin32Display *win32_display)
{
  GPtrArray *new_monitors;
  int i;
  GdkWin32Monitor *primary_to_move = NULL;

  for (i = 0; i < g_list_model_get_n_items (win32_display->monitors); i++)
    {
      GdkWin32Monitor *m;

      m = g_list_model_get_item (win32_display->monitors, i);
      m->remove = TRUE;
      g_object_unref (m);
    }

  new_monitors = _gdk_win32_display_get_monitor_list (win32_display);

  for (i = 0; i < new_monitors->len; i++)
    {
      GdkWin32Monitor *w32_m;
      GdkMonitor *m;
      GdkWin32Monitor *w32_ex_monitor;
      GdkMonitor *ex_monitor;
      GdkRectangle geometry;
      GdkRectangle workarea, ex_workarea;

      w32_m = GDK_WIN32_MONITOR (g_ptr_array_index (new_monitors, i));
      m = GDK_MONITOR (w32_m);
      ex_monitor = _gdk_win32_display_find_matching_monitor (win32_display, m);
      w32_ex_monitor = GDK_WIN32_MONITOR (ex_monitor);

      if (ex_monitor == NULL)
        {
          w32_m->add = TRUE;
          continue;
        }

      w32_ex_monitor->remove = FALSE;

      if (i == 0)
        primary_to_move = w32_ex_monitor;

      gdk_monitor_get_geometry (m, &geometry);
      gdk_win32_monitor_get_workarea (m, &workarea);
      gdk_win32_monitor_get_workarea (ex_monitor, &ex_workarea);

      if (memcmp (&workarea, &ex_workarea, sizeof (GdkRectangle)) != 0)
        {
          w32_ex_monitor->work_rect = workarea;
        }

      gdk_monitor_set_geometry (ex_monitor, &geometry);

      if (gdk_monitor_get_width_mm (m) != gdk_monitor_get_width_mm (ex_monitor) ||
          gdk_monitor_get_height_mm (m) != gdk_monitor_get_height_mm (ex_monitor))
        {
          gdk_monitor_set_physical_size (ex_monitor,
                                         gdk_monitor_get_width_mm (m),
                                         gdk_monitor_get_height_mm (m));
        }

      if (g_strcmp0 (gdk_monitor_get_model (m), gdk_monitor_get_model (ex_monitor)) != 0)
        {
          gdk_monitor_set_model (ex_monitor,
                                 gdk_monitor_get_model (m));
        }

      if (g_strcmp0 (gdk_monitor_get_manufacturer (m), gdk_monitor_get_manufacturer (ex_monitor)) != 0)
        {
          gdk_monitor_set_manufacturer (ex_monitor,
                                        gdk_monitor_get_manufacturer (m));
        }

      if (gdk_monitor_get_refresh_rate (m) != gdk_monitor_get_refresh_rate (ex_monitor))
        {
          gdk_monitor_set_refresh_rate (ex_monitor, gdk_monitor_get_refresh_rate (m));
        }

      if (gdk_monitor_get_scale_factor (m) != gdk_monitor_get_scale_factor (ex_monitor))
        {
          gdk_monitor_set_scale_factor (ex_monitor, gdk_monitor_get_scale_factor (m));
        }

      if (gdk_monitor_get_subpixel_layout (m) != gdk_monitor_get_subpixel_layout (ex_monitor))
        {
          gdk_monitor_set_subpixel_layout (ex_monitor, gdk_monitor_get_subpixel_layout (m));
        }
    }

  for (i = g_list_model_get_n_items (win32_display->monitors) - 1; i >= 0; i--)
    {
      GdkWin32Monitor *w32_ex_monitor;
      GdkMonitor *ex_monitor;

      w32_ex_monitor = GDK_WIN32_MONITOR (g_list_model_get_item (win32_display->monitors, i));
      ex_monitor = GDK_MONITOR (w32_ex_monitor);

      if (w32_ex_monitor->remove)
        {
          w32_ex_monitor->hmonitor = NULL;
          g_list_store_remove (G_LIST_STORE (win32_display->monitors), i);
          gdk_monitor_invalidate (ex_monitor);
        }

      g_object_unref (w32_ex_monitor);
    }

  for (i = 0; i < new_monitors->len; i++)
    {
      GdkWin32Monitor *w32_m;

      w32_m = GDK_WIN32_MONITOR (g_ptr_array_index (new_monitors, i));

      if (!w32_m->add)
        continue;

      if (i == 0)
        g_list_store_insert (G_LIST_STORE (win32_display->monitors), 0, w32_m);
      else
        g_list_store_append (G_LIST_STORE (win32_display->monitors), w32_m);
    }

  g_ptr_array_free (new_monitors, TRUE);

  if (primary_to_move)
    {
      guint pos;
      g_object_ref (primary_to_move);
      if (g_list_store_find (G_LIST_STORE (win32_display->monitors), primary_to_move, &pos))
        g_list_store_remove (G_LIST_STORE (win32_display->monitors), pos);
      g_list_store_insert (G_LIST_STORE (win32_display->monitors), 0, primary_to_move);
      g_object_unref (primary_to_move);
    }
}


/**
 * gdk_win32_display_set_cursor_theme:
 * @display: (type GdkWin32Display): a `GdkDisplay`
 * @name: (nullable): the name of the cursor theme to use, or %NULL
 *   to unset a previously set value
 * @size: the cursor size to use, or 0 to keep the previous size
 *
 * Sets the cursor theme from which the images for cursor
 * should be taken.
 *
 * If the windowing system supports it, existing cursors created
 * with [ctor@Gdk.Cursor.new_from_name] are updated to reflect the theme
 * change. Custom cursors constructed with [ctor@Gdk.Cursor.new_from_texture]
 * will have to be handled by the application (GTK applications can
 * learn about cursor theme changes by listening for change notification
 * for the corresponding `GtkSetting`).
 */
void
gdk_win32_display_set_cursor_theme (GdkDisplay  *display,
                                    const char *name,
                                    int          size)
{
  int cursor_size;
  int w, h;
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

static gboolean
gdk_win32_display_create_d3d_devices (GdkWin32Display    *self,
                                      DXGI_ADAPTER_FLAG   required,
                                      DXGI_ADAPTER_FLAG   disallowed,
                                      ID3D11Device      **d3d11_device,
                                      ID3D12Device      **d3d12_device,
                                      GError            **error)
{
  IDXGIAdapter1 *adapter;
  HRESULT hr;
  guint i;

  if (d3d11_device && !gdk_has_feature (GDK_FEATURE_D3D11))
    {
      g_set_error (error, GDK_WIN32_HRESULT_ERROR, DXGI_ERROR_NOT_FOUND, "D3D11 disabled via GDK_DISABLE");
      return FALSE;
    }
  if (d3d12_device && !gdk_has_feature (GDK_FEATURE_D3D12))
    {
      g_set_error (error, GDK_WIN32_HRESULT_ERROR, DXGI_ERROR_NOT_FOUND, "D3D12 disabled via GDK_DISABLE");
      return FALSE;
    }

  /* This function records the first error in the error variable and then
     sets error = NULL to ignore future errors. */

  for (i = 0; IDXGIFactory4_EnumAdapters1 (self->dxgi_factory, i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
    {
      DXGI_ADAPTER_DESC1 desc;

      hr = IDXGIAdapter1_GetDesc1 (adapter, &desc);
      if (!gdk_win32_check_hresult (hr, error, "Failed to get adapter description"))
        {
          error = NULL;
          gdk_win32_com_clear (&adapter);
          continue;
        }

      if ((desc.Flags & required) != required ||
          (desc.Flags & disallowed) != 0)
        {
          gdk_win32_com_clear (&adapter);
          continue;
        }

      if (d3d11_device != NULL)
        {
          hr = D3D11CreateDevice ((IDXGIAdapter *) adapter,
                                  D3D_DRIVER_TYPE_UNKNOWN,
                                  NULL,
                                  D3D11_CREATE_DEVICE_BGRA_SUPPORT, /* Cairo needs this */
                                  NULL,
                                  0,
                                  D3D11_SDK_VERSION,
                                  d3d11_device,
                                  NULL,
                                  NULL);
          if (!gdk_win32_check_hresult (hr, error, "Failed to create D3D11 device"))
            {
              error = NULL;
              gdk_win32_com_clear (&adapter);
              continue;
            }
        }

      if (d3d12_device != NULL)
        {
          hr = D3D12CreateDevice ((IUnknown *) adapter,
                                  D3D_FEATURE_LEVEL_12_0,
                                  &IID_ID3D12Device,
                                  (void **) d3d12_device);
          if (!gdk_win32_check_hresult (hr, error, "Failed to create D3D12 device"))
            {
              error = NULL;
              if (d3d11_device)
                gdk_win32_com_clear (d3d11_device);
              gdk_win32_com_clear (&adapter);
              continue;
            }
        }

      gdk_win32_com_clear (&adapter);
      return TRUE;
    }

  g_set_error (error, GDK_WIN32_HRESULT_ERROR, DXGI_ERROR_NOT_FOUND, "No adapters available");
  return FALSE;
}

static void
gdk_win32_display_init_dcomp (GdkWin32Display *self)
{
  const GUID my_IID_IDCompositionDevice = { 0xC37EA93A,0xE7AA,0x450D,0xB1,0x6F,0x97,0x46,0xCB,0x04,0x07,0xF3 };
  IDXGIDevice *dxgi_device;

  if (!gdk_has_feature (GDK_FEATURE_DCOMP))
    return;
  
  hr_warn (ID3D11Device_QueryInterface (self->d3d11_device, &IID_IDXGIDevice, (void **) &dxgi_device));

  hr_warn (DCompositionCreateDevice (dxgi_device, &my_IID_IDCompositionDevice, (void **) &self->dcomp_device));

  gdk_win32_com_clear (&dxgi_device);
}

static gboolean
gdk_win32_display_init_d3d (GdkWin32Display  *self,
                            GError          **error)
{
  GError *errors[6] = { NULL, };
  HRESULT hr;
  gboolean res;
  gsize i;

  hr = CreateDXGIFactory1 (&IID_IDXGIFactory4, (void **) &self->dxgi_factory);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create DXGI factory"))
    return FALSE;

  res = gdk_win32_display_create_d3d_devices (self, 0, DXGI_ADAPTER_FLAG_SOFTWARE, &self->d3d11_device, &self->d3d12_device, &errors[0]) ||
        gdk_win32_display_create_d3d_devices (self, 0, DXGI_ADAPTER_FLAG_SOFTWARE, &self->d3d11_device, NULL, &errors[1]) ||
        gdk_win32_display_create_d3d_devices (self, 0, DXGI_ADAPTER_FLAG_SOFTWARE, NULL, &self->d3d12_device, &errors[2]) ||
        gdk_win32_display_create_d3d_devices (self, DXGI_ADAPTER_FLAG_SOFTWARE, 0, &self->d3d11_device, &self->d3d12_device, &errors[3]) ||
        gdk_win32_display_create_d3d_devices (self, DXGI_ADAPTER_FLAG_SOFTWARE, 0, &self->d3d11_device, NULL, &errors[4]) ||
        gdk_win32_display_create_d3d_devices (self, DXGI_ADAPTER_FLAG_SOFTWARE, 0, NULL, &self->d3d12_device, &errors[5]);

  if (error && !res)
    {
      for (i = 0; i < G_N_ELEMENTS (errors); i++)
        {
          if (!g_error_matches (errors[i], GDK_WIN32_HRESULT_ERROR, DXGI_ERROR_NOT_FOUND))
            {
              *error = g_error_copy (errors[i]);
              break;
            }
        }
      
      if (i == G_N_ELEMENTS (errors))
        *error = g_error_copy (errors[0]);
    }

    for (i = 0; i < G_N_ELEMENTS (errors); i++)
      g_clear_error (&errors[i]);

  return res;
}

/*<private>
 * gdk_win32_display_get_dcomp_device:
 * @self: the display
 *
 * Gets the Direct Composition device used to composite
 * the UI.
 * 
 * If Direct Composition is not supported, NULL is returned.
 * 
 * Note that Wine does not support Direct Composition at this point.
 *
 * Returns: (nullable): the device
 */
IDCompositionDevice *
gdk_win32_display_get_dcomp_device (GdkWin32Display *self)
{
  return self->dcomp_device;
}

/*<private>
 * gdk_win32_display_get_dxgi_factory:
 * @self: the display
 *
 * Gets the factory used to create rendering resources.
 *
 * Returns: the factory
 */
IDXGIFactory4 *
gdk_win32_display_get_dxgi_factory (GdkWin32Display *self)
{
  return self->dxgi_factory;
}

/*<private>
 * gdk_win32_display_get_d3d11_device:
 * @self: the display
 *
 * Gets the D3D11 device used for rendering. It will be
 * created with the factory from
 * gdk_win32_display_get_dxgi_factory() and refer to the same adapter
 * as the D3D12 device.
 *
 * Returns: the D3D11 device used for rendering
 */
ID3D11Device *
gdk_win32_display_get_d3d11_device (GdkWin32Display *self)
{
  return self->d3d11_device;
}

/*<private>
 * gdk_win32_display_get_d3d12_device:
 * @self: the display
 *
 * Gets the D3D12 device used for rendering. It will be
 * created with the factory from
 * gdk_win32_display_get_dxgi_factory() and refer to the same adapter
 * as the D3D11 device.
 *
 * If D3D12 is not supported on this computer, NULL is returned.
 *
 * According to the Steam hardware survey, 5-10% of people were
 * still using Windows without D3D12 in early 2025.
 *
 * Returns: (nullable) the D3D12 device used for rendering
 */
ID3D12Device *
gdk_win32_display_get_d3d12_device (GdkWin32Display *self)
{
  return self->d3d12_device;
}

static LRESULT CALLBACK
inner_display_change_hwnd_procedure (HWND   hwnd,
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
        GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (GetWindowLongPtr (hwnd, GWLP_USERDATA));

        gdk_win32_display_init_monitors (win32_display);
        return 0;
      }
    default:
      /* Otherwise call DefWindowProcW(). */
      GDK_NOTE (EVENTS, g_print (" DefWindowProcW"));
      return DefWindowProc (hwnd, message, wparam, lparam);
    }
}

static LRESULT CALLBACK
display_change_hwnd_procedure (HWND   hwnd,
                                 UINT   message,
                                 WPARAM wparam,
                                 LPARAM lparam)
{
  LRESULT retval;
  GdkDisplay *display;

  if (message == WM_NCCREATE)
    {
      CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
      display = (GdkDisplay *)cs->lpCreateParams;

      SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) display);
      retval = TRUE;
    }
  else
    {
      int debug_indent;

	  display = GDK_DISPLAY (GetWindowLongPtr (hwnd, GWLP_USERDATA));

      debug_indent = GDK_WIN32_DISPLAY (display)->event_record->debug_indent_displaychange;
      GDK_NOTE (EVENTS, g_print ("%s%*s%s %p",
                (debug_indent > 0 ? "\n" : ""),
                 debug_indent, "",
                _gdk_win32_message_to_string (message), hwnd));

      GDK_WIN32_DISPLAY (display)->event_record->debug_indent_displaychange += 2;

      retval = inner_display_change_hwnd_procedure (hwnd, message, wparam, lparam);

      GDK_WIN32_DISPLAY (display)->event_record->debug_indent_displaychange -= 2;
      SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) display);
      debug_indent = GDK_WIN32_DISPLAY (display)->event_record->debug_indent_displaychange;
      GDK_NOTE (EVENTS, g_print (" => %" G_GINT64_FORMAT "%s", (gint64) retval, (debug_indent == 0 ? "\n" : "")));
    }

  return retval;
}

/* Use a hidden HWND to be notified about display changes */
static void
register_display_change_notification (GdkDisplay *display)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  WNDCLASS wclass = { 0, };
  ATOM klass;

  wclass.lpszClassName = L"GdkDisplayChange";
  wclass.lpfnWndProc = display_change_hwnd_procedure;
  wclass.hInstance = this_module ();
  wclass.style = CS_OWNDC;

  klass = RegisterClass (&wclass);
  if (klass)
    {
      display_win32->hwnd = CreateWindow (MAKEINTRESOURCE (klass),
                                          NULL, WS_POPUP,
                                          0, 0, 0, 0, NULL, NULL,
                                          this_module (), display);
      if (!display_win32->hwnd)
        {
          UnregisterClass (MAKEINTRESOURCE (klass), this_module ());
        }
    }
}

GdkDisplay *
_gdk_win32_display_open (const char *display_name)
{
  static gsize display_inited = 0;
  static GdkDisplay *display = NULL;
  GdkWin32Display *win32_display;
  GError *error = NULL;

  GDK_NOTE (MISC, g_print ("gdk_display_open: %s\n", (display_name ? display_name : "NULL")));

  if (display != NULL)
    {
      GDK_NOTE (MISC, g_print ("... Display is already open\n"));
      return NULL;
    }

  if (display_name != NULL)
    {
      /* we don't really support multiple GdkDisplay's on Windows at this point */
      GDK_NOTE (MISC, g_print ("... win32 does not support named displays, but given name was \"%s\"\n", display_name));
      return NULL;
    }

  if (g_once_init_enter (&display_inited))
    {
      display = g_object_new (GDK_TYPE_WIN32_DISPLAY, NULL);

      win32_display = GDK_WIN32_DISPLAY (display);

      gdk_win32_display_init_monitors (win32_display);
      _gdk_events_init (display);

      win32_display->device_manager = g_object_new (GDK_TYPE_DEVICE_MANAGER_WIN32,
                                                    "display", display,
                                                    NULL);
      gdk_dmanipulation_initialize (win32_display);

      gdk_win32_display_lang_notification_init (win32_display);
      _gdk_drag_init ();

      display->clipboard = gdk_win32_clipboard_new (display);
      display->primary_clipboard = gdk_clipboard_new (display);

      if (!gdk_win32_display_init_d3d (win32_display, &error))
        {
          GDK_DEBUG (D3D12, "Failed to initialize D3D: %s", error->message);
          g_clear_error (&error);
        }
      gdk_win32_display_init_dcomp (win32_display);

      /* Precalculate display name */
      gdk_display_get_name (display);

      register_display_change_notification (display);

      g_signal_emit_by_name (display, "opened");

      /* Precalculate keymap, see #6203 */
      gdk_display_get_keymap (display);

      GDK_NOTE (MISC, g_print ("... gdk_display now set up\n"));

      g_once_init_leave (&display_inited, 1);
    }

  return display;
}

G_DEFINE_TYPE (GdkWin32Display, gdk_win32_display, GDK_TYPE_DISPLAY)

static const char *
gdk_win32_display_get_name (GdkDisplay *display)
{
  static const char *display_name_cache = NULL;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (g_once_init_enter (&display_name_cache))
    {
      HDESK hdesk = GetThreadDesktop (GetCurrentThreadId ());
      char dummy;
      char dummy_dword[11];
      wchar_t *desktop_name;
      HWINSTA hwinsta = GetProcessWindowStation ();
      wchar_t *window_station_name;
      DWORD n;
      DWORD session_id;
      wchar_t *display_name_w;
      char *display_name;
      size_t wchar_size;
      size_t display_name_len = 0;

      n = 0;
      GetUserObjectInformation (hdesk, UOI_NAME, &dummy, 0, &n);
      wchar_size = sizeof (wchar_t);
      if (n == 0)
        desktop_name = L"Default";
      else
        {
          n++;
          desktop_name = g_alloca ((n + 1) * wchar_size);
          memset (desktop_name, 0, (n + 1) * wchar_size);

          if (!GetUserObjectInformation (hdesk, UOI_NAME, desktop_name, n, &n))
          desktop_name = L"Default";
        }

      n = 0;
      GetUserObjectInformation (hwinsta, UOI_NAME, &dummy, 0, &n);
      if (n == 0)
        window_station_name = L"WinSta0";
      else
        {
          n++;
          window_station_name = g_alloca ((n + 1) * wchar_size);
          memset (window_station_name, 0, (n + 1) * wchar_size);

          if (!GetUserObjectInformation (hwinsta, UOI_NAME, window_station_name, n, &n))
            window_station_name = L"WinSta0";
        }

      if (!ProcessIdToSessionId (GetCurrentProcessId (), &session_id))
        session_id = 0;

      /* display_name is in the form of "%ld\\%s\\%s" */
      display_name_len = strlen (itoa (session_id, dummy_dword, 10)) + 1 + wcslen (window_station_name) + 1 + wcslen (desktop_name);
      display_name_w = g_alloca ((display_name_len + 1) * wchar_size);
      memset (display_name_w, 0, (display_name_len + 1) * wchar_size);
      swprintf_s (display_name_w, display_name_len + 1, L"%ld\\%s\\%s", session_id, window_station_name, desktop_name);

      display_name = g_utf16_to_utf8 (display_name_w, -1, NULL, NULL, NULL);

      GDK_NOTE (MISC, g_print ("gdk_win32_display_get_name: %s\n", display_name));

      g_once_init_leave (&display_name_cache, display_name);
    }

  return display_name_cache;
}

static void
gdk_win32_display_beep (GdkDisplay *display)
{
  MessageBeep ((UINT)-1);
}

static void
gdk_win32_display_flush (GdkDisplay * display)
{
}

static void
gdk_win32_display_sync (GdkDisplay * display)
{
}

static void
gdk_win32_display_dispose (GObject *object)
{
  GdkWin32Display *self = GDK_WIN32_DISPLAY (object);

  if (self->hwnd != NULL)
    {
      DestroyWindow (self->hwnd);
      self->hwnd = NULL;
    }

  gdk_win32_com_clear (&self->d3d12_device);
  gdk_win32_com_clear (&self->dxgi_factory);

  G_OBJECT_CLASS (gdk_win32_display_parent_class)->dispose (object);
}

static void
gdk_win32_display_finalize (GObject *object)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (object);

  g_free (display_win32->event_record);
  if (display_win32->display_surface_record->modal_timer != 0)
    {
      KillTimer (NULL, display_win32->display_surface_record->modal_timer);
	  display_win32->display_surface_record->modal_timer = 0;
    }
  g_slist_free_full (display_win32->display_surface_record->modal_surface_stack, g_object_unref);
  g_hash_table_destroy (display_win32->display_surface_record->handle_ht);
  g_free (display_win32->display_surface_record);
  _gdk_win32_display_finalize_cursors (display_win32);
  gdk_win32_display_close_dmanip_manager (GDK_DISPLAY (display_win32));
  _gdk_win32_dnd_exit ();
  gdk_win32_display_lang_notification_exit (display_win32);
  g_free (display_win32->pointer_device_items);
  g_object_unref (display_win32->cb_dnd_items->clipdrop);
  g_free (display_win32->cb_dnd_items);

  g_list_store_remove_all (G_LIST_STORE (display_win32->monitors));
  g_object_unref (display_win32->monitors);

  while (display_win32->filters)
    _gdk_win32_message_filter_unref (display_win32, display_win32->filters->data);

  G_OBJECT_CLASS (gdk_win32_display_parent_class)->finalize (object);
}

static void
_gdk_win32_enable_hidpi (GdkWin32Display *display)
{
  gboolean check_for_dpi_awareness = FALSE;
  gboolean have_hpi_disable_envvar = FALSE;

  HMODULE user32;
  user32 = GetModuleHandleW (L"user32.dll");

  enum dpi_aware_status {
    DPI_STATUS_PENDING,
    DPI_STATUS_SUCCESS,
    DPI_STATUS_DISABLED,
    DPI_STATUS_FAILED
  } status = DPI_STATUS_PENDING;

  if (user32 != NULL)
    {
      display->user32_dpi_funcs.setPDAC =
        (funcSPDAC) GetProcAddress (user32, "SetProcessDpiAwarenessContext");
      display->user32_dpi_funcs.getTDAC =
        (funcGTDAC) GetProcAddress (user32, "GetThreadDpiAwarenessContext");
      display->user32_dpi_funcs.areDACEqual =
        (funcADACE) GetProcAddress (user32, "AreDpiAwarenessContextsEqual");
    }

  if (g_getenv ("GDK_WIN32_DISABLE_HIDPI") == NULL)
    {
      /* then make the GDK-using app DPI-aware */
      if (display->user32_dpi_funcs.setPDAC != NULL)
        {
          HANDLE hidpi_mode_ctx;
          PROCESS_DPI_AWARENESS hidpi_mode;

          /* TODO: See how per-monitor DPI awareness is done by the Wayland backend */
          if (g_getenv ("GDK_WIN32_PER_MONITOR_HIDPI") != NULL)
            {
              hidpi_mode_ctx = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
              hidpi_mode = PROCESS_PER_MONITOR_DPI_AWARE;
            }
          else
            {
              hidpi_mode_ctx = DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
              hidpi_mode = PROCESS_SYSTEM_DPI_AWARE;
            }

          if (display->user32_dpi_funcs.setPDAC (hidpi_mode_ctx))
            {
              display->dpi_aware_type = hidpi_mode;
              status = DPI_STATUS_SUCCESS;
            }
          else
            {
              DWORD err = GetLastError ();

              if (err == ERROR_ACCESS_DENIED)
                check_for_dpi_awareness = TRUE;
              else
                {
                  display->dpi_aware_type = PROCESS_DPI_UNAWARE;
                  status = DPI_STATUS_FAILED;
                }
            }
        }
      else
        {
          PROCESS_DPI_AWARENESS hidpi_mode;

          /* TODO: See how per-monitor DPI awareness is done by the Wayland backend */
          if (g_getenv ("GDK_WIN32_PER_MONITOR_HIDPI") != NULL)
            hidpi_mode = PROCESS_PER_MONITOR_DPI_AWARE;
          else
            hidpi_mode = PROCESS_SYSTEM_DPI_AWARE;

          switch (SetProcessDpiAwareness (hidpi_mode))
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
      if (display->user32_dpi_funcs.getTDAC != NULL &&
          display->user32_dpi_funcs.areDACEqual != NULL)
        {
          HANDLE dpi_aware_ctx = display->user32_dpi_funcs.getTDAC ();
          if (display->user32_dpi_funcs.areDACEqual (dpi_aware_ctx,
                                                      DPI_AWARENESS_CONTEXT_UNAWARE))
            /* This means the DPI awareness setting was forcefully disabled */
            status = DPI_STATUS_DISABLED;
          else
            {
              status = DPI_STATUS_SUCCESS;
              if (display->user32_dpi_funcs.areDACEqual (dpi_aware_ctx,
                                                          DPI_AWARENESS_CONTEXT_SYSTEM_AWARE))
                display->dpi_aware_type = PROCESS_SYSTEM_DPI_AWARE;
              else
                display->dpi_aware_type = PROCESS_PER_MONITOR_DPI_AWARE;
            }
        }
      else
        {
          GetProcessDpiAwareness (NULL, &display->dpi_aware_type);

          if (display->dpi_aware_type != PROCESS_DPI_UNAWARE)
            status = DPI_STATUS_SUCCESS;
          else
            /* This means the DPI awareness setting was forcefully disabled */
            status = DPI_STATUS_DISABLED;
        }

      if (have_hpi_disable_envvar &&
          status == DPI_STATUS_SUCCESS)
        {
          /* The user setting or application manifest trumps over GDK_WIN32_DISABLE_HIDPI */
          g_print ("Note: GDK_WIN32_DISABLE_HIDPI is ignored due to preset\n"
                   "      DPI awareness settings in user settings or application\n"
                   "      manifest, DPI awareness is still enabled.\n");
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

gboolean
_gdk_win32_check_processor (GdkWin32ProcessorCheckType check_type)
{
  static gsize checked = 0;
  static gboolean is_arm64 = FALSE;
  static gboolean is_wow64 = FALSE;

  if (g_once_init_enter (&checked))
    {
      gboolean fallback_wow64_check = FALSE;
      HMODULE kernel32 = LoadLibraryW (L"kernel32.dll");

      if (kernel32 != NULL)
        {
          typedef BOOL (WINAPI *funcIsWow64Process2) (HANDLE, USHORT *, USHORT *);

          funcIsWow64Process2 isWow64Process2 =
            (funcIsWow64Process2) GetProcAddress (kernel32, "IsWow64Process2");

          if (isWow64Process2 != NULL)
            {
              USHORT proc_cpu = 0;
              USHORT native_cpu = 0;

              isWow64Process2 (GetCurrentProcess (), &proc_cpu, &native_cpu);

              if (native_cpu == IMAGE_FILE_MACHINE_ARM64)
                is_arm64 = TRUE;

              if (proc_cpu != IMAGE_FILE_MACHINE_UNKNOWN)
                is_wow64 = TRUE;
            }
          else
            {
              fallback_wow64_check = TRUE;
            }

          FreeLibrary (kernel32);
        }
      else
        {
          fallback_wow64_check = TRUE;
        }

      if (fallback_wow64_check)
        IsWow64Process (GetCurrentProcess (), &is_wow64);

      g_once_init_leave (&checked, 1);
    }

  switch (check_type)
    {
      case GDK_WIN32_ARM64:
        return is_arm64;
        break;

      case GDK_WIN32_WOW64:
        return is_wow64;
        break;

      default:
        g_warning ("unknown CPU check type");
        return FALSE;
        break;
    }
}

static guint
gdk_handle_hash (HANDLE *handle)
{
#ifdef _WIN64
  return ((guint *) handle)[0] ^ ((guint *) handle)[1];
#else
  return (guint) *handle;
#endif
}

static int
gdk_handle_equal (HANDLE *a,
                  HANDLE *b)
{
  return (*a == *b);
}

static void
gdk_win32_display_init (GdkWin32Display *display_win32)
{
  display_win32->monitors = G_LIST_MODEL (g_list_store_new (GDK_TYPE_MONITOR));
  display_win32->pointer_device_items = g_new0 (GdkWin32PointerDeviceItems, 1);
  display_win32->cb_dnd_items = g_new0 (GdkWin32CbDnDItems, 1);
  display_win32->cb_dnd_items->display_main_thread = g_thread_self ();
  display_win32->cb_dnd_items->clipdrop = GDK_WIN32_CLIPDROP (g_object_new (GDK_TYPE_WIN32_CLIPDROP, NULL));
  display_win32->display_surface_record = g_new0 (surface_records, 1);
  display_win32->display_surface_record->handle_ht = g_hash_table_new ((GHashFunc) gdk_handle_hash,
                                                                       (GEqualFunc) gdk_handle_equal);

  display_win32->event_record = g_new0 (event_records, 1);
  _gdk_win32_enable_hidpi (display_win32);
  display_win32->running_on_arm64 = _gdk_win32_check_processor (GDK_WIN32_ARM64);

  display_win32->surface_scale = gdk_win32_display_get_monitor_scale_factor (display_win32, NULL, NULL);

  _gdk_win32_display_init_cursors (display_win32);
  gdk_display_set_composited (GDK_DISPLAY (display_win32), TRUE);
  gdk_display_set_shadow_width (GDK_DISPLAY (display_win32), TRUE);
}

static void
gdk_win32_display_notify_startup_complete (GdkDisplay  *display,
                                           const char *startup_id)
{
  /* nothing */
}

GdkMonitor *
gdk_win32_display_get_primary_monitor (GdkDisplay *display)
{
  GdkWin32Display *self = GDK_WIN32_DISPLAY (display);
  GdkMonitor *result;

  /* We arrange for the first monitor in the array to also be the primary monitor */
  result = g_list_model_get_item (self->monitors, 0);
  g_object_unref (result);

  return result;
}

static GListModel *
gdk_win32_display_get_monitors (GdkDisplay *display)
{
  GdkWin32Display *self = GDK_WIN32_DISPLAY (display);

  return self->monitors;
}

guint
gdk_win32_display_get_monitor_scale_factor (GdkWin32Display *display_win32,
                                            GdkSurface      *surface,
                                            HMONITOR         hmonitor)
{
  gboolean is_scale_acquired = FALSE;
  guint dpix, dpiy;

  if (surface != NULL && hmonitor == NULL)
    hmonitor = MonitorFromWindow (GDK_SURFACE_HWND (surface),
                                  MONITOR_DEFAULTTONEAREST);

  if (hmonitor != NULL)
    {
      /* Use GetDpiForMonitor() when we have a HMONITOR */
      if (GetDpiForMonitor (hmonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy) == S_OK)
        is_scale_acquired = TRUE;
    }
  else
    {
      /* Go back to GetDeviceCaps() when we don't have a HMONITOR nor a HWND */
      HDC hdc;

      if (surface != NULL)
        hdc = GetDC (GDK_SURFACE_HWND (surface));
      else
        hdc = GetDC (NULL);

      /* in case we can't get the DC for the HWND, return 1 for the scale */
      if (hdc == NULL)
        return 1;

      dpix = GetDeviceCaps (hdc, LOGPIXELSX);
      dpiy = GetDeviceCaps (hdc, LOGPIXELSY);

      if (surface != NULL)
        ReleaseDC (GDK_SURFACE_HWND (surface), hdc);
      else
        ReleaseDC (NULL, hdc);

      is_scale_acquired = TRUE;
    }

  if (is_scale_acquired)
    return dpix / USER_DEFAULT_SCREEN_DPI > 1 ? dpix / USER_DEFAULT_SCREEN_DPI : 1;
  else
    return 1;
}

#ifndef EGL_PLATFORM_ANGLE_ANGLE
#define EGL_PLATFORM_ANGLE_ANGLE          0x3202
#endif

static GdkGLContext *
gdk_win32_display_init_gl (GdkDisplay  *display,
                           GError     **error)
{
#ifdef HAVE_EGL
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  HDC init_gl_hdc = NULL;
#endif
  GdkGLContext *context;

  /*
   * No env vars set, do the regular GL initialization, first WGL and then EGL,
   * as WGL is the more tried-and-tested configuration.
   */

#ifdef HAVE_EGL
  /*
   * Disable defaulting to EGL as EGL is used more as a compatibility layer
   * on Windows rather than being a native citizen on Windows
   */
  if (!gdk_has_feature (GDK_FEATURE_WGL) || !gdk_has_feature (GDK_FEATURE_GL_API))
    {
      init_gl_hdc = GetDC (display_win32->hwnd);

      if (gdk_display_init_egl (display,
                                EGL_PLATFORM_ANGLE_ANGLE,
                                init_gl_hdc,
                                FALSE,
                                error))
        {
          return g_object_new (GDK_TYPE_WIN32_GL_CONTEXT_EGL,
                               "display", display,
                               "allowed-apis", GDK_GL_API_GLES,
                               NULL);
        }
      else
        g_clear_error (error);
    }
#endif

  context = gdk_win32_display_init_wgl (display, error);

  if (context)
    return context;

#ifdef HAVE_EGL
  g_clear_error (error);

  init_gl_hdc = GetDC (display_win32->hwnd);
  if (gdk_display_init_egl (display,
                            EGL_PLATFORM_ANGLE_ANGLE,
                            init_gl_hdc,
                            TRUE,
                            error))
    {
      return g_object_new (GDK_TYPE_WIN32_GL_CONTEXT_EGL,
                           "display", display,
                           NULL);

    }
#endif

  return NULL;
}

/**
 * gdk_win32_display_get_egl_display:
 * @display: (type GdkWin32Display): a Win32 display
 *
 * Retrieves the EGL display connection object for the given GDK display.
 *
 * Returns: (nullable): the EGL display
 *
 * Since: 4.4
 */
gpointer
gdk_win32_display_get_egl_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WIN32_DISPLAY (display), NULL);

  return gdk_display_get_egl_display (display);
}

GdkWin32Clipdrop *
gdk_win32_display_get_clipdrop (GdkDisplay *display)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);

  return display_win32->cb_dnd_items->clipdrop;
}

static GdkKeymap*
_gdk_win32_display_get_keymap (GdkDisplay *display)
{
  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  return gdk_win32_display_get_default_keymap (GDK_WIN32_DISPLAY (display));
}

static void
gdk_win32_display_class_init (GdkWin32DisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->dispose = gdk_win32_display_dispose;
  object_class->finalize = gdk_win32_display_finalize;

  display_class->toplevel_type = GDK_TYPE_WIN32_TOPLEVEL;
  display_class->popup_type = GDK_TYPE_WIN32_POPUP;
  display_class->cairo_context_type = GDK_TYPE_WIN32_CAIRO_CONTEXT;

  display_class->get_name = gdk_win32_display_get_name;
  display_class->beep = gdk_win32_display_beep;
  display_class->sync = gdk_win32_display_sync;
  display_class->flush = gdk_win32_display_flush;
  display_class->queue_events = _gdk_win32_display_queue_events;

  //? display_class->get_app_launch_context = _gdk_win32_display_get_app_launch_context;

  display_class->get_next_serial = gdk_win32_display_get_next_serial;
  display_class->notify_startup_complete = gdk_win32_display_notify_startup_complete;

  display_class->get_keymap = _gdk_win32_display_get_keymap;

  display_class->get_monitors = gdk_win32_display_get_monitors;

#ifdef GDK_RENDERING_VULKAN
  display_class->vk_context_type = GDK_TYPE_WIN32_VULKAN_CONTEXT;
  display_class->vk_extension_name = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif

  display_class->get_setting = gdk_win32_display_get_setting;
  display_class->set_cursor_theme = gdk_win32_display_set_cursor_theme;
  display_class->init_gl = gdk_win32_display_init_gl;
}
