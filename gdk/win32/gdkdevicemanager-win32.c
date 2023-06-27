/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <gdk/gdk.h>
#include "gdkwin32.h"
#include "gdkprivate-win32.h"
#include "gdkdevicemanager-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-win32.h"
#include "gdkdevice-virtual.h"
#include "gdkdevice-winpointer.h"
#include "gdkdevice-wintab.h"
#include "gdkdisplayprivate.h"
#include "gdkdevicetoolprivate.h"
#include "gdkseatdefaultprivate.h"

#include <tchar.h>
#include <tpcshrd.h>
#include <hidsdi.h>
#include "winpointer.h"

#define WINTAB32_DLL "Wintab32.dll"

#define PACKETDATA (PK_CONTEXT | PK_CURSOR | PK_BUTTONS | PK_X | PK_Y  | PK_NORMAL_PRESSURE | PK_ORIENTATION | PK_TANGENT_PRESSURE)
/* We want everything in absolute mode */
#define PACKETMODE (0)
#include <pktdef.h>

#define DEBUG_WINTAB 1		/* Verbose debug messages enabled */
#define TWOPI (2 * G_PI)

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
(WINAPI *getPointerPenInfo_t)(UINT32 pointerId, POINTER_PEN_INFO *penInfo);
typedef BOOL
(WINAPI *getPointerTouchInfo_t)(UINT32 pointerId, POINTER_TOUCH_INFO *touchInfo);
typedef BOOL
(WINAPI *getPointerPenInfoHistory_t)(UINT32 pointerId, UINT32 *entriesCount, POINTER_PEN_INFO *penInfo);
typedef BOOL
(WINAPI *getPointerTouchInfoHistory_t)(UINT32 pointerId, UINT32 *entriesCount, POINTER_TOUCH_INFO *touchInfo);
typedef BOOL
(WINAPI *setGestureConfig_t)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
typedef BOOL
(WINAPI *setWindowFeedbackSetting_t)(HWND hwnd, FEEDBACK_TYPE feedback, DWORD dwFlags, UINT32 size, const VOID *configuration);

static registerPointerDeviceNotifications_t registerPointerDeviceNotifications;
static getPointerDevices_t getPointerDevices;
static getPointerDeviceCursors_t getPointerDeviceCursors;
static getPointerDeviceRects_t getPointerDeviceRects;
static getPointerType_t getPointerType;
static getPointerCursorId_t getPointerCursorId;
static getPointerPenInfo_t getPointerPenInfo;
static getPointerTouchInfo_t getPointerTouchInfo;
static getPointerPenInfoHistory_t getPointerPenInfoHistory;
static getPointerTouchInfoHistory_t getPointerTouchInfoHistory;
static setGestureConfig_t setGestureConfig;
static setWindowFeedbackSetting_t setWindowFeedbackSetting;

static ATOM winpointer_notif_window_class;
static HWND winpointer_notif_window_handle;

static GPtrArray *winpointer_ignored_interactions;

static GList     *wintab_contexts = NULL;
static GdkWindow *wintab_window = NULL;
extern gint       _gdk_input_ignore_core;

typedef UINT (WINAPI *t_WTInfoA) (UINT a, UINT b, LPVOID c);
typedef UINT (WINAPI *t_WTInfoW) (UINT a, UINT b, LPVOID c);
typedef BOOL (WINAPI *t_WTEnable) (HCTX a, BOOL b);
typedef HCTX (WINAPI *t_WTOpenA) (HWND a, LPLOGCONTEXTA b, BOOL c);
typedef BOOL (WINAPI *t_WTGetA) (HCTX a, LPLOGCONTEXTA b);
typedef BOOL (WINAPI *t_WTSetA) (HCTX a, LPLOGCONTEXTA b);
typedef BOOL (WINAPI *t_WTOverlap) (HCTX a, BOOL b);
typedef BOOL (WINAPI *t_WTPacket) (HCTX a, UINT b, LPVOID c);
typedef int (WINAPI *t_WTQueueSizeSet) (HCTX a, int b);

static t_WTInfoA p_WTInfoA;
static t_WTInfoW p_WTInfoW;
static t_WTEnable p_WTEnable;
static t_WTOpenA p_WTOpenA;
static t_WTGetA p_WTGetA;
static t_WTSetA p_WTSetA;
static t_WTOverlap p_WTOverlap;
static t_WTPacket p_WTPacket;
static t_WTQueueSizeSet p_WTQueueSizeSet;

static gboolean default_display_opened = FALSE;

G_DEFINE_TYPE (GdkDeviceManagerWin32, gdk_device_manager_win32, GDK_TYPE_DEVICE_MANAGER)

static GdkDeviceWintab *gdk_device_manager_find_wintab_device (GdkDeviceManagerWin32 *, HCTX, UINT);
UINT _wintab_recognize_new_cursors (GdkDeviceManagerWin32 *, HCTX);
int _gdk_find_wintab_device_index (HCTX);

static GdkDevice *
create_pointer (GdkDeviceManager *device_manager,
		GType g_type,
		const char *name,
		GdkDeviceType type)
{
  return g_object_new (g_type,
                       "name", name,
                       "type", type,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", type == GDK_DEVICE_TYPE_MASTER,
                       "display", gdk_device_manager_get_display (device_manager),
                       "device-manager", device_manager,
                       NULL);
}

static GdkDevice *
create_keyboard (GdkDeviceManager *device_manager,
		 GType g_type,
		 const char *name,
		 GdkDeviceType type)
{
  return g_object_new (g_type,
                       "name", name,
                       "type", type,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", gdk_device_manager_get_display (device_manager),
                       "device-manager", device_manager,
                       NULL);
}

static void
gdk_device_manager_win32_init (GdkDeviceManagerWin32 *device_manager_win32)
{
}

static void
gdk_device_manager_win32_finalize (GObject *object)
{
  GdkDeviceManagerWin32 *device_manager_win32;

  device_manager_win32 = GDK_DEVICE_MANAGER_WIN32 (object);

  g_object_unref (device_manager_win32->core_pointer);
  g_object_unref (device_manager_win32->core_keyboard);

  G_OBJECT_CLASS (gdk_device_manager_win32_parent_class)->finalize (object);
}

static inline double
rect_width (RECT *rect)
{
  return rect->right - rect->left;
}

static inline double
rect_height (RECT *rect)
{
  return rect->bottom - rect->top;
}

static inline gboolean
rect_is_degenerate (RECT *rect)
{
  return rect_width (rect) == 0 || rect_height (rect) == 0;
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

  if (rect_is_degenerate (&device_rect))
    {
      g_warning ("Invalid coordinates from GetPointerDeviceRects");
      return FALSE;
    }

  device->origin_x = display_rect.left;
  device->origin_y = display_rect.top;
  device->scale_x = rect_width (&display_rect) / rect_width (&device_rect);
  device->scale_y = rect_height (&display_rect) / rect_height (&device_rect);

  return TRUE;
}

#define HID_STRING_BYTES_LIMIT 200
#define VID_PID_CHARS 4

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
      gunichar2 *path = g_new0 (gunichar2, wchars_count);

      if (GetRawInputDeviceInfoW (device, RIDI_DEVICENAME, path, &wchars_count) > 0)
        {
          HANDLE device_file = CreateFileW (path,
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

      g_free (path);
    }
}

static void
winpointer_create_device (GdkDeviceManagerWin32 *device_manager,
                          POINTER_DEVICE_INFO *info,
                          GdkInputSource source)
{
  GdkDisplay *display = NULL;
  GdkSeat *seat = NULL;
  GdkDeviceWinpointer *device = NULL;
  unsigned num_touches = 0;
  char vid[VID_PID_CHARS + 1];
  char pid[VID_PID_CHARS + 1];
  char *manufacturer = NULL;
  char *product = NULL;
  char *base_name = NULL;
  char *name = NULL;
  UINT32 num_cursors = 0;

  memset (pid, 0, VID_PID_CHARS + 1);
  memset (vid, 0, VID_PID_CHARS + 1);

  g_object_get (device_manager, "display", &display, NULL);
  seat = gdk_display_get_default_seat (display);

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
      name = g_strconcat (base_name, " Pen stylus", NULL);
    break;
    case GDK_SOURCE_ERASER:
      name = g_strconcat (base_name, " Eraser", NULL);
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
                         "display", display,
                         "device-manager", device_manager,
                         "seat", seat,
                         "type", GDK_DEVICE_TYPE_SLAVE,
                         "input-mode", GDK_MODE_SCREEN,
                         "has-cursor", TRUE,
                         "input-source", source,
                         "name", name,
                         "num-touches", num_touches,
                         "vendor-id", vid[0] ? vid : NULL,
                         "product-id", pid[0] ? pid : NULL,
                         NULL);

  switch (source)
    {
    case GDK_SOURCE_PEN:
    case GDK_SOURCE_ERASER:
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1.0, 1.0 / 1024.0);
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_XTILT, -1.0, 1.0, 1.0 / 90.0);
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_YTILT, -1.0, 1.0, 1.0 / 90.0);
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_ROTATION, 0.0, 1.0, 1.0 / 360.0);

      device->num_axes = 4;
    break;
    case GDK_SOURCE_TOUCHSCREEN:
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1.0, 1.0 / 1024.0);

      device->num_axes = 1;
    break;
    }

  device->device_handle = info->device;
  device->start_cursor_id = info->startingCursorId;
  device->end_cursor_id = info->startingCursorId + num_cursors - 1;

  device->last_axis_data = g_new0 (double, device->num_axes);

  switch (source)
    {
    case GDK_SOURCE_PEN:
      {
        GdkAxisFlags axes = gdk_device_get_axes (GDK_DEVICE (device));
        GdkDeviceTool *tool = gdk_device_tool_new (0, 0, GDK_DEVICE_TOOL_TYPE_PEN, axes);

        gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), tool);
        gdk_device_update_tool (GDK_DEVICE (device), tool);
      }
    break;
    case GDK_SOURCE_ERASER:
      {
        GdkAxisFlags axes = gdk_device_get_axes (GDK_DEVICE (device));
        GdkDeviceTool *tool = gdk_device_tool_new (0, 0, GDK_DEVICE_TOOL_TYPE_ERASER, axes);

        gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), tool);
        gdk_device_update_tool (GDK_DEVICE (device), tool);
      }
    break;
    case GDK_SOURCE_TOUCHSCREEN:
    break;
    }

  if (!winpointer_device_update_scale_factors (device))
    {
      g_set_object (&device, NULL);
      goto cleanup;
    }

  device_manager->winpointer_devices = g_list_append (device_manager->winpointer_devices, device);

  _gdk_device_set_associated_device (GDK_DEVICE (device), device_manager->core_pointer);
  _gdk_device_add_slave (device_manager->core_pointer, GDK_DEVICE (device));

  gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), GDK_DEVICE (device));

  g_signal_emit_by_name (device_manager, "device-added", device);

cleanup:
  g_free (name);
  g_free (base_name);
  g_free (product);
  g_free (manufacturer);
}

static void
winpointer_create_devices (GdkDeviceManagerWin32 *device_manager,
                           POINTER_DEVICE_INFO *info)
{
  switch (info->pointerDeviceType)
    {
    case POINTER_DEVICE_TYPE_INTEGRATED_PEN:
    case POINTER_DEVICE_TYPE_EXTERNAL_PEN:
      winpointer_create_device (device_manager, info, GDK_SOURCE_PEN);
      winpointer_create_device (device_manager, info, GDK_SOURCE_ERASER);
    break;
    case POINTER_DEVICE_TYPE_TOUCH:
      winpointer_create_device (device_manager, info, GDK_SOURCE_TOUCHSCREEN);
    break;
    }
}

static gboolean
winpointer_match_device_in_system_list (GdkDeviceWinpointer *device,
                                        POINTER_DEVICE_INFO *infos,
                                        UINT32 infos_count)
{
  UINT32 i = 0;

  for (i = 0; i < infos_count; i++)
    {
      if (device->device_handle == infos[i].device &&
          device->start_cursor_id == infos[i].startingCursorId)
        return TRUE;
    }

  return FALSE;
}

static gboolean
winpointer_match_system_device_in_device_manager (GdkDeviceManagerWin32 *device_manager,
                                                  POINTER_DEVICE_INFO *info)
{
  GList *l = NULL;

  for (l = device_manager->winpointer_devices; l; l = l->next)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (l->data);

      if (device->device_handle == info->device &&
          device->start_cursor_id == info->startingCursorId)
        return TRUE;
    }

  return FALSE;
}

static void
winpointer_enumerate_devices (GdkDeviceManagerWin32 *device_manager)
{
  POINTER_DEVICE_INFO *infos = NULL;
  UINT32 infos_count = 0;
  UINT32 i = 0;
  GList *current = NULL;

  if (!getPointerDevices (&infos_count, NULL))
    {
      WIN32_API_FAILED ("GetPointerDevices");
      return;
    }

  infos = g_new0 (POINTER_DEVICE_INFO, infos_count);

  /* Note: the device count may increase between the two
   * calls. In such case, the second call will fail with
   * ERROR_INSUFFICIENT_BUFFER.
   * However we'll also get a new WM_POINTERDEVICECHANGE
   * notification, which will start the enumeration again.
   * So do not treat ERROR_INSUFFICIENT_BUFFER as an
   * error, rather return and do the necessary work later
   */

  if (!getPointerDevices (&infos_count, infos))
    {
      if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        WIN32_API_FAILED ("GetPointerDevices");
      g_free (infos);
      return;
    }

  /* remove any gdk device not present anymore or update info */
  current = device_manager->winpointer_devices;
  while (current != NULL)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (current->data);
      GList *next = current->next;

      if (!winpointer_match_device_in_system_list (device, infos, infos_count))
        {
          GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (device));
          GdkDeviceTool *tool = (GDK_DEVICE (device))->last_tool;

          gdk_device_update_tool (GDK_DEVICE (device), NULL);
          gdk_seat_default_remove_tool (GDK_SEAT_DEFAULT (seat), tool);
          g_clear_pointer (&tool, g_object_unref);

          gdk_seat_default_remove_slave (GDK_SEAT_DEFAULT (seat), GDK_DEVICE (device));
          device_manager->winpointer_devices = g_list_delete_link (device_manager->winpointer_devices,
                                                                   current);
          g_signal_emit_by_name (device_manager, "device-removed", device);

          _gdk_device_set_associated_device (GDK_DEVICE (device), NULL);
          _gdk_device_remove_slave (device_manager->core_pointer, GDK_DEVICE (device));

          g_object_unref (device);
        }
      else
        winpointer_device_update_scale_factors (device);

      current = next;
    }

  /* create new gdk devices */
  for (i = 0; i < infos_count; i++)
    if (!winpointer_match_system_device_in_device_manager (device_manager,
                                                           &infos[i]))
      winpointer_create_devices (device_manager, &infos[i]);

  g_free (infos);
}

static LRESULT CALLBACK
winpointer_notif_window_proc (HWND hWnd,
                              UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam)
{
  switch (uMsg)
    {
    case WM_POINTERDEVICECHANGE:
      {
        LONG_PTR user_data = GetWindowLongPtrW (hWnd, GWLP_USERDATA);
        GdkDeviceManagerWin32 *device_manager = GDK_DEVICE_MANAGER_WIN32 (user_data);

        winpointer_enumerate_devices (device_manager);
      }
    return 0;
    }

  return DefWindowProcW (hWnd, uMsg, wParam, lParam);
}

static gboolean
winpointer_notif_window_create ()
{
  WNDCLASSEXW wndclass;

  memset (&wndclass, 0, sizeof (wndclass));
  wndclass.cbSize = sizeof (wndclass);
  wndclass.lpszClassName = L"GdkWin32WinPointerNotificationsWindowClass";
  wndclass.lpfnWndProc = winpointer_notif_window_proc;
  wndclass.hInstance = _gdk_dll_hinstance;

  if ((winpointer_notif_window_class = RegisterClassExW (&wndclass)) == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      return FALSE;
    }

  if (!(winpointer_notif_window_handle = CreateWindowExW (0,
                                                          (LPCWSTR) winpointer_notif_window_class,
                                                          L"GdkWin32 WinPointer Notifications",
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
winpointer_ensure_procedures ()
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
      getPointerPenInfo = (getPointerPenInfo_t)
        GetProcAddress (user32_dll, "GetPointerPenInfo");
      getPointerTouchInfo = (getPointerTouchInfo_t)
        GetProcAddress (user32_dll, "GetPointerTouchInfo");
      getPointerPenInfoHistory = (getPointerPenInfoHistory_t)
        GetProcAddress (user32_dll, "GetPointerPenInfoHistory");
      getPointerTouchInfoHistory = (getPointerTouchInfoHistory_t)
        GetProcAddress (user32_dll, "GetPointerTouchInfoHistory");
      setGestureConfig = (setGestureConfig_t)
        GetProcAddress (user32_dll, "SetGestureConfig");
      setWindowFeedbackSetting = (setWindowFeedbackSetting_t)
        GetProcAddress (user32_dll, "SetWindowFeedbackSetting");
    }

  return registerPointerDeviceNotifications &&
         getPointerDevices &&
         getPointerDeviceCursors &&
         getPointerDeviceRects &&
         getPointerType &&
         getPointerCursorId &&
         getPointerPenInfo &&
         getPointerTouchInfo &&
         getPointerPenInfoHistory &&
         getPointerTouchInfoHistory &&
         setGestureConfig;
}

static gboolean
winpointer_initialize (GdkDeviceManagerWin32 *device_manager)
{
  if (!winpointer_ensure_procedures ())
    return FALSE;

  if (!winpointer_notif_window_create ())
    return FALSE;

  /* associate device_manager with the window */
  SetLastError (0);
  if (SetWindowLongPtrW (winpointer_notif_window_handle,
                         GWLP_USERDATA,
                         (LONG_PTR) device_manager) == 0
      && GetLastError () != 0)
    {
      WIN32_API_FAILED ("SetWindowLongPtrW");
      return FALSE;
    }

  if (!registerPointerDeviceNotifications (winpointer_notif_window_handle, FALSE))
    {
      WIN32_API_FAILED ("RegisterPointerDeviceNotifications");
      return FALSE;
    }

  winpointer_ignored_interactions = g_ptr_array_new ();

  winpointer_enumerate_devices (device_manager);

  return TRUE;
}

void
gdk_winpointer_initialize_window (GdkWindow *window)
{
  HWND hwnd = GDK_WINDOW_HWND (window);
  ATOM key = 0;
  HANDLE val = (HANDLE)(TABLET_DISABLE_PRESSANDHOLD |
                        TABLET_DISABLE_PENTAPFEEDBACK |
                        TABLET_DISABLE_PENBARRELFEEDBACK |
                        TABLET_DISABLE_FLICKS |
                        TABLET_DISABLE_FLICKFALLBACKKEYS);

  winpointer_ensure_procedures ();

  key = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
  API_CALL (SetPropW, (hwnd, (LPCWSTR) key, val));
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

  if (setWindowFeedbackSetting != NULL)
    {
      FEEDBACK_TYPE feedbacks[] = {
        FEEDBACK_TOUCH_CONTACTVISUALIZATION,
        FEEDBACK_PEN_BARRELVISUALIZATION,
        FEEDBACK_PEN_TAP,
        FEEDBACK_PEN_DOUBLETAP,
        FEEDBACK_PEN_PRESSANDHOLD,
        FEEDBACK_PEN_RIGHTTAP,
        FEEDBACK_TOUCH_TAP,
        FEEDBACK_TOUCH_DOUBLETAP,
        FEEDBACK_TOUCH_PRESSANDHOLD,
        FEEDBACK_TOUCH_RIGHTTAP,
        FEEDBACK_GESTURE_PRESSANDTAP
      };
      gsize i = 0;

      for (i = 0; i < G_N_ELEMENTS (feedbacks); i++)
        {
          BOOL setting = FALSE;

          API_CALL (setWindowFeedbackSetting, (hwnd, feedbacks[i], 0, sizeof (BOOL), &setting));
        }
    }
}

void
gdk_winpointer_finalize_window (GdkWindow *window)
{
  HWND hwnd = GDK_WINDOW_HWND (window);
  ATOM key = 0;

  key = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
  RemovePropW (hwnd, (LPCWSTR) key);
  GlobalDeleteAtom (key);
}

#if DEBUG_WINTAB

static void
print_lc(LOGCONTEXT *lc)
{
  g_print ("lcName = %s\n", lc->lcName);
  g_print ("lcOptions =");
  if (lc->lcOptions & CXO_SYSTEM) g_print (" CXO_SYSTEM");
  if (lc->lcOptions & CXO_PEN) g_print (" CXO_PEN");
  if (lc->lcOptions & CXO_MESSAGES) g_print (" CXO_MESSAGES");
  if (lc->lcOptions & CXO_MARGIN) g_print (" CXO_MARGIN");
  if (lc->lcOptions & CXO_MGNINSIDE) g_print (" CXO_MGNINSIDE");
  if (lc->lcOptions & CXO_CSRMESSAGES) g_print (" CXO_CSRMESSAGES");
  g_print ("\n");
  g_print ("lcStatus =");
  if (lc->lcStatus & CXS_DISABLED) g_print (" CXS_DISABLED");
  if (lc->lcStatus & CXS_OBSCURED) g_print (" CXS_OBSCURED");
  if (lc->lcStatus & CXS_ONTOP) g_print (" CXS_ONTOP");
  g_print ("\n");
  g_print ("lcLocks =");
  if (lc->lcLocks & CXL_INSIZE) g_print (" CXL_INSIZE");
  if (lc->lcLocks & CXL_INASPECT) g_print (" CXL_INASPECT");
  if (lc->lcLocks & CXL_SENSITIVITY) g_print (" CXL_SENSITIVITY");
  if (lc->lcLocks & CXL_MARGIN) g_print (" CXL_MARGIN");
  g_print ("\n");
  g_print ("lcMsgBase = %#x, lcDevice = %#x, lcPktRate = %d\n",
	  lc->lcMsgBase, lc->lcDevice, lc->lcPktRate);
  g_print ("lcPktData =");
  if (lc->lcPktData & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcPktData & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcPktData & PK_TIME) g_print (" PK_TIME");
  if (lc->lcPktData & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcPktData & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcPktData & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcPktData & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcPktData & PK_X) g_print (" PK_X");
  if (lc->lcPktData & PK_Y) g_print (" PK_Y");
  if (lc->lcPktData & PK_Z) g_print (" PK_Z");
  if (lc->lcPktData & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcPktData & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcPktData & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcPktData & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcPktMode =");
  if (lc->lcPktMode & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcPktMode & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcPktMode & PK_TIME) g_print (" PK_TIME");
  if (lc->lcPktMode & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcPktMode & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcPktMode & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcPktMode & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcPktMode & PK_X) g_print (" PK_X");
  if (lc->lcPktMode & PK_Y) g_print (" PK_Y");
  if (lc->lcPktMode & PK_Z) g_print (" PK_Z");
  if (lc->lcPktMode & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcPktMode & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcPktMode & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcPktMode & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcMoveMask =");
  if (lc->lcMoveMask & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcMoveMask & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcMoveMask & PK_TIME) g_print (" PK_TIME");
  if (lc->lcMoveMask & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcMoveMask & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcMoveMask & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcMoveMask & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcMoveMask & PK_X) g_print (" PK_X");
  if (lc->lcMoveMask & PK_Y) g_print (" PK_Y");
  if (lc->lcMoveMask & PK_Z) g_print (" PK_Z");
  if (lc->lcMoveMask & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcMoveMask & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcMoveMask & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcMoveMask & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcBtnDnMask = %#x, lcBtnUpMask = %#x\n",
	  (guint) lc->lcBtnDnMask, (guint) lc->lcBtnUpMask);
  g_print ("lcInOrgX = %ld, lcInOrgY = %ld, lcInOrgZ = %ld\n",
	  lc->lcInOrgX, lc->lcInOrgY, lc->lcInOrgZ);
  g_print ("lcInExtX = %ld, lcInExtY = %ld, lcInExtZ = %ld\n",
	  lc->lcInExtX, lc->lcInExtY, lc->lcInExtZ);
  g_print ("lcOutOrgX = %ld, lcOutOrgY = %ld, lcOutOrgZ = %ld\n",
	  lc->lcOutOrgX, lc->lcOutOrgY, lc->lcOutOrgZ);
  g_print ("lcOutExtX = %ld, lcOutExtY = %ld, lcOutExtZ = %ld\n",
	  lc->lcOutExtX, lc->lcOutExtY, lc->lcOutExtZ);
  g_print ("lcSensX = %g, lcSensY = %g, lcSensZ = %g\n",
	  lc->lcSensX / 65536., lc->lcSensY / 65536., lc->lcSensZ / 65536.);
  g_print ("lcSysMode = %d\n", lc->lcSysMode);
  g_print ("lcSysOrgX = %d, lcSysOrgY = %d\n",
	  lc->lcSysOrgX, lc->lcSysOrgY);
  g_print ("lcSysExtX = %d, lcSysExtY = %d\n",
	  lc->lcSysExtX, lc->lcSysExtY);
  g_print ("lcSysSensX = %g, lcSysSensY = %g\n",
	  lc->lcSysSensX / 65536., lc->lcSysSensY / 65536.);
}

static void
print_cursor (int index)
{
  int size;
  int i;
  char *name;
  BOOL active;
  WTPKT wtpkt;
  BYTE buttons;
  BYTE buttonbits;
  char *btnnames;
  char *p;
  BYTE buttonmap[32];
  BYTE sysbtnmap[32];
  BYTE npbutton;
  UINT npbtnmarks[2];
  UINT *npresponse;
  BYTE tpbutton;
  UINT tpbtnmarks[2];
  UINT *tpresponse;
  DWORD physid;
  UINT mode;
  UINT minpktdata;
  UINT minbuttons;
  UINT capabilities;

  size = (*p_WTInfoA) (WTI_CURSORS + index, CSR_NAME, NULL);
  name = g_malloc (size + 1);
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_NAME, name);
  g_print ("NAME: %s\n", name);
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_ACTIVE, &active);
  g_print ("ACTIVE: %s\n", active ? "YES" : "NO");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_PKTDATA, &wtpkt);
  g_print ("PKTDATA: %#x:", (guint) wtpkt);
#define BIT(x) if (wtpkt & PK_##x) g_print (" " #x)
  BIT (CONTEXT);
  BIT (STATUS);
  BIT (TIME);
  BIT (CHANGED);
  BIT (SERIAL_NUMBER);
  BIT (BUTTONS);
  BIT (X);
  BIT (Y);
  BIT (Z);
  BIT (NORMAL_PRESSURE);
  BIT (TANGENT_PRESSURE);
  BIT (ORIENTATION);
  BIT (ROTATION);
#undef BIT
  g_print ("\n");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_BUTTONS, &buttons);
  g_print ("BUTTONS: %d\n", buttons);
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_BUTTONBITS, &buttonbits);
  g_print ("BUTTONBITS: %d\n", buttonbits);
  size = (*p_WTInfoA) (WTI_CURSORS + index, CSR_BTNNAMES, NULL);
  g_print ("BTNNAMES:");
  if (size > 0)
    {
      btnnames = g_malloc (size + 1);
      (*p_WTInfoA) (WTI_CURSORS + index, CSR_BTNNAMES, btnnames);
      p = btnnames;
      while (*p)
        {
          g_print (" %s", p);
          p += strlen (p) + 1;
        }
    }
  g_print ("\n");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_BUTTONMAP, buttonmap);
  g_print ("BUTTONMAP:");
  for (i = 0; i < buttons; i++)
    g_print (" %d", buttonmap[i]);
  g_print ("\n");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_SYSBTNMAP, sysbtnmap);
  g_print ("SYSBTNMAP:");
  for (i = 0; i < buttons; i++)
    g_print (" %d", sysbtnmap[i]);
  g_print ("\n");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_NPBUTTON, &npbutton);
  g_print ("NPBUTTON: %d\n", npbutton);
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_NPBTNMARKS, npbtnmarks);
  g_print ("NPBTNMARKS: %d %d\n", npbtnmarks[0], npbtnmarks[1]);
  size = (*p_WTInfoA) (WTI_CURSORS + index, CSR_NPRESPONSE, NULL);
  g_print ("NPRESPONSE:");
  if (size > 0)
    {
      npresponse = g_malloc (size);
      (*p_WTInfoA) (WTI_CURSORS + index, CSR_NPRESPONSE, npresponse);
      for (i = 0; i < size / sizeof (UINT); i++)
        g_print (" %d", npresponse[i]);
    }
  g_print ("\n");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_TPBUTTON, &tpbutton);
  g_print ("TPBUTTON: %d\n", tpbutton);
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_TPBTNMARKS, tpbtnmarks);
  g_print ("TPBTNMARKS: %d %d\n", tpbtnmarks[0], tpbtnmarks[1]);
  size = (*p_WTInfoA) (WTI_CURSORS + index, CSR_TPRESPONSE, NULL);
  g_print ("TPRESPONSE:");
  if (size > 0)
    {
      tpresponse = g_malloc (size);
      (*p_WTInfoA) (WTI_CURSORS + index, CSR_TPRESPONSE, tpresponse);
      for (i = 0; i < size / sizeof (UINT); i++)
        g_print (" %d", tpresponse[i]);
    }
  g_print ("\n");
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_PHYSID, &physid);
  g_print ("PHYSID: %#x\n", (guint) physid);
  (*p_WTInfoA) (WTI_CURSORS + index, CSR_CAPABILITIES, &capabilities);
  g_print ("CAPABILITIES: %#x:", capabilities);
#define BIT(x) if (capabilities & CRC_##x) g_print (" " #x)
  BIT (MULTIMODE);
  BIT (AGGREGATE);
  BIT (INVERT);
#undef BIT
  g_print ("\n");
  if (capabilities & CRC_MULTIMODE)
    {
      (*p_WTInfoA) (WTI_CURSORS + index, CSR_MODE, &mode);
      g_print ("MODE: %d\n", mode);
    }
  if (capabilities & CRC_AGGREGATE)
    {
      (*p_WTInfoA) (WTI_CURSORS + index, CSR_MINPKTDATA, &minpktdata);
      g_print ("MINPKTDATA: %d\n", minpktdata);
      (*p_WTInfoA) (WTI_CURSORS + index, CSR_MINBUTTONS, &minbuttons);
      g_print ("MINBUTTONS: %d\n", minbuttons);
    }
}
#endif

static void
wintab_init_check (GdkDeviceManagerWin32 *device_manager)
{
  GdkDisplay *display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  GdkWindow *root = gdk_screen_get_root_window (gdk_display_get_default_screen (display));
  static gboolean wintab_initialized = FALSE;
  GdkWindowAttr wa;
  WORD specversion;
  HCTX *hctx;
  UINT ndevices, ncursors;
  UINT devix;
  AXIS axis_x, axis_y;
  int i;
  wchar_t devname[100];
  gchar *devname_utf8;
  BOOL defcontext_done;
  HMODULE wintab32;
  char *wintab32_dll_path;
  char dummy;
  int n, k;

  if (wintab_initialized)
    return;

  wintab_initialized = TRUE;

  wintab_contexts = NULL;

  n = GetSystemDirectory (&dummy, 0);

  if (n <= 0)
    return;

  wintab32_dll_path = g_malloc (n + 1 + strlen (WINTAB32_DLL));
  k = GetSystemDirectory (wintab32_dll_path, n);

  if (k == 0 || k > n)
    {
      g_free (wintab32_dll_path);
      return;
    }

  if (!G_IS_DIR_SEPARATOR (wintab32_dll_path[strlen (wintab32_dll_path) -1]))
    strcat (wintab32_dll_path, G_DIR_SEPARATOR_S);
  strcat (wintab32_dll_path, WINTAB32_DLL);

  if ((wintab32 = LoadLibrary (wintab32_dll_path)) == NULL)
    return;

  if ((p_WTInfoA = (t_WTInfoA) GetProcAddress (wintab32, "WTInfoA")) == NULL)
    return;
  if ((p_WTInfoW = (t_WTInfoW) GetProcAddress (wintab32, "WTInfoW")) == NULL)
    return;
  if ((p_WTEnable = (t_WTEnable) GetProcAddress (wintab32, "WTEnable")) == NULL)
    return;
  if ((p_WTOpenA = (t_WTOpenA) GetProcAddress (wintab32, "WTOpenA")) == NULL)
    return;
  if ((p_WTGetA = (t_WTGetA) GetProcAddress (wintab32, "WTGetA")) == NULL)
    return;
  if ((p_WTSetA = (t_WTSetA) GetProcAddress (wintab32, "WTSetA")) == NULL)
    return;
  if ((p_WTOverlap = (t_WTOverlap) GetProcAddress (wintab32, "WTOverlap")) == NULL)
    return;
  if ((p_WTPacket = (t_WTPacket) GetProcAddress (wintab32, "WTPacket")) == NULL)
    return;
  if ((p_WTQueueSizeSet = (t_WTQueueSizeSet) GetProcAddress (wintab32, "WTQueueSizeSet")) == NULL)
    return;

  if (!(*p_WTInfoA) (0, 0, NULL))
    return;

  (*p_WTInfoA) (WTI_INTERFACE, IFC_SPECVERSION, &specversion);
  GDK_NOTE (INPUT, g_print ("Wintab interface version %d.%d\n",
			    HIBYTE (specversion), LOBYTE (specversion)));
  (*p_WTInfoA) (WTI_INTERFACE, IFC_NDEVICES, &ndevices);
  (*p_WTInfoA) (WTI_INTERFACE, IFC_NCURSORS, &ncursors);
#if DEBUG_WINTAB
  GDK_NOTE (INPUT, g_print ("NDEVICES: %d, NCURSORS: %d\n",
			    ndevices, ncursors));
#endif
  /* Create a dummy window to receive wintab events */
  wa.wclass = GDK_INPUT_OUTPUT;
  wa.event_mask = GDK_ALL_EVENTS_MASK;
  wa.width = 2;
  wa.height = 2;
  wa.x = -100;
  wa.y = -100;
  wa.window_type = GDK_WINDOW_TOPLEVEL;
  if ((wintab_window = gdk_window_new (root, &wa, GDK_WA_X | GDK_WA_Y)) == NULL)
    {
      g_warning ("gdk_input_wintab_init: gdk_window_new failed");
      return;
    }
  g_object_ref (wintab_window);

  for (devix = 0; devix < ndevices; devix++)
    {
      LOGCONTEXT lc;

      /* We open the Wintab device (hmm, what if there are several, or
       * can there even be several, probably not?) as a system
       * pointing device, i.e. it controls the normal Windows
       * cursor. This seems much more natural.
       */

      (*p_WTInfoW) (WTI_DEVICES + devix, DVC_NAME, devname);
      devname_utf8 = g_utf16_to_utf8 (devname, -1, NULL, NULL, NULL);
#ifdef DEBUG_WINTAB
      GDK_NOTE (INPUT, (g_print("Device %u: %s\n", devix, devname_utf8)));
#endif
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_X, &axis_x);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_Y, &axis_y);

      defcontext_done = FALSE;
      if (HIBYTE (specversion) > 1 || LOBYTE (specversion) >= 1)
        {
          /* Try to get device-specific default context */
          /* Some drivers, e.g. Aiptek, don't provide this info */
          if ((*p_WTInfoA) (WTI_DSCTXS + devix, 0, &lc) > 0)
            defcontext_done = TRUE;
#if DEBUG_WINTAB
          if (defcontext_done)
            GDK_NOTE (INPUT, (g_print("Using device-specific default context\n")));
          else
            GDK_NOTE (INPUT, (g_print("Note: Driver did not provide device specific default context info despite claiming to support version 1.1\n")));
#endif
        }

      if (!defcontext_done)
        (*p_WTInfoA) (WTI_DEFSYSCTX, 0, &lc);
#if DEBUG_WINTAB
      GDK_NOTE (INPUT, (g_print("Default context:\n"), print_lc(&lc)));
#endif
      lc.lcOptions |= CXO_MESSAGES | CXO_CSRMESSAGES;
      lc.lcStatus = 0;
      lc.lcMsgBase = WT_DEFBASE;
      lc.lcPktRate = 0;
      lc.lcPktData = PACKETDATA;
      lc.lcPktMode = PACKETMODE;
      lc.lcMoveMask = PACKETDATA;
      lc.lcBtnUpMask = lc.lcBtnDnMask = ~0;
      lc.lcOutOrgX = axis_x.axMin;
      lc.lcOutOrgY = axis_y.axMin;
      lc.lcOutExtX = axis_x.axMax - axis_x.axMin + 1;
      lc.lcOutExtY = axis_y.axMax - axis_y.axMin + 1;
      lc.lcOutExtY = -lc.lcOutExtY; /* We want Y growing downward */
#if DEBUG_WINTAB
      GDK_NOTE (INPUT, (g_print("context for device %u:\n", devix),
			print_lc(&lc)));
#endif
      hctx = g_new (HCTX, 1);
      if ((*hctx = (*p_WTOpenA) (GDK_WINDOW_HWND (wintab_window), &lc, TRUE)) == NULL)
        {
          g_warning ("gdk_input_wintab_init: WTOpen failed");
          return;
        }
      GDK_NOTE (INPUT, g_print ("opened Wintab device %u %p\n",
                                devix, *hctx));

      wintab_contexts = g_list_append (wintab_contexts, hctx);

      /* Set the CXO_SYSTEM flag */
      if (!(lc.lcOptions & CXO_SYSTEM))
        {
          lc.lcOptions |= CXO_SYSTEM;
          if (!p_WTSetA (*hctx, &lc))
            g_warning ("Could not set the CXO_SYSTEM option in the WINTAB context");
        }

      (*p_WTOverlap) (*hctx, TRUE);

#if DEBUG_WINTAB
      GDK_NOTE (INPUT, (g_print("context for device %u after WTOpen:\n", devix),
			print_lc(&lc)));
#endif
      /* Increase packet queue size to reduce the risk of lost packets.
       * According to the specs, if the function fails we must try again
       * with a smaller queue size.
       */
      GDK_NOTE (INPUT, g_print("Attempting to increase queue size\n"));
      for (i = 128; i >= 1; i >>= 1)
        {
          if ((*p_WTQueueSizeSet) (*hctx, i))
            {
              GDK_NOTE (INPUT, g_print("Queue size set to %d\n", i));
              break;
            }
        }
      if (!i)
        GDK_NOTE (INPUT, g_print("Whoops, no queue size could be set\n"));

      /* Get the cursors that Wintab is currently aware of */
      _wintab_recognize_new_cursors(device_manager, *hctx);
    }
}

UINT
_wintab_recognize_new_cursors (GdkDeviceManagerWin32 *device_manager,
                               HCTX                  hctx)
{
  GdkDisplay *display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  int devix;
  wchar_t devname[100], csrname[100];
  gchar *devname_utf8, *csrname_utf8, *device_name;
  UINT ncsrtypes, firstcsr, cursorix;
  BOOL active;
  DWORD physid;
  AXIS axis_x, axis_y, axis_npressure, axis_or[3], axis_tpressure;
  GdkDeviceWintab *device;
  LOGCONTEXT lc;
  int num_axes;
  UINT num_new_cursors = 0;

  devix = _gdk_find_wintab_device_index(hctx);
  if (devix == -1)
    return num_new_cursors;
    
  (*p_WTInfoW) (WTI_DEVICES + devix, DVC_NAME, devname);
  devname_utf8 = g_utf16_to_utf8 (devname, -1, NULL, NULL, NULL);
#ifdef DEBUG_WINTAB
  GDK_NOTE (INPUT, (g_print("Finding cursors for device %u: %s\n", devix, devname_utf8)));
#endif

  (*p_WTInfoA) (WTI_DEVICES + devix, DVC_FIRSTCSR, &firstcsr);
  (*p_WTInfoA) (WTI_DEVICES + devix, DVC_NCSRTYPES, &ncsrtypes);
  for (cursorix = firstcsr; cursorix < firstcsr + ncsrtypes; cursorix++)
    {
#ifdef DEBUG_WINTAB
      GDK_NOTE (INPUT, (g_print("Cursor %u:\n", cursorix), print_cursor (cursorix)));
#endif
      /* Skip cursors that are already known to us */
      if (gdk_device_manager_find_wintab_device(device_manager, hctx, cursorix) != NULL)
        continue;
        
      active = FALSE;
      (*p_WTInfoA) (WTI_CURSORS + cursorix, CSR_ACTIVE, &active);
      if (!active)
        continue;

      /* Wacom tablets iterate through all possible cursors,
       * even if the cursor's presence has not been recognized.
       * Unrecognized cursors have a physid of zero and are ignored. 
       * Recognized cursors have a non-zero physid and we create a 
       * Wintab device object for each of them.
       */
      (*p_WTInfoA) (WTI_CURSORS + cursorix, CSR_PHYSID, &physid);
      if (wcscmp (devname, L"WACOM Tablet") == 0 && physid == 0)
        continue;

      if (!(*p_WTGetA) (hctx, &lc))
        {
          g_warning ("wintab_recognize_new_cursors: Failed to retrieve device LOGCONTEXT");
          continue;
        }

      /* Create a Wintab device for this cursor */
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_X, &axis_x);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_Y, &axis_y);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_NPRESSURE, &axis_npressure);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_ORIENTATION, axis_or);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_TPRESSURE, &axis_tpressure);
      (*p_WTInfoW) (WTI_CURSORS + cursorix, CSR_NAME, csrname);
      csrname_utf8 = g_utf16_to_utf8 (csrname, -1, NULL, NULL, NULL);
      device_name = g_strconcat (devname_utf8, " ", csrname_utf8, NULL);
      g_free (csrname_utf8);

      device = g_object_new (GDK_TYPE_DEVICE_WINTAB,
                              "name", device_name,
                              "type", GDK_DEVICE_TYPE_FLOATING,
                              "input-source", GDK_SOURCE_PEN,
                              "input-mode", GDK_MODE_SCREEN,
                              "has-cursor", lc.lcOptions & CXO_SYSTEM,
                              "display", display,
                              "device-manager", device_manager,
                              NULL);

      device->sends_core = lc.lcOptions & CXO_SYSTEM;
      if (device->sends_core)
        {
          _gdk_device_set_associated_device (device_manager->system_pointer, GDK_DEVICE (device));
          _gdk_device_add_slave (device_manager->core_pointer, GDK_DEVICE (device));
        }

      device->hctx = hctx;
      device->cursor = cursorix;
      (*p_WTInfoA) (WTI_CURSORS + cursorix, CSR_PKTDATA, &device->pktdata);
      num_axes = 0;

      if (device->pktdata & PK_X)
        {
          _gdk_device_add_axis (GDK_DEVICE (device),
                                GDK_NONE,
                                GDK_AXIS_X,
                                axis_x.axMin,
                                axis_x.axMax,
                                axis_x.axResolution / 65535);
          num_axes++;
        }

      if (device->pktdata & PK_Y)
        {
          _gdk_device_add_axis (GDK_DEVICE (device),
                                GDK_NONE,
                                GDK_AXIS_Y,
                                axis_y.axMin,
                                axis_y.axMax,
                                axis_y.axResolution / 65535);
          num_axes++;
        }

      if (device->pktdata & PK_NORMAL_PRESSURE)
        {
          _gdk_device_add_axis (GDK_DEVICE (device),
                                GDK_NONE,
                                GDK_AXIS_PRESSURE,
                                axis_npressure.axMin,
                                axis_npressure.axMax,
                                axis_npressure.axResolution / 65535);
          num_axes++;
        }

      if (device->pktdata & PK_ORIENTATION)
        {
          if (device->pktdata & PK_TANGENT_PRESSURE) /* If we have a wheel, disable the twist axis */
            axis_or[2].axResolution = 0;

          device->orientation_axes[0] = axis_or[0];
          device->orientation_axes[1] = axis_or[1];
          device->orientation_axes[2] = axis_or[2];

          /* Wintab gives us azimuth and altitude, which
           * we convert to x and y tilt in the -1000..1000 range
           */
          _gdk_device_add_axis (GDK_DEVICE (device),
                                GDK_NONE,
                                GDK_AXIS_XTILT,
                                -1000,
                                1000,
                                1000);

          _gdk_device_add_axis (GDK_DEVICE (device),
                                GDK_NONE,
                                GDK_AXIS_YTILT,
                                -1000,
                                1000,
                                1000);
          num_axes += 2;

          if (axis_or[2].axResolution != 0) /* If twist is present */
            {
              /* Wacom's Wintab driver returns the rotation
               * of an Art Pen as the orientation twist value.
               * We're using GDK_AXIS_WHEEL as it's actually
               * called Wheel/Rotation to the user.
               * axMin and axMax are back to front on purpose. If you put them
               * the "correct" way round, rotation will be flipped!
               */
               _gdk_device_add_axis (GDK_DEVICE (device),
                                     GDK_NONE,
                                     GDK_AXIS_WHEEL,
                                     axis_or[2].axMax,
                                     axis_or[2].axMin,
                                     axis_or[2].axResolution / 65535);
              num_axes++;
            }
        }

      if (device->pktdata & PK_TANGENT_PRESSURE)
        {
          /* This is the finger wheel on a Wacom Airbrush
           */
          _gdk_device_add_axis (GDK_DEVICE (device),
                                GDK_NONE,
                                GDK_AXIS_WHEEL,
                                axis_tpressure.axMin,
                                axis_tpressure.axMax,
                                axis_tpressure.axResolution / 65535);
          num_axes++;
        }

      device->last_axis_data = g_new (gint, num_axes);

      GDK_NOTE (INPUT, g_print ("device: (%u) %s axes: %d\n",
                                cursorix,
                                device_name,
                                num_axes));

#if 0
      for (i = 0; i < gdkdev->info.num_axes; i++)
        GDK_NOTE (INPUT, g_print ("... axis %d: %d--%d@%d\n",
                                  i,
                                  gdkdev->axes[i].min_value,
                                  gdkdev->axes[i].max_value,
                                  gdkdev->axes[i].resolution));
#endif

      device_manager->wintab_devices = g_list_append (device_manager->wintab_devices,
                                                      device);
      num_new_cursors++;
      g_free (device_name);
    }
  g_free (devname_utf8);
  return num_new_cursors;
}

/* Only initialize Wintab after the default display is set for
 * the first time. WTOpenA() executes code beyond our control,
 * and it can cause messages to be sent to the application even
 * before a window is opened. GDK has to be in a fit state to
 * handle them when they come.
 *
 * https://bugzilla.gnome.org/show_bug.cgi?id=774379
 */
static void
wintab_default_display_notify_cb (GdkDisplayManager *display_manager)
{
  GdkDeviceManagerWin32 *device_manager = NULL;
  GdkDisplay *display = gdk_display_get_default();

  if (default_display_opened)
    return;

  g_assert (display != NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  device_manager = GDK_DEVICE_MANAGER_WIN32 (gdk_display_get_device_manager (display));
G_GNUC_END_IGNORE_DEPRECATIONS;
  g_assert (display_manager != NULL);

  default_display_opened = TRUE;
  GDK_NOTE (INPUT, g_print ("wintab init: doing delayed initialization\n"));
  wintab_init_check (device_manager);
}

static void
gdk_device_manager_win32_constructed (GObject *object)
{
  GdkDeviceManagerWin32 *device_manager;
  GdkSeat *seat;
  GdkDisplayManager *display_manager = NULL;
  GdkDisplay *default_display = NULL;
  const char *tablet_input_api_user_preference = NULL;
  gboolean have_tablet_input_api_preference = FALSE;

  device_manager = GDK_DEVICE_MANAGER_WIN32 (object);
  device_manager->core_pointer =
    create_pointer (GDK_DEVICE_MANAGER (device_manager),
		    GDK_TYPE_DEVICE_VIRTUAL,
		    "Virtual Core Pointer",
		    GDK_DEVICE_TYPE_MASTER);
  device_manager->system_pointer =
    create_pointer (GDK_DEVICE_MANAGER (device_manager),
		    GDK_TYPE_DEVICE_WIN32,
		    "System Aggregated Pointer",
		    GDK_DEVICE_TYPE_SLAVE);
  _gdk_device_virtual_set_active (device_manager->core_pointer,
				  device_manager->system_pointer);
  _gdk_device_set_associated_device (device_manager->system_pointer, device_manager->core_pointer);
  _gdk_device_add_slave (device_manager->core_pointer, device_manager->system_pointer);

  device_manager->core_keyboard =
    create_keyboard (GDK_DEVICE_MANAGER (device_manager),
		     GDK_TYPE_DEVICE_VIRTUAL,
		     "Virtual Core Keyboard",
		     GDK_DEVICE_TYPE_MASTER);
  device_manager->system_keyboard =
    create_keyboard (GDK_DEVICE_MANAGER (device_manager),
		    GDK_TYPE_DEVICE_WIN32,
		     "System Aggregated Keyboard",
		     GDK_DEVICE_TYPE_SLAVE);
  _gdk_device_virtual_set_active (device_manager->core_keyboard,
				  device_manager->system_keyboard);
  _gdk_device_set_associated_device (device_manager->system_keyboard, device_manager->core_keyboard);
  _gdk_device_add_slave (device_manager->core_keyboard, device_manager->system_keyboard);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);

  seat = gdk_seat_default_new_for_master_pair (device_manager->core_pointer,
                                               device_manager->core_keyboard);
  gdk_display_add_seat (gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object)), seat);
  gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), device_manager->system_pointer);
  gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), device_manager->system_keyboard);
  g_object_unref (seat);

  tablet_input_api_user_preference = g_getenv ("GDK_WIN32_TABLET_INPUT_API");
  if (g_strcmp0 (tablet_input_api_user_preference, "none") == 0)
    {
      have_tablet_input_api_preference = TRUE;
      _gdk_win32_tablet_input_api = GDK_WIN32_TABLET_INPUT_API_NONE;
    }
  else if (g_strcmp0 (tablet_input_api_user_preference, "wintab") == 0)
    {
      have_tablet_input_api_preference = TRUE;
      _gdk_win32_tablet_input_api = GDK_WIN32_TABLET_INPUT_API_WINTAB;
    }
  else if (g_strcmp0 (tablet_input_api_user_preference, "winpointer") == 0)
    {
      have_tablet_input_api_preference = TRUE;
      _gdk_win32_tablet_input_api = GDK_WIN32_TABLET_INPUT_API_WINPOINTER;
    }
  else
    {
      have_tablet_input_api_preference = FALSE;
      _gdk_win32_tablet_input_api = GDK_WIN32_TABLET_INPUT_API_WINPOINTER;
    }

  if (_gdk_win32_tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINPOINTER)
    {
      if (!winpointer_initialize (device_manager) &&
          !have_tablet_input_api_preference)
        _gdk_win32_tablet_input_api = GDK_WIN32_TABLET_INPUT_API_WINTAB;
    }

  if (_gdk_win32_tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINTAB)
    {
      /* Only call Wintab init stuff after the default display
       * is globally known and accessible through the display manager
       * singleton. Approach lifted from gtkmodules.c.
       */
      display_manager = gdk_display_manager_get();
      g_assert (display_manager != NULL);
      default_display = gdk_display_manager_get_default_display (display_manager);
      g_assert (default_display == NULL);

      g_signal_connect (display_manager, "notify::default-display",
                        G_CALLBACK (wintab_default_display_notify_cb),
                        NULL);
    }
}

static GList *
gdk_device_manager_win32_list_devices (GdkDeviceManager *device_manager,
                                       GdkDeviceType     type)
{
  GdkDeviceManagerWin32 *device_manager_win32;
  GList *devices = NULL, *l;

  device_manager_win32 = (GdkDeviceManagerWin32 *) device_manager;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      devices = g_list_prepend (devices, device_manager_win32->core_keyboard);
      devices = g_list_prepend (devices, device_manager_win32->core_pointer);
    }
  else
    {
      if (type == GDK_DEVICE_TYPE_SLAVE)
	{
	  devices = g_list_prepend (devices, device_manager_win32->system_keyboard);
	  devices = g_list_prepend (devices, device_manager_win32->system_pointer);
	}

      for (l = device_manager_win32->winpointer_devices; l != NULL; l = l->next)
        {
          GdkDevice *device = l->data;

          if (gdk_device_get_device_type (device) == type)
            devices = g_list_prepend (devices, device);
        }

      for (l = device_manager_win32->wintab_devices; l != NULL; l = l->next)
	{
	  GdkDevice *device = l->data;

	  if (gdk_device_get_device_type (device) == type)
	    devices = g_list_prepend (devices, device);
	}
    }

  return g_list_reverse (devices);
}

static GdkDevice *
gdk_device_manager_win32_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkDeviceManagerWin32 *device_manager_win32;

  device_manager_win32 = (GdkDeviceManagerWin32 *) device_manager;
  return device_manager_win32->core_pointer;
}

static void
gdk_device_manager_win32_class_init (GdkDeviceManagerWin32Class *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_manager_win32_finalize;
  object_class->constructed = gdk_device_manager_win32_constructed;
  device_manager_class->list_devices = gdk_device_manager_win32_list_devices;
  device_manager_class->get_client_pointer = gdk_device_manager_win32_get_client_pointer;
}

static inline void
winpointer_ignore_interaction (UINT32 pointer_id)
{
  g_ptr_array_add (winpointer_ignored_interactions, GUINT_TO_POINTER (pointer_id));
}

static inline void
winpointer_remove_ignored_interaction (UINT32 pointer_id)
{
  g_ptr_array_remove_fast (winpointer_ignored_interactions, GUINT_TO_POINTER (pointer_id));
}

static inline gboolean
winpointer_should_ignore_interaction (UINT32 pointer_id)
{
  return g_ptr_array_find (winpointer_ignored_interactions, GUINT_TO_POINTER (pointer_id), NULL);
}

static inline guint32
winpointer_get_time (MSG *msg, POINTER_INFO *info)
{
  return info->dwTime != 0 ? info->dwTime : msg->time;
}

static inline gboolean
winpointer_is_eraser (POINTER_PEN_INFO *pen_info)
{
  return (pen_info->penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) != 0;
}

static inline gboolean
winpointer_should_filter_message (MSG *msg,
                                  POINTER_INPUT_TYPE type)
{
  switch (type)
    {
    case PT_TOUCH:
      return msg->message == WM_POINTERENTER ||
             msg->message == WM_POINTERLEAVE;
    break;
    }

  return FALSE;
}

static GdkDeviceWinpointer*
winpointer_find_device_with_source (GdkDeviceManagerWin32 *device_manager,
                                    HANDLE device_handle,
                                    UINT32 cursor_id,
                                    GdkInputSource input_source)
{
  GList *l;

  for (l = device_manager->winpointer_devices; l; l = l->next)
    {
      GdkDeviceWinpointer *device = (GdkDeviceWinpointer*) l->data;

      if (device->device_handle == device_handle &&
          device->start_cursor_id <= cursor_id &&
          device->end_cursor_id >= cursor_id &&
          gdk_device_get_source (GDK_DEVICE (device)) == input_source)
        return device;
    }

  return NULL;
}

static GdkEvent*
winpointer_allocate_event (MSG *msg,
                           POINTER_INFO *info)
{
  switch (info->pointerType)
    {
    case PT_PEN:
      switch (msg->message)
        {
        case WM_POINTERENTER:
          g_return_val_if_fail (IS_POINTER_NEW_WPARAM (msg->wParam), NULL);
          return gdk_event_new (GDK_PROXIMITY_IN);
        break;
        case WM_POINTERLEAVE:
          g_return_val_if_fail (!IS_POINTER_INRANGE_WPARAM (msg->wParam), NULL);
          return gdk_event_new (GDK_PROXIMITY_OUT);
        break;
        case WM_POINTERDOWN:
          return gdk_event_new (GDK_BUTTON_PRESS);
        break;
        case WM_POINTERUP:
          return gdk_event_new (GDK_BUTTON_RELEASE);
        break;
        case WM_POINTERUPDATE:
          return gdk_event_new (GDK_MOTION_NOTIFY);
        break;
        }
    break;
    case PT_TOUCH:
      if (IS_POINTER_CANCELED_WPARAM (msg->wParam) ||
          !HAS_POINTER_CONFIDENCE_WPARAM (msg->wParam))
        {
          winpointer_ignore_interaction (GET_POINTERID_WPARAM (msg->wParam));

          if (((info->pointerFlags & POINTER_FLAG_INCONTACT) &&
               (info->pointerFlags & POINTER_FLAG_UPDATE))
              || (info->pointerFlags & POINTER_FLAG_UP))
            return gdk_event_new (GDK_TOUCH_CANCEL);
          else
            return NULL;
        }

      g_return_val_if_fail (msg->message != WM_POINTERENTER &&
                            msg->message != WM_POINTERLEAVE, NULL);

      switch (msg->message)
        {
          case WM_POINTERDOWN:
            return gdk_event_new (GDK_TOUCH_BEGIN);
          break;
          case WM_POINTERUP:
            return gdk_event_new (GDK_TOUCH_END);
          break;
          case WM_POINTERUPDATE:
            if (IS_POINTER_INCONTACT_WPARAM (msg->wParam))
              return gdk_event_new (GDK_TOUCH_UPDATE);
            else if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
              return gdk_event_new (GDK_MOTION_NOTIFY);
            else
              return NULL;
          break;
        }
    break;
    }

  g_warn_if_reached ();
  return NULL;
}

static void
winpointer_make_event (GdkDisplay *display,
                       GdkDeviceManagerWin32 *device_manager,
                       GdkDeviceWinpointer *device,
                       GdkWindow *window,
                       MSG *msg,
                       POINTER_INFO *info)
{
  guint32 time = 0;
  double x_root = 0.0;
  double y_root = 0.0;
  double x = 0.0;
  double y = 0.0;
  unsigned int state = 0;
  double *axes = NULL;
  unsigned int button = 0;
  GdkEventSequence *sequence = NULL;
  gboolean emulating_pointer = FALSE;
  POINT client_area_coordinates;
  GdkWindowImplWin32 *impl = NULL;
  GdkEvent *evt = NULL;

  evt = winpointer_allocate_event (msg, info);
  if (!evt)
    return;

  time = winpointer_get_time (msg, info);

  x_root = device->origin_x + info->ptHimetricLocation.x * device->scale_x;
  y_root = device->origin_y + info->ptHimetricLocation.y * device->scale_y;

  client_area_coordinates.x = 0;
  client_area_coordinates.y = 0;
  ClientToScreen (GDK_WINDOW_HWND (window), &client_area_coordinates);
  x = x_root - client_area_coordinates.x;
  y = y_root - client_area_coordinates.y;

  /* bring potential win32 negative screen coordinates to
     the non-negative screen coordinates that GDK expects. */
  x_root += _gdk_offset_x;
  y_root += _gdk_offset_y;

  /* handle DPI scaling */
  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  x_root /= impl->window_scale;
  y_root /= impl->window_scale;
  x /= impl->window_scale;
  y /= impl->window_scale;

  /* info->dwKeyStates is not reliable. We shall use
   * GetKeyState here even for Ctrl and Shift. */
  state = 0;
  if (GetKeyState (VK_CONTROL) < 0)
    state |= GDK_CONTROL_MASK;
  if (GetKeyState (VK_SHIFT) < 0)
    state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_MOD1_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  device->last_button_mask = 0;
  if (((info->pointerFlags & POINTER_FLAG_FIRSTBUTTON) &&
       (info->ButtonChangeType != POINTER_CHANGE_FIRSTBUTTON_DOWN))
      || info->ButtonChangeType == POINTER_CHANGE_FIRSTBUTTON_UP)
    device->last_button_mask |= GDK_BUTTON1_MASK;
  if (((info->pointerFlags & POINTER_FLAG_SECONDBUTTON) &&
       (info->ButtonChangeType != POINTER_CHANGE_SECONDBUTTON_DOWN))
      || info->ButtonChangeType == POINTER_CHANGE_SECONDBUTTON_UP)
    device->last_button_mask |= GDK_BUTTON3_MASK;
  state |= device->last_button_mask;

  switch (info->pointerType)
    {
    case PT_PEN:
      {
        POINTER_PEN_INFO *pen_info = (POINTER_PEN_INFO*) info;

        axes = g_new (double, device->num_axes);
        axes[0] = (pen_info->penMask & PEN_MASK_PRESSURE) ? pen_info->pressure / 1024.0 :
                  (pen_info->pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) ? 1.0 : 0.0;
        axes[1] = (pen_info->penMask & PEN_MASK_TILT_X) ? pen_info->tiltX / 90.0 : 0.0;
        axes[2] = (pen_info->penMask & PEN_MASK_TILT_Y) ? pen_info->tiltY / 90.0 : 0.0;
        axes[3] = (pen_info->penMask & PEN_MASK_ROTATION) ? pen_info->rotation / 360.0 : 0.0;
      }
    break;
    case PT_TOUCH:
      {
        POINTER_TOUCH_INFO *touch_info = (POINTER_TOUCH_INFO*) info;

        axes = g_new (double, device->num_axes);
        axes[0] = (touch_info->touchMask & TOUCH_MASK_PRESSURE) ? touch_info->pressure / 1024.0 :
                  (touch_info->pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) ? 1.0 : 0.0;
      }
    break;
    }

  if (axes)
    {
      memcpy (device->last_axis_data, axes, sizeof (double) * device->num_axes);
    }

  sequence = (GdkEventSequence*) GUINT_TO_POINTER (info->pointerId);
  emulating_pointer = (info->pointerFlags & POINTER_FLAG_PRIMARY) != 0;
  button = (info->pointerFlags & POINTER_FLAG_FIRSTBUTTON) ||
           (info->ButtonChangeType == POINTER_CHANGE_FIRSTBUTTON_UP) ? 1 : 3;

  switch (evt->any.type)
    {
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      evt->proximity.time = time;
    break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      evt->button.time = time;
      evt->button.x_root = x_root;
      evt->button.y_root = y_root;
      evt->button.x = x;
      evt->button.y = y;
      evt->button.state = state;
      evt->button.axes = axes;
      evt->button.button = button;
    break;
    case GDK_MOTION_NOTIFY:
      evt->motion.time = time;
      evt->motion.x_root = x_root;
      evt->motion.y_root = y_root;
      evt->motion.x = x;
      evt->motion.y = y;
      evt->motion.state = state;
      evt->motion.axes = axes;
    break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCH_END:
      evt->touch.time = time;
      evt->touch.x_root = x_root;
      evt->touch.y_root = y_root;
      evt->touch.x = x;
      evt->touch.y = y;
      evt->touch.state = state;
      evt->touch.axes = axes;
      evt->touch.sequence = sequence;
      evt->touch.emulating_pointer = emulating_pointer;
      gdk_event_set_pointer_emulated (evt, emulating_pointer);
    break;
    }

  evt->any.send_event = FALSE;
  evt->any.window = window;
  gdk_event_set_device (evt, device_manager->core_pointer);
  gdk_event_set_source_device (evt, GDK_DEVICE (device));
  gdk_event_set_device_tool (evt, ((GdkDevice*)device)->last_tool);
  gdk_event_set_seat (evt, gdk_device_get_seat (device_manager->core_pointer));
  gdk_event_set_screen (evt, gdk_display_get_default_screen (display));

  _gdk_device_virtual_set_active (device_manager->core_pointer, GDK_DEVICE (device));

  _gdk_win32_append_event (evt);
}

void
gdk_winpointer_input_events (GdkDisplay *display,
                             GdkWindow *window,
                             crossing_cb_t crossing_cb,
                             MSG *msg)
{
  GdkDeviceManagerWin32 *device_manager = NULL;
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INPUT_TYPE type = PT_POINTER;
  UINT32 cursor_id = 0;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  device_manager = GDK_DEVICE_MANAGER_WIN32 (gdk_display_get_device_manager (display));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return;
    }

  if (!getPointerCursorId (pointer_id, &cursor_id))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerCursorId");
      return;
    }

  if (winpointer_should_filter_message (msg, type))
    {
      return;
    }

  if (winpointer_should_ignore_interaction (pointer_id))
    {
      return;
    }

  switch (type)
    {
    case PT_PEN:
      {
        POINTER_PEN_INFO *infos = NULL;
        UINT32 history_count = 0;
        GdkDeviceWinpointer *device = NULL;
        UINT32 h = 0;

        do
          {
            infos = g_new0 (POINTER_PEN_INFO, history_count);
            if (!getPointerPenInfoHistory (pointer_id, &history_count, infos))
              {
                WIN32_API_FAILED_LOG_ONCE ("GetPointerPenInfoHistory");
                g_free (infos);
                return;
              }
          }
        while (!infos && history_count > 0);

        if (history_count == 0)
          return;

        device = winpointer_find_device_with_source (device_manager,
                                                     infos->pointerInfo.sourceDevice,
                                                     cursor_id,
                                                     winpointer_is_eraser (infos) ?
                                                     GDK_SOURCE_ERASER : GDK_SOURCE_PEN);
        if (!device)
          {
            g_free (infos);
            return;
          }

        h = history_count - 1;

        if (crossing_cb)
          {
            POINT screen_pt = infos[h].pointerInfo.ptPixelLocation;
            guint32 event_time = winpointer_get_time (msg, &infos[h].pointerInfo);

            crossing_cb(display, GDK_DEVICE (device), window, &screen_pt, event_time);
          }

        do
          winpointer_make_event (display,
                                 device_manager,
                                 device,
                                 window,
                                 msg,
                                 (POINTER_INFO*) &infos[h]);
        while (h-- > 0);

        g_free (infos);
      }
    break;
    case PT_TOUCH:
      {
        POINTER_TOUCH_INFO *infos = NULL;
        UINT32 history_count = 0;
        GdkDeviceWinpointer *device = NULL;
        UINT32 h = 0;

        do
          {
            infos = g_new0 (POINTER_TOUCH_INFO, history_count);
            if (!getPointerTouchInfoHistory (pointer_id, &history_count, infos))
              {
                WIN32_API_FAILED_LOG_ONCE ("GetPointerTouchInfoHistory");
                g_free (infos);
                return;
              }
          }
        while (!infos && history_count > 0);

        if (history_count == 0)
          return;

        device = winpointer_find_device_with_source (device_manager,
                                                     infos->pointerInfo.sourceDevice,
                                                     cursor_id,
                                                     GDK_SOURCE_TOUCHSCREEN);
        if (!device)
          {
            g_free (infos);
            return;
          }

        h = history_count - 1;

        if (crossing_cb)
          {
            POINT screen_pt = infos[h].pointerInfo.ptPixelLocation;
            guint32 event_time = winpointer_get_time (msg, &infos[h].pointerInfo);

            crossing_cb(display, GDK_DEVICE (device), window, &screen_pt, event_time);
          }

        do
          winpointer_make_event (display,
                                 device_manager,
                                 device,
                                 window,
                                 msg,
                                 (POINTER_INFO*) &infos[h]);
        while (h-- > 0);

        g_free (infos);
      }
    break;
    }
}

gboolean
gdk_winpointer_get_message_info (GdkDisplay *display,
                                 MSG *msg,
                                 GdkDevice **device,
                                 guint32 *time_)
{
  GdkDeviceManagerWin32 *device_manager = NULL;
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INPUT_TYPE type = PT_POINTER;
  UINT32 cursor_id = 0;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  device_manager = GDK_DEVICE_MANAGER_WIN32 (gdk_display_get_device_manager (display));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return FALSE;
    }

  if (!getPointerCursorId (pointer_id, &cursor_id))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerCursorId");
      return FALSE;
    }

  switch (type)
    {
    case PT_PEN:
      {
        POINTER_PEN_INFO pen_info;

        if (!getPointerPenInfo (pointer_id, &pen_info))
          {
            WIN32_API_FAILED_LOG_ONCE ("GetPointerPenInfo");
            return FALSE;
          }

        *device = GDK_DEVICE (winpointer_find_device_with_source (device_manager,
                                                                  pen_info.pointerInfo.sourceDevice,
                                                                  cursor_id,
                                                                  winpointer_is_eraser (&pen_info) ?
                                                                  GDK_SOURCE_ERASER : GDK_SOURCE_PEN));

        *time_ = winpointer_get_time (msg, &pen_info.pointerInfo);
      }
    break;
    case PT_TOUCH:
      {
        POINTER_TOUCH_INFO touch_info;

        if (!getPointerTouchInfo (pointer_id, &touch_info))
            {
              WIN32_API_FAILED_LOG_ONCE ("GetPointerTouchInfo");
              return FALSE;
            }

        *device = GDK_DEVICE (winpointer_find_device_with_source (device_manager,
                                                                  touch_info.pointerInfo.sourceDevice,
                                                                  cursor_id,
                                                                  GDK_SOURCE_TOUCHSCREEN));

        *time_ = winpointer_get_time (msg, &touch_info.pointerInfo);
      }
    break;
    default:
      g_warn_if_reached ();
      return FALSE;
    break;
    }

  return *device ? TRUE : FALSE;
}

gboolean
gdk_winpointer_should_forward_message (MSG *msg)
{
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INPUT_TYPE type = PT_POINTER;

  if (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return TRUE;
    }

  return !(type == PT_PEN || type == PT_TOUCH);
}

void
gdk_winpointer_interaction_ended (MSG *msg)
{
  winpointer_remove_ignored_interaction (GET_POINTERID_WPARAM (msg->wParam));
}

void
_gdk_wintab_set_tablet_active (void)
{
  GList *tmp_list;
  HCTX *hctx;

  /* Bring the contexts to the top of the overlap order when one of the
   * application's windows is activated */

  if (!wintab_contexts)
    return; /* No tablet devices found, or Wintab not initialized yet */

  GDK_NOTE (INPUT, g_print ("_gdk_wintab_set_tablet_active: "
                            "Bringing Wintab contexts to the top of the overlap order\n"));

  tmp_list = wintab_contexts;

  while (tmp_list)
    {
      hctx = (HCTX *) (tmp_list->data);
      (*p_WTOverlap) (*hctx, TRUE);
      tmp_list = tmp_list->next;
    }
}

static void
decode_tilt (gint   *axis_data,
             AXIS   *axes,
             PACKET *packet)
{
  double az, el;

  g_return_if_fail (axis_data != NULL);

  /* The wintab driver for the Wacom ArtPad II reports
   * PK_ORIENTATION in CSR_PKTDATA, but the tablet doesn't
   * actually sense tilt. Catch this by noticing that the
   * orientation axis's azimuth resolution is zero.
   *
   * The same is true of the Huion H610PRO, but in this case
   * it's the altitude resolution that's zero. GdkEvents with
   * sensible tilts will need both, so only add the GDK tilt axes
   * if both wintab axes are going to be well-behaved in use.
   */
  if (axes == NULL)
    return;

  if ((axes[0].axResolution == 0) ||
      (axes[1].axResolution == 0))
    {
      axis_data[0] = 0;
      axis_data[1] = 0;
    }
  else
    {
      /*
       * Tested with a Wacom Intuos 5 touch M (PTH-650) + Wacom drivers 6.3.18-5.
       * Wintab's reference angle leads gdk's by 90 degrees.
       */
      az = TWOPI * packet->pkOrientation.orAzimuth /
        (axes[0].axResolution / 65536.);
      az -= G_PI / 2;
      el = TWOPI * packet->pkOrientation.orAltitude /
        (axes[1].axResolution / 65536.);

      /* X tilt */
      axis_data[0] = cos (az) * cos (el) * 1000;
      /* Y tilt */
      axis_data[1] = sin (az) * cos (el) * 1000;
    }

  /* Twist (Rotation) if present */
  if (axes[2].axResolution != 0)
    axis_data[2] = packet->pkOrientation.orTwist;
}

/*
 * Get the currently active keyboard modifiers (ignoring the mouse buttons)
 * We could use gdk_window_get_pointer but that function does a lot of other
 * expensive things besides getting the modifiers. This code is somewhat based
 * on build_pointer_event_state from gdkevents-win32.c
 */
static guint
get_modifier_key_state (void)
{
  guint state;

  state = 0;
  /* High-order bit is up/down, low order bit is toggled/untoggled */
  if (GetKeyState (VK_CONTROL) < 0)
    state |= GDK_CONTROL_MASK;
  if (GetKeyState (VK_SHIFT) < 0)
    state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_MOD1_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  return state;
}

int
_gdk_find_wintab_device_index (HCTX hctx)
{
  GList *tmp_list;
  int devix;

  /* Find the index of the Wintab driver's input device (probably zero) */
  if (!wintab_contexts)
    return -1; /* No tablet devices found or Wintab not initialized yet */
  
  tmp_list = wintab_contexts;
  devix = 0;
  while (tmp_list)
    {
      if ((*(HCTX *) (tmp_list->data)) == hctx)
        return devix;
      else
        {
          devix++;
          tmp_list = tmp_list->next;
        }
    }
  return -1;
}

static GdkDeviceWintab *
gdk_device_manager_find_wintab_device (GdkDeviceManagerWin32 *device_manager,
                                       HCTX                   hctx,
                                       UINT                   cursor)
{
  GdkDeviceWintab *device;
  GList *tmp_list;

  for (tmp_list = device_manager->wintab_devices; tmp_list != NULL; tmp_list = tmp_list->next)
    {
      device = tmp_list->data;

      if (device->hctx == hctx &&
          device->cursor == cursor)
        return device;
    }

  return NULL;
}

gboolean
gdk_wintab_input_events (GdkDisplay *display,
                         GdkEvent   *event,
                         MSG        *msg,
                         GdkWindow  *window)
{
  GdkDeviceManagerWin32 *device_manager;
  GdkDeviceWintab *source_device = NULL;
  GdkDeviceGrabInfo *last_grab;
  GdkEventMask masktest;
  guint key_state;
  POINT pt;
  GdkWindowImplWin32 *impl;

  PACKET packet;
  gint root_x, root_y;
  gint num_axes;
  gint x, y;
  guint translated_buttons, button_diff, button_mask;
  /* Translation from tablet button state to GDK button state for
   * buttons 1-3 - swap button 2 and 3.
   */
  static guint button_map[8] = {0, 1, 4, 5, 2, 3, 6, 7};

  if (event->any.window != wintab_window)
    {
      g_warning ("gdk_wintab_input_events: not wintab_window?");
      return FALSE;
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  device_manager = GDK_DEVICE_MANAGER_WIN32 (gdk_display_get_device_manager (display));
G_GNUC_END_IGNORE_DEPRECATIONS;
  window = gdk_device_get_window_at_position (device_manager->core_pointer, &x, &y);
  if (window == NULL)
    window = gdk_get_default_root_window ();

  g_object_ref (window);

  GDK_NOTE (EVENTS_OR_INPUT,
	    g_print ("gdk_wintab_input_events: window=%p %+d%+d\n",
               GDK_WINDOW_HWND (window), x, y));

  if (msg->message == WT_PACKET || msg->message == WT_CSRCHANGE)
    {
      if (!(*p_WTPacket) ((HCTX) msg->lParam, msg->wParam, &packet))
        return FALSE;
    }

  switch (msg->message)
    {
    case WT_PACKET:
      source_device = gdk_device_manager_find_wintab_device (device_manager,
							     (HCTX) msg->lParam,
							     packet.pkCursor);

      /* Check this first, as we get WT_PROXIMITY for disabled devices too */
      if (device_manager->dev_entered_proximity > 0)
	{
	  /* This is the same code as in WT_CSRCHANGE. Some drivers send
	   * WT_CSRCHANGE after each WT_PROXIMITY with LOWORD(lParam) != 0,
	   * this code is for those that don't.
	   */
	  device_manager->dev_entered_proximity -= 1;

	  if (source_device != NULL &&
	      source_device->sends_core &&
	      gdk_device_get_mode (GDK_DEVICE (source_device)) != GDK_MODE_DISABLED)
	    {
	      _gdk_device_virtual_set_active (device_manager->core_pointer,
					      GDK_DEVICE (source_device));
	      _gdk_input_ignore_core += 1;
	    }
	}
      else if (source_device != NULL &&
	       source_device->sends_core &&
	       gdk_device_get_mode (GDK_DEVICE (source_device)) != GDK_MODE_DISABLED &&
               _gdk_input_ignore_core == 0)
        {
          /* A fallback for cases when two devices (disabled and enabled)
           * were in proximity simultaneously.
           * In this case the removal of a disabled device would also
           * make the system pointer active, as we don't know which
           * device was removed and assume it was the enabled one.
           * If we are still getting packets for the enabled device,
           * it means that the device that was removed was the disabled
           * device, so we must make the enabled device active again and
           * start ignoring the core pointer events. In practice this means that
           * removing a disabled device while an enabled device is still
           * in proximity might briefly make the core pointer active/visible.
           */
	  _gdk_device_virtual_set_active (device_manager->core_pointer,
					  GDK_DEVICE (source_device));
	  _gdk_input_ignore_core += 1;
        }

      if (source_device == NULL ||
	  gdk_device_get_mode (GDK_DEVICE (source_device)) == GDK_MODE_DISABLED)
	return FALSE;

      /* Don't produce any button or motion events while a window is being
       * moved or resized, see bug #151090.
       */
      if (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
        {
          GDK_NOTE (EVENTS_OR_INPUT, g_print ("... ignored when moving/sizing\n"));
          return FALSE;
        }

      last_grab = _gdk_display_get_last_device_grab (display, GDK_DEVICE (source_device));

      if (last_grab && last_grab->window)
        {
          g_object_unref (window);

          window = g_object_ref (last_grab->window);
        }

      if (window == gdk_get_default_root_window ())
        {
          GDK_NOTE (EVENTS_OR_INPUT, g_print ("... is root\n"));
          return FALSE;
        }

      num_axes = 0;
      if (source_device->pktdata & PK_X)
        source_device->last_axis_data[num_axes++] = packet.pkX;
      if (source_device->pktdata & PK_Y)
        source_device->last_axis_data[num_axes++] = packet.pkY;
      if (source_device->pktdata & PK_NORMAL_PRESSURE)
        source_device->last_axis_data[num_axes++] = packet.pkNormalPressure;
      if (source_device->pktdata & PK_ORIENTATION)
        {
          decode_tilt (source_device->last_axis_data + num_axes,
                       source_device->orientation_axes, &packet);
          /* we could have 3 axes if twist is present */
          if (source_device->orientation_axes[2].axResolution == 0)
            {
              num_axes += 2;
            }
          else
            {
              num_axes += 3;
            }
        }
      if (source_device->pktdata & PK_TANGENT_PRESSURE)
        source_device->last_axis_data[num_axes++] = packet.pkTangentPressure;

      translated_buttons = button_map[packet.pkButtons & 0x07] | (packet.pkButtons & ~0x07);

      if (translated_buttons != source_device->button_state)
        {
          /* At least one button has changed state so produce a button event
           * If more than one button has changed state (unlikely),
           * just care about the first and act on the next the next time
           * we get a packet
           */
          button_diff = translated_buttons ^ source_device->button_state;

          /* Gdk buttons are numbered 1.. */
          event->button.button = 1;

          for (button_mask = 1; button_mask != 0x80000000;
               button_mask <<= 1, event->button.button++)
            {
              if (button_diff & button_mask)
                {
                  /* Found a button that has changed state */
                  break;
                }
            }

          if (!(translated_buttons & button_mask))
            {
              event->any.type = GDK_BUTTON_RELEASE;
              masktest = GDK_BUTTON_RELEASE_MASK;
            }
          else
            {
              event->any.type = GDK_BUTTON_PRESS;
              masktest = GDK_BUTTON_PRESS_MASK;
            }
          source_device->button_state ^= button_mask;
        }
      else
        {
          event->any.type = GDK_MOTION_NOTIFY;
          masktest = GDK_POINTER_MOTION_MASK;
          if (source_device->button_state & (1 << 0))
            masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK;
          if (source_device->button_state & (1 << 1))
            masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON2_MOTION_MASK;
          if (source_device->button_state & (1 << 2))
            masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK;
        }

      /* Now we can check if the window wants the event, and
       * propagate if necessary.
       */
      while ((gdk_window_get_device_events (window, GDK_DEVICE (source_device)) & masktest) == 0 &&
	     (gdk_device_get_device_type (GDK_DEVICE (source_device)) == GDK_DEVICE_TYPE_SLAVE &&
	      (gdk_window_get_events (window) & masktest) == 0))
        {
          GDK_NOTE (EVENTS_OR_INPUT, g_print ("... not selected\n"));

          if (window->parent == gdk_get_default_root_window () || window->parent == NULL)
            return FALSE;

          impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
          pt.x = x * impl->window_scale;
          pt.y = y * impl->window_scale;
          ClientToScreen (GDK_WINDOW_HWND (window), &pt);
          g_object_unref (window);
          window = window->parent;
          impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
          g_object_ref (window);
          ScreenToClient (GDK_WINDOW_HWND (window), &pt);
          x = pt.x / impl->window_scale;
          y = pt.y / impl->window_scale;
          GDK_NOTE (EVENTS_OR_INPUT, g_print ("... propagating to %p %+d%+d\n",
                                              GDK_WINDOW_HWND (window), x, y));
        }

      event->any.window = window;
      key_state = get_modifier_key_state ();
      if (event->any.type == GDK_BUTTON_PRESS ||
          event->any.type == GDK_BUTTON_RELEASE)
        {
          event->button.time = _gdk_win32_get_next_tick (msg->time);
	  if (source_device->sends_core)
	    gdk_event_set_device (event, device_manager->core_pointer);
          else
            gdk_event_set_device (event, GDK_DEVICE (source_device));
          gdk_event_set_source_device (event, GDK_DEVICE (source_device));
          gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));

          event->button.axes = g_new (gdouble, num_axes);
	  gdk_window_get_origin (window, &root_x, &root_y);

          _gdk_device_wintab_translate_axes (source_device,
                                             window,
                                             event->button.axes,
                                             &event->button.x,
                                             &event->button.y);

          event->button.x_root = event->button.x + root_x;
          event->button.y_root = event->button.y + root_y;

          event->button.state =
            key_state | ((source_device->button_state << 8)
                         & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
                            | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
                            | GDK_BUTTON5_MASK));

          GDK_NOTE (EVENTS_OR_INPUT,
                    g_print ("WINTAB button %s:%d %g,%g\n",
                             (event->button.type == GDK_BUTTON_PRESS ?
                              "press" : "release"),
                             event->button.button,
                             event->button.x, event->button.y));
        }
      else
        {
          event->motion.time = _gdk_win32_get_next_tick (msg->time);
          event->motion.is_hint = FALSE;
          if (source_device->sends_core)
            gdk_event_set_device (event, device_manager->core_pointer);
          else
            gdk_event_set_device (event, GDK_DEVICE (source_device));
          gdk_event_set_source_device (event, GDK_DEVICE (source_device));
          gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));

          event->motion.axes = g_new (gdouble, num_axes);
	  gdk_window_get_origin (window, &root_x, &root_y);

          _gdk_device_wintab_translate_axes (source_device,
                                             window,
                                             event->motion.axes,
                                             &event->motion.x,
                                             &event->motion.y);

          event->motion.x_root = event->motion.x + root_x;
          event->motion.y_root = event->motion.y + root_y;

          event->motion.state =
            key_state | ((source_device->button_state << 8)
                         & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
                            | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
                            | GDK_BUTTON5_MASK));

          GDK_NOTE (EVENTS_OR_INPUT,
                    g_print ("WINTAB motion: %g,%g\n",
                             event->motion.x, event->motion.y));
        }
      return TRUE;

    case WT_CSRCHANGE:
      if (device_manager->dev_entered_proximity > 0)
        device_manager->dev_entered_proximity -= 1;

      if ((source_device = 
        gdk_device_manager_find_wintab_device (device_manager,
								                                (HCTX) msg->lParam,
								                                packet.pkCursor)) == NULL)
        {
          /* Check for new cursors and try again */
          if (_wintab_recognize_new_cursors(device_manager,
                                            (HCTX) msg->lParam) == 0)
              return FALSE;
          if ((source_device =
            gdk_device_manager_find_wintab_device (device_manager,
                                                    (HCTX) msg->lParam,
                                                    packet.pkCursor)) == NULL)
            return FALSE;
        }

      if (source_device->sends_core &&
	        gdk_device_get_mode (GDK_DEVICE (source_device)) != GDK_MODE_DISABLED)
	      {
	        _gdk_device_virtual_set_active (device_manager->core_pointer,
					                                GDK_DEVICE (source_device));
	        _gdk_input_ignore_core += 1;
	      }
      return FALSE;

    case WT_PROXIMITY:
      if (LOWORD (msg->lParam) == 0)
        {
          if (_gdk_input_ignore_core > 0)
            {
	      _gdk_input_ignore_core -= 1;

	      if (_gdk_input_ignore_core == 0)
		_gdk_device_virtual_set_active (device_manager->core_pointer,
						device_manager->system_pointer);
	    }
	}
      else
	{
	  device_manager->dev_entered_proximity += 1;
	}

      return FALSE;
    }

  return FALSE;
}
