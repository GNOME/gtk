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

/* Windows 8 level declarations. Needed
   for Windows Pointer Input Stack API. */
#undef _WIN32_WINNT
#undef WINVER
#define _WIN32_WINNT 0x602
#define WINVER 0x602
#include <windows.h>

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
#include "gdkdevice-wintab.h"
#include "gdkdisplayprivate.h"
#include "gdkseatdefaultprivate.h"

typedef BOOL
(WINAPI *t_RegisterPointerDeviceNotifications) (HWND window,
                                                BOOL notifyRange);
typedef BOOL
(WINAPI *t_GetPointerDevices) (UINT32 *deviceCount,
                               POINTER_DEVICE_INFO *pointerDevices);
typedef BOOL
(WINAPI *t_GetPointerDeviceCursors) (HANDLE device,
                                     UINT32 *cursorCount,
                                     POINTER_DEVICE_CURSOR_INFO *deviceCursors);
typedef BOOL
(WINAPI *t_GetPointerDeviceRects) (HANDLE device,
                                   RECT *pointerDeviceRect
                                   RECT *displayRect);
typedef BOOL
(WINAPI *t_GetPointerCursorId) (UINT32 pointerId,
                                UINT32 *cursorId);
typedef BOOL
(WINAPI *t_GetPointerInfo) (UINT32 pointerId,
                            POINTER_INFO *pointerInfo);
typedef BOOL
(WINAPI *t_GetPointerFramePenInfoHistory) (UINT32 pointerId,
                                           UINT32 *entriesCount,
                                           UINT32 *pointerCount,
                                           POINTER_PEN_INFO *penInfo);
typedef BOOL
(WINAPI *t_GetPointerFrameTouchInfoHistory) (UINT32 pointerId,
                                             UINT32 *entriesCount,
                                             UINT32 *pointerCount,
                                             POINTER_TOUCH_INFO *touchInfo);
typedef BOOL
(WINAPI *t_GetPointerDeviceProperties) (HANDLE device,
                                        UINT32 *propertyCount,
                                        POINTER_DEVICE_PROPERTY *pointerProperties);
typedef BOOL
(WINAPI *t_GetRawPointerDeviceData) (UINT32 pointerId,
                                     UINT32 historyCount,
                                     UINT32 propertiesCount,
                                     POINTER_DEVICE_PROPERTY *pProperties,
                                     LONG *pValues);

typedef struct _GdkWinpointerEventAll {
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble x_root;
  gdouble y_root;
  gdouble *axes;
  guint state;
  guint button;
  gpointer *sequence;
  gboolean emulating_pointer;
} GdkWinpointerEventAll;

static t_RegisterPointerDeviceNotifications p_RegisterPointerDeviceNotifications;
static t_GetPointerDevices p_GetPointerDevices;
static t_GetPointerDeviceCursors p_GetPointerDeviceCursors;
static t_GetPointerDeviceRects p_GetPointerDeviceRects;
static t_GetPointerCursorId p_GetPointerCursorId;
static t_GetPointerInfo p_GetPointerInfo;
static t_GetPointerFramePenInfoHistory p_GetPointerFramePenInfoHistory;
static t_GetPointerFrameTouchInfoHistory p_GetPointerFrameTouchInfoHistory;
static t_GetPointerDeviceProperties p_GetPointerDeviceProperties;
static t_GetRawPointerDeviceData p_GetRawPointerDeviceData;

#define HID_DIGITIZER_PRESSURE 0x30
#define HID_DIGITIZER_XTILT 0x3D
#define HID_DIGITIZER_YTILT 0x3E
#define HID_DIGITIZER_ROTATION 0x41

static ATOM winpointer_notif_window_class;
static HWND winpointer_notif_window_handle;

#define WINTAB32_DLL "Wintab32.dll"

#define PACKETDATA (PK_CONTEXT | PK_CURSOR | PK_BUTTONS | PK_X | PK_Y  | PK_NORMAL_PRESSURE | PK_ORIENTATION)
/* We want everything in absolute mode */
#define PACKETMODE (0)
#include <pktdef.h>

#define DEBUG_WINTAB 1		/* Verbose debug messages enabled */
#define TWOPI (2 * G_PI)

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

static GdkDeviceWinpointer*
gdk_device_manager_find_winpointer_device (GdkDeviceManagerWin32 *device_manager,
                                           HANDLE device_handle,
                                           UINT32 cursor_id,
                                           GdkInputSource source)
{
  GList *l;

  for (l = device_manager->winpointer_devices; l; l = l->next)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (l->data);
      device_source = gdk_device_get_source (GDK_DEVICE (device));

      if (device->device_handle == device_handle &&
          device->start_cursor_id <= cursor_id &&
          device->end_cursor_id >= cursor_id &&
          device_source == source)
        {
          return device;
        }
    }

  return NULL;
}

static void
winpointer_event_finalize_send (GdkDeviceManagerWin32 *device_manager,
                                GdkDeviceWinpointer *device,
                                GdkEvent *evt)
{
  gdk_event_set_device (evt, device_manager->core_pointer);
  gdk_event_set_source_device (evt, GDK_DEVICE (device));
  gdk_event_set_seat (evt, gdk_device_get_seat (device_manager->core_pointer));
  /*TODO: better move to gdkevents-win32.c? */
  _gdk_win32_append_event (evt);
}

static void
winpointer_fill_proximity_event (GdWinpointerEventAll *evt_all)
{
  GdkEvent *evt = gdk_event_new (evt_all->type);

  evt->proximity.window = g_object_ref (window);
  evt->proximity.time = evt_all.time;
}

static void
winpointer_fill_button_event (GdkWinpointerEventAll *evt_all)
{
  GdkEvent *evt = gdk_event_new (evt_all->type);

  evt->button.window = g_object_ref (window);
  evt->button.time = evt_all.time;
  evt->button.x_root = evt_all.x_root;
  evt->button.x_root = evt_all.x_root;
  evt->button.x = evt_all.x;
  evt->button.y = evt_all.y;
  evt->button.state = evt_all.state;
  evt->button.axes = evt_all.axes;
  evt->button.button = evt_all.button;
}

static void
winpointer_fill_motion_event (GdkWinpointerEventAll *evt_all)
{
  GdkEvent *evt = gdk_event_new (evt_all->type);

  evt->motion.window = g_object_ref (window);
  evt->motion.time = evt_all.time;
  evt->motion.x_root = evt_all.x_root;
  evt->motion.x_root = evt_all.x_root;
  evt->motion.x = evt_all.x;
  evt->motion.y = evt_all.y;
  evt->motion.state = evt_all.state;
  evt->motion.axes = evt_all.axes;
}

static GdkEvent*
winpointer_fill_touch_event (GdkWinpointerEventAll *evt_all)
{
  GdkEvent *evt = gdk_event_new (evt_all->type);

  evt->touch.window = g_object_ref (window);
  evt->touch.time = evt_all.time;
  evt->touch.x_root = evt_all.x_root;
  evt->touch.x_root = evt_all.x_root;
  evt->touch.x = evt_all.x;
  evt->touch.y = evt_all.y;
  evt->touch.state = evt_all.state;
  evt->touch.axes = evt_all.axes;
  evt->touch.sequence = evt_all.sequence;
  evt->touch.emulating_pointer = evt_all.emulating_pointer;
}

static void
winpointer_make_event ()
{
  GdkWinpointerEventAll evt_all;
  POINT client_area_origin_in_screen;

  /*TODO: info->pointerInfo.pointerFlags & POINTER_FLAG_CANCELED*/

  /*TODO: also use PerformanceCount? */
  event.time = info->pointerInfo.dwTime;
  if (event.time == 0)
    event.time = _gdk_win32_get_next_tick (msg->time);

  /*TODO: consider using predicted coordinates */
  event.x_root = info->pointerInfo.ptHimetricLocationRaw.x * device->scale_x;
  event.y_root = info->pointerInfo.ptHimetricLocationRaw.y * device->scale_y;

  client_area_origin_in_screen.x = 0;
  client_area_origin_in_screen.y = 0;
  /*TODO: how does ClientToScreen work for child windows? */
  /*TODO: also consider MapWindowPoints */
  ClientToScreen (GDK_WINDOW_HWND (window), &client_area_origin_in_screen);
  event.x = event.x_root - client_area_origin_in_screen.x;
  event.y = event.x_root - client_area_origin_in_screen.y;

  event.x_root /= impl->window_scale;
  event.y_root /= impl->window_scale;
  event.x /= impl->window_scale;
  event.y /= impl->window_scale;

  /*TODO: simply use get_modifier_key_state()? */
  event.state = 0;
  if (info->pointerInfo.dwKeyStates & POINTER_MOD_CTRL)
    event.state |= GDK_CONTROL_MASK;
  if (info->pointerInfo.dwKeyStates & POINTER_MOD_SHIFT)
    event.state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    event.state |= GDK_MOD1_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    event.state |= GDK_LOCK_MASK;

  /*TODO: info->pointerInfo.ButtonChangeType */

  if (info->penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER))
    /*eraser*/

  /*TODO: range? */
  event.axes = g_new0 (gdouble, device->num_axes);
  if (info->penMask & TOUCH_MASK_PRESSURE)
    event.axes[0] = (gdouble) info->pressure;
  if (info->penMask & TOUCH_MASK_ORIENTATION)
    event.axes[1] = (gdouble) info->orientation;

  event.sequence = (GdkEventSequence*) GUINT_TO_POINTER (info->pointerInfo.pointerId);
  event.emulating_pointer = (info.pointerFlags & POINTER_FLAG_PRIMARY) ? TRUE : FALSE;

  /*TODO*/
  /* should be a right click, barrel button down */
  if (info.pointerFlags & POINTER_FLAG_FIRSTBUTTON)
    event.button = 1;
  else if (info.pointerFlags & POINTER_FLAG_SECONDBUTTON)
    event.button = 3;

  if (msg->message == WM_POINTERENTER)
    {
      GdkEvent *evt = gdk_event_new (GDK_PROXIMITY_IN);
      winpointer_fill_proximity_event (&evt_all);
    }
  if (msg->message == WM_POINTERDOWN)
    {
      if (type == PT_PEN)
        {
          GdkEvent *evt = gdk_event_new
        }
    }
  else if (msg->message == WM_POINTERENTER)
}

static gboolean
winpointer_is_eraser (POINTER_PEN_INFO *pen_info)
{
  return ((pen_info.penFlags & PEN_FLAG_INVERTED) ||
          (pen_info.penFlags & PEN_FLAG_ERASER));
}

static GdkWindow*
winpointer_find_target_window (GdkWindow *window,
                               GdkDevice *device,
                               GdkEventMask mask)
{
  GdkWindow *target_window = window;

  do
    {
      GdkEventMask device_mask = gdk_window_get_device_events (target_window, device);
      GdkEventMask mask = gdk_window_get_events (target_window);

      if ((device_mask & mask_test) != 0 || (mask & mask_test) != 0)
        return target_window;

      if (target_window->parent == gdk_get_default_root_window ())
        return NULL;
    }
  while ((target_window = target_window->parent) != NULL);

  return NULL;
}

static GdkEvent*
winpointer_alloc_event (MSG *msg,
                        POINTER_INFO *info)
{
  switch (msg->message)
    {
    case WM_POINTERENTER:
      return gdk_event_new (GDK_PROXIMITY_IN);
    case WM_POINTERLEAVE:
      return gdk_event_new (GDK_PROXIMITY_OUT);
    }

  if (info->pointerType == PT_PEN)
    {
      switch (msg->message)
        {
        case WM_POINTERDOWN:
          return gdk_event_new (GDK_BUTTON_PRESS);
        case WM_POINTERUPDATE:
          return gdk_event_new (GDK_MOTION_EVENT);
        case WM_POINTERUP:
          return gdk_event_new (GDK_BUTTON_RELEASE);
        }
    }
  else if (info->pointerType == PT_TOUCH)
    {
      switch (msg->message)
        {
        case WM_POINTERDOWN:
          return gdk_event_new (GDK_TOUCH_BEGIN);
        case WM_POINTERUPDATE:
          return gdk_event_new (GDK_TOUCH_UPDATE);
        case WM_POINTERUP:
          return gdk_event_new (GDK_TOUCH_END);
        }
    }

  return NULL;
}

static GdkEventMask
winpointer_get_mask (MSG *msg,
                     POINTER_INFO *info)
{
  if (msg->message == WM_POINTERENTER)
    return GDK_PROXIMITY_IN_MASK;
  else if (msg->message == WM_POINTERLEAVE)
    return GDK_PROXIMITY_OUT_MASK;

  if (info->pointerType == PT_PEN)
    {
      switch (msg->message)
        {
        case WM_POINTERDOWN:
          return GDK_BUTTON_PRESS_MASK;
        case WM_POINTERUP:
          return GDK_BUTTON_RELEASE_MASK;
        case WM_POINTERUPDATE:
          {
            /*TODO*/
            if (info->pointerFlags & )
            else if (info->pointerFlags & )
            else if (info->pointerFlags & )
            else if (info->pointerFlags & )
          }
        }
    }
  else if (info->pointerType == PT_TOUCH)
    {
      return GDK_TOUCH_MASK;
    }

  return 0;
}

gboolean
gdk_input_winpointer_event (GdkDisplay *display,
                            MSG        *msg,
                            GdkWindow  *window)
{
  GdkDeviceManagerWin32 *device_manager = NULL;
  GdkWindow *target_window = NULL;
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INFO info;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  device_manager = GDK_DEVICE_MANAGER_WIN32 (gdk_display_get_device_manager (display));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!p_GetPointerInfo (pointer_id, &info))
    {
      WIN32_API_FAILED ("GetPointerInfo");
      return;
    }

  if (!p_GetCursorId (pointer_id, &cursor_id)
    {
      WIN32_API_FAILED ("GetCursorId");
      return;
    }

  if (info.pointerType == PT_PEN)
    {
      GdkDeviceWinpointer *device_tip = NULL;
      GdkDeviceWinpointer *device_eraser = NULL;

      device_tip = gdk_device_manager_find_winpointer_device (device_manager,
                                                              info.sourceDevice,
                                                              cursor_id,
                                                              GDK_SOURCE_PEN);
      device_eraser = gdk_device_manager_find_winpointer_device (device_manager,
                                                                 info.sourceDevice,
                                                                 cursor_id,
                                                                 GDK_SOURCE_ERASER);

      if (device->last_frame_id == info.frameId)
        return TRUE;
      device->last_frame_id = info.frameId;
      device_eraser->last_frame_id = info.frameId;

      /*TODO: could frame be equal on different messages? */

      if (msg->message == WM_POINTERENTER ||
          msg->message == WM_POINTERLEAVE ||
          msg->message == WM_POINTERDOWN ||
          msg->message == WM_POINTERUP)
        {
          POINTER_PEN_INFO info;
          UINT32 cursor_count = 0;
          UINT32 i = 0;
          GdkEventMask mask = 0;
          GdkWindow *target_window = NULL;
          GdkDevice *device = NULL;

          if (!p_GetPointerFramePenInfo (pointer_id, &cursor_count, &info))
            {
              WIN32_API_FAILED_ONCE ("GetPointerFramePenInfoHistory");
              return TRUE;
            }

          device = winpointer_is_eraser (info) ? device_eraser : device_tip;

          mask = winpointer_get_input_mask (msg, &info.pointerInfo);
          target_window = winpointer_find_target_window (window, device, mask);

          /* could not find any GdkWindow interested in this event */
          if (!target_window)
            return FALSE;

          winpointer_make_event (device_manager, msg, info, target_window);
        }
      else if (msg->message == WM_POINTERUPDATE)
        {
          POINTER_PEN_INFO *infos = NULL;
          UINT32 history_count = 0;
          UINT32 cursor_count = 0;
          UINT32 i = 0;
          UINT32 j = 0;

          do {
            infos = g_new0 (POINTER_PEN_INFO, history_count * cursor_count);
            if (!p_GetPointerFramePenInfoHistory (pointer_id, &history_count, &cursor_count, infos))
              {
                WIN32_API_FAILED_ONCE ("GetPointerFramePenInfoHistory");
                g_free (infos);
                return TRUE;
              }
          } while (!infos && history_count > 0 && cursor_count > 0);

          for (i = history_count; i > 0; i--)
            {
              for (j = 0; j < cursor_count; j++)
                {
                  POINTER_PEN_INFO *info = &info[(i-1)*cursor_count + j];
                  GdkEventMask mask = winpointer_get_mask (msg, &info->pointerInfo);
                  GdkWindow *target_window = target_window[mask]; /*TODO*/

                  if (!target_window)
                    {
                      target_window = winpointer_find_target_window (window, device, mask);
                      target_window[mask] = target_window;
                    }

                  if (!target_window)
                    continue;

                  winpointer_make_event (device_manager, msg, info, target_window);
                }
            }
        }
    }
  else if (info.pointerType == PT_TOUCH)
    {
      GdkDeviceWinpointer *device;

      device = gdk_device_manager_find_winpointer_device (device_manager,
                                                          info.sourceDevice,
                                                          cursor_id,
                                                          GDK_SOURCE_TOUCHSCREEN);

      if (device->last_frame_id == info.frameId)
        return TRUE;
      device->last_frame_id = info.frameId;

      
    }

  return TRUE;
}

static gboolean
winpointer_device_update_scale_factors (GdkDeviceWinpointer *device)
{
  RECT device_rect;
  RECT display_rect;
  LONG device_width;
  LONG device_height;
  LONG display_width;
  LONG display_height;

  if (!p_GetPointerDeviceRects (device->device_handle,
                                &device_rect,
                                &display_rect))
    {
      WIN32_API_FAILED ("GetPointerDeviceRects");
      return FALSE;
    }

  device_width = device_rect.right - device_rect.left;
  device_height = device_rect.bottom - device_rect.top;
  display_width = display_rect.right - display_rect.left;
  display_height = display_rect.bottom - display_rect.top;

  if (device_width == 0 ||
      device_height == 0 ||
      display_width == 0 ||
      display_height == 0)
    {
      g_warning ("Invalid coordinates from GetPointerDeviceRects");
      return FALSE;
    }

  device->scale_x = ((gdouble)display_width) / ((gdouble)device_width);
  device->scale_y = ((gdouble)display_height) / ((gdouble)device_height);

  return TRUE;
}

static gboolean
winpointer_find_device_in_info (GdkDeviceWinpointer *device,
                                POINTER_DEVICE_INFO *device_infos,
                                UINT32 device_infos_count)
{
  UINT32 i = 0;

  for (i = 0; i < device_infos_count; i++)
    {
      if (device->device_handle == devices_info[i].device &&
          device->start_cursor_id == devices_info[i].startingCursorId)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
winpointer_hid_supports_usage (GArray *hid_props,
                               USHORT page,
                               USHORT usage,
                               double *min_val,
                               double *max_val,
                               double *res_val)
{
  UINT32 i = 0;

  for (i = 0; i < hid_props->len; i++)
    {
      if (hid_properties[i].usagePageId == page && hid_properties[i].usageId == usage)
        {
          if (hid_properties[i].physicalMin != hid_properties[i].physicalMax)
            {
              /*TODO*/
              *min = hid_properties[i].physicalMin;
              *max = hid_properties[i].physicalMax;
              *res = 1.0;
              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
winpointer_hid_check_properties ()
{
}

static gboolean
winpointer_hid_supports_eraser (GArray *hid_props)
{
  /*TODO: stub */
  return TRUE;
}

static GdkAxisFlags
winpointer_plain_get_supported_axis_flags (GArray *hid_props)
{
  GdkAxisFlags flags = GDK_AXIS_FLAG_X | GDK_AXIS_FLAG_Y; /* always supported */
  guint i = 0;

  for (i = 0; i < hid_props->len; i++)
    {
      POINTER_DEVICE_PROPERTY *prop;
      prop = g_array_index (hid_props, POINTER_DEVICE_PROPERTY, i);

      if (prop->usagePageId == 0x0D)
        {
          if (prop->logicalMin != prop->logicalMax && prop->unit != 0) /*TODO*/
            {
              switch (prop->usageId)
                {
                case HID_DIGITIZER_PRESSURE:
                  flags |= GDK_AXIS_FLAG_PRESSURE;
                break;
                case HID_DIGITIZER_XTILT:
                  flags |= GDK_AXIS_FLAG_XTILT;
                break;
                case HID_DIGITIZER_YTILT:
                  flags |= GDK_AXIS_FLAG_YTILT;
                break;
                case HID_DIGITIZER_ROTATION:
                  flags |= GDK_AXIS_FLAG_ROTATION;
                break;
                }
            }
        }
    }
}

static GdkDeviceWinpointer*
winpointer_create_device_inner (GdkDeviceManagerWin32 *device_manager,
                                GdkInputSource source,
                                POINTER_DEVICE_INFO *info,
                                GArray *hid_props)
{
  GdkDeviceWinpointer *device = NULL;
  gchar *base_name = NULL;
  gchar *name = NULL;
  gdouble min_val = 0.0;
  gdouble max_val = 0.0;
  gdouble res_val = 0.0;
  guint num_touches = 0;
  UINT32 num_cursors = 0;

  if (info->productString != NULL && info->productString[0] != 0)
    base_name = g_utf16_to_utf8 (info->productString);
  if (!base_name)
    base_name = g_strdup ("Unnamed");

  if (source == GDK_SOURCE_PEN)
    name = g_strconcat (base_name, " Pen");
  else if (source == GDK_SOURCE_ERASER)
    name = g_strconcat (base_name, " Eraser");
  else if (source == GDK_SOURCE_TOUCHSCREEN)
    {
      name = g_strconcat (base_name, " TouchScreen");
      num_touches = info->maxActiveContacts;
    }

  device = g_object_new (GDK_TYPE_DEVICE_WINPOINTER,
                         "name", name,
                         "type", GDK_DEVICE_TYPE_FLOATING,
                         "input-source", source,
                         "input-mode", GDK_MODE_SCREEN,
                         "has-cursor", TRUE,
                         "display", display,
                         "device-manager", device_manager,
                         NULL);

  /* No need to add axis for X, Y. */

  if (source == GDK_SOURCE_PEN || source == GDK_SOURCE_ERASER)
    {
      if (_gdk_win32_tablet_api == GDK_WIN32_TABLET_API_WINPOINTER_ADVANCED)
        {
          if (winpointer_supports_hid_usage (hid_props, 0x0D, HID_DIGITIZER_PRESSURE, &min_val, &max_val, &res_val))
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_PRESSURE, min_val, max_val, res_val);
          if (winpointer_supports_hid_usage (hid_props, 0x0D, HID_DIGITIZER_XTILT, &min_val, &max_val, &res_val))
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_XTILT, min_val, max_val, res_val);
          if (winpointer_supports_hid_usage (hid_props, 0x0D, HID_DIGITIZER_YTILT, &min_val, &max_val, &res_val))
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_YTILT, min_val, max_val, res_val);
          if (winpointer_supports_hid_usage (hid_props, 0x0D, HID_DIGITIZER_ROTATION, &min_val, &max_val, &res_val))
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_ROTATION, min_val, max_val, res_val);
        }
      else /* plain API */
        {
          GdkAxisFlags axis_flags = winpointer_plain_get_supported_axis_flags (device);
          if (axis_flags & GDK_AXIS_FLAG_PRESSURE)
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1024.0, 1.0);
          if (axis_flags & GDK_AXIS_FLAG_XTILT)
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_XTILT, -90.0, 90.0, 1.0);
          if (axis_flags & GDK_AXIS_FLAG_YTILT)
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_YTILT, -90.0, 90.0, 1.0);
          if (axis_flags & GDK_AXIS_FLAG_ROTATION)
            _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_ROTATION, 0.0, 359.0, 1.0);
        }
    }
  else if (source == GDK_SOURCE_TOUCHSCREEN)
    {
      /*TODO: add diameter informations? */
      GdkAxisFlags axis_flags = winpointer_plain_get_supported_axis_flags (device);
      if (axis_flags & GDK_AXIS_FLAG_PRESSURE)
        _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1024.0, 1.0);
      if (axis_flags & GDK_AXIS_FLAG_ROTATION) /*TODO*/
        _gdk_device_add_axis (GDK_DEVICE (device), GDK_NONE, GDK_AXIS_ROTATION, 0.0, 359.0, 1.0);
    }

  if (!p_GetPointerDeviceCursors (info->device, &num_cursors, NULL))
    {
      WIN32_API_FAILED ("GetPointerDeviceCursors");
      g_clear_pointer (&device, g_object_unref);
      goto cleanup;
    }

  if (num_cursors == 0)
    {
      g_clear_pointer (&device, g_object_unref);
      goto cleanup;
    }

  device->device_handle = info->device;
  device->start_cursor_id = info->startingCursorId;
  device->end_cursor_id = info->startingCursorId + num_cursors - 1;
  device->hid_props = g_array_ref (hid_props);

  if !winpointer_device_update_scale_factors (device))
    {
      g_clear_pointer (&device, g_object_unref);
      goto cleanup;
    }

  /*TODO: is it ok for touchscreen devices? */
  _gdk_device_set_associated_device (device_manager->system_pointer, GDK_DEVICE (device));
  _gdk_device_add_slave (device_manager->core_pointer, GDK_DEVICE (device));

  device_manager->winpointer_devices = g_list_append (
                                         device_manager->winpointer_devices,
                                         device);

  g_signal_emit_by_name (device_manager, "device-added", device, NULL);

cleanup:
  g_free (name);
  g_free (base_name);

  return device;
}

static void
winpointer_create_devices (GdkDeviceManagerWin32 *device_manager,
                           POINTER_DEVICE_INFO *info)
{
  HANDLE device_handle = info->device;
  UINT32 properties_count = 0;
  GArrray *hid_props = NULL;

  if (!p_GetPointerDeviceProperties (device_handle, &properties_count, NULL))
    {
      WIN32_API_FAILED ("GetPointerDeviceProperties");
      return;
    }

  hid_props = g_array_new (FALSE, TRUE, sizeof (POINTER_DEVICE_PROPERTY));
  g_array_set_size (hid_props, properties_count);
  if (!p_GetPointerDeviceProperties (device_handle, &properties_count, hid_props->data))
    {
      WIN32_API_FAILED ("GetPointerDeviceProperties");
      g_array_unref (hid_props);
      return;
    }
  g_array_set_size (hid_props, properties_count);

  if (info->pointerDeviceType == POINTER_DEVICE_TYPE_INTEGRATED_PEN ||
      info->pointerDeviceType == POINTER_DEVICE_TYPE_EXTERNAL_PEN)
    {
      winpointer_create_device_inner (device_manager,
                                      GDK_SOURCE_PEN,
                                      info->device,
                                      hid_props);

      if (winpointer_hid_supports_eraser (hid_props))
        {
          winpointer_create_device_inner (device_manager,
                                          GDK_SOURCE_ERASER,
                                          info->device,
                                          hid_props);
        }
    }
  else if (info->pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH)
    {
      winpointer_create_device_inner (device_manager,
                                      GDK_SOURCE_TOUCHSCREEN,
                                      info->device,
                                      hid_props);
    }

  g_array_unref (hid_props);
}

static void
winpointer_remove_device (GdkDeviceManagerWin32 *device_manager,
                          GdkDeviceWinpointer *device)
{
  g_signal_emit_by_name (device_manager, "device-removed", device, NULL);

  /* TODO: does set_associated_devices take a ref? */
  g_object_dispose (device);
  device_manager->winpointer_devices = g_list_remove (device_manager->winpointer_devices,
                                                      device,
                                                      g_object_unref);
}

static void
winpointer_enumerate_devices (GdkDeviceManagerWin32 *device_manager)
{
  POINTER_DEVICE_INFO *infos = NULL;
  UINT32 infos_count = 0;
  GList *l = NULL;
  UINT32 i = 0;

  if (!p_GetPointerDevices (&infos_count, NULL))
    {
      WIN32_API_FAILED ("GetPointerDevices");
      return;
    }
  infos = g_new0 (POINTER_DEVICE_INFO, infos_count);
  if (!p_GetPointerDevices (&infos_count, infos))
    {
      WIN32_API_FAILED ("GetPointerDevices");
      g_free (infos);
      return;
    }

  for (l = device_manager->winpointer_devices; l; l = l->next)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (l->data);

      if (winpointer_find_device_in_info (device, infos, infos_count))
        {
          winpointer_device_update_scale_factors (device);
        }
      else
        {
          winpointer_remove_device (device);
        }
    }

  for (i = 0; i < infos_count; i++)
    {
      POINTER_DEVICE_INFO *info = &infos[i];

      if (!gdk_device_manager_find_winpointer_device (device_manager,
                                                      info->device,
                                                      info->startingCursorId))
        {
          winpointer_create_devices (device_manager, info);
        }
    }

  g_free (infos);
}

static LRESULT CALLBACK
winpointer_notif_window_proc (HWND hwnd,
                              UINT msg,
                              WPARAM wParam,
                              LPARAM lParam)
{
  if (msg == WM_POINTERDEVICECHANGE)
    {
      LONG_PTR data = GetWindowLongPtrW (hwnd, GWLP_USERDATA);
      GdkDeviceManager *device_manager = GDK_DEVICE_MANAGER (data);

      winpointer_enumerate_devices (device_manager);
      return 0;
    }

  return DefWindowProc (hwnd, msg, wParam, lParam);
}

static gboolean
winpointer_notif_window_create ()
{
  WNDCLASSEXW wndclass;

  memset (&wndclass, 0, sizeof (wndclass));
  wndclass.cbSize = sizeof (wndclass);
  wndclass.lpszClassName = L"GdkWin32WinPointerNotificationsWindowClass";
  wndclass.lpfnWndProc = (WNDPROC) winpointer_notif_window_proc;
  wndclass.hInstance = _gdk_dll_hinstance; /*TODO*/

  if ((winpointer_notif_window_class = RegisterClassExW (&wndclass)) == 0)
    {
      g_warning ("Could not register window class for WinPointer device notifications");
      return FALSE;
    }

  if ((winpointer_notif_window_handle = CreateWindowExW (
                                          0,
                                          (LPCWSTR) winpointer_notif_window_class,
                                          L"GdkWin32 WinPointer Notifications",
                                          0,
                                          0, 0, 0, 0,
                                          HWND_MESSAGE,
                                          NULL,
                                          _gdk_dll_hinstance,
                                          NULL) == NULL)
    {
      g_warning ("Could not create window for WinPointer device notifications");
      return FALSE;
    }

  return TRUE;
}

static gboolean
winpointer_ensure_procedures ()
{
    /*TODO use g_once_init_enter/leave? */
  static HMODULE user32_dll = NULL;

  if (!user32_dll)
    {
      user32_dll = LoadLibraryW (L"user32.dll");
      if (!user32_dll)
        {
          g_warning ("Failed to load user32.dll"); /*TODO*/
          return FALSE;
        }

      p_RegisterPointerDeviceNotifications = (t_RegisterPointerDeviceNotifications)
        GetProcAddress (user32_dll, "RegisterPointerDeviceNotifications");

      p_GetPointerDevices = (t_GetPointerDevices)
        GetProcAddress (user32_dll, "GetPointerDevices");

      p_GetPointerDeviceCursors = (t_GetPointerDeviceCursors)
        GetProcAddress (user32_dll, "GetPointerDeviceCursors");

      p_GetPointerDeviceRects = (t_GetPointerDeviceRects)
        GetProcAddress (user32_dll, "GetPointerDeviceRects");

      p_GetPointerCursorId = (t_GetPointerCursorId)
        GetProcAddress (user32_dll, "GetPointerCursorId");

      p_GetPointerInfo = (t_GetPointerInfo)
        GetProcAddress (user32_dll, "GetPointerInfo");

      p_GetPointerFramePenInfoHistory = (t_GetPointerFramePenInfoHistory)
        GetProcAddress (user32_dll, "GetPointerFramePenInfoHistory");

      p_GetPointerFrameTouchInfoHistory = (t_GetPointerFrameTouchInfoHistory)
        GetProcAddress (user32_dll, "GetPointerFrameTouchInfoHistory");

      p_GetPointerDeviceProperties = (t_GetPointerDeviceProperties)
        GetProcAddress (user32_dll, "GetPointerDeviceProperties");

      p_GetRawPointerDeviceData = (t_GetRawPointerDeviceData)
        GetProcAddress (user32_dll, "GetRawPointerDeviceData");
    }

  return (p_RegisterPointerDeviceNotifications &&
          p_GetPointerDevices &&
          p_GetPointerDeviceCursors &&
          p_GetPointerDeviceRects &&
          p_GetPointerCursorId &&
          p_GetPointerInfo &&
          p_GetPointerFramePenInfoHistory &&
          p_GetPointerFrameTouchInfoHistory &&
          p_GetPointerDeviceProperties &&
          p_GetRawPointerDeviceData);
}

static gboolean
winpointer_init_check (GdkDeviceManagerWin32 *device_manager)
{
  if (!winpointer_ensure_procedures ())
    return FALSE;

  if (!winpointer_notif_window_create ())
    return FALSE;

  if (!API_CALL (SetWindowLongPtrW, (winpointer_notif_window_handle,
                                     GWLP_USERDATA,
                                     device_manager)))
    return FALSE;

  if (!p_RegisterPointerDeviceNotifications (winpointer_notif_window_handle, FALSE))
    {
      WIN32_API_FAILED ("RegisterPointerDeviceNotifications");
      return FALSE;
    }

  winpointer_enumerate_devices (device_manager);

  return TRUE;
}

static void
wintab_init_check (GdkDeviceManagerWin32 *device_manager)
{
  GdkDisplay *display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  GdkWindow *root = gdk_screen_get_root_window (gdk_display_get_default_screen (display));
  static gboolean wintab_initialized = FALSE;
  GdkDeviceWintab *device;
  GdkWindowAttr wa;
  WORD specversion;
  HCTX *hctx;
  UINT ndevices, ncursors, ncsrtypes, firstcsr, hardware;
  BOOL active;
  DWORD physid;
  AXIS axis_x, axis_y, axis_npressure, axis_or[3];
  UINT devix, cursorix;
  int i, num_axes = 0;
  wchar_t devname[100], csrname[100];
  gchar *devname_utf8, *csrname_utf8, *device_name;
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
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_NCSRTYPES, &ncsrtypes);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_FIRSTCSR, &firstcsr);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_HARDWARE, &hardware);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_X, &axis_x);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_Y, &axis_y);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_NPRESSURE, &axis_npressure);
      (*p_WTInfoA) (WTI_DEVICES + devix, DVC_ORIENTATION, axis_or);

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
#if 0
      (*p_WTEnable) (*hctx, TRUE);
#endif
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
      for (cursorix = firstcsr; cursorix < firstcsr + ncsrtypes; cursorix++)
        {
#ifdef DEBUG_WINTAB
          GDK_NOTE (INPUT, (g_print("Cursor %u:\n", cursorix), print_cursor (cursorix)));
#endif
          active = FALSE;
          (*p_WTInfoA) (WTI_CURSORS + cursorix, CSR_ACTIVE, &active);
          if (!active)
            continue;

          /* Wacom tablets seem to report cursors corresponding to
           * nonexistent pens or pucks. At least my ArtPad II reports
           * six cursors: a puck, pressure stylus and eraser stylus,
           * and then the same three again. I only have a
           * pressure-sensitive pen. The puck instances, and the
           * second instances of the styluses report physid zero. So
           * at least for Wacom, skip cursors with physid zero.
           */
          (*p_WTInfoA) (WTI_CURSORS + cursorix, CSR_PHYSID, &physid);
          if (wcscmp (devname, L"WACOM Tablet") == 0 && physid == 0)
            continue;

          (*p_WTInfoW) (WTI_CURSORS + cursorix, CSR_NAME, csrname);
          csrname_utf8 = g_utf16_to_utf8 (csrname, -1, NULL, NULL, NULL);
          device_name = g_strconcat (devname_utf8, " ", csrname_utf8, NULL);

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

          g_free (csrname_utf8);

          device->hctx = *hctx;
          device->cursor = cursorix;
          (*p_WTInfoA) (WTI_CURSORS + cursorix, CSR_PKTDATA, &device->pktdata);

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
              device->orientation_axes[0] = axis_or[0];
              device->orientation_axes[1] = axis_or[1];

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

          g_free (device_name);
        }

      g_free (devname_utf8);
    }
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

  if (_gdk_win32_tablet_api == GDK_WIN32_TABLET_API_WINPOINTER_PLAIN ||
      _gdk_win32_tablet_api == GDK_WIN32_TABLET_API_WINPOINTER_ADVANCED)
    {
      if (!g_win32_check_windows_version (6, 2, 0, G_WIN32_OS_ANY))
        {
          g_warning ("WinPointer tablet input API is not available on this system. Switching to WinTab.");
          _gdk_win32_tablet_api == GDK_WIN32_TABLET_API_WINTAB;
        }
      if (!winpointer_initialize (device_manager))
        {
          g_warning ("Switching to WinTab.");
          _gdk_win32_tablet_api == GDK_WIN32_TABLET_API_WINTAB;
        }
    }
  if (_gdk_win32_tablet_api == GDK_WIN32_TABLET_API_WINTAB)
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
          devices = g_list_prepend (devices, device_manager_win32->winpointer_devices);
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

void
_gdk_input_wintab_set_tablet_active (void)
{
  GList *tmp_list;
  HCTX *hctx;

  /* Bring the contexts to the top of the overlap order when one of the
   * application's windows is activated */

  if (!wintab_contexts)
    return; /* No tablet devices found, or Wintab not initialized yet */

  GDK_NOTE (INPUT, g_print ("_gdk_input_wintab_set_tablet_active: "
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
  if ((axes == NULL) ||
      (axes[0].axResolution == 0) ||
      (axes[1].axResolution == 0))
    {
      axis_data[0] = 0;
      axis_data[1] = 0;
      return;
    }

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
gdk_input_wintab_event (GdkDisplay *display,
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
      g_warning ("gdk_input_wintab_event: not wintab_window?");
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
	    g_print ("gdk_input_wintab_event: window=%p %+d%+d\n",
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
          num_axes += 2;
        }

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
          gdk_event_set_device (event, device_manager->core_pointer);
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

      if ((source_device = gdk_device_manager_find_wintab_device (device_manager,
								  (HCTX) msg->lParam,
								  packet.pkCursor)) == NULL)
	return FALSE;

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
