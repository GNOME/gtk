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
#include "gdksurfaceprivate.h"
#include "gdkprivate-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-win32.h"
#include "gdkenumtypes.h"
#include "gdkwin32.h"
#include "gdkdisplayprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkmonitorprivate.h"
#include "gdkwin32surface.h"
#include "gdkwin32cursor.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"

#include <cairo-win32.h>
#include <dwmapi.h>
#include <math.h>
#include "fallback-c89.c"

static void gdk_surface_win32_finalize (GObject *object);

static gpointer parent_class = NULL;
static GSList *modal_window_stack = NULL;

typedef struct _FullscreenInfo FullscreenInfo;

struct _FullscreenInfo
{
  RECT  r;
  guint hint_flags;
  LONG  style;
};

struct _AeroSnapEdgeRegion
{
  /* The rectangle along the edge of the desktop
   * that allows application of the snap transformation.
   */
  GdkRectangle edge;

  /* A subregion of the "edge". When the pointer hits
   * this region, the transformation is revealed.
   * Usually it is 1-pixel thick and is located at the
   * very edge of the screen. When there's a toolbar
   * at that edge, the "trigger" and the "edge" regions
   * are extended to cover that toolbar.
   */
  GdkRectangle trigger;
};

typedef struct _AeroSnapEdgeRegion AeroSnapEdgeRegion;

/* Size of the regions at the edges of the desktop where
 * snapping can take place (in pixels)
 */
#define AEROSNAP_REGION_THICKNESS (20)
/* Size of the subregions that actually trigger the snapping prompt
 * (in pixels).
 */
#define AEROSNAP_REGION_TRIGGER_THICKNESS (1)

/* The gap between the snap indicator and the edge of the work area
 * (in pixels).
 */
#define AEROSNAP_INDICATOR_EDGE_GAP (10)

/* Width of the outline of the snap indicator
 * (in pixels).
 */
#define AEROSNAP_INDICATOR_LINE_WIDTH (3.0)

/* Corner radius of the snap indicator.
 */
#define AEROSNAP_INDICATOR_CORNER_RADIUS (3.0)

/* The time it takes for snap indicator to expand/shrink
 * from current window size to future position of the
 * snapped window (in microseconds).
 */
#define AEROSNAP_INDICATOR_ANIMATION_DURATION (200 * 1000)

/* Opacity if the snap indicator. */
#define AEROSNAP_INDICATOR_OPACITY (0.5)

/* The interval between snap indicator redraws (in milliseconds).
 * 16 is ~ 1/60 of a second, for ~60 FPS.
 */
#define AEROSNAP_INDICATOR_ANIMATION_TICK (16)

static void     gdk_win32_impl_frame_clock_after_paint (GdkFrameClock *clock,
                                                        GdkSurface    *surface);
static gboolean _gdk_surface_get_functions (GdkSurface         *window,
                                           GdkWMFunction     *functions);

G_DEFINE_TYPE (GdkWin32Surface, gdk_win32_surface, GDK_TYPE_SURFACE)

static void
gdk_win32_surface_init (GdkWin32Surface *impl)
{
  impl->hicon_big = NULL;
  impl->hicon_small = NULL;
  impl->hint_flags = 0;
  impl->type_hint = GDK_SURFACE_TYPE_HINT_NORMAL;
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

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gdk_surface_win32_finalize (GObject *object)
{
  GdkWin32Surface *surface;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (object));

  surface = GDK_WIN32_SURFACE (object);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      gdk_win32_handle_table_remove (surface->handle);
    }

  g_clear_pointer (&surface->snap_stash, g_free);
  g_clear_pointer (&surface->snap_stash_int, g_free);

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

  g_free (surface->decorations);

  if (surface->cache_surface)
    {
      cairo_surface_destroy (surface->cache_surface);
      surface->cache_surface = NULL;
    }

  _gdk_win32_surface_unregister_dnd (GDK_SURFACE (surface));
  g_clear_object (&surface->drop);

  g_assert (surface->transient_owner == NULL);
  g_assert (surface->transient_children == NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
_gdk_win32_get_window_client_area_rect (GdkSurface *window,
                                        gint        scale,
                                        RECT       *rect)
{
  gint x, y, width, height;

  gdk_surface_get_position (window, &x, &y);
  width = gdk_surface_get_width (window);
  height = gdk_surface_get_height (window);
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
              timings->refresh_interval = timing_info.qpcRefreshPeriod * (gdouble)G_USEC_PER_SEC / tick_frequency.QuadPart;
              timings->presentation_time = timing_info.qpcCompose * (gdouble)G_USEC_PER_SEC / tick_frequency.QuadPart;
            }
        }

      timings->complete = TRUE;
    }
}

void
_gdk_win32_adjust_client_rect (GdkSurface *window,
			       RECT      *rect)
{
  LONG style, exstyle;

  style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);
  API_CALL (AdjustWindowRectEx, (rect, style, FALSE, exstyle));
}

gboolean
_gdk_win32_surface_enable_transparency (GdkSurface *window)
{
  GdkWin32Surface *impl;
  DWM_BLURBEHIND blur_behind;
  HRGN empty_region;
  HRESULT call_result;
  HWND thiswindow;

  if (window == NULL || GDK_SURFACE_HWND (window) == NULL)
    return FALSE;

  impl = GDK_WIN32_SURFACE (window);

  /* layered windows don't need blurbehind for transparency */
  if (impl->layered)
    return TRUE;

  if (!gdk_display_is_composited (gdk_surface_get_display (window)))
    return FALSE;

  thiswindow = GDK_SURFACE_HWND (window);

  empty_region = CreateRectRgn (0, 0, -1, -1);

  if (empty_region == NULL)
    return FALSE;

  memset (&blur_behind, 0, sizeof (blur_behind));
  blur_behind.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  blur_behind.hRgnBlur = empty_region;
  blur_behind.fEnable = TRUE;
  call_result = DwmEnableBlurBehindWindow (thiswindow, &blur_behind);

  if (!SUCCEEDED (call_result))
    g_warning ("%s: %s (%p) failed: %" G_GINT32_MODIFIER "x",
        G_STRLOC, "DwmEnableBlurBehindWindow", thiswindow, (guint32) call_result);

  DeleteObject (empty_region);

  return SUCCEEDED (call_result);
}

static const gchar *
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
RegisterGdkClass (GdkSurfaceType wtype, GdkSurfaceTypeHint wtype_hint)
{
  static ATOM klassTOPLEVEL   = 0;
  static ATOM klassTEMP       = 0;
  static ATOM klassTEMPSHADOW = 0;
  static HICON hAppIcon = NULL;
  static HICON hAppIconSm = NULL;
  static WNDCLASSEXW wcl;
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = _gdk_win32_surface_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = _gdk_dll_hinstance;
  wcl.hIcon = 0;
  wcl.hIconSm = 0;

  /* initialize once! */
  if (0 == hAppIcon && 0 == hAppIconSm)
    {
      gchar sLoc [MAX_PATH+1];

      if (0 != GetModuleFileName (_gdk_dll_hinstance, sLoc, MAX_PATH))
        {
          ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);

          if (0 == hAppIcon && 0 == hAppIconSm)
            {
              if (0 != GetModuleFileName (_gdk_dll_hinstance, sLoc, MAX_PATH))
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
   * we want, at least not for input_only windows ...
   */
#define ONCE_PER_CLASS() \
  wcl.hIcon = CopyIcon (hAppIcon); \
  wcl.hIconSm = CopyIcon (hAppIconSm); \
  wcl.hbrBackground = NULL; \
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);

  switch (wtype)
    {
    case GDK_SURFACE_TOPLEVEL:
    case GDK_SURFACE_POPUP:
      /* MSDN: CS_OWNDC is needed for OpenGL contexts */
      wcl.style |= CS_OWNDC;
      if (0 == klassTOPLEVEL)
        {
          wcl.lpszClassName = L"gdkSurfaceToplevel";

          ONCE_PER_CLASS ();
          klassTOPLEVEL = RegisterClassExW (&wcl);
        }
      klass = klassTOPLEVEL;
      break;

    case GDK_SURFACE_TEMP:
      if ((wtype_hint == GDK_SURFACE_TYPE_HINT_MENU) ||
          (wtype_hint == GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU) ||
          (wtype_hint == GDK_SURFACE_TYPE_HINT_POPUP_MENU))
        {
          if (klassTEMPSHADOW == 0)
            {
              wcl.lpszClassName = L"gdkSurfaceTempShadow";
              wcl.style |= CS_SAVEBITS;
              wcl.style |= 0x00020000; /* CS_DROPSHADOW */

              ONCE_PER_CLASS ();
              klassTEMPSHADOW = RegisterClassExW (&wcl);
            }

          klass = klassTEMPSHADOW;
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
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      g_error ("That is a fatal error");
    }
  return klass;
}

/*
 * Create native windows.
 *
 * With the default Gdk the created windows are mostly toplevel windows.
 *
 * Placement of the window is derived from the passed in window,
 * except for toplevel window where OS/Window Manager placement
 * is used.
 *
 * [1] http://mail.gnome.org/archives/gtk-devel-list/2010-August/msg00214.html
 */
GdkSurface *
_gdk_win32_display_create_surface (GdkDisplay     *display,
                                   GdkSurfaceType  surface_type,
                                   GdkSurface     *parent,
                                   int             x,
                                   int             y,
                                   int             width,
                                   int             height)
{
  HWND hwndNew;
  HANDLE owner;
  ATOM klass = 0;
  DWORD dwStyle = 0, dwExStyle;
  RECT rect;
  GdkWin32Surface *impl;
  GdkWin32Display *display_win32;
  GdkSurface *surface;
  const gchar *title;
  wchar_t *wtitle;
  gint window_width, window_height;
  gint window_x, window_y;
  gint offset_x = 0, offset_y = 0;
  gint real_x = 0, real_y = 0;
  GdkFrameClock *frame_clock;

  g_return_val_if_fail (display == _gdk_display, NULL);

  GDK_NOTE (MISC,
            g_print ("_gdk_surface_new: %s\n", (surface_type == GDK_SURFACE_TOPLEVEL ? "TOPLEVEL" :
                                                       (surface_type == GDK_SURFACE_TEMP ? "TEMP" :
                                                      (surface_type == GDK_SURFACE_TEMP ? "POPUP" : "???")))));

  display_win32 = GDK_WIN32_DISPLAY (display);

  if (parent)
    frame_clock = g_object_ref (gdk_surface_get_frame_clock (parent));
  else
    frame_clock = _gdk_frame_clock_idle_new ();

  impl = g_object_new (GDK_TYPE_WIN32_SURFACE,
                       "surface-type", surface_type,
                       "display", display,
                       "parent", parent,
                       "frame-clock", frame_clock,
                       NULL);

  surface = GDK_SURFACE (impl);
  surface->x = x;
  surface->y = y;
  surface->width = width;
  surface->height = height;

  impl->layered = FALSE;
  impl->layered_opacity = 1.0;

  impl->surface_scale = _gdk_win32_display_get_monitor_scale_factor (display_win32, NULL, NULL, NULL);
  impl->unscaled_width = width * impl->surface_scale;
  impl->unscaled_height = height * impl->surface_scale;

  dwExStyle = 0;
  owner = NULL;

  offset_x = _gdk_offset_x;
  offset_y = _gdk_offset_y;
  /* MSDN: We need WS_CLIPCHILDREN and WS_CLIPSIBLINGS for GL Context Creation */
  dwStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

  switch (surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
      dwStyle |= WS_OVERLAPPEDWINDOW;
      break;

    case GDK_SURFACE_TEMP:
      dwExStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
      /* fall through */
    case GDK_SURFACE_POPUP:
      dwStyle |= WS_POPUP;

      /* Only popup and temp windows are fit to use the Owner Window mechanism */
      if (parent != NULL)
        owner = GDK_SURFACE_HWND (parent);
      break;

    default:
      g_assert_not_reached ();
    }

  rect.left = x * impl->surface_scale;
  rect.top = y * impl->surface_scale;
  rect.right = rect.left + width * impl->surface_scale;
  rect.bottom = rect.top + height * impl->surface_scale;

  AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);

  real_x = (x - offset_x) * impl->surface_scale;
  real_y = (y - offset_y) * impl->surface_scale;

  if (surface_type == GDK_SURFACE_TOPLEVEL)
    {
      /* We initially place it at default so that we can get the
         default window positioning if we want */
      window_x = window_y = CW_USEDEFAULT;
    }
  else
    {
      /* TEMP: Put these where requested */
      window_x = real_x;
      window_y = real_y;
    }

  window_width = rect.right - rect.left;
  window_height = rect.bottom - rect.top;

  title = get_default_title ();
  if (!title || !*title)
    title = "";

  if (impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY)
    dwExStyle |= WS_EX_TOOLWINDOW;

  /* WS_EX_TRANSPARENT means "try draw this window last, and ignore input".
   * It's the last part we're after. We don't want DND indicator to accept
   * input, because that will make it a potential drop target, and if it's
   * under the mouse cursor, this will kill any DND.
   */
  if (impl->type_hint == GDK_SURFACE_TYPE_HINT_DND)
    dwExStyle |= WS_EX_TRANSPARENT;

  klass = RegisterGdkClass (surface_type, impl->type_hint);

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);

  hwndNew = CreateWindowExW (dwExStyle,
			     MAKEINTRESOURCEW (klass),
			     wtitle,
			     dwStyle,
			     window_x, window_y,
			     window_width, window_height,
			     owner,
			     NULL,
			     _gdk_dll_hinstance,
			     surface);
  impl->handle = hwndNew;

  GetWindowRect (hwndNew, &rect);
  impl->initial_x = rect.left;
  impl->initial_y = rect.top;

  /* Now we know the initial position, move to actually specified position */
  if (real_x != window_x || real_y != window_y)
    {
      API_CALL (SetWindowPos, (hwndNew,
                SWP_NOZORDER_SPECIFIED,
                real_x, real_y, 0, 0,
                SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }

  g_object_ref (impl);
  /* Take note: we're inserting a pointer into a heap-allocated
   * object (impl). Inserting a pointer to a stack variable
   * will break the logic, since stack variables are short-lived.
   * We insert a pointer to the handle instead of the handle itself
   * probably because we need to hash them differently depending
   * on the bitness of the OS. That pointer is still unique,
   * so this works out in the end.
   */
  gdk_win32_handle_table_insert (&GDK_SURFACE_HWND (impl), impl);

  GDK_NOTE (MISC, g_print ("... \"%s\" %dx%d@%+d%+d %p = %p\n",
			   title,
			   window_width, window_height,
			   surface->x - offset_x,
			   surface->y - offset_y,
			   owner,
			   hwndNew));

  g_free (wtitle);

  if (impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowExW");
      g_object_unref (impl);
      return NULL;
    }

  _gdk_win32_surface_enable_transparency (surface);

  g_signal_connect (frame_clock,
                    "after-paint",
                    G_CALLBACK (gdk_win32_impl_frame_clock_after_paint),
                    impl);

  g_object_unref (frame_clock);

  return surface;
}

static void
gdk_win32_surface_destroy (GdkSurface *window,
			   gboolean   foreign_destroy)
{
  GdkWin32Surface *surface = GDK_WIN32_SURFACE (window);

  g_return_if_fail (GDK_IS_SURFACE (window));

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_destroy: %p\n",
			   GDK_SURFACE_HWND (window)));

  /* Remove ourself from the modal stack */
  _gdk_remove_modal_window (window);

  g_signal_handlers_disconnect_by_func (gdk_surface_get_frame_clock (window),
                                        gdk_win32_impl_frame_clock_after_paint,
                                        window);

  /* Remove all our transient children */
  while (surface->transient_children != NULL)
    {
      GdkSurface *child = surface->transient_children->data;
      gdk_surface_set_transient_for (child, NULL);
    }

  /* Remove ourself from our transient owner */
  if (surface->transient_owner != NULL)
    {
      gdk_surface_set_transient_for (window, NULL);
    }

  if (!foreign_destroy)
    {
      window->destroyed = TRUE;
      DestroyWindow (GDK_SURFACE_HWND (window));
    }
}

/* This function is called when the window really gone.
 */
static void
gdk_win32_surface_destroy_notify (GdkSurface *window)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_surface_destroy_notify: %p%s\n",
		     GDK_SURFACE_HWND (window),
		     (GDK_SURFACE_DESTROYED (window) ? " (destroyed)" : "")));

  if (!GDK_SURFACE_DESTROYED (window))
    {
      g_warning ("window %p unexpectedly destroyed",
                 GDK_SURFACE_HWND (window));

      _gdk_surface_destroy (window, TRUE);
    }

  gdk_win32_handle_table_remove (GDK_SURFACE_HWND (window));
  g_object_unref (window);
}

static void
get_outer_rect (GdkSurface *window,
		gint       width,
		gint       height,
		RECT      *rect)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  rect->left = rect->top = 0;
  rect->right = width * impl->surface_scale;
  rect->bottom = height * impl->surface_scale;

  _gdk_win32_adjust_client_rect (window, rect);
}

static void
adjust_for_gravity_hints (GdkSurface *window,
			  RECT      *outer_rect,
			  gint		*x,
			  gint		*y)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (impl->hint_flags & GDK_HINT_WIN_GRAVITY)
    {
#ifdef G_ENABLE_DEBUG
      gint orig_x = *x, orig_y = *y;
#endif

      switch (impl->hints.win_gravity)
	{
	case GDK_GRAVITY_NORTH:
	case GDK_GRAVITY_CENTER:
	case GDK_GRAVITY_SOUTH:
	  *x -= (outer_rect->right - outer_rect->left / 2) / impl->surface_scale;
	  *x += window->width / 2;
	  break;

	case GDK_GRAVITY_SOUTH_EAST:
	case GDK_GRAVITY_EAST:
	case GDK_GRAVITY_NORTH_EAST:
	  *x -= (outer_rect->right - outer_rect->left) / impl->surface_scale;
	  *x += window->width;
	  break;

	case GDK_GRAVITY_STATIC:
	  *x += outer_rect->left / impl->surface_scale;
	  break;

	default:
	  break;
	}

      switch (impl->hints.win_gravity)
	{
	case GDK_GRAVITY_WEST:
	case GDK_GRAVITY_CENTER:
	case GDK_GRAVITY_EAST:
	  *y -= ((outer_rect->bottom - outer_rect->top) / 2) / impl->surface_scale;
	  *y += window->height / 2;
	  break;

	case GDK_GRAVITY_SOUTH_WEST:
	case GDK_GRAVITY_SOUTH:
	case GDK_GRAVITY_SOUTH_EAST:
	  *y -= (outer_rect->bottom - outer_rect->top) / impl->surface_scale;
	  *y += window->height;
	  break;

	case GDK_GRAVITY_STATIC:
	  *y += outer_rect->top * impl->surface_scale;
	  break;

	default:
	  break;
	}
      GDK_NOTE (MISC,
		(orig_x != *x || orig_y != *y) ?
		g_print ("adjust_for_gravity_hints: x: %d->%d, y: %d->%d\n",
			 orig_x, *x, orig_y, *y)
		  : (void) 0);
    }
}

static void
show_window_internal (GdkSurface *window,
                      gboolean   already_mapped,
		      gboolean   deiconify)
{
  GdkWin32Surface *surface;
  gboolean focus_on_map = FALSE;
  DWORD exstyle;

  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("show_window_internal: %p: %s%s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state),
			   (deiconify ? " deiconify" : "")));

  /* If asked to show (not deiconify) an withdrawn and iconified
   * window, do that.
   */
  if (!deiconify &&
      !already_mapped &&
      (window->state & GDK_SURFACE_STATE_ICONIFIED))
    {
      GtkShowWindow (window, SW_SHOWMINNOACTIVE);
      return;
    }

  /* If asked to just show an iconified window, do nothing. */
  if (!deiconify && (window->state & GDK_SURFACE_STATE_ICONIFIED))
    return;

  /* If asked to deiconify an already noniconified window, do
   * nothing. (Especially, don't cause the window to rise and
   * activate. There are different calls for that.)
   */
  if (deiconify && !(window->state & GDK_SURFACE_STATE_ICONIFIED))
    return;

  /* If asked to show (but not raise) a window that is already
   * visible, do nothing.
   */
  if (!deiconify && !already_mapped && IsWindowVisible (GDK_SURFACE_HWND (window)))
    return;

  /* Other cases */

  if (!already_mapped)
    focus_on_map = window->focus_on_map;

  exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);

  /* Use SetWindowPos to show transparent windows so automatic redraws
   * in other windows can be suppressed.
   */
  if (exstyle & WS_EX_TRANSPARENT)
    {
      UINT flags = SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER;

      if (GDK_SURFACE_TYPE (window) == GDK_SURFACE_TEMP || !focus_on_map)
	flags |= SWP_NOACTIVATE;

      SetWindowPos (GDK_SURFACE_HWND (window),
		    SWP_NOZORDER_SPECIFIED, 0, 0, 0, 0, flags);

      return;
    }

  /* For initial map of "normal" windows we want to emulate WM window
   * positioning behaviour, which means:
   * + Use user specified position if GDK_HINT_POS or GDK_HINT_USER_POS
   * otherwise:
   * + default to the initial CW_USEDEFAULT placement,
   *   no matter if the user moved the window before showing it.
   * + Certain window types and hints have more elaborate positioning
   *   schemes.
   */
  surface = GDK_WIN32_SURFACE (window);
  if (!already_mapped &&
      GDK_SURFACE_TYPE (window) == GDK_SURFACE_TOPLEVEL &&
      (surface->hint_flags & (GDK_HINT_POS | GDK_HINT_USER_POS)) == 0)
    {
      gboolean center = FALSE;
      RECT window_rect, center_on_rect;
      int x, y;

      x = surface->initial_x;
      y = surface->initial_y;

      if (surface->type_hint == GDK_SURFACE_TYPE_HINT_SPLASHSCREEN)
	{
	  HMONITOR monitor;
	  MONITORINFO mi;

	  monitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
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
      else if (surface->transient_owner != NULL &&
	       GDK_SURFACE_IS_MAPPED (surface->transient_owner))
	{
	  GdkSurface *owner = surface->transient_owner;
	  /* Center on transient parent */
	  center_on_rect.left = (owner->x - _gdk_offset_x) * surface->surface_scale;
	  center_on_rect.top = (owner->y - _gdk_offset_y) * surface->surface_scale;
	  center_on_rect.right = center_on_rect.left + owner->width * surface->surface_scale;
	  center_on_rect.bottom = center_on_rect.top + owner->height * surface->surface_scale;

	  _gdk_win32_adjust_client_rect (GDK_SURFACE (owner), &center_on_rect);
	  center = TRUE;
	}

      if (center)
	{
	  window_rect.left = 0;
	  window_rect.top = 0;
	  window_rect.right = window->width * surface->surface_scale;
	  window_rect.bottom = window->height * surface->surface_scale;
	  _gdk_win32_adjust_client_rect (window, &window_rect);

	  x = center_on_rect.left + ((center_on_rect.right - center_on_rect.left) - (window_rect.right - window_rect.left)) / 2;
	  y = center_on_rect.top + ((center_on_rect.bottom - center_on_rect.top) - (window_rect.bottom - window_rect.top)) / 2;
	}

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			       SWP_NOZORDER_SPECIFIED,
			       x, y, 0, 0,
			       SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }

  if (!already_mapped &&
      GDK_SURFACE_TYPE (window) == GDK_SURFACE_TOPLEVEL)
    {
      /* Ensure new windows are fully onscreen */
      RECT window_rect;
      HMONITOR monitor;
      MONITORINFO mi;
      int x, y;

      GetWindowRect (GDK_SURFACE_HWND (window), &window_rect);

      monitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
      mi.cbSize = sizeof (mi);
      if (monitor && GetMonitorInfo (monitor, &mi))
	{
	  x = window_rect.left;
	  y = window_rect.top;

	  if (window_rect.right > mi.rcWork.right)
	    {
	      window_rect.left -= (window_rect.right - mi.rcWork.right);
	      window_rect.right -= (window_rect.right - mi.rcWork.right);
	    }

	  if (window_rect.bottom > mi.rcWork.bottom)
	    {
	      window_rect.top -= (window_rect.bottom - mi.rcWork.bottom);
	      window_rect.bottom -= (window_rect.bottom - mi.rcWork.bottom);
	    }

	  if (window_rect.left < mi.rcWork.left)
	    {
	      window_rect.right += (mi.rcWork.left - window_rect.left);
	      window_rect.left += (mi.rcWork.left - window_rect.left);
	    }

	  if (window_rect.top < mi.rcWork.top)
	    {
	      window_rect.bottom += (mi.rcWork.top - window_rect.top);
	      window_rect.top += (mi.rcWork.top - window_rect.top);
	    }

	  if (x != window_rect.left || y != window_rect.top)
	    API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
				     SWP_NOZORDER_SPECIFIED,
				     window_rect.left, window_rect.top, 0, 0,
				     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
	}
    }


  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    {
      gdk_surface_fullscreen (window);
    }
  else if (window->state & GDK_SURFACE_STATE_MAXIMIZED)
    {
      GtkShowWindow (window, SW_MAXIMIZE);
    }
  else if (window->state & GDK_SURFACE_STATE_ICONIFIED)
    {
      if (focus_on_map)
        GtkShowWindow (window, SW_RESTORE);
      else
        GtkShowWindow (window, SW_SHOWNOACTIVATE);
    }
  else if (GDK_SURFACE_TYPE (window) == GDK_SURFACE_TEMP || !focus_on_map)
    {
      if (!IsWindowVisible (GDK_SURFACE_HWND (window)))
        GtkShowWindow (window, SW_SHOWNOACTIVATE);
      else
        GtkShowWindow (window, SW_SHOWNA);
    }
  else if (!IsWindowVisible (GDK_SURFACE_HWND (window)))
    {
      GtkShowWindow (window, SW_SHOWNORMAL);
    }
  else
    {
      GtkShowWindow (window, SW_SHOW);
    }

  /* Sync STATE_ABOVE to TOPMOST */
  if (GDK_SURFACE_TYPE (window) != GDK_SURFACE_TEMP &&
      (((window->state & GDK_SURFACE_STATE_ABOVE) &&
	!(exstyle & WS_EX_TOPMOST)) ||
       (!(window->state & GDK_SURFACE_STATE_ABOVE) &&
	(exstyle & WS_EX_TOPMOST))))
    {
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			       (window->state & GDK_SURFACE_STATE_ABOVE)?HWND_TOPMOST:HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER));
    }
}

static void
gdk_win32_surface_show (GdkSurface *window,
		       gboolean already_mapped)
{
  show_window_internal (window, FALSE, FALSE);
}

static void
gdk_win32_surface_hide (GdkSurface *window)
{
  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_hide: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    gdk_synthesize_surface_state (window,
				 0,
				 GDK_SURFACE_STATE_WITHDRAWN);

  _gdk_surface_clear_update_area (window);

  if (GDK_SURFACE_TYPE (window) == GDK_SURFACE_TOPLEVEL)
    ShowOwnedPopups (GDK_SURFACE_HWND (window), FALSE);

  /* Use SetWindowPos to hide transparent windows so automatic redraws
   * in other windows can be suppressed.
   */
  if (GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
    {
      SetWindowPos (GDK_SURFACE_HWND (window), SWP_NOZORDER_SPECIFIED,
		    0, 0, 0, 0,
		    SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
    }
  else
    {
      GtkShowWindow (window, SW_HIDE);
    }
}

static void
gdk_win32_surface_withdraw (GdkSurface *window)
{
  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_withdraw: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  gdk_surface_hide (window);	/* ??? */
}

static void
gdk_win32_surface_move (GdkSurface *window,
		       gint x, gint y)
{
  RECT outer_rect;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_move: %p: %+d%+d\n",
                           GDK_SURFACE_HWND (window), x, y));

  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    return;

  impl = GDK_WIN32_SURFACE (window);
  get_outer_rect (window, window->width, window->height, &outer_rect);

  adjust_for_gravity_hints (window, &outer_rect, &x, &y);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,0,0,"
                           "NOACTIVATE|NOSIZE|NOZORDER)\n",
                           GDK_SURFACE_HWND (window),
                           (x - _gdk_offset_x) * impl->surface_scale,
                           (y - _gdk_offset_y) * impl->surface_scale));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                           SWP_NOZORDER_SPECIFIED,
                           (x - _gdk_offset_x) * impl->surface_scale,
                           (y - _gdk_offset_y) * impl->surface_scale,
                           0, 0,
                           SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
}

static void
gdk_win32_surface_resize (GdkSurface *window,
			 gint width, gint height)
{
  RECT outer_rect;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_resize: %p: %dx%d\n",
                           GDK_SURFACE_HWND (window), width, height));

  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    return;

  get_outer_rect (window, width, height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,0,0,%ld,%ld,"
                           "NOACTIVATE|NOMOVE|NOZORDER)\n",
                           GDK_SURFACE_HWND (window),
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                           SWP_NOZORDER_SPECIFIED,
                           0, 0,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top,
                           SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));
  window->resize_count += 1;
}

static void
gdk_win32_surface_do_move_resize (GdkSurface *window,
                                  gint        x,
                                  gint        y,
                                  gint        width,
                                  gint        height)
{
  RECT outer_rect;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_move_resize: %p: %dx%d@%+d%+d\n",
                           GDK_SURFACE_HWND (window),
                           width, height, x, y));

  impl = GDK_WIN32_SURFACE (window);

  get_outer_rect (window, width, height, &outer_rect);

  adjust_for_gravity_hints (window, &outer_rect, &x, &y);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%ld,%ld,"
                           "NOACTIVATE|NOZORDER)\n",
                           GDK_SURFACE_HWND (window),
                           (x - _gdk_offset_x) * impl->surface_scale,
                           (y - _gdk_offset_y) * impl->surface_scale,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                           SWP_NOZORDER_SPECIFIED,
                           (x - _gdk_offset_x) * impl->surface_scale,
                           (y - _gdk_offset_y) * impl->surface_scale,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top,
                           SWP_NOACTIVATE | SWP_NOZORDER));
}

static void
gdk_win32_surface_move_resize_internal (GdkSurface *window,
                                        gboolean    with_move,
                                        gint        x,
                                        gint        y,
                                        gint        width,
                                        gint        height)
{
  GdkWin32Surface *surface = GDK_WIN32_SURFACE (window);

  surface->inhibit_configure = TRUE;

  /* We ignore changes to the window being moved or resized by the
     user, as we don't want to fight the user */
  if (GDK_SURFACE_HWND (window) == _modal_move_resize_window)
    goto out;

  if (with_move && (width < 0 && height < 0))
    {
      gdk_win32_surface_move (window, x, y);
    }
  else
    {
      if (with_move)
	{
          gdk_win32_surface_do_move_resize (window, x, y, width, height);
	}
      else
	{
	  gdk_win32_surface_resize (window, width, height);
	}
    }

 out:
  surface->inhibit_configure = FALSE;

  _gdk_win32_emit_configure_event (window);
}

static void
gdk_win32_surface_move_resize (GdkSurface *window,
                               gint        x,
                               gint        y,
                               gint        width,
                               gint        height)
{
  gdk_win32_surface_move_resize_internal (window, TRUE, x, y, width, height);
}

static void
gdk_win32_surface_toplevel_resize (GdkSurface *surface,
                                   gint        x,
                                   gint        y)
{
  gdk_win32_surface_move_resize_internal (surface, FALSE, 0, 0, width, height);
}

void
gdk_win32_surface_move (GdkSurface *surface,
                        gint        x,
                        gint        y)
{
  gdk_win32_surface_move_resize_internal (surface, TRUE, x, y, -1, -1);
}

static void
gdk_win32_surface_moved_to_rect (GdkSurface   *surface,
                                 GdkRectangle  final_rect)
{
  GdkSurface *toplevel;
  int x, y;

  if (surface->surface_type == GDK_SURFACE_POPUP)
    toplevel = surface->parent;
  else
    toplevel = surface->transient_for;

  gdk_surface_get_origin (toplevel, &x, &y);
  x += final_rect.x;
  y += final_rect.y;

  if (final_rect.width != surface->width ||
      final_rect.height != surface->height)
    {
      gdk_win32_surface_move_resize (surface,
                                     x, y,
                                     final_rect.width, final_rect.height);
    }
  else
    {
      gdk_win32_surface_move (surface, x, y);
    }
}

static void
gdk_win32_surface_move_to_rect (GdkSurface         *surface,
                                const GdkRectangle *rect,
                                GdkGravity          rect_anchor,
                                GdkGravity          surface_anchor,
                                GdkAnchorHints      anchor_hints,
                                gint                rect_anchor_dx,
                                gint                rect_anchor_dy)
{
  gdk_surface_move_to_rect_helper (surface,
                                   rect,
                                   rect_anchor,
                                   surface_anchor,
                                   anchor_hints,
                                   rect_anchor_dx,
                                   rect_anchor_dy,
                                   gdk_win32_surface_moved_to_rect);
}

static void
gdk_win32_surface_raise (GdkSurface *window)
{
  if (!GDK_SURFACE_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_surface_raise: %p\n",
			       GDK_SURFACE_HWND (window)));

      if (GDK_SURFACE_TYPE (window) == GDK_SURFACE_TEMP)
        API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_TOPMOST,
	                         0, 0, 0, 0,
				 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER));
      else if (window->accept_focus)
        /* Do not wrap this in an API_CALL macro as SetForegroundWindow might
         * fail when for example dragging a window belonging to a different
         * application at the time of a gtk_window_present() call due to focus
         * stealing prevention. */
        SetForegroundWindow (GDK_SURFACE_HWND (window));
      else
        API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_TOP,
  			         0, 0, 0, 0,
			         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER));
    }
}

static void
gdk_win32_surface_lower (GdkSurface *window)
{
  if (!GDK_SURFACE_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_surface_lower: %p\n"
			       "... SetWindowPos(%p,HWND_BOTTOM,0,0,0,0,"
			       "NOACTIVATE|NOMOVE|NOSIZE)\n",
			       GDK_SURFACE_HWND (window),
			       GDK_SURFACE_HWND (window)));

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_BOTTOM,
			       0, 0, 0, 0,
			       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER));
    }
}

void
gdk_win32_surface_set_urgency_hint (GdkSurface *window,
                                    gboolean    urgent)
{
  FLASHWINFO flashwinfo;
  typedef BOOL (WINAPI *PFN_FlashWindowEx) (FLASHWINFO*);
  PFN_FlashWindowEx flashWindowEx = NULL;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  flashWindowEx = (PFN_FlashWindowEx) GetProcAddress (GetModuleHandle ("user32.dll"), "FlashWindowEx");

  if (flashWindowEx)
    {
      flashwinfo.cbSize = sizeof (flashwinfo);
      flashwinfo.hwnd = GDK_SURFACE_HWND (window);
      if (urgent)
	flashwinfo.dwFlags = FLASHW_ALL | FLASHW_TIMER;
      else
	flashwinfo.dwFlags = FLASHW_STOP;
      flashwinfo.uCount = 0;
      flashwinfo.dwTimeout = 0;

      flashWindowEx (&flashwinfo);
    }
  else
    {
      FlashWindow (GDK_SURFACE_HWND (window), urgent);
    }
}

static gboolean
get_effective_window_decorations (GdkSurface       *window,
                                  GdkWMDecoration *decoration)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (gdk_surface_get_decorations (window, decoration))
    return TRUE;

  if (window->surface_type != GDK_SURFACE_TOPLEVEL)
    {
      return FALSE;
    }

  if ((impl->hint_flags & GDK_HINT_MIN_SIZE) &&
      (impl->hint_flags & GDK_HINT_MAX_SIZE) &&
      impl->hints.min_width == impl->hints.max_width &&
      impl->hints.min_height == impl->hints.max_height)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MAXIMIZE;

      if (impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
	  impl->type_hint == GDK_SURFACE_TYPE_HINT_MENU ||
	  impl->type_hint == GDK_SURFACE_TYPE_HINT_TOOLBAR)
	{
	  *decoration |= GDK_DECOR_MINIMIZE;
	}
      else if (impl->type_hint == GDK_SURFACE_TYPE_HINT_SPLASHSCREEN)
	{
	  *decoration |= GDK_DECOR_MENU | GDK_DECOR_MINIMIZE;
	}

      return TRUE;
    }
  else if (impl->hint_flags & GDK_HINT_MAX_SIZE)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_MAXIMIZE;
      if (impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
	  impl->type_hint == GDK_SURFACE_TYPE_HINT_MENU ||
	  impl->type_hint == GDK_SURFACE_TYPE_HINT_TOOLBAR)
	{
	  *decoration |= GDK_DECOR_MINIMIZE;
	}

      return TRUE;
    }
  else
    {
      switch (impl->type_hint)
	{
	case GDK_SURFACE_TYPE_HINT_DIALOG:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_SURFACE_TYPE_HINT_MENU:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_SURFACE_TYPE_HINT_TOOLBAR:
	case GDK_SURFACE_TYPE_HINT_UTILITY:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MENU |
			 GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_SURFACE_TYPE_HINT_DOCK:
	  return FALSE;

	case GDK_SURFACE_TYPE_HINT_DESKTOP:
	  return FALSE;

	default:
	  /* Fall thru */
	case GDK_SURFACE_TYPE_HINT_NORMAL:
	  *decoration = GDK_DECOR_ALL;
	  return TRUE;
	}
    }

  return FALSE;
}

static void
gdk_win32_surface_set_geometry_hints (GdkSurface         *window,
			       const GdkGeometry *geometry,
			       GdkSurfaceHints     geom_mask)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_geometry_hints: %p\n",
			   GDK_SURFACE_HWND (window)));

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    fi->hint_flags = geom_mask;
  else
    impl->hint_flags = geom_mask;
  impl->hints = *geometry;

  if (geom_mask & GDK_HINT_POS)
    {
      /* even the X11 mplementation doesn't care */
    }

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

  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... BASE_SIZE: %dx%d\n",
			       geometry->base_width, geometry->base_height));
    }

  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      GDK_NOTE (MISC, g_print ("... RESIZE_INC: (%d,%d)\n",
			       geometry->width_inc, geometry->height_inc));
    }

  if (geom_mask & GDK_HINT_ASPECT)
    {
      GDK_NOTE (MISC, g_print ("... ASPECT: %g--%g\n",
			       geometry->min_aspect, geometry->max_aspect));
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      GDK_NOTE (MISC, g_print ("... GRAVITY: %d\n", geometry->win_gravity));
    }

  _gdk_win32_surface_update_style_bits (window);
}

static void
gdk_win32_surface_set_title (GdkSurface   *window,
		      const gchar *title)
{
  wchar_t *wtitle;

  g_return_if_fail (GDK_IS_SURFACE (window));
  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (window))
    return;

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";

  GDK_NOTE (MISC, g_print ("gdk_surface_set_title: %p: %s\n",
			   GDK_SURFACE_HWND (window), title));

  GDK_NOTE (MISC_OR_EVENTS, title = g_strdup_printf ("%p %s", GDK_SURFACE_HWND (window), title));

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
  API_CALL (SetWindowTextW, (GDK_SURFACE_HWND (window), wtitle));
  g_free (wtitle);

  GDK_NOTE (MISC_OR_EVENTS, g_free ((char *) title));
}

static void
gdk_win32_surface_set_transient_for (GdkSurface *window,
			      GdkSurface *parent)
{
  HWND window_id, parent_id;
  LONG_PTR old_ptr;
  DWORD w32_error;
  GdkWin32Surface *surface = GDK_WIN32_SURFACE (window);
  GdkWin32Surface *parent_impl = NULL;
  GSList *item;

  g_return_if_fail (GDK_IS_SURFACE (window));

  window_id = GDK_SURFACE_HWND (window);
  parent_id = parent != NULL ? GDK_SURFACE_HWND (parent) : NULL;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_transient_for: %p: %p\n", window_id, parent_id));

  if (GDK_SURFACE_DESTROYED (window) || (parent && GDK_SURFACE_DESTROYED (parent)))
    {
      if (GDK_SURFACE_DESTROYED (window))
	GDK_NOTE (MISC, g_print ("... destroyed!\n"));
      else
	GDK_NOTE (MISC, g_print ("... owner destroyed!\n"));

      return;
    }

  if (surface->transient_owner == parent)
    return;

  if (GDK_IS_SURFACE (surface->transient_owner))
    {
      GdkWin32Surface *trans_impl = GDK_WIN32_SURFACE (surface->transient_owner);
      item = g_slist_find (trans_impl->transient_children, window);
      item->data = NULL;
      trans_impl->transient_children = g_slist_delete_link (trans_impl->transient_children, item);
      trans_impl->num_transients--;

      if (!trans_impl->num_transients)
        {
          trans_impl->transient_children = NULL;
        }

      g_object_unref (G_OBJECT (surface->transient_owner));
      g_object_unref (G_OBJECT (window));

      surface->transient_owner = NULL;
    }

  if (parent)
    {
      parent_impl = GDK_WIN32_SURFACE (parent);

      parent_impl->transient_children = g_slist_append (parent_impl->transient_children, window);
      g_object_ref (G_OBJECT (window));
      parent_impl->num_transients++;
      surface->transient_owner = parent;
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

  /* This changes the *owner* of the window, despite the misleading
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

void
_gdk_push_modal_window (GdkSurface *window)
{
  modal_window_stack = g_slist_prepend (modal_window_stack, window);
}

void
_gdk_remove_modal_window (GdkSurface *window)
{
  GSList *tmp;

  g_return_if_fail (window != NULL);

  /* It's possible to be NULL here if someone sets the modal hint of the window
   * to FALSE before a modal window stack has ever been created. */
  if (modal_window_stack == NULL)
    return;

  /* Find the requested window in the stack and remove it.  Yeah, I realize this
   * means we're not a 'real stack', strictly speaking.  Sue me. :) */
  tmp = g_slist_find (modal_window_stack, window);
  if (tmp != NULL)
    {
      modal_window_stack = g_slist_delete_link (modal_window_stack, tmp);
    }
}

gboolean
_gdk_modal_blocked (GdkSurface *window)
{
  GSList *l;
  gboolean found_any = FALSE;

  for (l = modal_window_stack; l != NULL; l = l->next)
    {
      GdkSurface *modal = l->data;

      if (modal == window)
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

  for (l = modal_window_stack; l != NULL; l = l->next)
    {
      GdkSurface *modal = l->data;

      if (GDK_SURFACE_IS_MAPPED (modal))
	return modal;
    }

  return NULL;
}

static void
gdk_win32_surface_get_geometry (GdkSurface *window,
			       gint      *x,
			       gint      *y,
			       gint      *width,
			       gint      *height)
{
  if (!GDK_SURFACE_DESTROYED (window))
    {
      RECT rect;
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      API_CALL (GetClientRect, (GDK_SURFACE_HWND (window), &rect));

	  POINT pt;
	  GdkSurface *parent = gdk_surface_get_parent (window);

	  pt.x = rect.left;
	  pt.y = rect.top;
	  ClientToScreen (GDK_SURFACE_HWND (window), &pt);
          if (parent)
	    ScreenToClient (GDK_SURFACE_HWND (parent), &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;

	  pt.x = rect.right;
	  pt.y = rect.bottom;
	  ClientToScreen (GDK_SURFACE_HWND (window), &pt);
          if (parent)
	    ScreenToClient (GDK_SURFACE_HWND (parent), &pt);
	  rect.right = pt.x;
	  rect.bottom = pt.y;

	  if (parent == NULL)
	    {
	      rect.left += _gdk_offset_x * impl->surface_scale;
	      rect.top += _gdk_offset_y * impl->surface_scale;
	      rect.right += _gdk_offset_x * impl->surface_scale;
	      rect.bottom += _gdk_offset_y * impl->surface_scale;
	    }

      if (x)
	*x = rect.left / impl->surface_scale;
      if (y)
	*y = rect.top / impl->surface_scale;
      if (width)
	*width = (rect.right - rect.left) / impl->surface_scale;
      if (height)
	*height = (rect.bottom - rect.top) / impl->surface_scale;

      GDK_NOTE (MISC, g_print ("gdk_win32_surface_get_geometry: %p: %ldx%ld@%+ld%+ld\n",
			       GDK_SURFACE_HWND (window),
			       (rect.right - rect.left) / impl->surface_scale,
			       (rect.bottom - rect.top) / impl->surface_scale,
			       rect.left, rect.top));
    }
}

static void
gdk_win32_surface_get_root_coords (GdkSurface *window,
				  gint       x,
				  gint       y,
				  gint      *root_x,
				  gint      *root_y)
{
  gint tx;
  gint ty;
  POINT pt;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  pt.x = x * impl->surface_scale;
  pt.y = y * impl->surface_scale;
  ClientToScreen (GDK_SURFACE_HWND (window), &pt);
  tx = pt.x;
  ty = pt.y;

  if (root_x)
    *root_x = (tx + _gdk_offset_x) / impl->surface_scale;
  if (root_y)
    *root_y = (ty + _gdk_offset_y) / impl->surface_scale;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_get_root_coords: %p: %+d%+d %+d%+d\n",
			   GDK_SURFACE_HWND (window),
			   x * impl->surface_scale,
			   y * impl->surface_scale,
			   (tx + _gdk_offset_x) / impl->surface_scale,
			   (ty + _gdk_offset_y) / impl->surface_scale));
}

static void
gdk_win32_surface_restack_toplevel (GdkSurface *window,
				   GdkSurface *sibling,
				   gboolean   above)
{
	// ### TODO
}

static gboolean
gdk_surface_win32_get_device_state (GdkSurface       *window,
                                   GdkDevice       *device,
                                   gdouble         *x,
                                   gdouble         *y,
                                   GdkModifierType *mask)
{
  GdkSurface *child;

  g_return_val_if_fail (window == NULL || GDK_IS_SURFACE (window), FALSE);

  GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
                                              &child,
                                              NULL, NULL,
                                              x, y, mask);
  return (child != NULL);
}

static void
gdk_win32_surface_set_accept_focus (GdkSurface *window,
			     gboolean accept_focus)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  accept_focus = accept_focus != FALSE;

  if (window->accept_focus != accept_focus)
    window->accept_focus = accept_focus;
}

static void
gdk_win32_surface_set_focus_on_map (GdkSurface *window,
			     gboolean focus_on_map)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    window->focus_on_map = focus_on_map;
}

static void
gdk_win32_surface_set_icon_list (GdkSurface *window,
                                GList     *textures)
{
  GdkTexture *big_texture, *small_texture;
  gint big_diff, small_diff;
  gint big_w, big_h, small_w, small_h;
  gint w, h;
  gint dw, dh, diff;
  HICON small_hicon, big_hicon;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) || textures == NULL)
    return;

  impl = GDK_WIN32_SURFACE (window);

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

  for (GList *l = textures; l; l = l->next)
    {
      GdkTexture *texture = l->data;
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

  /* Create the icons */
  big_hicon = _gdk_win32_texture_to_hicon (big_texture);
  small_hicon = _gdk_win32_texture_to_hicon (small_texture);

  /* Set the icons */
  SendMessageW (GDK_SURFACE_HWND (window), WM_SETICON, ICON_BIG,
		(LPARAM)big_hicon);
  SendMessageW (GDK_SURFACE_HWND (window), WM_SETICON, ICON_SMALL,
		(LPARAM)small_hicon);

  /* Store the icons, destroying any previous icons */
  if (impl->hicon_big)
    GDI_CALL (DestroyIcon, (impl->hicon_big));
  impl->hicon_big = big_hicon;
  if (impl->hicon_small)
    GDI_CALL (DestroyIcon, (impl->hicon_small));
  impl->hicon_small = small_hicon;
}

static void
gdk_win32_surface_set_icon_name (GdkSurface   *window,
                                const gchar *name)
{
  /* In case I manage to confuse this again (or somebody else does):
   * Please note that "icon name" here really *does* mean the name or
   * title of an window minimized as an icon on the desktop, or in the
   * taskbar. It has nothing to do with the freedesktop.org icon
   * naming stuff.
   */

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

#if 0
  /* This is not the correct thing to do. We should keep both the
   * "normal" window title, and the icon name. When the window is
   * minimized, call SetWindowText() with the icon name, and when the
   * window is restored, with the normal window title. Also, the name
   * is in UTF-8, so we should do the normal conversion to either wide
   * chars or system codepage, and use either the W or A version of
   * SetWindowText(), depending on Windows version.
   */
  API_CALL (SetWindowText, (GDK_SURFACE_HWND (window), name));
#endif
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
 * Returns TRUE if window has no decorations.
 * Usually it means CSD windows, because GTK
 * calls gdk_surface_set_decorations (window, 0);
 * This is used to decide whether a toplevel should
 * be made layered, thus it
 * only returns TRUE for toplevels (until GTK minimal
 * system requirements are lifted to Windows 8 or newer,
 * because only toplevels can be layered).
 */
gboolean
_gdk_win32_surface_lacks_wm_decorations (GdkSurface *window)
{
  GdkWin32Surface *impl;
  LONG style;
  gboolean has_any_decorations;

  if (GDK_SURFACE_DESTROYED (window))
    return FALSE;

  impl = GDK_WIN32_SURFACE (window);

  /* This is because GTK calls gdk_surface_set_decorations (window, 0),
   * even though GdkWMDecoration docs indicate that 0 does NOT mean
   * "no decorations".
   */
  if (impl->decorations &&
      *impl->decorations == 0)
    return TRUE;

  if (GDK_SURFACE_HWND (window) == 0)
    return FALSE;

  style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);

  if (style == 0)
    {
      DWORD w32_error = GetLastError ();

      GDK_NOTE (MISC, g_print ("Failed to get style of window %p (handle %p): %lu\n",
                               window, GDK_SURFACE_HWND (window), w32_error));
      return FALSE;
    }

  /* Keep this in sync with _gdk_win32_surface_update_style_bits() */
  /* We don't check what get_effective_window_decorations()
   * has to say, because it gives suggestions based on
   * various hints, while we want *actual* decorations,
   * or their absence.
   */
  has_any_decorations = FALSE;

  if (style & (WS_BORDER | WS_THICKFRAME | WS_CAPTION |
               WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX))
    has_any_decorations = TRUE;
  else
    GDK_NOTE (MISC, g_print ("Window %p (handle %p): has no decorations (style %lx)\n",
                             window, GDK_SURFACE_HWND (window), style));

  return !has_any_decorations;
}

void
_gdk_win32_surface_update_style_bits (GdkSurface *window)
{
  GdkWin32Surface *impl = (GdkWin32Surface *)window;
  GdkWMDecoration decorations;
  LONG old_style, new_style, old_exstyle, new_exstyle;
  gboolean all;
  RECT rect, before, after;
  gboolean was_topmost;
  gboolean will_be_topmost;
  HWND insert_after;
  UINT flags;

  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    return;

  old_style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);
  old_exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);

  GetClientRect (GDK_SURFACE_HWND (window), &before);
  after = before;
  AdjustWindowRectEx (&before, old_style, FALSE, old_exstyle);

  was_topmost = (old_exstyle & WS_EX_TOPMOST) ? TRUE : FALSE;
  will_be_topmost = was_topmost;

  old_exstyle &= ~WS_EX_TOPMOST;

  new_style = old_style;
  new_exstyle = old_exstyle;

  if (window->surface_type == GDK_SURFACE_TEMP)
    {
      new_exstyle |= WS_EX_TOOLWINDOW;
      will_be_topmost = TRUE;
    }
  else if (impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY)
    {
      new_exstyle |= WS_EX_TOOLWINDOW;
    }
  else
    {
      new_exstyle &= ~WS_EX_TOOLWINDOW;
    }

  /* We can get away with using layered windows
   * only when no decorations are needed. It can mean
   * CSD or borderless non-CSD windows (tooltips?).
   *
   * If this window cannot use layered windows, disable it always.
   * This currently applies to windows using OpenGL, which
   * does not work with layered windows.
   */
  if (impl->suppress_layered == 0)
    {
      if (_gdk_win32_surface_lacks_wm_decorations (window))
        impl->layered = g_strcmp0 (g_getenv ("GDK_WIN32_LAYERED"), "0") != 0;
    }
  else
    impl->layered = FALSE;

  if (impl->layered)
    new_exstyle |= WS_EX_LAYERED;
  else
    new_exstyle &= ~WS_EX_LAYERED;

  if (get_effective_window_decorations (window, &decorations))
    {
      all = (decorations & GDK_DECOR_ALL);
      /* Keep this in sync with the test in _gdk_win32_surface_lacks_wm_decorations() */
      update_single_bit (&new_style, all, decorations & GDK_DECOR_BORDER, WS_BORDER);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_RESIZEH, WS_THICKFRAME);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_TITLE, WS_CAPTION);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MENU, WS_SYSMENU);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MINIMIZE, WS_MINIMIZEBOX);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MAXIMIZE, WS_MAXIMIZEBOX);
    }

  if (old_style == new_style && old_exstyle == new_exstyle )
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: no change\n",
			       GDK_SURFACE_HWND (window)));
      return;
    }

  if (old_style != new_style)
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: STYLE: %s => %s\n",
			       GDK_SURFACE_HWND (window),
			       _gdk_win32_surface_style_to_string (old_style),
			       _gdk_win32_surface_style_to_string (new_style)));

      SetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE, new_style);
    }

  if (old_exstyle != new_exstyle)
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: EXSTYLE: %s => %s\n",
			       GDK_SURFACE_HWND (window),
			       _gdk_win32_surface_exstyle_to_string (old_exstyle),
			       _gdk_win32_surface_exstyle_to_string (new_exstyle)));

      SetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE, new_exstyle);
    }

  AdjustWindowRectEx (&after, new_style, FALSE, new_exstyle);

  GetWindowRect (GDK_SURFACE_HWND (window), &rect);
  rect.left += after.left - before.left;
  rect.top += after.top - before.top;
  rect.right += after.right - before.right;
  rect.bottom += after.bottom - before.bottom;

  flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOREPOSITION | SWP_NOOWNERZORDER;

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

  SetWindowPos (GDK_SURFACE_HWND (window), insert_after,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		flags);
}

static void
update_single_system_menu_entry (HMENU    hmenu,
				 gboolean all,
				 int      gdk_bit,
				 int      menu_entry)
{
  /* all controls the interpretation of gdk_bit -- if all is TRUE,
   * gdk_bit indicates whether menu entry is disabled; if all is
   * FALSE, gdk bit indicate whether menu entry is enabled
   */
  if ((!all && gdk_bit) || (all && !gdk_bit))
    EnableMenuItem (hmenu, menu_entry, MF_BYCOMMAND | MF_ENABLED);
  else
    EnableMenuItem (hmenu, menu_entry, MF_BYCOMMAND | MF_GRAYED);
}

static void
update_system_menu (GdkSurface *window)
{
  GdkWMFunction functions;
  BOOL all;

  if (_gdk_surface_get_functions (window, &functions))
    {
      HMENU hmenu = GetSystemMenu (GDK_SURFACE_HWND (window), FALSE);

      all = (functions & GDK_FUNC_ALL);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_RESIZE, SC_SIZE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_MOVE, SC_MOVE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_MINIMIZE, SC_MINIMIZE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_MAXIMIZE, SC_MAXIMIZE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_CLOSE, SC_CLOSE);
    }
}

static void
gdk_win32_surface_set_decorations (GdkSurface      *window,
				  GdkWMDecoration decorations)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_WIN32_SURFACE (window);

  GDK_NOTE (MISC, g_print ("gdk_surface_set_decorations: %p: %s %s%s%s%s%s%s\n",
			   GDK_SURFACE_HWND (window),
			   (decorations & GDK_DECOR_ALL ? "clearing" : "setting"),
			   (decorations & GDK_DECOR_BORDER ? "BORDER " : ""),
			   (decorations & GDK_DECOR_RESIZEH ? "RESIZEH " : ""),
			   (decorations & GDK_DECOR_TITLE ? "TITLE " : ""),
			   (decorations & GDK_DECOR_MENU ? "MENU " : ""),
			   (decorations & GDK_DECOR_MINIMIZE ? "MINIMIZE " : ""),
			   (decorations & GDK_DECOR_MAXIMIZE ? "MAXIMIZE " : "")));

  if (!impl->decorations)
    impl->decorations = g_malloc (sizeof (GdkWMDecoration));

  *impl->decorations = decorations;

  _gdk_win32_surface_update_style_bits (window);
}

static gboolean
gdk_win32_surface_get_decorations (GdkSurface       *window,
				  GdkWMDecoration *decorations)
{
  GdkWin32Surface *impl;

  g_return_val_if_fail (GDK_IS_SURFACE (window), FALSE);

  impl = GDK_WIN32_SURFACE (window);

  if (impl->decorations == NULL)
    return FALSE;

  *decorations = *impl->decorations;

  return TRUE;
}

static GQuark
get_functions_quark ()
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string ("gdk-surface-functions");

  return quark;
}

static void
gdk_win32_surface_set_functions (GdkSurface    *window,
			  GdkWMFunction functions)
{
  GdkWMFunction* functions_copy;

  g_return_if_fail (GDK_IS_SURFACE (window));

  GDK_NOTE (MISC, g_print ("gdk_surface_set_functions: %p: %s %s%s%s%s%s\n",
			   GDK_SURFACE_HWND (window),
			   (functions & GDK_FUNC_ALL ? "clearing" : "setting"),
			   (functions & GDK_FUNC_RESIZE ? "RESIZE " : ""),
			   (functions & GDK_FUNC_MOVE ? "MOVE " : ""),
			   (functions & GDK_FUNC_MINIMIZE ? "MINIMIZE " : ""),
			   (functions & GDK_FUNC_MAXIMIZE ? "MAXIMIZE " : ""),
			   (functions & GDK_FUNC_CLOSE ? "CLOSE " : "")));

  functions_copy = g_malloc (sizeof (GdkWMFunction));
  *functions_copy = functions;
  g_object_set_qdata_full (G_OBJECT (window), get_functions_quark (), functions_copy, g_free);

  update_system_menu (window);
}

gboolean
_gdk_surface_get_functions (GdkSurface     *window,
		           GdkWMFunction *functions)
{
  GdkWMFunction* functions_set;

  functions_set = g_object_get_qdata (G_OBJECT (window), get_functions_quark ());
  if (functions_set)
    *functions = *functions_set;

  return (functions_set != NULL);
}

#if defined(MORE_AEROSNAP_DEBUGGING)
static void
log_region (gchar *prefix, AeroSnapEdgeRegion *region)
{
  GDK_NOTE (MISC, g_print ("Region %s:\n"
                           "edge %d x %d @ %d x %d\n"
                           "trig %d x %d @ %d x %d\n",
                           prefix,
                           region->edge.width,
                           region->edge.height,
                           region->edge.x,
                           region->edge.y,
                           region->trigger.width,
                           region->trigger.height,
                           region->trigger.x,
                           region->trigger.y));
}
#endif

static void
calculate_aerosnap_regions (GdkW32DragMoveResizeContext *context)
{
  GdkDisplay *display;
  gint n_monitors, monitor_idx, other_monitor_idx;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (context->window);
#if defined(MORE_AEROSNAP_DEBUGGING)
  gint i;
#endif

  display = gdk_display_get_default ();
  n_monitors = gdk_display_get_n_monitors (display);

#define _M_UP 0
#define _M_DOWN 1
#define _M_LEFT 2
#define _M_RIGHT 3

  for (monitor_idx = 0; monitor_idx < n_monitors; monitor_idx++)
    {
      GdkRectangle wa;
      GdkRectangle geometry;
      AeroSnapEdgeRegion snap_region;
      gboolean move_edge[4] = { TRUE, FALSE, TRUE, TRUE };
      gboolean resize_edge[2] = { TRUE, TRUE };
      gint diff;
      gint thickness, trigger_thickness;
      GdkMonitor *monitor;

      monitor = gdk_display_get_monitor (display, monitor_idx);
      gdk_monitor_get_workarea (monitor, &wa);
      gdk_monitor_get_geometry (monitor, &geometry);

      for (other_monitor_idx = 0;
           other_monitor_idx < n_monitors &&
           (move_edge[_M_UP] || move_edge[_M_LEFT] ||
           move_edge[_M_RIGHT] || resize_edge[_M_DOWN]);
           other_monitor_idx++)
        {
          GdkRectangle other_wa;
          GdkMonitor *other_monitor;

          if (other_monitor_idx == monitor_idx)
            continue;

          other_monitor = gdk_display_get_monitor (display, other_monitor_idx);
          gdk_monitor_get_workarea (other_monitor, &other_wa);

          /* An edge triggers AeroSnap only if there are no
           * monitors beyond that edge.
           * Even if there's another monitor, but it does not cover
           * the whole edge (it's smaller or is not aligned to
           * the corner of current monitor), that edge is still
           * removed from the trigger list.
           */
          if (other_wa.x >= wa.x + wa.width)
            move_edge[_M_RIGHT] = FALSE;

          if (other_wa.x + other_wa.width <= wa.x)
            move_edge[_M_LEFT] = FALSE;

          if (other_wa.y + other_wa.height <= wa.y)
            {
              move_edge[_M_UP] = FALSE;
              resize_edge[_M_UP] = FALSE;
            }

          if (other_wa.y >= wa.y + wa.height)
            {
              /* no move_edge for the bottom edge, just resize_edge */
              resize_edge[_M_DOWN] = FALSE;
            }
        }

      thickness = AEROSNAP_REGION_THICKNESS * impl->surface_scale;
      trigger_thickness = AEROSNAP_REGION_TRIGGER_THICKNESS * impl->surface_scale;

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.height = thickness;
      snap_region.trigger.height = trigger_thickness;

      /* Extend both regions into toolbar space.
       * When there's no toolbar, diff == 0.
       */
      diff = wa.y - geometry.y;
      snap_region.edge.height += diff;
      snap_region.edge.y -= diff;
      snap_region.trigger.height += diff;
      snap_region.trigger.y -= diff;

      if (move_edge[_M_UP])
        g_array_append_val (context->maximize_regions, snap_region);

      if (resize_edge[_M_UP])
        g_array_append_val (context->fullup_regions, snap_region);

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.width = thickness;
      snap_region.trigger.width = trigger_thickness;

      diff = wa.x - geometry.x;
      snap_region.edge.width += diff;
      snap_region.edge.x -= diff;
      snap_region.trigger.width += diff;
      snap_region.trigger.x -= diff;

      if (move_edge[_M_LEFT])
        g_array_append_val (context->halfleft_regions, snap_region);

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.x += wa.width - thickness;
      snap_region.edge.width = thickness;
      snap_region.trigger.x += wa.width - trigger_thickness;
      snap_region.trigger.width = trigger_thickness;

      diff = (geometry.x + geometry.width) - (wa.x + wa.width);
      snap_region.edge.width += diff;
      snap_region.trigger.width += diff;

      if (move_edge[_M_RIGHT])
        g_array_append_val (context->halfright_regions, snap_region);

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.y += wa.height - thickness;
      snap_region.edge.height = thickness;
      snap_region.trigger.y += wa.height - trigger_thickness;
      snap_region.trigger.height = trigger_thickness;

      diff = (geometry.y + geometry.height) - (wa.y + wa.height);
      snap_region.edge.height += diff;
      snap_region.trigger.height += diff;

      if (resize_edge[_M_DOWN])
        g_array_append_val (context->fullup_regions, snap_region);
    }

#undef _M_UP
#undef _M_DOWN
#undef _M_LEFT
#undef _M_RIGHT

#if defined(MORE_AEROSNAP_DEBUGGING)
  for (i = 0; i < context->maximize_regions->len; i++)
    log_region ("maximize", &g_array_index (context->maximize_regions, AeroSnapEdgeRegion, i));

  for (i = 0; i < context->halfleft_regions->len; i++)
    log_region ("halfleft", &g_array_index (context->halfleft_regions, AeroSnapEdgeRegion, i));

  for (i = 0; i < context->halfright_regions->len; i++)
    log_region ("halfright", &g_array_index (context->halfright_regions, AeroSnapEdgeRegion, i));

  for (i = 0; i < context->fullup_regions->len; i++)
    log_region ("fullup", &g_array_index (context->fullup_regions, AeroSnapEdgeRegion, i));
#endif
}

static void
discard_snapinfo (GdkSurface *window)
{
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;

  if (impl->snap_stash == NULL)
    return;

  g_clear_pointer (&impl->snap_stash, g_free);
  g_clear_pointer (&impl->snap_stash_int, g_free);
}

static void
unsnap (GdkSurface  *window,
        GdkMonitor *monitor)
{
  GdkWin32Surface *impl;
  GdkRectangle rect;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;

  if (impl->snap_stash == NULL)
    return;

  gdk_monitor_get_workarea (monitor, &rect);

  GDK_NOTE (MISC, g_print ("Monitor work area %d x %d @ %d : %d\n", rect.width, rect.height, rect.x, rect.y));

  if (rect.width >= impl->snap_stash_int->width &&
      rect.height >= impl->snap_stash_int->height)
    {
      /* If the window fits into new work area without resizing it,
       * place it into new work area without resizing it.
       */
      gdouble left, right, up, down, hratio, vratio;
      gdouble hscale, vscale;
      gdouble new_left, new_up;

      left = impl->snap_stash->x;
      right = 1.0 - (impl->snap_stash->x + impl->snap_stash->width);
      up = impl->snap_stash->y;
      down = 1.0 - (impl->snap_stash->y + impl->snap_stash->height);
      hscale = 1.0;

      if (right > 0.001)
        {
          hratio = left / right;
          hscale = hratio / (1.0 + hratio);
        }

      new_left = (gdouble) (rect.width - impl->snap_stash_int->width) * hscale;

      vscale = 1.0;

      if (down > 0.001)
        {
          vratio = up / down;
          vscale = vratio / (1.0 + vratio);
        }

      new_up = (gdouble) (rect.height - impl->snap_stash_int->height) * vscale;

      rect.x = round (rect.x + new_left);
      rect.y = round (rect.y + new_up);
      rect.width = impl->snap_stash_int->width;
      rect.height = impl->snap_stash_int->height;
    }
  else
    {
      /* Calculate actual unsnapped window size based on its
       * old relative size. Same for position.
       */
      rect.x += round (rect.width * impl->snap_stash->x);
      rect.y += round (rect.height * impl->snap_stash->y);
      rect.width = round (rect.width * impl->snap_stash->width);
      rect.height = round (rect.height * impl->snap_stash->height);
    }

  GDK_NOTE (MISC, g_print ("Unsnapped window size %d x %d @ %d : %d\n", rect.width, rect.height, rect.x, rect.y));

  gdk_win32_surface_move_resize (window, rect.x, rect.y,
                                 rect.width, rect.height);

  g_clear_pointer (&impl->snap_stash, g_free);
  g_clear_pointer (&impl->snap_stash_int, g_free);
}

static void
stash_window (GdkSurface          *window,
              GdkWin32Surface *impl)
{
  gint x, y;
  gint width, wwidth;
  gint height, wheight;
  WINDOWPLACEMENT placement;
  HMONITOR hmonitor;
  MONITORINFO hmonitor_info;

  placement.length = sizeof(WINDOWPLACEMENT);

  /* Use W32 API to get unmaximized window size, which GDK doesn't remember */
  if (!GetWindowPlacement (GDK_SURFACE_HWND (window), &placement))
    return;

  /* MSDN is very vague, but in practice rcNormalPosition is the same as GetWindowRect(),
   * only with adjustments for toolbars (which creates rather weird coodinate space issues).
   * We need to get monitor info and apply workarea vs monitorarea diff to turn
   * these into screen coordinates proper.
   */
  hmonitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
  hmonitor_info.cbSize = sizeof (hmonitor_info);

  if (!GetMonitorInfoA (hmonitor, &hmonitor_info))
    return;

  if (impl->snap_stash == NULL)
    impl->snap_stash = g_new0 (GdkRectangleDouble, 1);

  if (impl->snap_stash_int == NULL)
    impl->snap_stash_int = g_new0 (GdkRectangle, 1);

  GDK_NOTE (MISC, g_print ("monitor work area  %ld x %ld @ %ld : %ld\n",
                           (hmonitor_info.rcWork.right - hmonitor_info.rcWork.left) / impl->surface_scale,
                           (hmonitor_info.rcWork.bottom - hmonitor_info.rcWork.top) / impl->surface_scale,
                           hmonitor_info.rcWork.left,
                           hmonitor_info.rcWork.top));
  GDK_NOTE (MISC, g_print ("monitor      area  %ld x %ld @ %ld : %ld\n",
                           (hmonitor_info.rcMonitor.right - hmonitor_info.rcMonitor.left) / impl->surface_scale,
                           (hmonitor_info.rcMonitor.bottom - hmonitor_info.rcMonitor.top) / impl->surface_scale,
                           hmonitor_info.rcMonitor.left,
                           hmonitor_info.rcMonitor.top));
  GDK_NOTE (MISC, g_print ("window  work place %ld x %ld @ %ld : %ld\n",
                           (placement.rcNormalPosition.right - placement.rcNormalPosition.left) / impl->surface_scale,
                           (placement.rcNormalPosition.bottom - placement.rcNormalPosition.top) / impl->surface_scale,
                           placement.rcNormalPosition.left,
                           placement.rcNormalPosition.top));

  width = (placement.rcNormalPosition.right - placement.rcNormalPosition.left) / impl->surface_scale;
  height = (placement.rcNormalPosition.bottom - placement.rcNormalPosition.top) / impl->surface_scale;
  x = (placement.rcNormalPosition.left - hmonitor_info.rcMonitor.left) / impl->surface_scale;
  y = (placement.rcNormalPosition.top - hmonitor_info.rcMonitor.top) / impl->surface_scale;

  wwidth = (hmonitor_info.rcWork.right - hmonitor_info.rcWork.left) / impl->surface_scale;
  wheight = (hmonitor_info.rcWork.bottom - hmonitor_info.rcWork.top) / impl->surface_scale;

  impl->snap_stash->x = (gdouble) (x) / (gdouble) (wwidth);
  impl->snap_stash->y = (gdouble) (y) / (gdouble) (wheight);
  impl->snap_stash->width = (gdouble) width / (gdouble) (wwidth);
  impl->snap_stash->height = (gdouble) height / (gdouble) (wheight);

  impl->snap_stash_int->x = x;
  impl->snap_stash_int->y = y;
  impl->snap_stash_int->width = width;
  impl->snap_stash_int->height = height;

  GDK_NOTE (MISC, g_print ("Stashed window %d x %d @ %d : %d as %f x %f @ %f : %f\n",
                           width, height, x, y,
                           impl->snap_stash->width, impl->snap_stash->height, impl->snap_stash->x, impl->snap_stash->y));
}

static void
snap_up (GdkSurface *window)
{
  SHORT maxysize;
  gint x, y;
  gint width, height;
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_FULLUP;

  stash_window (window, impl);

  maxysize = GetSystemMetrics (SM_CYVIRTUALSCREEN) / impl->surface_scale;
  gdk_surface_get_position (window, &x, &y);
  width = gdk_surface_get_width (window);

  y = 0;
  height = maxysize;

  x = x - impl->margins.left;
  y = y - impl->margins.top;
  width += impl->margins_x;
  height += impl->margins_y;

  gdk_win32_surface_move_resize (window, x, y, width, height);
}

static void
snap_left (GdkSurface  *window,
           GdkMonitor *monitor,
           GdkMonitor *snap_monitor)
{
  GdkRectangle rect;
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;

  gdk_monitor_get_workarea (snap_monitor, &rect);

  stash_window (window, impl);

  rect.width = rect.width / 2;

  rect.x = rect.x - impl->margins.left;
  rect.y = rect.y - impl->margins.top;
  rect.width = rect.width + impl->margins_x;
  rect.height = rect.height + impl->margins_y;

  gdk_win32_surface_move_resize (window,
                                 rect.x, rect.y,
                                 rect.width, rect.height);
}

static void
snap_right (GdkSurface  *window,
            GdkMonitor *monitor,
            GdkMonitor *snap_monitor)
{
  GdkRectangle rect;
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;

  gdk_monitor_get_workarea (snap_monitor, &rect);

  stash_window (window, impl);

  rect.width = rect.width / 2;
  rect.x += rect.width;

  rect.x = rect.x - impl->margins.left;
  rect.y = rect.y - impl->margins.top;
  rect.width = rect.width + impl->margins_x;
  rect.height = rect.height + impl->margins_y;

  gdk_win32_surface_move_resize (window,
                                 rect.x, rect.y,
                                 rect.width, rect.height);
}

void
_gdk_win32_surface_handle_aerosnap (GdkSurface            *window,
                                   GdkWin32AeroSnapCombo combo)
{
  GdkWin32Surface *impl;
  GdkDisplay *display;
  gint n_monitors;
  GdkSurfaceState surface_state = gdk_surface_get_state (window);
  gboolean minimized = surface_state & GDK_SURFACE_STATE_ICONIFIED;
  gboolean maximized = surface_state & GDK_SURFACE_STATE_MAXIMIZED;
  gboolean halfsnapped;
  GdkMonitor *monitor;

  impl = GDK_WIN32_SURFACE (window);
  display = gdk_surface_get_display (window);
  n_monitors = gdk_display_get_n_monitors (display);
  monitor = gdk_display_get_monitor_at_surface (display, window);

  if (minimized && maximized)
    minimized = FALSE;

  halfsnapped = (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT ||
                 impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT ||
                 impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP);

  switch (combo)
    {
    case GDK_WIN32_AEROSNAP_COMBO_NOTHING:
      /* Do nothing */
      break;
    case GDK_WIN32_AEROSNAP_COMBO_UP:
      if (!maximized)
        {
	  unsnap (window, monitor);
          gdk_surface_maximize (window);
        }
      break;
    case GDK_WIN32_AEROSNAP_COMBO_DOWN:
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTDOWN:
      if (maximized)
        {
	  gdk_surface_unmaximize (window);
	  unsnap (window, monitor);
        }
      else if (halfsnapped)
	unsnap (window, monitor);
      else if (!minimized)
	gdk_surface_iconify (window);
      break;
    case GDK_WIN32_AEROSNAP_COMBO_LEFT:
      if (maximized)
        gdk_surface_unmaximize (window);

      if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_UNDETERMINED ||
	  impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP)
	{
	  unsnap (window, monitor);
	  snap_left (window, monitor, monitor);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT)
	{
	  unsnap (window, monitor);
	  snap_right (window,
	              monitor,
	              gdk_monitor_is_primary (monitor) ? monitor : gdk_display_get_monitor (display, n_monitors - 1));
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT)
	{
	  unsnap (window, monitor);
	}
      break;
    case GDK_WIN32_AEROSNAP_COMBO_RIGHT:
      if (maximized)
        gdk_surface_unmaximize (window);

      if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_UNDETERMINED ||
	  impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP)
	{
	  unsnap (window, monitor);
	  snap_right (window, monitor, monitor);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT)
	{
	  unsnap (window, monitor);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT)
	{
	  gint i;

	  unsnap (window, monitor);
	  if (n_monitors == 1 ||
	      monitor == gdk_display_get_monitor (display, n_monitors - 1))
	    {
	      snap_left (window, monitor, monitor);
	    }
	  else
	    {
	      for (i = 0; i < n_monitors; i++)
	        {
	          if (monitor == gdk_display_get_monitor (display, i))
	            break;
	        }

	      snap_left (window, monitor, gdk_display_get_monitor (display, i + 1));
	    }
	}
      break;
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTUP:
      if (!maximized &&
          impl->snap_state == GDK_WIN32_AEROSNAP_STATE_UNDETERMINED)
	{
	  snap_up (window);
	}
      break;
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTLEFT:
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTRIGHT:
      /* No implementation needed at the moment */
      break;
    }
}

static void
apply_snap (GdkSurface             *window,
            GdkWin32AeroSnapState  snap)
{
  GdkMonitor *monitor;
  GdkDisplay *display;

  display = gdk_surface_get_display (window);
  monitor = gdk_display_get_monitor_at_surface (display, window);

  switch (snap)
    {
    case GDK_WIN32_AEROSNAP_STATE_UNDETERMINED:
      break;
    case GDK_WIN32_AEROSNAP_STATE_MAXIMIZE:
      unsnap (window, monitor);
      gdk_surface_maximize (window);
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFLEFT:
      unsnap (window, monitor);
      snap_left (window, monitor, monitor);
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFRIGHT:
      unsnap (window, monitor);
      snap_right (window, monitor, monitor);
      break;
    case GDK_WIN32_AEROSNAP_STATE_FULLUP:
      snap_up (window);
      break;
    }
}

/* Registers a dumb window class. This window
 * has DefWindowProc() for a window procedure and
 * does not do anything that GdkSurface-bound HWNDs do.
 */
static ATOM
RegisterGdkDumbClass ()
{
  static ATOM klassDUMB = 0;
  static WNDCLASSEXW wcl;
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = DefWindowProcW;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = _gdk_dll_hinstance;
  wcl.hIcon = 0;
  wcl.hIconSm = 0;
  wcl.lpszMenuName = NULL;
  wcl.hbrBackground = NULL;
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);
  wcl.style |= CS_OWNDC;
  wcl.lpszClassName = L"gdkSurfaceDumb";

  if (klassDUMB == 0)
    klassDUMB = RegisterClassExW (&wcl);

  klass = klassDUMB;

  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      g_error ("That is a fatal error");
    }

  return klass;
}

static gboolean
ensure_snap_indicator_exists (GdkW32DragMoveResizeContext *context)
{
  if (context->shape_indicator == NULL)
    {
      HWND handle;
      ATOM klass;
      klass = RegisterGdkDumbClass ();

      handle = CreateWindowExW (WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
                                MAKEINTRESOURCEW (klass),
                                L"",
                                WS_POPUP,
                                0,
                                0,
                                0, 0,
                                NULL,
                                NULL,
                                _gdk_dll_hinstance,
                                NULL);

      context->shape_indicator = handle;
    }

  return context->shape_indicator != NULL;
}

static gboolean
ensure_snap_indicator_surface (GdkW32DragMoveResizeContext *context,
                          gint                         width,
                          gint                         height,
                          guint                        scale)
{
  if (context->indicator_surface != NULL &&
      (context->indicator_surface_width < width ||
       context->indicator_surface_height < height))
    {
      cairo_surface_destroy (context->indicator_surface);
      context->indicator_surface = NULL;
    }

  if (context->indicator_surface == NULL)
    context->indicator_surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32,
                                                                      width * scale,
                                                                      height * scale);

  if (cairo_surface_status (context->indicator_surface) != CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (context->indicator_surface);
      context->indicator_surface = NULL;

      return FALSE;
    }

  return TRUE;
}

/* Indicator is drawn with some inward offset, so that it does
 * not hug screen edges.
 */
static void
adjust_indicator_rectangle (GdkRectangle *rect,
                            gboolean      inward)
{
  gdouble inverter;
  const gint gap = AEROSNAP_INDICATOR_EDGE_GAP;
#if defined(MORE_AEROSNAP_DEBUGGING)
  GdkRectangle cache = *rect;
#endif

  if (inward)
    inverter = 1.0;
  else
    inverter = -1.0;

  rect->x += (gap * inverter);
  rect->y += (gap * inverter);
  rect->width -= (gap * 2 * inverter);
  rect->height -= (gap * 2 * inverter);

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Adjusted %d x %d @ %d : %d -> %d x %d @ %d : %d\n",
                           cache.width, cache.height, cache.x, cache.y,
                           rect->width, rect->height, rect->x, rect->y));
#endif
}

static void
rounded_rectangle (cairo_t  *cr,
                   gint      x,
                   gint      y,
                   gint      width,
                   gint      height,
                   gdouble   radius,
                   gdouble   line_width,
                   GdkRGBA  *fill,
                   GdkRGBA  *outline)
{
  gdouble degrees = M_PI / 180.0;

  if (fill == NULL && outline == NULL)
    return;

  cairo_save (cr);
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, (x + radius), y + height - radius, radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, (x + radius), (y + radius), radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);

  if (fill)
    {
      cairo_set_source_rgba (cr, fill->red, fill->green, fill->blue, fill->alpha);

      if (outline)
        cairo_fill_preserve (cr);
      else
        cairo_fill (cr);
    }

  if (outline)
    {
      cairo_set_source_rgba (cr, outline->red, outline->green, outline->blue, outline->alpha);
      cairo_set_line_width (cr, line_width);
      cairo_stroke (cr);
    }

  cairo_restore (cr);
}

/* Translates linear animation scale into some kind of curve */
static gdouble
curve (gdouble val)
{
  /* TODO: try different curves. For now it's just linear */
  return val;
}

static gboolean
draw_indicator (GdkW32DragMoveResizeContext *context,
                gint64                       timestamp)
{
  cairo_t *cr;
  GdkRGBA outline = {0, 0, 1.0, 1.0};
  GdkRGBA fill = {0, 0, 1.0, 0.8};
  GdkRectangle current_rect;
  gint64 current_time = g_get_monotonic_time ();
  gdouble animation_progress;
  gboolean last_draw;
  gdouble line_width;
  gdouble corner_radius;
  gint64 animation_duration;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (context->window);

  line_width = AEROSNAP_INDICATOR_LINE_WIDTH * impl->surface_scale;
  corner_radius = AEROSNAP_INDICATOR_CORNER_RADIUS;
  animation_duration = AEROSNAP_INDICATOR_ANIMATION_DURATION;
  last_draw = FALSE;

  if (timestamp == 0 &&
      current_time - context->indicator_start_time > animation_duration)
    {
      timestamp = context->indicator_start_time + animation_duration;
      last_draw = TRUE;
    }

  if (timestamp != 0)
    current_time = timestamp;

  animation_progress = (gdouble) (current_time - context->indicator_start_time) / animation_duration;

  if (animation_progress > 1.0)
    animation_progress = 1.0;

  if (animation_progress < 0)
    animation_progress = 0;

  animation_progress = curve (animation_progress);

  current_rect = context->indicator_start;
  current_rect.x += (context->indicator_target.x - context->indicator_start.x) * animation_progress;
  current_rect.y += (context->indicator_target.y - context->indicator_start.y) * animation_progress;
  current_rect.width += (context->indicator_target.width - context->indicator_start.width) * animation_progress;
  current_rect.height += (context->indicator_target.height - context->indicator_start.height) * animation_progress;

  if (context->op == GDK_WIN32_DRAGOP_RESIZE && last_draw)
    {
      switch (context->edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          current_rect.x = context->indicator_target.x + (context->indicator_target.width - current_rect.width);
          current_rect.y = context->indicator_target.y + (context->indicator_target.height - current_rect.height);
          break;
        case GDK_SURFACE_EDGE_NORTH:
          current_rect.y = context->indicator_target.y + (context->indicator_target.height - current_rect.height);
          break;
        case GDK_SURFACE_EDGE_WEST:
          current_rect.x = context->indicator_target.x + (context->indicator_target.width - current_rect.width);
          break;
        case GDK_SURFACE_EDGE_SOUTH_WEST:
          current_rect.x = context->indicator_target.x + (context->indicator_target.width - current_rect.width);
          current_rect.y = context->indicator_target.y;
          break;
        case GDK_SURFACE_EDGE_NORTH_EAST:
          current_rect.x = context->indicator_target.x;
          current_rect.y = context->indicator_target.y + (context->indicator_target.height - current_rect.height);
          break;
        case GDK_SURFACE_EDGE_SOUTH_EAST:
          current_rect.x = context->indicator_target.x;
          current_rect.y = context->indicator_target.y;
          break;
        case GDK_SURFACE_EDGE_SOUTH:
          current_rect.y = context->indicator_target.y;
          break;
        case GDK_SURFACE_EDGE_EAST:
          current_rect.x = context->indicator_target.x;
          break;
        }
    }

  cr = cairo_create (context->indicator_surface);
  rounded_rectangle (cr,
                     (current_rect.x - context->indicator_window_rect.x) * impl->surface_scale,
                     (current_rect.y - context->indicator_window_rect.y) * impl->surface_scale,
                     current_rect.width * impl->surface_scale,
                     current_rect.height * impl->surface_scale,
                     corner_radius,
                     line_width,
                     &fill, &outline);
  cairo_destroy (cr);

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Indicator is %d x %d @ %d : %d; current time is %" G_GINT64_FORMAT "\n",
                           current_rect.width, current_rect.height,
                           current_rect.x - context->indicator_window_rect.x,
                           current_rect.y - context->indicator_window_rect.y,
                           current_time));
#endif

  return last_draw;
}

static gboolean
redraw_indicator (gpointer user_data)
{
  GdkW32DragMoveResizeContext *context = user_data;
  POINT window_position;
  SIZE window_size;
  BLENDFUNCTION blender;
  HDC hdc;
  POINT source_point = { 0, 0 };
  gboolean last_draw;
  gdouble indicator_opacity;
  GdkWin32Surface *impl;
  gboolean do_source_remove = FALSE;

  indicator_opacity = AEROSNAP_INDICATOR_OPACITY;

  if (GDK_SURFACE_DESTROYED (context->window) ||
      !ensure_snap_indicator_exists (context))
    {
      do_source_remove = TRUE;
    }

  impl = GDK_WIN32_SURFACE (context->window);

  if (!ensure_snap_indicator_surface (context,
                                      context->indicator_window_rect.width,
                                      context->indicator_window_rect.height,
                                      impl->surface_scale))
    {
      do_source_remove = TRUE;
    }

  if (do_source_remove)
    {
      context->timer = 0;
      return G_SOURCE_REMOVE;
    }

  last_draw = draw_indicator (context, context->draw_timestamp);

  window_position.x = (context->indicator_window_rect.x - _gdk_offset_x) * impl->surface_scale;
  window_position.y = (context->indicator_window_rect.y - _gdk_offset_y) * impl->surface_scale;
  window_size.cx = context->indicator_window_rect.width * impl->surface_scale;
  window_size.cy = context->indicator_window_rect.height * impl->surface_scale;

  blender.BlendOp = AC_SRC_OVER;
  blender.BlendFlags = 0;
  blender.AlphaFormat = AC_SRC_ALPHA;
  blender.SourceConstantAlpha = 255 * indicator_opacity;

  hdc = cairo_win32_surface_get_dc (context->indicator_surface);

  API_CALL (SetWindowPos, (context->shape_indicator,
                           GDK_SURFACE_HWND (context->window),
                           0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_SHOWWINDOW | SWP_NOACTIVATE));

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Indicator window position is %ld x %ld @ %ld : %ld\n",
                           window_size.cx, window_size.cy,
                           window_position.x, window_position.y));
#endif

  API_CALL (UpdateLayeredWindow, (context->shape_indicator, NULL,
                                  &window_position, &window_size,
                                  hdc, &source_point,
                                  0, &blender, ULW_ALPHA));

  if (last_draw)
    context->timer = 0;

  return last_draw ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
}

static GdkRectangle
unity_of_rects (GdkRectangle a,
                GdkRectangle b)
{
  GdkRectangle u = b;

  if (a.x < u.x)
    {
      u.width += u.x - a.x;
      u.x = a.x;
    }

  if (a.y < u.y)
    {
      u.height += (u.y - a.y);
      u.y = a.y;
    }

  if (a.x + a.width > u.x + u.width)
    u.width += (a.x + a.width) - (u.x + u.width);

  if (a.y + a.height > u.y + u.height)
    u.height += (a.y + a.height) - (u.y + u.height);

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Unified 2 rects into %d x %d @ %d : %d\n",
                           u.width, u.height, u.x, u.y));
#endif

  return u;
}

static void
start_indicator_drawing (GdkW32DragMoveResizeContext *context,
                         GdkRectangle                 from,
                         GdkRectangle                 to,
                         guint                        scale)
{
  GdkRectangle to_adjusted, from_adjusted, from_or_to;
  gint64 indicator_animation_tick = AEROSNAP_INDICATOR_ANIMATION_TICK;

  GDK_NOTE (MISC, g_print ("Start drawing snap indicator %d x %d @ %d : %d -> %d x %d @ %d : %d\n",
                           from.width * scale, from.height * scale, from.x, from.y, to.width * scale, to.height * scale, to.x, to.y));

  if (GDK_SURFACE_DESTROYED (context->window))
    return;

  if (!ensure_snap_indicator_exists (context))
    return;

  from_or_to = unity_of_rects (from, to);

  if (!ensure_snap_indicator_surface (context, from_or_to.width, from_or_to.height, scale))
    return;

  to_adjusted = to;
  adjust_indicator_rectangle (&to_adjusted, TRUE);

  from_adjusted = from;
  adjust_indicator_rectangle (&from_adjusted, TRUE);

  context->draw_timestamp = 0;
  context->indicator_start = from_adjusted;
  context->indicator_target = to_adjusted;
  context->indicator_window_rect = from_or_to;
  context->indicator_start_time = g_get_monotonic_time ();

  if (context->timer)
    {
      g_source_remove (context->timer);
      context->timer = 0;
    }

  context->timer = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                       indicator_animation_tick,
                                       redraw_indicator,
                                       context,
                                       NULL);
}

static void
update_fullup_indicator (GdkSurface                   *window,
                         GdkW32DragMoveResizeContext *context)
{
  SHORT maxysize;
  GdkRectangle from, to;
  GdkRectangle to_adjusted, from_adjusted, from_or_to;
  GdkWin32Surface *impl;

  GDK_NOTE (MISC, g_print ("Update fullup indicator\n"));

  if (GDK_SURFACE_DESTROYED (context->window))
    return;

  if (context->shape_indicator == NULL)
    return;

  impl = GDK_WIN32_SURFACE (window);
  maxysize = GetSystemMetrics (SM_CYVIRTUALSCREEN);
  gdk_surface_get_position (window, &to.x, &to.y);
  to.width = gdk_surface_get_width (window);
  to.height = gdk_surface_get_height (window);

  to.y = 0;
  to.height = maxysize;
  from = context->indicator_target;

  if (context->timer == 0)
    {
      from_adjusted = from;
      adjust_indicator_rectangle (&from_adjusted, FALSE);

      GDK_NOTE (MISC, g_print ("Restart fullup animation from %d x %d @ %d : %d -> %d x %d @ %d x %d\n",
                               context->indicator_target.width, context->indicator_target.height,
                               context->indicator_target.x, context->indicator_target.y,
                               to.width, to.height, to.x, to.y));
      start_indicator_drawing (context, from_adjusted, to, impl->surface_scale);

      return;
    }

  from_or_to = unity_of_rects (from, to);

  to_adjusted = to;
  adjust_indicator_rectangle (&to_adjusted, TRUE);

  GDK_NOTE (MISC, g_print ("Retarget fullup animation %d x %d @ %d : %d -> %d x %d @ %d x %d\n",
                           context->indicator_target.width, context->indicator_target.height,
                           context->indicator_target.x, context->indicator_target.y,
                           to_adjusted.width, to_adjusted.height, to_adjusted.x, to_adjusted.y));

  context->indicator_target = to_adjusted;
  context->indicator_window_rect = from_or_to;

  ensure_snap_indicator_surface (context, from_or_to.width, from_or_to.height, impl->surface_scale);
}

static GdkMonitor *
get_monitor_at_point (GdkDisplay *display,
                      int         x,
                      int         y)
{
  GdkMonitor *nearest = NULL;
  int nearest_dist = G_MAXINT;
  int n_monitors, i;

  n_monitors = gdk_display_get_n_monitors (display);
  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;
      int dist_x, dist_y, dist;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &geometry);

      if (x < geometry.x)
        dist_x = geometry.x - x;
      else if (geometry.x + geometry.width <= x)
        dist_x = x - (geometry.x + geometry.width) + 1;
      else
        dist_x = 0;

      if (y < geometry.y)
        dist_y = geometry.y - y;
      else if (geometry.y + geometry.height <= y)
        dist_y = y - (geometry.y + geometry.height) + 1;
      else
        dist_y = 0;

      dist = dist_x + dist_y;
      if (dist < nearest_dist)
        {
          nearest_dist = dist;
          nearest = monitor;
        }

      if (x < geometry.x)
        dist_x = geometry.x - x;
      else if (geometry.x + geometry.width <= x)
        dist_x = x - (geometry.x + geometry.width) + 1;
      else
        dist_x = 0;

      if (y < geometry.y)
        dist_y = geometry.y - y;
      else if (geometry.y + geometry.height <= y)
        dist_y = y - (geometry.y + geometry.height) + 1;
      else
        dist_y = 0;

      dist = dist_x + dist_y;
      if (dist < nearest_dist)
        {
          nearest_dist = dist;
          nearest = monitor;
        }

      if (nearest_dist == 0)
        break;
    }

  return nearest;
}

static void
start_indicator (GdkSurface                   *window,
                 GdkW32DragMoveResizeContext *context,
                 gint                         x,
                 gint                         y,
                 GdkWin32AeroSnapState        state)
{
  GdkMonitor *monitor;
  GdkRectangle workarea;
  SHORT maxysize;
  GdkRectangle start_size, end_size;
  GdkDisplay *display;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  display = gdk_surface_get_display (window);
  monitor = get_monitor_at_point (display, x, y);
  gdk_monitor_get_workarea (monitor, &workarea);

  maxysize = GetSystemMetrics (SM_CYVIRTUALSCREEN) / impl->surface_scale;
  gdk_surface_get_position (window, &start_size.x, &start_size.y);
  start_size.width = gdk_surface_get_width (window);
  start_size.height = gdk_surface_get_height (window);

  end_size = start_size;

  switch (state)
    {
    case GDK_WIN32_AEROSNAP_STATE_UNDETERMINED:
      return;
    case GDK_WIN32_AEROSNAP_STATE_MAXIMIZE:
      end_size.x = workarea.x;
      end_size.y = workarea.y;
      end_size.width = workarea.width;
      end_size.height = workarea.height;
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFLEFT:
      end_size.x = workarea.x;
      end_size.y = workarea.y;
      end_size.width = workarea.width / 2;
      end_size.height = workarea.height;
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFRIGHT:
      end_size.x = (workarea.x + workarea.width / 2);
      end_size.y = workarea.y;
      end_size.width = workarea.width / 2;
      end_size.height = workarea.height;
      break;
    case GDK_WIN32_AEROSNAP_STATE_FULLUP:
      end_size.y = 0;
      end_size.height = maxysize;
      break;
    }

  start_indicator_drawing (context, start_size, end_size, impl->surface_scale);
}

static void
stop_indicator (GdkSurface                   *window,
                GdkW32DragMoveResizeContext *context)
{
  GDK_NOTE (MISC, g_print ("Stop drawing snap indicator\n"));

  if (context->timer)
    {
      g_source_remove (context->timer);
      context->timer = 0;
    }

  API_CALL (SetWindowPos, (context->shape_indicator,
                           SWP_NOZORDER_SPECIFIED,
                           0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE |
                           SWP_NOSIZE | SWP_NOREDRAW | SWP_HIDEWINDOW | SWP_NOACTIVATE));
}

static gint
point_in_aerosnap_region (gint                x,
                          gint                y,
                          AeroSnapEdgeRegion *region)
{
  gint edge, trigger;

  edge = (x >= region->edge.x &&
          y >= region->edge.y &&
          x <= region->edge.x + region->edge.width &&
          y <= region->edge.y + region->edge.height) ? 1 : 0;
  trigger = (x >= region->trigger.x &&
             y >= region->trigger.y &&
             x <= region->trigger.x + region->trigger.width &&
             y <= region->trigger.y + region->trigger.height) ? 1 : 0;
  return edge + trigger;
}

static void
handle_aerosnap_move_resize (GdkSurface                   *window,
                             GdkW32DragMoveResizeContext *context,
                             gint                         x,
                             gint                         y)
{
  gint i;
  AeroSnapEdgeRegion *reg;
  gint maximize = 0;
  gint halfleft = 0;
  gint halfright = 0;
  gint fullup = 0;
  gboolean fullup_edge = FALSE;

  if (context->op == GDK_WIN32_DRAGOP_RESIZE)
    switch (context->edge)
      {
      case GDK_SURFACE_EDGE_NORTH_WEST:
      case GDK_SURFACE_EDGE_NORTH_EAST:
      case GDK_SURFACE_EDGE_WEST:
      case GDK_SURFACE_EDGE_EAST:
      case GDK_SURFACE_EDGE_SOUTH_WEST:
      case GDK_SURFACE_EDGE_SOUTH_EAST:
        break;
      case GDK_SURFACE_EDGE_SOUTH:
      case GDK_SURFACE_EDGE_NORTH:
        fullup_edge = TRUE;
        break;
      }

  for (i = 0; i < context->maximize_regions->len && maximize == 0; i++)
    {
      reg = &g_array_index (context->maximize_regions, AeroSnapEdgeRegion, i);
      maximize = point_in_aerosnap_region (x, y, reg);
    }

  for (i = 0; i < context->halfleft_regions->len && halfleft == 0; i++)
    {
      reg = &g_array_index (context->halfleft_regions, AeroSnapEdgeRegion, i);
      halfleft = point_in_aerosnap_region (x, y, reg);
    }

  for (i = 0; i < context->halfright_regions->len && halfright == 0; i++)
    {
      reg = &g_array_index (context->halfright_regions, AeroSnapEdgeRegion, i);
      halfright = point_in_aerosnap_region (x, y, reg);
    }

  for (i = 0; i < context->fullup_regions->len && fullup == 0; i++)
    {
      reg = &g_array_index (context->fullup_regions, AeroSnapEdgeRegion, i);
      fullup = point_in_aerosnap_region (x, y, reg);
    }

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("AeroSnap: point %d : %d - max: %d, left %d, right %d, up %d\n",
                           x, y, maximize, halfleft, halfright, fullup));
#endif

  if (!context->revealed)
    {
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize == 2)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_MAXIMIZE;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft == 2)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright == 2)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_RESIZE && fullup == 2 && fullup_edge)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_FULLUP;
          start_indicator (window, context, x, y, context->current_snap);
        }

      return;
    }

  switch (context->current_snap)
    {
    case GDK_WIN32_AEROSNAP_STATE_UNDETERMINED:
      if (context->op == GDK_WIN32_DRAGOP_RESIZE && fullup > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_FULLUP;
          start_indicator (window, context, x, y, context->current_snap);
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_MAXIMIZE:
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize > 0)
        break;
      if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
          stop_indicator (window, context);
          context->revealed = FALSE;
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFLEFT:
      if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft > 0)
        break;
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_MAXIMIZE;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
          stop_indicator (window, context);
          context->revealed = FALSE;
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFRIGHT:
      if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright > 0)
        break;
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_MAXIMIZE;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
          stop_indicator (window, context);
          context->revealed = FALSE;
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_FULLUP:
      if (context->op == GDK_WIN32_DRAGOP_RESIZE && fullup > 0 && fullup_edge)
        {
          update_fullup_indicator (window, context);
          break;
        }

      context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
      stop_indicator (window, context);
      break;
    }
}


static const gchar *
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
      g_assert_not_reached ();
    case GDK_WIN32_DRAGOP_NONE:
      return "default";
    /* default: warn about unhandled enum values */
    }

  g_assert_not_reached ();

  return NULL;
}

static void
setup_drag_move_resize_context (GdkSurface                   *window,
                                GdkW32DragMoveResizeContext *context,
                                GdkW32WindowDragOp           op,
                                GdkSurfaceEdge                edge,
                                GdkDevice                   *device,
                                gint                         button,
                                gint                         x,
                                gint                         y,
                                guint32                      timestamp)
{
  RECT rect;
  const gchar *cursor_name;
  GdkSurface *pointer_window;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  gboolean maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;
  gint root_x, root_y;

  gdk_win32_surface_get_root_coords (window, x, y, &root_x, &root_y);

  /* Before we drag, we need to undo any maximization or snapping.
   * AeroSnap behaviour:
   *   If snapped halfleft/halfright:
   *     horizontal resize:
   *       resize
   *       don't unsnap
   *       keep stashed unsnapped size intact
   *     vertical resize:
   *       resize
   *       unsnap to new size (merge cached unsnapped state with current
   *                           snapped state in such a way that the gripped edge
   *                           does not move)
   *     diagonal resize:
   *       difficult to test (first move is usually either purely
   *                          horizontal or purely vertical, in which
   *                          case the above behaviour applies)
   *   If snapped up:
   *     horizontal resize:
   *       resize
   *       don't unsnap
   *       apply new width and x position to unsnapped cache,
   *         so that unsnapped window only regains its height
   *         and y position, but inherits x and width from
   *         the fullup snapped state
   *     vertical resize:
   *       unsnap to new size (merge cached unsnapped state with current
   *                           snapped state in such a way that the gripped edge
   *                           does not move)
   *
   * This implementation behaviour:
   *   If snapped halfleft/halfright/fullup:
   *     any resize:
   *       unsnap to current size, discard cached pre-snap state
   *
   * TODO: make this implementation behave as AeroSnap on resizes?
   * There's also the case where
   * a halfleft/halfright window isn't unsnapped when it's
   * being moved horizontally, but it's more difficult to implement.
   */
  if (op == GDK_WIN32_DRAGOP_RESIZE &&
      (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT ||
       impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT ||
       impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP))
    {
      discard_snapinfo (window);
    }
  else if (maximized ||
           (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT ||
            impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT ||
            impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP))
    {
      GdkMonitor *monitor;
      gint wx, wy, wwidth, wheight;
      gint swx, swy, swwidth, swheight;
      gboolean pointer_outside_of_window;
      gint offsetx, offsety;
      gboolean left_half;
      GdkDisplay *display;

      display = gdk_surface_get_display (window);
      monitor = gdk_display_get_monitor_at_surface (display, window);
      gdk_surface_get_geometry (window, &wx, &wy, &wwidth, &wheight);

      swx = wx;
      swy = wy;
      swwidth = wwidth;
      swheight = wheight;

      /* Subtract window shadow. We don't want pointer to go outside of
       * the visible window during drag-move. For drag-resize it's OK.
       * Don't take shadow into account if the window is maximized -
       * maximized windows don't have shadows.
       */
      if (op == GDK_WIN32_DRAGOP_MOVE && !maximized)
        {
          swx += impl->margins.left / impl->surface_scale;
          swy += impl->margins.top / impl->surface_scale;
          swwidth -= impl->margins_x;
          swheight -= impl->margins_y;
        }

      pointer_outside_of_window = root_x < swx || root_x > swx + swwidth ||
                                  root_y < swy || root_y > swy + swheight;
      /* Calculate the offset of the pointer relative to the window */
      offsetx = root_x - swx;
      offsety = root_y - swy;

      /* Figure out in which half of the window the pointer is.
       * The code currently only concerns itself with horizontal
       * dimension (left/right halves).
       * There's no upper/lower half, because usually window
       * is dragged by its upper half anyway. If that changes, adjust
       * accordingly.
       */
      left_half = (offsetx < swwidth / 2);

      /* Inverse the offset for it to be from the right edge */
      if (!left_half)
        offsetx = swwidth - offsetx;

      GDK_NOTE (MISC, g_print ("Pointer at %d : %d, this is %d : %d relative to the window's %s\n",
                               root_x, root_y, offsetx, offsety,
                               left_half ? "left half" : "right half"));

      /* Move window in such a way that on unmaximization/unsnapping the pointer
       * is still pointing at the appropriate half of the window,
       * with the same offset from the left or right edge. If the new
       * window size is too small, and adding that offset puts the pointer
       * into the other half or even beyond, move the pointer to the middle.
       */
      if (!pointer_outside_of_window && maximized)
        {
          WINDOWPLACEMENT placement;
          gint unmax_width, unmax_height;
          gint shadow_unmax_width, shadow_unmax_height;

          placement.length = sizeof (placement);
          API_CALL (GetWindowPlacement, (GDK_SURFACE_HWND (window), &placement));

          GDK_NOTE (MISC, g_print ("W32 WM unmaximized window placement is %ld x %ld @ %ld : %ld\n",
                                   placement.rcNormalPosition.right - placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.bottom - placement.rcNormalPosition.top,
                                   placement.rcNormalPosition.left + _gdk_offset_x * impl->surface_scale,
                                   placement.rcNormalPosition.top + _gdk_offset_y * impl->surface_scale));

          unmax_width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
          unmax_height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;

          shadow_unmax_width = unmax_width - impl->margins_x * impl->surface_scale;
          shadow_unmax_height = unmax_height - impl->margins_y * impl->surface_scale;

          if (offsetx * impl->surface_scale < (shadow_unmax_width / 2) &&
              offsety * impl->surface_scale < (shadow_unmax_height / 2))
            {
              placement.rcNormalPosition.top = (root_y - offsety + impl->margins.top - _gdk_offset_y) * impl->surface_scale;
              placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + unmax_height;

              if (left_half)
                {
                  placement.rcNormalPosition.left = (root_x - offsetx + impl->margins.left - _gdk_offset_x) * impl->surface_scale;
                  placement.rcNormalPosition.right = placement.rcNormalPosition.left + unmax_width;
                }
              else
                {
                  placement.rcNormalPosition.right = (root_x + offsetx + impl->margins.right - _gdk_offset_x) * impl->surface_scale;
                  placement.rcNormalPosition.left = placement.rcNormalPosition.right - unmax_width;
                }
            }
          else
            {
              placement.rcNormalPosition.left = (root_x * impl->surface_scale) -
                                                (unmax_width / 2) -
                                                (_gdk_offset_x * impl->surface_scale);

              if (offsety * impl->surface_scale < shadow_unmax_height / 2)
                placement.rcNormalPosition.top = (root_y - offsety + impl->margins.top - _gdk_offset_y) * impl->surface_scale;
              else
                placement.rcNormalPosition.top = (root_y * impl->surface_scale) -
                                                 (unmax_height / 2) -
                                                 (_gdk_offset_y * impl->surface_scale);

              placement.rcNormalPosition.right = placement.rcNormalPosition.left + unmax_width;
              placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + unmax_height;
            }

          GDK_NOTE (MISC, g_print ("Unmaximized window will be at %ld : %ld\n",
                                   placement.rcNormalPosition.left + _gdk_offset_x * impl->surface_scale,
                                   placement.rcNormalPosition.top + _gdk_offset_y * impl->surface_scale));

          API_CALL (SetWindowPlacement, (GDK_SURFACE_HWND (window), &placement));
        }
      else if (!pointer_outside_of_window && impl->snap_stash_int)
        {
          GdkRectangle new_pos;
          GdkRectangle snew_pos;

          new_pos.width = impl->snap_stash_int->width;
          new_pos.height = impl->snap_stash_int->height;
          snew_pos = new_pos;

          if (op == GDK_WIN32_DRAGOP_MOVE)
            {
              snew_pos.width -= impl->margins_x;
              snew_pos.height -= impl->margins_y;
            }

          if (offsetx < snew_pos.width / 2 && offsety < snew_pos.height / 2)
            {
              new_pos.y = root_y - offsety + impl->margins.top / impl->surface_scale;

              if (left_half)
                new_pos.x = root_x - offsetx + impl->margins.left / impl->surface_scale;
              else
                new_pos.x = root_x + offsetx + impl->margins.left / impl->surface_scale - new_pos.width;
            }
          else
            {
              new_pos.x = root_x - new_pos.width / 2;
              new_pos.y = root_y - new_pos.height / 2;
            }

          GDK_NOTE (MISC, g_print ("Unsnapped window to %d : %d\n",
                                   new_pos.x, new_pos.y));
          discard_snapinfo (window);
          gdk_win32_surface_move_resize (window, new_pos.x, new_pos.y,
                                         new_pos.width, new_pos.height);
        }


      if (maximized)
        gdk_surface_unmaximize (window);
      else
        unsnap (window, monitor);

      if (pointer_outside_of_window)
        {
          /* Pointer outside of the window, move pointer into window */
          GDK_NOTE (MISC, g_print ("Pointer at %d : %d is outside of %d x %d @ %d : %d, move it to %d : %d\n",
                                   root_x, root_y, wwidth, wheight, wx, wy, wx + wwidth / 2, wy + wheight / 2));
          root_x = wx + wwidth / 2;
          /* This is Gnome behaviour. Windows WM would put the pointer
           * in the middle of the titlebar, but GDK doesn't know where
           * the titlebar is, if any.
           */
          root_y = wy + wheight / 2;
          SetCursorPos (root_x - _gdk_offset_x, root_y - _gdk_offset_y);
        }
    }

  _gdk_win32_get_window_rect (window, &rect);

  cursor_name = get_cursor_name_from_op (op, edge);

  context->cursor = gdk_cursor_new_from_name (cursor_name, NULL);

  pointer_window = window;

  /* Note: This triggers a WM_CAPTURECHANGED, which will trigger
   * gdk_win32_surface_end_move_resize_drag(), which will end
   * our op before it even begins, but only if context->op is not NONE.
   * This is why we first do the grab, *then* set the op.
   */
  gdk_device_grab (device, pointer_window,
                   GDK_OWNERSHIP_NONE, FALSE,
                   GDK_ALL_EVENTS_MASK,
                   context->cursor,
                   timestamp);

  context->window = g_object_ref (window);
  context->op = op;
  context->edge = edge;
  context->device = device;
  context->button = button;
  context->start_root_x = root_x;
  context->start_root_y = root_y;
  context->timestamp = timestamp;
  context->start_rect = rect;

  context->shape_indicator = NULL;
  context->revealed = FALSE;
  context->halfleft_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));
  context->halfright_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));
  context->maximize_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));
  context->fullup_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));

  calculate_aerosnap_regions (context);

  GDK_NOTE (EVENTS,
            g_print ("begin drag moveresize: window %p, toplevel %p, "
                     "op %u, edge %d, device %p, "
                     "button %d, coord %d:%d, time %u\n",
                     pointer_window, window,
                     context->op, context->edge, context->device,
                     context->button, context->start_root_x,
                     context->start_root_y, context->timestamp));
}

void
gdk_win32_surface_end_move_resize_drag (GdkSurface *window)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  GdkW32DragMoveResizeContext *context = &impl->drag_move_resize_context;

  context->op = GDK_WIN32_DRAGOP_NONE;

  gdk_device_ungrab (context->device, GDK_CURRENT_TIME);

  g_clear_object (&context->cursor);

  context->revealed = FALSE;

  if (context->timer)
    {
      g_source_remove (context->timer);
      context->timer = 0;
    }

  g_clear_object (&context->window);

  if (context->indicator_surface)
    {
      cairo_surface_destroy (context->indicator_surface);
      context->indicator_surface = NULL;
    }

  if (context->shape_indicator)
    {
      stop_indicator (window, context);
      DestroyWindow (context->shape_indicator);
      context->shape_indicator = NULL;
    }

  g_clear_pointer (&context->halfleft_regions, g_array_unref);
  g_clear_pointer (&context->halfright_regions, g_array_unref);
  g_clear_pointer (&context->maximize_regions, g_array_unref);
  g_clear_pointer (&context->fullup_regions, g_array_unref);

  GDK_NOTE (EVENTS,
            g_print ("end drag moveresize: window %p, toplevel %p,"
                     "op %u, edge %d, device %p, "
                     "button %d, coord %d:%d, time %u\n",
                     window, window,
                     context->op, context->edge, context->device,
                     context->button, context->start_root_x,
                     context->start_root_y, context->timestamp));

  if (context->current_snap != GDK_WIN32_AEROSNAP_STATE_UNDETERMINED)
    apply_snap (window, context->current_snap);

  context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
}

static void
gdk_win32_get_window_size_and_position_from_client_rect (GdkSurface *window,
                                                         RECT      *window_rect,
                                                         SIZE      *window_size,
                                                         POINT     *window_position)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  /* Turn client area into window area */
  _gdk_win32_adjust_client_rect (window, window_rect);

  /* Convert GDK screen coordinates to W32 desktop coordinates */
  window_rect->left -= _gdk_offset_x * impl->surface_scale;
  window_rect->right -= _gdk_offset_x * impl->surface_scale;
  window_rect->top -= _gdk_offset_y * impl->surface_scale;
  window_rect->bottom -= _gdk_offset_y * impl->surface_scale;

  window_position->x = window_rect->left;
  window_position->y = window_rect->top;
  window_size->cx = window_rect->right - window_rect->left;
  window_size->cy = window_rect->bottom - window_rect->top;
}

void
_gdk_win32_update_layered_window_from_cache (GdkSurface *surface,
                                             RECT       *client_rect,
                                             gboolean    do_move,
                                             gboolean    do_resize,
                                             gboolean    do_paint)
{
  POINT window_position;
  SIZE window_size;
  BLENDFUNCTION blender;
  HDC hdc;
  SIZE *window_size_ptr;
  POINT source_point = { 0, 0 };
  POINT *source_point_ptr;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  gdk_win32_get_window_size_and_position_from_client_rect (surface,
                                                           client_rect,
                                                           &window_size,
                                                           &window_position);

  blender.BlendOp = AC_SRC_OVER;
  blender.BlendFlags = 0;
  blender.AlphaFormat = AC_SRC_ALPHA;
  blender.SourceConstantAlpha = impl->layered_opacity * 255;

  /* Strictly speaking, we don't need to supply hdc, source_point and
   * window_size to just move the window. However, without these arguments
   * the window moves but does not update its contents on Windows 7 when
   * desktop composition is off. This forces us to provide hdc and
   * source_point. window_size is here to avoid the function
   * inexplicably failing with error 317.
   */
  hdc = cairo_win32_surface_get_dc (impl->cache_surface);
  window_size_ptr = &window_size;
  source_point_ptr = &source_point;

  if (gdk_display_is_composited (gdk_surface_get_display (surface)))
    {
      if (!do_paint)
        hdc = NULL;
      if (!do_resize)
        window_size_ptr = NULL;
      if (!do_move)
        source_point_ptr = NULL;
    }

  /* Don't use UpdateLayeredWindow on minimized windows */
  if (IsIconic (GDK_SURFACE_HWND (surface)))
    API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                             SWP_NOZORDER_SPECIFIED,
                             window_position.x, window_position.y,
                             window_size.cx, window_size.cy,
                             SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW));
  else
    API_CALL (UpdateLayeredWindow, (GDK_SURFACE_HWND (surface), NULL,
                                    &window_position, window_size_ptr,
                                    hdc, source_point_ptr,
                                    0, &blender, ULW_ALPHA));
}

void
gdk_win32_surface_do_move_resize_drag (GdkSurface *window,
                                      gint       x,
                                      gint       y)
{
  RECT rect;
  RECT new_rect;
  gint diffy, diffx;
  MINMAXINFO mmi;
  GdkWin32Surface *impl;
  GdkW32DragMoveResizeContext *context;
  gint width;
  gint height;

  impl = GDK_WIN32_SURFACE (window);
  context = &impl->drag_move_resize_context;

  if (!_gdk_win32_get_window_rect (window, &rect))
    return;

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

      if (!_gdk_win32_surface_fill_min_max_info (window, &mmi))
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
      context->native_move_resize_pending = TRUE;
      _gdk_win32_do_emit_configure_event (window, new_rect);
    }
  else if (context->op == GDK_WIN32_DRAGOP_MOVE &&
           (rect.left != new_rect.left ||
            rect.top != new_rect.top))
    {
      context->native_move_resize_pending = FALSE;

      _gdk_win32_do_emit_configure_event (window, new_rect);

      if (impl->layered)
        {
          _gdk_win32_update_layered_window_from_cache (window, &new_rect, TRUE, FALSE, FALSE);
        }
      else
        {
          SIZE window_size;
          POINT window_position;

          gdk_win32_get_window_size_and_position_from_client_rect (window,
                                                                   &new_rect,
                                                                   &window_size,
                                                                   &window_position);

          API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                                   SWP_NOZORDER_SPECIFIED,
                                   window_position.x, window_position.y,
                                   0, 0,
                                   SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE));
        }
    }

  if (context->op == GDK_WIN32_DRAGOP_RESIZE ||
      context->op == GDK_WIN32_DRAGOP_MOVE)
    handle_aerosnap_move_resize (window, context, x, y);
}

static void
gdk_win32_surface_begin_resize_drag (GdkSurface     *window,
                                    GdkSurfaceEdge  edge,
                                    GdkDevice     *device,
                                    gint           button,
                                    gint           x,
                                    gint           y,
                                    guint32        timestamp)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      IsIconic (GDK_SURFACE_HWND (window)))
    return;

  /* Tell Windows to start interactively resizing the window by pretending that
   * the left pointer button was clicked in the suitable edge or corner. This
   * will only work if the button is down when this function is called, and
   * will only work with button 1 (left), since Windows only allows window
   * dragging using the left mouse button.
   */

  if (button != 1)
    return;

  impl = GDK_WIN32_SURFACE (window);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_end_move_resize_drag (window);

  setup_drag_move_resize_context (window, &impl->drag_move_resize_context,
                                  GDK_WIN32_DRAGOP_RESIZE, edge, device,
                                  button, x, y, timestamp);
}

static void
gdk_win32_surface_begin_move_drag (GdkSurface *window,
                                  GdkDevice *device,
                                  gint       button,
                                  gint       x,
                                  gint       y,
                                  guint32    timestamp)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      IsIconic (GDK_SURFACE_HWND (window)))
    return;

  /* Tell Windows to start interactively moving the window by pretending that
   * the left pointer button was clicked in the titlebar. This will only work
   * if the button is down when this function is called, and will only work
   * with button 1 (left), since Windows only allows window dragging using the
   * left mouse button.
   */
  if (button != 1)
    return;

  impl = GDK_WIN32_SURFACE (window);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_end_move_resize_drag (window);

  setup_drag_move_resize_context (window, &impl->drag_move_resize_context,
                                  GDK_WIN32_DRAGOP_MOVE, GDK_SURFACE_EDGE_NORTH_WEST,
                                  device, button, x, y, timestamp);
}


/*
 * Setting window states
 */
static void
gdk_win32_surface_iconify (GdkSurface *window)
{
  HWND old_active_window;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_iconify: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      old_active_window = GetActiveWindow ();
      GtkShowWindow (window, SW_MINIMIZE);
      if (old_active_window != GDK_SURFACE_HWND (window))
	SetActiveWindow (old_active_window);
    }
  else
    {
      gdk_synthesize_surface_state (window,
                                   0,
                                   GDK_SURFACE_STATE_ICONIFIED);
    }
}

static void
gdk_win32_surface_deiconify (GdkSurface *window)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_deiconify: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      show_window_internal (window, GDK_SURFACE_IS_MAPPED (window), TRUE);
    }
  else
    {
      gdk_synthesize_surface_state (window,
                                   GDK_SURFACE_STATE_ICONIFIED,
                                   0);
    }
}

static void
gdk_win32_surface_stick (GdkSurface *window)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

static void
gdk_win32_surface_unstick (GdkSurface *window)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

static void
gdk_win32_surface_maximize (GdkSurface *window)
{

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_maximize: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    GtkShowWindow (window, SW_MAXIMIZE);
  else
    gdk_synthesize_surface_state (window,
				 0,
				 GDK_SURFACE_STATE_MAXIMIZED);
}

static void
gdk_win32_surface_unmaximize (GdkSurface *window)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_unmaximize: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    GtkShowWindow (window, SW_RESTORE);
  else
    gdk_synthesize_surface_state (window,
				 GDK_SURFACE_STATE_MAXIMIZED,
				 0);
}

static void
gdk_win32_surface_fullscreen (GdkSurface *window)
{
  gint x, y, width, height;
  FullscreenInfo *fi;
  HMONITOR monitor;
  MONITORINFO mi;

  g_return_if_fail (GDK_IS_SURFACE (window));

  fi = g_new (FullscreenInfo, 1);

  if (!GetWindowRect (GDK_SURFACE_HWND (window), &(fi->r)))
    g_free (fi);
  else
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      monitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
      mi.cbSize = sizeof (mi);
      if (monitor && GetMonitorInfo (monitor, &mi))
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
      g_object_set_data (G_OBJECT (window), "fullscreen-info", fi);
      fi->style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);

      /* Send state change before configure event */
      gdk_synthesize_surface_state (window, 0, GDK_SURFACE_STATE_FULLSCREEN);

      SetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE,
                     (fi->style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_TOP,
                x, y, width, height,
                SWP_NOCOPYBITS | SWP_SHOWWINDOW | SWP_NOOWNERZORDER));
    }
}

static void
gdk_win32_surface_unfullscreen (GdkSurface *window)
{
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_SURFACE (window));

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_FULLSCREEN, 0);

      impl->hint_flags = fi->hint_flags;
      SetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE, fi->style);
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_NOTOPMOST,
			       fi->r.left, fi->r.top,
			       fi->r.right - fi->r.left, fi->r.bottom - fi->r.top,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW | SWP_NOOWNERZORDER));

      g_object_set_data (G_OBJECT (window), "fullscreen-info", NULL);
      g_free (fi);
      _gdk_win32_surface_update_style_bits (window);
    }
}

static void
gdk_win32_surface_set_keep_above (GdkSurface *window,
			   gboolean   setting)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_keep_above: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   setting ? "YES" : "NO"));

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			       setting ? HWND_TOPMOST : HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER));
    }

  gdk_synthesize_surface_state (window,
			       setting ? GDK_SURFACE_STATE_BELOW : GDK_SURFACE_STATE_ABOVE,
			       setting ? GDK_SURFACE_STATE_ABOVE : 0);
}

static void
gdk_win32_surface_set_keep_below (GdkSurface *window,
			   gboolean   setting)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_keep_below: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   setting ? "YES" : "NO"));

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			       setting ? HWND_BOTTOM : HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER));
    }

  gdk_synthesize_surface_state (window,
			       setting ? GDK_SURFACE_STATE_ABOVE : GDK_SURFACE_STATE_BELOW,
			       setting ? GDK_SURFACE_STATE_BELOW : 0);
}

static void
gdk_win32_surface_focus (GdkSurface *window,
			guint32    timestamp)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_focus: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (window->state & GDK_SURFACE_STATE_MAXIMIZED)
    GtkShowWindow (window, SW_SHOWMAXIMIZED);
  else if (window->state & GDK_SURFACE_STATE_ICONIFIED)
    GtkShowWindow (window, SW_RESTORE);
  else if (!IsWindowVisible (GDK_SURFACE_HWND (window)))
    GtkShowWindow (window, SW_SHOWNORMAL);
  else
    GtkShowWindow (window, SW_SHOW);

  SetFocus (GDK_SURFACE_HWND (window));
}

static void
gdk_win32_surface_set_modal_hint (GdkSurface *window,
			   gboolean   modal)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_modal_hint: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   modal ? "YES" : "NO"));

  if (modal == window->modal_hint)
    return;

  window->modal_hint = modal;

#if 0
  /* Not sure about this one.. -- Cody */
  if (GDK_SURFACE_IS_MAPPED (window))
    API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			     modal ? HWND_TOPMOST : HWND_NOTOPMOST,
			     0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE));
#else

  if (modal)
    {
      _gdk_push_modal_window (window);
      gdk_surface_raise (window);
    }
  else
    {
      _gdk_remove_modal_window (window);
    }

#endif
}

static void
gdk_win32_surface_set_type_hint (GdkSurface        *window,
			  GdkSurfaceTypeHint hint)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC,
	    G_STMT_START{
	      static GEnumClass *class = NULL;
	      if (!class)
		class = g_type_class_ref (GDK_TYPE_SURFACE_TYPE_HINT);
	      g_print ("gdk_surface_set_type_hint: %p: %s\n",
		       GDK_SURFACE_HWND (window),
		       g_enum_get_value (class, hint)->value_name);
	    }G_STMT_END);

  GDK_WIN32_SURFACE (window)->type_hint = hint;

  _gdk_win32_surface_update_style_bits (window);
}

static GdkSurfaceTypeHint
gdk_win32_surface_get_type_hint (GdkSurface *window)
{
  g_return_val_if_fail (GDK_IS_SURFACE (window), GDK_SURFACE_TYPE_HINT_NORMAL);

  if (GDK_SURFACE_DESTROYED (window))
    return GDK_SURFACE_TYPE_HINT_NORMAL;

  return GDK_WIN32_SURFACE (window)->type_hint;
}

GdkSurface *
gdk_win32_surface_lookup_for_display (GdkDisplay *display,
                                     HWND        anid)
{
  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  return (GdkSurface*) gdk_win32_handle_table_lookup (anid);
}

static void
gdk_win32_surface_set_opacity (GdkSurface *window,
			gdouble    opacity)
{
  LONG exstyle;
  typedef BOOL (WINAPI *PFN_SetLayeredWindowAttributes) (HWND, COLORREF, BYTE, DWORD);
  PFN_SetLayeredWindowAttributes setLayeredWindowAttributes = NULL;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  impl = GDK_WIN32_SURFACE (window);

  if (impl->layered)
    {
      if (impl->layered_opacity != opacity)
        {
          RECT window_rect;

          impl->layered_opacity = opacity;

          _gdk_win32_get_window_client_area_rect (window, impl->surface_scale, &window_rect);
          _gdk_win32_update_layered_window_from_cache (window, &window_rect, TRUE, TRUE, TRUE);
        }

      return;
    }

  exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);

  if (!(exstyle & WS_EX_LAYERED))
    SetWindowLong (GDK_SURFACE_HWND (window),
		    GWL_EXSTYLE,
		    exstyle | WS_EX_LAYERED);

  setLayeredWindowAttributes =
    (PFN_SetLayeredWindowAttributes)GetProcAddress (GetModuleHandle ("user32.dll"), "SetLayeredWindowAttributes");

  if (setLayeredWindowAttributes)
    {
      API_CALL (setLayeredWindowAttributes, (GDK_SURFACE_HWND (window),
					     0,
					     opacity * 0xff,
					     LWA_ALPHA));
    }
}

gboolean
gdk_win32_surface_is_win32 (GdkSurface *window)
{
  return GDK_IS_WIN32_SURFACE (window);
}

static gboolean
gdk_win32_surface_show_window_menu (GdkSurface *window,
                                   GdkEvent  *event)
{
  double event_x, event_y;
  gint x, y;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  switch (event->any.type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  gdk_event_get_coords (event, &event_x, &event_y);
  x = round (event_x);
  y = round (event_y);

  SendMessage (GDK_SURFACE_HWND (window),
               WM_SYSMENU,
               0,
               MAKELPARAM (x * impl->surface_scale, y * impl->surface_scale));

  return TRUE;
}

HWND
gdk_win32_surface_get_impl_hwnd (GdkSurface *window)
{
  if (GDK_IS_WIN32_SURFACE (window))
    return GDK_SURFACE_HWND (window);
  return NULL;
}

BOOL WINAPI
GtkShowWindow (GdkSurface *window,
               int        cmd_show)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  RECT window_rect;
  HDC hdc;
  POINT window_position;
  SIZE window_size;
  POINT source_point;
  BLENDFUNCTION blender;

  HWND hwnd = GDK_SURFACE_HWND (window);
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  switch (cmd_show)
    {
    case SW_FORCEMINIMIZE:
    case SW_HIDE:
    case SW_MINIMIZE:
      break;
    case SW_MAXIMIZE:
    case SW_RESTORE:
    case SW_SHOW:
    case SW_SHOWDEFAULT:
    case SW_SHOWMINIMIZED:
    case SW_SHOWMINNOACTIVE:
    case SW_SHOWNA:
    case SW_SHOWNOACTIVATE:
    case SW_SHOWNORMAL:
      if (IsWindowVisible (hwnd))
        break;

      if ((WS_EX_LAYERED & GetWindowLongPtr (hwnd, GWL_EXSTYLE)) != WS_EX_LAYERED)
        break;

      /* Window was hidden, will be shown. Erase it, GDK will repaint soon,
       * but not soon enough, so it's possible to see old content before
       * the next redraw, unless we erase the window first.
       */
      GetWindowRect (hwnd, &window_rect);
      source_point.x = source_point.y = 0;

      window_position.x = window_rect.left;
      window_position.y = window_rect.top;
      window_size.cx = window_rect.right - window_rect.left;
      window_size.cy = window_rect.bottom - window_rect.top;

      blender.BlendOp = AC_SRC_OVER;
      blender.BlendFlags = 0;
      blender.AlphaFormat = AC_SRC_ALPHA;
      blender.SourceConstantAlpha = 255;

      /* Create a surface of appropriate size and clear it */
      surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32,
                                                     window_size.cx,
                                                     window_size.cy);
      cairo_surface_set_device_scale (surface, impl->surface_scale, impl->surface_scale);
      cr = cairo_create (surface);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
      cairo_paint (cr);
      cairo_destroy (cr);
      cairo_surface_flush (surface);
      hdc = cairo_win32_surface_get_dc (surface);

      /* No API_CALL() wrapper, don't check for errors */
      UpdateLayeredWindow (hwnd, NULL,
                           &window_position, &window_size,
                           hdc, &source_point,
                           0, &blender, ULW_ALPHA);

      cairo_surface_destroy (surface);

      break;
    }

  /* Ensure that maximized window size is corrected later on */
  if (cmd_show == SW_MAXIMIZE)
    impl->maximizing = TRUE;

  return ShowWindow (hwnd, cmd_show);
}

static void
gdk_win32_surface_set_shadow_width (GdkSurface *window,
                                   gint       left,
                                   gint       right,
                                   gint       top,
                                   gint       bottom)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_set_shadow_width: window %p, "
                           "left %d, top %d, right %d, bottom %d\n",
                           window, left, top, right, bottom));

  impl->zero_margins = left == 0 && right == 0 && top == 0 && bottom == 0;

  if (impl->zero_margins)
    return;

  impl->margins.left = left;
  impl->margins.right = right * impl->surface_scale;
  impl->margins.top = top;
  impl->margins.bottom = bottom * impl->surface_scale;
  impl->margins_x = left + right;
  impl->margins_y = top + bottom;
}


gint
_gdk_win32_surface_get_scale_factor (GdkSurface *window)
{
  GdkDisplay *display;
  GdkWin32Surface *impl;
  GdkWin32Display *win32_display;

  if (GDK_SURFACE_DESTROYED (window))
    return 1;

  g_return_val_if_fail (window != NULL, 1);

  display = gdk_surface_get_display (window);
  impl = GDK_WIN32_SURFACE (window);

  win32_display = GDK_WIN32_DISPLAY (display);

  if (win32_display->dpi_aware_type != PROCESS_DPI_UNAWARE)
    {
      if (win32_display->has_fixed_scale)
        impl->surface_scale = win32_display->surface_scale;
      else
        impl->surface_scale = _gdk_win32_display_get_monitor_scale_factor (win32_display,
                                                                          NULL,
                                                                          GDK_SURFACE_HWND (window),
                                                                          NULL);

      return impl->surface_scale;
    }
  else
    {
      if (win32_display->has_fixed_scale)
        {
          static gsize hidpi_msg_displayed = 0;

          if (g_once_init_enter (&hidpi_msg_displayed))
            {
              g_message ("Note: GDK_SCALE is ignored as HiDPI awareness is disabled.");
              g_once_init_leave (&hidpi_msg_displayed, 1);
            }
        }

      /* Application is not DPI aware, don't bother */
      return 1;
    }
}

void
_gdk_win32_surface_get_unscaled_size (GdkSurface *window,
                                    gint      *unscaled_width,
                                    gint      *unscaled_height)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (unscaled_width)
    *unscaled_width = impl->unscaled_width;
  if (unscaled_height)
    *unscaled_height = impl->unscaled_height;
}

static void
gdk_win32_input_shape_combine_region (GdkSurface            *window,
                                      const cairo_region_t *shape_region,
                                      gint                  offset_x,
                                      gint                  offset_y)
{
  /* Partial input shape support is implemented by handling the
   * NC_NCHITTEST message
   */
}

static void
gdk_win32_surface_class_init (GdkWin32SurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = gdk_surface_win32_dispose;
  object_class->finalize = gdk_surface_win32_finalize;

  impl_class->show = gdk_win32_surface_show;
  impl_class->hide = gdk_win32_surface_hide;
  impl_class->withdraw = gdk_win32_surface_withdraw;
  impl_class->raise = gdk_win32_surface_raise;
  impl_class->lower = gdk_win32_surface_lower;
  impl_class->restack_toplevel = gdk_win32_surface_restack_toplevel;
  impl_class->toplevel_resize = gdk_win32_surface_toplevel_resize;
  impl_class->move_to_rect = gdk_win32_surface_move_to_rect;
  impl_class->get_geometry = gdk_win32_surface_get_geometry;
  impl_class->get_device_state = gdk_surface_win32_get_device_state;
  impl_class->get_root_coords = gdk_win32_surface_get_root_coords;

  impl_class->input_shape_combine_region = gdk_win32_input_shape_combine_region;
  impl_class->destroy = gdk_win32_surface_destroy;

  //impl_class->beep = gdk_x11_surface_beep;


  impl_class->show_window_menu = gdk_win32_surface_show_window_menu;
  impl_class->focus = gdk_win32_surface_focus;
  impl_class->set_type_hint = gdk_win32_surface_set_type_hint;
  impl_class->get_type_hint = gdk_win32_surface_get_type_hint;
  impl_class->set_modal_hint = gdk_win32_surface_set_modal_hint;
  impl_class->set_geometry_hints = gdk_win32_surface_set_geometry_hints;
  impl_class->set_title = gdk_win32_surface_set_title;
  //impl_class->set_startup_id = gdk_x11_surface_set_startup_id;
  impl_class->set_transient_for = gdk_win32_surface_set_transient_for;
  impl_class->set_accept_focus = gdk_win32_surface_set_accept_focus;
  impl_class->set_focus_on_map = gdk_win32_surface_set_focus_on_map;
  impl_class->set_icon_list = gdk_win32_surface_set_icon_list;
  impl_class->set_icon_name = gdk_win32_surface_set_icon_name;
  impl_class->iconify = gdk_win32_surface_iconify;
  impl_class->deiconify = gdk_win32_surface_deiconify;
  impl_class->stick = gdk_win32_surface_stick;
  impl_class->unstick = gdk_win32_surface_unstick;
  impl_class->maximize = gdk_win32_surface_maximize;
  impl_class->unmaximize = gdk_win32_surface_unmaximize;
  impl_class->fullscreen = gdk_win32_surface_fullscreen;
  impl_class->unfullscreen = gdk_win32_surface_unfullscreen;
  impl_class->set_keep_above = gdk_win32_surface_set_keep_above;
  impl_class->set_keep_below = gdk_win32_surface_set_keep_below;
  impl_class->set_decorations = gdk_win32_surface_set_decorations;
  impl_class->get_decorations = gdk_win32_surface_get_decorations;
  impl_class->set_functions = gdk_win32_surface_set_functions;

  impl_class->set_shadow_width = gdk_win32_surface_set_shadow_width;
  impl_class->begin_resize_drag = gdk_win32_surface_begin_resize_drag;
  impl_class->begin_move_drag = gdk_win32_surface_begin_move_drag;
  impl_class->set_opacity = gdk_win32_surface_set_opacity;
  impl_class->destroy_notify = gdk_win32_surface_destroy_notify;
  impl_class->register_dnd = _gdk_win32_surface_register_dnd;
  impl_class->drag_begin = _gdk_win32_surface_drag_begin;
  impl_class->create_gl_context = _gdk_win32_surface_create_gl_context;
  impl_class->get_scale_factor = _gdk_win32_surface_get_scale_factor;
  impl_class->get_unscaled_size = _gdk_win32_surface_get_unscaled_size;
}

HGDIOBJ
gdk_win32_surface_get_handle (GdkSurface *window)
{
  if (!GDK_IS_WIN32_SURFACE (window))
    {
      g_warning (G_STRLOC " window is not a native Win32 window");
      return NULL;
    }

  return GDK_SURFACE_HWND (window);
}
