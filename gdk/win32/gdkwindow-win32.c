/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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

#include <stdlib.h>

#include "gdk.h" /* gdk_rectangle_intersect */
#include "gdkevents.h"
#include "gdkpixmap.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

static gboolean gdk_window_gravity_works (void);
static void     gdk_window_set_static_win_gravity (GdkWindow *window, 
						   gboolean   on);

static GdkColormap* gdk_window_impl_win32_get_colormap (GdkDrawable *drawable);
static void         gdk_window_impl_win32_set_colormap (GdkDrawable *drawable,
							GdkColormap *cmap);
static void         gdk_window_impl_win32_get_size     (GdkDrawable *drawable,
							gint *width,
							gint *height);
static GdkRegion*  gdk_window_impl_win32_get_visible_region (GdkDrawable *drawable);
static void gdk_window_impl_win32_init       (GdkWindowImplWin32      *window);
static void gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass);
static void gdk_window_impl_win32_finalize   (GObject                 *object);

static gpointer parent_class = NULL;

GType
gdk_window_impl_win32_get_type (void)
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
  return gdk_window_impl_win32_get_type ();
}

static void
gdk_window_impl_win32_init (GdkWindowImplWin32 *impl)
{
  impl->width = 1;
  impl->height = 1;

  impl->event_mask = 0;
  impl->hcursor = NULL;
  impl->hint_flags = 0;
  impl->extension_events_selected = FALSE;
  impl->input_locale = GetKeyboardLayout (0);
  TranslateCharsetInfo ((DWORD FAR *) GetACP (), &impl->charset_info,
			TCI_SRCCODEPAGE);
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
      if (!DestroyCursor (window_impl->hcursor))
        WIN32_GDI_FAILED("DestroyCursor");
      window_impl->hcursor = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GdkColormap*
gdk_window_impl_win32_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *drawable_impl;
  GdkWindowImplWin32 *window_impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable), NULL);

  drawable_impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  window_impl = GDK_WINDOW_IMPL_WIN32 (drawable);

  if (!((GdkWindowObject *) drawable_impl->wrapper)->input_only && 
      drawable_impl->colormap == NULL)
    {
      drawable_impl->colormap = gdk_colormap_get_system ();
      gdk_colormap_ref (drawable_impl->colormap);
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
  g_return_if_fail (gdk_colormap_get_visual (cmap) != gdk_drawable_get_visual (drawable));

  impl = GDK_WINDOW_IMPL_WIN32 (drawable);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  GDK_DRAWABLE_GET_CLASS (draw_impl)->set_colormap (drawable, cmap);
  
  /* XXX */
  g_print("gdk_window_impl_win32_set_colormap: XXX\n");
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
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;
  RECT rect;
  guint width;
  guint height;

  g_assert (_gdk_parent_root == NULL);
  
  SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
  width  = rect.right - rect.left;
  height = rect.bottom - rect.top;

  _gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)_gdk_parent_root;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  
  draw_impl->handle = gdk_root_window;
  draw_impl->wrapper = GDK_DRAWABLE (private);
  
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = gdk_visual_get_system ()->depth;
  impl->width = width;
  impl->height = height;

  gdk_win32_handle_table_insert (&gdk_root_window, _gdk_parent_root);
}

/* The Win API function AdjustWindowRect may return negative values
 * resulting in obscured title bars. This helper function is coreccting it.
 */
BOOL
SafeAdjustWindowRectEx (RECT* lpRect,
			DWORD dwStyle, 
			BOOL  bMenu, 
			DWORD dwExStyle)
{
  if (!AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle))
    {
      WIN32_API_FAILED ("AdjustWindowRectEx");
      return FALSE;
    }
  if (lpRect->left < 0)
    {
      lpRect->right -= lpRect->left;
      lpRect->left = 0;
    }
  if (lpRect->top < 0)
    {
      lpRect->bottom -= lpRect->top;
      lpRect->top = 0;
    }
  return TRUE;
}

/* RegisterGdkClass
 *   is a wrapper function for RegisterWindowClassEx.
 *   It creates at least one unique class for every 
 *   GdkWindowType. If support for single window-specific icons
 *   is ever needed (e.g Dialog specific), every such window should
 *   get its own class
 */
ATOM
RegisterGdkClass (GdkWindowType wtype)
{
  static ATOM klassTOPLEVEL = 0;
  static ATOM klassDIALOG   = 0;
  static ATOM klassCHILD    = 0;
  static ATOM klassTEMP     = 0;
  static HICON hAppIcon = NULL;
  static WNDCLASSEX wcl; 
  ATOM klass = 0;

  wcl.cbSize = sizeof(WNDCLASSEX);     
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = gdk_window_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = gdk_app_hmodule;
  wcl.hIcon = 0;
  /* initialize once! */
  if (0 == hAppIcon)
    {
      gchar sLoc [_MAX_PATH+1];

      if (0 != GetModuleFileName (gdk_app_hmodule, sLoc, _MAX_PATH))
	{
	  hAppIcon = ExtractIcon (gdk_app_hmodule, sLoc, 0);
	  if (0 == hAppIcon)
	    {
	      char *gdklibname = g_strdup_printf ("gdk-%s.dll", GDK_VERSION);

	      hAppIcon = ExtractIcon (gdk_app_hmodule, gdklibname, 0);
	      g_free (gdklibname);
	    }
	  
	  if (0 == hAppIcon) 
	    hAppIcon = LoadIcon (NULL, IDI_APPLICATION);
	}
    }

  wcl.lpszMenuName = NULL;
  wcl.hIconSm = 0;

  /* initialize once per class */
  /*
   * HB: Setting the background brush leads to flicker, because we
   * don't get asked how to clear the background. This is not what
   * we want, at least not for input_only windows ...
   */
#define ONCE_PER_CLASS() \
  wcl.hIcon = CopyIcon (hAppIcon); \
  wcl.hIconSm = CopyIcon (hAppIcon); \
  wcl.hbrBackground = NULL; /* CreateSolidBrush (RGB (0,0,0)); */ \
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW); 
  
  switch (wtype)
  {
  case GDK_WINDOW_TOPLEVEL:
    if (0 == klassTOPLEVEL)
      {
	wcl.lpszClassName = "gdkWindowToplevel";
	
	ONCE_PER_CLASS();
	klassTOPLEVEL = RegisterClassEx (&wcl);
      }
    klass = klassTOPLEVEL;
    break;

    case GDK_WINDOW_CHILD:
      if (0 == klassCHILD)
	{
	  wcl.lpszClassName = "gdkWindowChild";
	  
	  wcl.style |= CS_PARENTDC; /* MSDN: ... enhances system performance. */
	  ONCE_PER_CLASS();
	  klassCHILD = RegisterClassEx (&wcl);
	}
      klass = klassCHILD;
      break;

  case GDK_WINDOW_DIALOG:
    if (0 == klassDIALOG)
      {
	wcl.lpszClassName = "gdkWindowDialog";
        wcl.style |= CS_SAVEBITS;
	ONCE_PER_CLASS();
	klassDIALOG = RegisterClassEx (&wcl);
      }
    klass = klassDIALOG;
    break;

  case GDK_WINDOW_TEMP:
    if (0 == klassTEMP)
      {
	wcl.lpszClassName = "gdkWindowTemp";
        wcl.style |= CS_SAVEBITS;
	ONCE_PER_CLASS();
	klassTEMP = RegisterClassEx (&wcl);
      }
    klass = klassTEMP;
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

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowObject *parent_private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;

  GdkVisual *visual;

  HANDLE hparent;
  ATOM klass = 0;
  DWORD dwStyle, dwExStyle;
  RECT rect;
  Visual *xvisual;

  int width, height;
  int x, y;
  char *title;
  char *mbtitle;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    parent = _gdk_parent_root;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  GDK_NOTE (MISC,
	    g_print ("gdk_window_new: %s\n",
		     (attributes->window_type == GDK_WINDOW_TOPLEVEL ? "TOPLEVEL" :
		      (attributes->window_type == GDK_WINDOW_CHILD ? "CHILD" :
		       (attributes->window_type == GDK_WINDOW_DIALOG ? "DIALOG" :
			(attributes->window_type == GDK_WINDOW_TEMP ? "TEMP" :
			 "???"))))));

  parent_private = (GdkWindowObject*) parent;
  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;
  
  hparent = GDK_WINDOW_HWND (parent);

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  private->parent = (GdkWindowObject *)parent;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = CW_USEDEFAULT;
  
  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else if (attributes_mask & GDK_WA_X)
    y = 100;			/* ??? We must put it somewhere... */
  else
    y = 0;			/* x is CW_USEDEFAULT, y doesn't matter then */
  
  private->x = x;
  private->y = y;
  impl->width = (attributes->width > 1) ? (attributes->width) : (1);
  impl->height = (attributes->height > 1) ? (attributes->height) : (1);
  impl->extension_events_selected = FALSE;
  private->window_type = attributes->window_type;

  _gdk_window_init_position (GDK_WINDOW (private));
  if (impl->position_info.big)
    private->guffaw_gravity = TRUE;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = g_get_prgname ();
  if (!title || !*title)
    title = "GDK client window";

  impl->event_mask = GDK_STRUCTURE_MASK | attributes->event_mask;
      
  if (parent_private && parent_private->guffaw_gravity)
    {
      /* XXX ??? */
    }

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      dwExStyle = 0;

      private->input_only = FALSE;
      private->depth = visual->depth;
      
      if (attributes_mask & GDK_WA_COLORMAP)
	{
	  draw_impl->colormap = attributes->colormap;
          gdk_colormap_ref (attributes->colormap);
        }
      else
	{
	  if ((((GdkVisualPrivate*)gdk_visual_get_system ())->xvisual) == xvisual)
            {
              draw_impl->colormap = gdk_colormap_get_system ();
              gdk_colormap_ref (draw_impl->colormap);
	      GDK_NOTE (MISC, g_print ("...using system colormap %p\n",
				       draw_impl->colormap));
            }
	  else
            {
              draw_impl->colormap = gdk_colormap_new (visual, FALSE);
	      GDK_NOTE (MISC, g_print ("...using new colormap %p\n",
				       draw_impl->colormap));
            }
	}
    }
  else
    {
      dwExStyle = WS_EX_TRANSPARENT;
      private->depth = 0;
      private->input_only = TRUE;
      draw_impl->colormap = gdk_colormap_get_system ();
      gdk_colormap_ref (draw_impl->colormap);
      GDK_NOTE (MISC, g_print ("...GDK_INPUT_ONLY, system colormap\n"));
    }

  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
      dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
      hparent = gdk_root_window;
      break;

    case GDK_WINDOW_CHILD:
      dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      break;

    case GDK_WINDOW_DIALOG:
      dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_CLIPCHILDREN;
#if 0
      dwExStyle |= WS_EX_TOPMOST; /* //HB: want this? */
#endif
      hparent = gdk_root_window;
      break;

    case GDK_WINDOW_TEMP:
      dwStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      /* a temp window is not necessarily a top level window */
      dwStyle |= (_gdk_parent_root == parent ? WS_POPUP : WS_CHILDWINDOW);
      dwExStyle |= WS_EX_TOOLWINDOW;
      break;

    case GDK_WINDOW_ROOT:
      g_error ("cannot make windows of type GDK_WINDOW_ROOT");
      break;
    }

  klass = RegisterGdkClass (private->window_type);

  if (private->window_type != GDK_WINDOW_CHILD)
    {
      if (x == CW_USEDEFAULT)
	{
	  rect.left = 100;
	  rect.top = 100;
	}
      else
	{
	  rect.left = x;
	  rect.top = y;
	}

      rect.right = rect.left + impl->width;
      rect.bottom = rect.top + impl->height;

      SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);

      if (x != CW_USEDEFAULT)
	{
	  x = rect.left;
	  y = rect.top;
	}
      width = rect.right - rect.left;
      height = rect.bottom - rect.top;
    }
  else
    {
      width = impl->width;
      height = impl->height;
    }

  mbtitle = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);
  
#ifdef WITHOUT_WM_CREATE
  draw_impl->handle = CreateWindowEx (dwExStyle,
				      MAKEINTRESOURCE(klass),
				      mbtitle,
				      dwStyle,
				      x, y, 
				      width, height,
				      hparent,
				      NULL,
				      gdk_app_hmodule,
				      NULL);
#else
  {
  HWND hwndNew =
    CreateWindowEx (dwExStyle,
		    MAKEINTRESOURCE(klass),
		    mbtitle,
		    dwStyle,
		    x, y, 
		    width, height,
		    hparent,
		    NULL,
		    gdk_app_hmodule,
		    window);
  if (GDK_WINDOW_HWND (window) != hwndNew)
    {
      g_warning("gdk_window_new: gdk_event_translate::WM_CREATE (%#x, %#x) HWND mismatch.",
		(guint) GDK_WINDOW_HWND (window),
		(guint) hwndNew);

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
      GDK_WINDOW_HWND (window) = hwndNew;
# endif

    }
  }
  gdk_drawable_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
#endif

  GDK_NOTE (MISC,
	    g_print ("... \"%s\" %dx%d@+%d+%d %#x = %#x\n"
		     "... locale %#x codepage %d\n",
		     mbtitle,
		     width, height, (x == CW_USEDEFAULT ? -9999 : x), y, 
		     (guint) hparent,
		     (guint) GDK_WINDOW_HWND (window),
		     (guint) impl->input_locale,
		     (guint) impl->charset_info.ciACP));

  g_free (mbtitle);

  if (draw_impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowEx");
      g_object_unref ((GObject *) window);
      return NULL;
    }

#ifdef WITHOUT_WM_CREATE
  gdk_drawable_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
#endif

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  return window;
}

GdkWindow *
gdk_window_foreign_new (GdkNativeWindow anid)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowObject *parent_private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;

  HANDLE parent;
  RECT rect;
  POINT point;

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  parent = GetParent ((HWND)anid);
  
  private->parent = gdk_win32_handle_table_lookup ((GdkNativeWindow) parent);
  
  parent_private = (GdkWindowObject *)private->parent;
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);

  draw_impl->handle = (HWND) anid;
  GetClientRect ((HWND) anid, &rect);
  point.x = rect.left;
  point.y = rect.right;
  ClientToScreen ((HWND) anid, &point);
  if (parent != gdk_root_window)
    ScreenToClient (parent, &point);
  private->x = point.x;
  private->y = point.y;
  impl->width = rect.right - rect.left;
  impl->height = rect.bottom - rect.top;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;
  if (IsWindowVisible ((HWND) anid))
    private->state &= (~GDK_WINDOW_STATE_WITHDRAWN);
  else
    private->state |= GDK_WINDOW_STATE_WITHDRAWN;
  private->depth = gdk_visual_get_system ()->depth;

  gdk_drawable_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);

  return window;
}

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("_gdk_windowing_window_destroy %#x\n",
			   (guint) GDK_WINDOW_HWND (window)));

  if (private->extension_events != 0)
    gdk_input_window_destroy (window);

  if (private->window_type == GDK_WINDOW_FOREIGN)
    {
      if (!foreign_destroy && (private->parent != NULL))
	{
	  /* It's somebody else's window, but in our hierarchy,
	   * so reparent it to the root window, and then call
	   * DestroyWindow() on it.
	   */
	  gdk_window_hide (window);
	  gdk_window_reparent (window, NULL, 0, 0);
	  
	  /* Is this too drastic? Many (most?) applications
	   * quit if any window receives WM_QUIT I think.
	   * OTOH, I don't think foreign windows are much
	   * used, so the question is maybe academic.
	   */
	  PostMessage (GDK_WINDOW_HWND (window), WM_QUIT, 0, 0);
	}
    }
  else if (!recursing && !foreign_destroy)
    {
      private->destroyed = TRUE;
      DestroyWindow (GDK_WINDOW_HWND (window));
    }
}

/* This function is called when the window really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_window_destroy_notify: %#x  %s\n",
		     (guint) GDK_WINDOW_HWND (window),
		     (GDK_WINDOW_DESTROYED (window) ? "(destroyed)" : "")));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("window %#x unexpectedly destroyed",
		   (guint) GDK_WINDOW_HWND (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
  gdk_drawable_unref (window);
}

static void
show_window_internal (GdkWindow *window,
                      gboolean   raise)
{
  GdkWindowObject *private;
  
  private = GDK_WINDOW_OBJECT (window);

  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_show: %#x\n",
			       (guint) GDK_WINDOW_HWND (window)));

      private->state &= (~GDK_WINDOW_STATE_WITHDRAWN);
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)
	{
	  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNOACTIVATE);
	  SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
#if 0
	  /* Don't put on toolbar */
	  ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
#endif
	}
      else
	{
          if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
	    {
	      SetWindowPos(GDK_WINDOW_HWND (window), HWND_TOP, 0, 0, 0, 0,
			   SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);
	    }
          else
            {
	      GdkWindow *parent = GDK_WINDOW (private->parent);

	      /* Todo: GDK_WINDOW_STATE_STICKY */
	      if (private->state & GDK_WINDOW_STATE_ICONIFIED)
	        ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWMINIMIZED);
	      else if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
	        ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWMAXIMIZED);
	      else
	        {
	          ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);
	          ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
	        }
              if (parent == _gdk_parent_root)
                SetForegroundWindow (GDK_WINDOW_HWND (window));
	      if (raise)
	        BringWindowToTop (GDK_WINDOW_HWND (window));
#if 0
	      ShowOwnedPopups (GDK_WINDOW_HWND (window), TRUE);
#endif
	    }
	}
    }
}

void
gdk_window_show_unraised (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  show_window_internal (window, FALSE);
}

void
gdk_window_show (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, TRUE);
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);

  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_hide: %#x\n",
			       (guint) GDK_WINDOW_HWND (window)));

      private->state |= GDK_WINDOW_STATE_WITHDRAWN;
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
	ShowOwnedPopups (GDK_WINDOW_HWND (window), FALSE);

      if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
	{
	  SetWindowPos(GDK_WINDOW_HWND (window), HWND_BOTTOM, 0, 0, 0, 0,
		       SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
	}
      else
	{
	  ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
	}
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_withdraw: %#x\n",
			       (guint) GDK_WINDOW_HWND (window)));

      gdk_window_hide (window);	/* ??? */
    }
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, x, y,
				       impl->width, impl->height);
      else
	{
	  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
	                     x, y, impl->width, impl->height,
	                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER))
	    WIN32_API_FAILED ("SetWindowPos");
	}
    }
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowObject *private = (GdkWindowObject*) window;
  GdkWindowImplWin32 *impl;
  int x, y;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, private->x, private->y,
				       width, height);
      else
	{
	  POINT pt;
	  RECT rect;
	  DWORD dwStyle;
	  DWORD dwExStyle;

	  pt.x = 0;
	  pt.y = 0; 
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;
	  rect.right = pt.x + width;
	  rect.bottom = pt.y + height;

	  dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	  dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	  if (!AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	    WIN32_API_FAILED ("AdjustWindowRectEx");

	  x = rect.left;
	  y = rect.top;
	  width = rect.right - rect.left;
	  height = rect.bottom - rect.top;
	  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
	                     x, y, width, height,
	                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER))
	    WIN32_API_FAILED ("SetWindowPos");
	}
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

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

  if (!private->destroyed)
    {
      RECT rect;
      DWORD dwStyle;
      DWORD dwExStyle;

      GDK_NOTE (MISC, g_print ("gdk_window_move_resize: %#x %dx%d@+%d+%d\n",
			       (guint) GDK_WINDOW_HWND (window),
			       width, height, x, y));
      
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, x, y, width, height);
      else
	{
	  rect.left = x;
	  rect.top = y;
	  rect.right = x + width;
	  rect.bottom = y + height;

	  dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	  dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	  if (!AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	    WIN32_API_FAILED ("AdjustWindowRectEx");

	  GDK_NOTE (MISC, g_print ("...SetWindowPos(%#x,%ldx%ld@+%ld+%ld)\n",
				   (guint) GDK_WINDOW_HWND (window),
				   rect.right - rect.left, rect.bottom - rect.top,
				   rect.left, rect.top));
	  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
	                     rect.left, rect.top,
	                     rect.right - rect.left, rect.bottom - rect.top,
	                     SWP_NOACTIVATE | SWP_NOZORDER))
	    WIN32_API_FAILED ("SetWindowPos");
	}
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

  g_return_if_fail (window != NULL);

  if (!new_parent)
    new_parent = _gdk_parent_root;

  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject *) window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  impl = GDK_WINDOW_IMPL_WIN32 (window_private->impl);

  if (!GDK_WINDOW_DESTROYED (window) && !GDK_WINDOW_DESTROYED (new_parent))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_reparent: %#x %#x\n",
			       (guint) GDK_WINDOW_HWND (window),
			       (guint) GDK_WINDOW_HWND (new_parent)));
      if (!SetParent (GDK_WINDOW_HWND (window),
		      GDK_WINDOW_HWND (new_parent)))
	WIN32_API_FAILED ("SetParent");

      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       x, y, impl->width, impl->height, TRUE))
	WIN32_API_FAILED ("MoveWindow");
    }
  
  window_private->parent = (GdkWindowObject *)new_parent;

  if (old_parent_private)
    old_parent_private->children =
      g_list_remove (old_parent_private->children, window);

  if ((old_parent_private &&
       (!old_parent_private->guffaw_gravity != !parent_private->guffaw_gravity)) ||
      (!old_parent_private && parent_private->guffaw_gravity))
    gdk_window_set_static_win_gravity (window, parent_private->guffaw_gravity);
  
  parent_private->children = g_list_prepend (parent_private->children, window);
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      HDC hdc;

      if (width == 0)
	width = impl->width - x;
      if (height == 0)
	height = impl->height - y;
      GDK_NOTE (MISC, g_print ("_gdk_windowing_window_clear_area: "
			       "%#x %dx%d@+%d+%d\n",
			       (guint) GDK_WINDOW_HWND (window),
			       width, height, x, y));
      hdc = GetDC (GDK_WINDOW_HWND (window));
      IntersectClipRect (hdc, x, y, x + width + 1, y + height + 1);
      SendMessage (GDK_WINDOW_HWND (window), WM_ERASEBKGND, (WPARAM) hdc, 0);
      ReleaseDC (GDK_WINDOW_HWND (window), hdc);
    }
}

void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      RECT rect;

      GDK_NOTE (MISC, g_print ("_gdk_windowing_window_clear_area_e: "
			       "%#x %dx%d@+%d+%d\n",
			       (guint) GDK_WINDOW_HWND (window),
			       width, height, x, y));

      rect.left = x;
      rect.right = x + width + 1;
      rect.top = y;
      rect.bottom = y + height + 1;
      if (!InvalidateRect (GDK_WINDOW_HWND (window), &rect, TRUE))
	WIN32_GDI_FAILED ("InvalidateRect");
      UpdateWindow (GDK_WINDOW_HWND (window));
    }
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_raise: %#x\n",
			       (guint) GDK_WINDOW_HWND (window)));

      if (!BringWindowToTop (GDK_WINDOW_HWND (window)))
	WIN32_API_FAILED ("BringWindowToTop");
    }
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_lower: %#x\n",
			       (guint) GDK_WINDOW_HWND (window)));

      if (!SetWindowPos (GDK_WINDOW_HWND (window), HWND_BOTTOM, 0, 0, 0, 0,
			 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE))
	WIN32_API_FAILED ("SetWindowPos");
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
  GdkWindowImplWin32 *impl;
  WINDOWPLACEMENT size_hints;
  RECT rect;
  DWORD dwStyle;
  DWORD dwExStyle;
  int diff;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  GDK_NOTE (MISC, g_print ("gdk_window_set_hints: %#x %dx%d..%dx%d @+%d+%d\n",
			   (guint) GDK_WINDOW_HWND (window),
			   min_width, min_height, max_width, max_height,
			   x, y));

  impl->hint_flags = flags;
  size_hints.length = sizeof (size_hints);

  if (flags)
    {
      if (flags & GDK_HINT_POS)
	{
	  if (!GetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
	    WIN32_API_FAILED ("GetWindowPlacement");
	  else
	    {
	      GDK_NOTE (MISC, g_print ("...rcNormalPosition:"
				       " (%ld,%ld)--(%ld,%ld)\n",
				       size_hints.rcNormalPosition.left,
				       size_hints.rcNormalPosition.top,
				       size_hints.rcNormalPosition.right,
				       size_hints.rcNormalPosition.bottom));
	      /* What are the corresponding window coordinates for client
	       * area coordinates x, y
	       */
	      rect.left = x;
	      rect.top = y;
	      rect.right = rect.left + 200;	/* dummy */
	      rect.bottom = rect.top + 200;
	      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
	      size_hints.flags = 0;
	      size_hints.showCmd = SW_SHOWNA;
	      
	      /* Set the normal position hint to that location, with unchanged
	       * width and height.
	       */
	      diff = size_hints.rcNormalPosition.left - rect.left;
	      size_hints.rcNormalPosition.left = rect.left;
	      size_hints.rcNormalPosition.right -= diff;
	      diff = size_hints.rcNormalPosition.top - rect.top;
	      size_hints.rcNormalPosition.top = rect.top;
	      size_hints.rcNormalPosition.bottom -= diff;
	      GDK_NOTE (MISC, g_print ("...setting: (%ld,%ld)--(%ld,%ld)\n",
				       size_hints.rcNormalPosition.left,
				       size_hints.rcNormalPosition.top,
				       size_hints.rcNormalPosition.right,
				       size_hints.rcNormalPosition.bottom));
	      if (!SetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
		WIN32_API_FAILED ("SetWindowPlacement");
	      impl->hint_x = rect.left;
	      impl->hint_y = rect.top;
	    }
	}

      if (flags & GDK_HINT_MIN_SIZE)
	{
	  rect.left = 0;
	  rect.top = 0;
	  rect.right = min_width;
	  rect.bottom = min_height;
	  dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	  dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	  SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
	  impl->hint_min_width = rect.right - rect.left;
	  impl->hint_min_height = rect.bottom - rect.top;

	  /* Also chek if he current size of the window is in bounds. */
	  GetClientRect (GDK_WINDOW_HWND (window), &rect);
	  if (rect.right < min_width && rect.bottom < min_height)
	    gdk_window_resize (window, min_width, min_height);
	  else if (rect.right < min_width)
	    gdk_window_resize (window, min_width, rect.bottom);
	  else if (rect.bottom < min_height)
	    gdk_window_resize (window, rect.right, min_height);
	}

      if (flags & GDK_HINT_MAX_SIZE)
	{
	  rect.left = 0;
	  rect.top = 0;
	  rect.right = max_width;
	  rect.bottom = max_height;
	  dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	  dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	  AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
	  impl->hint_max_width = rect.right - rect.left;
	  impl->hint_max_height = rect.bottom - rect.top;
	  /* Again, check if the window is too large currently. */
	  GetClientRect (GDK_WINDOW_HWND (window), &rect);
	  if (rect.right > max_width && rect.bottom > max_height)
	    gdk_window_resize (window, max_width, max_height);
	  else if (rect.right > max_width)
	    gdk_window_resize (window, max_width, rect.bottom);
	  else if (rect.bottom > max_height)
	    gdk_window_resize (window, rect.right, max_height);
	}
    }
}

void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  GdkWindowImplWin32 *impl;
  WINDOWPLACEMENT size_hints;
  RECT rect;
  DWORD dwStyle;
  DWORD dwExStyle;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  size_hints.length = sizeof (size_hints);

  impl->hint_flags = geom_mask;

  if (geom_mask & GDK_HINT_POS)
    ; /* even the X11 mplementation doesn't care */

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      rect.left = 0;
      rect.top = 0;
      rect.right = geometry->min_width;
      rect.bottom = geometry->min_height;
      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      impl->hint_min_width = rect.right - rect.left;
      impl->hint_min_height = rect.bottom - rect.top;

      /* Also check if he current size of the window is in bounds */
      GetClientRect (GDK_WINDOW_HWND (window), &rect);
      if (rect.right < geometry->min_width
	  && rect.bottom < geometry->min_height)
	gdk_window_resize (window, geometry->min_width, geometry->min_height);
      else if (rect.right < geometry->min_width)
	gdk_window_resize (window, geometry->min_width, rect.bottom);
      else if (rect.bottom < geometry->min_height)
	gdk_window_resize (window, rect.right, geometry->min_height);
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      rect.left = 0;
      rect.top = 0;
      rect.right = geometry->max_width;
      rect.bottom = geometry->max_height;
      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
      /* HB: dont' know why AdjustWindowRectEx is called here, ... */
      SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      impl->hint_max_width = rect.right - rect.left;
      impl->hint_max_height = rect.bottom - rect.top;
      /* ... but negative sizes are always wrong */
      if (impl->hint_max_width < 0) impl->hint_max_width = G_MAXSHORT;
      if (impl->hint_max_height < 0) impl->hint_max_height = G_MAXSHORT;

      /* Again, check if the window is too large currently. */
      GetClientRect (GDK_WINDOW_HWND (window), &rect);
      if (rect.right > geometry->max_width
	  && rect.bottom > geometry->max_height)
	gdk_window_resize (window, geometry->max_width, geometry->max_height);
      else if (rect.right > geometry->max_width)
	gdk_window_resize (window, geometry->max_width, rect.bottom);
      else if (rect.bottom > geometry->max_height)
	gdk_window_resize (window, rect.right, geometry->max_height);
    }
  
  /* I don't know what to do when called with zero base_width and height. */
  if (geom_mask & GDK_HINT_BASE_SIZE
      && geometry->base_width > 0
      && geometry->base_height > 0)
    {
      if (!GetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
	WIN32_API_FAILED ("GetWindowPlacement");
      else
	{
	  GDK_NOTE (MISC, g_print ("gdk_window_set_geometry_hints:"
				   " rcNormalPosition: (%ld,%ld)--(%ld,%ld)\n",
				   size_hints.rcNormalPosition.left,
				   size_hints.rcNormalPosition.top,
				   size_hints.rcNormalPosition.right,
				   size_hints.rcNormalPosition.bottom));
	  size_hints.rcNormalPosition.right =
	    size_hints.rcNormalPosition.left + geometry->base_width;
	  size_hints.rcNormalPosition.bottom =
	    size_hints.rcNormalPosition.top + geometry->base_height;
	  GDK_NOTE (MISC, g_print ("...setting: rcNormal: (%ld,%ld)--(%ld,%ld)\n",
				   size_hints.rcNormalPosition.left,
				   size_hints.rcNormalPosition.top,
				   size_hints.rcNormalPosition.right,
				   size_hints.rcNormalPosition.bottom));
	  if (!SetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
	    WIN32_API_FAILED ("SetWindowPlacement");
	}
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      /* XXX */
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      /* XXX */
    }
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  char *mbtitle;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_title: %#x %s\n",
			   (guint) GDK_WINDOW_HWND (window), title));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      /* As the title is in UTF-8 we must translate it
       * to the system codepage.
       */
      mbtitle = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);
      if (!SetWindowText (GDK_WINDOW_HWND (window), mbtitle))
	WIN32_API_FAILED ("SetWindowText");

      g_free (mbtitle);
    }
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_role: %#x %s\n",
			   (guint) GDK_WINDOW_HWND (window),
			   (role ? role : "NULL")));
  /* XXX */
}

void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  HWND window_id, parent_id;
  LONG style;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_transient_for: %#x %#x\n",
			   (guint) GDK_WINDOW_HWND (window),
			   (guint) GDK_WINDOW_HWND (parent)));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (parent))
    return;

  window_id = GDK_WINDOW_HWND (window);
  parent_id = GDK_WINDOW_HWND (parent);

  if ((style = GetWindowLong (window_id, GWL_STYLE)) == 0)
    WIN32_API_FAILED ("GetWindowLong");

  style |= WS_POPUP;
#if 0 /* not sure if we want to do this */
  style &= ~(WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
#endif

  if (!SetWindowLong (window_id, GWL_STYLE, style))
    WIN32_API_FAILED ("SetWindowLong");
#if 0 /* not sure if we want to do this, clipping to parent size! */
  if (!SetParent (window_id, parent_id))
	WIN32_API_FAILED ("SetParent");
#else /* make the modal window topmost instead */
  if (!SetWindowPos (window_id, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE))
    WIN32_API_FAILED ("SetWindowPos");
#endif

  if (!RedrawWindow (window_id, NULL, NULL, 
                     RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW))
    WIN32_API_FAILED ("RedrawWindow");
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_background: %#x %s\n",
			   (guint) GDK_WINDOW_HWND (window), 
			   gdk_win32_color_to_string (color)));

  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    {
      gdk_drawable_unref (private->bg_pixmap);
      private->bg_pixmap = NULL;
    }
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);
  
  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    gdk_drawable_unref (private->bg_pixmap);

  if (parent_relative)
    {
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
    }
  else
    {
      if (pixmap)
	{
	  gdk_drawable_ref (pixmap);
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
  HCURSOR hcursor;
  HCURSOR hprevcursor;
  POINT pt;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (!cursor)
	hcursor = NULL;
      else
	hcursor = cursor_private->hcursor;

      GDK_NOTE (MISC, g_print ("gdk_window_set_cursor: %#x %#x\n",
			       (guint) GDK_WINDOW_HWND (window),
			       (guint) hcursor));
      hprevcursor = impl->hcursor;
      if (hcursor == NULL)
	impl->hcursor = NULL;
      else
	{
	  /* We must copy the cursor as it is OK to destroy the GdkCursor
	   * while still in use for some window. See for instance
	   * gimp_change_win_cursor() which calls
	   * gdk_window_set_cursor (win, cursor), and immediately
	   * afterwards gdk_cursor_destroy (cursor).
	   */
	  impl->hcursor = CopyCursor (hcursor);
	  GDK_NOTE (MISC, g_print ("...CopyCursor (%#x) = %#x\n",
				   (guint) hcursor, (guint) impl->hcursor));

	  if (hprevcursor != NULL && GetCursor () == hprevcursor)
	    SetCursor (impl->hcursor);

	  if (hprevcursor != NULL)
	    {
	      GDK_NOTE (MISC, g_print ("...DestroyCursor (%#x)\n",
				       (guint) hprevcursor));
	  
	      if (!DestroyCursor (hprevcursor))
		WIN32_API_FAILED ("DestroyCursor");
	    }
	}
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
    window = _gdk_parent_root;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      RECT rect;

      if (!GetClientRect (GDK_WINDOW_HWND (window), &rect))
	WIN32_API_FAILED ("GetClientRect");

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

  g_return_val_if_fail (window != NULL, 0);

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
    *x = tx;
  if (y)
    *y = ty;

  GDK_NOTE (MISC, g_print ("gdk_window_get_origin: %#x: +%d+%d\n",
			   (guint) GDK_WINDOW_HWND (window),
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
  GdkWindowObject *rover;
  POINT pt;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  rover = (GdkWindowObject*) window;
  if (x)
    *x = 0;
  if (y)
    *y = 0;

  if (GDK_WINDOW_DESTROYED (window))
    return;
      
  while (rover->parent && ((GdkWindowObject*) rover->parent)->parent)
    rover = (GdkWindowObject *) rover->parent;
  if (rover->destroyed)
    return;

  pt.x = 0;
  pt.y = 0;
  ClientToScreen (GDK_WINDOW_HWND (rover), &pt);
  if (x)
    *x = pt.x;
  if (y)
    *y = pt.y;

  GDK_NOTE (MISC, g_print ("gdk_window_get_root_origin: %#x: (%#x) +%ld+%ld\n",
			   (guint) GDK_WINDOW_HWND (window),
			   (guint) GDK_WINDOW_HWND (rover),
			   pt.x, pt.y));
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

  while (private->parent && ((GdkWindowObject*) private->parent)->parent)
    private = (GdkWindowObject*) private->parent;

  hwnd = GDK_WINDOW_HWND (window);
  /* find the frame window */
  while (HWND_DESKTOP != GetParent (hwnd))
    {
      hwnd = GetParent (hwnd);
      g_return_if_fail (NULL != hwnd);
    }

  if (!GetWindowRect (hwnd, &r))
    WIN32_API_FAILED ("GetWindowRect");

  rect->x = r.left;
  rect->y = r.right;
  rect->width = r.right - r.left;
  rect->height = r.bottom - r.top;
}

GdkWindow*
_gdk_windowing_window_get_pointer (GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindow *return_val;
  POINT pointc, point;
  HWND hwnd, hwndc;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  
  if (!window)
    window = _gdk_parent_root;

  return_val = NULL;
  GetCursorPos (&pointc);
  point = pointc;
  ScreenToClient (GDK_WINDOW_HWND (window), &point);

  if (x)
    *x = point.x;
  if (y)
    *y = point.y;

  hwnd = WindowFromPoint (point);
  point = pointc;
  ScreenToClient (hwnd, &point);
  
  do {
    hwndc = ChildWindowFromPoint (hwnd, point);
    ClientToScreen (hwnd, &point);
    ScreenToClient (hwndc, &point);
  } while (hwndc != hwnd && (hwnd = hwndc, 1));	/* Ouch! */

  return_val = gdk_win32_handle_table_lookup ((GdkNativeWindow) hwnd);

  if (mask)
    {
      BYTE kbd[256];

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
    }
  
  return return_val;
}

GdkWindow*
_gdk_windowing_window_at_pointer (gint *win_x,
				  gint *win_y)
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
      window = _gdk_parent_root;
      if (win_x)
	*win_x = pointc.x;
      if (win_y)
	*win_y = pointc.y;
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
      if (win_x)
	*win_x = point.x - rect.left;
      if (win_y)
	*win_y = point.y - rect.top;
    }

  GDK_NOTE (MISC, g_print ("gdk_window_at_pointer: +%ld+%ld %#x%s\n",
			   point.x, point.y,
			   (guint) hwnd,
			   (window == NULL ? " NULL" : "")));

  return window;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  return GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl)->event_mask;
}

void          
gdk_window_set_events (GdkWindow   *window,
		       GdkEventMask event_mask)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl)->event_mask = event_mask;
}

void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!mask)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %#x none\n",
			       (guint) GDK_WINDOW_HWND (window)));
      SetWindowRgn (GDK_WINDOW_HWND (window), NULL, TRUE);
    }
  else
    {
      HRGN hrgn;
      DWORD dwStyle;
      DWORD dwExStyle;
      RECT rect;

      /* Convert mask bitmap to region */
      hrgn = BitmapToRegion (GDK_WINDOW_HWND (mask));

      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %#x %#x\n",
			       (guint) GDK_WINDOW_HWND (window),
			       (guint) GDK_WINDOW_HWND (mask)));

      /* SetWindowRgn wants window (not client) coordinates */ 
      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
      GetClientRect (GDK_WINDOW_HWND (window), &rect);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      OffsetRgn (hrgn, -rect.left, -rect.top);

      OffsetRgn (hrgn, x, y);

      /* If this is a top-level window, add the title bar to the region */
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
	{
	  CombineRgn (hrgn, hrgn,
		      CreateRectRgn (0, 0, rect.right - rect.left, -rect.top),
		      RGN_OR);
	}
      
      SetWindowRgn (GDK_WINDOW_HWND (window), hrgn, TRUE);
    }
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean   override_redirect)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  g_warning ("gdk_window_set_override_redirect not implemented");
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  /* Nothing to do, really. As we share window classes between windows
   * we can't have window-specific icons, sorry. Don't print any warning
   * either.
   */
}

void
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (!SetWindowText (GDK_WINDOW_HWND (window), name))
    WIN32_API_FAILED ("SetWindowText");
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (leader != NULL);
  g_return_if_fail (GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (leader))
    return;
  
  g_warning ("gdk_window_set_group not implemented");
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  LONG style, exstyle;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

  style &= (WS_OVERLAPPED|WS_POPUP|WS_CHILD|WS_MINIMIZE|WS_VISIBLE|WS_DISABLED
	    |WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZE);

  exstyle &= (WS_EX_TOPMOST|WS_EX_TRANSPARENT);

  if (decorations & GDK_DECOR_ALL)
    style |= (WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
  if (decorations & GDK_DECOR_BORDER)
    style |= (WS_BORDER);
  if (decorations & GDK_DECOR_RESIZEH)
    style |= (WS_THICKFRAME);
  if (decorations & GDK_DECOR_TITLE)
    style |= (WS_CAPTION);
  if (decorations & GDK_DECOR_MENU)
    style |= (WS_SYSMENU);
  if (decorations & GDK_DECOR_MINIMIZE)
    style |= (WS_MINIMIZEBOX);
  if (decorations & GDK_DECOR_MAXIMIZE)
    style |= (WS_MAXIMIZEBOX);
  
  SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  LONG style, exstyle;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

  style &= (WS_OVERLAPPED|WS_POPUP|WS_CHILD|WS_MINIMIZE|WS_VISIBLE|WS_DISABLED
	    |WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZE|WS_CAPTION|WS_BORDER
	    |WS_SYSMENU);

  exstyle &= (WS_EX_TOPMOST|WS_EX_TRANSPARENT);

  if (functions & GDK_FUNC_ALL)
    style |= (WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
  if (functions & GDK_FUNC_RESIZE)
    style |= (WS_THICKFRAME);
  if (functions & GDK_FUNC_MOVE)
    style |= (WS_THICKFRAME);
  if (functions & GDK_FUNC_MINIMIZE)
    style |= (WS_MINIMIZEBOX);
  if (functions & GDK_FUNC_MAXIMIZE)
    style |= (WS_MAXIMIZEBOX);
  
  SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
}

/* 
 * propagate the shapes from all child windows of a GDK window to the parent 
 * window. Shamelessly ripped from Enlightenment's code
 * 
 * - Raster
 */

static void
QueryTree (HWND hwnd,
	   HWND **children,
	   gint *nchildren)
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
     }
   else
     DeleteObject (region);
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
   
  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_propagate_shapes (GDK_WINDOW_HWND (window), FALSE);
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_propagate_shapes (GDK_WINDOW_HWND (window), TRUE);
}

/* Support for windows that can be guffaw-scrolled
 * (See http://www.gtk.org/~otaylor/whitepapers/guffaw-scrolling.txt)
 */

static gboolean
gdk_window_gravity_works (void)
{
  enum { UNKNOWN, NO, YES };
  static gint gravity_works = UNKNOWN;
  
  if (gravity_works == UNKNOWN)
    {
      GdkWindowAttr attr;
      GdkWindow *parent;
      GdkWindow *child;
      gint y;
      
      attr.window_type = GDK_WINDOW_TEMP;
      attr.wclass = GDK_INPUT_OUTPUT;
      attr.x = 0;
      attr.y = 0;
      attr.width = 100;
      attr.height = 100;
      attr.event_mask = 0;
      
      parent = gdk_window_new (NULL, &attr, GDK_WA_X | GDK_WA_Y);
      
      attr.window_type = GDK_WINDOW_CHILD;
      child = gdk_window_new (parent, &attr, GDK_WA_X | GDK_WA_Y);
      
      gdk_window_set_static_win_gravity (child, TRUE);
      
      gdk_window_resize (parent, 100, 110);
      gdk_window_move (parent, 0, -10);
      gdk_window_move_resize (parent, 0, 0, 100, 100);
      
      gdk_window_resize (parent, 100, 110);
      gdk_window_move (parent, 0, -10);
      gdk_window_move_resize (parent, 0, 0, 100, 100);
      
      gdk_window_get_geometry (child, NULL, &y, NULL, NULL, NULL);
      
      gdk_window_destroy (parent);
      gdk_window_destroy (child);
      
      gravity_works = ((y == -20) ? YES : NO);
    }
  
  return (gravity_works == YES);
}

static void
gdk_window_set_static_bit_gravity (GdkWindow *window, gboolean on)
{
  g_return_if_fail (window != NULL);

  GDK_NOTE (MISC, g_print ("gdk_window_set_static_bit_gravity: Not implemented\n"));
}

static void
gdk_window_set_static_win_gravity (GdkWindow *window, gboolean on)
{
  g_return_if_fail (window != NULL);

  GDK_NOTE (MISC,
	    g_print ("gdk_window_set_static_win_gravity: Not implemented\n"));
}

/*************************************************************
 * gdk_window_set_static_gravities:
 *     Set the bit gravity of the given window to static,
 *     and flag it so all children get static subwindow
 *     gravity.
 *   arguments:
 *     window: window for which to set static gravity
 *     use_static: Whether to turn static gravity on or off.
 *   results:
 *     Does the XServer support static gravity?
 *************************************************************/

gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GList *tmp_list;
  
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!use_static == !private->guffaw_gravity)
    return TRUE;
  
  if (use_static && !gdk_window_gravity_works ())
    return FALSE;
  
  private->guffaw_gravity = use_static;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      gdk_window_set_static_bit_gravity (window, use_static);
      
      tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_set_static_win_gravity (window, use_static);
	  
	  tmp_list = tmp_list->next;
	}
    }
  
  return TRUE;
}

/*
 * Setting window states
 */
void
gdk_window_iconify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      ShowWindow (GDK_WINDOW_HWND (window), SW_MINIMIZE);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_ICONIFIED);
    }
}

void
gdk_window_deiconify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      gdk_window_show (window);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
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

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      /* "stick" means stick to all desktops _and_ do not scroll with the
       * viewport. i.e. glue to the monitor glass in all cases.
       */
      g_warning ("gdk_window_stick (0x%X) ???", GDK_WINDOW_HWND (window));
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_STICKY);
    }
}

void
gdk_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      g_warning ("gdk_window_unstick (0x%X) ???", GDK_WINDOW_HWND (window));
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_STICKY,
                                   0);

    }
}

void
gdk_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

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

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);
}

void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  GdkWindowObject *private;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = (GdkWindowObject*) window;

  private->modal_hint = modal;

  if (GDK_WINDOW_IS_MAPPED (window))
    if (!SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOPMOST,
                           0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
      WIN32_API_FAILED ("SetWindowPos");
}

void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  GdkAtom atom;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DIALOG", FALSE);
      break;
    case GDK_WINDOW_TYPE_HINT_MENU:
      atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_MENU", FALSE);
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_TOOLBAR", FALSE);
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_window_set_type_hint", hint);
      /* Fall thru */
    case GDK_WINDOW_TYPE_HINT_NORMAL:
      atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_NORMAL", FALSE);
      break;
    }
  /*
   * XXX ???
   */
  GDK_NOTE (MISC,
            g_print ("gdk_window_set_type_hint (0x%0X)\n",
                     GDK_WINDOW_HWND (window)));
}

void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape_region,
                                 gint       offset_x,
                                 gint       offset_y)
{
  gint xoffset, yoffset;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* XXX: even on X implemented conditional ... */  
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* XXX: isn't all this default on win32 ... */  
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

  /* XXX: isn't all this default on win32 ... */  
}
