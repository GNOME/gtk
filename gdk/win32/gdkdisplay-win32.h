/*
 * gdkdisplay-win32.h
 *
 * Copyright 2014 Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gdkdisplayprivate.h"

#pragma once

#include "gdkwin32screen.h"
#include "gdkwin32cursor.h"
#include "gdkprivate-win32.h"
 
#include "gdkglversionprivate.h"

/* Used for active language or text service change notifications */
#define COBJMACROS
#include <msctf.h>

#ifdef HAVE_EGL
# include <epoxy/egl.h>
#endif

struct _GdkWin32PointerDeviceItems
{
  /* Input Core items */
  int input_ignore_core;
};

typedef struct _GdkWin32PointerDeviceItems GdkWin32PointerDeviceItems;

typedef struct _GdkWin32InputLocaleItems GdkWin32InputLocaleItems;

struct _GdkWin32CbDnDItems
{
  /* used to identify the main thread for this GdkWin32Display */
  GThread *display_main_thread;

  GdkWin32Clipdrop *clipdrop;
};
typedef struct _GdkWin32CbDnDItems GdkWin32CbDnDItems;

/* Define values used to set DPI-awareness */
typedef enum _GdkWin32ProcessDpiAwareness {
  PROCESS_DPI_UNAWARE = 0,
  PROCESS_SYSTEM_DPI_AWARE = 1,
  PROCESS_PER_MONITOR_DPI_AWARE = 2,
  PROCESS_PER_MONITOR_DPI_AWARE_V2 = 3, /* Newer HiDPI type for Windows 10 1607+ */
} GdkWin32ProcessDpiAwareness;

/* From https://docs.microsoft.com/en-US/windows/win32/hidpi/dpi-awareness-context */
/* DPI_AWARENESS_CONTEXT is declared by DEFINE_HANDLE */
#ifndef DPI_AWARENESS_CONTEXT_UNAWARE
#define DPI_AWARENESS_CONTEXT_UNAWARE (HANDLE)-1
#endif

#ifndef DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE (HANDLE)-2
#endif

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (HANDLE)-4
#endif

typedef enum _GdkWin32MonitorDpiType { 
  MDT_EFFECTIVE_DPI  = 0,
  MDT_ANGULAR_DPI    = 1,
  MDT_RAW_DPI        = 2,
  MDT_DEFAULT        = MDT_EFFECTIVE_DPI
} GdkWin32MonitorDpiType;

/* APIs from shcore.dll */
typedef HRESULT (WINAPI *funcSetProcessDpiAwareness) (GdkWin32ProcessDpiAwareness value);
typedef HRESULT (WINAPI *funcGetProcessDpiAwareness) (HANDLE handle, GdkWin32ProcessDpiAwareness *awareness);
typedef HRESULT (WINAPI *funcGetDpiForMonitor)       (HMONITOR                monitor,
                                                      GdkWin32MonitorDpiType  dpi_type,
                                                      UINT                   *dpi_x,
                                                      UINT                   *dpi_y);

typedef struct _GdkWin32ShcoreFuncs
{
  HMODULE hshcore;
  funcSetProcessDpiAwareness setDpiAwareFunc;
  funcGetProcessDpiAwareness getDpiAwareFunc;
  funcGetDpiForMonitor getDpiForMonitorFunc;
} GdkWin32ShcoreFuncs;

/* DPI awareness APIs from user32.dll */
typedef BOOL (WINAPI *funcSetProcessDPIAware) (void);
typedef BOOL (WINAPI *funcIsProcessDPIAware)  (void);

/*
 * funcSPDAC is SetProcessDpiAwarenessContext() and
 * funcGTDAC is GetThreadDpiAwarenessContext() and
 * funcADACE is AreDpiAwarenessContextsEqual() provided by user32.dll, on
 * Windows 10 Creator Edition and later.
 * Treat HANDLE as void*, for convenience, since DPI_AWARENESS_CONTEXT is
 * declared using DEFINE_HANDLE.
 */
typedef BOOL (WINAPI *funcSPDAC)  (void *);
typedef HANDLE (WINAPI *funcGTDAC)  (void);
typedef BOOL (WINAPI *funcADACE) (void *, void *);

typedef struct _GdkWin32User32DPIFuncs
{
  funcSetProcessDPIAware setDpiAwareFunc;
  funcIsProcessDPIAware isDpiAwareFunc;
  funcSPDAC setPDAC;
  funcGTDAC getTDAC;
  funcADACE areDACEqual;
} GdkWin32User32DPIFuncs;

typedef enum {
  GDK_WIN32_TABLET_INPUT_API_NONE,
  GDK_WIN32_TABLET_INPUT_API_WINTAB,
  GDK_WIN32_TABLET_INPUT_API_WINPOINTER
} GdkWin32TabletInputAPI;

typedef struct
{
  HWND hwnd;
  HDC hdc;
  HGLRC hglrc;
} GdkWin32GLDummyContextWGL;

/* for Direct Manipulation support */
typedef struct
{
  /* this is an IDirectManipulationManager object */
  void *manager;

  /* GetPointerType (UINT32 pointerId, POINTER_INPUT_TYPE *pointerType) function pointer */
  void *getPointerType;
} dmanip_items;

/* for surface tracking items (modal, HWNDs used, etc) */
typedef struct
{
  GHashTable *handle_ht;
  GSList *modal_surface_stack;
  HWND modal_move_resize_hwnd;

  /* Non-zero while a modal sizing, moving, or dnd operation is in progress */
  GdkWin32ModalOpKind modal_operation_in_progress;
  UINT modal_timer;
} surface_records;

/* for tracking various events that go on */
typedef struct
{
  /* for tracking various mouse/wintab/winpointer events */
  GdkSurface *mouse_surface;
  GdkSurface *mouse_surface_ignored_leave;
  int current_root_x;
  int current_root_y;

  int debug_indent_displaychange;
  int debug_indent_surface_events;

  /* for tracking whether we are using IME */
  guint in_ime_composition : 1;

  /* to store keycodes for shift keys */
  int both_shift_pressed[2];

  /* AeroSnap emulation event handling */
  /* low-level keyboard hook handle */
  HHOOK aerosnap_keyboard_hook;
  UINT aerosnap_message;
} event_records;

struct _GdkWin32Display
{
  GdkDisplay display;

  GdkWin32Screen *screen;

  Win32CursorTheme *cursor_theme;
  char *cursor_theme_name;
  int cursor_theme_size;

  HWND hwnd;

  GListModel *monitors;
  GdkWin32InputLocaleItems *input_locale_items;
  GdkWin32PointerDeviceItems *pointer_device_items;
  GdkWin32CbDnDItems *cb_dnd_items;
  GdkDeviceManagerWin32 *device_manager;
  surface_records *display_surface_record;
  event_records *event_record;

  dmanip_items *dmanip_items;

  /* WGL/OpenGL Items */
  GdkWin32GLDummyContextWGL dummy_context_wgl;
  guint hasWglARBCreateContext : 1;
  guint hasWglEXTSwapControl : 1;
  guint hasWglOMLSyncControl : 1;
  guint hasWglARBPixelFormat : 1;
  guint hasGlWINSwapHint : 1;

  struct {
    guint disallow_swap_exchange : 1;
  } wgl_quirks;

#ifdef HAVE_EGL
  guint hasEglKHRCreateContext : 1;
  guint hasEglSurfacelessContext : 1;
  EGLint egl_min_swap_interval;
#endif

  /* HiDPI Items */
  guint have_at_least_win81 : 1;
  GdkWin32ProcessDpiAwareness dpi_aware_type;
  guint has_fixed_scale : 1;
  guint surface_scale;

  GdkWin32ShcoreFuncs shcore_funcs;
  GdkWin32User32DPIFuncs user32_dpi_funcs;

  GdkWin32TabletInputAPI tablet_input_api;

  /* Cursor Items (GdkCursor->GdkWin32HCursor) */
  GHashTable *cursors;
  /* The cursor that is used by current grab (if any) */
  GdkWin32HCursor *grab_cursor;
  /* HCURSOR -> GdkWin32HCursorTableEntry */
  GHashTable *cursor_reftable;
  /* ID of the idle callback scheduled to destroy cursors */
  guint idle_cursor_destructor_id;

  /* A list of cursor handles slated for destruction. */
  GList *cursors_for_destruction;

  /* Message filters */
  GList *filters;

  /* Running CPU items */
  guint running_on_arm64 : 1;
};

struct _GdkWin32DisplayClass
{
  GdkDisplayClass display_class;
};

void       _gdk_win32_display_init_monitors    (GdkWin32Display *display);

GPtrArray *_gdk_win32_display_get_monitor_list (GdkWin32Display *display);

void        gdk_win32_display_check_composited (GdkWin32Display *display);

guint      gdk_win32_display_get_monitor_scale_factor  (GdkWin32Display *display_win32,
                                                        GdkSurface      *surface,
                                                        HMONITOR         hmonitor);

GdkWin32Clipdrop *gdk_win32_display_get_clipdrop       (GdkDisplay *display);

typedef struct _GdkWin32MessageFilter GdkWin32MessageFilter;

struct _GdkWin32MessageFilter
{
  GdkWin32MessageFilterFunc function;
  gpointer data;
  gboolean removed;
  guint ref_count;
};

