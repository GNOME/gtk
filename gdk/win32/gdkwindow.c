/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <gdk/gdk.h>
#include "gdkprivate.h"
#include "gdkinput.h"

/* The Win API function AdjustWindowRect may return negative values
 * resulting in obscured title bars. This helper function is coreccting it.
 */
BOOL
SafeAdjustWindowRectEx (RECT* lpRect, 
			DWORD dwStyle, 
			BOOL bMenu, 
			DWORD dwExStyle)
{
  if (!AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle))
    return FALSE;
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

/* Forward declarations */
static gboolean gdk_window_gravity_works (void);
static void     gdk_window_set_static_win_gravity (GdkWindow *window, 
						   gboolean   on);

/* 
 * The following fucntion by The Rasterman <raster@redhat.com>
 * This function returns the X Window ID in which the x y location is in 
 * (x and y being relative to the root window), excluding any windows listed
 * in the GList excludes (this is a list of X Window ID's - gpointer being
 * the Window ID).
 * 
 * This is primarily designed for internal gdk use - for DND for example
 * when using a shaped icon window as the drag object - you exclude the
 * X Window ID of the "icon" (perhaps more if excludes may be needed) and
 * You can get back an X Window ID as to what X Window ID is infact under
 * those X,Y co-ordinates.
 */
HWND
gdk_window_xid_at_coords (gint     x,
			  gint     y,
			  GList   *excludes,
			  gboolean excl_child)
{
  POINT pt;
  gboolean warned = FALSE;

  pt.x = x;
  pt.y = y;
  /* This is probably not correct, just a quick hack */

  if (!warned)
    {
      g_warning ("gdk_window_xid_at_coords probably not implemented correctly");
      warned = TRUE;
    }

  /* XXX */
  return WindowFromPoint (pt);
}

void
gdk_window_init (void)
{
  unsigned int width;
  unsigned int height;
#if 0
  width = GetSystemMetrics (SM_CXSCREEN);
  height = GetSystemMetrics (SM_CYSCREEN);
#else
  { RECT r; /* //HB: don't obscure tray window (task bar) */
    SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0);
    width  = r.right - r.left;
    height = r.bottom - r.top;
  }
#endif

  gdk_root_parent.xwindow = gdk_root_window;
  gdk_root_parent.window_type = GDK_WINDOW_ROOT;
  gdk_root_parent.window.user_data = NULL;
  gdk_root_parent.width = width;
  gdk_root_parent.height = height;
  gdk_root_parent.children = NULL;
  gdk_root_parent.colormap = NULL;
  gdk_root_parent.ref_count = 1;

  gdk_xid_table_insert (&gdk_root_window, &gdk_root_parent);
}

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  GdkVisual *visual;
  HANDLE xparent;
  Visual *xvisual;
#ifdef MULTIPLE_WINDOW_CLASSES
  WNDCLASSEX wcl; 
  ATOM klass;
  char wcl_name_buf[20];
  static int wcl_cnt = 0;
#else
  static WNDCLASSEX wcl; 
  static ATOM klass = 0;
#endif
  static HICON hAppIcon = NULL;
  DWORD dwStyle, dwExStyle;
  RECT rect;
  int width, height;
  int x, y;
  char *title;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    parent = (GdkWindow*) &gdk_root_parent;

  parent_private = (GdkWindowPrivate*) parent;
  if (parent_private->destroyed)
    return NULL;

  xparent = parent_private->xwindow;

  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;

  private->parent = parent;

  private->destroyed = FALSE;
  private->mapped = FALSE;
  private->guffaw_gravity = FALSE;
  private->resize_count = 0;
  private->ref_count = 1;

  private->x = (attributes_mask & GDK_WA_X) ? attributes->x : 0;
  private->y = (attributes_mask & GDK_WA_Y) ? attributes->y : 0;

  private->width = (attributes->width > 1) ? (attributes->width) : (1);
  private->height = (attributes->height > 1) ? (attributes->height) : (1);
  private->window_type = attributes->window_type;
  private->extension_events = 0;
  private->extension_events_selected = FALSE;

  private->filters = NULL;
  private->children = NULL;

  window->user_data = NULL;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = g_get_prgname ();

  private->event_mask = GDK_STRUCTURE_MASK | attributes->event_mask;
  private->bg_type = GDK_WIN32_BG_NORMAL;
  private->hint_flags = 0;
  
#ifndef MULTIPLE_WINDOW_CLASSES
  if (klass == 0)
    {
#endif
  wcl.cbSize = sizeof (WNDCLASSEX);
#if 1
  wcl.style = CS_HREDRAW | CS_VREDRAW;
#else
  wcl.style = 0;
#endif
  wcl.lpfnWndProc = gdk_WindowProc;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = gdk_ProgInstance;
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);

#if 0 /* tml: orig -> generates SetClassLong errors in set background */
  wcl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
  wcl.hbrBackground = NULL;
#else
  /* initialize once! */
  if (0 == hAppIcon)
    {
      gchar sLoc [_MAX_PATH+1];
      HINSTANCE hInst = GetModuleHandle(NULL);

      if (0 != GetModuleFileName(hInst, sLoc, _MAX_PATH))
	{
	  hAppIcon = ExtractIcon(hInst, sLoc, 0);
	  if (0 == hAppIcon)
	    {
	      char *gdklibname = g_strdup_printf ("gdk-%s.dll", GDK_VERSION);

	      hAppIcon = ExtractIcon(hInst, gdklibname, 0);
	      g_free (gdklibname);
	    }
	  
	  if (0 == hAppIcon) 
	    hAppIcon = LoadIcon (NULL, IDI_APPLICATION);
	}
    }
  wcl.hIcon = CopyIcon (hAppIcon);
  wcl.hIconSm = CopyIcon (hAppIcon);
  /* HB: starting with black to have something to release ... */
  wcl.hbrBackground = CreateSolidBrush( RGB(0,0,0));
#endif

  wcl.lpszMenuName = NULL;
#ifdef MULTIPLE_WINDOW_CLASSES
  sprintf (wcl_name_buf, "gdk-wcl-%d", wcl_cnt++);
  wcl.lpszClassName = g_strdup (wcl_name_buf);
  /* wcl.hIconSm = LoadIcon (NULL, IDI_APPLICATION); */
#else
  wcl.lpszClassName = "GDK-window-class";
  klass = RegisterClassEx (&wcl);
  if (!klass)
    g_error ("RegisterClassEx failed");
    }

  private->xcursor = NULL;
#endif
      
  if (parent_private && parent_private->guffaw_gravity)
    {
      /* XXX ??? */
    }

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      dwExStyle = 0;
      if (attributes_mask & GDK_WA_COLORMAP)
	private->colormap = attributes->colormap;
      else
	private->colormap = gdk_colormap_get_system ();
    }
  else
    {
      dwExStyle = WS_EX_TRANSPARENT;
      private->colormap = NULL;
      private->bg_type = GDK_WIN32_BG_TRANSPARENT;
      private->bg_pixmap = NULL;
    }

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = CW_USEDEFAULT;

  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else if (attributes_mask & GDK_WA_X)
    y = 100;			/* ??? We must put it somewhere... */
  else
    y = 500;			/* x is CW_USEDEFAULT, y doesn't matter then */

  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
      dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
      xparent = gdk_root_window;
      break;
    case GDK_WINDOW_CHILD:
      dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      break;
    case GDK_WINDOW_DIALOG:
      dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_CLIPCHILDREN;
      xparent = gdk_root_window;
      break;
    case GDK_WINDOW_TEMP:
      dwStyle = WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
#ifdef MULTIPLE_WINDOW_CLASSES
      wcl.style |= CS_SAVEBITS;
#endif
      dwExStyle |= WS_EX_TOOLWINDOW;
      break;
    case GDK_WINDOW_ROOT:
      g_error ("cannot make windows of type GDK_WINDOW_ROOT");
      break;
    case GDK_WINDOW_PIXMAP:
      g_error ("cannot make windows of type GDK_WINDOW_PIXMAP (use gdk_pixmap_new)");
      break;
    }

#ifdef MULTIPLE_WINDOW_CLASSES
  klass = RegisterClassEx (&wcl);
  if (!klass)
    g_error ("RegisterClassEx failed");
#endif

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

      rect.right = rect.left + private->width;
      rect.bottom = rect.top + private->height;

      if (!SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	g_warning ("gdk_window_new: AdjustWindowRectEx failed");

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
      width = private->width;
      height = private->height;
    }

  private->xwindow =
    CreateWindowEx (dwExStyle,
		    wcl.lpszClassName,
		    title,
		    dwStyle,
		    x, y, 
		    width, height,
		    xparent,
		    NULL,
		    gdk_ProgInstance,
		    NULL);
  GDK_NOTE (MISC,
	    g_print ("gdk_window_create: %s %s %#x %dx%d@+%d+%d %#x = %#x\n",
		     (private->window_type == GDK_WINDOW_TOPLEVEL ? "TOPLEVEL" :
		      (private->window_type == GDK_WINDOW_CHILD ? "CHILD" :
		       (private->window_type == GDK_WINDOW_DIALOG ? "DIALOG" :
			(private->window_type == GDK_WINDOW_TEMP ? "TEMP" :
			 "???")))),
		     title,
		     dwStyle,
		     width, height, (x == CW_USEDEFAULT ? -9999 : x), y, 
		     xparent,
		     private->xwindow));

  gdk_window_ref (window);
  gdk_xid_table_insert (&private->xwindow, window);

  if (private->colormap)
    gdk_colormap_ref (private->colormap);

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  return window;
}

GdkWindow *
gdk_window_foreign_new (guint32 anid)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  HANDLE parent;
  RECT rect;
  POINT point;

  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;

  parent = GetParent ((HWND) anid);
  private->parent = gdk_xid_table_lookup (parent);

  parent_private = (GdkWindowPrivate *)private->parent;
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);

  private->xwindow = (HWND) anid;
  GetClientRect ((HWND) anid, &rect);
  point.x = rect.left;
  point.y = rect.right;
  ClientToScreen ((HWND) anid, &point);
  if (parent != HWND_DESKTOP)
    ScreenToClient (parent, &point);
  private->x = point.x;
  private->y = point.y;
  private->width = rect.right - rect.left;
  private->height = rect.bottom - rect.top;
  private->resize_count = 0;
  private->ref_count = 1;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;
  private->mapped = IsWindowVisible (private->xwindow);
  private->guffaw_gravity = FALSE;
  private->extension_events = 0;
  private->extension_events_selected = FALSE;

  private->colormap = NULL;

  private->filters = NULL;
  private->children = NULL;

  window->user_data = NULL;

  gdk_window_ref (window);
  gdk_xid_table_insert (&private->xwindow, window);

  return window;
}

/* Call this function when you want a window and all its children to
 * disappear.  When xdestroy is true, a request to destroy the XWindow
 * is sent out.  When it is false, it is assumed that the XWindow has
 * been or will be destroyed by destroying some ancestor of this
 * window.
 */
static void
gdk_window_internal_destroy (GdkWindow *window,
			     gboolean   xdestroy,
			     gboolean   our_destroy)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *temp_private;
  GdkWindow *temp_window;
  GList *children;
  GList *tmp;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  GDK_NOTE (MISC, g_print ("gdk_window_internal_destroy %#x\n",
			   private->xwindow));

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
      if (!private->destroyed)
	{
	  if (private->parent)
	    {
	      GdkWindowPrivate *parent_private = (GdkWindowPrivate *)private->parent;
	      if (parent_private->children)
		parent_private->children = g_list_remove (parent_private->children, window);
	    }

	  if (private->window_type != GDK_WINDOW_FOREIGN)
	    {
	      children = tmp = private->children;
	      private->children = NULL;

	      while (tmp)
		{
		  temp_window = tmp->data;
		  tmp = tmp->next;
		  
		  temp_private = (GdkWindowPrivate*) temp_window;
		  if (temp_private)
		    gdk_window_internal_destroy (temp_window, FALSE,
						 our_destroy);
		}

	      g_list_free (children);
	    }

	  if (private->extension_events != 0)
	    gdk_input_window_destroy (window);

	  if (private->filters)
	    {
	      tmp = private->filters;

	      while (tmp)
		{
		  g_free (tmp->data);
		  tmp = tmp->next;
		}

	      g_list_free (private->filters);
	      private->filters = NULL;
	    }
	  
	  if (private->window_type == GDK_WINDOW_FOREIGN)
	    {
	      if (our_destroy && (private->parent != NULL))
		{
		  /* It's somebody elses window, but in our hierarchy,
		   * so reparent it to the root window, and then send
		   * it a delete event, as if we were a WM
		   */
		  gdk_window_hide (window);
		  gdk_window_reparent (window, NULL, 0, 0);
		  
		  /* Is this too drastic? Many (most?) applications
		   * quit if any window receives WM_QUIT I think.
		   * OTOH, I don't think foreign windows are much
		   * used, so the question is maybe academic.
		   */
		  PostMessage (private->xwindow, WM_QUIT, 0, 0);
		}
	    }
	  else if (xdestroy)
	    DestroyWindow (private->xwindow);

	  if (private->colormap)
	    gdk_colormap_unref (private->colormap);

	  private->mapped = FALSE;
	  private->destroyed = TRUE;
	}
      break;

    case GDK_WINDOW_ROOT:
      g_error ("attempted to destroy root window");
      break;

    case GDK_WINDOW_PIXMAP:
      g_error ("called gdk_window_destroy on a pixmap (use gdk_pixmap_unref)");
      break;
    }
}

/* Like internal_destroy, but also destroys the reference created by
   gdk_window_new. */

void
gdk_window_destroy (GdkWindow *window)
{
  gdk_window_internal_destroy (window, TRUE, TRUE);
  gdk_window_unref (window);
}

/* This function is called when the XWindow is really gone.  */

void
gdk_window_destroy_notify (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  GDK_NOTE (EVENTS, g_print ("gdk_window_destroy_notify: %#x  %d\n",
			     private->xwindow, private->destroyed));

  if (!private->destroyed)
    {
      if (private->window_type == GDK_WINDOW_FOREIGN)
	gdk_window_internal_destroy (window, FALSE, FALSE);
      else
	g_warning ("GdkWindow %#lx unexpectedly destroyed", private->xwindow);
    }
  
  gdk_xid_table_remove (private->xwindow);
  gdk_window_unref (window);
}

GdkWindow*
gdk_window_ref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_val_if_fail (window != NULL, NULL);

  private->ref_count += 1;

  GDK_NOTE (MISC, g_print ("gdk_window_ref %#x %d\n",
			   private->xwindow, private->ref_count));

  return window;
}

void
gdk_window_unref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_if_fail (window != NULL);

  private->ref_count -= 1;

  GDK_NOTE (MISC, g_print ("gdk_window_unref %#x %d%s\n",
			   private->xwindow, private->ref_count,
			   (private->ref_count == 0 ? " freeing" : "")));

  if (private->ref_count == 0)
    {
      if (private->bg_type == GDK_WIN32_BG_PIXMAP && private->bg_pixmap != NULL)
	gdk_pixmap_unref (private->bg_pixmap);

      if (!private->destroyed)
	{
	  if (private->window_type == GDK_WINDOW_FOREIGN)
	    gdk_xid_table_remove (private->xwindow);
	  else
	    g_warning ("losing last reference to undestroyed window");
	}
      g_dataset_destroy (window);
      g_free (window);
    }
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_show: %#x\n", private->xwindow));

      private->mapped = TRUE;
      if (private->window_type == GDK_WINDOW_TEMP)
	{
	  ShowWindow (private->xwindow, SW_SHOWNOACTIVATE);
	  SetWindowPos (private->xwindow, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
#if 0
	  ShowWindow (private->xwindow, SW_HIDE); /* Don't put on toolbar */
#endif
	}
      else
	{
	  ShowWindow (private->xwindow, SW_SHOWNORMAL);
	  ShowWindow (private->xwindow, SW_RESTORE);
	  SetForegroundWindow (private->xwindow);
#if 0
	  ShowOwnedPopups (private->xwindow, TRUE);
#endif
	}
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_hide: %#x\n", private->xwindow));

      private->mapped = FALSE;
      if (private->window_type == GDK_WINDOW_TOPLEVEL)
	ShowOwnedPopups (private->xwindow, FALSE);
#if 1
      ShowWindow (private->xwindow, SW_HIDE);
#elif 0
      ShowWindow (private->xwindow, SW_MINIMIZE);
#else
      CloseWindow (private->xwindow);
#endif
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_withdraw: %#x\n", private->xwindow));

      gdk_window_hide (window);	/* XXX */
    }
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      RECT rect;

      GDK_NOTE (MISC, g_print ("gdk_window_move: %#x +%d+%d\n",
			       private->xwindow, x, y));

      GetClientRect (private->xwindow, &rect);

      if (private->window_type != GDK_WINDOW_CHILD)
	{
	  POINT ptTL, ptBR;
	  DWORD dwStyle;
	  DWORD dwExStyle;

	  ptTL.x = 0;
	  ptTL.y = 0; 
	  ClientToScreen (private->xwindow, &ptTL);
	  rect.left = x;
	  rect.top = y;

	  ptBR.x = rect.right;
	  ptBR.y = rect.bottom;
	  ClientToScreen (private->xwindow, &ptBR);
	  rect.right = x + ptBR.x - ptTL.x;
	  rect.bottom = y + ptBR.y - ptTL.y;

	  dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
	  dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
	  if (!SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	    g_warning ("gdk_window_move: AdjustWindowRectEx failed");

	  x = rect.left;
	  y = rect.top;
	}
      else
	{
	  private->x = x;
	  private->y = y;
	}
      GDK_NOTE (MISC, g_print ("...MoveWindow(%#x,%dx%d@+%d+%d)\n",
			       private->xwindow,
			       rect.right - rect.left, rect.bottom - rect.top,
			       x, y));
      if (!MoveWindow (private->xwindow,
		       x, y, rect.right - rect.left, rect.bottom - rect.top,
		       TRUE))
	g_warning ("gdk_window_move: MoveWindow failed");
    }
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  if ((gint16) width < 1)
    width = 1;
  if ((gint16) height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed &&
      ((private->resize_count > 0) ||
       (private->width != (guint16) width) ||
       (private->height != (guint16) height)))
    {
      int x, y;

      GDK_NOTE (MISC, g_print ("gdk_window_resize: %#x %dx%d\n",
			       private->xwindow, width, height));
      
      if (private->window_type != GDK_WINDOW_CHILD)
	{
	  POINT pt;
	  RECT rect;
	  DWORD dwStyle;
	  DWORD dwExStyle;

	  pt.x = 0;
	  pt.y = 0; 
	  ClientToScreen (private->xwindow, &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;
	  rect.right = pt.x + width;
	  rect.bottom = pt.y + height;

	  dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
	  dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
	  if (!AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	    g_warning ("gdk_window_resize: AdjustWindowRectEx failed");

	  x = rect.left;
	  y = rect.top;
	  width = rect.right - rect.left;
	  height = rect.bottom - rect.top;
	}
      else
	{
	  x = private->x;
	  y = private->y;
	  private->width = width;
	  private->height = height;
	}

      private->resize_count += 1;

      GDK_NOTE (MISC, g_print ("...MoveWindow(%#x,%dx%d@+%d+%d)\n",
			       private->xwindow, width, height, x, y));
      if (!MoveWindow (private->xwindow,
		       x, y, width, height,
		       TRUE))
	g_warning ("gdk_window_resize: MoveWindow failed");
    }
}

void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  if ((gint16) width < 1)
    width = 1;
  if ((gint16) height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      RECT rect;
      DWORD dwStyle;
      DWORD dwExStyle;

      GDK_NOTE (MISC, g_print ("gdk_window_move_resize: %#x %dx%d@+%d+%d\n",
			       private->xwindow, width, height, x, y));
      
      rect.left = x;
      rect.top = y;
      rect.right = x + width;
      rect.bottom = y + height;

      dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
      dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
      if (!AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	g_warning ("gdk_window_move_resize: AdjustWindowRectEx failed");

      if (private->window_type == GDK_WINDOW_CHILD)
	{
	  private->x = x;
	  private->y = y;
	  private->width = width;
	  private->height = height;
	}
      GDK_NOTE (MISC, g_print ("...MoveWindow(%#x,%dx%d@+%d+%d)\n",
			       private->xwindow,
			       rect.right - rect.left, rect.bottom - rect.top,
			       rect.left, rect.top));
      if (!MoveWindow (private->xwindow,
		       rect.left, rect.top,
		       rect.right - rect.left, rect.bottom - rect.top,
		       TRUE))
	g_warning ("gdk_window_move_resize: MoveWindow failed");

      if (private->guffaw_gravity)
	{
	  GList *tmp_list = private->children;
	  while (tmp_list)
	    {
	      GdkWindowPrivate *child_private = tmp_list->data;
	      
	      child_private->x -= x - private->x;
	      child_private->y -= y - private->y;
	      
	      tmp_list = tmp_list->next;
	    }
	}
      
    }
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *parent_private;
  GdkWindowPrivate *old_parent_private;

  g_return_if_fail (window != NULL);

  if (!new_parent)
    new_parent = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;
  old_parent_private = (GdkWindowPrivate*)window_private->parent;
  parent_private = (GdkWindowPrivate*) new_parent;

  if (!window_private->destroyed && !parent_private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_reparent: %#x %#x\n",
			       window_private->xwindow,
			       parent_private->xwindow));
      if (!SetParent (window_private->xwindow, parent_private->xwindow))
	g_warning ("gdk_window_reparent: SetParent failed");

      if (!MoveWindow (window_private->xwindow,
		       x, y,
		       window_private->width, window_private->height,
		       TRUE))
	g_warning ("gdk_window_reparent: MoveWindow failed");
    }
  
  window_private->parent = new_parent;

  if (old_parent_private)
    old_parent_private->children = g_list_remove (old_parent_private->children, window);

  if ((old_parent_private &&
       (!old_parent_private->guffaw_gravity != !parent_private->guffaw_gravity)) ||
      (!old_parent_private && parent_private->guffaw_gravity))
    gdk_window_set_static_win_gravity (window, parent_private->guffaw_gravity);
  
  parent_private->children = g_list_prepend (parent_private->children, window);
}

void
gdk_window_clear (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    {
      gdk_window_clear_area (window, 0, 0, private->width, private->height);
    }
}


void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    {
      HDC hdc;

      if (width == -1)
	width = G_MAXSHORT/2;		/* Yeah, right */
      if (height == -1)
	height = G_MAXSHORT/2;
      GDK_NOTE (MISC, g_print ("gdk_window_clear_area: %#x %dx%d@+%d+%d\n",
			       private->xwindow, width, height, x, y));
      hdc = GetDC (private->xwindow);
      IntersectClipRect (hdc, x, y, x + width, y + height);
      SendMessage (private->xwindow, WM_ERASEBKGND, (WPARAM) hdc, 0);
      ReleaseDC (private->xwindow, hdc);
    }
}

void
gdk_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    {
      RECT rect;

      GDK_NOTE (MISC, g_print ("gdk_window_clear_area_e: %#x %dx%d@+%d+%d\n",
			       private->xwindow, width, height, x, y));

      rect.left = x;
      rect.right = x + width;
      rect.top = y;
      rect.bottom = y + height;
      if (!InvalidateRect (private->xwindow, &rect, TRUE))
	g_warning ("gdk_window_clear_area_e: InvalidateRect failed");
      UpdateWindow (private->xwindow);
    }
}

void
gdk_window_copy_area (GdkWindow    *window,
		      GdkGC        *gc,
		      gint          x,
		      gint          y,
		      GdkWindow    *source_window,
		      gint          source_x,
		      gint          source_y,
		      gint          width,
		      gint          height)
{
  GdkWindowPrivate *src_private;
  GdkWindowPrivate *dest_private;
  GdkGCPrivate *gc_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (gc != NULL);
  
  if (source_window == NULL)
    source_window = window;
  
  src_private = (GdkWindowPrivate*) source_window;
  dest_private = (GdkWindowPrivate*) window;
  gc_private = (GdkGCPrivate*) gc;
  
  if (!src_private->destroyed && !dest_private->destroyed)
    {
      HDC hdcDest, hdcSrc;

      if ((hdcDest = GetDC (dest_private->xwindow)) == NULL)
	g_warning ("gdk_window_copy_area: GetDC failed");

      if ((hdcSrc = GetDC (src_private->xwindow)) == NULL)
	g_warning ("gdk_window_copy_area: GetDC failed");

      if (!BitBlt (hdcDest, x, y, width, height, hdcSrc, source_x, source_y, SRCCOPY))
	g_warning ("gdk_window_copy_area: BitBlt failed");

      ReleaseDC (dest_private->xwindow, hdcDest);
      ReleaseDC (src_private->xwindow, hdcSrc);
    }
}

void
gdk_window_raise (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_raise: %#x\n", private->xwindow));

      if (!BringWindowToTop (private->xwindow))
	g_warning ("gdk_window_raise: BringWindowToTop failed");
    }
}

void
gdk_window_lower (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_lower: %#x\n", private->xwindow));

      if (!SetWindowPos (private->xwindow, HWND_BOTTOM, 0, 0, 0, 0,
			 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE))
	g_warning ("gdk_window_lower: SetWindowPos failed");
    }
}

void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (window != NULL);
  
  window->user_data = user_data;
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
  GdkWindowPrivate *private;
  WINDOWPLACEMENT size_hints;
  RECT rect;
  DWORD dwStyle;
  DWORD dwExStyle;
  int diff;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_hints: %#x %dx%d..%dx%d @+%d+%d\n",
			   private->xwindow,
			   min_width, min_height, max_width, max_height,
			   x, y));

  private->hint_flags = flags;
  size_hints.length = sizeof (size_hints);

  if (flags)
    {
      if (flags & GDK_HINT_POS)
	if (!GetWindowPlacement (private->xwindow, &size_hints))
	  g_warning ("gdk_window_set_hints: GetWindowPlacement failed");
	else
	  {
	    GDK_NOTE (MISC, g_print ("...rcNormalPosition:"
				     " (%d,%d)--(%d,%d)\n",
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
	    dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
	    dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
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
	    GDK_NOTE (MISC, g_print ("...setting: (%d,%d)--(%d,%d)\n",
				     size_hints.rcNormalPosition.left,
				     size_hints.rcNormalPosition.top,
				     size_hints.rcNormalPosition.right,
				     size_hints.rcNormalPosition.bottom));
	    if (!SetWindowPlacement (private->xwindow, &size_hints))
	      g_warning ("gdk_window_set_hints: SetWindowPlacement failed");
	    private->hint_x = rect.left;
	    private->hint_y = rect.top;
	  }

      if (flags & GDK_HINT_MIN_SIZE)
	{
	  rect.left = 0;
	  rect.top = 0;
	  rect.right = min_width;
	  rect.bottom = min_height;
	  dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
	  dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
	  AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
	  private->hint_min_width = rect.right - rect.left;
	  private->hint_min_height = rect.bottom - rect.top;

	  /* Also chek if he current size of the window is in bounds. */
	  GetClientRect (private->xwindow, &rect);
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
	  dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
	  dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
	  AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
	  private->hint_max_width = rect.right - rect.left;
	  private->hint_max_height = rect.bottom - rect.top;
	  /* Again, check if the window is too large currently. */
	  GetClientRect (private->xwindow, &rect);
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
  GdkWindowPrivate *private;
  WINDOWPLACEMENT size_hints;
  RECT rect;
  DWORD dwStyle;
  DWORD dwExStyle;
  int diff;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;
  
  size_hints.length = sizeof (size_hints);

  private->hint_flags = geom_mask;

  if (geom_mask & GDK_HINT_POS)
    ; /* XXX */

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      rect.left = 0;
      rect.top = 0;
      rect.right = geometry->min_width;
      rect.bottom = geometry->min_height;
      dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
      dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      private->hint_min_width = rect.right - rect.left;
      private->hint_min_height = rect.bottom - rect.top;

      /* Also check if he current size of the window is in bounds */
      GetClientRect (private->xwindow, &rect);
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
      dwStyle = GetWindowLong (private->xwindow, GWL_STYLE);
      dwExStyle = GetWindowLong (private->xwindow, GWL_EXSTYLE);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      private->hint_max_width = rect.right - rect.left;
      private->hint_max_height = rect.bottom - rect.top;

      /* Again, check if the window is too large currently. */
      GetClientRect (private->xwindow, &rect);
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
    if (!GetWindowPlacement (private->xwindow, &size_hints))
      g_warning ("gdk_window_set_hints: GetWindowPlacement failed");
    else
      {
	GDK_NOTE (MISC, g_print ("gdk_window_set_geometry_hints:"
				 " rcNormalPosition: (%d,%d)--(%d,%d)\n",
				 size_hints.rcNormalPosition.left,
				 size_hints.rcNormalPosition.top,
				 size_hints.rcNormalPosition.right,
				 size_hints.rcNormalPosition.bottom));
	size_hints.rcNormalPosition.right =
	  size_hints.rcNormalPosition.left + geometry->base_width;
	size_hints.rcNormalPosition.bottom =
	  size_hints.rcNormalPosition.top + geometry->base_height;
	GDK_NOTE (MISC, g_print ("...setting: rcNormal: (%d,%d)--(%d,%d)\n",
				 size_hints.rcNormalPosition.left,
				 size_hints.rcNormalPosition.top,
				 size_hints.rcNormalPosition.right,
				 size_hints.rcNormalPosition.bottom));
	if (!SetWindowPlacement (private->xwindow, &size_hints))
	  g_warning ("gdk_window_set_hints: SetWindowPlacement failed");
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
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      if (!SetWindowText (private->xwindow, title))
	g_warning ("gdk_window_set_title: SetWindowText failed");
    }
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;

  GDK_NOTE (MISC, g_print ("gdk_window_set_role: %#x %s\n",
			   private->xwindow, (role ? role : "NULL")));
  /* XXX */
}

void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  parent_private = (GdkWindowPrivate*) parent;

  GDK_NOTE (MISC, g_print ("gdk_window_set_transient_for: %#x %#x\n",
			   private->xwindow, parent_private->xwindow));
  /* XXX */
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      GdkColormapPrivate *colormap_private =
	(GdkColormapPrivate *) private->colormap;

      GDK_NOTE (MISC, g_print ("gdk_window_set_background: %#x %s\n",
			       private->xwindow, 
			       gdk_color_to_string (color)));

      if (private->bg_type == GDK_WIN32_BG_PIXMAP)
	{
	  if (private->bg_pixmap != NULL)
	    {
	      gdk_pixmap_unref (private->bg_pixmap);
	      private->bg_pixmap = NULL;
	    }
	  private->bg_type = GDK_WIN32_BG_NORMAL;
	}
#ifdef MULTIPLE_WINDOW_CLASSES
      if (colormap_private != NULL
	      && colormap_private->xcolormap->rc_palette)
	{
	  /* If we are on a palettized display we can't use the window
	   * class background brush, but must handle WM_ERASEBKGND.
	   * At least, I think so.
	   */
#endif
	  private->bg_type = GDK_WIN32_BG_PIXEL;
	  private->bg_pixel = *color;
#ifdef MULTIPLE_WINDOW_CLASSES
	}
      else
	{
	  /* Non-palettized display; just set the window class background
	     brush. */
	  HBRUSH hbr;
	  HGDIOBJ oldbrush;
	  COLORREF background;

	  background = RGB (color->red >> 8,
			    color->green >> 8,
			    color->blue >> 8);

	  if ((hbr = CreateSolidBrush (GetNearestColor (gdk_DC,
							background))) == NULL)
	    {
	      g_warning ("gdk_window_set_background: CreateSolidBrush failed");
	      return;
	    }

	  oldbrush = (HGDIOBJ) GetClassLong (private->xwindow,
					     GCL_HBRBACKGROUND);

	  if (SetClassLong (private->xwindow, GCL_HBRBACKGROUND,
			    (LONG) hbr) == 0)
	    g_warning ("gdk_window_set_background: SetClassLong failed");

	  if (!DeleteObject (oldbrush))
	    g_warning ("gdk_window_set_background: DeleteObject failed");
	}
#endif
    }
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  GdkWindowPrivate *window_private;
#ifdef MULTIPLE_WINDOW_CLASSES
  GdkPixmapPrivate *pixmap_private;
#endif

  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
#ifdef MULTIPLE_WINDOW_CLASSES
  pixmap_private = (GdkPixmapPrivate*) pixmap;
#endif

  if (!window_private->destroyed)
    {
      GdkColormapPrivate *colormap_private =
	(GdkColormapPrivate *) window_private->colormap;
      if (window_private->bg_type == GDK_WIN32_BG_PIXMAP)
	{
	  if (window_private->bg_pixmap != NULL)
	    {
	      gdk_pixmap_unref (window_private->bg_pixmap);
	      window_private->bg_pixmap = NULL;
	    }
	  window_private->bg_type = GDK_WIN32_BG_NORMAL;
	}
      if (parent_relative)
	{
	  window_private->bg_type = GDK_WIN32_BG_PARENT_RELATIVE;
	}
      else if (!pixmap)
	{
#ifdef MULTIPLE_WINDOW_CLASSES
	  SetClassLong (window_private->xwindow, GCL_HBRBACKGROUND,
			(LONG) GetStockObject (BLACK_BRUSH));
#endif
	}
#ifdef MULTIPLE_WINDOW_CLASSES
      else if (colormap_private->xcolormap->rc_palette)
	{
	  /* Must do the background painting in the
	   * WM_ERASEBKGND handler.
	   */
	  window_private->bg_type = GDK_WIN32_BG_PIXMAP;
	  window_private->bg_pixmap = pixmap;
	  gdk_pixmap_ref (pixmap);
	}
      else if (pixmap_private->width <= 8
	       && pixmap_private->height <= 8)
	{
	  /* We can use small pixmaps directly as background brush */
	  SetClassLong (window_private->xwindow, GCL_HBRBACKGROUND,
			(LONG) CreatePatternBrush (pixmap_private->xwindow));
	}
#endif
      else
	{
	  /* We must cache the pixmap in the WindowPrivate and
	   * paint it each time we get WM_ERASEBKGND
	   */
	  window_private->bg_type = GDK_WIN32_BG_PIXMAP;
	  window_private->bg_pixmap = pixmap;
	  gdk_pixmap_ref (pixmap);
	}
    }
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowPrivate *window_private;
  GdkCursorPrivate *cursor_private;
  HCURSOR xcursor;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!window_private->destroyed)
    {
      if (!cursor)
	xcursor = LoadCursor (NULL, IDC_ARROW);
      else
	xcursor = cursor_private->xcursor;

      GDK_NOTE (MISC, g_print ("gdk_window_set_cursor: %#x %#x\n",
			       window_private->xwindow, xcursor));
#ifdef MULTIPLE_WINDOW_CLASSES
      if (!SetClassLong (window_private->xwindow, GCL_HCURSOR, (LONG) xcursor))
	g_warning ("gdk_window_set_cursor: SetClassLong failed");
#else
      window_private->xcursor = xcursor;
#endif
      SetCursor (xcursor);
    }
}

void
gdk_window_set_colormap (GdkWindow   *window,
			 GdkColormap *colormap)
{
  GdkWindowPrivate *window_private;
  GdkColormapPrivate *colormap_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (colormap != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  colormap_private = (GdkColormapPrivate*) colormap;
  
  if (!window_private->destroyed)
    {
      /* XXX ??? */
      GDK_NOTE (MISC, g_print ("gdk_window_set_colormap: %#x %#x\n",
			       window_private->xwindow,
			       colormap_private->xcolormap));
      if (window_private->colormap)
	gdk_colormap_unref (window_private->colormap);
      window_private->colormap = colormap;
      gdk_colormap_ref (window_private->colormap);
      
      if (window_private->window_type != GDK_WINDOW_TOPLEVEL)
	gdk_window_add_colormap_windows (window);
    }
}

void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  g_return_if_fail (window != NULL);
  
  *data = window->user_data;
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GdkWindowPrivate *window_private;
  
  if (!window)
    window = (GdkWindow*) &gdk_root_parent;
  
  window_private = (GdkWindowPrivate*) window;
  
  if (!window_private->destroyed)
    {
      RECT rect;

      if (!GetClientRect (window_private->xwindow, &rect))
	g_warning ("gdk_window_get_geometry: GetClientRect failed");

      if (x)
	*x = rect.left;
      if (y)
	*y = rect.top;
      if (width)
	*width = rect.right - rect.left;
      if (height)
	*height = rect.bottom - rect.top;
      if (depth)
	*depth = gdk_window_get_visual (window)->depth;
    }
}

void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  GdkWindowPrivate *window_private;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  
  if (x)
    *x = window_private->x;
  if (y)
    *y = window_private->y;
}

void
gdk_window_get_size (GdkWindow *window,
		     gint       *width,
		     gint       *height)
{
  GdkWindowPrivate *window_private;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  
  if (width)
    *width = window_private->width;
  if (height)
    *height = window_private->height;
}

GdkVisual*
gdk_window_get_visual (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
   
  g_return_val_if_fail (window != NULL, NULL);

  window_private = (GdkWindowPrivate*) window;
  /* Huh? ->parent is never set for a pixmap. We should just return
   * null immeditately. Well, do it then!
   */
  if (window_private->window_type == GDK_WINDOW_PIXMAP)
    return NULL;
  
  if (!window_private->destroyed)
    {
       if (window_private->colormap == NULL)
	 return gdk_visual_get_system (); /* XXX ??? */
       else
	 return ((GdkColormapPrivate *)window_private->colormap)->visual;
    }
  
  return NULL;
}

GdkColormap*
gdk_window_get_colormap (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  
  g_return_val_if_fail (window != NULL, NULL);
  window_private = (GdkWindowPrivate*) window;

  g_return_val_if_fail (window_private->window_type != GDK_WINDOW_PIXMAP, NULL);
  if (!window_private->destroyed)
    {
      if (window_private->colormap == NULL)
	return gdk_colormap_get_system (); /* XXX ??? */
      else
	return window_private->colormap;
    }
  
  return NULL;
}

GdkWindowType
gdk_window_get_type (GdkWindow *window)
{
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (window != NULL, (GdkWindowType) -1);

  window_private = (GdkWindowPrivate*) window;
  return window_private->window_type;
}

gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  GdkWindowPrivate *private;
  gint return_val;
  gint tx = 0;
  gint ty = 0;

  g_return_val_if_fail (window != NULL, 0);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    {
      POINT pt;

      pt.x = 0;
      pt.y = 0;
      ClientToScreen (private->xwindow, &pt);
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
			   private->xwindow, tx, ty));
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
  GdkWindowPrivate *private;
  POINT pt;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
  if (private->destroyed)
    return;
      
  while (private->parent && ((GdkWindowPrivate*) private->parent)->parent)
    private = (GdkWindowPrivate*) private->parent;
  if (private->destroyed)
    return;

  pt.x = 0;
  pt.y = 0;
  ClientToScreen (private->xwindow, &pt);
  if (x)
    *x = pt.x;
  if (y)
    *y = pt.y;

  GDK_NOTE (MISC, g_print ("gdk_window_get_root_origin: %#x: (%#x) +%d+%d\n",
			   ((GdkWindowPrivate *) window)->xwindow,
			   private->xwindow, pt.x, pt.y));
}

GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
			gint            *x,
			gint            *y,
			GdkModifierType *mask)
{
  GdkWindowPrivate *private;
  GdkWindow *return_val;
  POINT pointc, point;
  HWND hwnd, hwndc;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  private = (GdkWindowPrivate*) window;

  return_val = NULL;
  GetCursorPos (&pointc);
  point = pointc;
  ScreenToClient (private->xwindow, &point);

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

  return_val = gdk_window_lookup (hwnd);

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
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  GdkWindowPrivate *private;
  GdkWindow *window;
  POINT point, pointc;
  HWND hwnd, hwndc;
  RECT rect;

  private = &gdk_root_parent;

  GetCursorPos (&pointc);
  point = pointc;
  hwnd = WindowFromPoint (point);

  if (hwnd == NULL)
    {
      window = (GdkWindow *) &gdk_root_parent;
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

  window = gdk_window_lookup (hwnd);

  if (window && (win_x || win_y))
    {
      GetClientRect (hwnd, &rect);
      if (win_x)
	*win_x = point.x - rect.left;
      if (win_y)
	*win_y = point.y - rect.top;
    }

  GDK_NOTE (MISC, g_print ("gdk_window_at_pointer: +%d+%d %#x%s\n",
			   point.x, point.y, hwnd,
			   (window == NULL ? " NULL" : "")));

  return window;
}

GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, NULL);

  return ((GdkWindowPrivate*) window)->parent;
}

GdkWindow*
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_val_if_fail (window != NULL, NULL);

  private = (GdkWindowPrivate*) window;

  while (private->window_type == GDK_WINDOW_CHILD)
    {
      window = ((GdkWindowPrivate*) window)->parent;
      private = (GdkWindowPrivate*) window;
    }

  return window;
}

GList*
gdk_window_get_children (GdkWindow *window)
{
  GdkWindowPrivate *private;
  GList *children;

  g_return_val_if_fail (window != NULL, NULL);

  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return NULL;

  /* XXX ??? */
  g_warning ("gdk_window_get_children ???");
  children = NULL;

  return children;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  GdkWindowPrivate *private;
  GdkEventMask event_mask;

  g_return_val_if_fail (window != NULL, 0);

  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return 0;

  event_mask = 0;

  event_mask = private->event_mask;

  return event_mask;
}

void          
gdk_window_set_events (GdkWindow   *window,
		       GdkEventMask event_mask)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;

  private->event_mask = event_mask;
}

void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  g_warning ("gdk_window_add_colormap_windows not implemented"); /* XXX */
}

/*
 * This needs the X11 shape extension.
 * If not available, shaped windows will look
 * ugly, but programs still work.    Stefan Wille
 */
void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  GdkWindowPrivate *window_private;

  g_return_if_fail (window != NULL);

  window_private = (GdkWindowPrivate*) window;
  
  if (!mask)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %#x none\n",
			       window_private->xwindow));
      SetWindowRgn (window_private->xwindow, NULL, TRUE);
    }
  else
    {
      GdkPixmapPrivate *pixmap_private;
      HRGN hrgn;
      DWORD dwStyle;
      DWORD dwExStyle;
      RECT rect;

      /* Convert mask bitmap to region */
      pixmap_private = (GdkPixmapPrivate*) mask;
      hrgn = BitmapToRegion (pixmap_private->xwindow);

      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %#x %#x\n",
			       window_private->xwindow,
			       pixmap_private->xwindow));

      /* SetWindowRgn wants window (not client) coordinates */ 
      dwStyle = GetWindowLong (window_private->xwindow, GWL_STYLE);
      dwExStyle = GetWindowLong (window_private->xwindow, GWL_EXSTYLE);
      GetClientRect (window_private->xwindow, &rect);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      OffsetRgn (hrgn, -rect.left, -rect.top);

      OffsetRgn (hrgn, x, y);

      /* If this is a top-level window, add the title bar to the region */
      if (window_private->window_type == GDK_WINDOW_TOPLEVEL)
	{
	  CombineRgn (hrgn, hrgn,
		      CreateRectRgn (0, 0, rect.right - rect.left, -rect.top),
		      RGN_OR);
	}
      
      SetWindowRgn (window_private->xwindow, hrgn, TRUE);
    }
}

void          
gdk_window_add_filter (GdkWindow     *window,
		       GdkFilterFunc  function,
		       gpointer       data)
{
  GdkWindowPrivate *private;
  GList *tmp_list;
  GdkEventFilter *filter;

  private = (GdkWindowPrivate*) window;
  if (private && private->destroyed)
    return;

  if (private)
    tmp_list = private->filters;
  else
    tmp_list = gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      if ((filter->function == function) && (filter->data == data))
	return;
      tmp_list = tmp_list->next;
    }

  filter = g_new (GdkEventFilter, 1);
  filter->function = function;
  filter->data = data;

  if (private)
    private->filters = g_list_append (private->filters, filter);
  else
    gdk_default_filters = g_list_append (gdk_default_filters, filter);
}

void
gdk_window_remove_filter (GdkWindow     *window,
			  GdkFilterFunc  function,
			  gpointer       data)
{
  GdkWindowPrivate *private;
  GList *tmp_list, *node;
  GdkEventFilter *filter;

  private = (GdkWindowPrivate*) window;

  if(private)
    tmp_list = private->filters;
  else
    tmp_list = gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      node = tmp_list;
      tmp_list = tmp_list->next;

      if ((filter->function == function) && (filter->data == data))
	{
	  if(private)
	    private->filters = g_list_remove_link (private->filters, node);
	  else
	    gdk_default_filters = g_list_remove_link (gdk_default_filters, tmp_list);
	  g_list_free_1 (node);
	  g_free (filter);
	  
	  return;
	}
    }
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean   override_redirect)
{
  g_warning ("gdk_window_set_override_redirect not implemented"); /* XXX */
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_warning ("gdk_window_set_icon not implemented"); /* XXX */
}

void          
gdk_window_set_icon_name (GdkWindow *window, 
			  gchar     *name)
{
  GdkWindowPrivate *window_private;

  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return;

  if (!SetWindowText (window_private->xwindow, name))
    g_warning ("gdk_window_set_icon_name: SetWindowText failed");
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  g_warning ("gdk_window_set_group not implemented"); /* XXX */
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  GdkWindowPrivate *window_private = (GdkWindowPrivate *) window;
  LONG style, exstyle;

  style = GetWindowLong (window_private->xwindow, GWL_STYLE);
  exstyle = GetWindowLong (window_private->xwindow, GWL_EXSTYLE);

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
  
  SetWindowLong (window_private->xwindow, GWL_STYLE, style);
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  GdkWindowPrivate *window_private = (GdkWindowPrivate *) window;
  LONG style, exstyle;

  style = GetWindowLong (window_private->xwindow, GWL_STYLE);
  exstyle = GetWindowLong (window_private->xwindow, GWL_EXSTYLE);

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
  
  SetWindowLong (window_private->xwindow, GWL_STYLE, style);
}

GList *
gdk_window_get_toplevels (void)
{
  GList *new_list = NULL;
  GList *tmp_list;

  tmp_list = gdk_root_parent.children;
  while (tmp_list)
    {
      new_list = g_list_prepend (new_list, tmp_list->data);
      tmp_list = tmp_list->next;
    }

  return new_list;
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
   RECT rect;
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
       /* go through all child windows and create/insert spans */
       for (i = 0; i < num; i++)
	 {
	   GetWindowPlacement (list[i], &placement);
	   if (placement.showCmd = SW_SHOWNORMAL)
	     {
	       childRegion = CreateRectRgnIndirect (&emptyRect);
	       GetWindowRgn (list[i], childRegion);
	       CombineRgn (region, region, childRegion, RGN_OR);
	     }
	  }
       SetWindowRgn (win, region, TRUE);
     }
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  GdkWindowPrivate *private;
   
  g_return_if_fail (window != NULL);
   
  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;

  gdk_propagate_shapes ( private->xwindow, FALSE);
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;

  gdk_propagate_shapes (private->xwindow, TRUE);
}

/*************************************************************
 * gdk_window_is_visible:
 *     Check if the given window is mapped.
 *   arguments:
 *     window: 
 *   results:
 *     is the window mapped
 *************************************************************/

gboolean 
gdk_window_is_visible (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_val_if_fail (window != NULL, FALSE);

  return private->mapped;
}

/*************************************************************
 * gdk_window_is_viewable:
 *     Check if the window and all ancestors of the window
 *     are mapped. (This is not necessarily "viewable" in
 *     the X sense, since we only check as far as we have
 *     GDK window parents, not to the root window)
 *   arguments:
 *     window:
 *   results:
 *     is the window viewable
 *************************************************************/

gboolean 
gdk_window_is_viewable (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_val_if_fail (window != NULL, FALSE);

  while (private && 
	 (private != &gdk_root_parent) &&
	 (private->window_type != GDK_WINDOW_FOREIGN))
    {
      if (!private->mapped)
	return FALSE;

      private = (GdkWindowPrivate *)private->parent;
    }

  return TRUE;
}

void          
gdk_drawable_set_data (GdkDrawable   *drawable,
		       const gchar   *key,
		       gpointer	      data,
		       GDestroyNotify destroy_func)
{
  g_dataset_set_data_full (drawable, key, data, destroy_func);
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
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);

  GDK_NOTE (MISC,
	    g_print ("gdk_window_set_static_bit_gravity: Not implemented\n"));
}

static void
gdk_window_set_static_win_gravity (GdkWindow *window, gboolean on)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

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
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GList *tmp_list;

  g_return_val_if_fail (window != NULL, FALSE);
  
  if (!use_static == !private->guffaw_gravity)
    return TRUE;
  
  if (use_static && !gdk_window_gravity_works ())
    return FALSE;

  private->guffaw_gravity = use_static;

  gdk_window_set_static_bit_gravity (window, use_static);

  tmp_list = private->children;
  while (tmp_list)
    {
      gdk_window_set_static_win_gravity (window, use_static);
      
      tmp_list = tmp_list->next;
    }

  return TRUE;
}
