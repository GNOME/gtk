/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 the GTK team
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

#include <gdk/gdk.h>
#include "gdkwin32.h"
#include "gdkprivate-win32.h"
#include "gdkdevicemanager-win32.h"
#include "gdkdevice-winpointer.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkseatdefaultprivate.h"
#include "gdkdevicetoolprivate.h"

#include <windows.h>

#include <tchar.h>
#include <tpcshrd.h>
#include <hidsdi.h>

#define HID_STRING_BYTES_LIMIT 200
#define VID_PID_CHARS 4

typedef BOOL
(WINAPI *registerPointerDeviceNotifications_t)(HWND window, BOOL notifyRange);
typedef BOOL
(WINAPI *getPointerDevices_t)(UINT32 *deviceCount, POINTER_DEVICE_INFO *pointerDevices);
typedef BOOL
(WINAPI *getPointerDeviceCursors_t)(HANDLE device, UINT32 *cursorCount, POINTER_DEVICE_CURSOR_INFO *deviceCursors);
typedef BOOL
(WINAPI *getPointerDeviceRects_t)(HANDLE device, RECT *pointerDeviceRect, RECT *displayRect);
typedef BOOL
(WINAPI *getPointerType_t)(UINT32 pointerId, POINTER_INPUT_TYPE *pointerType);
typedef BOOL
(WINAPI *getPointerCursorId_t)(UINT32 pointerId, UINT32 *cursorId);
typedef BOOL
(WINAPI *getPointerPenInfoHistory_t)(UINT32 pointerId, UINT32 *entriesCount, POINTER_PEN_INFO *penInfo);
typedef BOOL
(WINAPI *getPointerTouchInfoHistory_t)(UINT32 pointerId, UINT32 *entriesCount, POINTER_TOUCH_INFO *touchInfo);
typedef BOOL
(WINAPI *setGestureConfig_t)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);

static registerPointerDeviceNotifications_t registerPointerDeviceNotifications;
static getPointerDevices_t getPointerDevices;
static getPointerDeviceCursors_t getPointerDeviceCursors;
static getPointerDeviceRects_t getPointerDeviceRects;
static getPointerType_t getPointerType;
static getPointerCursorId_t getPointerCursorId;
static getPointerPenInfoHistory_t getPointerPenInfoHistory;
static getPointerTouchInfoHistory_t getPointerTouchInfoHistory;
static setGestureConfig_t setGestureConfig;

static ATOM notifications_window_class;
static HWND notifications_window_handle;

static inline double
utils_rect_width (RECT *rect)
{
  return rect->right - rect->left;
}

static inline double
utils_rect_height (RECT *rect)
{
  return rect->bottom - rect->top;
}

static inline gboolean
utils_rect_is_degenerate (RECT *rect)
{
  return utils_rect_width (rect) == 0 || utils_rect_height (rect) == 0;
}

static gboolean
winpointer_device_update_scale_factors (GdkDeviceWinpointer *device)
{
  RECT device_rect;
  RECT display_rect;

  if (!getPointerDeviceRects (device->device_handle, &device_rect, &display_rect))
    {
      WIN32_API_FAILED ("GetPointerDeviceRects");
      return FALSE;
    }

  if (utils_rect_is_degenerate (&device_rect))
    {
      g_warning ("Invalid coordinates from GetPointerDeviceRects");
      return FALSE;
    }

  device->origin_x = display_rect.left;
  device->origin_y = display_rect.top;
  device->scale_x = utils_rect_width (&display_rect) / utils_rect_width (&device_rect);
  device->scale_y = utils_rect_height (&display_rect) / utils_rect_height (&device_rect);

  return TRUE;
}

static void
winpointer_get_device_details (HANDLE device,
                               char *vid,
                               char *pid,
                               char **manufacturer,
                               char **product)
{
  RID_DEVICE_INFO info;
  UINT wchars_count = 0;
  UINT size = 0;

  memset (&info, 0, sizeof (info));

  info.cbSize = sizeof (info);
  size = sizeof (info);

  if (GetRawInputDeviceInfoW (device, RIDI_DEVICEINFO, &info, &size) > 0 &&
      info.dwType == RIM_TYPEHID &&
      info.hid.dwVendorId > 0 &&
      info.hid.dwProductId > 0)
    {
      const char *format_string = "%0" G_STRINGIFY (VID_PID_CHARS) "x";

      g_snprintf (vid, VID_PID_CHARS + 1, format_string, (unsigned) info.hid.dwVendorId);
      g_snprintf (pid, VID_PID_CHARS + 1, format_string, (unsigned) info.hid.dwProductId);
    }

  if (GetRawInputDeviceInfoW (device, RIDI_DEVICENAME, NULL, &wchars_count) == 0)
    {
      gunichar2 *device_path = g_new0 (gunichar2, wchars_count);

      if (GetRawInputDeviceInfoW (device, RIDI_DEVICENAME, device_path, &wchars_count) > 0)
        {
          HANDLE device_file = CreateFileW (device_path,
                                            0,
                                            FILE_SHARE_READ |
                                            FILE_SHARE_WRITE |
                                            FILE_SHARE_DELETE,
                                            NULL,
                                            OPEN_EXISTING,
                                            FILE_FLAG_SESSION_AWARE,
                                            NULL);

          if (device_file != INVALID_HANDLE_VALUE)
            {
              gunichar2 *buffer = g_malloc0 (HID_STRING_BYTES_LIMIT);

              if (HidD_GetManufacturerString (device_file, buffer, HID_STRING_BYTES_LIMIT))
                if (buffer[0])
                  *manufacturer = g_utf16_to_utf8 (buffer, -1, NULL, NULL, NULL);

              if (HidD_GetProductString (device_file, buffer, HID_STRING_BYTES_LIMIT))
                if (buffer[0])
                  *product = g_utf16_to_utf8 (buffer, -1, NULL, NULL, NULL);

              g_free (buffer);
              CloseHandle (device_file);
            }
        }

      g_free (device_path);
    }
}

static void
winpointer_create_device (POINTER_DEVICE_INFO *info,
                          GdkInputSource source)
{
  GdkDeviceWinpointer *device = NULL;
  GdkSeat *seat = NULL;
  unsigned num_touches = 0;
  char vid[VID_PID_CHARS + 1];
  char pid[VID_PID_CHARS + 1];
  char *manufacturer = NULL;
  char *product = NULL;
  char *base_name = NULL;
  char *name = NULL;
  UINT32 num_cursors = 0;
  GdkAxisFlags axes_flags = 0;

  seat = gdk_display_get_default_seat (_gdk_display);

  memset (pid, 0, VID_PID_CHARS + 1);
  memset (vid, 0, VID_PID_CHARS + 1);

  if (!getPointerDeviceCursors (info->device, &num_cursors, NULL))
    {
      WIN32_API_FAILED ("GetPointerDeviceCursors");
      return;
    }

  if (num_cursors == 0)
    return;

  winpointer_get_device_details (info->device, vid, pid, &manufacturer, &product);

  /* build up the name */
  if (!manufacturer && vid[0])
    manufacturer = g_strdup (vid);

  if (!product && pid[0])
    product = g_strdup (pid);

  if (manufacturer && product)
    base_name = g_strconcat (manufacturer, " ", product, NULL);

  if (!base_name && info->productString[0])
    base_name = g_utf16_to_utf8 (info->productString, -1, NULL, NULL, NULL);

  if (!base_name)
    base_name = g_strdup ("Unnamed");

  switch (source)
    {
    case GDK_SOURCE_PEN:
      name = g_strconcat (base_name, " Pen", NULL);
    break;
    case GDK_SOURCE_TOUCHSCREEN:
      num_touches = info->maxActiveContacts;
      name = g_strconcat (base_name, " Finger touch", NULL);
    break;
    default:
      name = g_strdup (base_name);
    break;
    }

  device = g_object_new (GDK_TYPE_DEVICE_WINPOINTER,
                         "display", _gdk_display,
                         "seat", seat,
                         "has-cursor", TRUE,
                         "source", source,
                         "name", name,
                         "num-touches", num_touches,
                         "vendor-id", vid[0] ? vid : NULL,
                         "product-id", pid[0] ? pid : NULL,
                         NULL);

  switch (source)
    {
    case GDK_SOURCE_PEN:
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_PRESSURE, 0.0, 1.0, 1.0 / 1024.0);
      axes_flags |= GDK_AXIS_FLAG_PRESSURE;
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_XTILT, -1.0, 1.0, 1.0 / 90.0);
      axes_flags |= GDK_AXIS_FLAG_XTILT;
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_YTILT, -1.0, 1.0, 1.0 / 90.0);
      axes_flags |= GDK_AXIS_FLAG_YTILT;
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_ROTATION, 0.0, 1.0, 1.0 / 360.0);
      axes_flags |= GDK_AXIS_FLAG_ROTATION;

      device->num_axes = 4;
    break;
    case GDK_SOURCE_TOUCHSCREEN:
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_PRESSURE, 0.0, 1.0, 1.0 / 1024.0);
      axes_flags |= GDK_AXIS_FLAG_PRESSURE;

      device->num_axes = 1;
    break;
    default:
      g_warn_if_reached ();
    break;
    }

  device->device_handle = info->device;
  device->start_cursor_id = info->startingCursorId;
  device->end_cursor_id = info->startingCursorId + num_cursors - 1;

  device->last_axis_data = g_new0 (double, device->num_axes);

  if (!winpointer_device_update_scale_factors (device))
    {
      g_set_object (&device, NULL);
      goto cleanup;
    }

  switch (source)
    {
    case GDK_SOURCE_PEN:
      {
        device->tool_pen = gdk_device_tool_new (0, 0, GDK_DEVICE_TOOL_TYPE_PEN, axes_flags);
        gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), device->tool_pen);

        device->tool_eraser = gdk_device_tool_new (0, 0, GDK_DEVICE_TOOL_TYPE_ERASER, axes_flags);
        gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), device->tool_pen);

        /* TODO */
        gdk_device_update_tool (GDK_DEVICE (device), device->tool_pen);
      }
    break;
    case GDK_SOURCE_TOUCHSCREEN:
    break;
    default:
      g_warn_if_reached ();
    break;
    }

  _gdk_device_manager->winpointer_devices = g_list_append (_gdk_device_manager->winpointer_devices, device);

  _gdk_device_set_associated_device (GDK_DEVICE (device), _gdk_device_manager->core_pointer);
  _gdk_device_add_physical_device (_gdk_device_manager->core_pointer, GDK_DEVICE (device));

  gdk_seat_default_add_physical_device (GDK_SEAT_DEFAULT (seat), GDK_DEVICE (device));

cleanup:
  g_free (name);
  g_free (base_name);
  g_free (product);
  g_free (manufacturer);
}

static void
winpointer_create_devices (POINTER_DEVICE_INFO *info)
{
  switch (info->pointerDeviceType)
    {
    case POINTER_DEVICE_TYPE_INTEGRATED_PEN:
    case POINTER_DEVICE_TYPE_EXTERNAL_PEN:
      winpointer_create_device (info, GDK_SOURCE_PEN);
    break;
    case POINTER_DEVICE_TYPE_TOUCH:
      winpointer_create_device (info, GDK_SOURCE_TOUCHSCREEN);
    break;
    default:
      g_warn_if_reached ();
    break;
    }
}

static gboolean
winpointer_find_device_in_system_list (GdkDeviceWinpointer *device,
                                       POINTER_DEVICE_INFO *infos,
                                       UINT32 infos_count)
{
  for (UINT32 i = 0; i < infos_count; i++)
    {
      if (device->device_handle == infos[i].device &&
          device->start_cursor_id == infos[i].startingCursorId)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
winpointer_find_system_device_in_device_manager (POINTER_DEVICE_INFO *info)
{
  for (GList *l = _gdk_device_manager->winpointer_devices; l != NULL; l = l->next)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (l->data);

      if (device->device_handle == info->device &&
          device->start_cursor_id == info->startingCursorId)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static void
winpointer_enumerate_devices (void)
{
  POINTER_DEVICE_INFO *infos = NULL;
  UINT32 infos_count = 0;
  UINT32 i = 0;
  GList *current = NULL;

  do
    {
      infos = g_new0 (POINTER_DEVICE_INFO, infos_count);
      if (!getPointerDevices (&infos_count, infos))
        {
          WIN32_API_FAILED ("GetPointerDevices");
          g_free (infos);
          return;
        }
    }
  while (infos_count > 0 && !infos);

  current = _gdk_device_manager->winpointer_devices;

  while (current != NULL)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (current->data);
      GList *next = current->next;

      if (!winpointer_find_device_in_system_list (device, infos, infos_count))
        {
          GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (device));

          _gdk_device_manager->winpointer_devices = g_list_delete_link (_gdk_device_manager->winpointer_devices,
                                                                        current);

          gdk_device_update_tool (GDK_DEVICE (device), NULL);

          if (device->tool_pen)
            gdk_seat_default_remove_tool (GDK_SEAT_DEFAULT (seat), device->tool_pen);

          if (device->tool_eraser)
            gdk_seat_default_remove_tool (GDK_SEAT_DEFAULT (seat), device->tool_eraser);

          _gdk_device_set_associated_device (GDK_DEVICE (device), NULL);
          _gdk_device_remove_physical_device (_gdk_device_manager->core_pointer, GDK_DEVICE (device));

          gdk_seat_default_remove_physical_device (GDK_SEAT_DEFAULT (seat), GDK_DEVICE (device));

          g_object_unref (device);
        }
      else
        {
          winpointer_device_update_scale_factors (device);
        }

      current = next;
    }

  /* create new gdk devices */
  for (i = 0; i < infos_count; i++)
    {
      if (!winpointer_find_system_device_in_device_manager (&infos[i]))
        {
          winpointer_create_devices (&infos[i]);
        }
    }

  g_free (infos);
}

static LRESULT CALLBACK
winpointer_notifications_window_procedure (HWND hWnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam)
{
  switch (uMsg)
    {
    case WM_POINTERDEVICECHANGE:
      winpointer_enumerate_devices ();
      return 0;
    }

  return DefWindowProcW (hWnd, uMsg, wParam, lParam);
}

static gboolean
winpointer_notif_window_create (void)
{
  WNDCLASSEXW wndclassex;

  memset (&wndclassex, 0, sizeof (wndclassex));
  wndclassex.cbSize = sizeof (wndclassex);
  wndclassex.lpszClassName = L"GdkWin32WinpointerNotificationsWindowClass";
  wndclassex.lpfnWndProc = winpointer_notifications_window_procedure;
  wndclassex.hInstance = _gdk_dll_hinstance;

  if ((notifications_window_class = RegisterClassExW (&wndclassex)) == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      return FALSE;
    }

  if (!(notifications_window_handle = CreateWindowExW (0,
                                                       (LPCWSTR)(guintptr)notifications_window_class,
                                                       L"GdkWin32 Winpointer Notifications",
                                                       0,
                                                       0, 0, 0, 0,
                                                       HWND_MESSAGE,
                                                       NULL,
                                                       _gdk_dll_hinstance,
                                                       NULL)))
    {
      WIN32_API_FAILED ("CreateWindowExW");
      return FALSE;
    }

  return TRUE;
}

static gboolean
winpointer_ensure_procedures (void)
{
  static HMODULE user32_dll = NULL;

  if (!user32_dll)
    {
      user32_dll = LoadLibraryW (L"user32.dll");
      if (!user32_dll)
        {
          WIN32_API_FAILED ("LoadLibraryW");
          return FALSE;
        }

      registerPointerDeviceNotifications = (registerPointerDeviceNotifications_t)
        GetProcAddress (user32_dll, "RegisterPointerDeviceNotifications");
      getPointerDevices = (getPointerDevices_t)
        GetProcAddress (user32_dll, "GetPointerDevices");
      getPointerDeviceCursors = (getPointerDeviceCursors_t)
        GetProcAddress (user32_dll, "GetPointerDeviceCursors");
      getPointerDeviceRects = (getPointerDeviceRects_t)
        GetProcAddress (user32_dll, "GetPointerDeviceRects");
      getPointerType = (getPointerType_t)
        GetProcAddress (user32_dll, "GetPointerType");
      getPointerCursorId = (getPointerCursorId_t)
        GetProcAddress (user32_dll, "GetPointerCursorId");
      getPointerPenInfoHistory = (getPointerPenInfoHistory_t)
        GetProcAddress (user32_dll, "GetPointerPenInfoHistory");
      getPointerTouchInfoHistory = (getPointerTouchInfoHistory_t)
        GetProcAddress (user32_dll, "GetPointerTouchInfoHistory");
      setGestureConfig = (setGestureConfig_t)
        GetProcAddress (user32_dll, "SetGestureConfig");
    }

  return registerPointerDeviceNotifications &&
         getPointerDevices &&
         getPointerDeviceCursors &&
         getPointerDeviceRects &&
         getPointerType &&
         getPointerCursorId &&
         getPointerPenInfoHistory &&
         getPointerTouchInfoHistory &&
         setGestureConfig;
}

gboolean
gdk_winpointer_initialize (void)
{
  if (!winpointer_ensure_procedures ())
    return FALSE;

  if (!winpointer_notif_window_create ())
    return FALSE;

  if (!registerPointerDeviceNotifications (notifications_window_handle, FALSE))
    {
      WIN32_API_FAILED ("RegisterPointerDeviceNotifications");
      return FALSE;
    }

  winpointer_enumerate_devices ();

  return TRUE;
}

#ifndef MICROSOFT_TABLETPENSERVICE_PROPERTY
#define MICROSOFT_TABLETPENSERVICE_PROPERTY \
_T("MicrosoftTabletPenServiceProperty")
#endif

void
gdk_winpointer_initialize_surface (GdkSurface *surface)
{
  HWND hwnd = GDK_SURFACE_HWND (surface);
  ATOM key = 0;
  HANDLE val = (HANDLE)(TABLET_DISABLE_PRESSANDHOLD |
                        TABLET_DISABLE_PENTAPFEEDBACK |
                        TABLET_DISABLE_PENBARRELFEEDBACK |
                        TABLET_DISABLE_FLICKS |
                        TABLET_DISABLE_FLICKFALLBACKKEYS);

  winpointer_ensure_procedures ();

  key = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
  API_CALL (SetPropW, (hwnd, (LPCWSTR)(guintptr)key, val));
  GlobalDeleteAtom (key);

  if (setGestureConfig != NULL)
    {
      GESTURECONFIG gesture_config;
      memset (&gesture_config, 0, sizeof (gesture_config));

      gesture_config.dwID = 0;
      gesture_config.dwWant = 0;
      gesture_config.dwBlock = GC_ALLGESTURES;

      API_CALL (setGestureConfig, (hwnd, 0, 1, &gesture_config, sizeof (gesture_config)));
    }
}

void
gdk_winpointer_finalize_surface (GdkSurface *surface)
{
  HWND hwnd = GDK_SURFACE_HWND (surface);
  ATOM key = 0;

  key = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
  RemovePropW (hwnd, (LPCWSTR)(guintptr)key);
  GlobalDeleteAtom (key);
}
