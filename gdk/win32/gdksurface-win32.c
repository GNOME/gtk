/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2001-2011 Hans Breuer
 * Copyright (C) 2007-2009 Cody Russell
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <stdlib.h>

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-win32.h"
#include "gdkenumtypes.h"
#include "gdkwin32.h"
#include "gdkdisplayprivate.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkmonitorprivate.h"
#include "gdkpopupprivate.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkwin32surface.h"
#include "gdkwin32cursor.h"
#include "gdkinput-dmanipulation.h"
#include "gdkinput-winpointer.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkdevice-win32.h"
#include "gdkcairocontext-win32.h"
#include "gdkmonitor-win32.h"

#include <cairo-win32.h>
#include <dwmapi.h>
#include <math.h>

/* forward declarations */
static void gdk_surface_win32_finalize (GObject *object);

static void gdk_win32_toplevel_state_callback   (GdkSurface *surface);
static void gdk_win32_surface_set_transient_for (GdkSurface *surface,
                                                 GdkSurface *parent);
static void gdk_win32_push_modal_surface        (GdkSurface *surface);
static void gdk_win32_remove_modal_surface      (GdkSurface *surface);
static void gdk_win32_impl_frame_clock_after_paint (GdkFrameClock *clock,
                                                    GdkSurface    *surface);

static void gdk_win32_surface_maximize (GdkSurface *surface);
static void gdk_win32_surface_unmaximize (GdkSurface *surface);
static void gdk_win32_surface_minimize (GdkSurface *surface);

typedef struct _FullscreenInfo FullscreenInfo;

struct _FullscreenInfo
{
  RECT  r;
  guint hint_flags;
  LONG  style;
};

G_DEFINE_TYPE (GdkWin32Surface, gdk_win32_surface, GDK_TYPE_SURFACE)

static void
gdk_win32_surface_init (GdkWin32Surface *impl)
{
  impl->hicon_big = NULL;
  impl->hicon_small = NULL;
  impl->hint_flags = 0;
  impl->transient_owner = NULL;
  impl->transient_children = NULL;
  impl->num_transients = 0;
  impl->changing_state = FALSE;
  impl->surface_scale = 1;
}


static void
gdk_surface_win32_dispose (GObject *object)
{
  GdkWin32Surface *surface;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (object));

  surface = GDK_WIN32_SURFACE (object);

  g_clear_object (&surface->cursor);

  G_OBJECT_CLASS (gdk_win32_surface_parent_class)->dispose (object);
}


static void
gdk_surface_win32_finalize (GObject *object)
{
  GdkWin32Surface *surface;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (object));

  surface = GDK_WIN32_SURFACE (object);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      gdk_win32_display_handle_table_remove (gdk_surface_get_display (GDK_SURFACE (surface)),
                                             surface->handle);
    }

  if (surface->hicon_big != NULL)
    {
      GDI_CALL (DestroyIcon, (surface->hicon_big));
      surface->hicon_big = NULL;
    }

  if (surface->hicon_small != NULL)
    {
      GDI_CALL (DestroyIcon, (surface->hicon_small));
      surface->hicon_small = NULL;
    }

  _gdk_win32_surface_unregister_dnd (GDK_SURFACE (surface));

  g_assert (surface->transient_owner == NULL);
  g_assert (surface->transient_children == NULL);

  G_OBJECT_CLASS (gdk_win32_surface_parent_class)->finalize (object);
}

static void
_gdk_win32_get_window_client_area_rect (GdkSurface *surface,
                                        int         scale,
                                        RECT       *rect)
{
  int x, y, width, height;

  gdk_surface_get_geometry (surface, &x, &y, NULL, NULL);
  width = gdk_surface_get_width (surface);
  height = gdk_surface_get_height (surface);
  rect->left = x * scale;
  rect->top = y * scale;
  rect->right = rect->left + width * scale;
  rect->bottom = rect->top + height * scale;
}

static void
gdk_win32_impl_frame_clock_after_paint (GdkFrameClock *clock,
                                        GdkSurface    *surface)
{
  DWM_TIMING_INFO timing_info;
  LARGE_INTEGER tick_frequency;
  GdkFrameTimings *timings;

  timings = gdk_frame_clock_get_timings (clock, gdk_frame_clock_get_frame_counter (clock));

  if (timings)
    {
      timings->refresh_interval = 16667; /* default to 1/60th of a second */
      timings->presentation_time = 0;

      if (QueryPerformanceFrequency (&tick_frequency))
        {
          HRESULT hr;

          timing_info.cbSize = sizeof (timing_info);
          hr = DwmGetCompositionTimingInfo (NULL, &timing_info);

          if (SUCCEEDED (hr))
            {
              timings->refresh_interval = timing_info.qpcRefreshPeriod * (double)G_USEC_PER_SEC / tick_frequency.QuadPart;
              timings->presentation_time = timing_info.qpcCompose * (double)G_USEC_PER_SEC / tick_frequency.QuadPart;
            }
        }

      timings->complete = TRUE;
    }
}

void
_gdk_win32_adjust_client_rect (GdkSurface *surface,
                               RECT       *rect)
{
  LONG style, exstyle;

  style = GetWindowLong (GDK_SURFACE_HWND (surface), GWL_STYLE);
  exstyle = GetWindowLong (GDK_SURFACE_HWND (surface), GWL_EXSTYLE);
  API_CALL (AdjustWindowRectEx, (rect, style, FALSE, exstyle));
}

void
gdk_win32_surface_enable_transparency (GdkSurface *surface)
{
  DWM_BLURBEHIND blur_behind;
  HRGN empty_region;
  HRESULT call_result;
  HWND this_hwnd;
  BOOL dummy;

  if (surface == NULL || GDK_SURFACE_HWND (surface) == NULL)
    return;

  /* happens while dwm.exe is restarting */
  if (FAILED (DwmIsCompositionEnabled (&dummy)))
    return;

  this_hwnd = GDK_SURFACE_HWND (surface);

  empty_region = CreateRectRgn (0, 0, -1, -1);

  if (empty_region == NULL)
    return;

  memset (&blur_behind, 0, sizeof (blur_behind));
  blur_behind.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  blur_behind.hRgnBlur = empty_region;
  blur_behind.fEnable = TRUE;
  call_result = DwmEnableBlurBehindWindow (this_hwnd, &blur_behind);

  if (!SUCCEEDED (call_result))
    g_warning ("%s: %s (%p) failed: %" G_GINT32_MODIFIER "x",
        G_STRLOC, "DwmEnableBlurBehindWindow", this_hwnd, (guint32) call_result);

  DeleteObject (empty_region);
}

static const char *
get_default_title (void)
{
  const char *title;
  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();

  return title;
}

/* RegisterGdkClass
 *   is a wrapper function for RegisterWindowClassEx.
 *   It creates at least one unique class for every
 *   GdkSurfaceType. If support for single window-specific icons
 *   is ever needed (e.g Dialog specific), every such window should
 *   get its own class
 */
static ATOM
RegisterGdkClass (GType wtype)
{
  static ATOM klassTOPLEVEL = 0;
  static ATOM klassTEMP     = 0;
  static HICON hAppIcon = NULL;
  static HICON hAppIconSm = NULL;
  static WNDCLASSEXW wcl;
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEXW);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = _gdk_win32_surface_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = this_module ();
  wcl.hIcon = 0;
  wcl.hIconSm = 0;

  /* initialize once! */
  if (0 == hAppIcon && 0 == hAppIconSm)
    {
      wchar_t sLoc [MAX_PATH+1];

      // try to load first icon of executable program
      if (0 != GetModuleFileName (NULL, sLoc, MAX_PATH))
        {
          ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);

          if (0 == hAppIcon && 0 == hAppIconSm)
            {
              // fallback : load icon from GTK DLL
              if (0 != GetModuleFileName (this_module (), sLoc, MAX_PATH))
		{
		  ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);
		}
            }
        }

      if (0 == hAppIcon && 0 == hAppIconSm)
        {
          hAppIcon = LoadImage (NULL, IDI_APPLICATION, IMAGE_ICON,
                                GetSystemMetrics (SM_CXICON),
                                GetSystemMetrics (SM_CYICON), 0);
          hAppIconSm = LoadImage (NULL, IDI_APPLICATION, IMAGE_ICON,
                                  GetSystemMetrics (SM_CXSMICON),
                                  GetSystemMetrics (SM_CYSMICON), 0);
        }
    }

  if (0 == hAppIcon)
    hAppIcon = hAppIconSm;
  else if (0 == hAppIconSm)
    hAppIconSm = hAppIcon;

  wcl.lpszMenuName = NULL;

  /* initialize once per class */
  /*
   * HB: Setting the background brush leads to flicker, because we
   * don't get asked how to clear the background. This is not what
   * we want, at least not for input_only surfaces ...
   */
#define ONCE_PER_CLASS() \
  wcl.hIcon = CopyIcon (hAppIcon); \
  wcl.hIconSm = CopyIcon (hAppIconSm); \
  wcl.hbrBackground = NULL; \
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);

  if (wtype != GDK_TYPE_WIN32_DRAG_SURFACE)
    {
      if (0 == klassTOPLEVEL)
        {
          wcl.lpszClassName = L"gdkSurfaceToplevel";

          ONCE_PER_CLASS ();
          klassTOPLEVEL = RegisterClassExW (&wcl);
        }
      klass = klassTOPLEVEL;
    }
  else
    {
      if (klassTEMP == 0)
        {
          wcl.lpszClassName = L"gdkSurfaceTemp";
          wcl.style |= CS_SAVEBITS;
          ONCE_PER_CLASS ();
          klassTEMP = RegisterClassExW (&wcl);
        }

      klass = klassTEMP;
    }

  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      g_error ("That is a fatal error");
    }
  return klass;
}

static void
gdk_win32_surface_constructed (GObject *object)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (impl);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkFrameClock *frame_clock;
  IDCompositionDevice *dcomp_device;
  HANDLE owner;
  ATOM klass = 0;
  DWORD dwStyle = 0, dwExStyle;
  RECT rect;
  const char *title;
  wchar_t *wtitle;
  int pos;
  int size;

  impl->surface_scale = gdk_win32_display_get_monitor_scale_factor (display_win32, NULL, NULL);

  dwStyle = 0;
  dwExStyle = 0;
  owner = NULL;

  if (gdk_win32_display_get_dcomp_device (display_win32))
    dwExStyle |= WS_EX_NOREDIRECTIONBITMAP;
  
  if (G_OBJECT_TYPE (impl) == GDK_TYPE_WIN32_TOPLEVEL)
    {
      dwStyle |= WS_OVERLAPPEDWINDOW;
      frame_clock = _gdk_frame_clock_idle_new ();
    }
  else if (G_OBJECT_TYPE (impl) == GDK_TYPE_WIN32_DRAG_SURFACE)
    {
      dwExStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
      dwStyle |= WS_POPUP;
      frame_clock = _gdk_frame_clock_idle_new ();
    }
  else if (G_OBJECT_TYPE (impl) == GDK_TYPE_WIN32_POPUP)
    {
      GdkSurface *parent = gdk_popup_get_parent (GDK_POPUP (impl));
      dwStyle |= WS_POPUP;
      owner = GDK_SURFACE_HWND (parent);
      frame_clock = g_object_ref (gdk_surface_get_frame_clock (parent));
    }
  else
    {
      g_assert_not_reached ();
    }

  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  title = get_default_title ();
  if (!title || !*title)
    title = "";

  klass = RegisterGdkClass (G_OBJECT_TYPE (impl));

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);

  pos = (dwStyle & WS_POPUP) ? 0 : CW_USEDEFAULT;
  size = (dwStyle & WS_POPUP) ? 1 : CW_USEDEFAULT;
  impl->handle = CreateWindowExW (dwExStyle,
                                  MAKEINTRESOURCEW (klass),
                                  wtitle,
                                  dwStyle,
                                  pos, pos,
                                  size, size,
                                  owner,
                                  NULL,
                                  this_module (),
                                  surface);
  if (impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowExW");
      g_error ("Fatal error: CreateWindowExW failed.");
    }

  GetWindowRect (impl->handle, &rect);
  impl->initial_x = rect.left;
  impl->initial_y = rect.top;

  g_object_ref (impl);
  /* Take note: we're inserting a pointer into a heap-allocated
   * object (impl). Inserting a pointer to a stack variable
   * will break the logic, since stack variables are short-lived.
   * We insert a pointer to the handle instead of the handle itself
   * probably because we need to hash them differently depending
   * on the bitness of the OS. That pointer is still unique,
   * so this works out in the end.
   */
  gdk_win32_display_handle_table_insert (display, &GDK_SURFACE_HWND (impl), impl);

  g_free (wtitle);

  if (G_OBJECT_TYPE (impl) != GDK_TYPE_WIN32_DRAG_SURFACE)
    {
      if (display_win32->tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINPOINTER)
        gdk_winpointer_initialize_surface (surface);

      gdk_dmanipulation_initialize_surface (surface);
    }

  dcomp_device = gdk_win32_display_get_dcomp_device (display_win32);
  if (dcomp_device)
    {
      hr_warn (IDCompositionDevice_CreateTargetForHwnd (dcomp_device,
                                                        impl->handle,
                                                        FALSE,
                                                        &impl->dcomp_target));
      hr_warn (IDCompositionDevice_CreateVisual (dcomp_device, &impl->dcomp_visual));
      hr_warn (IDCompositionTarget_SetRoot (impl->dcomp_target, impl->dcomp_visual));
    }

  gdk_win32_surface_enable_transparency (surface);
  _gdk_win32_surface_register_dnd (surface);
  _gdk_win32_surface_update_style_bits (surface);

  g_signal_connect (frame_clock,
                    "after-paint",
                    G_CALLBACK (gdk_win32_impl_frame_clock_after_paint),
                    impl);

  impl->inhibit_configure = TRUE;

  G_OBJECT_CLASS (gdk_win32_surface_parent_class)->constructed (object);
}

static void
gdk_win32_surface_destroy (GdkSurface *surface,
                           gboolean    foreign_destroy)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  g_return_if_fail (GDK_IS_SURFACE (surface));

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_destroy: %p\n",
			   GDK_SURFACE_HWND (surface)));

  /* Remove ourself from the modal stack */
  gdk_win32_remove_modal_surface (surface);

  g_signal_handlers_disconnect_by_func (gdk_surface_get_frame_clock (surface),
                                        gdk_win32_impl_frame_clock_after_paint,
                                        surface);

  /* Remove all our transient children */
  while (impl->transient_children != NULL)
    {
      GdkSurface *child = impl->transient_children->data;
      gdk_win32_surface_set_transient_for (child, NULL);
    }

  /* Remove ourself from our transient owner */
  if (impl->transient_owner != NULL)
    {
      gdk_win32_surface_set_transient_for (surface, NULL);
    }

  if (!foreign_destroy)
    {
      surface->destroyed = TRUE;
      DestroyWindow (GDK_SURFACE_HWND (surface));
    }
}

/* This function is called when the surface really gone.
 */
static void
gdk_win32_surface_destroy_notify (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_surface_destroy_notify: %p%s\n",
		     GDK_SURFACE_HWND (surface),
		     (GDK_SURFACE_DESTROYED (surface) ? " (destroyed)" : "")));

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("surface %p unexpectedly destroyed",
                 GDK_SURFACE_HWND (surface));

      _gdk_surface_destroy (surface, TRUE);
    }

  gdk_win32_display_handle_table_remove (gdk_surface_get_display (surface),
                                         GDK_SURFACE_HWND (surface));
  g_object_unref (surface);
}

static void
get_outer_rect (GdkSurface *surface,
                int         width,
                int         height,
                RECT       *rect)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  rect->left = rect->top = 0;
  rect->right = width * impl->surface_scale;
  rect->bottom = height * impl->surface_scale;

  _gdk_win32_adjust_client_rect (surface, rect);
}

static void
gdk_win32_surface_fullscreen (GdkSurface *surface,
                              GdkMonitor *monitor);

static void
show_surface_internal (GdkSurface *surface,
                       gboolean    already_mapped,
                       gboolean    unminimize)
{
  GdkWin32Surface *impl;
  DWORD exstyle;

  if (surface->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("show_surface_internal: %p: %s%s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state),
			   (unminimize ? " unminimize" : "")));

  /* If asked to show (not unminimize) a withdrawn and iconified
   * surface, do that.
   */
  if (!unminimize &&
      !already_mapped &&
      (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED))
    {
      GtkShowSurfaceHWND (surface, SW_SHOWMINNOACTIVE);
      return;
    }

  /* If asked to just show an iconified surface, do nothing. */
  if (!unminimize && (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED))
    return;

  /* If asked to unminimize an already noniconified surface, do
   * nothing. (Especially, don't cause the surface to rise and
   * activate. There are different calls for that.)
   */
  if (unminimize && !(surface->state & GDK_TOPLEVEL_STATE_MINIMIZED))
    return;

  /* If asked to show (but not raise) a surface that is already
   * visible, do nothing.
   */
  if (!unminimize && !already_mapped && IsWindowVisible (GDK_SURFACE_HWND (surface)))
    return;

  /* For initial map of "normal" surfaces we want to emulate WM surface
   * positioning behaviour, which means:
   * + default to the initial CW_USEDEFAULT placement,
   *   no matter if the user moved the surface before showing it.
   * + Certain surface types and hints have more elaborate positioning
   *   schemes.
   */
  impl = GDK_WIN32_SURFACE (surface);
  if (!already_mapped &&
      GDK_IS_TOPLEVEL (surface))
    {
      gboolean center;
      RECT hwnd_rect, center_on_rect;
      int x, y;

      x = impl->initial_x;
      y = impl->initial_y;

      if (FALSE)
	{
	  HMONITOR monitor;
	  MONITORINFO mi;

	  monitor = MonitorFromWindow (GDK_SURFACE_HWND (surface), MONITOR_DEFAULTTONEAREST);
	  mi.cbSize = sizeof (mi);
	  if (monitor && GetMonitorInfo (monitor, &mi))
	    center_on_rect = mi.rcMonitor;
	  else
	    {
	      center_on_rect.left = 0;
	      center_on_rect.top = 0;
	      center_on_rect.right = GetSystemMetrics (SM_CXSCREEN);
	      center_on_rect.bottom = GetSystemMetrics (SM_CYSCREEN);
	    }
	  center = TRUE;
	}
      else if (impl->transient_owner != NULL &&
	       GDK_SURFACE_IS_MAPPED (impl->transient_owner))
	{
	  GdkSurface *owner = impl->transient_owner;
	  /* Center on transient parent */
          center_on_rect.left = owner->x * impl->surface_scale;
          center_on_rect.top = owner->y * impl->surface_scale;
	  center_on_rect.right = center_on_rect.left + owner->width * impl->surface_scale;
	  center_on_rect.bottom = center_on_rect.top + owner->height * impl->surface_scale;

	  _gdk_win32_adjust_client_rect (GDK_SURFACE (owner), &center_on_rect);
	  center = TRUE;
	}
      else
        {
          center_on_rect = (RECT){ 0, };
          center = FALSE;
        }

      if (center)
	{
	  GetWindowRect (GDK_SURFACE_HWND (surface), &hwnd_rect);

	  x = center_on_rect.left + ((center_on_rect.right - center_on_rect.left) - (hwnd_rect.right - hwnd_rect.left)) / 2;
	  y = center_on_rect.top + ((center_on_rect.bottom - center_on_rect.top) - (hwnd_rect.bottom - hwnd_rect.top)) / 2;
	}

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
			       SWP_NOZORDER_SPECIFIED,
			       x, y, 0, 0,
			       SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }

  if (!already_mapped && GDK_IS_TOPLEVEL (surface))
    {
      /* Ensure new surfaces are fully onscreen */
      RECT hwnd_rect;
      HMONITOR monitor;
      MONITORINFO mi;
      int x, y;

      GetWindowRect (GDK_SURFACE_HWND (surface), &hwnd_rect);

      monitor = MonitorFromWindow (GDK_SURFACE_HWND (surface), MONITOR_DEFAULTTONEAREST);
      mi.cbSize = sizeof (mi);
      if (monitor && GetMonitorInfo (monitor, &mi))
	{
	  x = hwnd_rect.left;
	  y = hwnd_rect.top;

	  if (hwnd_rect.right > mi.rcWork.right)
	    {
	      hwnd_rect.left -= (hwnd_rect.right - mi.rcWork.right);
	      hwnd_rect.right -= (hwnd_rect.right - mi.rcWork.right);
	    }

	  if (hwnd_rect.bottom > mi.rcWork.bottom)
	    {
	      hwnd_rect.top -= (hwnd_rect.bottom - mi.rcWork.bottom);
	      hwnd_rect.bottom -= (hwnd_rect.bottom - mi.rcWork.bottom);
	    }

	  if (hwnd_rect.left < mi.rcWork.left)
	    {
	      hwnd_rect.right += (mi.rcWork.left - hwnd_rect.left);
	      hwnd_rect.left += (mi.rcWork.left - hwnd_rect.left);
	    }

	  if (hwnd_rect.top < mi.rcWork.top)
	    {
	      hwnd_rect.bottom += (mi.rcWork.top - hwnd_rect.top);
	      hwnd_rect.top += (mi.rcWork.top - hwnd_rect.top);
	    }

	  if (x != hwnd_rect.left || y != hwnd_rect.top)
	    API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
				     SWP_NOZORDER_SPECIFIED,
				     hwnd_rect.left, hwnd_rect.top, 0, 0,
				     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
	}
    }


  if (surface->state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    {
      GtkShowSurfaceHWND (surface, SW_MAXIMIZE);
    }
  else if (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED)
    {
      GtkShowSurfaceHWND (surface, SW_RESTORE);
    }
  else if (GDK_IS_DRAG_SURFACE (surface))
    {
      if (!IsWindowVisible (GDK_SURFACE_HWND (surface)))
        GtkShowSurfaceHWND (surface, SW_SHOWNOACTIVATE);
      else
        GtkShowSurfaceHWND (surface, SW_SHOWNA);
    }
  else if (!IsWindowVisible (GDK_SURFACE_HWND (surface)))
    {
      GtkShowSurfaceHWND (surface, SW_SHOWNORMAL);
    }
  else
    {
      GtkShowSurfaceHWND (surface, SW_SHOW);
    }

  exstyle = GetWindowLong (GDK_SURFACE_HWND (surface), GWL_EXSTYLE);

  /* Sync STATE_ABOVE to TOPMOST */
  if (!GDK_IS_DRAG_SURFACE (surface) &&
      (((surface->state & GDK_TOPLEVEL_STATE_ABOVE) &&
	!(exstyle & WS_EX_TOPMOST)) ||
       (!(surface->state & GDK_TOPLEVEL_STATE_ABOVE) &&
	(exstyle & WS_EX_TOPMOST))))
    {
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
			       (surface->state & GDK_TOPLEVEL_STATE_ABOVE)?HWND_TOPMOST:HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }
}

void
gdk_win32_surface_show (GdkSurface *surface,
		       gboolean already_mapped)
{
  show_surface_internal (surface, FALSE, FALSE);
}

static void
gdk_win32_surface_hide (GdkSurface *surface)
{
  if (surface->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_hide: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_surface_set_is_mapped (surface, FALSE);

  _gdk_surface_clear_update_area (surface);

  GtkShowSurfaceHWND (surface, SW_HIDE);
}

static void
gdk_win32_surface_do_move (GdkSurface *surface,
                           int         x,
                           int         y)
{
  RECT outer_rect;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_move: %p: %+d%+d\n",
                           GDK_SURFACE_HWND (surface), x, y));

  if (surface->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  impl = GDK_WIN32_SURFACE (surface);
  get_outer_rect (surface, surface->width, surface->height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,0,0,"
                           "NOACTIVATE|NOSIZE|NOZORDER)\n",
                           GDK_SURFACE_HWND (surface),
                           x * impl->surface_scale,
                           y * impl->surface_scale));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                           SWP_NOZORDER_SPECIFIED,
                           x * impl->surface_scale,
                           y * impl->surface_scale,
                           0, 0,
                           SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
}

void
gdk_win32_surface_resize (GdkSurface *surface,
                          int width,
                          int height)
{
  RECT outer_rect;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_resize: %p: %dx%d\n",
                           GDK_SURFACE_HWND (surface), width, height));

  if (surface->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  get_outer_rect (surface, width, height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,0,0,%ld,%ld,"
                           "NOACTIVATE|NOMOVE|NOZORDER)\n",
                           GDK_SURFACE_HWND (surface),
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                           SWP_NOZORDER_SPECIFIED,
                           0, 0,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top,
                           SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));

  if (!GDK_WIN32_SURFACE (surface)->force_recompute_size)
    gdk_surface_request_layout (surface);
}

static void
gdk_win32_surface_do_move_resize (GdkSurface *surface,
                                  int         x,
                                  int         y,
                                  int         width,
                                  int         height)
{
  RECT outer_rect;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  if (surface->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_move_resize: %p: %dx%d@%+d%+d\n",
                           GDK_SURFACE_HWND (surface),
                           width, height, x, y));

  impl = GDK_WIN32_SURFACE (surface);

  get_outer_rect (surface, width, height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%ld,%ld,"
                           "NOACTIVATE|NOZORDER)\n",
                           GDK_SURFACE_HWND (surface),
                           x * impl->surface_scale,
                           y * impl->surface_scale,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                           SWP_NOZORDER_SPECIFIED,
                           x * impl->surface_scale,
                           y * impl->surface_scale,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top,
                           SWP_NOACTIVATE | SWP_NOZORDER));
}

static void
gdk_win32_surface_move_resize_internal (GdkSurface *surface,
                                        gboolean    with_move,
                                        int         x,
                                        int         y,
                                        int         width,
                                        int         height)
{
  /* We ignore changes to the surface being moved or resized by the
     user, as we don't want to fight the user */
  if (GDK_SURFACE_HWND (surface) == GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_move_resize_hwnd)
    goto out;

  if (with_move && (width < 0 && height < 0))
    {
      gdk_win32_surface_do_move (surface, x, y);
    }
  else
    {
      if (with_move)
        {
          gdk_win32_surface_do_move_resize (surface, x, y, width, height);
        }
      else
        {
          gdk_win32_surface_resize (surface, width, height);
        }
    }

 out:
  gdk_surface_request_layout (surface);
}

void
gdk_win32_surface_move_resize (GdkSurface *surface,
                               int         x,
                               int         y,
                               int         width,
                               int         height)
{
  gdk_win32_surface_move_resize_internal (surface, TRUE, x, y, width, height);
}

void
gdk_win32_surface_move (GdkSurface *surface,
                        int         x,
                        int         y)
{
  gdk_win32_surface_move_resize_internal (surface, TRUE, x, y, -1, -1);
}

static void gdk_win32_surface_set_shadow_width (GdkSurface *surface,
                                                int         left,
                                                int         right,
                                                int         top,
                                                int         bottom);

static void
gdk_win32_surface_layout_popup (GdkSurface     *surface,
                                int             width,
                                int             height,
                                GdkPopupLayout *layout)
{
  GdkMonitor *monitor;
  GdkRectangle bounds;
  GdkRectangle final_rect;
  int x, y;
  int shadow_left, shadow_right, shadow_top, shadow_bottom;

  monitor = gdk_surface_get_layout_monitor (surface, layout,
                                            gdk_win32_monitor_get_workarea);
  if (!monitor)
    {
      GdkDisplay *display = gdk_surface_get_display (surface);
      monitor = gdk_win32_display_get_primary_monitor (display);
    }
  gdk_win32_monitor_get_workarea (monitor, &bounds);

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);

  gdk_win32_surface_set_shadow_width (surface,
                                      shadow_left,
                                      shadow_right,
                                      shadow_top,
                                      shadow_bottom);

  gdk_surface_layout_popup_helper (surface,
                                   width,
                                   height,
                                   shadow_left,
                                   shadow_right,
                                   shadow_top,
                                   shadow_bottom,
                                   monitor,
                                   &bounds,
                                   layout,
                                   &final_rect);

  gdk_surface_get_origin (surface->parent, &x, &y);
  x += final_rect.x;
  y += final_rect.y;

  if (final_rect.width != surface->width ||
      final_rect.height != surface->height)
    {
      gdk_win32_surface_move_resize (surface,
                                     x,
                                     y,
                                     final_rect.width,
                                     final_rect.height);
    }
  else
    gdk_win32_surface_move (surface, x, y);
}

static void
maybe_notify_mapped (GdkSurface *surface)
{
  if (surface->destroyed)
    return;

  if (!GDK_SURFACE_IS_MAPPED (surface))
    {
      gdk_surface_set_is_mapped (surface, TRUE);
      gdk_surface_invalidate_rect (surface, NULL);
    }
}

static void
show_popup (GdkSurface *surface)
{
  gdk_win32_surface_raise (surface);
  maybe_notify_mapped (surface);
  show_surface_internal (surface, FALSE, FALSE);
  gdk_surface_invalidate_rect (surface, NULL);
}

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  show_popup (surface);
}

static gboolean
gdk_win32_surface_present_popup (GdkSurface     *surface,
                                 int             width,
                                 int             height,
                                 GdkPopupLayout *layout)
{
  gdk_win32_surface_layout_popup (surface, width, height, layout);

  if (GDK_SURFACE_IS_MAPPED (surface))
    return TRUE;

  if (surface->autohide)
    {
      gdk_seat_grab (gdk_display_get_default_seat (surface->display),
                     surface,
                     GDK_SEAT_CAPABILITY_ALL,
                     TRUE,
                     NULL, NULL,
                     show_grabbing_popup, NULL);
    }
  else
    {
      show_popup (surface);
    }

  return GDK_SURFACE_IS_MAPPED (surface);
}

void
gdk_win32_surface_raise (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_surface_raise: %p\n",
			       GDK_SURFACE_HWND (surface)));

      if (GDK_IS_DRAG_SURFACE (surface))
        API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface), HWND_TOPMOST,
	                         0, 0, 0, 0,
				 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER));
         
      else if (GDK_IS_POPUP (surface))
        ShowWindow (GDK_SURFACE_HWND (surface), SW_SHOWNOACTIVATE);
      else
        /* Do not wrap this in an API_CALL macro as SetForegroundWindow might
         * fail when for example dragging a surface belonging to a different
         * application at the time of a gtk_window_present() call due to focus
         * stealing prevention. */
        SetForegroundWindow (GDK_SURFACE_HWND (surface));
    }
}

/**
 * gdk_win32_surface_set_urgency_hint:
 * @surface: (type GdkWin32Surface): a native `GdkSurface`.
 * @urgent: if %TRUE, flashes both the window and the taskbar button
 *          continuously.
 *
 * Flashes the specified @surface.
 */
void
gdk_win32_surface_set_urgency_hint (GdkSurface *surface,
                                    gboolean    urgent)
{
  FLASHWINFO flashwinfo;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  flashwinfo.cbSize = sizeof (flashwinfo);
  flashwinfo.hwnd = GDK_SURFACE_HWND (surface);
  if (urgent)
    flashwinfo.dwFlags = FLASHW_ALL | FLASHW_TIMER;
  else
    flashwinfo.dwFlags = FLASHW_STOP;
  flashwinfo.uCount = 0;
  flashwinfo.dwTimeout = 0;

  FlashWindowEx (&flashwinfo);
}

static gboolean
get_effective_surface_decorations (GdkSurface       *surface,
                                  GdkWMDecoration  *decoration)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  *decoration = 0;

  if (!GDK_IS_TOPLEVEL (surface))
    return FALSE;

  /* we want to apply the "no decorations", if decorations are disabled */
  if (!GDK_WIN32_SURFACE (surface)->decorate_all)
    return TRUE;

  if ((impl->hint_flags & GDK_HINT_MIN_SIZE) &&
      (impl->hint_flags & GDK_HINT_MAX_SIZE) &&
      impl->hints.min_width == impl->hints.max_width &&
      impl->hints.min_height == impl->hints.max_height)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MAXIMIZE;

      *decoration |= GDK_DECOR_MINIMIZE;

      return TRUE;
    }
  else
    {
      *decoration = GDK_DECOR_ALL;
      return TRUE;
    }
}

static void
gdk_win32_surface_set_geometry_hints (GdkSurface         *surface,
                                      const GdkGeometry  *geometry,
                                      GdkSurfaceHints     geom_mask)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_geometry_hints: %p\n",
			   GDK_SURFACE_HWND (surface)));

  fi = g_object_get_data (G_OBJECT (surface), "fullscreen-info");
  if (fi)
    fi->hint_flags = geom_mask;
  else
    impl->hint_flags = geom_mask;
  impl->hints = *geometry;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... MIN_SIZE: %dx%d\n",
			       geometry->min_width, geometry->min_height));
    }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... MAX_SIZE: %dx%d\n",
			       geometry->max_width, geometry->max_height));
    }

  _gdk_win32_surface_update_style_bits (surface);
}

static void
gdk_win32_surface_set_title (GdkSurface *surface,
                             const char *title)
{
  wchar_t *wtitle;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";

  GDK_NOTE (MISC, g_print ("gdk_surface_set_title: %p: %s\n",
			   GDK_SURFACE_HWND (surface), title));

  GDK_NOTE (MISC_OR_EVENTS, title = g_strdup_printf ("%p %s", GDK_SURFACE_HWND (surface), title));

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
  API_CALL (SetWindowTextW, (GDK_SURFACE_HWND (surface), wtitle));
  g_free (wtitle);

  GDK_NOTE (MISC_OR_EVENTS, g_free ((char *) title));
}

static void
gdk_win32_surface_set_transient_for (GdkSurface *surface,
                                     GdkSurface *parent)
{
  HWND window_id, parent_id;
  LONG_PTR old_ptr;
  DWORD w32_error;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkWin32Surface *parent_impl = NULL;
  GSList *item;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  window_id = GDK_SURFACE_HWND (surface);
  parent_id = parent != NULL ? GDK_SURFACE_HWND (parent) : NULL;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_transient_for: %p: %p\n", window_id, parent_id));

  if (GDK_SURFACE_DESTROYED (surface) || (parent && GDK_SURFACE_DESTROYED (parent)))
    {
      if (GDK_SURFACE_DESTROYED (surface))
	GDK_NOTE (MISC, g_print ("... destroyed!\n"));
      else
	GDK_NOTE (MISC, g_print ("... owner destroyed!\n"));

      return;
    }

  if (impl->transient_owner == parent)
    return;

  if (GDK_IS_SURFACE (impl->transient_owner))
    {
      GdkWin32Surface *trans_impl = GDK_WIN32_SURFACE (impl->transient_owner);
      item = g_slist_find (trans_impl->transient_children, surface);
      item->data = NULL;
      trans_impl->transient_children = g_slist_delete_link (trans_impl->transient_children, item);
      trans_impl->num_transients--;

      if (!trans_impl->num_transients)
        {
          trans_impl->transient_children = NULL;
        }

      g_object_unref (G_OBJECT (impl->transient_owner));
      g_object_unref (G_OBJECT (surface));

      impl->transient_owner = NULL;
    }

  if (parent)
    {
      parent_impl = GDK_WIN32_SURFACE (parent);

      parent_impl->transient_children = g_slist_append (parent_impl->transient_children, surface);
      g_object_ref (G_OBJECT (surface));
      parent_impl->num_transients++;
      impl->transient_owner = parent;
      g_object_ref (G_OBJECT (parent));
    }

  SetLastError (0);
  old_ptr = GetWindowLongPtr (window_id, GWLP_HWNDPARENT);
  w32_error = GetLastError ();

  /* Don't re-set GWLP_HWNDPARENT to the same value */
  if ((HWND) old_ptr == parent_id && w32_error == NO_ERROR)
    return;

  /* Don't return if it failed, try SetWindowLongPtr() anyway */
  if (old_ptr == 0 && w32_error != NO_ERROR)
    WIN32_API_FAILED ("GetWindowLongPtr");

  /* This changes the *owner* of the surface, despite the misleading
   * name. (Owner and parent are unrelated concepts.) At least that's
   * what people who seem to know what they talk about say on
   * USENET. Search on Google.
   */
  SetLastError (0);
  old_ptr = SetWindowLongPtr (window_id, GWLP_HWNDPARENT, (LONG_PTR) parent_id);
  w32_error = GetLastError ();

  if (old_ptr == 0 && w32_error != NO_ERROR)
    WIN32_API_FAILED ("SetWindowLongPtr");
}

#define MODAL_SURFACE_STACK(s) GDK_WIN32_DISPLAY (gdk_surface_get_display (s))->display_surface_record->modal_surface_stack

static void
gdk_win32_push_modal_surface (GdkSurface *surface)
{
  MODAL_SURFACE_STACK (surface) = g_slist_prepend (MODAL_SURFACE_STACK (surface), surface);
}

static void
gdk_win32_remove_modal_surface (GdkSurface *surface)
{
  GSList *tmp;

  g_return_if_fail (surface != NULL);

  /* It's possible to be NULL here if someone sets the modal hint of the surface
   * to FALSE before a modal surface stack has ever been created. */
  if (MODAL_SURFACE_STACK (surface) == NULL)
    return;

  /* Find the requested surface in the stack and remove it.  Yeah, I realize this
   * means we're not a 'real stack', strictly speaking.  Sue me. :) */
  tmp = g_slist_find (MODAL_SURFACE_STACK (surface), surface);
  if (tmp != NULL)
    {
      MODAL_SURFACE_STACK (surface) = g_slist_delete_link (MODAL_SURFACE_STACK (surface), tmp);
    }
}

gboolean
_gdk_modal_blocked (GdkSurface *surface)
{
  GSList *l;
  gboolean found_any = FALSE;

  for (l = MODAL_SURFACE_STACK (surface); l != NULL; l = l->next)
    {
      GdkSurface *modal = l->data;

      if (modal == surface)
	return FALSE;

      if (GDK_SURFACE_IS_MAPPED (modal))
	found_any = TRUE;
    }

  return found_any;
}

GdkSurface *
_gdk_modal_current (void)
{
  GSList *l;
  GdkDisplay *display = gdk_display_get_default ();

  for (l = GDK_WIN32_DISPLAY (display)->display_surface_record->modal_surface_stack;
       l != NULL;
       l = l->next)
    {
      GdkSurface *modal = l->data;

      if (GDK_SURFACE_IS_MAPPED (modal))
	return modal;
    }

  return NULL;
}

static void
gdk_win32_surface_get_geometry (GdkSurface *surface,
                                int        *x,
                                int        *y,
                                int        *width,
                                int        *height)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
      RECT rect;
      POINT pt;
      GdkSurface *parent;

      if (GDK_IS_TOPLEVEL (surface))
        parent = NULL;
      else if (GDK_IS_POPUP (surface))
        parent = gdk_popup_get_parent (GDK_POPUP (surface));
      else
        parent = NULL;

      API_CALL (GetClientRect, (GDK_SURFACE_HWND (surface), &rect));

      pt.x = rect.left;
      pt.y = rect.top;
      ClientToScreen (GDK_SURFACE_HWND (surface), &pt);
      if (parent)
        ScreenToClient (GDK_SURFACE_HWND (parent), &pt);

      rect.left = pt.x;
      rect.top = pt.y;

      pt.x = rect.right;
      pt.y = rect.bottom;
      ClientToScreen (GDK_SURFACE_HWND (surface), &pt);
      if (parent)
        ScreenToClient (GDK_SURFACE_HWND (parent), &pt);

      rect.right = pt.x;
      rect.bottom = pt.y;

      if (x)
        *x = rect.left / impl->surface_scale;
      if (y)
        *y = rect.top / impl->surface_scale;
      if (width)
        *width = (rect.right - rect.left) / impl->surface_scale;
      if (height)
        *height = (rect.bottom - rect.top) / impl->surface_scale;

      GDK_DEBUG (MISC, "gdk_win32_surface_get_geometry: %p: %ldx%ld@%+ld%+ld\n",
                 GDK_SURFACE_HWND (surface),
                 (rect.right - rect.left) / impl->surface_scale,
                 (rect.bottom - rect.top) / impl->surface_scale,
                 rect.left, rect.top);
    }
}

static void
gdk_win32_surface_get_root_coords (GdkSurface *surface,
                                   int         x,
                                   int         y,
                                   int        *root_x,
                                   int        *root_y)
{
  int tx;
  int ty;
  POINT pt;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  pt.x = x * impl->surface_scale;
  pt.y = y * impl->surface_scale;
  ClientToScreen (GDK_SURFACE_HWND (surface), &pt);
  tx = pt.x;
  ty = pt.y;

  if (root_x)
    *root_x = tx / impl->surface_scale;
  if (root_y)
    *root_y = ty / impl->surface_scale;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_get_root_coords: %p: %+d%+d %+d%+d\n",
			   GDK_SURFACE_HWND (surface),
			   x * impl->surface_scale,
			   y * impl->surface_scale,
			   tx / impl->surface_scale,
			   ty / impl->surface_scale));
}

static gboolean
gdk_surface_win32_get_device_state (GdkSurface      *surface,
                                    GdkDevice       *device,
                                    double          *x,
                                    double          *y,
                                    GdkModifierType *mask)
{
  _gdk_device_win32_query_state (device, surface, NULL, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;

}

static void
update_single_bit (LONG    *style,
                   gboolean all,
		   int      gdk_bit,
		   int      style_bit)
{
  /* all controls the interpretation of gdk_bit -- if all is TRUE,
   * gdk_bit indicates whether style_bit is off; if all is FALSE, gdk
   * bit indicate whether style_bit is on
   */
  if ((!all && gdk_bit) || (all && !gdk_bit))
    *style |= style_bit;
  else
    *style &= ~style_bit;
}

/*
 * Returns TRUE if surface has no decorations.
 * Usually it means CSD windows, because GTK
 * calls gdk_surface_set_decorations (surface, 0);
 */
gboolean
_gdk_win32_surface_lacks_wm_decorations (GdkSurface *surface)
{
  GdkWin32Surface *impl;
  LONG style;
  gboolean has_any_decorations;

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  impl = GDK_WIN32_SURFACE (surface);

  /* This is because GTK calls gdk_surface_set_decorations (surface, 0),
   * even though GdkWMDecoration docs indicate that 0 does NOT mean
   * "no decorations".
   */
  if (!impl->decorate_all)
    return TRUE;

  if (GDK_SURFACE_HWND (surface) == 0)
    return FALSE;

  style = GetWindowLong (GDK_SURFACE_HWND (surface), GWL_STYLE);

  if (style == 0)
    {
      DWORD w32_error = GetLastError ();

      GDK_NOTE (MISC, g_print ("Failed to get style of surface %p (handle %p): %lu\n",
                               surface, GDK_SURFACE_HWND (surface), w32_error));
      return FALSE;
    }

  /* Keep this in sync with _gdk_win32_surface_update_style_bits() */
  /* We don't check what get_effective_surface_decorations()
   * has to say, because it gives suggestions based on
   * various hints, while we want *actual* decorations,
   * or their absence.
   */
  has_any_decorations = FALSE;

  if (style & (WS_BORDER | WS_THICKFRAME | WS_CAPTION |
               WS_SYSMENU | WS_MAXIMIZEBOX))
    has_any_decorations = TRUE;
  else
    GDK_NOTE (MISC, g_print ("Surface %p (handle %p): has no decorations (style %lx)\n",
                             surface, GDK_SURFACE_HWND (surface), style));

  return !has_any_decorations;
}

void
_gdk_win32_surface_update_style_bits (GdkSurface *surface)
{
  GdkWMDecoration decorations;
  LONG old_style, new_style, old_exstyle, new_exstyle;
  gboolean all;
  RECT rect, before, after;
  gboolean was_topmost;
  gboolean will_be_topmost;
  gboolean was_layered;
  gboolean will_be_layered;
  HWND insert_after;
  UINT flags;

  if (surface->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  old_style = GetWindowLong (GDK_SURFACE_HWND (surface), GWL_STYLE);
  old_exstyle = GetWindowLong (GDK_SURFACE_HWND (surface), GWL_EXSTYLE);

  GetClientRect (GDK_SURFACE_HWND (surface), &before);
  after = before;
  AdjustWindowRectEx (&before, old_style, FALSE, old_exstyle);

  was_topmost = (old_exstyle & WS_EX_TOPMOST) ? TRUE : FALSE;
  was_layered = (old_exstyle & WS_EX_LAYERED) ? TRUE : FALSE;
  will_be_topmost = was_topmost;
  will_be_layered = was_layered;

  old_exstyle &= ~WS_EX_TOPMOST;

  new_style = old_style;
  new_exstyle = old_exstyle;

  if (GDK_IS_DRAG_SURFACE (surface))
    {
      new_exstyle |= WS_EX_TOOLWINDOW;

      /* WS_EX_LAYERED | WS_EX_TRANSPARENT makes the drag surface behave
       * in pointer input passthrough mode, so it doesn't interfere with
       * the drag and drop operation.
       */
      new_exstyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
      will_be_topmost = TRUE;
      will_be_layered = TRUE;
    }
  else
    {
      new_exstyle &= ~WS_EX_TOOLWINDOW;
    }

  if (get_effective_surface_decorations (surface, &decorations))
    {
      all = (decorations & GDK_DECOR_ALL);

      /* Keep this in sync with the test in _gdk_win32_surface_lacks_wm_decorations() */
      update_single_bit (&new_style, all, decorations & GDK_DECOR_BORDER, WS_BORDER);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_RESIZEH, WS_THICKFRAME);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_TITLE, WS_CAPTION);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MENU, WS_SYSMENU);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MAXIMIZE, WS_MAXIMIZEBOX);
    }

  if (old_style == new_style && old_exstyle == new_exstyle )
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: no change\n",
			       GDK_SURFACE_HWND (surface)));
      return;
    }

  if (old_style != new_style)
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: STYLE: %s => %s\n",
			       GDK_SURFACE_HWND (surface),
			       _gdk_win32_surface_style_to_string (old_style),
			       _gdk_win32_surface_style_to_string (new_style)));

      SetWindowLong (GDK_SURFACE_HWND (surface), GWL_STYLE, new_style);
    }

  if (old_exstyle != new_exstyle)
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: EXSTYLE: %s => %s\n",
			       GDK_SURFACE_HWND (surface),
			       _gdk_win32_surface_exstyle_to_string (old_exstyle),
			       _gdk_win32_surface_exstyle_to_string (new_exstyle)));

      SetWindowLong (GDK_SURFACE_HWND (surface), GWL_EXSTYLE, new_exstyle);

      if (!was_layered && will_be_layered)
        {
          /* We have to call SetLayeredWindowAttributes when setting the
           * WS_EX_LAYERED style anew, otherwise the surface won't show up
           */
          API_CALL (SetLayeredWindowAttributes, (GDK_SURFACE_HWND (surface), 0, 255, LWA_ALPHA));
        }
    }

  AdjustWindowRectEx (&after, new_style, FALSE, new_exstyle);

  GetWindowRect (GDK_SURFACE_HWND (surface), &rect);
  rect.left += after.left - before.left;
  rect.top += after.top - before.top;
  rect.right += after.right - before.right;
  rect.bottom += after.bottom - before.bottom;

  flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOREPOSITION;

  if (will_be_topmost && !was_topmost)
    {
      insert_after = HWND_TOPMOST;
    }
  else if (was_topmost && !will_be_topmost)
    {
      insert_after = HWND_NOTOPMOST;
    }
  else
    {
      flags |= SWP_NOZORDER;
      insert_after = SWP_NOZORDER_SPECIFIED;
    }

  SetWindowPos (GDK_SURFACE_HWND (surface), insert_after,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		flags);
}

static const char *
get_cursor_name_from_op (GdkW32WindowDragOp op,
                         GdkSurfaceEdge      edge)
{
  switch (op)
    {
    case GDK_WIN32_DRAGOP_MOVE:
      return "move";
    case GDK_WIN32_DRAGOP_RESIZE:
      switch (edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          return "nw-resize";
        case GDK_SURFACE_EDGE_NORTH:
          return "n-resize";
        case GDK_SURFACE_EDGE_NORTH_EAST:
          return "ne-resize";
        case GDK_SURFACE_EDGE_WEST:
          return "w-resize";
        case GDK_SURFACE_EDGE_EAST:
          return "e-resize";
        case GDK_SURFACE_EDGE_SOUTH_WEST:
          return "sw-resize";
        case GDK_SURFACE_EDGE_SOUTH:
          return "s-resize";
        case GDK_SURFACE_EDGE_SOUTH_EAST:
          return "se-resize";
        }
      /* default: warn about unhandled enum values,
       * fallthrough to GDK_WIN32_DRAGOP_NONE case
       */
    case GDK_WIN32_DRAGOP_COUNT:
    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;
    case GDK_WIN32_DRAGOP_NONE:
      return "default";
    /* default: warn about unhandled enum values */
    }
}

static void
setup_drag_move_resize_context (GdkSurface                  *surface,
                                GdkW32DragMoveResizeContext *context,
                                GdkW32WindowDragOp           op,
                                GdkSurfaceEdge               edge,
                                GdkDevice                   *device,
                                int                          button,
                                double                       x,
                                double                       y,
                                guint32                      timestamp)
{
  RECT rect;
  const char *cursor_name;
  GdkSurface *pointer_surface;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  gboolean maximized = gdk_toplevel_get_state (GDK_TOPLEVEL (surface)) & GDK_TOPLEVEL_STATE_MAXIMIZED;
  int root_x, root_y;
  gboolean restore_configure = FALSE;

  gdk_win32_surface_get_root_coords (surface, x, y, &root_x, &root_y);

  /* Before we drag, we need to undo any maximization */
  if (maximized)
    {
      int wx, wy, wwidth, wheight;
      int swx, swy, swwidth, swheight;
      gboolean pointer_outside_of_surface;
      int offsetx, offsety;
      gboolean left_half;

      restore_configure = TRUE;
      gdk_surface_get_geometry (surface, &wx, &wy, &wwidth, &wheight);

      swx = wx;
      swy = wy;
      swwidth = wwidth;
      swheight = wheight;

      /* Subtract surface shadow. We don't want pointer to go outside of
       * the visible surface during drag-move. For drag-resize it's OK.
       * Don't take shadow into account if the surface is maximized -
       * maximized surfaces don't have shadows.
       */
      if (op == GDK_WIN32_DRAGOP_MOVE && !maximized)
        {
          swx += impl->shadow.left;
          swy += impl->shadow.top;
          swwidth -= impl->shadow.left + impl->shadow.right;
          swheight -= impl->shadow.top + impl->shadow.bottom;
        }

      pointer_outside_of_surface = root_x < swx || root_x > swx + swwidth ||
                                  root_y < swy || root_y > swy + swheight;
      /* Calculate the offset of the pointer relative to the surface */
      offsetx = root_x - swx;
      offsety = root_y - swy;

      /* Figure out in which half of the surface the pointer is.
       * The code currently only concerns itself with horizontal
       * dimension (left/right halves).
       * There's no upper/lower half, because usually surface
       * is dragged by its upper half anyway. If that changes, adjust
       * accordingly.
       */
      left_half = (offsetx < swwidth / 2);

      /* Inverse the offset for it to be from the right edge */
      if (!left_half)
        offsetx = swwidth - offsetx;

      GDK_NOTE (MISC, g_print ("Pointer at %d : %d, this is %d : %d relative to the surface's %s\n",
                               root_x, root_y, offsetx, offsety,
                               left_half ? "left half" : "right half"));

      /* Move surface in such a way that on unmaximization the pointer
       * is still pointing at the appropriate half of the surface,
       * with the same offset from the left or right edge. If the new
       * surface size is too small, and adding that offset puts the pointer
       * into the other half or even beyond, move the pointer to the middle.
       */
      if (!pointer_outside_of_surface && maximized)
        {
          WINDOWPLACEMENT placement;
          int unmax_width, unmax_height;
          int shadow_unmax_width, shadow_unmax_height;

          placement.length = sizeof (placement);
          API_CALL (GetWindowPlacement, (GDK_SURFACE_HWND (surface), &placement));

          GDK_NOTE (MISC, g_print ("W32 WM unmaximized surface placement is %ld x %ld @ %ld : %ld\n",
                                   placement.rcNormalPosition.right - placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.bottom - placement.rcNormalPosition.top,
                                   placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.top));

          unmax_width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
          unmax_height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;

          shadow_unmax_width = unmax_width - (impl->shadow.left + impl->shadow.right) * impl->surface_scale;
          shadow_unmax_height = unmax_height - (impl->shadow.top + impl->shadow.bottom) * impl->surface_scale;

          if (offsetx * impl->surface_scale < (shadow_unmax_width / 2) &&
              offsety * impl->surface_scale < (shadow_unmax_height / 2))
            {
              placement.rcNormalPosition.top = (root_y - offsety + impl->shadow.top) * impl->surface_scale;
              placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + unmax_height;

              if (left_half)
                {
                  placement.rcNormalPosition.left = (root_x - offsetx + impl->shadow.left) * impl->surface_scale;
                  placement.rcNormalPosition.right = placement.rcNormalPosition.left + unmax_width;
                }
              else
                {
                  placement.rcNormalPosition.right = (root_x + offsetx + impl->shadow.right) * impl->surface_scale;
                  placement.rcNormalPosition.left = placement.rcNormalPosition.right - unmax_width;
                }
            }
          else
            {
              placement.rcNormalPosition.left = root_x * impl->surface_scale - unmax_width / 2;

              if (offsety * impl->surface_scale < shadow_unmax_height / 2)
                placement.rcNormalPosition.top = (root_y - offsety + impl->shadow.top) * impl->surface_scale;
              else
                placement.rcNormalPosition.top = root_y * impl->surface_scale - unmax_height / 2;

              placement.rcNormalPosition.right = placement.rcNormalPosition.left + unmax_width;
              placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + unmax_height;
            }

          GDK_NOTE (MISC, g_print ("Unmaximized HWND will be at %ld : %ld\n",
                                   placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.top));

          API_CALL (SetWindowPlacement, (GDK_SURFACE_HWND (surface), &placement));
        }

      if (maximized)
        gdk_win32_surface_unmaximize (surface);

      if (pointer_outside_of_surface)
        {
          /* Pointer outside of the surface, move pointer into surface */
          GDK_NOTE (MISC, g_print ("Pointer at %d : %d is outside of %d x %d @ %d : %d, move it to %d : %d\n",
                                   root_x, root_y, wwidth, wheight, wx, wy, wx + wwidth / 2, wy + wheight / 2));
          root_x = wx + wwidth / 2;
          /* This is Gnome behaviour. Windows WM would put the pointer
           * in the middle of the titlebar, but GDK doesn't know where
           * the titlebar is, if any.
           */
          root_y = wy + wheight / 2;
          SetCursorPos (root_x, root_y);
        }
    }

  if (restore_configure)
    impl->inhibit_configure = FALSE;

  gdk_win32_get_surface_hwnd_rect (surface, &rect);

  cursor_name = get_cursor_name_from_op (op, edge);

  context->cursor = gdk_cursor_new_from_name (cursor_name, NULL);

  pointer_surface = surface;

  /* Note: This triggers a WM_CAPTURECHANGED, which will trigger
   * gdk_win32_surface_end_move_resize_drag(), which will end
   * our op before it even begins, but only if context->op is not NONE.
   * This is why we first do the grab, *then* set the op.
   */
  gdk_device_grab (device, pointer_surface,
                   FALSE,
                   GDK_ALL_EVENTS_MASK,
                   context->cursor,
                   timestamp);

  context->surface = g_object_ref (surface);
  context->op = op;
  context->edge = edge;
  context->device = device;
  context->button = button;
  context->start_root_x = root_x;
  context->start_root_y = root_y;
  context->current_root_x = root_x;
  context->current_root_y = root_y;
  context->timestamp = timestamp;
  context->start_rect = rect;

  GDK_NOTE (EVENTS,
            g_print ("begin drag moveresize: surface %p, toplevel %p, "
                     "op %u, edge %d, device %p, "
                     "button %d, coord %d:%d, time %u\n",
                     pointer_surface, surface,
                     context->op, context->edge, context->device,
                     context->button, context->start_root_x,
                     context->start_root_y, context->timestamp));
}

void
gdk_win32_surface_end_move_resize_drag (GdkSurface *surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkW32DragMoveResizeContext *context = &impl->drag_move_resize_context;

  context->op = GDK_WIN32_DRAGOP_NONE;

  gdk_device_ungrab (context->device, GDK_CURRENT_TIME);

  g_clear_object (&context->cursor);

  g_clear_object (&context->surface);

  GDK_NOTE (EVENTS,
            g_print ("end drag moveresize: surface %p, toplevel %p,"
                     "op %u, edge %d, device %p, "
                     "button %d, coord %d:%d, time %u\n",
                     surface, surface,
                     context->op, context->edge, context->device,
                     context->button, context->start_root_x,
                     context->start_root_y, context->timestamp));
}

static void
gdk_win32_get_window_size_and_position_from_client_rect (GdkSurface *surface,
                                                         RECT       *hwnd_rect,
                                                         SIZE       *hwnd_size,
                                                         POINT      *hwnd_position)
{
  /* Turn client area into HWND area */
  _gdk_win32_adjust_client_rect (surface, hwnd_rect);

  hwnd_position->x = hwnd_rect->left;
  hwnd_position->y = hwnd_rect->top;
  hwnd_size->cx = hwnd_rect->right - hwnd_rect->left;
  hwnd_size->cy = hwnd_rect->bottom - hwnd_rect->top;
}

void
gdk_win32_surface_do_move_resize_drag (GdkSurface *surface,
                                       int         x,
                                       int         y)
{
  RECT rect;
  RECT new_rect;
  int diffy, diffx;
  MINMAXINFO mmi;
  GdkWin32Surface *impl;
  GdkW32DragMoveResizeContext *context;
  int width;
  int height;

  impl = GDK_WIN32_SURFACE (surface);
  context = &impl->drag_move_resize_context;

  if (!gdk_win32_get_surface_hwnd_rect (surface, &rect))
    return;

  x /= impl->surface_scale;
  y /= impl->surface_scale;

  if (context->current_root_x == x &&
      context->current_root_y == y)
    return;

  context->current_root_x = x;
  context->current_root_y = y;

  new_rect = context->start_rect;
  diffx = (x - context->start_root_x) * impl->surface_scale;
  diffy = (y - context->start_root_y) * impl->surface_scale;

  switch (context->op)
    {
    case GDK_WIN32_DRAGOP_RESIZE:

      switch (context->edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          new_rect.left += diffx;
          new_rect.top += diffy;
          break;

        case GDK_SURFACE_EDGE_NORTH:
          new_rect.top += diffy;
          break;

        case GDK_SURFACE_EDGE_NORTH_EAST:
          new_rect.right += diffx;
          new_rect.top += diffy;
          break;

        case GDK_SURFACE_EDGE_WEST:
          new_rect.left += diffx;
          break;

        case GDK_SURFACE_EDGE_EAST:
          new_rect.right += diffx;
          break;

        case GDK_SURFACE_EDGE_SOUTH_WEST:
          new_rect.left += diffx;
          new_rect.bottom += diffy;
          break;

        case GDK_SURFACE_EDGE_SOUTH:
          new_rect.bottom += diffy;
          break;

        case GDK_SURFACE_EDGE_SOUTH_EAST:
        default:
          new_rect.right += diffx;
          new_rect.bottom += diffy;
          break;
        }

      /* When handling WM_GETMINMAXINFO, mmi is already populated
       * by W32 WM and we apply our stuff on top of that.
       * Here it isn't, so we should at least clear it.
       */
      memset (&mmi, 0, sizeof (mmi));

      if (!_gdk_win32_surface_fill_min_max_info (surface, &mmi))
        break;

      width = new_rect.right - new_rect.left;
      height = new_rect.bottom - new_rect.top;

      if (width > mmi.ptMaxTrackSize.x)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_WEST:
            case GDK_SURFACE_EDGE_SOUTH_WEST:
              new_rect.left = new_rect.right - mmi.ptMaxTrackSize.x;
              break;

            case GDK_SURFACE_EDGE_NORTH_EAST:
            case GDK_SURFACE_EDGE_EAST:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.right = new_rect.left + mmi.ptMaxTrackSize.x;
              break;
            }
        }
      else if (width < mmi.ptMinTrackSize.x)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_WEST:
            case GDK_SURFACE_EDGE_SOUTH_WEST:
              new_rect.left = new_rect.right - mmi.ptMinTrackSize.x;
              break;

            case GDK_SURFACE_EDGE_NORTH_EAST:
            case GDK_SURFACE_EDGE_EAST:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.right = new_rect.left + mmi.ptMinTrackSize.x;
              break;
            }
        }

      if (height > mmi.ptMaxTrackSize.y)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_NORTH:
            case GDK_SURFACE_EDGE_NORTH_EAST:
              new_rect.top = new_rect.bottom - mmi.ptMaxTrackSize.y;

            case GDK_SURFACE_EDGE_SOUTH_WEST:
            case GDK_SURFACE_EDGE_SOUTH:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.bottom = new_rect.top + mmi.ptMaxTrackSize.y;
              break;
            }
        }
      else if (height < mmi.ptMinTrackSize.y)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_NORTH:
            case GDK_SURFACE_EDGE_NORTH_EAST:
              new_rect.top = new_rect.bottom - mmi.ptMinTrackSize.y;

            case GDK_SURFACE_EDGE_SOUTH_WEST:
            case GDK_SURFACE_EDGE_SOUTH:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.bottom = new_rect.top + mmi.ptMinTrackSize.y;
              break;
            }
        }

      break;
    case GDK_WIN32_DRAGOP_MOVE:
      new_rect.left += diffx;
      new_rect.top += diffy;
      new_rect.right += diffx;
      new_rect.bottom += diffy;
      break;
    default:
      break;
    }

  if (context->op == GDK_WIN32_DRAGOP_RESIZE &&
      (rect.left != new_rect.left ||
       rect.right != new_rect.right ||
       rect.top != new_rect.top ||
       rect.bottom != new_rect.bottom))
    {
      SIZE hwnd_size;
      POINT hwnd_position;

      if (GDK_IS_TOPLEVEL (surface))
        {
          int scale = impl->surface_scale;

          impl->next_layout.configured_rect = new_rect;
          impl->next_layout.configured_width = (new_rect.right - new_rect.left + scale - 1) / scale;
          impl->next_layout.configured_height = (new_rect.bottom - new_rect.top + scale - 1) / scale;
        }
      gdk_win32_get_window_size_and_position_from_client_rect (surface,
                                                              &new_rect,
                                                              &hwnd_size,
                                                              &hwnd_position);

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                               SWP_NOZORDER_SPECIFIED,
                               hwnd_position.x, hwnd_position.y,
                               hwnd_size.cx, hwnd_size.cy,
                               SWP_NOACTIVATE | SWP_NOZORDER));
    }
  else if (context->op == GDK_WIN32_DRAGOP_MOVE &&
           (rect.left != new_rect.left ||
            rect.top != new_rect.top))
    {
      SIZE hwnd_size;
      POINT hwnd_position;

      gdk_win32_get_window_size_and_position_from_client_rect (surface,
                                                               &new_rect,
                                                               &hwnd_size,
                                                               &hwnd_position);

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                               SWP_NOZORDER_SPECIFIED,
                               hwnd_position.x, hwnd_position.y,
                               0, 0,
                               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE));
    }

  gdk_surface_request_layout (surface);
}

static void
gdk_win32_toplevel_begin_resize (GdkToplevel    *toplevel,
                                 GdkSurfaceEdge  edge,
                                 GdkDevice      *device,
                                 int             button,
                                 double          x,
                                 double          y,
                                 guint32         timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWin32Surface *impl;

  if (GDK_SURFACE_DESTROYED (surface) ||
      IsIconic (GDK_SURFACE_HWND (surface)))
    return;

  /* Tell Windows to start interactively resizing the surface by pretending that
   * the left pointer button was clicked in the suitable edge or corner. This
   * will only work if the button is down when this function is called, and
   * will only work with button 1 (left), since Windows only allows surface
   * dragging using the left mouse button.
   */

  if (button != 1)
    return;

  impl = GDK_WIN32_SURFACE (surface);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_end_move_resize_drag (surface);

  setup_drag_move_resize_context (surface, &impl->drag_move_resize_context,
                                  GDK_WIN32_DRAGOP_RESIZE, edge, device,
                                  button, x, y, timestamp);
}

static void
gdk_win32_toplevel_begin_move (GdkToplevel *toplevel,
                               GdkDevice   *device,
                               int          button,
                               double       x,
                               double       y,
                               guint32      timestamp)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWin32Surface *impl;

  if (GDK_SURFACE_DESTROYED (surface) ||
      IsIconic (GDK_SURFACE_HWND (surface)))
    return;

  /* Tell Windows to start interactively moving the surface by pretending that
   * the left pointer button was clicked in the titlebar. This will only work
   * if the button is down when this function is called, and will only work
   * with button 1 (left), since Windows only allows surface dragging using the
   * left mouse button.
   */
  if (button != 1)
    return;

  impl = GDK_WIN32_SURFACE (surface);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_end_move_resize_drag (surface);

  setup_drag_move_resize_context (surface, &impl->drag_move_resize_context,
                                  GDK_WIN32_DRAGOP_MOVE, GDK_SURFACE_EDGE_NORTH_WEST,
                                  device, button, x, y, timestamp);
}


/*
 * Setting surface states
 */
static void
gdk_win32_surface_minimize (GdkSurface *surface)
{
  HWND old_active_hwnd;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_minimize: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  if (GDK_SURFACE_IS_MAPPED (surface))
    {
      old_active_hwnd = GetActiveWindow ();
      GtkShowSurfaceHWND (surface, SW_MINIMIZE);
      if (old_active_hwnd != GDK_SURFACE_HWND (surface))
	SetActiveWindow (old_active_hwnd);
    }
  else
    {
      gdk_synthesize_surface_state (surface,
                                    0,
                                    GDK_TOPLEVEL_STATE_MINIMIZED);
    }
}

static void
gdk_win32_surface_maximize (GdkSurface *surface)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_maximize: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  impl = GDK_WIN32_SURFACE (surface);
  impl->inhibit_configure = TRUE;
  impl->force_recompute_size = FALSE;

  if (GDK_SURFACE_IS_MAPPED (surface))
    GtkShowSurfaceHWND (surface, SW_MAXIMIZE);
  else
    gdk_synthesize_surface_state (surface,
				 0,
				 GDK_TOPLEVEL_STATE_MAXIMIZED);
}

static void
gdk_win32_surface_unmaximize (GdkSurface *surface)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_unmaximize: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  if (GDK_SURFACE_IS_MAPPED (surface))
    GtkShowSurfaceHWND (surface, SW_RESTORE);
  else
    gdk_synthesize_surface_state (surface,
				 GDK_TOPLEVEL_STATE_MAXIMIZED,
				 0);

  impl = GDK_WIN32_SURFACE (surface);

  if (impl->inhibit_configure)
    {
      impl->inhibit_configure = FALSE;
      impl->force_recompute_size = TRUE;
    }
}

static void
gdk_win32_surface_fullscreen (GdkSurface *surface,
                              GdkMonitor *monitor)
{
  int x, y, width, height;
  FullscreenInfo *fi;
  HMONITOR hmonitor =  NULL;
  MONITORINFO mi;
  HWND surface_hwnd;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  surface_hwnd = GDK_SURFACE_HWND (surface);
  fi = g_new (FullscreenInfo, 1);

  if (!GetWindowRect (surface_hwnd, &(fi->r)))
    g_free (fi);
  else
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      if (monitor && GDK_IS_WIN32_MONITOR (monitor))
        hmonitor = GDK_WIN32_MONITOR (monitor)->hmonitor;

      if (!hmonitor)
        hmonitor = MonitorFromWindow (surface_hwnd, MONITOR_DEFAULTTONEAREST);

      mi.cbSize = sizeof (mi);
      if (hmonitor && GetMonitorInfo (hmonitor, &mi))
	{
	  x = mi.rcMonitor.left;
	  y = mi.rcMonitor.top;
	  width = mi.rcMonitor.right - x;
	  height = mi.rcMonitor.bottom - y;
	}
      else
	{
	  x = y = 0;
	  width = GetSystemMetrics (SM_CXSCREEN);
	  height = GetSystemMetrics (SM_CYSCREEN);
	}

      /* remember for restoring */
      fi->hint_flags = impl->hint_flags;
      impl->hint_flags &= ~GDK_HINT_MAX_SIZE;
      g_object_set_data (G_OBJECT (surface), "fullscreen-info", fi);
      fi->style = GetWindowLong (surface_hwnd, GWL_STYLE);

      impl->inhibit_configure = TRUE;
      impl->force_recompute_size = FALSE;

      /* Send state change before configure event */
      gdk_synthesize_surface_state (surface, 0, GDK_TOPLEVEL_STATE_FULLSCREEN);

      SetWindowLong (surface_hwnd, GWL_STYLE,
                     (fi->style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);

      API_CALL (SetWindowPos, (surface_hwnd, HWND_TOP,
                x, y, width, height,
                SWP_NOCOPYBITS | SWP_SHOWWINDOW | SWP_FRAMECHANGED));
    }
}

static void
gdk_win32_surface_unfullscreen (GdkSurface *surface)
{
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  fi = g_object_get_data (G_OBJECT (surface), "fullscreen-info");
  if (fi)
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      gdk_synthesize_surface_state (surface, GDK_TOPLEVEL_STATE_FULLSCREEN, 0);

      impl->hint_flags = fi->hint_flags;
      SetWindowLong (GDK_SURFACE_HWND (surface), GWL_STYLE, fi->style);
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface), HWND_NOTOPMOST,
			       fi->r.left, fi->r.top,
			       fi->r.right - fi->r.left, fi->r.bottom - fi->r.top,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW | SWP_FRAMECHANGED));

      g_object_set_data (G_OBJECT (surface), "fullscreen-info", NULL);
      g_free (fi);
      _gdk_win32_surface_update_style_bits (surface);

      if (impl->inhibit_configure)
        {
          impl->inhibit_configure = FALSE;
          impl->force_recompute_size = TRUE;
        }
    }
}

static void
gdk_win32_surface_focus (GdkSurface *surface,
                         guint32     timestamp)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_focus: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  if (surface->state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    GtkShowSurfaceHWND (surface, SW_SHOWMAXIMIZED);
  else if (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED)
    GtkShowSurfaceHWND (surface, SW_RESTORE);
  else if (!IsWindowVisible (GDK_SURFACE_HWND (surface)))
    GtkShowSurfaceHWND (surface, SW_SHOWNORMAL);
  else
    GtkShowSurfaceHWND (surface, SW_SHOW);

  SetFocus (GDK_SURFACE_HWND (surface));
}

/**
 * gdk_win32_surface_lookup_for_display:
 * @display: a %GdkDisplay
 * @anid: a HWND window handle
 *
 * Returns: (nullable): the %GdkSurface associated with the given @anid, or %NULL.
 */
GdkSurface *
gdk_win32_surface_lookup_for_display (GdkDisplay *display,
                                      HWND        anid)
{
  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  return (GdkSurface*) gdk_win32_display_handle_table_lookup_ (display, anid);
}

/**
 * gdk_win32_surface_is_win32:
 * @surface: a `GdkSurface`
 *
 * Returns: %TRUE if the @surface is a win32 implemented surface.
 *
 * Deprecated: 4.8: Use `GDK_IS_WIN32_SURFACE` instead.
 */
gboolean
gdk_win32_surface_is_win32 (GdkSurface *surface)
{
  return GDK_IS_WIN32_SURFACE (surface);
}

static gboolean
gdk_win32_surface_show_window_menu (GdkSurface *surface,
                                    GdkEvent   *event)
{
  double event_x, event_y;
  int x, y;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkEventType event_type = gdk_event_get_event_type (event);

  switch ((int) event_type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  gdk_event_get_position (event, &event_x, &event_y);
  gdk_win32_surface_get_root_coords (surface, event_x, event_y, &x, &y);

  SendMessage (GDK_SURFACE_HWND (surface),
               WM_SYSMENU,
               0,
               MAKELPARAM (x * impl->surface_scale, y * impl->surface_scale));

  return TRUE;
}

/**
 * gdk_win32_surface_get_impl_hwnd:
 * @surface: a `GdkSurface`
 *
 * Returns: the associated @surface HWND handle.
 *
 * Deprecated: 4.8: Use gdk_win32_surface_get_handle() instead.
 */
HWND
gdk_win32_surface_get_impl_hwnd (GdkSurface *surface)
{
  if (GDK_IS_WIN32_SURFACE (surface))
    return GDK_SURFACE_HWND (surface);
  return NULL;
}

BOOL WINAPI
GtkShowSurfaceHWND (GdkSurface *surface,
                    int         cmd_show)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  /* Ensure that maximized surface HWND size is corrected later on */
  if (cmd_show == SW_MAXIMIZE)
    impl->maximizing = TRUE;

  return ShowWindow (GDK_SURFACE_HWND (surface), cmd_show);
}

static void
gdk_win32_surface_set_shadow_width (GdkSurface *surface,
                                    int         left,
                                    int         right,
                                    int         top,
                                    int         bottom)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_DEBUG (MISC, "gdk_win32_surface_set_shadow_width: surface %p, "
                   "left %d, top %d, right %d, bottom %d\n",
                   surface, left, top, right, bottom);

  impl->shadow.left = left;
  impl->shadow.right = right;
  impl->shadow.top = top;
  impl->shadow.bottom = bottom;
}

static void
gdk_win32_surface_set_icon_list (GdkSurface *surface,
                                 GList      *textures)
{
  GdkTexture *texture, *big_texture, *small_texture;
  gint big_diff, small_diff;
  gint big_w, big_h, small_w, small_h;
  gint w, h;
  gint dw, dh, diff;
  HICON small_hicon, big_hicon;
  GdkWin32Surface *impl;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_WIN32_SURFACE (surface);

  /* ideal sizes for small and large icons */
  big_w = GetSystemMetrics (SM_CXICON);
  big_h = GetSystemMetrics (SM_CYICON);
  small_w = GetSystemMetrics (SM_CXSMICON);
  small_h = GetSystemMetrics (SM_CYSMICON);

  /* find closest sized icons in the list */
  big_texture = NULL;
  small_texture = NULL;
  big_diff = 0;
  small_diff = 0;
  while (textures)
    {
      texture = (GdkTexture*) textures->data;
      w = gdk_texture_get_width (texture);
      h = gdk_texture_get_height (texture);

      dw = ABS (w - big_w);
      dh = ABS (h - big_h);
      diff = dw*dw + dh*dh;
      if (big_texture == NULL || diff < big_diff)
        {
          big_texture = texture;
          big_diff = diff;
        }

      dw = ABS (w - small_w);
      dh = ABS (h - small_h);
      diff = dw*dw + dh*dh;
      if (small_texture == NULL || diff < small_diff)
        {
          small_texture = texture;
          small_diff = diff;
        }

      textures = textures->next;
    }

  if (big_texture == NULL || small_texture == NULL)
    return;

  /* Create the icons */
  big_hicon = big_texture ? _gdk_win32_create_hicon_for_texture (big_texture, TRUE, 0, 0) : NULL;
  small_hicon = small_texture ? _gdk_win32_create_hicon_for_texture (small_texture, TRUE, 0, 0) : NULL;

  /* Set the icons */
  SendMessage (GDK_SURFACE_HWND (surface), WM_SETICON, ICON_BIG,
               (LPARAM)big_hicon);
  SendMessage (GDK_SURFACE_HWND (surface), WM_SETICON, ICON_SMALL,
               (LPARAM)small_hicon);

  /* Store the icons, destroying any previous icons */
  if (impl->hicon_big)
    GDI_CALL (DestroyIcon, (impl->hicon_big));
  impl->hicon_big = big_hicon;
  if (impl->hicon_small)
    GDI_CALL (DestroyIcon, (impl->hicon_small));
  impl->hicon_small = small_hicon;
}

double
_gdk_win32_surface_get_scale (GdkSurface *surface)
{
  GdkDisplay *display;
  GdkWin32Surface *impl;
  GdkWin32Display *win32_display;

  g_return_val_if_fail (surface != NULL, 1);

  display = gdk_surface_get_display (surface);
  impl = GDK_WIN32_SURFACE (surface);

  win32_display = GDK_WIN32_DISPLAY (display);

  if (win32_display->dpi_aware_type != PROCESS_DPI_UNAWARE)
    {
      impl->surface_scale = gdk_win32_display_get_monitor_scale_factor (win32_display,
                                                                        surface,
                                                                        NULL);

      return impl->surface_scale;
    }
  else
    {
      /* Application is not DPI aware, don't bother */
      return 1;
    }
}

static void
gdk_win32_surface_set_input_region (GdkSurface     *surface,
                                    cairo_region_t *input_region)
{
  /* Input region support is implemented by handling the
   * WM_NCHITTEST message. */
}

static void
compute_toplevel_size (GdkSurface *surface,
                       gboolean    update_geometry,
                       int         desired_width,
                       int         desired_height,
                       int        *width,
                       int        *height)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;
  GdkToplevelSize size;
  int bounds_width, bounds_height;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  monitor = gdk_display_get_monitor_at_surface (display, surface);
  if (monitor)
    {
      GdkRectangle workarea;

      gdk_win32_monitor_get_workarea (monitor, &workarea);
      bounds_width = workarea.width;
      bounds_height = workarea.height;
    }
  else
    {
      bounds_width = G_MAXINT;
      bounds_height = G_MAXINT;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (GDK_TOPLEVEL (surface), &size);
  g_warn_if_fail (size.width > 0);
  g_warn_if_fail (size.height > 0);
  if (desired_width > 0)
    *width = MAX (size.min_width, desired_width);
  else
    *width = size.width;
  if (desired_height > 0)
    *height = MAX (size.min_height, desired_height);
  else
    *height = size.height;

  if (size.shadow.is_valid)
    {
      gdk_win32_surface_set_shadow_width (surface,
                                          size.shadow.left,
                                          size.shadow.right,
                                          size.shadow.top,
                                          size.shadow.bottom);
    }

  if (update_geometry)
    {
      GdkGeometry geometry;
      GdkSurfaceHints mask;
      GdkToplevelLayout *layout = impl->toplevel_layout;
      gboolean is_maximized;
      gboolean is_fullscreened;
      GdkRectangle monitor_geometry;

      if (monitor &&
          gdk_toplevel_layout_get_maximized (layout, &is_maximized) &&
          is_maximized)
        {
          geometry.min_width = bounds_width;
          geometry.min_height = bounds_height;
          mask = GDK_HINT_MIN_SIZE;
        }
      else if (monitor &&
               gdk_toplevel_layout_get_fullscreen (layout, &is_fullscreened) &&
               is_fullscreened)
        {
          gdk_monitor_get_geometry (monitor, &monitor_geometry);
          geometry.min_width = monitor_geometry.width;
          geometry.min_height = monitor_geometry.height;
          mask = GDK_HINT_MIN_SIZE;
        }
      else if (gdk_toplevel_layout_get_resizable (layout))
        {
          geometry.min_width = size.min_width;
          geometry.min_height = size.min_height;
          mask = GDK_HINT_MIN_SIZE;
          gdk_win32_surface_set_geometry_hints (surface, &geometry, mask);
        }
      else
        {
          geometry.max_width = geometry.min_width = *width;
          geometry.max_height = geometry.min_height = *height;
          mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
          gdk_win32_surface_set_geometry_hints (surface, &geometry, mask);
        }
      gdk_surface_constrain_size (&geometry, mask, *width, *height, width, height);
    }
}

static void
_gdk_win32_surface_request_layout (GdkSurface *surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  int scale = impl->surface_scale;
  RECT rect;

  gdk_win32_get_surface_hwnd_rect (surface, &rect);

  /* Keep current position if rect is invalid (i.e. queried in bad context) */
  if (rect.right == rect.left || rect.bottom == rect.top)
    return;

  impl->next_layout.configured_width = (rect.right - rect.left + scale - 1) / scale;
  impl->next_layout.configured_height = (rect.bottom - rect.top + scale - 1) / scale;

  if (GDK_IS_TOPLEVEL (surface))
    {
      surface->x = rect.left / scale;
      surface->y = rect.top / scale;
    }
  else if (GDK_IS_POPUP (surface))
    {
      gdk_win32_surface_get_geometry (surface,
                                      &surface->x, &surface->y,
                                      NULL, NULL);
    }
}

static gboolean
gdk_win32_surface_compute_size (GdkSurface *surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  bool size_changed;

  size_changed = surface->width != impl->next_layout.configured_width ||
                  surface->height != impl->next_layout.configured_height;

  surface->width = impl->next_layout.configured_width;
  surface->height = impl->next_layout.configured_height;

  if (size_changed)
    _gdk_surface_update_size (surface);

  return FALSE;
}

static void
gdk_win32_surface_class_init (GdkWin32SurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = gdk_win32_surface_constructed;
  object_class->dispose = gdk_surface_win32_dispose;
  object_class->finalize = gdk_surface_win32_finalize;

  impl_class->hide = gdk_win32_surface_hide;
  impl_class->get_geometry = gdk_win32_surface_get_geometry;
  impl_class->get_device_state = gdk_surface_win32_get_device_state;
  impl_class->get_root_coords = gdk_win32_surface_get_root_coords;

  impl_class->set_input_region = gdk_win32_surface_set_input_region;
  impl_class->destroy = gdk_win32_surface_destroy;

  impl_class->destroy_notify = gdk_win32_surface_destroy_notify;
  impl_class->drag_begin = _gdk_win32_surface_drag_begin;
  impl_class->get_scale = _gdk_win32_surface_get_scale;
  impl_class->request_layout = _gdk_win32_surface_request_layout;
  impl_class->compute_size = gdk_win32_surface_compute_size;
}

/**
 * gdk_win32_surface_get_handle:
 * @surface: (type GdkWin32Surface): a native `GdkSurface`.
 *
 * Returns the HWND handle belonging to @surface.
 *
 * Returns: the associated HWND handle.
 */
HWND
gdk_win32_surface_get_handle (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WIN32_SURFACE (surface), NULL);

  return GDK_SURFACE_HWND (surface);
}

#define LAST_PROP 1

typedef struct
{
  GdkWin32Surface parent_instance;
} GdkWin32Popup;

typedef struct
{
  GdkWin32SurfaceClass parent_class;
} GdkWin32PopupClass;

static void gdk_win32_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWin32Popup, gdk_win32_popup, GDK_TYPE_WIN32_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                gdk_win32_popup_iface_init))

static void
gdk_win32_popup_init (GdkWin32Popup *popup)
{
}

static void
gdk_win32_popup_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_popup_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent != NULL)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_popup_class_init (GdkWin32PopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_win32_popup_get_property;
  object_class->set_property = gdk_win32_popup_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
gdk_win32_popup_present (GdkPopup       *popup,
                         int             width,
                         int             height,
                         GdkPopupLayout *layout)
{
  return gdk_win32_surface_present_popup (GDK_SURFACE (popup), width, height, layout);
}

static GdkGravity
gdk_win32_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_win32_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_win32_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_win32_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_win32_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_win32_popup_present;
  iface->get_surface_anchor = gdk_win32_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_win32_popup_get_rect_anchor;
  iface->get_position_x = gdk_win32_popup_get_position_x;
  iface->get_position_y = gdk_win32_popup_get_position_y;
}


typedef struct
{
  GdkWin32Surface parent_instance;
} GdkWin32Toplevel;

typedef struct
{
  GdkWin32SurfaceClass parent_class;
} GdkWin32ToplevelClass;

static void gdk_win32_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWin32Toplevel, gdk_win32_toplevel, GDK_TYPE_WIN32_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                gdk_win32_toplevel_iface_init))

static void
gdk_win32_toplevel_constructed (GObject *object)
{
  g_signal_connect (object, "notify::state",
                    G_CALLBACK (gdk_win32_toplevel_state_callback),
                    NULL);

  G_OBJECT_CLASS (gdk_win32_toplevel_parent_class)->constructed (object);
}

static void
gdk_win32_toplevel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_win32_surface_set_title (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_win32_surface_set_transient_for (surface, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      GDK_SURFACE (surface)->modal_hint = g_value_get_boolean (value);

      if (GDK_SURFACE (surface)->modal_hint)
        gdk_win32_push_modal_surface (surface);

      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      gdk_win32_surface_set_icon_list (surface, g_value_get_pointer (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      GDK_WIN32_SURFACE (surface)->decorate_all = g_value_get_boolean (value);
      _gdk_win32_surface_update_style_bits (surface);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_GRAVITY:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_toplevel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, surface->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, GDK_SURFACE (surface)->modal_hint);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      g_value_set_boolean (value, GDK_WIN32_SURFACE (surface)->decorate_all);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      g_value_set_enum (value, surface->fullscreen_mode);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_CAPABILITIES:
      g_value_set_flags (value, GDK_TOPLEVEL_CAPABILITIES_INHIBIT_SHORTCUTS |
                                GDK_TOPLEVEL_CAPABILITIES_WINDOW_MENU |
                                GDK_TOPLEVEL_CAPABILITIES_MAXIMIZE |
                                GDK_TOPLEVEL_CAPABILITIES_FULLSCREEN |
                                GDK_TOPLEVEL_CAPABILITIES_MINIMIZE);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_GRAVITY:
      g_value_set_boolean (value, GDK_GRAVITY_NORTH_EAST);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_toplevel_finalize (GObject *object)
{
  GdkWin32Surface *self = GDK_WIN32_SURFACE (object);

  g_signal_handlers_disconnect_by_func (self,
                                        gdk_win32_toplevel_state_callback,
                                        NULL);

  G_OBJECT_CLASS (gdk_win32_toplevel_parent_class)->finalize (object);
}

static gboolean
gdk_win32_toplevel_compute_size (GdkSurface *surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  int width, height;
  bool size_changed, needs_resize;

  compute_toplevel_size (surface,
                         TRUE,
                         impl->next_layout.configured_width,
                         impl->next_layout.configured_height,
                         &width, &height);

  needs_resize = impl->force_recompute_size;
  if (impl->next_layout.configured_width)
    needs_resize |= width != impl->next_layout.configured_width;
  else
    needs_resize |= width != surface->width;
  if (impl->next_layout.configured_height)
    needs_resize |= height != impl->next_layout.configured_height;
  else
    needs_resize |= height != surface->height;

  size_changed = surface->width != width ||
                 surface->height != height;
  surface->width = width;
  surface->height = height;

  if (needs_resize)
    gdk_win32_surface_resize (surface, width, height);

  impl->force_recompute_size = FALSE;
  impl->next_layout.configured_width = 0;
  impl->next_layout.configured_height = 0;

  if (size_changed)
    _gdk_surface_update_size (surface);

  return FALSE;
}

static void
gdk_win32_toplevel_class_init (GdkWin32ToplevelClass *class)
{
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_win32_toplevel_constructed;
  object_class->finalize = gdk_win32_toplevel_finalize;
  object_class->get_property = gdk_win32_toplevel_get_property;
  object_class->set_property = gdk_win32_toplevel_set_property;

  surface_class->compute_size = gdk_win32_toplevel_compute_size;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
gdk_win32_toplevel_init (GdkWin32Toplevel *toplevel)
{
}

static void
gdk_win32_toplevel_present (GdkToplevel       *toplevel,
                            GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  int width, height;
  gboolean maximize;
  gboolean fullscreen;

  g_clear_pointer (&impl->toplevel_layout, gdk_toplevel_layout_unref);
  impl->toplevel_layout = gdk_toplevel_layout_copy (layout);
  compute_toplevel_size (surface, FALSE, 0, 0, &width, &height);
  gdk_win32_surface_resize (surface, width, height);

  if (gdk_toplevel_layout_get_maximized (layout, &maximize) && maximize)
    {
      gdk_win32_surface_maximize (surface);
    }
  else
    {
      gdk_win32_surface_unmaximize (surface);
    }

  if (gdk_toplevel_layout_get_fullscreen (layout, &fullscreen) && fullscreen)
    {
      GdkMonitor *monitor;

      monitor = gdk_toplevel_layout_get_fullscreen_monitor (layout);
      gdk_win32_surface_fullscreen (surface, monitor);
    }
  else
    {
      gdk_win32_surface_unfullscreen (surface);
    }

  gdk_win32_surface_show (surface, FALSE);
  maybe_notify_mapped (surface);
}

static gboolean
gdk_win32_toplevel_minimize (GdkToplevel *toplevel)
{
  gdk_win32_surface_minimize (GDK_SURFACE (toplevel));

  return TRUE;
}

static void
gdk_win32_toplevel_focus (GdkToplevel *toplevel,
                            guint32      timestamp)
{
  gdk_win32_surface_focus (GDK_SURFACE (toplevel), timestamp);
}

static gboolean
gdk_win32_toplevel_show_window_menu (GdkToplevel *toplevel,
                                     GdkEvent    *event)
{
  return gdk_win32_surface_show_window_menu (GDK_SURFACE (toplevel), event);
}

static void
gdk_win32_toplevel_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                             GdkEvent    *gdk_event)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkSeat *gdk_seat;
  GdkGrabStatus status;

  if (surface->shortcuts_inhibited)
    return; /* Already inhibited */

  if (!(surface->state & GDK_TOPLEVEL_STATE_FOCUSED))
    return;

  gdk_seat = gdk_surface_get_seat_from_event (surface, gdk_event);

  if (!(gdk_seat_get_capabilities (gdk_seat) & GDK_SEAT_CAPABILITY_KEYBOARD))
    return;

  status = gdk_seat_grab (gdk_seat, surface, GDK_SEAT_CAPABILITY_KEYBOARD,
                          TRUE, NULL, gdk_event, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    return;

  // TODO: install a WH_KEYBOARD_LL hook to take alt-tab/win etc.

  surface->shortcuts_inhibited = TRUE;
  surface->current_shortcuts_inhibited_seat = gdk_seat;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
gdk_win32_toplevel_restore_system_shortcuts (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkSeat *gdk_seat;

  if (!surface->shortcuts_inhibited)
    return; /* Not inhibited */

  gdk_seat = surface->current_shortcuts_inhibited_seat;
  gdk_seat_ungrab (gdk_seat);
  surface->current_shortcuts_inhibited_seat = NULL;

  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
gdk_win32_toplevel_state_callback (GdkSurface *surface)
{
  if (surface->state & GDK_TOPLEVEL_STATE_FOCUSED)
    return;

  if (surface->shortcuts_inhibited)
    gdk_win32_toplevel_restore_system_shortcuts (GDK_TOPLEVEL (surface));
}

static void
gdk_win32_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_win32_toplevel_present;
  iface->minimize = gdk_win32_toplevel_minimize;
  iface->focus = gdk_win32_toplevel_focus;
  iface->show_window_menu = gdk_win32_toplevel_show_window_menu;
  iface->inhibit_system_shortcuts = gdk_win32_toplevel_inhibit_system_shortcuts;
  iface->restore_system_shortcuts = gdk_win32_toplevel_restore_system_shortcuts;
  iface->begin_resize = gdk_win32_toplevel_begin_resize;
  iface->begin_move = gdk_win32_toplevel_begin_move;
}

typedef struct
{
  GdkWin32Surface parent_instance;
} GdkWin32DragSurface;

typedef struct
{
  GdkWin32SurfaceClass parent_class;
} GdkWin32DragSurfaceClass;

static void gdk_win32_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWin32DragSurface, gdk_win32_drag_surface, GDK_TYPE_WIN32_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                gdk_win32_drag_surface_iface_init))

static void
gdk_win32_drag_surface_init (GdkWin32DragSurface *surface)
{
}

static void
gdk_win32_drag_surface_class_init (GdkWin32DragSurfaceClass *class)
{
}

static gboolean
gdk_win32_drag_surface_present (GdkDragSurface *drag_surface,
                                  int             width,
                                  int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);

  gdk_win32_surface_resize (surface, width, height);  
  gdk_win32_surface_show (surface, FALSE);
  maybe_notify_mapped (surface);

  return TRUE;
}

static void
gdk_win32_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_win32_drag_surface_present;
}

/*<private>
 * gdk_win32_surface_set_dcomp_content:
 * @self: The surface to set the content on
 * @dcomp_content: (nullable): The content to set.
 * 
 * Sets the content to be displayed in the surface.
 * 
 * This function should be called by draw contexts when they are created
 * or destroyed.
 * They set up their preferred method of rendering and then set it using
 * this function. The dcomp_content must be valid content for the
 * [IDCompositionVisual::SetContent()](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-setcontent)
 * function.
 * 
 * The content should be set to NULL again when the draw context gets
 * destroyed.
 * 
 * This function may not be called when Direct Composition is not in use.
 * See gdk_win32_display_get_dcomp_device() for details.
 */
void
gdk_win32_surface_set_dcomp_content (GdkWin32Surface *self,
                                     IUnknown        *dcomp_content)
{
  GdkWin32Display *display;
  IDCompositionDevice *dcomp_device;

  display = GDK_WIN32_DISPLAY (gdk_surface_get_display (GDK_SURFACE (self)));
  dcomp_device = gdk_win32_display_get_dcomp_device (display);
  g_return_if_fail (dcomp_device != NULL);

  hr_warn (IDCompositionVisual_SetContent (self->dcomp_visual, dcomp_content));
  hr_warn (IDCompositionDevice_Commit (dcomp_device));
}

GdkSurface *
gdk_win32_drag_surface_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_WIN32_DRAG_SURFACE,
                       "display", display,
                       NULL);
}
