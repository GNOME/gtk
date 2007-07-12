/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2001-2004 Hans Breuer
 * Copyright (C) 2007 Cody Russell
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <stdlib.h>

#ifndef _MSC_VER
#define _WIN32_WINNT 0x0500
#define WINVER _WIN32_WINNT
#endif

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

#if defined(_MSC_VER) && (WINVER < 0x0500)

typedef struct
{
  UINT cbSize;
  HWND hwnd;
  DWORD dwFlags;
  UINT uCount;
  DWORD dwTimeout;
} FLASHWINFO;

#define FLASHW_STOP 0
#define FLASHW_CAPTION 1
#define FLASHW_TRAY 2
#define FLASHW_ALL (FLASHW_CAPTION|FLASHW_TRAY)
#define FLASHW_TIMER 4

#define GetAncestor(hwnd,what) _gdk_win32_get_ancestor_parent (hwnd)

static HWND
_gdk_win32_get_ancestor_parent (HWND hwnd)
{
#ifndef GA_PARENT
#  define GA_PARENT 1 
#endif
  typedef HWND (WINAPI *PFN_GetAncestor) (HWND,UINT);
  static PFN_GetAncestor p_GetAncestor = NULL;
  static gboolean once = FALSE;
  
  if (!once)
    {
      HMODULE user32;

      user32 = GetModuleHandle ("user32.dll");
      p_GetAncestor = (PFN_GetAncestor)GetProcAddress (user32, "GetAncestor");
      once = TRUE;
    }
  if (p_GetAncestor)
    return p_GetAncestor (hwnd, GA_PARENT);
  else /* not completely right, but better than nothing ? */
    return GetParent (hwnd);
}

#endif

#if 0
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#endif

static GdkColormap* gdk_window_impl_win32_get_colormap (GdkDrawable *drawable);
static void         gdk_window_impl_win32_set_colormap (GdkDrawable *drawable,
							GdkColormap *cmap);
static void         gdk_window_impl_win32_get_size     (GdkDrawable *drawable,
							gint *width,
							gint *height);
static GdkRegion*   gdk_window_impl_win32_get_visible_region (GdkDrawable *drawable);
static void gdk_window_impl_win32_init       (GdkWindowImplWin32      *window);
static void gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass);
static void gdk_window_impl_win32_finalize   (GObject                 *object);

static gpointer parent_class = NULL;

static void     update_style_bits         (GdkWindow *window);
static gboolean _gdk_window_get_functions (GdkWindow     *window,
                                           GdkWMFunction *functions);

#define WINDOW_IS_TOPLEVEL(window)		   \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)

GType
_gdk_window_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkWindowImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_window_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkWindowImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_window_impl_win32_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_WIN32,
                                            "GdkWindowImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

GType
_gdk_window_impl_get_type (void)
{
  return _gdk_window_impl_win32_get_type ();
}

static void
gdk_window_impl_win32_init (GdkWindowImplWin32 *impl)
{
  impl->width = 1;
  impl->height = 1;
  impl->toplevel_window_type = -1;
  impl->hcursor = NULL;
  impl->hicon_big = NULL;
  impl->hicon_small = NULL;
  impl->hint_flags = 0;
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  impl->extension_events_selected = FALSE;
  impl->transient_owner = NULL;
}

static void
gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_win32_finalize;

  drawable_class->set_colormap = gdk_window_impl_win32_set_colormap;
  drawable_class->get_colormap = gdk_window_impl_win32_get_colormap;
  drawable_class->get_size = gdk_window_impl_win32_get_size;

  /* Visible and clip regions are the same */
  drawable_class->get_clip_region = gdk_window_impl_win32_get_visible_region;
  drawable_class->get_visible_region = gdk_window_impl_win32_get_visible_region;
}

static void
gdk_window_impl_win32_finalize (GObject *object)
{
  GdkWindowObject *wrapper;
  GdkDrawableImplWin32 *draw_impl;
  GdkWindowImplWin32 *window_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (object));

  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (object);
  window_impl = GDK_WINDOW_IMPL_WIN32 (object);
  
  wrapper = (GdkWindowObject*) draw_impl->wrapper;

  if (!GDK_WINDOW_DESTROYED (wrapper))
    {
      gdk_win32_handle_table_remove (draw_impl->handle);
    }

  if (window_impl->hcursor != NULL)
    {
      if (GetCursor () == window_impl->hcursor)
	SetCursor (NULL);
      GDI_CALL (DestroyCursor, (window_impl->hcursor));
      window_impl->hcursor = NULL;
    }
  if (window_impl->hicon_big != NULL)
    {
      GDI_CALL (DestroyIcon, (window_impl->hicon_big));
      window_impl->hicon_big = NULL;
    }
  if (window_impl->hicon_small != NULL)
    {
      GDI_CALL (DestroyIcon, (window_impl->hicon_small));
      window_impl->hicon_small = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
_gdk_win32_adjust_client_rect (GdkWindow *window,
			       RECT      *rect)
{
  LONG style, exstyle;

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
  API_CALL (AdjustWindowRectEx, (rect, style, FALSE, exstyle));
}

static GdkColormap*
gdk_window_impl_win32_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *drawable_impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable), NULL);

  drawable_impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (!((GdkWindowObject *) drawable_impl->wrapper)->input_only && 
      drawable_impl->colormap == NULL)
    {
      drawable_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
      g_object_ref (drawable_impl->colormap);
    }
  
  return drawable_impl->colormap;
}

static void
gdk_window_impl_win32_set_colormap (GdkDrawable *drawable,
				    GdkColormap *cmap)
{
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable));

  impl = GDK_WINDOW_IMPL_WIN32 (drawable);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  /* chain up */
  GDK_DRAWABLE_CLASS (parent_class)->set_colormap (drawable, cmap);
  
  if (cmap)
    {
      /* XXX */
      g_print ("gdk_window_impl_win32_set_colormap: XXX\n");
    }
}

static void
gdk_window_impl_win32_get_size (GdkDrawable *drawable,
				gint        *width,
				gint        *height)
{
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable));

  if (width)
    *width = GDK_WINDOW_IMPL_WIN32 (drawable)->width;
  if (height)
    *height = GDK_WINDOW_IMPL_WIN32 (drawable)->height;
}

static GdkRegion*
gdk_window_impl_win32_get_visible_region (GdkDrawable *drawable)
{
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (drawable);
  GdkRectangle result_rect;

  result_rect.x = 0;
  result_rect.y = 0;
  result_rect.width = impl->width;
  result_rect.height = impl->height;

  gdk_rectangle_intersect (&result_rect, &impl->position_info.clip_rect, &result_rect);

  return gdk_region_rectangle (&result_rect);
}

void
_gdk_root_window_size_init (void)
{
  GdkWindowImplWin32 *impl;
  GdkRectangle rect;
  int i;

  impl = GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) _gdk_root)->impl);
  rect = _gdk_monitors[0];
  for (i = 1; i < _gdk_num_monitors; i++)
    gdk_rectangle_union (&rect, _gdk_monitors+i, &rect);

  impl->width = rect.width;
  impl->height = rect.height;
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;
  GdkDrawableImplWin32 *draw_impl;

  g_assert (_gdk_root == NULL);
  
  _gdk_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)_gdk_root;
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  
  draw_impl->handle = GetDesktopWindow ();
  draw_impl->wrapper = GDK_DRAWABLE (private);
  draw_impl->colormap = gdk_screen_get_default_colormap (_gdk_screen);
  g_object_ref (draw_impl->colormap);
  
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = gdk_visual_get_system ()->depth;

  _gdk_root_window_size_init ();
 
  _gdk_window_init_position (GDK_WINDOW (private));

  gdk_win32_handle_table_insert ((HANDLE *) &draw_impl->handle, _gdk_root);

  GDK_NOTE (MISC, g_print ("_gdk_root=%p\n", GDK_WINDOW_HWND (_gdk_root)));
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
 *   GdkWindowType. If support for single window-specific icons
 *   is ever needed (e.g Dialog specific), every such window should
 *   get its own class
 */
static ATOM
RegisterGdkClass (GdkWindowType wtype, GdkWindowTypeHint wtype_hint)
{
  static ATOM klassTOPLEVEL   = 0;
  static ATOM klassDIALOG     = 0;
  static ATOM klassCHILD      = 0;
  static ATOM klassTEMP       = 0;
  static ATOM klassTEMPSHADOW = 0;
  static HICON hAppIcon = NULL;
  static HICON hAppIconSm = NULL;
  static WNDCLASSEX wcl; 
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = _gdk_win32_window_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = _gdk_app_hmodule;
  wcl.hIcon = 0;
  wcl.hIconSm = 0;
  /* initialize once! */
  if (0 == hAppIcon && 0 == hAppIconSm)
    {
      gchar sLoc [MAX_PATH+1];

      if (0 != GetModuleFileName (_gdk_app_hmodule, sLoc, MAX_PATH))
        {
          ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);
          if (0 == hAppIcon && 0 == hAppIconSm)
            {
              if (0 != GetModuleFileName (_gdk_dll_hinstance, sLoc, MAX_PATH))
                ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);
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
    case GDK_WINDOW_TOPLEVEL:
      if (0 == klassTOPLEVEL)
	{
	  wcl.lpszClassName = "gdkWindowToplevel";
	  
	  ONCE_PER_CLASS ();
	  klassTOPLEVEL = RegisterClassEx (&wcl);
	}
      klass = klassTOPLEVEL;
      break;
      
    case GDK_WINDOW_CHILD:
      if (0 == klassCHILD)
	{
	  wcl.lpszClassName = "gdkWindowChild";
	  
	  wcl.style |= CS_PARENTDC; /* MSDN: ... enhances system performance. */
	  ONCE_PER_CLASS ();
	  klassCHILD = RegisterClassEx (&wcl);
	}
      klass = klassCHILD;
      break;
      
    case GDK_WINDOW_DIALOG:
      if (0 == klassDIALOG)
	{
	  wcl.lpszClassName = "gdkWindowDialog";
	  wcl.style |= CS_SAVEBITS;
	  ONCE_PER_CLASS ();
	  klassDIALOG = RegisterClassEx (&wcl);
	}
      klass = klassDIALOG;
      break;
      
    case GDK_WINDOW_TEMP:
      if ((wtype_hint == GDK_WINDOW_TYPE_HINT_MENU) ||
	  (wtype_hint == GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU) ||
	  (wtype_hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU) ||
	  (wtype_hint == GDK_WINDOW_TYPE_HINT_TOOLTIP))
	{
	  if (klassTEMPSHADOW == 0)
	    {
	      wcl.lpszClassName = "gdkWindowTempShadow";
	      wcl.style |= CS_SAVEBITS;
	      if (_winver >= 0x0501) /* Windows XP (5.1) or above */
		wcl.style |= 0x00020000; /* CS_DROPSHADOW */
	      ONCE_PER_CLASS ();
	      klassTEMPSHADOW = RegisterClassEx (&wcl);
	    }
	  klass = klassTEMPSHADOW;
	}
      else
	{
	  if (0 == klassTEMP)
	    {
	      wcl.lpszClassName = "gdkWindowTemp";
	      wcl.style |= CS_SAVEBITS;
	      ONCE_PER_CLASS ();
	      klassTEMP = RegisterClassEx (&wcl);
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
      WIN32_API_FAILED ("RegisterClassEx");
      g_error ("That is a fatal error");
    }
  return klass;
}

static GdkWindow*
gdk_window_new_internal (GdkWindow     *parent,
			 GdkWindowAttr *attributes,
			 gint           attributes_mask,
			 gboolean       from_set_skip_taskbar_hint)
{
  HWND hwndNew;
  HANDLE hparent;
  ATOM klass = 0;
  DWORD dwStyle = 0, dwExStyle;
  RECT rect;
  GdkWindow *window;
  GdkWindow *orig_parent;
  GdkWindowObject *private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;
  GdkVisual *visual;
  const gchar *title;
  char *mbtitle;
  gint window_width, window_height;
  gint offset_x = 0, offset_y = 0;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    parent = _gdk_root;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  orig_parent = parent;

  GDK_NOTE (MISC,
	    g_print ("gdk_window_new: %s\n",
		     (attributes->window_type == GDK_WINDOW_TOPLEVEL ? "TOPLEVEL" :
		      (attributes->window_type == GDK_WINDOW_CHILD ? "CHILD" :
		       (attributes->window_type == GDK_WINDOW_DIALOG ? "DIALOG" :
			(attributes->window_type == GDK_WINDOW_TEMP ? "TEMP" :
			 "???"))))));

  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;
  
  hparent = GDK_WINDOW_HWND (parent);

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = _gdk_root;
  
  private->parent = (GdkWindowObject *)parent;

  private->accept_focus = TRUE;
  private->focus_on_map = TRUE;

  if (attributes_mask & GDK_WA_X)
    private->x = attributes->x;
  else
    private->x = 0;
  
  if (attributes_mask & GDK_WA_Y)
    private->y = attributes->y;
  else if (attributes_mask & GDK_WA_X)
    private->y = 100;		/* ??? We must put it somewhere... */
  else
    private->y = 0;
  
  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();

  impl->width = (attributes->width > 1) ? (attributes->width) : (1);
  impl->height = (attributes->height > 1) ? (attributes->height) : (1);
  impl->extension_events_selected = FALSE;
  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->window_type for input-only windows
       * before
       */
      if (parent == _gdk_root)
	private->window_type = GDK_WINDOW_TEMP;
      else
	private->window_type = GDK_WINDOW_CHILD;
    }
  else
    private->window_type = attributes->window_type;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      dwExStyle = 0;

      private->input_only = FALSE;
      private->depth = visual->depth;
      
      if (attributes_mask & GDK_WA_COLORMAP)
	{
	  draw_impl->colormap = attributes->colormap;
          g_object_ref (attributes->colormap);
        }
      else
	{
	  draw_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
	  g_object_ref (draw_impl->colormap);
	}
    }
  else
    {
      dwExStyle = WS_EX_TRANSPARENT;
      private->depth = 0;
      private->input_only = TRUE;
      draw_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
      g_object_ref (draw_impl->colormap);
      GDK_NOTE (MISC, g_print ("... GDK_INPUT_ONLY, system colormap"));
    }

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
      if (parent != _gdk_root)
	{
	  g_warning (G_STRLOC ": Toplevel windows must be created as children\n"
		     "of a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
	  hparent = GetDesktopWindow ();
	}
      /* Children of foreign windows aren't toplevel windows */
      if (GDK_WINDOW_TYPE (orig_parent) == GDK_WINDOW_FOREIGN)
	{
	  dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN;
	}
      else
	{
	  if (private->window_type == GDK_WINDOW_TOPLEVEL)
	    dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
	  else
	    dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_CLIPCHILDREN;

	  offset_x = _gdk_offset_x;
	  offset_y = _gdk_offset_y;
	}
      break;

    case GDK_WINDOW_CHILD:
      dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      break;

    case GDK_WINDOW_TEMP:
      /* A temp window is not necessarily a top level window */
      dwStyle = (_gdk_root == parent ? WS_POPUP : WS_CHILDWINDOW);
      dwStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      dwExStyle |= WS_EX_TOOLWINDOW;
      offset_x = _gdk_offset_x;
      offset_y = _gdk_offset_y;
      break;

    case GDK_WINDOW_ROOT:
      g_error ("cannot make windows of type GDK_WINDOW_ROOT");
      break;

    default:
      g_assert_not_reached ();
    }

  _gdk_window_init_position (GDK_WINDOW (private));

  if (private->window_type != GDK_WINDOW_CHILD)
    {
      rect.left = rect.top = 0;
      rect.right = impl->position_info.width;
      rect.bottom = impl->position_info.height;

      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);

      window_width = rect.right - rect.left;
      window_height = rect.bottom - rect.top;
    }
  else
    {
      window_width = impl->position_info.width;
      window_height = impl->position_info.height;
    }

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = get_default_title ();
  if (!title || !*title)
    title = "";

  private->event_mask = GDK_STRUCTURE_MASK | attributes->event_mask;
      
  if (attributes_mask & GDK_WA_TYPE_HINT)
    impl->type_hint = attributes->type_hint;
  else
    impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;

  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);

  klass = RegisterGdkClass (private->window_type, impl->type_hint);

  mbtitle = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);
  
  hwndNew = CreateWindowEx (dwExStyle,
			    MAKEINTRESOURCE (klass),
			    mbtitle,
			    dwStyle,
			    ((attributes_mask & GDK_WA_X) ?
			     impl->position_info.x - offset_x : CW_USEDEFAULT),
			    impl->position_info.y - offset_y, 
			    window_width, window_height,
			    hparent,
			    NULL,
			    _gdk_app_hmodule,
			    window);
  if (GDK_WINDOW_HWND (window) != hwndNew)
    {
      g_warning ("gdk_window_new: gdk_event_translate::WM_CREATE (%p, %p) HWND mismatch.",
		 GDK_WINDOW_HWND (window),
		 hwndNew);

      /* HB: IHMO due to a race condition the handle was increased by
       * one, which causes much trouble. Because I can't find the 
       * real bug, try to workaround it ...
       * To reproduce: compile with MSVC 5, DEBUG=1
       */
# if 0
      gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
      GDK_WINDOW_HWND (window) = hwndNew;
      gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
# else
      /* the old behaviour, but with warning */
      draw_impl->handle = hwndNew;
# endif

    }

  g_object_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);

  GDK_NOTE (MISC, g_print ("... \"%s\" %dx%d@%+d%+d %p = %p\n",
			   mbtitle,
			   window_width, window_height,
			   ((attributes_mask & GDK_WA_X) ?
			    impl->position_info.x - offset_x: CW_USEDEFAULT),
			   impl->position_info.y - offset_y, 
			   hparent,
			   GDK_WINDOW_HWND (window)));

  g_free (mbtitle);

  if (draw_impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowEx");
      g_object_unref (window);
      return NULL;
    }

  if (!from_set_skip_taskbar_hint && private->window_type == GDK_WINDOW_TEMP)
    gdk_window_set_skip_taskbar_hint (window, TRUE);

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  return window;
}

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  return gdk_window_new_internal (parent, attributes, attributes_mask, FALSE);
}

GdkWindow *
gdk_window_foreign_new_for_display (GdkDisplay      *display,
                                    GdkNativeWindow  anid)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;

  HANDLE parent;
  RECT rect;
  POINT point;

  g_return_val_if_fail (display == _gdk_display, NULL);

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  parent = GetParent ((HWND)anid);
  
  private->parent = gdk_win32_handle_table_lookup ((GdkNativeWindow) parent);
  if (!private->parent || GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_FOREIGN)
    private->parent = (GdkWindowObject *)_gdk_root;
  
  private->parent->children = g_list_prepend (private->parent->children, window);

  draw_impl->handle = (HWND) anid;
  GetClientRect ((HWND) anid, &rect);
  point.x = rect.left;
  point.y = rect.right;
  ClientToScreen ((HWND) anid, &point);
  if (parent != GetDesktopWindow ())
    ScreenToClient (parent, &point);
  private->x = point.x;
  private->y = point.y;
  impl->width = rect.right - rect.left;
  impl->height = rect.bottom - rect.top;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;
  private->event_mask = GDK_ALL_EVENTS_MASK; /* XXX */
  if (IsWindowVisible ((HWND) anid))
    private->state &= (~GDK_WINDOW_STATE_WITHDRAWN);
  else
    private->state |= GDK_WINDOW_STATE_WITHDRAWN;
  if (GetWindowLong ((HWND)anid, GWL_EXSTYLE) & WS_EX_TOPMOST)
    private->state |= GDK_WINDOW_STATE_ABOVE;
  else
    private->state &= (~GDK_WINDOW_STATE_ABOVE);
  private->state &= (~GDK_WINDOW_STATE_BELOW);

  private->depth = gdk_visual_get_system ()->depth;

  _gdk_window_init_position (GDK_WINDOW (private));

  g_object_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);

  GDK_NOTE (MISC, g_print ("gdk_window_foreign_new_for_display: %p: %s@%+d%+d\n",
			   (HWND) anid,
			   _gdk_win32_drawable_description (window),
			   private->x, private->y));

  return window;
}

GdkWindow*
gdk_window_lookup (GdkNativeWindow hwnd)
{
  return (GdkWindow*) gdk_win32_handle_table_lookup (hwnd); 
}

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplWin32 *window_impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("_gdk_windowing_window_destroy: %p\n",
			   GDK_WINDOW_HWND (window)));

  if (private->extension_events != 0)
    _gdk_input_window_destroy (window);

  /* Remove ourself from our transient owner */
  if (window_impl->transient_owner != NULL)
    {
      gdk_window_set_transient_for (window, NULL);
    }

  if (!recursing && !foreign_destroy)
    {
      _gdk_win32_drawable_finish (private->impl);

      private->destroyed = TRUE;
      DestroyWindow (GDK_WINDOW_HWND (window));
    }

  gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
}

void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
  /* It's somebody else's window, but in our hierarchy, so reparent it
   * to the desktop, and then try to destroy it.
   */
  gdk_window_hide (window);
  gdk_window_reparent (window, NULL, 0, 0);
  
  PostMessage (GDK_WINDOW_HWND (window), WM_CLOSE, 0, 0);
}

/* This function is called when the window really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_window_destroy_notify: %p%s\n",
		     GDK_WINDOW_HWND (window),
		     (GDK_WINDOW_DESTROYED (window) ? " (destroyed)" : "")));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	g_warning ("window %p unexpectedly destroyed",
		   GDK_WINDOW_HWND (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
  g_object_unref (window);
}

static void
get_outer_rect (GdkWindow *window,
		gint       width,
		gint       height,
		RECT      *rect)
{
  rect->left = rect->top = 0;
  rect->right = width;
  rect->bottom = height;
      
  _gdk_win32_adjust_client_rect (window, rect);
}

static void
adjust_for_gravity_hints (GdkWindowImplWin32 *impl,
			  RECT               *outer_rect,
			  gint		     *x,
			  gint		     *y)
{
  if (impl->hint_flags & GDK_HINT_WIN_GRAVITY)
    {
      gint orig_x = *x, orig_y = *y;

      switch (impl->hints.win_gravity)
	{
	case GDK_GRAVITY_NORTH:
	case GDK_GRAVITY_CENTER:
	case GDK_GRAVITY_SOUTH:
	  *x -= (outer_rect->right - outer_rect->left) / 2;
	  *x += impl->width / 2;
	  break;
	      
	case GDK_GRAVITY_SOUTH_EAST:
	case GDK_GRAVITY_EAST:
	case GDK_GRAVITY_NORTH_EAST:
	  *x -= outer_rect->right - outer_rect->left;
	  *x += impl->width;
	  break;

	case GDK_GRAVITY_STATIC:
	  *x += outer_rect->left;
	  break;

	default:
	  break;
	}

      switch (impl->hints.win_gravity)
	{
	case GDK_GRAVITY_WEST:
	case GDK_GRAVITY_CENTER:
	case GDK_GRAVITY_EAST:
	  *y -= (outer_rect->bottom - outer_rect->top) / 2;
	  *y += impl->height / 2;
	  break;

	case GDK_GRAVITY_SOUTH_WEST:
	case GDK_GRAVITY_SOUTH:
	case GDK_GRAVITY_SOUTH_EAST:
	  *y -= outer_rect->bottom - outer_rect->top;
	  *y += impl->height;
	  break;

	case GDK_GRAVITY_STATIC:
	  *y += outer_rect->top;
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
show_window_internal (GdkWindow *window,
                      gboolean   raise,
		      gboolean   deiconify)
{
  GdkWindowObject *private;
  HWND old_active_window;
  gboolean focus_on_map = TRUE;

  private = (GdkWindowObject *) window;

  if (private->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("show_window_internal: %p: %s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (private->state),
			   (raise ? " raise" : ""),
			   (deiconify ? " deiconify" : "")));
  
  /* If asked to show (not deiconify) an withdrawn and iconified
   * window, do that.
   */
  if (!deiconify &&
      !GDK_WINDOW_IS_MAPPED (window) &&
      (private->state & GDK_WINDOW_STATE_ICONIFIED))
    {	
      ShowWindow (GDK_WINDOW_HWND (window), SW_MINIMIZE);
      return;
    }
  
  /* If asked to just show an iconified window, do nothing. */
  if (!deiconify && (private->state & GDK_WINDOW_STATE_ICONIFIED))
    return;
  
  /* If asked to deiconify an already noniconified window, do
   * nothing. (Especially, don't cause the window to rise and
   * activate. There are different calls for that.)
   */
  if (deiconify && !(private->state & GDK_WINDOW_STATE_ICONIFIED))
    return;
  
  /* If asked to show (but not raise) a window that is already
   * visible, do nothing.
   */
  if (!deiconify && !raise && IsWindowVisible (GDK_WINDOW_HWND (window)))
    return;

  /* Other cases */
  
  if (!GDK_WINDOW_IS_MAPPED (window))
    {
      gdk_synthesize_window_state (window,
				   GDK_WINDOW_STATE_WITHDRAWN,
				   0);
      focus_on_map = private->focus_on_map;
    }

  /* Use SetWindowPos to show transparent windows so automatic redraws
   * in other windows can be suppressed.
   */
  if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
    {
      UINT flags = SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE;
      if (!raise)
	flags |= SWP_NOZORDER;
      if (!raise || GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP || !focus_on_map)
	flags |= SWP_NOACTIVATE;

      SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOP, 0, 0, 0, 0, flags);
      return;
    }

  old_active_window = GetActiveWindow ();

  if (private->state & (GDK_WINDOW_STATE_BELOW | GDK_WINDOW_STATE_ABOVE))
    {
      DWORD exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

      if (private->state & GDK_WINDOW_STATE_BELOW)
        exstyle &= (~WS_EX_TOPMOST);
      if (private->state & GDK_WINDOW_STATE_ABOVE)
        exstyle |= WS_EX_TOPMOST;

      API_CALL (SetWindowLong, (GDK_WINDOW_HWND (window), GWL_EXSTYLE, exstyle));
    }

  if (private->state & GDK_WINDOW_STATE_FULLSCREEN)
    gdk_window_fullscreen (window);
  else if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
    ShowWindow (GDK_WINDOW_HWND (window), SW_MAXIMIZE);
  else if (private->state & GDK_WINDOW_STATE_ICONIFIED)
    ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
  else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP || !focus_on_map)
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNOACTIVATE);
  else
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);

  if (raise)
    {
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)
        SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOPMOST,
		      0, 0, 0, 0,
		      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL ||
	       GDK_WINDOW_TYPE (window) == GDK_WINDOW_DIALOG)
	{
          if (focus_on_map && private->accept_focus)
            SetForegroundWindow (GDK_WINDOW_HWND (window));
	  else
            SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOP,
	  	          0, 0, 0, 0,
		          SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
      else
        BringWindowToTop (GDK_WINDOW_HWND (window));
    }
  else if (old_active_window != GDK_WINDOW_HWND (window))
    SetActiveWindow (old_active_window);
}

void
gdk_window_show_unraised (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  show_window_internal (window, FALSE, FALSE);
}

void
gdk_window_show (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, TRUE, FALSE);
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  if (private->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_hide: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (private->state)));
  
  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_WITHDRAWN);
  
  _gdk_window_clear_update_area (window);
  
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
    ShowOwnedPopups (GDK_WINDOW_HWND (window), FALSE);
  
  if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
    {
      SetWindowPos (GDK_WINDOW_HWND (window), HWND_BOTTOM,
		    0, 0, 0, 0,
		    SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
    }
  else
    {
      ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  if (private->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_withdraw: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (private->state)));
  
  gdk_window_hide (window);	/* ??? */
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_move: %p: %+d%+d\n",
			   GDK_WINDOW_HWND (window), x, y));
      
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

  if (private->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  /* Don't check GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD.
   * Foreign windows (another app's windows) might be children of our
   * windows! Especially in the case of gtkplug/socket.
   */ 
  if (GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) != GetDesktopWindow ())
    {
      _gdk_window_move_resize_child (window, x, y, impl->width, impl->height);
    }
  else
    {
      RECT outer_rect;

      get_outer_rect (window, impl->width, impl->height, &outer_rect);
      
      adjust_for_gravity_hints (impl, &outer_rect, &x, &y);

      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,0,0,"
			       "NOACTIVATE|NOSIZE|NOZORDER)\n",
			       GDK_WINDOW_HWND (window),
			       x - _gdk_offset_x, y - _gdk_offset_y));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			       x - _gdk_offset_x, y - _gdk_offset_y, 0, 0,
			       SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowObject *private = (GdkWindowObject*) window;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  GDK_NOTE (MISC, g_print ("gdk_window_resize: %p: %dx%d\n",
			   GDK_WINDOW_HWND (window), width, height));

  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  
  if (private->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  if (GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) != GetDesktopWindow ())
    {
      _gdk_window_move_resize_child (window, private->x, private->y, width, height);
    }
  else
    {
      RECT outer_rect;

      get_outer_rect (window, width, height, &outer_rect);

      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,0,0,%ld,%ld,"
			       "NOACTIVATE|NOMOVE|NOZORDER)\n",
			       GDK_WINDOW_HWND (window),
			       outer_rect.right - outer_rect.left,
			       outer_rect.bottom - outer_rect.top));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			       0, 0,
			       outer_rect.right - outer_rect.left,
			       outer_rect.bottom - outer_rect.top,
			       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));
      private->resize_count += 1;
    }
}

void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowObject *private = (GdkWindowObject*) window;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

  if (private->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_move_resize: %p: %dx%d@%+d%+d\n",
			   GDK_WINDOW_HWND (window),
			   width, height, x, y));
  
  if (GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) != GetDesktopWindow ())
    {
      _gdk_window_move_resize_child (window, x, y, width, height);
    }
  else
    {
      RECT outer_rect;

      get_outer_rect (window, width, height, &outer_rect);

      adjust_for_gravity_hints (impl, &outer_rect, &x, &y);

      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%ld,%ld,"
			       "NOACTIVATE|NOZORDER)\n",
			       GDK_WINDOW_HWND (window),
			       x - _gdk_offset_x, y - _gdk_offset_y,
			       outer_rect.right - outer_rect.left,
			       outer_rect.bottom - outer_rect.top));
			       
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			       x - _gdk_offset_x, y - _gdk_offset_y,
			       outer_rect.right - outer_rect.left,
			       outer_rect.bottom - outer_rect.top,
			       SWP_NOACTIVATE | SWP_NOZORDER));
    }
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowObject *window_private;
  GdkWindowObject *parent_private;
  GdkWindowObject *old_parent_private;
  GdkWindowImplWin32 *impl;
  gboolean was_toplevel;
  LONG style;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (window != _gdk_root);

  if (GDK_WINDOW_DESTROYED (window) ||
      (new_parent && GDK_WINDOW_DESTROYED (new_parent)))
    {
      return;
    }

  if (!new_parent)
    new_parent = _gdk_root;

  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject *) window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  impl = GDK_WINDOW_IMPL_WIN32 (window_private->impl);

  GDK_NOTE (MISC, g_print ("gdk_window_reparent: %p: %p\n",
			   GDK_WINDOW_HWND (window),
			   GDK_WINDOW_HWND (new_parent)));

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);

  was_toplevel = GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) == GetDesktopWindow ();
  if (was_toplevel && new_parent != _gdk_root)
    {
      /* Reparenting from top-level (child of desktop). Clear out
       * decorations.
       */
      style &= ~(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
      style |= WS_CHILD;
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
    }
  else if (new_parent == _gdk_root)
    {
      /* Reparenting to top-level. Add decorations. */
      style &= ~(WS_CHILD);
      style |= WS_OVERLAPPEDWINDOW;
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
    }

  API_CALL (SetParent, (GDK_WINDOW_HWND (window),
			GDK_WINDOW_HWND (new_parent)));
  
  API_CALL (MoveWindow, (GDK_WINDOW_HWND (window),
			 x, y, impl->width, impl->height, TRUE));

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    new_parent = _gdk_root;
  
  window_private->parent = (GdkWindowObject *)new_parent;

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
      if (impl->toplevel_window_type != -1)
	GDK_WINDOW_TYPE (window) = impl->toplevel_window_type;
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
      break;

    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (WINDOW_IS_TOPLEVEL (window))
	{
	  /* Save the original window type so we can restore it if the
	   * window is reparented back to be a toplevel.
	   */
	  impl->toplevel_window_type = GDK_WINDOW_TYPE (window);
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
	}
    }

  if (old_parent_private)
    old_parent_private->children =
      g_list_remove (old_parent_private->children, window);

  parent_private->children = g_list_prepend (parent_private->children, window);
  _gdk_window_init_position (GDK_WINDOW (window_private));
}

static void
erase_background (GdkWindow *window,
		  HDC        hdc)
{
  HDC bgdc = NULL;
  HBRUSH hbr = NULL;
  HPALETTE holdpal = NULL;
  RECT rect;
  COLORREF bg;
  GdkColormap *colormap;
  GdkColormapPrivateWin32 *colormap_private;
  int x, y;
  int x_offset, y_offset;
  
  if (((GdkWindowObject *) window)->input_only ||
      ((GdkWindowObject *) window)->bg_pixmap == GDK_NO_BG ||
      GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->position_info.no_bg)
    {
      return;
    }

  colormap = gdk_drawable_get_colormap (window);

  if (colormap &&
      (colormap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       colormap->visual->type == GDK_VISUAL_STATIC_COLOR))
    {
      int k;
	  
      colormap_private = GDK_WIN32_COLORMAP_DATA (colormap);

      if (!(holdpal = SelectPalette (hdc,  colormap_private->hpal, FALSE)))
        WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("erase_background: realized %p: %d colors\n",
				     colormap_private->hpal, k));
    }
  
  x_offset = y_offset = 0;
  while (window && ((GdkWindowObject *) window)->bg_pixmap == GDK_PARENT_RELATIVE_BG)
    {
      /* If this window should have the same background as the parent,
       * fetch the parent. (And if the same goes for the parent, fetch
       * the grandparent, etc.)
       */
      x_offset += ((GdkWindowObject *) window)->x;
      y_offset += ((GdkWindowObject *) window)->y;
      window = GDK_WINDOW (((GdkWindowObject *) window)->parent);
    }
  
  if (GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->position_info.no_bg)
    {
      /* Improves scolling effect, e.g. main buttons of testgtk */
      return;
    }

  GetClipBox (hdc, &rect);

  if (((GdkWindowObject *) window)->bg_pixmap == NULL)
    {
      bg = _gdk_win32_colormap_color (GDK_DRAWABLE_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->colormap,
				      ((GdkWindowObject *) window)->bg_color.pixel);
      
      if (!(hbr = CreateSolidBrush (bg)))
	WIN32_GDI_FAILED ("CreateSolidBrush");
      else if (!FillRect (hdc, &rect, hbr))
	WIN32_GDI_FAILED ("FillRect");
      if (hbr != NULL)
	DeleteObject (hbr);
    }
  else if (((GdkWindowObject *) window)->bg_pixmap != GDK_NO_BG)
    {
      GdkPixmap *pixmap = ((GdkWindowObject *) window)->bg_pixmap;
      GdkPixmapImplWin32 *pixmap_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
      
      if (x_offset == 0 && y_offset == 0 &&
	  pixmap_impl->width <= 8 && pixmap_impl->height <= 8)
	{
	  if (!(hbr = CreatePatternBrush (GDK_PIXMAP_HBITMAP (pixmap))))
	    WIN32_GDI_FAILED ("CreatePatternBrush");
	  else if (!FillRect (hdc, &rect, hbr))
	    WIN32_GDI_FAILED ("FillRect");
	  if (hbr != NULL)
	    DeleteObject (hbr);
	}
      else
	{
	  HGDIOBJ oldbitmap;

	  if (!(bgdc = CreateCompatibleDC (hdc)))
	    {
	      WIN32_GDI_FAILED ("CreateCompatibleDC");
	      return;
	    }
	  if (!(oldbitmap = SelectObject (bgdc, GDK_PIXMAP_HBITMAP (pixmap))))
	    {
	      WIN32_GDI_FAILED ("SelectObject");
	      DeleteDC (bgdc);
	      return;
	    }
	  x = -x_offset;
	  while (x < rect.right)
	    {
	      if (x + pixmap_impl->width >= rect.left)
		{
		  y = -y_offset;
		  while (y < rect.bottom)
		    {
		      if (y + pixmap_impl->height >= rect.top)
			{
			  if (!BitBlt (hdc, x, y,
				       pixmap_impl->width, pixmap_impl->height,
				       bgdc, 0, 0, SRCCOPY))
			    {
			      WIN32_GDI_FAILED ("BitBlt");
			      SelectObject (bgdc, oldbitmap);
			      DeleteDC (bgdc);
			      return;
			    }
			}
		      y += pixmap_impl->height;
		    }
		}
	      x += pixmap_impl->width;
	    }
	  SelectObject (bgdc, oldbitmap);
	  DeleteDC (bgdc);
	}
    }
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      HDC hdc;

      if (width == 0)
	width = impl->width - x;
      if (height == 0)
	height = impl->height - y;
      GDK_NOTE (MISC, g_print ("_gdk_windowing_window_clear_area: %p: "
			       "%dx%d@%+d%+d\n",
			       GDK_WINDOW_HWND (window),
			       width, height, x, y));
      hdc = GetDC (GDK_WINDOW_HWND (window));
      IntersectClipRect (hdc, x, y, x + width, y + height);
      erase_background (window, hdc);

      GDI_CALL (ReleaseDC, (GDK_WINDOW_HWND (window), hdc));
    }
}

void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      HDC hdc;
      RECT rect;

      GDK_NOTE (MISC, g_print ("_gdk_windowing_window_clear_area_e: %p: "
			       "%dx%d@%+d%+d\n",
			       GDK_WINDOW_HWND (window),
			       width, height, x, y));

      /* The background should be erased before the expose event is
	 generated */
      hdc = GetDC (GDK_WINDOW_HWND (window));
      IntersectClipRect (hdc, x, y, x + width, y + height);
      erase_background (window, hdc);
      GDI_CALL (ReleaseDC, (GDK_WINDOW_HWND (window), hdc));

      rect.left = x;
      rect.right = x + width;
      rect.top = y;
      rect.bottom = y + height;

      GDI_CALL (InvalidateRect, (GDK_WINDOW_HWND (window), &rect, TRUE));
      UpdateWindow (GDK_WINDOW_HWND (window));
    }
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_raise: %p\n",
			       GDK_WINDOW_HWND (window)));

      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)
        API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_TOPMOST,
	                         0, 0, 0, 0,
				 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
      else if (((GdkWindowObject *)window)->accept_focus)
        API_CALL (BringWindowToTop, (GDK_WINDOW_HWND (window)));
      else
        API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_TOP,
  			         0, 0, 0, 0,
			         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
    }
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_lower: %p\n"
			       "... SetWindowPos(%p,HWND_BOTTOM,0,0,0,0,"
			       "NOACTIVATE|NOMOVE|NOSIZE)\n",
			       GDK_WINDOW_HWND (window),
			       GDK_WINDOW_HWND (window)));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_BOTTOM,
			       0, 0, 0, 0,
			       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
    }
}

void
gdk_window_set_hints (GdkWindow *window,
		      gint       x,
		      gint       y,
		      gint       min_width,
		      gint       min_height,
		      gint       max_width,
		      gint       max_height,
		      gint       flags)
{
  /* Note that this function is obsolete */

  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  GDK_NOTE (MISC, g_print ("gdk_window_set_hints: %p: %dx%d..%dx%d @%+d%+d\n",
			   GDK_WINDOW_HWND (window),
			   min_width, min_height, max_width, max_height,
			   x, y));

  if (flags)
    {
      GdkGeometry geom;
      gint geom_mask = 0;

      geom.min_width  = min_width;
      geom.min_height = min_height;
      geom.max_width  = max_width;
      geom.max_height = max_height;

      if (flags & GDK_HINT_MIN_SIZE)
        geom_mask |= GDK_HINT_MIN_SIZE;

      if (flags & GDK_HINT_MAX_SIZE)
        geom_mask |= GDK_HINT_MAX_SIZE;

      gdk_window_set_geometry_hints (window, &geom, geom_mask);
    }
}

void
gdk_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  FLASHWINFO flashwinfo;
  typedef BOOL (*PFN_FlashWindowEx) (FLASHWINFO*);
  PFN_FlashWindowEx flashWindowEx = NULL;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  flashWindowEx = (PFN_FlashWindowEx) GetProcAddress (GetModuleHandle ("user32.dll"), "FlashWindowEx");

  if (flashWindowEx)
    {
      flashwinfo.cbSize = sizeof (flashwinfo);
      flashwinfo.hwnd = GDK_WINDOW_HWND (window);
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
      FlashWindow (GDK_WINDOW_HWND (window), urgent);
    }
}

static gboolean
get_effective_window_decorations (GdkWindow       *window,
                                  GdkWMDecoration *decoration)
{
  GdkWindowImplWin32 *impl;

  impl = (GdkWindowImplWin32 *)((GdkWindowObject *)window)->impl;

  if (gdk_window_get_decorations (window, decoration))
    return TRUE;
    
  if (((GdkWindowObject *) window)->window_type != GDK_WINDOW_TOPLEVEL &&
      ((GdkWindowObject *) window)->window_type != GDK_WINDOW_DIALOG)
    return FALSE;

  if ((impl->hint_flags & GDK_HINT_MIN_SIZE) &&
      (impl->hint_flags & GDK_HINT_MAX_SIZE) &&
      impl->hints.min_width == impl->hints.max_width &&
      impl->hints.min_height == impl->hints.max_height)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MAXIMIZE;
      if (impl->type_hint == GDK_WINDOW_TYPE_HINT_DIALOG ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_MENU ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
	*decoration |= GDK_DECOR_MINIMIZE;
      else if (impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN)
	*decoration |= GDK_DECOR_MENU | GDK_DECOR_MINIMIZE;

      return TRUE;
    }
  else if (impl->hint_flags & GDK_HINT_MAX_SIZE)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_MAXIMIZE;
      if (impl->type_hint == GDK_WINDOW_TYPE_HINT_DIALOG ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_MENU ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
	*decoration |= GDK_DECOR_MINIMIZE;
      return TRUE;
    }
  else
    {
      switch (impl->type_hint)
	{
	case GDK_WINDOW_TYPE_HINT_DIALOG:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_WINDOW_TYPE_HINT_MENU:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_WINDOW_TYPE_HINT_TOOLBAR:
	  gdk_window_set_skip_taskbar_hint (window, TRUE);
	  *decoration = GDK_DECOR_ALL | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE;
	  return TRUE;

	case GDK_WINDOW_TYPE_HINT_UTILITY:
	  return FALSE;

	case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MENU
                  | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;
	  
	case GDK_WINDOW_TYPE_HINT_DOCK:
	  return FALSE;
	  
	case GDK_WINDOW_TYPE_HINT_DESKTOP:
	  return FALSE;

	default:
	  /* Fall thru */
	case GDK_WINDOW_TYPE_HINT_NORMAL:
	  *decoration = GDK_DECOR_ALL;
	  return TRUE;
	}
    }
    
  return FALSE;
}

void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_geometry_hints: %p\n",
			   GDK_WINDOW_HWND (window)));

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  impl->hint_flags = geom_mask;
  impl->hints = *geometry;

  if (geom_mask & GDK_HINT_POS)
    ; /* even the X11 mplementation doesn't care */

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

  update_style_bits (window);
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_title: %p: %s\n",
			   GDK_WINDOW_HWND (window), title));
  
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
      API_CALL (SetWindowTextW, (GDK_WINDOW_HWND (window), wtitle));
      g_free (wtitle);
    }
  else
    {
      char *cptitle = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);
      API_CALL (SetWindowTextA, (GDK_WINDOW_HWND (window), cptitle));
      g_free (cptitle);
    }
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_role: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   (role ? role : "NULL")));
  /* XXX */
}

void
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  HWND window_id, parent_id;
  GdkWindowImplWin32 *window_impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  g_return_if_fail (GDK_IS_WINDOW (window));

  window_id = GDK_WINDOW_HWND (window);
  parent_id = parent != NULL ? GDK_WINDOW_HWND (parent) : NULL;

  if (GDK_WINDOW_DESTROYED (window) || (parent && GDK_WINDOW_DESTROYED (parent)))
    {
      if (GDK_WINDOW_DESTROYED (window))
	GDK_NOTE (MISC, g_print ("... destroyed!\n"));
      else
	GDK_NOTE (MISC, g_print ("... owner destroyed!\n"));

      return;
    }

  if (((GdkWindowObject *) window)->window_type == GDK_WINDOW_CHILD)
    {
      GDK_NOTE (MISC, g_print ("... a child window!\n"));
      return;
    }

  window_impl->transient_owner = parent;

  /* This changes the *owner* of the window, despite the misleading
   * name. (Owner and parent are unrelated concepts.) At least that's
   * what people who seem to know what they talk about say on
   * USENET. Search on Google.
   */
  SetLastError (0);
  if (SetWindowLong (window_id, GWL_HWNDPARENT, (long) parent_id) == 0 &&
      GetLastError () != 0)
    WIN32_API_FAILED ("SetWindowLong");
}

void
gdk_window_set_background (GdkWindow      *window,
			   const GdkColor *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_background: %p: %s\n",
			   GDK_WINDOW_HWND (window), 
			   _gdk_win32_color_to_string (color)));

  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    {
      g_object_unref (private->bg_pixmap);
      private->bg_pixmap = NULL;
    }
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);
  g_return_if_fail (pixmap == NULL || gdk_drawable_get_depth (window) == gdk_drawable_get_depth (pixmap));
  
  if (pixmap && !gdk_drawable_get_colormap (pixmap))
    {
      g_warning ("gdk_window_set_back_pixmap(): pixmap must have a colormap");
      return;
    }
  
  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  if (parent_relative)
    {
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
      GDK_NOTE (MISC, g_print (G_STRLOC ": setting background pixmap to parent_relative\n"));
    }
  else
    {
      if (pixmap)
	{
	  g_object_ref (pixmap);
	  private->bg_pixmap = pixmap;
	}
      else
	{
	  private->bg_pixmap = GDK_NO_BG;
	}
    }
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowImplWin32 *impl;
  GdkCursorPrivate *cursor_private;
  GdkWindowObject *parent_window;
  HCURSOR hcursor;
  HCURSOR hprevcursor;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!cursor)
    hcursor = NULL;
  else
    hcursor = cursor_private->hcursor;
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_cursor: %p: %p\n",
			   GDK_WINDOW_HWND (window),
			   hcursor));

  /* First get the old cursor, if any (we wait to free the old one
   * since it may be the current cursor set in the Win32 API right
   * now).
   */
  hprevcursor = impl->hcursor;

  if (hcursor == NULL)
    impl->hcursor = NULL;
  else
    {
      /* We must copy the cursor as it is OK to destroy the GdkCursor
       * while still in use for some window. See for instance
       * gimp_change_win_cursor() which calls gdk_window_set_cursor
       * (win, cursor), and immediately afterwards gdk_cursor_destroy
       * (cursor).
       */
      if ((impl->hcursor = CopyCursor (hcursor)) == NULL)
	WIN32_API_FAILED ("CopyCursor");
      GDK_NOTE (MISC, g_print ("... CopyCursor (%p) = %p\n",
			       hcursor, impl->hcursor));
    }

  if (impl->hcursor != NULL)
    {
      /* If the pointer is over our window, set new cursor */
      GdkWindow *curr_window = gdk_window_get_pointer (window, NULL, NULL, NULL);
      if (curr_window == window)
        SetCursor (impl->hcursor);
      else
	{
	  /* Climb up the tree and find whether our window is the
	   * first ancestor that has cursor defined, and if so, set
	   * new cursor.
	   */
	  GdkWindowObject *curr_window_obj = GDK_WINDOW_OBJECT (curr_window);
	  while (curr_window_obj &&
		 !GDK_WINDOW_IMPL_WIN32 (curr_window_obj->impl)->hcursor)
	    {
	      curr_window_obj = curr_window_obj->parent;
	      if (curr_window_obj == GDK_WINDOW_OBJECT (window))
		{
	          SetCursor (impl->hcursor);
		  break;
		}
	    }
	}
    }

  /* Destroy the previous cursor: Need to make sure it's no longer in
   * use before we destroy it, in case we're not over our window but
   * the cursor is still set to our old one.
   */
  if (hprevcursor != NULL)
    {
      if (GetCursor () == hprevcursor)
	{
	  /* Look for a suitable cursor to use instead */
	  hcursor = NULL;
          parent_window = GDK_WINDOW_OBJECT (window)->parent;
          while (hcursor == NULL)
	    {
	      if (parent_window)
		{
		  impl = GDK_WINDOW_IMPL_WIN32 (parent_window->impl);
		  hcursor = impl->hcursor;
		  parent_window = parent_window->parent;
		}
	      else
		{
		  hcursor = LoadCursor (NULL, IDC_ARROW);
		}
	    }
          SetCursor (hcursor);
        }

      GDK_NOTE (MISC, g_print ("... DestroyCursor (%p)\n", hprevcursor));
      
      API_CALL (DestroyCursor, (hprevcursor));
    }
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    window = _gdk_root;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      RECT rect;

      API_CALL (GetClientRect, (GDK_WINDOW_HWND (window), &rect));

      if (window != _gdk_root)
	{
	  POINT pt;
	  GdkWindow *parent = gdk_window_get_parent (window);

	  pt.x = rect.left;
	  pt.y = rect.top;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  ScreenToClient (GDK_WINDOW_HWND (parent), &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;

	  pt.x = rect.right;
	  pt.y = rect.bottom;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  ScreenToClient (GDK_WINDOW_HWND (parent), &pt);
	  rect.right = pt.x;
	  rect.bottom = pt.y;

	  if (parent == _gdk_root)
	    {
	      rect.left += _gdk_offset_x;
	      rect.top += _gdk_offset_y;
	      rect.right += _gdk_offset_x;
	      rect.bottom += _gdk_offset_y;
	    }
	}

      if (x)
	*x = rect.left;
      if (y)
	*y = rect.top;
      if (width)
	*width = rect.right - rect.left;
      if (height)
	*height = rect.bottom - rect.top;
      if (depth)
	*depth = gdk_drawable_get_visual (window)->depth;

      GDK_NOTE (MISC, g_print ("gdk_window_get_geometry: %p: %ldx%ldx%d@%+ld%+ld\n",
			       GDK_WINDOW_HWND (window),
			       rect.right - rect.left, rect.bottom - rect.top,
			       gdk_drawable_get_visual (window)->depth,
			       rect.left, rect.top));
    }
}

gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  gint return_val;
  gint tx = 0;
  gint ty = 0;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      POINT pt;

      pt.x = 0;
      pt.y = 0;
      ClientToScreen (GDK_WINDOW_HWND (window), &pt);
      tx = pt.x;
      ty = pt.y;
      return_val = 1;
    }
  else
    return_val = 0;
  
  if (x)
    *x = tx + _gdk_offset_x;
  if (y)
    *y = ty + _gdk_offset_y;

  GDK_NOTE (MISC, g_print ("gdk_window_get_origin: %p: %+d%+d\n",
			   GDK_WINDOW_HWND (window),
			   tx, ty));
  return return_val;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  return gdk_window_get_origin (window, x, y);
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkRectangle rect;

  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_frame_extents (window, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;

  GDK_NOTE (MISC, g_print ("gdk_window_get_root_origin: %p: %+d%+d\n",
			   GDK_WINDOW_HWND (window), rect.x, rect.y));
}

void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkWindowObject *private;
  HWND hwnd;
  RECT r;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rect != NULL);

  private = GDK_WINDOW_OBJECT (window);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: window is documented to be a toplevel GdkWindow, so is it really
   * necessary to walk its parent chain?
   */
  while (private->parent && ((GdkWindowObject*) private->parent)->parent)
    private = (GdkWindowObject*) private->parent;

  hwnd = GDK_WINDOW_HWND (window);
  API_CALL (GetWindowRect, (hwnd, &r));

  rect->x = r.left + _gdk_offset_x;
  rect->y = r.top + _gdk_offset_y;
  rect->width = r.right - r.left;
  rect->height = r.bottom - r.top;

  GDK_NOTE (MISC, g_print ("gdk_window_get_frame_extents: %p: %ldx%ld@%+ld%+ld\n",
			   GDK_WINDOW_HWND (window),
			   r.right - r.left, r.bottom - r.top,
			   r.left, r.top));
}

GdkWindow*
_gdk_windowing_window_get_pointer (GdkDisplay      *display,
				   GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindow *return_val;
  POINT screen_point, point;
  HWND hwnd, hwndc;
  BYTE kbd[256];

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  
  return_val = NULL;
  GetCursorPos (&screen_point);
  point = screen_point;
  ScreenToClient (GDK_WINDOW_HWND (window), &point);

  *x = point.x;
  *y = point.y;

  if (window == _gdk_root)
    {
      *x += _gdk_offset_x;
      *y += _gdk_offset_y;
    }

  hwnd = WindowFromPoint (screen_point);
  if (hwnd != NULL)
    {
      gboolean done = FALSE;
      
      while (!done)
	{
	  point = screen_point;
	  ScreenToClient (hwnd, &point);
	  hwndc = ChildWindowFromPoint (hwnd, point);
	  if (hwndc == NULL)
	    done = TRUE;
	  else if (hwndc == hwnd)
	    done = TRUE;
	  else
	    hwnd = hwndc;
	}
      
      return_val = gdk_window_lookup ((GdkNativeWindow) hwnd);
    }
  else
    return_val = NULL;
      
  GetKeyboardState (kbd);
  *mask = 0;
  if (kbd[VK_SHIFT] & 0x80)
    *mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    *mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    *mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    *mask |= GDK_MOD1_MASK;
  if (kbd[VK_LBUTTON] & 0x80)
    *mask |= GDK_BUTTON1_MASK;
  if (kbd[VK_MBUTTON] & 0x80)
    *mask |= GDK_BUTTON2_MASK;
  if (kbd[VK_RBUTTON] & 0x80)
    *mask |= GDK_BUTTON3_MASK;
  
  return return_val;
}

void
_gdk_windowing_get_pointer (GdkDisplay       *display,
			    GdkScreen       **screen,
			    gint             *x,
			    gint             *y,
			    GdkModifierType  *mask)
{
  g_return_if_fail (display == _gdk_display);
  
  *screen = _gdk_screen;
  _gdk_windowing_window_get_pointer (_gdk_display, _gdk_root, x, y, mask);
}

void
gdk_display_warp_pointer (GdkDisplay *display,
			  GdkScreen  *screen,
			  gint        x,
			  gint        y)
{
  g_return_if_fail (display == _gdk_display);
  g_return_if_fail (screen == _gdk_screen);

  SetCursorPos (x - _gdk_offset_x, y - _gdk_offset_y);
}

GdkWindow*
_gdk_windowing_window_at_pointer (GdkDisplay *display,
				  gint       *win_x,
				  gint       *win_y)
{
  GdkWindow *window;
  POINT point, pointc;
  HWND hwnd, hwndc;
  RECT rect;

  GetCursorPos (&pointc);
  point = pointc;
  hwnd = WindowFromPoint (point);

  if (hwnd == NULL)
    {
      window = _gdk_root;
      *win_x = pointc.x + _gdk_offset_x;
      *win_y = pointc.y + _gdk_offset_y;
      return window;
    }
      
  ScreenToClient (hwnd, &point);

  do {
    hwndc = ChildWindowFromPoint (hwnd, point);
    ClientToScreen (hwnd, &point);
    ScreenToClient (hwndc, &point);
  } while (hwndc != hwnd && (hwnd = hwndc, 1));

  window = gdk_win32_handle_table_lookup ((GdkNativeWindow) hwnd);

  if (window && (win_x || win_y))
    {
      GetClientRect (hwnd, &rect);
      *win_x = point.x - rect.left;
      *win_y = point.y - rect.top;
    }

  GDK_NOTE (MISC, g_print ("_gdk_windowing_window_at_pointer: %+d%+d %p%s\n",
			   *win_x, *win_y,
			   hwnd,
			   (window == NULL ? " NULL" : "")));

  return window;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  return GDK_WINDOW_OBJECT (window)->event_mask;
}

void          
gdk_window_set_events (GdkWindow   *window,
		       GdkEventMask event_mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* gdk_window_new() always sets the GDK_STRUCTURE_MASK, so better
   * set it here, too. Not that I know or remember why it is
   * necessary, will have to test some day.
   */
  GDK_WINDOW_OBJECT (window)->event_mask = GDK_STRUCTURE_MASK | event_mask;
}

static void
do_shape_combine_region (GdkWindow *window,
			 HRGN	    hrgn,
			 gint       x, gint y)
{
  RECT rect;

  GetClientRect (GDK_WINDOW_HWND (window), &rect);
  _gdk_win32_adjust_client_rect (window, &rect);

  OffsetRgn (hrgn, -rect.left, -rect.top);
  OffsetRgn (hrgn, x, y);

  /* If this is a top-level window, add the title bar to the region */
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
    {
      HRGN tmp = CreateRectRgn (0, 0, rect.right - rect.left, -rect.top);
      CombineRgn (hrgn, hrgn, tmp, RGN_OR);
      DeleteObject (tmp);
    }
  
  SetWindowRgn (GDK_WINDOW_HWND (window), hrgn, TRUE);
}

void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!mask)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %p: none\n",
			       GDK_WINDOW_HWND (window)));
      SetWindowRgn (GDK_WINDOW_HWND (window), NULL, TRUE);

      private->shaped = FALSE;
    }
  else
    {
      HRGN hrgn;

      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %p: %p\n",
			       GDK_WINDOW_HWND (window),
			       GDK_WINDOW_HWND (mask)));

      /* Convert mask bitmap to region */
      hrgn = _gdk_win32_bitmap_to_hrgn (mask);

      do_shape_combine_region (window, hrgn, x, y);

      private->shaped = TRUE;
    }
}

void 
gdk_window_input_shape_combine_mask (GdkWindow *window,
				     GdkBitmap *mask,
				     gint       x,
				     gint       y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* Not yet implemented
   *
   * I don't think there is anything in the Win32 API to directly
   * support this. And anyway, as we don't currently support RGBA
   * windows, it doesn't really matter.
   *
   * When we do support RGBA, input shape functionality could probably
   * be implemented by saving the input shape region in the per-window
   * private data, and then simply checking before generating an input
   * event whether the event's coordinates are inside the region.
   */
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean   override_redirect)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  g_warning ("gdk_window_set_override_redirect not implemented");
}

void
gdk_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;  
  
  accept_focus = accept_focus != FALSE;

  if (private->accept_focus != accept_focus)
    private->accept_focus = accept_focus;
}

void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;  
  
  focus_on_map = focus_on_map != FALSE;

  if (private->focus_on_map != focus_on_map)
    private->focus_on_map = focus_on_map;
}

void          
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  GdkPixbuf *pixbuf, *big_pixbuf, *small_pixbuf;
  gint big_diff, small_diff;
  gint big_w, big_h, small_w, small_h;
  gint w, h;
  gint dw, dh, diff;
  HICON small_hicon, big_hicon;
  GdkWindowImplWin32 *impl;
  gint i, big_i, small_i;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  /* ideal sizes for small and large icons */
  big_w = GetSystemMetrics (SM_CXICON);
  big_h = GetSystemMetrics (SM_CYICON);
  small_w = GetSystemMetrics (SM_CXSMICON);
  small_h = GetSystemMetrics (SM_CYSMICON);

  /* find closest sized icons in the list */
  big_pixbuf = NULL;
  small_pixbuf = NULL;
  big_diff = 0;
  small_diff = 0;
  i = 0;
  while (pixbufs)
    {
      pixbuf = (GdkPixbuf*) pixbufs->data;
      w = gdk_pixbuf_get_width (pixbuf);
      h = gdk_pixbuf_get_height (pixbuf);

      dw = ABS (w - big_w);
      dh = ABS (h - big_h);
      diff = dw*dw + dh*dh;
      if (big_pixbuf == NULL || diff < big_diff)
	{
	  big_pixbuf = pixbuf;
	  big_diff = diff;
	  big_i = i;
	}

      dw = ABS (w - small_w);
      dh = ABS (h - small_h);
      diff = dw*dw + dh*dh;
      if (small_pixbuf == NULL || diff < small_diff)
	{
	  small_pixbuf = pixbuf;
	  small_diff = diff;
	  small_i = i;
	}

      pixbufs = g_list_next (pixbufs);
      i++;
    }

  /* Create the icons */
  big_hicon = _gdk_win32_pixbuf_to_hicon (big_pixbuf);
  small_hicon = _gdk_win32_pixbuf_to_hicon (small_pixbuf);

  /* Set the icons */
  SendMessage (GDK_WINDOW_HWND (window), WM_SETICON, ICON_BIG,
	       (LPARAM)big_hicon);
  SendMessage (GDK_WINDOW_HWND (window), WM_SETICON, ICON_SMALL,
  	       (LPARAM)small_hicon);

  /* Store the icons, destroying any previous icons */
  if (impl->hicon_big)
    GDI_CALL (DestroyIcon, (impl->hicon_big));
  impl->hicon_big = big_hicon;
  if (impl->hicon_small)
    GDI_CALL (DestroyIcon, (impl->hicon_small));
  impl->hicon_small = small_hicon;
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* do nothing, use gdk_window_set_icon_list instead */
}

void
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
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
  API_CALL (SetWindowText, (GDK_WINDOW_HWND (window), name));
#endif
}

GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD, NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;
  
  g_warning ("gdk_window_get_group not yet implemented");

  return NULL;
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  g_return_if_fail (leader == NULL || GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (leader))
    return;
  
  g_warning ("gdk_window_set_group not implemented");
}

static void
update_single_bit (LONG *style,
                   BOOL  all,
		   int   gdk_bit,
		   int   style_bit)
{
  /* all controls the interpretation of gdk_bit -- if all is true, gdk_bit
     indicates whether style_bit is off; if all is false, gdk bit indicate whether
     style_bit is on */
  if ((!all && gdk_bit) || (all && !gdk_bit))
    *style |= style_bit;
  else
    *style &= ~style_bit;
}

static void
update_style_bits (GdkWindow *window)
{
  GdkWMDecoration decorations;
  GdkWMFunction functions;
  LONG style, exstyle;
  BOOL all;
  RECT rect, before, after;

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

  GetClientRect (GDK_WINDOW_HWND (window), &before);
  after = before;
  AdjustWindowRectEx (&before, style, FALSE, exstyle);

  if (get_effective_window_decorations (window, &decorations))
    {
      all = (decorations & GDK_DECOR_ALL);
      update_single_bit (&style, all, decorations & GDK_DECOR_BORDER, WS_BORDER);
      update_single_bit (&style, all, decorations & GDK_DECOR_RESIZEH, WS_THICKFRAME);
      update_single_bit (&style, all, decorations & GDK_DECOR_TITLE, WS_CAPTION);
      update_single_bit (&style, all, decorations & GDK_DECOR_MENU, WS_SYSMENU);
      update_single_bit (&style, all, decorations & GDK_DECOR_MINIMIZE, WS_MINIMIZEBOX);
      update_single_bit (&style, all, decorations & GDK_DECOR_MAXIMIZE, WS_MAXIMIZEBOX);
    }

  /* XXX this is actually incorrect.  The menu entries should be added or removed
     from the system menu without affecting the window style. */
  if (_gdk_window_get_functions (window, &functions))
    {
      all = (functions & GDK_DECOR_ALL);
      update_single_bit (&style, all, functions & GDK_FUNC_RESIZE, WS_THICKFRAME);
      update_single_bit (&style, all, functions & GDK_FUNC_MOVE, WS_THICKFRAME | WS_SYSMENU);
      update_single_bit (&style, all, functions & GDK_FUNC_MINIMIZE, WS_MINIMIZE);
      update_single_bit (&style, all, functions & GDK_FUNC_MOVE, WS_MAXIMIZE);
      update_single_bit (&style, all, functions & GDK_FUNC_CLOSE, WS_SYSMENU);
    }
    
  SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);

  AdjustWindowRectEx (&after, style, FALSE, exstyle);

  GetWindowRect (GDK_WINDOW_HWND (window), &rect);
  rect.left += after.left - before.left;
  rect.top += after.top - before.top;
  rect.right += after.right - before.right;
  rect.bottom += after.bottom - before.bottom;

  SetWindowPos (GDK_WINDOW_HWND (window), NULL,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		SWP_FRAMECHANGED | SWP_NOACTIVATE | 
		SWP_NOREPOSITION | SWP_NOZORDER);
}

static GQuark
get_decorations_quark ()
{
  static GQuark quark = 0;
  
  if (!quark)
    quark = g_quark_from_static_string ("gdk-window-decorations");
  
  return quark;
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  GdkWMDecoration* decorations_copy;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_decorations: %p: %s %s%s%s%s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   (decorations & GDK_DECOR_ALL ? "clearing" : "setting"),
			   (decorations & GDK_DECOR_BORDER ? "BORDER " : ""),
			   (decorations & GDK_DECOR_RESIZEH ? "RESIZEH " : ""),
			   (decorations & GDK_DECOR_TITLE ? "TITLE " : ""),
			   (decorations & GDK_DECOR_MENU ? "MENU " : ""),
			   (decorations & GDK_DECOR_MINIMIZE ? "MINIMIZE " : ""),
			   (decorations & GDK_DECOR_MAXIMIZE ? "MAXIMIZE " : "")));

  decorations_copy = g_malloc (sizeof (GdkWMDecoration));
  *decorations_copy = decorations;
  g_object_set_qdata_full (G_OBJECT (window), get_decorations_quark (), decorations_copy, g_free);

  update_style_bits (window);
}

gboolean
gdk_window_get_decorations (GdkWindow       *window,
			    GdkWMDecoration *decorations)
{
  GdkWMDecoration* decorations_set;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  decorations_set = g_object_get_qdata (G_OBJECT (window), get_decorations_quark ());
  if (decorations_set)
    *decorations = *decorations_set;

  return (decorations_set != NULL);
}

static GQuark
get_functions_quark ()
{
  static GQuark quark = 0;
  
  if (!quark)
    quark = g_quark_from_static_string ("gdk-window-functions");
  
  return quark;
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  GdkWMFunction* functions_copy;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_functions: %p: %s %s%s%s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   (functions & GDK_FUNC_ALL ? "clearing" : "setting"),
			   (functions & GDK_FUNC_RESIZE ? "RESIZE " : ""),
			   (functions & GDK_FUNC_MOVE ? "MOVE " : ""),
			   (functions & GDK_FUNC_MINIMIZE ? "MINIMIZE " : ""),
			   (functions & GDK_FUNC_MAXIMIZE ? "MAXIMIZE " : ""),
			   (functions & GDK_FUNC_CLOSE ? "CLOSE " : "")));

  functions_copy = g_malloc (sizeof (GdkWMFunction));
  *functions_copy = functions;
  g_object_set_qdata_full (G_OBJECT (window), get_functions_quark (), functions_copy, g_free);

  update_style_bits (window);
}

gboolean
_gdk_window_get_functions (GdkWindow     *window,
		           GdkWMFunction *functions)
{
  GdkWMDecoration* functions_set;
  
  functions_set = g_object_get_qdata (G_OBJECT (window), get_functions_quark ());
  if (functions_set)
    *functions = *functions_set;

  return (functions_set != NULL);
}

static void
QueryTree (HWND   hwnd,
	   HWND **children,
	   gint  *nchildren)
{
  guint i, n;
  HWND child;

  n = 0;
  do {
    if (n == 0)
      child = GetWindow (hwnd, GW_CHILD);
    else
      child = GetWindow (child, GW_HWNDNEXT);
    if (child != NULL)
      n++;
  } while (child != NULL);

  if (n > 0)
    {
      *children = g_new (HWND, n);
      for (i = 0; i < n; i++)
	{
	  if (i == 0)
	    child = GetWindow (hwnd, GW_CHILD);
	  else
	    child = GetWindow (child, GW_HWNDNEXT);
	  *children[i] = child;
	}
    }
}

static void
gdk_propagate_shapes (HANDLE   win,
		      gboolean merge)
{
   RECT emptyRect;
   HRGN region, childRegion;
   HWND *list = NULL;
   gint i, num;

   SetRectEmpty (&emptyRect);
   region = CreateRectRgnIndirect (&emptyRect);
   if (merge)
     GetWindowRgn (win, region);
   
   QueryTree (win, &list, &num);
   if (list != NULL)
     {
       WINDOWPLACEMENT placement;

       placement.length = sizeof (WINDOWPLACEMENT);
       /* go through all child windows and combine regions */
       for (i = 0; i < num; i++)
	 {
	   GetWindowPlacement (list[i], &placement);
	   if (placement.showCmd == SW_SHOWNORMAL)
	     {
	       childRegion = CreateRectRgnIndirect (&emptyRect);
	       GetWindowRgn (list[i], childRegion);
	       CombineRgn (region, region, childRegion, RGN_OR);
	       DeleteObject (childRegion);
	     }
	  }
       SetWindowRgn (win, region, TRUE);
       g_free (list);
     }
   else
     DeleteObject (region);
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
   
  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_propagate_shapes (GDK_WINDOW_HWND (window), FALSE);
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_propagate_shapes (GDK_WINDOW_HWND (window), TRUE);
}

void 
gdk_window_set_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* Not yet implemented. See comment in
   * gdk_window_input_shape_combine_mask().
   */
}

void 
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* Not yet implemented. See comment in
   * gdk_window_input_shape_combine_mask().
   */
}

gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return !use_static;
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  WPARAM winedge;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Tell Windows to start interactively resizing the window by pretending that
   * the left pointer button was clicked in the suitable edge or corner. This
   * will only work if the button is down when this function is called, and
   * will only work with button 1 (left), since Windows only allows window
   * dragging using the left mouse button.
   */
  if (button != 1)
    return;
  
  /* Must break the automatic grab that occured when the button was
   * pressed, otherwise it won't work.
   */
  gdk_display_pointer_ungrab (_gdk_display, 0);

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      winedge = HTTOPLEFT;
      break;

    case GDK_WINDOW_EDGE_NORTH:
      winedge = HTTOP;
      break;

    case GDK_WINDOW_EDGE_NORTH_EAST:
      winedge = HTTOPRIGHT;
      break;

    case GDK_WINDOW_EDGE_WEST:
      winedge = HTLEFT;
      break;

    case GDK_WINDOW_EDGE_EAST:
      winedge = HTRIGHT;
      break;

    case GDK_WINDOW_EDGE_SOUTH_WEST:
      winedge = HTBOTTOMLEFT;
      break;

    case GDK_WINDOW_EDGE_SOUTH:
      winedge = HTBOTTOM;
      break;

    case GDK_WINDOW_EDGE_SOUTH_EAST:
    default:
      winedge = HTBOTTOMRIGHT;
      break;
    }

  DefWindowProc (GDK_WINDOW_HWND (window), WM_NCLBUTTONDOWN, winedge,
		 MAKELPARAM (root_x - _gdk_offset_x, root_y - _gdk_offset_y));
}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Tell Windows to start interactively moving the window by pretending that
   * the left pointer button was clicked in the titlebar. This will only work
   * if the button is down when this function is called, and will only work
   * with button 1 (left), since Windows only allows window dragging using the
   * left mouse button.
   */
  if (button != 1)
    return;
  
  /* Must break the automatic grab that occured when the button was pressed,
   * otherwise it won't work.
   */
  gdk_display_pointer_ungrab (_gdk_display, 0);

  DefWindowProc (GDK_WINDOW_HWND (window), WM_NCLBUTTONDOWN, HTCAPTION,
		 MAKELPARAM (root_x - _gdk_offset_x, root_y - _gdk_offset_y));
}


/*
 * Setting window states
 */
void
gdk_window_iconify (GdkWindow *window)
{
  HWND old_active_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_iconify: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      old_active_window = GetActiveWindow ();
      ShowWindow (GDK_WINDOW_HWND (window), SW_MINIMIZE);
      if (old_active_window != GDK_WINDOW_HWND (window))
	SetActiveWindow (old_active_window);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_ICONIFIED);
    }
}

void
gdk_window_deiconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_deiconify: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      show_window_internal (window, FALSE, TRUE);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_ICONIFIED,
                                   0);
    }
}

void
gdk_window_stick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

void
gdk_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

void
gdk_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_maximize: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_MAXIMIZE);
  else
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_MAXIMIZED);
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_unmaximize: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

typedef struct _FullscreenInfo FullscreenInfo;

struct _FullscreenInfo
{
  RECT  r;
  guint hint_flags;
  LONG  style;
};

void
gdk_window_fullscreen (GdkWindow *window)
{
  gint width, height;
  FullscreenInfo *fi;
  GdkWindowObject *private = (GdkWindowObject *) window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  fi = g_new (FullscreenInfo, 1);

  if (!GetWindowRect (GDK_WINDOW_HWND (window), &(fi->r)))
    g_free (fi);
  else
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

      width = GetSystemMetrics (SM_CXSCREEN);
      height = GetSystemMetrics (SM_CYSCREEN);
 
      /* remember for restoring */
      fi->hint_flags = impl->hint_flags;
      impl->hint_flags &= ~GDK_HINT_MAX_SIZE;
      g_object_set_data (G_OBJECT (window), "fullscreen-info", fi);
      fi->style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);

      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, 
                     (fi->style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_TOP,
			       0, 0, width, height,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW));

      gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
    }
}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  FullscreenInfo *fi;
  GdkWindowObject *private = (GdkWindowObject *) window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

      impl->hint_flags = fi->hint_flags;
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, fi->style);
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_NOTOPMOST,
			       fi->r.left, fi->r.top,
			       fi->r.right - fi->r.left, fi->r.bottom - fi->r.top,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW));
      
      g_object_set_data (G_OBJECT (window), "fullscreen-info", NULL);
      g_free (fi);

      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
    }
}

void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window),
			       setting ? HWND_TOPMOST : HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }
  else
    gdk_synthesize_window_state (window,
    				 setting ? GDK_WINDOW_STATE_BELOW : GDK_WINDOW_STATE_ABOVE,
				 setting ? GDK_WINDOW_STATE_ABOVE : 0);
}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window),
			       setting ? HWND_BOTTOM : HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }
  else
    gdk_synthesize_window_state (window,
				 setting ? GDK_WINDOW_STATE_ABOVE : GDK_WINDOW_STATE_BELOW,
				 setting ? GDK_WINDOW_STATE_BELOW : 0);
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  GDK_NOTE (MISC, g_print ("gdk_window_focus: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (((GdkWindowObject *) window)->state & GDK_WINDOW_STATE_MAXIMIZED)
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWMAXIMIZED);
  else
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);
  SetFocus (GDK_WINDOW_HWND (window));
}

void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = (GdkWindowObject*) window;

  private->modal_hint = modal;

#if 0
  /* Not sure about this one.. -- Cody */
  if (GDK_WINDOW_IS_MAPPED (window))
    API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), 
			     modal ? HWND_TOPMOST : HWND_NOTOPMOST,
			     0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE));
#endif
}

void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
				  gboolean   skips_taskbar)
{
  static GdkWindow *owner = NULL;
  GdkWindowAttr wa;

  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (MISC, g_print ("gdk_window_set_skip_taskbar_hint: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   skips_taskbar ? "TRUE" : "FALSE"));

  if (skips_taskbar)
    {
      if (owner == NULL)
	{
	  wa.window_type = GDK_WINDOW_TEMP;
	  wa.wclass = GDK_INPUT_OUTPUT;
	  wa.width = wa.height = 1;
	  wa.event_mask = 0;
	  owner = gdk_window_new_internal (NULL, &wa, 0, TRUE);
	}

      SetWindowLong (GDK_WINDOW_HWND (window), GWL_HWNDPARENT,
		     (long) GDK_WINDOW_HWND (owner));

#if 0 /* Should we also turn off the minimize and maximize buttons? */
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE,
		     GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE) & ~(WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU));
      SetWindowPos (GDK_WINDOW_HWND (window), NULL,
		    0, 0, 0, 0,
		    SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
		    SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
#endif
    }
  else
    {
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_HWNDPARENT, 0);
    }
}

void
gdk_window_set_skip_pager_hint (GdkWindow *window,
				gboolean   skips_pager)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_type_hint: %p: %d\n",
			   GDK_WINDOW_HWND (window), hint));

  ((GdkWindowImplWin32 *)((GdkWindowObject *)window)->impl)->type_hint = hint;

  update_style_bits (window);
}

GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
  
  if (GDK_WINDOW_DESTROYED (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;

  return GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->type_hint;
}

void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape_region,
                                 gint       offset_x,
                                 gint       offset_y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!shape_region)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_region: %p: none\n",
			       GDK_WINDOW_HWND (window)));
      SetWindowRgn (GDK_WINDOW_HWND (window), NULL, TRUE);

      private->shaped = FALSE;
    }
  else
    {
      HRGN hrgn;

      hrgn = _gdk_win32_gdkregion_to_hrgn (shape_region, 0, 0);
      
      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_region: %p: %p\n",
			       GDK_WINDOW_HWND (window),
			       hrgn));

      do_shape_combine_region (window, hrgn, offset_x, offset_y);

      private->shaped = TRUE;
    }
}

void 
gdk_window_input_shape_combine_region (GdkWindow *window,
				       GdkRegion *shape_region,
				       gint       offset_x,
				       gint       offset_y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* Not yet implemented. See comment in
   * gdk_window_input_shape_combine_mask().
   */
}

GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (display == _gdk_display, NULL);

  return gdk_window_lookup (anid);
}

void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_configure_finished (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}
