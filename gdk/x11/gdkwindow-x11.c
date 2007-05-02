/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <netinet/in.h>
#include <unistd.h>

#include "gdk.h"

#include "gdkwindow.h"
#include "gdkasync.h"
#include "gdkinputprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "gdkregion.h"
#include "gdkinternals.h"
#include "MwmUtil.h"
#include "gdkwindow-x11.h"
#include "gdkalias.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#ifdef HAVE_SHAPE_EXT
#include <X11/extensions/shape.h>
#endif

const int _gdk_event_mask_table[21] =
{
  ExposureMask,
  PointerMotionMask,
  PointerMotionHintMask,
  ButtonMotionMask,
  Button1MotionMask,
  Button2MotionMask,
  Button3MotionMask,
  ButtonPressMask,
  ButtonReleaseMask,
  KeyPressMask,
  KeyReleaseMask,
  EnterWindowMask,
  LeaveWindowMask,
  FocusChangeMask,
  StructureNotifyMask,
  PropertyChangeMask,
  VisibilityChangeMask,
  0,				/* PROXIMITY_IN */
  0,				/* PROXIMTY_OUT */
  SubstructureNotifyMask,
  ButtonPressMask      /* SCROLL; on X mouse wheel events is treated as mouse button 4/5 */
};
const int _gdk_nenvent_masks = sizeof (_gdk_event_mask_table) / sizeof (int);

/* Forward declarations */
static void     gdk_window_set_static_win_gravity (GdkWindow  *window,
						   gboolean    on);
static gboolean gdk_window_icon_name_set          (GdkWindow  *window);
static void     gdk_window_add_colormap_windows   (GdkWindow  *window);
static void     set_wm_name                       (GdkDisplay  *display,
						   Window       xwindow,
						   const gchar *name);
static void     move_to_current_desktop           (GdkWindow *window);

static GdkColormap* gdk_window_impl_x11_get_colormap (GdkDrawable *drawable);
static void         gdk_window_impl_x11_set_colormap (GdkDrawable *drawable,
						      GdkColormap *cmap);
static void         gdk_window_impl_x11_get_size    (GdkDrawable *drawable,
						     gint *width,
						     gint *height);
static GdkRegion*  gdk_window_impl_x11_get_visible_region (GdkDrawable *drawable);
static void gdk_window_impl_x11_finalize   (GObject            *object);

#define WINDOW_IS_TOPLEVEL(window)		   \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)

/* Return whether time1 is considered later than time2 as far as xserver
 * time is concerned.  Accounts for wraparound.
 */
#define XSERVER_TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

G_DEFINE_TYPE (GdkWindowImplX11, gdk_window_impl_x11, GDK_TYPE_DRAWABLE_IMPL_X11)

GType
_gdk_window_impl_get_type (void)
{
  return gdk_window_impl_x11_get_type ();
}

static void
gdk_window_impl_x11_init (GdkWindowImplX11 *impl)
{  
  impl->width = 1;
  impl->height = 1;
  impl->toplevel_window_type = -1;
}

GdkToplevelX11 *
_gdk_x11_window_get_toplevel (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    return NULL;

  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);

  if (!impl->toplevel)
    impl->toplevel = g_new0 (GdkToplevelX11, 1);

  return impl->toplevel;
}

static void
gdk_window_impl_x11_class_init (GdkWindowImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  object_class->finalize = gdk_window_impl_x11_finalize;

  drawable_class->set_colormap = gdk_window_impl_x11_set_colormap;
  drawable_class->get_colormap = gdk_window_impl_x11_get_colormap;
  drawable_class->get_size = gdk_window_impl_x11_get_size;

  /* Visible and clip regions are the same */
  drawable_class->get_clip_region = gdk_window_impl_x11_get_visible_region;
  drawable_class->get_visible_region = gdk_window_impl_x11_get_visible_region;
}

static void
gdk_window_impl_x11_finalize (GObject *object)
{
  GdkWindowObject *wrapper;
  GdkDrawableImplX11 *draw_impl;
  GdkWindowImplX11 *window_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (object));

  draw_impl = GDK_DRAWABLE_IMPL_X11 (object);
  window_impl = GDK_WINDOW_IMPL_X11 (object);
  
  wrapper = (GdkWindowObject*) draw_impl->wrapper;

  _gdk_xgrab_check_destroy (GDK_WINDOW (wrapper));

  if (!GDK_WINDOW_DESTROYED (wrapper))
    {
      GdkDisplay *display = GDK_WINDOW_DISPLAY (wrapper);
      
      _gdk_xid_table_remove (display, draw_impl->xid);
      if (window_impl->toplevel && window_impl->toplevel->focus_window)
	_gdk_xid_table_remove (display, window_impl->toplevel->focus_window);
    }

  if (window_impl->toplevel)
    g_free (window_impl->toplevel);

  if (window_impl->cursor)
    gdk_cursor_unref (window_impl->cursor);

  G_OBJECT_CLASS (gdk_window_impl_x11_parent_class)->finalize (object);
}

static void
tmp_unset_bg (GdkWindow *window)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_X11 (obj->impl);

  /* For windows without EXPOSURE_MASK, we can't do this
   * unsetting because such windows depend on the drawing
   * that the X server is going to do
   */
  if (!(obj->event_mask & GDK_EXPOSURE_MASK))
    return;
    
  impl->position_info.no_bg = TRUE;

  if (obj->bg_pixmap != GDK_NO_BG)
    XSetWindowBackgroundPixmap (GDK_DRAWABLE_XDISPLAY (window),
				GDK_DRAWABLE_XID (window), None);
}

static void
tmp_reset_bg (GdkWindow *window)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_X11 (obj->impl);

  if (!(obj->event_mask & GDK_EXPOSURE_MASK))
    return;
    
  impl->position_info.no_bg = FALSE;

  if (obj->bg_pixmap == GDK_NO_BG)
    return;
  
  if (obj->bg_pixmap)
    {
      Pixmap xpixmap;

      if (obj->bg_pixmap == GDK_PARENT_RELATIVE_BG)
	xpixmap = ParentRelative;
      else 
	xpixmap = GDK_DRAWABLE_XID (obj->bg_pixmap);

      XSetWindowBackgroundPixmap (GDK_DRAWABLE_XDISPLAY (window),
				  GDK_DRAWABLE_XID (window), xpixmap);
    }
  else
    {
      XSetWindowBackground (GDK_DRAWABLE_XDISPLAY (window),
			    GDK_DRAWABLE_XID (window),
			    obj->bg_color.pixel);
    }
}

/* Unsetting and resetting window backgrounds.
 *
 * In many cases it is possible to avoid flicker by unsetting the
 * background of windows. For example if the background of the
 * parent window is unset when a window is unmapped, a brief flicker
 * of background painting is avoided.
 */
void
_gdk_x11_window_tmp_unset_bg (GdkWindow *window,
			      gboolean   recurse)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject *)window;

  if (private->input_only || private->destroyed ||
      (private->window_type != GDK_WINDOW_ROOT &&
       !GDK_WINDOW_IS_MAPPED (window)))
    {
      return;
    }

  if (private->window_type != GDK_WINDOW_ROOT &&
      private->window_type != GDK_WINDOW_FOREIGN)
    {
      tmp_unset_bg (window);
    }

  if (recurse)
    {
      GList *l;

      for (l = private->children; l != NULL; l = l->next)
	_gdk_x11_window_tmp_unset_bg (l->data, TRUE);
    }
}

void
_gdk_x11_window_tmp_reset_bg (GdkWindow *window,
			      gboolean   recurse)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;

  if (private->input_only || private->destroyed ||
      (private->window_type != GDK_WINDOW_ROOT &&
       !GDK_WINDOW_IS_MAPPED (window)))
    {
      return;
    }

  if (private->window_type != GDK_WINDOW_ROOT &&
      private->window_type != GDK_WINDOW_FOREIGN)
    {
      tmp_reset_bg (window);
    }

  if (recurse)
    {
      GList *l;

      for (l = private->children; l != NULL; l = l->next)
	_gdk_x11_window_tmp_reset_bg (l->data, TRUE);
    }
}

static GdkColormap*
gdk_window_impl_x11_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *drawable_impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW_IMPL_X11 (drawable), NULL);

  drawable_impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (!((GdkWindowObject *) drawable_impl->wrapper)->input_only && 
      drawable_impl->colormap == NULL)
    {
      XWindowAttributes window_attributes;
      GdkVisual *visual;

      XGetWindowAttributes (GDK_SCREEN_XDISPLAY (drawable_impl->screen),
                            drawable_impl->xid,
                            &window_attributes);

      visual = gdk_x11_screen_lookup_visual (drawable_impl->screen,
					     window_attributes.visual->visualid);
      drawable_impl->colormap = gdk_x11_colormap_foreign_new (visual,
							      window_attributes.colormap);
    }
  
  return drawable_impl->colormap;
}

static void
gdk_window_impl_x11_set_colormap (GdkDrawable *drawable,
                                  GdkColormap *cmap)
{
  GdkDrawableImplX11 *draw_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (drawable));

  draw_impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (cmap && GDK_WINDOW_DESTROYED (draw_impl->wrapper))
    return;

  /* chain up */
  GDK_DRAWABLE_CLASS (gdk_window_impl_x11_parent_class)->set_colormap (drawable, cmap);

  if (cmap)
    {
      XSetWindowColormap (GDK_SCREEN_XDISPLAY (draw_impl->screen),
                          draw_impl->xid,
                          GDK_COLORMAP_XCOLORMAP (cmap));

      if (((GdkWindowObject*)draw_impl->wrapper)->window_type !=
          GDK_WINDOW_TOPLEVEL)
        gdk_window_add_colormap_windows (GDK_WINDOW (draw_impl->wrapper));
    }
}


static void
gdk_window_impl_x11_get_size (GdkDrawable *drawable,
                              gint        *width,
                              gint        *height)
{
  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (drawable));

  if (width)
    *width = GDK_WINDOW_IMPL_X11 (drawable)->width;
  if (height)
    *height = GDK_WINDOW_IMPL_X11 (drawable)->height;
}

static GdkRegion*
gdk_window_impl_x11_get_visible_region (GdkDrawable *drawable)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (drawable);
  GdkRectangle result_rect;

  result_rect.x = 0;
  result_rect.y = 0;
  result_rect.width = impl->width;
  result_rect.height = impl->height;

  gdk_rectangle_intersect (&result_rect, &impl->position_info.clip_rect, &result_rect);

  return gdk_region_rectangle (&result_rect);
}

void
_gdk_windowing_window_init (GdkScreen * screen)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  GdkScreenX11 *screen_x11;

  screen_x11 = GDK_SCREEN_X11 (screen);

  g_assert (screen_x11->root_window == NULL);

  gdk_screen_set_default_colormap (screen,
				   gdk_screen_get_system_colormap (screen));

  screen_x11->root_window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)screen_x11->root_window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  
  draw_impl->screen = screen;
  draw_impl->xid = screen_x11->xroot_window;
  draw_impl->wrapper = GDK_DRAWABLE (private);
  draw_impl->colormap = gdk_screen_get_system_colormap (screen);
  g_object_ref (draw_impl->colormap);
  
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = DefaultDepthOfScreen (screen_x11->xscreen);
  
  impl->width = WidthOfScreen (screen_x11->xscreen);
  impl->height = HeightOfScreen (screen_x11->xscreen);
  
  _gdk_window_init_position (GDK_WINDOW (private));

  _gdk_xid_table_insert (screen_x11->display,
			 &screen_x11->xroot_window,
			 screen_x11->root_window);
}

static void
set_wm_protocols (GdkWindow *window)
{
  GdkDisplay *display = gdk_drawable_get_display (window);
  Atom protocols[4];
  int n = 0;
  
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW");
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS");
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PING");

#ifdef HAVE_XSYNC
  if (GDK_DISPLAY_X11 (display)->use_sync)
    protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_SYNC_REQUEST");
#endif
  
  XSetWMProtocols (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window), protocols, n);
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

static void
check_leader_window_title (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (display_x11->leader_window && !display_x11->leader_window_title_set)
    {
      set_wm_name (display,
		   display_x11->leader_window,
		   get_default_title ());
      
      display_x11->leader_window_title_set = TRUE;
    }
}

static Window
create_focus_window (Display *xdisplay,
		     XID      parent)
{
  Window focus_window = XCreateSimpleWindow (xdisplay, parent,
					     -1, -1, 1, 1, 0,
					     0, 0);
  
  /* FIXME: probably better to actually track the requested event mask for the toplevel
   */
  XSelectInput (xdisplay, focus_window,
		KeyPressMask | KeyReleaseMask | FocusChangeMask);
  
  XMapWindow (xdisplay, focus_window);

  return focus_window;
}

static void
ensure_sync_counter (GdkWindow *window)
{
#ifdef HAVE_XSYNC
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
      GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
      GdkWindowObject *private = (GdkWindowObject *)window;
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);

      if (toplevel && impl->use_synchronized_configure &&
	  toplevel->update_counter == None &&
	  GDK_DISPLAY_X11 (display)->use_sync)
	{
	  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
	  XSyncValue value;
	  Atom atom;

	  XSyncIntToValue (&value, 0);
	  
	  toplevel->update_counter = XSyncCreateCounter (xdisplay, value);
	  
	  atom = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_SYNC_REQUEST_COUNTER");
	  
	  XChangeProperty (xdisplay, GDK_WINDOW_XID (window),
			   atom, XA_CARDINAL,
			   32, PropModeReplace,
			   (guchar *)&toplevel->update_counter, 1);
	  
	  XSyncIntToValue (&toplevel->current_counter_value, 0);
	}
    }
#endif
}

static void
setup_toplevel_window (GdkWindow *window, 
		       GdkWindow *parent)
{
  GdkWindowObject *obj = (GdkWindowObject *)window;
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkWindowImplX11 *impl = (GdkWindowImplX11 *)obj->impl;
  Display *xdisplay = GDK_WINDOW_XDISPLAY (window);
  XID xid = GDK_WINDOW_XID (window);
  XID xparent = GDK_WINDOW_XID (parent);
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (GDK_WINDOW_SCREEN (parent));
  XSizeHints size_hints;
  long pid;
  Window leader_window;
    
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_DIALOG)
    XSetTransientForHint (xdisplay, xid, xparent);
  
  set_wm_protocols (window);
  
  if (!obj->input_only)
    {
      /* The focus window is off the visible area, and serves to receive key
       * press events so they don't get sent to child windows.
       */
      toplevel->focus_window = create_focus_window (xdisplay, xid);
      _gdk_xid_table_insert (screen_x11->display, &toplevel->focus_window, window);
    }
  
  check_leader_window_title (screen_x11->display);
  
  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  size_hints.flags = PSize;
  size_hints.width = impl->width;
  size_hints.height = impl->height;
  
  XSetWMNormalHints (xdisplay, xid, &size_hints);
  
  /* This will set WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (xdisplay, xid, NULL, NULL, NULL, 0, NULL, NULL, NULL);
  
  pid = getpid ();
  XChangeProperty (xdisplay, xid,
		   gdk_x11_get_xatom_by_name_for_display (screen_x11->display, "_NET_WM_PID"),
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *)&pid, 1);

  leader_window = GDK_DISPLAY_X11 (screen_x11->display)->leader_window;
  if (!leader_window)
    leader_window = xid;
  XChangeProperty (xdisplay, xid, 
		   gdk_x11_get_xatom_by_name_for_display (screen_x11->display, "WM_CLIENT_LEADER"),
		   XA_WINDOW, 32, PropModeReplace,
		   (guchar *) &leader_window, 1);

  if (!obj->focus_on_map)
    gdk_x11_window_set_user_time (window, 0);
  else if (GDK_DISPLAY_X11 (screen_x11->display)->user_time != 0)
    gdk_x11_window_set_user_time (window, GDK_DISPLAY_X11 (screen_x11->display)->user_time);

  ensure_sync_counter (window);
}

/**
 * gdk_window_new:
 * @parent: a #GdkWindow, or %NULL to create the window as a child of
 *   the default root window for the default display.
 * @attributes: attributes of the new window
 * @attributes_mask: mask indicating which fields in @attributes are valid
 * 
 * Creates a new #GdkWindow using the attributes from
 * @attributes. See #GdkWindowAttr and #GdkWindowAttributesType for
 * more details.  Note: to use this on displays other than the default
 * display, @parent must be specified.
 * 
 * Return value: the new #GdkWindow
 **/
GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  GdkScreenX11 *screen_x11;
  GdkScreen *screen;
  
  GdkVisual *visual;
  Window xparent;
  Visual *xvisual;
  Display *xdisplay;
  Window xid;

  XSetWindowAttributes xattributes;
  long xattributes_mask;
  XClassHint *class_hint;
  int x, y, depth;
  
  unsigned int class;
  const char *title;
  int i;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent)
    {
      GDK_NOTE (MULTIHEAD,
		g_warning ("gdk_window_new(): no parent specified reverting to parent = default root window"));
      
      screen = gdk_screen_get_default ();
      parent = gdk_screen_get_root_window (screen);
    }
  else
    screen = gdk_drawable_get_screen (parent);

  screen_x11 = GDK_SCREEN_X11 (screen);

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;
  
  xparent = GDK_WINDOW_XID (parent);
  
  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  
  draw_impl->screen = screen;
  xdisplay = screen_x11->xdisplay;

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = gdk_screen_get_root_window (screen);
  
  private->parent = (GdkWindowObject *)parent;

  private->accept_focus = TRUE;
  private->focus_on_map = TRUE;

  xattributes_mask = 0;
  
  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;
  
  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;
  
  private->x = x;
  private->y = y;
  impl->width = (attributes->width > 1) ? (attributes->width) : (1);
  impl->height = (attributes->height > 1) ? (attributes->height) : (1);

  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->window_type for input-only windows
       * before
       */
      if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_ROOT)
	private->window_type = GDK_WINDOW_TEMP;
      else
	private->window_type = GDK_WINDOW_CHILD;
    }
  else
    private->window_type = attributes->window_type;

  /* Work around a bug where Xorg refuses to map toplevel InputOnly windows 
   * from an untrusted client: http://bugs.freedesktop.org/show_bug.cgi?id=6988
   */
  if (attributes->wclass == GDK_INPUT_ONLY &&
      GDK_WINDOW_TYPE (parent) == GDK_WINDOW_ROOT &&
      !G_LIKELY (GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (parent))->trusted_client))
    {
      g_warning ("Coercing GDK_INPUT_ONLY toplevel window to GDK_INPUT_OUTPUT to work around bug in Xorg server");
      attributes->wclass = GDK_INPUT_OUTPUT;
    }

  _gdk_window_init_position (GDK_WINDOW (private));
  if (impl->position_info.big)
    private->guffaw_gravity = TRUE;
  
  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_screen_get_system_visual (screen);
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;
  
  xattributes.event_mask = StructureNotifyMask | PropertyChangeMask;
  for (i = 0; i < _gdk_nenvent_masks; i++)
    {
      if (attributes->event_mask & (1 << (i + 1)))
	xattributes.event_mask |= _gdk_event_mask_table[i];
    }
  private->event_mask = attributes->event_mask;
  
  if (xattributes.event_mask)
    xattributes_mask |= CWEventMask;
  
  if (attributes_mask & GDK_WA_NOREDIR)
    {
      xattributes.override_redirect =
	(attributes->override_redirect == FALSE)?False:True;
      xattributes_mask |= CWOverrideRedirect;
    } 
  else
    xattributes.override_redirect = False;

  impl->override_redirect = xattributes.override_redirect;
  
  if (private->parent && private->parent->guffaw_gravity)
    {
      xattributes.win_gravity = StaticGravity;
      xattributes_mask |= CWWinGravity;
    }
  
  /* Sanity checks */
  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (parent) != GDK_WINDOW_ROOT)
	{
	  g_warning (G_STRLOC "Toplevel windows must be created as children of\n"
		     "of a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
	  xparent = GDK_SCREEN_XROOTWIN (screen);
	}
    case GDK_WINDOW_CHILD:
      break;
    default:
      g_warning (G_STRLOC "cannot make windows of type %d", private->window_type);
      return NULL;
    }
	  
  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      class = InputOutput;
      depth = visual->depth;

      private->input_only = FALSE;
      private->depth = depth;
      
      if (attributes_mask & GDK_WA_COLORMAP)
        {
          draw_impl->colormap = attributes->colormap;
          g_object_ref (attributes->colormap);
        }
      else
	{
	  if ((((GdkVisualPrivate *)gdk_screen_get_system_visual (screen))->xvisual) ==  xvisual)
            {
	      draw_impl->colormap = gdk_screen_get_system_colormap (screen);
              g_object_ref (draw_impl->colormap);
            }
	  else
            {
              draw_impl->colormap = gdk_colormap_new (visual, FALSE);
            }
	}
      
      private->bg_color.pixel = BlackPixel (xdisplay, screen_x11->screen_num);
      private->bg_color.red = private->bg_color.green = private->bg_color.blue = 0;
      xattributes.background_pixel = private->bg_color.pixel;

      private->bg_pixmap = NULL;
      
      xattributes.border_pixel = BlackPixel (xdisplay, screen_x11->screen_num);
      xattributes_mask |= CWBorderPixel | CWBackPixel;

      if (private->guffaw_gravity)
	xattributes.bit_gravity = StaticGravity;
      else
	xattributes.bit_gravity = NorthWestGravity;
      
      xattributes_mask |= CWBitGravity;

      xattributes.colormap = GDK_COLORMAP_XCOLORMAP (draw_impl->colormap);
      xattributes_mask |= CWColormap;

      if (private->window_type == GDK_WINDOW_TEMP)
	{
	  xattributes.save_under = True;
	  xattributes.override_redirect = True;
	  xattributes.cursor = None;
	  xattributes_mask |= CWSaveUnder | CWOverrideRedirect;

	  impl->override_redirect = TRUE;
	}
    }
  else
    {
      depth = 0;
      private->depth = 0;
      class = InputOnly;
      private->input_only = TRUE;
      draw_impl->colormap = gdk_screen_get_system_colormap (screen);
      g_object_ref (draw_impl->colormap);
    }

  xid = draw_impl->xid = XCreateWindow (xdisplay, xparent,
					impl->position_info.x, impl->position_info.y,
					impl->position_info.width, impl->position_info.height,
					0, depth, class, xvisual,
					xattributes_mask, &xattributes);

  g_object_ref (window);
  _gdk_xid_table_insert (screen_x11->display, &draw_impl->xid, window);
  
  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));
  
  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);
  
  switch (GDK_WINDOW_TYPE (private))
    {
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      if (attributes_mask & GDK_WA_TITLE)
	title = attributes->title;
      else
	title = get_default_title ();
      
      gdk_window_set_title (window, title);
      
      if (attributes_mask & GDK_WA_WMCLASS)
	{
	  class_hint = XAllocClassHint ();
	  class_hint->res_name = attributes->wmclass_name;
	  class_hint->res_class = attributes->wmclass_class;
	  XSetClassHint (xdisplay, xid, class_hint);
	  XFree (class_hint);
	}
  
      setup_toplevel_window (window, parent);
      break;

    case GDK_WINDOW_CHILD:
      if ((attributes->wclass == GDK_INPUT_OUTPUT) &&
	  (draw_impl->colormap != gdk_screen_get_system_colormap (screen)) &&
	  (draw_impl->colormap != gdk_drawable_get_colormap (gdk_window_get_toplevel (window))))
	{
	  GDK_NOTE (MISC, g_message ("adding colormap window\n"));
	  gdk_window_add_colormap_windows (window);
	}
      break;
      
    default:
      break;
    }

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  return window;
}

static GdkEventMask
x_event_mask_to_gdk_event_mask (long mask)
{
  GdkEventMask event_mask = 0;
  int i;

  for (i = 0; i < _gdk_nenvent_masks; i++)
    {
      if (mask & _gdk_event_mask_table[i])
	event_mask |= 1 << (i + 1);
    }

  return event_mask;
}

/**
 * gdk_window_foreign_new_for_display:
 * @display: the #GdkDisplay where the window handle comes from.
 * @anid: a native window handle.
 * 
 * Wraps a native window in a #GdkWindow.
 * This may fail if the window has been destroyed. If the window
 * was already known to GDK, a new reference to the existing 
 * #GdkWindow is returned.
 *
 * For example in the X backend, a native window handle is an Xlib
 * <type>XID</type>.
 * 
 * Return value: a #GdkWindow wrapper for the native window or 
 *   %NULL if the window has been destroyed. The wrapper will be
 *   newly created, if one doesn't exist already.
 *
 * Since: 2.2
 **/
GdkWindow *
gdk_window_foreign_new_for_display (GdkDisplay     *display,
				    GdkNativeWindow anid)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  GdkDisplayX11 *display_x11;
  XWindowAttributes attrs;
  Window root, parent;
  Window *children = NULL;
  guint nchildren;
  gboolean result;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  display_x11 = GDK_DISPLAY_X11 (display);

  if ((window = gdk_xid_table_lookup_for_display (display, anid)) != NULL)
    return g_object_ref (window);

  gdk_error_trap_push ();
  result = XGetWindowAttributes (display_x11->xdisplay, anid, &attrs);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  /* FIXME: This is pretty expensive. Maybe the caller should supply
   *        the parent */
  gdk_error_trap_push ();
  result = XQueryTree (display_x11->xdisplay, anid, &root, &parent, &children, &nchildren);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  if (children)
    XFree (children);
  
  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  draw_impl->screen = _gdk_x11_display_screen_for_xrootwin (display, root);
  
  private->parent = gdk_xid_table_lookup_for_display (display, parent);
  
  if (!private->parent || GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_FOREIGN)
    private->parent = (GdkWindowObject *) gdk_screen_get_root_window (draw_impl->screen);
  
  private->parent->children = g_list_prepend (private->parent->children, window);

  draw_impl->xid = anid;

  private->x = attrs.x;
  private->y = attrs.y;
  impl->width = attrs.width;
  impl->height = attrs.height;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;

  private->event_mask = x_event_mask_to_gdk_event_mask (attrs.your_event_mask);

  if (attrs.map_state == IsUnmapped)
    private->state = GDK_WINDOW_STATE_WITHDRAWN;
  else
    private->state = 0;

  private->depth = attrs.depth;
  
  _gdk_window_init_position (GDK_WINDOW (private));

  g_object_ref (window);
  _gdk_xid_table_insert (display, &GDK_WINDOW_XID (window), window);
  return window;
}

/**
 * gdk_window_lookup_for_display:
 * @display: the #GdkDisplay corresponding to the window handle
 * @anid: a native window handle.
 *
 * Looks up the #GdkWindow that wraps the given native window handle.
 *
 * For example in the X backend, a native window handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the #GdkWindow wrapper for the native window, 
 *    or %NULL if there is none.
 *
 * Since: 2.2
 **/
GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  return (GdkWindow*) gdk_xid_table_lookup_for_display (display, anid);
}

/**
 * gdk_window_lookup:
 * @anid: a native window handle.
 *
 * Looks up the #GdkWindow that wraps the given native window handle. 
 *
 * For example in the X backend, a native window handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the #GdkWindow wrapper for the native window, 
 *    or %NULL if there is none.
 **/
GdkWindow *
gdk_window_lookup (GdkNativeWindow anid)
{
  return (GdkWindow*) gdk_xid_table_lookup (anid);
}

static void
gdk_toplevel_x11_free_contents (GdkDisplay *display,
				GdkToplevelX11 *toplevel)
{
  if (toplevel->icon_window)
    {
      g_object_unref (toplevel->icon_window);
      toplevel->icon_window = NULL;
    }
  if (toplevel->icon_pixmap)
    {
      g_object_unref (toplevel->icon_pixmap);
      toplevel->icon_pixmap = NULL;
    }
  if (toplevel->icon_mask)
    {
      g_object_unref (toplevel->icon_mask);
      toplevel->icon_mask = NULL;
    }
  if (toplevel->group_leader)
    {
      g_object_unref (toplevel->group_leader);
      toplevel->group_leader = NULL;
    }
#ifdef HAVE_XSYNC
  if (toplevel->update_counter != None)
    {
      XSyncDestroyCounter (GDK_DISPLAY_XDISPLAY (display), 
			   toplevel->update_counter);
      toplevel->update_counter = None;

      XSyncIntToValue (&toplevel->current_counter_value, 0);
    }
#endif
}

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  _gdk_selection_window_destroyed (window);
  
  if (private->extension_events != 0)
    _gdk_input_window_destroy (window);

  toplevel = _gdk_x11_window_get_toplevel (window);
  if (toplevel)
    gdk_toplevel_x11_free_contents (GDK_WINDOW_DISPLAY (window), toplevel);

  _gdk_x11_drawable_finish (private->impl);

  if (!recursing && !foreign_destroy)
    {
      XDestroyWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
    }
}

void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
  /* It's somebody else's window, but in our heirarchy,
   * so reparent it to the root window, and then send
   * it a delete event, as if we were a WM
   */
  XClientMessageEvent xclient;
  
  gdk_error_trap_push ();
  gdk_window_hide (window);
  gdk_window_reparent (window, NULL, 0, 0);
  
  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
							       "WM_PROTOCOLS");
  xclient.format = 32;
  xclient.data.l[0] = gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
							    "WM_DELETE_WINDOW");
  xclient.data.l[1] = CurrentTime;
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;
  
  XSendEvent (GDK_WINDOW_XDISPLAY (window),
	      GDK_WINDOW_XID (window),
	      False, 0, (XEvent *)&xclient);
  gdk_display_sync (GDK_WINDOW_DISPLAY (window));
  gdk_error_trap_pop ();
}

static GdkWindow *
get_root (GdkWindow *window)
{
  GdkScreen *screen = gdk_drawable_get_screen (window);

  return gdk_screen_get_root_window (screen);
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  GdkWindowImplX11 *window_impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  window_impl = GDK_WINDOW_IMPL_X11 (((GdkWindowObject *)window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %#lx unexpectedly destroyed", GDK_WINDOW_XID (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  _gdk_xid_table_remove (GDK_WINDOW_DISPLAY (window), GDK_WINDOW_XID (window));
  if (window_impl->toplevel && window_impl->toplevel->focus_window)
    _gdk_xid_table_remove (GDK_WINDOW_DISPLAY (window), window_impl->toplevel->focus_window);

  _gdk_xgrab_check_destroy (window);
  
  g_object_unref (window);
}

static void
update_wm_hints (GdkWindow *window,
		 gboolean   force)
{
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  XWMHints wm_hints;

  if (!force &&
      !toplevel->is_leader &&
      private->state & GDK_WINDOW_STATE_WITHDRAWN)
    return;

  wm_hints.flags = StateHint | InputHint;
  wm_hints.input = private->accept_focus ? True : False;
  wm_hints.initial_state = NormalState;
  
  if (private->state & GDK_WINDOW_STATE_ICONIFIED)
    {
      wm_hints.flags |= StateHint;
      wm_hints.initial_state = IconicState;
    }

  if (toplevel->icon_window && !GDK_WINDOW_DESTROYED (toplevel->icon_window))
    {
      wm_hints.flags |= IconWindowHint;
      wm_hints.icon_window = GDK_WINDOW_XID (toplevel->icon_window);
    }

  if (toplevel->icon_pixmap)
    {
      wm_hints.flags |= IconPixmapHint;
      wm_hints.icon_pixmap = GDK_PIXMAP_XID (toplevel->icon_pixmap);
    }

  if (toplevel->icon_mask)
    {
      wm_hints.flags |= IconMaskHint;
      wm_hints.icon_mask = GDK_PIXMAP_XID (toplevel->icon_mask);
    }
  
  wm_hints.flags |= WindowGroupHint;
  if (toplevel->group_leader && !GDK_WINDOW_DESTROYED (toplevel->group_leader))
    {
      wm_hints.flags |= WindowGroupHint;
      wm_hints.window_group = GDK_WINDOW_XID (toplevel->group_leader);
    }
  else
    wm_hints.window_group = GDK_DISPLAY_X11 (display)->leader_window;

  if (toplevel->urgency_hint)
    wm_hints.flags |= XUrgencyHint;
  
  XSetWMHints (GDK_WINDOW_XDISPLAY (window),
	       GDK_WINDOW_XID (window),
	       &wm_hints);
}

static void
set_initial_hints (GdkWindow *window)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  Window xwindow = GDK_WINDOW_XID (window);  
  GdkWindowObject *private;
  GdkToplevelX11 *toplevel;
  Atom atoms[9];
  gint i;

  private = (GdkWindowObject*) window;
  toplevel = _gdk_x11_window_get_toplevel (window);

  if (!toplevel)
    return;

  update_wm_hints (window, TRUE);
  
  /* We set the spec hints regardless of whether the spec is supported,
   * since it can't hurt and it's kind of expensive to check whether
   * it's supported.
   */
  
  i = 0;

  if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MAXIMIZED_VERT");
      ++i;
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MAXIMIZED_HORZ");
      ++i;
    }

  if (private->state & GDK_WINDOW_STATE_ABOVE)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_ABOVE");
      ++i;
    }
  
  if (private->state & GDK_WINDOW_STATE_BELOW)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_BELOW");
      ++i;
    }
  
  if (private->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_STICKY");
      ++i;
    }

  if (private->state & GDK_WINDOW_STATE_FULLSCREEN)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_FULLSCREEN");
      ++i;
    }

  if (private->modal_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MODAL");
      ++i;
    }

  if (toplevel->skip_taskbar_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_SKIP_TASKBAR");
      ++i;
    }

  if (toplevel->skip_pager_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_SKIP_PAGER");
      ++i;
    }

  if (i > 0)
    {
      XChangeProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
                       XA_ATOM, 32, PropModeReplace,
                       (guchar*) atoms, i);
    }
  else 
    {
      XDeleteProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"));
    }

  if (private->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[0] = 0xFFFFFFFF;
      XChangeProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"),
                       XA_CARDINAL, 32, PropModeReplace,
                       (guchar*) atoms, 1);
    }
  else
    {
      XDeleteProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"));
    }

  toplevel->map_serial = NextRequest (xdisplay);
}

static void
show_window_internal (GdkWindow *window,
                      gboolean   raise)
{
  GdkWindowObject *private;
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);
      Display *xdisplay = GDK_WINDOW_XDISPLAY (window);
      Window xwindow = GDK_WINDOW_XID (window);
      
      if (raise)
        XRaiseWindow (xdisplay, xwindow);

      if (!GDK_WINDOW_IS_MAPPED (window))
        {
          set_initial_hints (window);
          
          gdk_synthesize_window_state (window,
                                       GDK_WINDOW_STATE_WITHDRAWN,
                                       0);
        }
      
      g_assert (GDK_WINDOW_IS_MAPPED (window));

      if (WINDOW_IS_TOPLEVEL (window))
	{
	  display = gdk_drawable_get_display (window);
	  display_x11 = GDK_DISPLAY_X11 (display);
	  toplevel = _gdk_x11_window_get_toplevel (window);

          if (toplevel->user_time != 0 &&
	      display_x11->user_time != 0 &&
	      XSERVER_TIME_IS_LATER (display_x11->user_time, toplevel->user_time))
	    gdk_x11_window_set_user_time (window, display_x11->user_time);
	}

      if (impl->position_info.mapped)
	{
	  gboolean unset_bg = !private->input_only &&
	    (private->window_type == GDK_WINDOW_CHILD ||
	     impl->override_redirect) &&
	    gdk_window_is_viewable (window);

	  if (unset_bg)
	    _gdk_x11_window_tmp_unset_bg (window, TRUE);
	  
	  XMapWindow (xdisplay, xwindow);

	  if (unset_bg)
	    {
	      _gdk_x11_window_tmp_reset_bg (window, TRUE);
	      gdk_window_invalidate_rect (window, NULL, TRUE);
	    }
	}
    }
}

/**
 * gdk_window_show_unraised:
 * @window: a #GdkWindow
 *
 * Shows a #GdkWindow onscreen, but does not modify its stacking
 * order. In contrast, gdk_window_show() will raise the window
 * to the top of the window stack.
 *
 * On the X11 platform, in Xlib terms, this function calls
 * XMapWindow() (it also updates some internal GDK state, which means
 * that you can't really use XMapWindow() directly on a GDK window).
 * 
 **/
void
gdk_window_show_unraised (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  show_window_internal (window, FALSE);
}

/**
 * gdk_window_show:
 * @window: a #GdkWindow
 *
 * Like gdk_window_show_unraised(), but also raises the window to the
 * top of the window stack (moves the window to the front of the
 * Z-order).
 *
 * This function maps a window so it's visible onscreen. Its opposite
 * is gdk_window_hide().
 *
 * When implementing a #GtkWidget, you should call this function on the widget's
 * #GdkWindow as part of the "map" method.
 * 
 **/
void
gdk_window_show (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, TRUE);
}

static void
pre_unmap (GdkWindow *window)
{
  GdkWindow *start_window = NULL;
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (private->input_only)
    return;

  if (private->window_type == GDK_WINDOW_CHILD)
    start_window = (GdkWindow *)private->parent;
  else if (private->window_type == GDK_WINDOW_TEMP)
    start_window = get_root (window);

  if (start_window)
    _gdk_x11_window_tmp_unset_bg (start_window, TRUE);
}

static void
post_unmap (GdkWindow *window)
{
  GdkWindow *start_window = NULL;
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  if (private->input_only)
    return;

  if (private->window_type == GDK_WINDOW_CHILD)
    start_window = (GdkWindow *)private->parent;
  else if (private->window_type == GDK_WINDOW_TEMP)
    start_window = get_root (window);

  if (start_window)
    {
      _gdk_x11_window_tmp_reset_bg (start_window, TRUE);

      if (private->window_type == GDK_WINDOW_CHILD && private->parent)
	{
	  GdkRectangle invalid_rect;
      
	  gdk_window_get_position (window, &invalid_rect.x, &invalid_rect.y);
	  gdk_drawable_get_size (GDK_DRAWABLE (window),
				 &invalid_rect.width, &invalid_rect.height);
	  gdk_window_invalidate_rect ((GdkWindow *)private->parent,
				      &invalid_rect, TRUE);
	}
    }
}

/**
 * gdk_window_hide:
 * @window: a #GdkWindow
 *
 * For toplevel windows, withdraws them, so they will no longer be
 * known to the window manager; for all windows, unmaps them, so
 * they won't be displayed. Normally done automatically as
 * part of gtk_widget_hide().
 * 
 **/
void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;

  /* We'll get the unmap notify eventually, and handle it then,
   * but checking here makes things more consistent if we are
   * just doing stuff ourself.
   */
  _gdk_xgrab_check_unmap (window,
			  NextRequest (GDK_WINDOW_XDISPLAY (window)));

  /* You can't simply unmap toplevel windows. */
  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP: /* ? */
      gdk_window_withdraw (window);
      return;
      break;
      
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_CHILD:
      break;
    }
  
  if (!private->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_WITHDRAWN);

      g_assert (!GDK_WINDOW_IS_MAPPED (window));
      
      _gdk_window_clear_update_area (window);

      pre_unmap (window);
      
      XUnmapWindow (GDK_WINDOW_XDISPLAY (window),
                    GDK_WINDOW_XID (window));

      post_unmap (window);
    }
}

/**
 * gdk_window_withdraw:
 * @window: a toplevel #GdkWindow
 * 
 * Withdraws a window (unmaps it and asks the window manager to forget about it).
 * This function is not really useful as gdk_window_hide() automatically
 * withdraws toplevel windows before hiding them.
 * 
 **/
void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_WITHDRAWN);

      g_assert (!GDK_WINDOW_IS_MAPPED (window));

      pre_unmap (window);
      
      XWithdrawWindow (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window), 0);

      post_unmap (window);
    }
}

/**
 * gdk_window_move:
 * @window: a #GdkWindow
 * @x: X coordinate relative to window's parent
 * @y: Y coordinate relative to window's parent
 *
 * Repositions a window relative to its parent window.
 * For toplevel windows, window managers may ignore or modify the move;
 * you should probably use gtk_window_move() on a #GtkWindow widget
 * anyway, instead of using GDK functions. For child windows,
 * the move will reliably succeed.
 *
 * If you're also planning to resize the window, use gdk_window_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_X11 (private->impl);
	  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	{
	  _gdk_window_move_resize_child (window, x, y,
					 impl->width, impl->height);
	}
      else
	{
	  XMoveWindow (GDK_WINDOW_XDISPLAY (window),
		       GDK_WINDOW_XID (window),
		       x, y);

	  if (impl->override_redirect)
	    {
	      private->x = x;
	      private->y = y;
	    }
	}
    }
}

/**
 * gdk_window_resize:
 * @window: a #GdkWindow
 * @width: new width of the window
 * @height: new height of the window
 *
 * Resizes @window; for toplevel windows, asks the window manager to resize
 * the window. The window manager may not allow the resize. When using GTK+,
 * use gtk_window_resize() instead of this low-level GDK function.
 *
 * Windows may not be resized below 1x1.
 * 
 * If you're also planning to move the window, use gdk_window_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowObject*) window;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	{
	  _gdk_window_move_resize_child (window, private->x, private->y,
					 width, height);
	  _gdk_x11_drawable_update_size (private->impl);
	}
      else
	{
	  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);

	  XResizeWindow (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 width, height);

	  if (impl->override_redirect)
	    {
	      impl->width = width;
	      impl->height = height;
	      _gdk_x11_drawable_update_size (private->impl);
	    }
	  else
	    {
	      if (width != impl->width || height != impl->height)
		private->resize_count += 1;
	    }
	}

      _gdk_x11_drawable_update_size (private->impl);
    }
}

/**
 * gdk_window_move_resize:
 * @window: a #GdkWindow
 * @x: new X position relative to window's parent
 * @y: new Y position relative to window's parent
 * @width: new width
 * @height: new height
 *
 * Equivalent to calling gdk_window_move() and gdk_window_resize(),
 * except that both operations are performed at once, avoiding strange
 * visual effects. (i.e. the user may be able to see the window first
 * move, then resize, if you don't use gdk_window_move_resize().)
 **/
void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  
  private = (GdkWindowObject*) window;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	{
	  _gdk_window_move_resize_child (window, x, y, width, height);
	  _gdk_x11_drawable_update_size (private->impl);
	}
      else
	{
	  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);

	  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
			     GDK_WINDOW_XID (window),
			     x, y, width, height);
	  if (impl->override_redirect)
	    {
	      private->x = x;
	      private->y = y;
	      impl->width = width;
	      impl->height = height;

	      _gdk_x11_drawable_update_size (private->impl);
	    }
	  else
	    {
	      if (width != impl->width || height != impl->height)
		private->resize_count += 1;
	    }
	}
    }
}

/**
 * gdk_window_reparent:
 * @window: a #GdkWindow
 * @new_parent: new parent to move @window into
 * @x: X location inside the new parent
 * @y: Y location inside the new parent
 *
 * Reparents @window into the given @new_parent. The window being
 * reparented will be unmapped as a side effect.
 * 
 **/
void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowObject *window_private;
  GdkWindowObject *parent_private;
  GdkWindowObject *old_parent_private;
  GdkWindowImplX11 *impl;
  gboolean was_toplevel;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  if (GDK_WINDOW_DESTROYED (window) ||
      (new_parent && GDK_WINDOW_DESTROYED (new_parent)))
    {
      return;
    }
  
  if (!new_parent)
    new_parent = gdk_screen_get_root_window (GDK_WINDOW_SCREEN (window));

  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject*)window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  impl = GDK_WINDOW_IMPL_X11 (window_private->impl);
  
  XReparentWindow (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   GDK_WINDOW_XID (new_parent),
		   x, y);

  window_private->x = x;
  window_private->y = y;
  
  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    new_parent = gdk_screen_get_root_window (GDK_WINDOW_SCREEN (window));
  
  window_private->parent = (GdkWindowObject *)new_parent;

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_FOREIGN:
      was_toplevel = WINDOW_IS_TOPLEVEL (window);
      
      if (impl->toplevel_window_type != -1)
	GDK_WINDOW_TYPE (window) = impl->toplevel_window_type;
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;

      if (WINDOW_IS_TOPLEVEL (window) && !was_toplevel)
	setup_toplevel_window (window, new_parent);
      break;
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (WINDOW_IS_TOPLEVEL (window))
	{
	  /* Save the original window type so we can restore it if the
	   * window is reparented back to be a toplevel
	   */
	  impl->toplevel_window_type = GDK_WINDOW_TYPE (window);
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
	  if (impl->toplevel)
	    {
	      if (impl->toplevel->focus_window)
		{
		  XDestroyWindow (GDK_WINDOW_XDISPLAY (window), impl->toplevel->focus_window);
		  _gdk_xid_table_remove (GDK_WINDOW_DISPLAY (window), impl->toplevel->focus_window);
		}
		
	      gdk_toplevel_x11_free_contents (GDK_WINDOW_DISPLAY (window), 
					      impl->toplevel);
	      g_free (impl->toplevel);
	      impl->toplevel = NULL;
	    }
	}
    }

  if (old_parent_private)
    old_parent_private->children = g_list_remove (old_parent_private->children, window);
  
  if ((old_parent_private &&
       (!old_parent_private->guffaw_gravity != !parent_private->guffaw_gravity)) ||
      (!old_parent_private && parent_private->guffaw_gravity))
    gdk_window_set_static_win_gravity (window, parent_private->guffaw_gravity);
  
  parent_private->children = g_list_prepend (parent_private->children, window);
  _gdk_window_init_position (GDK_WINDOW (window_private));
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XClearArea (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		x, y, width, height, False);
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
    XClearArea (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		x, y, width, height, True);
}


/**
 * gdk_window_raise:
 * @window: a #GdkWindow
 * 
 * Raises @window to the top of the Z-order (stacking order), so that
 * other windows with the same parent window appear below @window.
 * This is true whether or not the windows are visible.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_raise() only
 * requests the restack, does not guarantee it.
 * 
 **/
void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XRaiseWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

/**
 * gdk_window_lower:
 * @window: a #GdkWindow
 * 
 * Lowers @window to the bottom of the Z-order (stacking order), so that
 * other windows with the same parent window appear above @window.
 * This is true whether or not the other windows are visible.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_lower() only
 * requests the restack, does not guarantee it.
 *
 * Note that gdk_window_show() raises the window again, so don't call this
 * function before gdk_window_show(). (Try gdk_window_show_unraised().)
 * 
 **/
void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XLowerWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

/**
 * gdk_x11_window_move_to_current_desktop:
 * @window: a #GdkWindow
 * 
 * Moves the window to the correct workspace when running under a 
 * window manager that supports multiple workspaces, as described
 * in the <ulink url="http://www.freedesktop.org/Standards/wm-spec">Extended 
 * Window Manager Hints</ulink>.  Will not do anything if the
 * window is already on all workspaces.
 * 
 * Since: 2.8
 */
void
gdk_x11_window_move_to_current_desktop (GdkWindow *window)
{
  GdkToplevelX11 *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (toplevel->on_all_desktops)
    return;
  
  move_to_current_desktop (window);
}

static void
move_to_current_desktop (GdkWindow *window)
{
  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
					   gdk_atom_intern_static_string ("_NET_WM_DESKTOP")))
    {
      Atom type;
      gint format;
      gulong nitems;
      gulong bytes_after;
      guchar *data;
      gulong *current_desktop;
      GdkDisplay *display;
      
      display = gdk_drawable_get_display (window);

      /* Get current desktop, then set it; this is a race, but not
       * one that matters much in practice.
       */
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), 
                          GDK_WINDOW_XROOTWIN (window),
			  gdk_x11_get_xatom_by_name_for_display (display, "_NET_CURRENT_DESKTOP"),
                          0, G_MAXLONG,
                          False, XA_CARDINAL, &type, &format, &nitems,
                          &bytes_after, &data);

      if (type == XA_CARDINAL)
        {
	  XClientMessageEvent xclient;
	  current_desktop = (gulong *)data;
	  
	  memset (&xclient, 0, sizeof (xclient));
          xclient.type = ClientMessage;
          xclient.serial = 0;
          xclient.send_event = True;
          xclient.window = GDK_WINDOW_XWINDOW (window);
	  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP");
          xclient.format = 32;

          xclient.data.l[0] = *current_desktop;
          xclient.data.l[1] = 0;
          xclient.data.l[2] = 0;
          xclient.data.l[3] = 0;
          xclient.data.l[4] = 0;
      
          XSendEvent (GDK_DISPLAY_XDISPLAY (display), 
                      GDK_WINDOW_XROOTWIN (window), 
                      False,
                      SubstructureRedirectMask | SubstructureNotifyMask,
                      (XEvent *)&xclient);

          XFree (current_desktop);
        }
    }
}

/**
 * gdk_window_focus:
 * @window: a #GdkWindow
 * @timestamp: timestamp of the event triggering the window focus
 *
 * Sets keyboard focus to @window. In most cases, gtk_window_present() 
 * should be used on a #GtkWindow, rather than calling this function.
 * 
 **/
void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  GdkDisplay *display;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = GDK_WINDOW_DISPLAY (window);

  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
					   gdk_atom_intern_static_string ("_NET_ACTIVE_WINDOW")))
    {
      XClientMessageEvent xclient;

      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_WINDOW_XWINDOW (window);
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display,
									"_NET_ACTIVE_WINDOW");
      xclient.format = 32;
      xclient.data.l[0] = 1; /* requestor type; we're an app */
      xclient.data.l[1] = timestamp;
      xclient.data.l[2] = None; /* currently active window */
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;
      
      XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XROOTWIN (window), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
  else
    {
      XRaiseWindow (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window));

      /* There is no way of knowing reliably whether we are viewable;
       * _gdk_x11_set_input_focus_safe() traps errors asynchronously.
       */
      _gdk_x11_set_input_focus_safe (display, GDK_WINDOW_XID (window),
				     RevertToParent,
				     timestamp);
    }
}

/**
 * gdk_window_set_hints:
 * @window: a #GdkWindow
 * @x: ignored field, does not matter
 * @y: ignored field, does not matter
 * @min_width: minimum width hint
 * @min_height: minimum height hint
 * @max_width: max width hint
 * @max_height: max height hint
 * @flags: logical OR of GDK_HINT_POS, GDK_HINT_MIN_SIZE, and/or GDK_HINT_MAX_SIZE
 *
 * This function is broken and useless and you should ignore it.
 * If using GTK+, use functions such as gtk_window_resize(), gtk_window_set_size_request(),
 * gtk_window_move(), gtk_window_parse_geometry(), and gtk_window_set_geometry_hints(),
 * depending on what you're trying to do.
 *
 * If using GDK directly, use gdk_window_set_geometry_hints().
 * 
 **/
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
  XSizeHints size_hints;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  size_hints.flags = 0;
  
  if (flags & GDK_HINT_POS)
    {
      size_hints.flags |= PPosition;
      size_hints.x = x;
      size_hints.y = y;
    }
  
  if (flags & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = min_width;
      size_hints.min_height = min_height;
    }
  
  if (flags & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = max_width;
      size_hints.max_height = max_height;
    }
  
  /* FIXME: Would it be better to delete this property if
   *        flags == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &size_hints);
}

/**
 * gdk_window_set_type_hint:
 * @window: A toplevel #GdkWindow
 * @hint: A hint of the function this window will have
 *
 * The application can use this call to provide a hint to the window
 * manager about the functionality of a window. The window manager
 * can use this information when determining the decoration and behaviour
 * of the window.
 *
 * The hint must be set before the window is mapped.
 **/
void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  GdkDisplay *display;
  Atom atom;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DIALOG");
      break;
    case GDK_WINDOW_TYPE_HINT_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLBAR");
      break;
    case GDK_WINDOW_TYPE_HINT_UTILITY:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_UTILITY");
      break;
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_SPLASH");
      break;
    case GDK_WINDOW_TYPE_HINT_DOCK:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DOCK");
      break;
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DESKTOP");
      break;
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case GDK_WINDOW_TYPE_HINT_COMBO:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_COMBO");
      break;
    case GDK_WINDOW_TYPE_HINT_DND:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_window_set_type_hint", hint);
      /* Fall thru */
    case GDK_WINDOW_TYPE_HINT_NORMAL:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NORMAL");
      break;
    }

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&atom, 1);
}

/**
 * gdk_window_get_type_hint:
 * @window: A toplevel #GdkWindow
 *
 * This function returns the type hint set for a window.
 *
 * Return value: The type hint set for @window
 *
 * Since: 2.10
 **/
GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  GdkDisplay *display;
  GdkWindowTypeHint type;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);

  if (GDK_WINDOW_DESTROYED (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;

  type = GDK_WINDOW_TYPE_HINT_NORMAL;

  display = gdk_drawable_get_display (window);

  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                          gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
                          0, G_MAXLONG, False, XA_ATOM, &type_return,
                          &format_return, &nitems_return, &bytes_after_return,
                          &data) == Success)
    {
      if ((type_return == XA_ATOM) && (format_return == 32) &&
          (data) && (nitems_return == 1))
        {
          Atom atom = *(Atom*)data;

          if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DIALOG"))
            type = GDK_WINDOW_TYPE_HINT_DIALOG;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_MENU"))
            type = GDK_WINDOW_TYPE_HINT_MENU;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLBAR"))
            type = GDK_WINDOW_TYPE_HINT_TOOLBAR;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_UTILITY"))
            type = GDK_WINDOW_TYPE_HINT_UTILITY;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_SPLASH"))
            type = GDK_WINDOW_TYPE_HINT_SPLASHSCREEN;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DOCK"))
            type = GDK_WINDOW_TYPE_HINT_DOCK;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DESKTOP"))
            type = GDK_WINDOW_TYPE_HINT_DESKTOP;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"))
	    type = GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_POPUP_MENU"))
	    type = GDK_WINDOW_TYPE_HINT_POPUP_MENU;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLTIP"))
	    type = GDK_WINDOW_TYPE_HINT_TOOLTIP;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NOTIFICATION"))
	    type = GDK_WINDOW_TYPE_HINT_NOTIFICATION;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_COMBO"))
	    type = GDK_WINDOW_TYPE_HINT_COMBO;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DND"))
	    type = GDK_WINDOW_TYPE_HINT_DND;
        }

      if (type_return != None && data != NULL)
        XFree (data);
    }

  return type;
}

static void
gdk_wmspec_change_state (gboolean   add,
			 GdkWindow *window,
			 GdkAtom    state1,
			 GdkAtom    state2)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  XClientMessageEvent xclient;
  
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE");
  xclient.format = 32;
  xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xclient.data.l[1] = gdk_x11_atom_to_xatom_for_display (display, state1);
  xclient.data.l[2] = gdk_x11_atom_to_xatom_for_display (display, state2);
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;
  
  XSendEvent (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XROOTWIN (window), False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      (XEvent *)&xclient);
}

/**
 * gdk_window_set_modal_hint:
 * @window: A toplevel #GdkWindow
 * @modal: %TRUE if the window is modal, %FALSE otherwise.
 *
 * The application can use this hint to tell the window manager
 * that a certain window has modal behaviour. The window manager
 * can use this information to handle modal windows in a special
 * way.
 *
 * You should only use this on windows for which you have
 * previously called gdk_window_set_transient_for()
 **/
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

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (modal, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MODAL"), 
			     NULL);
}

/**
 * gdk_window_set_skip_taskbar_hint:
 * @window: a toplevel #GdkWindow
 * @skips_taskbar: %TRUE to skip the taskbar
 * 
 * Toggles whether a window should appear in a task list or window
 * list. If a window's semantic type as specified with
 * gdk_window_set_type_hint() already fully describes the window, this
 * function should <emphasis>not</emphasis> be called in addition, 
 * instead you should allow the window to be treated according to 
 * standard policy for its semantic type.
 *
 * Since: 2.2
 **/
void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
                                  gboolean   skips_taskbar)
{
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  toplevel->skip_taskbar_hint = skips_taskbar;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (skips_taskbar, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_SKIP_TASKBAR"), 
			     NULL);
}

/**
 * gdk_window_set_skip_pager_hint:
 * @window: a toplevel #GdkWindow
 * @skips_pager: %TRUE to skip the pager
 * 
 * Toggles whether a window should appear in a pager (workspace
 * switcher, or other desktop utility program that displays a small
 * thumbnail representation of the windows on the desktop). If a
 * window's semantic type as specified with gdk_window_set_type_hint()
 * already fully describes the window, this function should 
 * <emphasis>not</emphasis> be called in addition, instead you should 
 * allow the window to be treated according to standard policy for 
 * its semantic type.
 *
 * Since: 2.2
 **/
void
gdk_window_set_skip_pager_hint (GdkWindow *window,
                                gboolean   skips_pager)
{
  GdkToplevelX11 *toplevel;
    
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  toplevel->skip_pager_hint = skips_pager;
  
  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (skips_pager, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_SKIP_PAGER"), 
			     NULL);
}

/**
 * gdk_window_set_urgency_hint:
 * @window: a toplevel #GdkWindow
 * @urgent: %TRUE if the window is urgent
 * 
 * Toggles whether a window needs the user's
 * urgent attention.
 *
 * Since: 2.8
 **/
void
gdk_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  GdkToplevelX11 *toplevel;
    
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  toplevel->urgency_hint = urgent;
  
  update_wm_hints (window, FALSE);
}

/**
 * gdk_window_set_geometry_hints:
 * @window: a toplevel #GdkWindow
 * @geometry: geometry hints
 * @geom_mask: bitmask indicating fields of @geometry to pay attention to
 *
 * Sets the geometry hints for @window. Hints flagged in @geom_mask
 * are set, hints not flagged in @geom_mask are unset.
 * To unset all hints, use a @geom_mask of 0 and a @geometry of %NULL.
 *
 * This function provides hints to the windowing system about
 * acceptable sizes for a toplevel window. The purpose of 
 * this is to constrain user resizing, but the windowing system
 * will typically  (but is not required to) also constrain the
 * current size of the window to the provided values and
 * constrain programatic resizing via gdk_window_resize() or
 * gdk_window_move_resize().
 * 
 * Note that on X11, this effect has no effect on windows
 * of type %GDK_WINDOW_TEMP or windows where override redirect
 * has been turned on via gdk_window_set_override_redirect()
 * since these windows are not resizable by the user.
 * 
 * Since you can't count on the windowing system doing the
 * constraints for programmatic resizes, you should generally
 * call gdk_window_constrain_size() yourself to determine
 * appropriate sizes.
 *
 **/
void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  XSizeHints size_hints;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  size_hints.flags = 0;
  
  if (geom_mask & GDK_HINT_POS)
    {
      size_hints.flags |= PPosition;
      /* We need to initialize the following obsolete fields because KWM 
       * apparently uses these fields if they are non-zero.
       * #@#!#!$!.
       */
      size_hints.x = 0;
      size_hints.y = 0;
    }

  if (geom_mask & GDK_HINT_USER_POS)
    {
      size_hints.flags |= USPosition;
    }

  if (geom_mask & GDK_HINT_USER_SIZE)
    {
      size_hints.flags |= USSize;
    }
  
  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = geometry->min_width;
      size_hints.min_height = geometry->min_height;
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = MAX (geometry->max_width, 1);
      size_hints.max_height = MAX (geometry->max_height, 1);
    }
  
  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      size_hints.flags |= PBaseSize;
      size_hints.base_width = geometry->base_width;
      size_hints.base_height = geometry->base_height;
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      size_hints.flags |= PResizeInc;
      size_hints.width_inc = geometry->width_inc;
      size_hints.height_inc = geometry->height_inc;
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      size_hints.flags |= PAspect;
      if (geometry->min_aspect <= 1)
	{
	  size_hints.min_aspect.x = 65536 * geometry->min_aspect;
	  size_hints.min_aspect.y = 65536;
	}
      else
	{
	  size_hints.min_aspect.x = 65536;
	  size_hints.min_aspect.y = 65536 / geometry->min_aspect;;
	}
      if (geometry->max_aspect <= 1)
	{
	  size_hints.max_aspect.x = 65536 * geometry->max_aspect;
	  size_hints.max_aspect.y = 65536;
	}
      else
	{
	  size_hints.max_aspect.x = 65536;
	  size_hints.max_aspect.y = 65536 / geometry->max_aspect;;
	}
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      size_hints.flags |= PWinGravity;
      size_hints.win_gravity = geometry->win_gravity;
    }
  
  /* FIXME: Would it be better to delete this property if
   *        geom_mask == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &size_hints);
}

static void
gdk_window_get_geometry_hints (GdkWindow      *window,
                               GdkGeometry    *geometry,
                               GdkWindowHints *geom_mask)
{
  XSizeHints *size_hints;  
  glong junk_supplied_mask = 0;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (geometry != NULL);
  g_return_if_fail (geom_mask != NULL);

  *geom_mask = 0;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  size_hints = XAllocSizeHints ();
  if (!size_hints)
    return;
  
  if (!XGetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
                          GDK_WINDOW_XID (window),
                          size_hints,
                          &junk_supplied_mask))
    size_hints->flags = 0;

  if (size_hints->flags & PMinSize)
    {
      *geom_mask |= GDK_HINT_MIN_SIZE;
      geometry->min_width = size_hints->min_width;
      geometry->min_height = size_hints->min_height;
    }

  if (size_hints->flags & PMaxSize)
    {
      *geom_mask |= GDK_HINT_MAX_SIZE;
      geometry->max_width = MAX (size_hints->max_width, 1);
      geometry->max_height = MAX (size_hints->max_height, 1);
    }

  if (size_hints->flags & PResizeInc)
    {
      *geom_mask |= GDK_HINT_RESIZE_INC;
      geometry->width_inc = size_hints->width_inc;
      geometry->height_inc = size_hints->height_inc;
    }

  if (size_hints->flags & PAspect)
    {
      *geom_mask |= GDK_HINT_ASPECT;

      geometry->min_aspect = (gdouble) size_hints->min_aspect.x / (gdouble) size_hints->min_aspect.y;
      geometry->max_aspect = (gdouble) size_hints->max_aspect.x / (gdouble) size_hints->max_aspect.y;
    }

  if (size_hints->flags & PWinGravity)
    {
      *geom_mask |= GDK_HINT_WIN_GRAVITY;
      geometry->win_gravity = size_hints->win_gravity;
    }

  XFree (size_hints);
}

static gboolean
utf8_is_latin1 (const gchar *str)
{
  const char *p = str;

  while (*p)
    {
      gunichar ch = g_utf8_get_char (p);

      if (ch > 0xff)
	return FALSE;
      
      p = g_utf8_next_char (p);
    }

  return TRUE;
}

/* Set the property to @utf8_str as STRING if the @utf8_str is fully
 * convertable to STRING, otherwise, set it as compound text
 */
static void
set_text_property (GdkDisplay  *display,
		   Window       xwindow,
		   Atom         property,
		   const gchar *utf8_str)
{
  gchar *prop_text = NULL;
  Atom prop_type;
  gint prop_length;
  gint prop_format;
  gboolean is_compound_text;
  
  if (utf8_is_latin1 (utf8_str))
    {
      prop_type = XA_STRING;
      prop_text = gdk_utf8_to_string_target (utf8_str);
      prop_length = prop_text ? strlen (prop_text) : 0;
      prop_format = 8;
      is_compound_text = FALSE;
    }
  else
    {
      GdkAtom gdk_type;
      
      gdk_utf8_to_compound_text_for_display (display,
					     utf8_str, &gdk_type, &prop_format,
					     (guchar **)&prop_text, &prop_length);
      prop_type = gdk_x11_atom_to_xatom_for_display (display, gdk_type);
      is_compound_text = TRUE;
    }

  if (prop_text)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		       xwindow,
		       property,
		       prop_type, prop_format,
		       PropModeReplace, (guchar *)prop_text,
		       prop_length);

      if (is_compound_text)
	gdk_free_compound_text ((guchar *)prop_text);
      else
	g_free (prop_text);
    }
}

/* Set WM_NAME and _NET_WM_NAME
 */
static void
set_wm_name (GdkDisplay  *display,
	     Window       xwindow,
	     const gchar *name)
{
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_NAME"),
		   gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		   PropModeReplace, (guchar *)name, strlen (name));
  
  set_text_property (display, xwindow,
		     gdk_x11_get_xatom_by_name_for_display (display, "WM_NAME"),
		     name);
}

/**
 * gdk_window_set_title:
 * @window: a toplevel #GdkWindow
 * @title: title of @window
 *
 * Sets the title of a toplevel window, to be displayed in the titlebar.
 * If you haven't explicitly set the icon name for the window
 * (using gdk_window_set_icon_name()), the icon name will be set to
 * @title as well. @title must be in UTF-8 encoding (as with all
 * user-readable strings in GDK/GTK+). @title may not be %NULL.
 **/
void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  display = gdk_drawable_get_display (window);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  xwindow = GDK_WINDOW_XID (window);

  set_wm_name (display, xwindow, title);
  
  if (!gdk_window_icon_name_set (window))
    {
      XChangeProperty (xdisplay, xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON_NAME"),
		       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		       PropModeReplace, (guchar *)title, strlen (title));
      
      set_text_property (display, xwindow,
			 gdk_x11_get_xatom_by_name_for_display (display, "WM_ICON_NAME"),
			 title);
    }
}

/**
 * gdk_window_set_role:
 * @window: a toplevel #GdkWindow
 * @role: a string indicating its role
 *
 * When using GTK+, typically you should use gtk_window_set_role() instead
 * of this low-level function.
 * 
 * The window manager and session manager use a window's role to
 * distinguish it from other kinds of window in the same application.
 * When an application is restarted after being saved in a previous
 * session, all windows with the same title and role are treated as
 * interchangeable.  So if you have two windows with the same title
 * that should be distinguished for session management purposes, you
 * should set the role on those windows. It doesn't matter what string
 * you use for the role, as long as you have a different role for each
 * non-interchangeable kind of window.
 * 
 **/
void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  GdkDisplay *display;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  display = gdk_drawable_get_display (window);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (role)
	XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
			 gdk_x11_get_xatom_by_name_for_display (display, "WM_WINDOW_ROLE"),
			 XA_STRING, 8, PropModeReplace, (guchar *)role, strlen (role));
      else
	XDeleteProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
			 gdk_x11_get_xatom_by_name_for_display (display, "WM_WINDOW_ROLE"));
    }
}

/**
 * gdk_window_set_transient_for:
 * @window: a toplevel #GdkWindow
 * @parent: another toplevel #GdkWindow
 *
 * Indicates to the window manager that @window is a transient dialog
 * associated with the application window @parent. This allows the
 * window manager to do things like center @window on @parent and
 * keep @window above @parent.
 *
 * See gtk_window_set_transient_for() if you're using #GtkWindow or
 * #GtkDialog.
 * 
 **/
void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window) && !GDK_WINDOW_DESTROYED (parent))
    XSetTransientForHint (GDK_WINDOW_XDISPLAY (window), 
			  GDK_WINDOW_XID (window),
			  GDK_WINDOW_XID (parent));
}

/**
 * gdk_window_set_background:
 * @window: a #GdkWindow
 * @color: an allocated #GdkColor
 *
 * Sets the background color of @window. (However, when using GTK+,
 * set the background of a widget with gtk_widget_modify_bg() - if
 * you're an application - or gtk_style_set_background() - if you're
 * implementing a custom widget.)
 *
 * The @color must be allocated; gdk_rgb_find_color() is the best way
 * to allocate a color.
 *
 * See also gdk_window_set_back_pixmap().
 * 
 **/
void
gdk_window_set_background (GdkWindow      *window,
			   const GdkColor *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkColormap *colormap = gdk_drawable_get_colormap (window);
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XSetWindowBackground (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), color->pixel);

  private->bg_color = *color;
  gdk_colormap_query_color (colormap, private->bg_color.pixel, &private->bg_color);

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);
  
  private->bg_pixmap = NULL;
}

/**
 * gdk_window_set_back_pixmap:
 * @window: a #GdkWindow
 * @pixmap: a #GdkPixmap, or %NULL
 * @parent_relative: whether the tiling origin is at the origin of @window's parent
 *
 * Sets the background pixmap of @window. May also be used to set a background of
 * "None" on @window, by setting a background pixmap of %NULL.
 * A background pixmap will be tiled, positioning the first tile at the origin of
 * @window, or if @parent_relative is %TRUE, the tiling will be done based on the
 * origin of the parent window (useful to align tiles in a parent with tiles
 * in a child).
 *
 * A background pixmap of %NULL means that the window will have no
 * background.  A window with no background will never have its
 * background filled by the windowing system, instead the window will
 * contain whatever pixels were already in the corresponding area of
 * the display.
 *
 * The windowing system will normally fill a window with its background
 * when the window is obscured then exposed, and when you call
 * gdk_window_clear().
 * 
 **/
void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gboolean   parent_relative)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  Pixmap xpixmap;
  
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
      xpixmap = ParentRelative;
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
    }
  else
    {
      if (pixmap)
	{
	  g_object_ref (pixmap);
	  private->bg_pixmap = pixmap;
	  xpixmap = GDK_PIXMAP_XID (pixmap);
	}
      else
	{
	  xpixmap = None;
	  private->bg_pixmap = GDK_NO_BG;
	}
    }
  
  if (!GDK_WINDOW_DESTROYED (window))
    XSetWindowBackgroundPixmap (GDK_WINDOW_XDISPLAY (window),
				GDK_WINDOW_XID (window), xpixmap);
}

/**
 * gdk_window_set_cursor:
 * @window: a #GdkWindow
 * @cursor: a cursor
 *
 * Sets the mouse pointer for a #GdkWindow. Use gdk_cursor_new() or
 * gdk_cursor_new_from_pixmap() to create the cursor.
 * To make the cursor invisible, use gdk_cursor_new_from_pixmap() to create
 * a cursor with no pixels in it. Passing %NULL for the @cursor argument
 * to gdk_window_set_cursor() means that @window will use the cursor of
 * its parent window. Most windows should use this default.
 * 
 **/
void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkCursorPrivate *cursor_private;
  Cursor xcursor;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
    
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  cursor_private = (GdkCursorPrivate*) cursor;

  if (impl->cursor)
    {
      gdk_cursor_unref (impl->cursor);
      impl->cursor = NULL;
    }

  if (!cursor)
    xcursor = None;
  else
    {
      _gdk_x11_cursor_update_theme (cursor);
      xcursor = cursor_private->xcursor;
    }
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      XDefineCursor (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     xcursor);
      
      if (cursor)
	impl->cursor = gdk_cursor_ref (cursor);
    }
}

GdkCursor *
_gdk_x11_window_get_cursor (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
    
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);

  return impl->cursor;
}

/**
 * gdk_window_get_geometry:
 * @window: a #GdkWindow
 * @x: return location for X coordinate of window (relative to its parent)
 * @y: return location for Y coordinate of window (relative to its parent)
 * @width: return location for width of window
 * @height: return location for height of window
 * @depth: return location for bit depth of window
 *
 * Any of the return location arguments to this function may be %NULL,
 * if you aren't interested in getting the value of that field.
 *
 * The X and Y coordinates returned are relative to the parent window
 * of @window, which for toplevels usually means relative to the
 * window decorations (titlebar, etc.) rather than relative to the
 * root window (screen-size background window).
 *
 * On the X11 platform, the geometry is obtained from the X server,
 * so reflects the latest position of @window; this may be out-of-sync
 * with the position of @window delivered in the most-recently-processed
 * #GdkEventConfigure. gdk_window_get_position() in contrast gets the
 * position from the most recent configure event.
 * 
 * <note>
 * If @window is not a toplevel, it is <emphasis>much</emphasis> better 
 * to call gdk_window_get_position() and gdk_drawable_get_size() instead, 
 * because it avoids the roundtrip to the X server and because 
 * gdk_drawable_get_size() supports the full 32-bit coordinate space,
 * whereas gdk_window_get_geometry() is restricted to the 16-bit
 * coordinates of X11.
 *</note>
 **/
void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  Window root;
  gint tx;
  gint ty;
  guint twidth;
  guint theight;
  guint tborder_width;
  guint tdepth;
  
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    {
      GDK_NOTE (MULTIHEAD,
		g_message ("gdk_window_get_geometry(): Window needs to be non-NULL to be multi head safe"));
      window = gdk_screen_get_root_window ((gdk_screen_get_default ()));
    }

  if (!GDK_WINDOW_DESTROYED (window))
    {
      XGetGeometry (GDK_WINDOW_XDISPLAY (window),
		    GDK_WINDOW_XID (window),
		    &root, &tx, &ty, &twidth, &theight, &tborder_width, &tdepth);
      
      if (x)
	*x = tx;
      if (y)
	*y = ty;
      if (width)
	*width = twidth;
      if (height)
	*height = theight;
      if (depth)
	*depth = tdepth;
    }
}

/**
 * gdk_window_get_origin:
 * @window: a #GdkWindow
 * @x: return location for X coordinate
 * @y: return location for Y coordinate
 * 
 * Obtains the position of a window in root window coordinates.
 * (Compare with gdk_window_get_position() and
 * gdk_window_get_geometry() which return the position of a window
 * relative to its parent window.)
 * 
 * Return value: not meaningful, ignore
 **/
gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  gint return_val;
  Window child;
  gint tx = 0;
  gint ty = 0;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      return_val = XTranslateCoordinates (GDK_WINDOW_XDISPLAY (window),
					  GDK_WINDOW_XID (window),
					  GDK_WINDOW_XROOTWIN (window),
					  0, 0, &tx, &ty,
					  &child);
    }
  else
    return_val = 0;
  
  if (x)
    *x = tx;
  if (y)
    *y = ty;
  
  return return_val;
}

/**
 * gdk_window_get_deskrelative_origin:
 * @window: a toplevel #GdkWindow
 * @x: return location for X coordinate
 * @y: return location for Y coordinate
 * 
 * This gets the origin of a #GdkWindow relative to
 * an Enlightenment-window-manager desktop. As long as you don't
 * assume that the user's desktop/workspace covers the entire
 * root window (i.e. you don't assume that the desktop begins
 * at root window coordinate 0,0) this function is not necessary.
 * It's deprecated for that reason.
 * 
 * Return value: not meaningful
 **/
gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  gboolean return_val = FALSE;
  gint num_children, format_return;
  Window win, *child, parent, root;
  gint tx = 0;
  gint ty = 0;
  Atom type_return;
  Atom atom;
  gulong number_return, bytes_after_return;
  guchar *data_return;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      atom = gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
						    "ENLIGHTENMENT_DESKTOP");
      win = GDK_WINDOW_XID (window);
      
      while (XQueryTree (GDK_WINDOW_XDISPLAY (window), win, &root, &parent,
			 &child, (unsigned int *)&num_children))
	{
	  if ((child) && (num_children > 0))
	    XFree (child);
	  
	  if (!parent)
	    break;
	  else
	    win = parent;
	  
	  if (win == root)
	    break;
	  
	  data_return = NULL;
	  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), win, atom, 0, 0,
			      False, XA_CARDINAL, &type_return, &format_return,
			      &number_return, &bytes_after_return, &data_return);

	  if (type_return == XA_CARDINAL)
	    {
	      XFree (data_return);
              break;
	    }
	}
      
      return_val = XTranslateCoordinates (GDK_WINDOW_XDISPLAY (window),
					  GDK_WINDOW_XID (window),
					  win,
					  0, 0, &tx, &ty,
					  &root);
      if (x)
	*x = tx;
      if (y)
	*y = ty;
    }
  
  
  return return_val;
}

/**
 * gdk_window_get_root_origin:
 * @window: a toplevel #GdkWindow
 * @x: return location for X position of window frame
 * @y: return location for Y position of window frame
 *
 * Obtains the top-left corner of the window manager frame in root
 * window coordinates.
 * 
 **/
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
}

/**
 * gdk_window_get_frame_extents:
 * @window: a toplevel #GdkWindow
 * @rect: rectangle to fill with bounding box of the window frame
 *
 * Obtains the bounding box of the window, including window manager
 * titlebar/borders if any. The frame position is given in root window
 * coordinates. To get the position of the window itself (rather than
 * the frame) in root window coordinates, use gdk_window_get_origin().
 * 
 **/
void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkDisplay *display;
  GdkWindowObject *private;
  Window xwindow;
  Window xparent;
  Window root;
  Window *children;
  guchar *data;
  Window *vroots;
  Atom type_return;
  guint nchildren;
  guint nvroots;
  gulong nitems_return;
  gulong bytes_after_return;
  gint format_return;
  gint i;
  guint ww, wh, wb, wd;
  gint wx, wy;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rect != NULL);
  
  private = (GdkWindowObject*) window;
  
  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  while (private->parent && ((GdkWindowObject*) private->parent)->parent)
    private = (GdkWindowObject*) private->parent;

  /* Refine our fallback answer a bit using local information */
  rect->x = private->x;
  rect->y = private->y;
  gdk_drawable_get_size ((GdkDrawable *)private, &rect->width, &rect->height);

  if (GDK_WINDOW_DESTROYED (private))
    return;

  gdk_error_trap_push();
  
  /* use NETWM_VIRTUAL_ROOTS if available */
  display = gdk_drawable_get_display (window);
  root = GDK_WINDOW_XROOTWIN (window);

  nvroots = 0;
  vroots = NULL;

  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), root,
			  gdk_x11_get_xatom_by_name_for_display (display, 
								 "_NET_VIRTUAL_ROOTS"),
			  0, G_MAXLONG, False, XA_WINDOW, &type_return,
			  &format_return, &nitems_return, &bytes_after_return,
			  &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) && (data))
	{
	  nvroots = nitems_return;
	  vroots = (Window *)data;
	}
    }

  xparent = GDK_WINDOW_XID (window);

  do
    {
      xwindow = xparent;

      if (!XQueryTree (GDK_DISPLAY_XDISPLAY (display), xwindow,
		       &root, &xparent,
		       &children, &nchildren))
	goto fail;
      
      if (children)
	XFree (children);

      /* check virtual roots */
      for (i = 0; i < nvroots; i++)
	{
	  if (xparent == vroots[i])
	    {
	      root = xparent;
	      break;
           }
	}
    }
  while (xparent != root);
  
  if (XGetGeometry (GDK_DISPLAY_XDISPLAY (display), xwindow, 
		    &root, &wx, &wy, &ww, &wh, &wb, &wd))
    {
      rect->x = wx;
      rect->y = wy;
      rect->width = ww;
      rect->height = wh;
    }

 fail:
  if (vroots)
    XFree (vroots);

  gdk_error_trap_pop ();
}

void
_gdk_windowing_get_pointer (GdkDisplay       *display,
			    GdkScreen       **screen,
			    gint             *x,
			    gint             *y,
			    GdkModifierType  *mask)
{
  GdkScreen *default_screen;
  Display *xdisplay;
  Window xwindow;
  Window root = None;
  Window child;
  int rootx, rooty;
  int winx;
  int winy;
  unsigned int xmask;

  if (display->closed)
    return;

  default_screen = gdk_display_get_default_screen (display);

  xdisplay = GDK_SCREEN_XDISPLAY (default_screen);
  xwindow = GDK_SCREEN_XROOTWIN (default_screen);
  
  if (G_LIKELY (GDK_DISPLAY_X11 (display)->trusted_client)) 
    {
      XQueryPointer (xdisplay, xwindow,
		     &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
    } 
  else 
    {
      XSetWindowAttributes attributes;
      Window w;
      
      w = XCreateWindow (xdisplay, xwindow, 0, 0, 1, 1, 0, 
			 CopyFromParent, InputOnly, CopyFromParent, 
			 0, &attributes);
      XQueryPointer (xdisplay, w, 
		     &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
      XDestroyWindow (xdisplay, w);
    }
  
  if (root != None)
    {
      GdkWindow *gdk_root = gdk_window_lookup_for_display (display, root);
      *screen = gdk_drawable_get_screen (gdk_root);
    }
  
  *x = rootx;
  *y = rooty;
  *mask = xmask;
}

GdkWindow*
_gdk_windowing_window_get_pointer (GdkDisplay      *display,
				   GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindow *return_val;
  Window root;
  Window child;
  int rootx, rooty;
  int winx = 0;
  int winy = 0;
  unsigned int xmask = 0;
  gint xoffset, yoffset;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  
  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);

  return_val = NULL;
  if (!GDK_WINDOW_DESTROYED (window)) 
    {
      if (G_LIKELY (GDK_DISPLAY_X11 (display)->trusted_client)) 
	{
	  if (XQueryPointer (GDK_WINDOW_XDISPLAY (window),
			     GDK_WINDOW_XID (window),
			     &root, &child, &rootx, &rooty, &winx, &winy, &xmask))
	    {
	      if (child)
		return_val = gdk_window_lookup_for_display (GDK_WINDOW_DISPLAY (window), child);
	    }
	} 
      else 
	{
	  GdkScreen *screen;
	  int originx, originy;
	  _gdk_windowing_get_pointer (gdk_drawable_get_display (window), &screen, 
				      &rootx, &rooty, &xmask);
	  gdk_window_get_origin (window, &originx, &originy);
	  winx = rootx - originx;
	  winy = rooty - originy;
	}
    }
  
  *x = winx + xoffset;
  *y = winy + yoffset;
  *mask = xmask;
  
  return return_val;
}

/**
 * gdk_display_warp_pointer:
 * @display: a #GdkDisplay
 * @screen: the screen of @display to warp the pointer to
 * @x: the x coordinate of the destination
 * @y: the y coordinate of the destination
 * 
 * Warps the pointer of @display to the point @x,@y on 
 * the screen @screen, unless the pointer is confined
 * to a window by a grab, in which case it will be moved
 * as far as allowed by the grab. Warping the pointer 
 * creates events as if the user had moved the mouse 
 * instantaneously to the destination.
 * 
 * Note that the pointer should normally be under the
 * control of the user. This function was added to cover
 * some rare use cases like keyboard navigation support
 * for the color picker in the #GtkColorSelectionDialog.
 *
 * Since: 2.8
 */ 
void
gdk_display_warp_pointer (GdkDisplay *display,
			  GdkScreen  *screen,
			  gint        x,
			  gint        y)
{
  Display *xdisplay;
  Window dest;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  dest = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));

  XWarpPointer (xdisplay, None, dest, 0, 0, 0, 0, x, y);  
}

GdkWindow*
_gdk_windowing_window_at_pointer (GdkDisplay *display,
                                  gint       *win_x,
				  gint       *win_y)
{
  GdkWindow *window;
  GdkScreen *screen;
  Window root;
  Window xwindow;
  Window child;
  Window xwindow_last = 0;
  Display *xdisplay;
  int rootx = -1, rooty = -1;
  int winx, winy;
  unsigned int xmask;

  screen = gdk_display_get_default_screen (display);
  
  xwindow = GDK_SCREEN_XROOTWIN (screen);
  xdisplay = GDK_SCREEN_XDISPLAY (screen);

  /* This function really only works if the mouse pointer is held still
   * during its operation. If it moves from one leaf window to another
   * than we'll end up with inaccurate values for win_x, win_y
   * and the result.
   */
  gdk_x11_display_grab (display);
  if (G_LIKELY (GDK_DISPLAY_X11 (display)->trusted_client)) 
    {
      XQueryPointer (xdisplay, xwindow,
		     &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
      if (root == xwindow)
	xwindow = child;
      else
	xwindow = root;
      
      while (xwindow)
	{
	  xwindow_last = xwindow;
	  XQueryPointer (xdisplay, xwindow,
			 &root, &xwindow, &rootx, &rooty, &winx, &winy, &xmask);
	}
    } 
  else 
    {
      gint i, screens, width, height;
      GList *toplevels, *list;
      Window pointer_window;
      
      pointer_window = None;
      screens = gdk_display_get_n_screens (display);
      for (i = 0; i < screens; ++i) {
	screen = gdk_display_get_screen (display, i);
	toplevels = gdk_screen_get_toplevel_windows (screen);
	for (list = toplevels; list != NULL; list = g_list_next (list)) {
	  window = GDK_WINDOW (list->data);
	  xwindow = GDK_WINDOW_XWINDOW (window);
	  gdk_error_trap_push ();
	  XQueryPointer (xdisplay, xwindow,
			 &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
	  gdk_flush ();
	  if (gdk_error_trap_pop ())
	    continue;
	  if (child != None) 
	    {
	      pointer_window = child;
	      break;
	    }
	  gdk_window_get_geometry (window, NULL, NULL, &width, &height, NULL);
	  if (winx >= 0 && winy >= 0 && winx < width && winy < height) 
	    {
	      /* A childless toplevel, or below another window? */
	      XSetWindowAttributes attributes;
	      Window w;
	      
	      w = XCreateWindow (xdisplay, xwindow, winx, winy, 1, 1, 0, 
				 CopyFromParent, InputOnly, CopyFromParent, 
				 0, &attributes);
	      XMapWindow (xdisplay, w);
	      XQueryPointer (xdisplay, xwindow, 
			     &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
	      XDestroyWindow (xdisplay, w);
	      if (child == w) 
		{
		  pointer_window = xwindow;
		  break;
		}
	    }
	}
	g_list_free (toplevels);
	if (pointer_window != None)
	  break;
      }
      xwindow = pointer_window;

      while (xwindow)
	{
	  xwindow_last = xwindow;
	  gdk_error_trap_push ();
	  XQueryPointer (xdisplay, xwindow,
			 &root, &xwindow, &rootx, &rooty, &winx, &winy, &xmask);
	  gdk_flush ();
	  if (gdk_error_trap_pop ())
	    break;
	}
    }
  
  gdk_x11_display_ungrab (display);

  window = gdk_window_lookup_for_display (display, xwindow_last);
  *win_x = window ? winx : -1;
  *win_y = window ? winy : -1;

  return window;
}

/**
 * gdk_window_get_events:
 * @window: a #GdkWindow
 * 
 * Gets the event mask for @window. See gdk_window_set_events().
 * 
 * Return value: event mask for @window
 **/
GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  XWindowAttributes attrs;
  GdkEventMask event_mask;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    {
      XGetWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			    GDK_WINDOW_XID (window), 
			    &attrs);
      
      event_mask = x_event_mask_to_gdk_event_mask (attrs.your_event_mask);
      GDK_WINDOW_OBJECT (window)->event_mask = event_mask;
  
      return event_mask;
    }
}

/**
 * gdk_window_set_events:
 * @window: a #GdkWindow
 * @event_mask: event mask for @window
 *
 * The event mask for a window determines which events will be reported
 * for that window. For example, an event mask including #GDK_BUTTON_PRESS_MASK
 * means the window should report button press events. The event mask
 * is the bitwise OR of values from the #GdkEventMask enumeration.
 * 
 **/
void          
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  long xevent_mask;
  int i;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_WINDOW_OBJECT (window)->event_mask = event_mask;
      xevent_mask = StructureNotifyMask | PropertyChangeMask;
      for (i = 0; i < _gdk_nenvent_masks; i++)
	{
	  if (event_mask & (1 << (i + 1)))
	    xevent_mask |= _gdk_event_mask_table[i];
	}
      
      XSelectInput (GDK_WINDOW_XDISPLAY (window),
		    GDK_WINDOW_XID (window),
		    xevent_mask);
    }
}

static void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  GdkWindow *toplevel;
  Window *old_windows;
  Window *new_windows;
  int i, count;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = gdk_window_get_toplevel (window);
  
  old_windows = NULL;
  if (!XGetWMColormapWindows (GDK_WINDOW_XDISPLAY (toplevel),
			      GDK_WINDOW_XID (toplevel),
			      &old_windows, &count))
    {
      count = 0;
    }
  
  for (i = 0; i < count; i++)
    if (old_windows[i] == GDK_WINDOW_XID (window))
      {
	XFree (old_windows);
	return;
      }
  
  new_windows = g_new (Window, count + 1);
  
  for (i = 0; i < count; i++)
    new_windows[i] = old_windows[i];
  new_windows[count] = GDK_WINDOW_XID (window);
  
  XSetWMColormapWindows (GDK_WINDOW_XDISPLAY (toplevel),
			 GDK_WINDOW_XID (toplevel),
			 new_windows, count + 1);
  
  g_free (new_windows);
  if (old_windows)
    XFree (old_windows);
}

#define WARN_SHAPE_TOO_BIG() g_warning ("GdkWindow is too large to allow the use of shape masks or shape regions.")

/*
 * This needs the X11 shape extension.
 * If not available, shaped windows will look
 * ugly, but programs still work.    Stefan Wille
 */
static void
do_shape_combine_mask (GdkWindow *window,
		       GdkBitmap *mask,
		       gint       x, 
		       gint       y,
		       gint       shape)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  Pixmap pixmap;
  gint xoffset, yoffset;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (GDK_WINDOW_DESTROYED (window))
    return;

  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);

  if (xoffset != 0 || yoffset != 0)
    {
      WARN_SHAPE_TOO_BIG ();
      return;
    }
  
  if (shape == ShapeBounding
      ? gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window))
      : gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
    {
      if (mask)
	{
	  pixmap = GDK_PIXMAP_XID (mask);
	  
	  private->shaped = (shape == ShapeBounding);
	}
      else
	{
	  x = 0;
	  y = 0;
	  pixmap = None;

	  private->shaped = FALSE;
	}
      
      XShapeCombineMask (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 shape,
			 x, y,
			 pixmap,
			 ShapeSet);
    }
#endif /* HAVE_SHAPE_EXT */
}

/**
 * gdk_window_shape_combine_mask:
 * @window: a #GdkWindow
 * @mask: shape mask
 * @x: X position of shape mask with respect to @window
 * @y: Y position of shape mask with respect to @window
 *
 * Applies a shape mask to @window. Pixels in @window corresponding to
 * set bits in the @mask will be visible; pixels in @window
 * corresponding to unset bits in the @mask will be transparent. This
 * gives a non-rectangular window.
 *
 * If @mask is %NULL, the shape mask will be unset, and the @x/@y
 * parameters are not used.
 *
 * On the X11 platform, this uses an X server extension which is
 * widely available on most common platforms, but not available on
 * very old X servers, and occasionally the implementation will be
 * buggy. On servers without the shape extension, this function
 * will do nothing.
 *
 * On the Win32 platform the functionality is always present.
 *
 * This function works on both toplevel and child windows.
 * 
 **/
void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint       x, 
			       gint       y)
{
  do_shape_combine_mask (window, mask, x, y, ShapeBounding);
}

/**
 * gdk_window_input_shape_combine_mask:
 * @window: a #GdkWindow
 * @mask: shape mask
 * @x: X position of shape mask with respect to @window
 * @y: Y position of shape mask with respect to @window
 * 
 * Like gdk_window_shape_combine_mask(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the 
 * mask will be passed on the window below @window.
 *
 * An input shape is typically used with RGBA windows.
 * The alpha channel of the window defines which pixels are 
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the window is
 * "clickable".
 *
 * On the X11 platform, this requires version 1.1 of the
 * shape extension.
 *
 * On the Win32 platform, this functionality is not present and the
 * function does nothing.
 *
 * Since: 2.10
 */
void 
gdk_window_input_shape_combine_mask (GdkWindow *window,
				     GdkBitmap *mask,
				     gint       x,
				     gint       y)
{
#ifdef ShapeInput
  do_shape_combine_mask (window, mask, x, y, ShapeInput);
#endif
}


static void
do_shape_combine_region (GdkWindow *window,
			 GdkRegion *shape_region,
			 gint       offset_x,
			 gint       offset_y,
			 gint       shape)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  gint xoffset, yoffset;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (GDK_WINDOW_DESTROYED (window))
    return;

  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);

  if (xoffset != 0 || yoffset != 0)
    {
      WARN_SHAPE_TOO_BIG ();
      return;
    }
  
  if (shape_region == NULL)
    {
      /* Use NULL mask to unset the shape */
      gdk_window_shape_combine_mask (window, NULL, 0, 0);
      return;
    }
  
  if (shape == ShapeBounding
      ? gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window))
      : gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
    {
      gint n_rects = 0;
      XRectangle *xrects = NULL;

      private->shaped = shape == ShapeBounding;

      _gdk_region_get_xrectangles (shape_region,
                                   0, 0,
                                   &xrects, &n_rects);
      
      XShapeCombineRectangles (GDK_WINDOW_XDISPLAY (window),
                               GDK_WINDOW_XID (window),
                               shape,
                               offset_x, offset_y,
                               xrects, n_rects,
                               ShapeSet,
                               YXBanded);

      g_free (xrects);
    }
#endif /* HAVE_SHAPE_EXT */
}

/**
 * gdk_window_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 *
 * Makes pixels in @window outside @shape_region be transparent,
 * so that the window may be nonrectangular. See also
 * gdk_window_shape_combine_mask() to use a bitmap as the mask.
 *
 * If @shape_region is %NULL, the shape will be unset, so the whole
 * window will be opaque again. @offset_x and @offset_y are ignored
 * if @shape_region is %NULL.
 * 
 * On the X11 platform, this uses an X server extension which is
 * widely available on most common platforms, but not available on
 * very old X servers, and occasionally the implementation will be
 * buggy. On servers without the shape extension, this function
 * will do nothing.
 *
 * On the Win32 platform, this functionality is always present.
 *
 * This function works on both toplevel and child windows.
 * 
 **/
void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape_region,
                                 gint       offset_x,
                                 gint       offset_y)
{ 
  do_shape_combine_region (window, shape_region, offset_x, offset_y, ShapeBounding);
}

/**
 * gdk_window_input_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 * 
 * Like gdk_window_shape_combine_region(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the 
 * mask will be passed on the window below @window.
 *
 * An input shape is typically used with RGBA windows.
 * The alpha channel of the window defines which pixels are 
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the window is
 * "clickable".
 *
 * On the X11 platform, this requires version 1.1 of the
 * shape extension.
 *
 * On the Win32 platform, this functionality is not present and the
 * function does nothing.
 *
 * Since: 2.10
 */
void 
gdk_window_input_shape_combine_region (GdkWindow *window,
				       GdkRegion *shape_region,
				       gint       offset_x,
				       gint       offset_y)
{
#ifdef ShapeInput
  do_shape_combine_region (window, shape_region, offset_x, offset_y, ShapeInput);
#endif
}


/**
 * gdk_window_set_override_redirect:
 * @window: a toplevel #GdkWindow
 * @override_redirect: %TRUE if window should be override redirect
 *
 * An override redirect window is not under the control of the window manager.
 * This means it won't have a titlebar, won't be minimizable, etc. - it will
 * be entirely under the control of the application. The window manager
 * can't see the override redirect window at all.
 *
 * Override redirect should only be used for short-lived temporary
 * windows, such as popup menus. #GtkMenu uses an override redirect
 * window in its implementation, for example.
 * 
 **/
void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  XSetWindowAttributes attr;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkWindowObject *private = (GdkWindowObject *)window;
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);

      attr.override_redirect = (override_redirect? True : False);
      XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			       GDK_WINDOW_XID (window),
			       CWOverrideRedirect,
			       &attr);

      impl->override_redirect = attr.override_redirect;
    }
}

/**
 * gdk_window_set_accept_focus:
 * @window: a toplevel #GdkWindow
 * @accept_focus: %TRUE if the window should receive input focus
 *
 * Setting @accept_focus to %FALSE hints the desktop environment that the
 * window doesn't want to receive input focus. 
 *
 * On X, it is the responsibility of the window manager to interpret this 
 * hint. ICCCM-compliant window manager usually respect it.
 *
 * Since: 2.4 
 **/
void
gdk_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;  
  
  accept_focus = accept_focus != FALSE;

  if (private->accept_focus != accept_focus)
    {
      private->accept_focus = accept_focus;

      if (!GDK_WINDOW_DESTROYED (window))
	update_wm_hints (window, FALSE);
    }
}

/**
 * gdk_window_set_focus_on_map:
 * @window: a toplevel #GdkWindow
 * @focus_on_map: %TRUE if the window should receive input focus when mapped
 *
 * Setting @focus_on_map to %FALSE hints the desktop environment that the
 * window doesn't want to receive input focus when it is mapped.  
 * focus_on_map should be turned off for windows that aren't triggered
 * interactively (such as popups from network activity).
 *
 * On X, it is the responsibility of the window manager to interpret
 * this hint. Window managers following the freedesktop.org window
 * manager extension specification should respect it.
 *
 * Since: 2.6 
 **/
void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;  
  
  focus_on_map = focus_on_map != FALSE;

  if (private->focus_on_map != focus_on_map)
    {
      private->focus_on_map = focus_on_map;

      if ((!GDK_WINDOW_DESTROYED (window)) && (!private->focus_on_map))
	gdk_x11_window_set_user_time (window, 0);
    }
}

/**
 * gdk_x11_window_set_user_time:
 * @window: A toplevel #GdkWindow
 * @timestamp: An XServer timestamp to which the property should be set
 *
 * The application can use this call to update the _NET_WM_USER_TIME
 * property on a toplevel window.  This property stores an Xserver
 * time which represents the time of the last user input event
 * received for this window.  This property may be used by the window
 * manager to alter the focus, stacking, and/or placement behavior of
 * windows when they are mapped depending on whether the new window
 * was created by a user action or is a "pop-up" window activated by a
 * timer or some other event.
 *
 * Note that this property is automatically updated by GDK, so this
 * function should only be used by applications which handle input
 * events bypassing GDK.
 *
 * Since: 2.6
 **/
void
gdk_x11_window_set_user_time (GdkWindow *window,
                              guint32    timestamp)
{
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  GdkToplevelX11 *toplevel;
  glong timestamp_long = (glong)timestamp;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);
  display_x11 = GDK_DISPLAY_X11 (display);
  toplevel = _gdk_x11_window_get_toplevel (window);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_USER_TIME"),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *)&timestamp_long, 1);

  if (timestamp_long != GDK_CURRENT_TIME)
    display_x11->user_time = timestamp_long;

  if (toplevel)
    toplevel->user_time = timestamp_long;
}

#define GDK_SELECTION_MAX_SIZE(display)                                 \
  MIN(262144,                                                           \
      XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) == 0     \
       ? XMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100         \
       : XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100)

/**
 * gdk_window_set_icon_list:
 * @window: The #GdkWindow toplevel window to set the icon of.
 * @pixbufs: A list of pixbufs, of different sizes.
 *
 * Sets a list of icons for the window. One of these will be used
 * to represent the window when it has been iconified. The icon is
 * usually shown in an icon box or some sort of task bar. Which icon
 * size is shown depends on the window manager. The window manager
 * can scale the icon  but setting several size icons can give better
 * image quality since the window manager may only need to scale the
 * icon by a small amount or not at all.
 *
 **/
void
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  gulong *data;
  guchar *pixels;
  gulong *p;
  gint size;
  GList *l;
  GdkPixbuf *pixbuf;
  gint width, height, stride;
  gint x, y;
  gint n_channels;
  GdkDisplay *display;
  gint n;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);
  
  l = pixbufs;
  size = 0;
  n = 0;
  while (l)
    {
      pixbuf = l->data;
      g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      
      /* silently ignore overlarge icons */
      if (size + 2 + width * height > GDK_SELECTION_MAX_SIZE(display))
	{
	  g_warning ("gdk_window_set_icon_list: icons too large");
	  break;
	}
     
      n++;
      size += 2 + width * height;
      
      l = g_list_next (l);
    }

  data = g_malloc (size * sizeof (gulong));

  l = pixbufs;
  p = data;
  while (l && n > 0)
    {
      pixbuf = l->data;
      
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      stride = gdk_pixbuf_get_rowstride (pixbuf);
      n_channels = gdk_pixbuf_get_n_channels (pixbuf);
      
      *p++ = width;
      *p++ = height;

      pixels = gdk_pixbuf_get_pixels (pixbuf);

      for (y = 0; y < height; y++)
	{
	  for (x = 0; x < width; x++)
	    {
	      guchar r, g, b, a;
	      
	      r = pixels[y*stride + x*n_channels + 0];
	      g = pixels[y*stride + x*n_channels + 1];
	      b = pixels[y*stride + x*n_channels + 2];
	      if (n_channels >= 4)
		a = pixels[y*stride + x*n_channels + 3];
	      else
		a = 255;
	      
	      *p++ = a << 24 | r << 16 | g << 8 | b ;
	    }
	}

      l = g_list_next (l);
      n--;
    }

  if (size > 0)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON"),
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (guchar*) data, size);
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON"));
    }
  
  g_free (data);
}

/**
 * gdk_window_set_icon:
 * @window: a toplevel #GdkWindow
 * @icon_window: a #GdkWindow to use for the icon, or %NULL to unset
 * @pixmap: a #GdkPixmap to use as the icon, or %NULL to unset
 * @mask: a 1-bit pixmap (#GdkBitmap) to use as mask for @pixmap, or %NULL to have none
 *
 * Sets the icon of @window as a pixmap or window. If using GTK+, investigate
 * gtk_window_set_default_icon_list() first, and then gtk_window_set_icon_list()
 * and gtk_window_set_icon(). If those don't meet your needs, look at
 * gdk_window_set_icon_list(). Only if all those are too high-level do you
 * want to fall back to gdk_window_set_icon().
 * 
 **/
void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  GdkToplevelX11 *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (toplevel->icon_window != icon_window)
    {
      if (toplevel->icon_window)
	g_object_unref (toplevel->icon_window);
      toplevel->icon_window = g_object_ref (icon_window);
    }
  
  if (toplevel->icon_pixmap != pixmap)
    {
      if (pixmap)
	g_object_ref (pixmap);
      if (toplevel->icon_pixmap)
	g_object_unref (toplevel->icon_pixmap);
      toplevel->icon_pixmap = pixmap;
    }
  
  if (toplevel->icon_mask != mask)
    {
      if (mask)
	g_object_ref (mask);
      if (toplevel->icon_mask)
	g_object_unref (toplevel->icon_mask);
      toplevel->icon_mask = mask;
    }
  
  update_wm_hints (window, FALSE);
}

static gboolean
gdk_window_icon_name_set (GdkWindow *window)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (window),
					       g_quark_from_static_string ("gdk-icon-name-set")));
}

/**
 * gdk_window_set_icon_name:
 * @window: a toplevel #GdkWindow
 * @name: name of window while iconified (minimized)
 *
 * Windows may have a name used while minimized, distinct from the
 * name they display in their titlebar. Most of the time this is a bad
 * idea from a user interface standpoint. But you can set such a name
 * with this function, if you like.
 *
 **/
void          
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  GdkDisplay *display;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
		      GUINT_TO_POINTER (TRUE));

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_WINDOW_XID (window),
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON_NAME"),
		   gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		   PropModeReplace, (guchar *)name, strlen (name));
  
  set_text_property (display, GDK_WINDOW_XID (window),
		     gdk_x11_get_xatom_by_name_for_display (display, "WM_ICON_NAME"),
		     name);
}

/**
 * gdk_window_iconify:
 * @window: a toplevel #GdkWindow
 * 
 * Asks to iconify (minimize) @window. The window manager may choose
 * to ignore the request, but normally will honor it. Using
 * gtk_window_iconify() is preferred, if you have a #GtkWindow widget.
 *
 * This function only makes sense when @window is a toplevel window.
 *
 **/
void
gdk_window_iconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      XIconifyWindow (GDK_WINDOW_XDISPLAY (window),
		      GDK_WINDOW_XWINDOW (window),
		      gdk_screen_get_number (GDK_WINDOW_SCREEN (window)));
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_ICONIFIED);
    }
}

/**
 * gdk_window_deiconify:
 * @window: a toplevel #GdkWindow
 *
 * Attempt to deiconify (unminimize) @window. On X11 the window manager may
 * choose to ignore the request to deiconify. When using GTK+,
 * use gtk_window_deiconify() instead of the #GdkWindow variant. Or better yet,
 * you probably want to use gtk_window_present(), which raises the window, focuses it,
 * unminimizes it, and puts it on the current desktop.
 *
 **/
void
gdk_window_deiconify (GdkWindow *window)
{
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

/**
 * gdk_window_stick:
 * @window: a toplevel #GdkWindow
 *
 * "Pins" a window such that it's on all workspaces and does not scroll
 * with viewports, for window managers that have scrollable viewports.
 * (When using #GtkWindow, gtk_window_stick() may be more useful.)
 *
 * On the X11 platform, this function depends on window manager
 * support, so may have no effect with many window managers. However,
 * GDK will do the best it can to convince the window manager to stick
 * the window. For window managers that don't support this operation,
 * there's nothing you can do to force it to happen.
 * 
 **/
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
      
      XClientMessageEvent xclient;

      /* Request stick during viewport scroll */
      gdk_wmspec_change_state (TRUE, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_STICKY"),
			       NULL);

      /* Request desktop 0xFFFFFFFF */
      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_WINDOW_XWINDOW (window);
      xclient.display = GDK_WINDOW_XDISPLAY (window);
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window), 
									"_NET_WM_DESKTOP");
      xclient.format = 32;

      xclient.data.l[0] = 0xFFFFFFFF;
      xclient.data.l[1] = 0;
      xclient.data.l[2] = 0;
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;

      XSendEvent (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XROOTWIN (window), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_STICKY);
    }
}

/**
 * gdk_window_unstick:
 * @window: a toplevel #GdkWindow
 *
 * Reverse operation for gdk_window_stick(); see gdk_window_stick(),
 * and gtk_window_unstick().
 * 
 **/
void
gdk_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      /* Request unstick from viewport */
      gdk_wmspec_change_state (FALSE, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_STICKY"),
			       NULL);

      move_to_current_desktop (window);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_STICKY,
                                   0);

    }
}

/**
 * gdk_window_maximize:
 * @window: a toplevel #GdkWindow
 *
 * Maximizes the window. If the window was already maximized, then
 * this function does nothing.
 * 
 * On X11, asks the window manager to maximize @window, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don't have a concept of
 * "maximized"; so you can't rely on the maximization actually
 * happening. But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * On Windows, reliably maximizes the window.
 * 
 **/
void
gdk_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (TRUE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_VERT"),
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_HORZ"));
  else
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_MAXIMIZED);
}

/**
 * gdk_window_unmaximize:
 * @window: a toplevel #GdkWindow
 *
 * Unmaximizes the window. If the window wasn't maximized, then this
 * function does nothing.
 * 
 * On X11, asks the window manager to unmaximize @window, if the
 * window manager supports this operation. Not all window managers
 * support this, and some deliberately ignore it or don't have a
 * concept of "maximized"; so you can't rely on the unmaximization
 * actually happening. But it will happen with most standard window
 * managers, and GDK makes a best effort to get it to happen.
 *
 * On Windows, reliably unmaximizes the window.
 * 
 **/
void
gdk_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (FALSE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_VERT"),
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_HORZ"));
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

/**
 * gdk_window_fullscreen:
 * @window: a toplevel #GdkWindow
 *
 * Moves the window into fullscreen mode. This means the
 * window covers the entire screen and is above any panels
 * or task bars.
 *
 * If the window was already fullscreen, then this function does nothing.
 * 
 * On X11, asks the window manager to put @window in a fullscreen
 * state, if the window manager supports this operation. Not all
 * window managers support this, and some deliberately ignore it or
 * don't have a concept of "fullscreen"; so you can't rely on the
 * fullscreenification actually happening. But it will happen with
 * most standard window managers, and GDK makes a best effort to get
 * it to happen.
 *
 * Since: 2.2
 **/
void
gdk_window_fullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (TRUE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_FULLSCREEN"),
                             GDK_NONE);

  else
    gdk_synthesize_window_state (window,
                                 0,
                                 GDK_WINDOW_STATE_FULLSCREEN);
}

/**
 * gdk_window_unfullscreen:
 * @window: a toplevel #GdkWindow
 *
 * Moves the window out of fullscreen mode. If the window was not
 * fullscreen, does nothing.
 * 
 * On X11, asks the window manager to move @window out of the fullscreen
 * state, if the window manager supports this operation. Not all
 * window managers support this, and some deliberately ignore it or
 * don't have a concept of "fullscreen"; so you can't rely on the
 * unfullscreenification actually happening. But it will happen with
 * most standard window managers, and GDK makes a best effort to get
 * it to happen. 
 *
 * Since: 2.2
 **/
void
gdk_window_unfullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (FALSE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_FULLSCREEN"),
                             GDK_NONE);

  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_FULLSCREEN,
				 0);
}

/**
 * gdk_window_set_keep_above:
 * @window: a toplevel #GdkWindow
 * @setting: whether to keep @window above other windows
 *
 * Set if @window must be kept above other windows. If the
 * window was already above, then this function does nothing.
 * 
 * On X11, asks the window manager to keep @window above, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don't have a concept of
 * "keep above"; so you can't rely on the window being kept above.
 * But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * Since: 2.4
 **/
void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      if (setting)
	gdk_wmspec_change_state (FALSE, window,
				 gdk_atom_intern_static_string ("_NET_WM_STATE_BELOW"),
				 GDK_NONE);
      gdk_wmspec_change_state (setting, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_ABOVE"),
			       GDK_NONE);
    }
  else
    gdk_synthesize_window_state (window,
    				 setting ? GDK_WINDOW_STATE_BELOW : GDK_WINDOW_STATE_ABOVE,
				 setting ? GDK_WINDOW_STATE_ABOVE : 0);
}

/**
 * gdk_window_set_keep_below:
 * @window: a toplevel #GdkWindow
 * @setting: whether to keep @window below other windows
 *
 * Set if @window must be kept below other windows. If the
 * window was already below, then this function does nothing.
 * 
 * On X11, asks the window manager to keep @window below, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don't have a concept of
 * "keep below"; so you can't rely on the window being kept below.
 * But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * Since: 2.4
 **/
void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      if (setting)
	gdk_wmspec_change_state (FALSE, window,
				 gdk_atom_intern_static_string ("_NET_WM_STATE_ABOVE"),
				 GDK_NONE);
      gdk_wmspec_change_state (setting, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_BELOW"),
			       GDK_NONE);
    }
  else
    gdk_synthesize_window_state (window,
				 setting ? GDK_WINDOW_STATE_ABOVE : GDK_WINDOW_STATE_BELOW,
				 setting ? GDK_WINDOW_STATE_BELOW : 0);
}

/**
 * gdk_window_get_group:
 * @window: a toplevel #GdkWindow
 * 
 * Returns the group leader window for @window. See gdk_window_set_group().
 * 
 * Return value: the group leader window for @window
 *
 * Since: 2.4
 **/
GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  GdkToplevelX11 *toplevel;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD, NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;
  
  toplevel = _gdk_x11_window_get_toplevel (window);

  return toplevel->group_leader;
}

/**
 * gdk_window_set_group:
 * @window: a toplevel #GdkWindow
 * @leader: group leader window, or %NULL to restore the default group leader window
 *
 * Sets the group leader window for @window. By default,
 * GDK sets the group leader for all toplevel windows
 * to a global window implicitly created by GDK. With this function
 * you can override this default.
 *
 * The group leader window allows the window manager to distinguish
 * all windows that belong to a single application. It may for example
 * allow users to minimize/unminimize all windows belonging to an
 * application at once. You should only set a non-default group window
 * if your application pretends to be multiple applications.
 **/
void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  g_return_if_fail (leader == NULL || GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) || (leader != NULL && GDK_WINDOW_DESTROYED (leader)))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (leader == NULL) 
    leader = gdk_display_get_default_group (gdk_drawable_get_display (window));
  
  if (toplevel->group_leader != leader)
    {
      if (toplevel->group_leader)
	g_object_unref (toplevel->group_leader);
      toplevel->group_leader = g_object_ref (leader);
      (_gdk_x11_window_get_toplevel (leader))->is_leader = TRUE;      
    }

  update_wm_hints (window, FALSE);
}

static MotifWmHints *
gdk_window_get_mwm_hints (GdkWindow *window)
{
  GdkDisplay *display;
  Atom hints_atom = None;
  guchar *data;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  display = gdk_drawable_get_display (window);
  
  hints_atom = gdk_x11_get_xatom_by_name_for_display (display, _XA_MOTIF_WM_HINTS);

  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, &data);

  if (type == None)
    return NULL;
  
  return (MotifWmHints *)data;
}

static void
gdk_window_set_mwm_hints (GdkWindow *window,
			  MotifWmHints *new_hints)
{
  GdkDisplay *display;
  Atom hints_atom = None;
  guchar *data;
  MotifWmHints *hints;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  display = gdk_drawable_get_display (window);
  
  hints_atom = gdk_x11_get_xatom_by_name_for_display (display, _XA_MOTIF_WM_HINTS);

  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, &data);
  
  if (type == None)
    hints = new_hints;
  else
    {
      hints = (MotifWmHints *)data;
	
      if (new_hints->flags & MWM_HINTS_FUNCTIONS)
	{
	  hints->flags |= MWM_HINTS_FUNCTIONS;
	  hints->functions = new_hints->functions;
	}
      if (new_hints->flags & MWM_HINTS_DECORATIONS)
	{
	  hints->flags |= MWM_HINTS_DECORATIONS;
	  hints->decorations = new_hints->decorations;
	}
    }
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		   hints_atom, hints_atom, 32, PropModeReplace,
		   (guchar *)hints, sizeof (MotifWmHints)/sizeof (long));
  
  if (hints != new_hints)
    XFree (hints);
}

/**
 * gdk_window_set_decorations:
 * @window: a toplevel #GdkWindow
 * @decorations: decoration hint mask
 *
 * "Decorations" are the features the window manager adds to a toplevel #GdkWindow.
 * This function sets the traditional Motif window manager hints that tell the
 * window manager which decorations you would like your window to have.
 * Usually you should use gtk_window_set_decorated() on a #GtkWindow instead of
 * using the GDK function directly.
 *
 * The @decorations argument is the logical OR of the fields in
 * the #GdkWMDecoration enumeration. If #GDK_DECOR_ALL is included in the
 * mask, the other bits indicate which decorations should be turned off.
 * If #GDK_DECOR_ALL is not included, then the other bits indicate
 * which decorations should be turned on.
 *
 * Most window managers honor a decorations hint of 0 to disable all decorations,
 * but very few honor all possible combinations of bits.
 * 
 **/
void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  MotifWmHints hints;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&hints, 0, sizeof(hints));
  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = decorations;
  
  gdk_window_set_mwm_hints (window, &hints);
}

/**
 * gdk_window_get_decorations:
 * @window: The toplevel #GdkWindow to get the decorations from
 * @decorations: The window decorations will be written here
 *
 * Returns the decorations set on the GdkWindow with #gdk_window_set_decorations
 * Returns: TRUE if the window has decorations set, FALSE otherwise.
 **/
gboolean
gdk_window_get_decorations(GdkWindow       *window,
			   GdkWMDecoration *decorations)
{
  MotifWmHints *hints;
  gboolean result = FALSE;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  hints = gdk_window_get_mwm_hints (window);
  
  if (hints)
    {
      if (hints->flags & MWM_HINTS_DECORATIONS)
	{
	  if (decorations)
	    *decorations = hints->decorations;
	  result = TRUE;
	}
      
      XFree (hints);
    }

  return result;
}

/**
 * gdk_window_set_functions:
 * @window: a toplevel #GdkWindow
 * @functions: bitmask of operations to allow on @window
 *
 * Sets hints about the window management functions to make available
 * via buttons on the window frame.
 * 
 * On the X backend, this function sets the traditional Motif window 
 * manager hint for this purpose. However, few window managers do
 * anything reliable or interesting with this hint. Many ignore it
 * entirely.
 *
 * The @functions argument is the logical OR of values from the
 * #GdkWMFunction enumeration. If the bitmask includes #GDK_FUNC_ALL,
 * then the other bits indicate which functions to disable; if
 * it doesn't include #GDK_FUNC_ALL, it indicates which functions to
 * enable.
 * 
 **/
void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  MotifWmHints hints;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&hints, 0, sizeof(hints));
  hints.flags = MWM_HINTS_FUNCTIONS;
  hints.functions = functions;
  
  gdk_window_set_mwm_hints (window, &hints);
}

#ifdef HAVE_SHAPE_EXT

/* 
 * propagate the shapes from all child windows of a GDK window to the parent 
 * window. Shamelessly ripped from Enlightenment's code
 * 
 * - Raster
 */
struct _gdk_span
{
  gint                start;
  gint                end;
  struct _gdk_span    *next;
};

static void
gdk_add_to_span (struct _gdk_span **s,
		 gint               x,
		 gint               xx)
{
  struct _gdk_span *ptr1, *ptr2, *noo, *ss;
  gchar             spanning;
  
  ptr2 = NULL;
  ptr1 = *s;
  spanning = 0;
  ss = NULL;
  /* scan the spans for this line */
  while (ptr1)
    {
      /* -- -> new span */
      /* == -> existing span */
      /* ## -> spans intersect */
      /* if we are in the middle of spanning the span into the line */
      if (spanning)
	{
	  /* case: ---- ==== */
	  if (xx < ptr1->start - 1)
	    {
	      /* ends before next span - extend to here */
	      ss->end = xx;
	      return;
	    }
	  /* case: ----##=== */
	  else if (xx <= ptr1->end)
	    {
	      /* crosses into next span - delete next span and append */
	      ss->end = ptr1->end;
	      ss->next = ptr1->next;
	      g_free (ptr1);
	      return;
	    }
	  /* case: ---###--- */
	  else
	    {
	      /* overlaps next span - delete and keep checking */
	      ss->next = ptr1->next;
	      g_free (ptr1);
	      ptr1 = ss;
	    }
	}
      /* otherwise havent started spanning it in yet */
      else
	{
	  /* case: ---- ==== */
	  if (xx < ptr1->start - 1)
	    {
	      /* insert span here in list */
	      noo = g_malloc (sizeof (struct _gdk_span));
	      
	      if (noo)
		{
		  noo->start = x;
		  noo->end = xx;
		  noo->next = ptr1;
		  if (ptr2)
		    ptr2->next = noo;
		  else
		    *s = noo;
		}
	      return;
	    }
	  /* case: ----##=== */
	  else if ((x < ptr1->start) && (xx <= ptr1->end))
	    {
	      /* expand this span to the left point of the new one */
	      ptr1->start = x;
	      return;
	    }
	  /* case: ===###=== */
	  else if ((x >= ptr1->start) && (xx <= ptr1->end))
	    {
	      /* throw the span away */
	      return;
	    }
	  /* case: ---###--- */
	  else if ((x < ptr1->start) && (xx > ptr1->end))
	    {
	      ss = ptr1;
	      spanning = 1;
	      ptr1->start = x;
	      ptr1->end = xx;
	    }
	  /* case: ===##---- */
	  else if ((x >= ptr1->start) && (x <= ptr1->end + 1) && (xx > ptr1->end))
	    {
	      ss = ptr1;
	      spanning = 1;
	      ptr1->end = xx;
	    }
	  /* case: ==== ---- */
	  /* case handled by next loop iteration - first case */
	}
      ptr2 = ptr1;
      ptr1 = ptr1->next;
    }
  /* it started in the middle but spans beyond your current list */
  if (spanning)
    {
      ptr2->end = xx;
      return;
    }
  /* it does not start inside a span or in the middle, so add it to the end */
  noo = g_malloc (sizeof (struct _gdk_span));
  
  if (noo)
    {
      noo->start = x;
      noo->end = xx;
      if (ptr2)
	{
	  noo->next = ptr2->next;
	  ptr2->next = noo;
	}
      else
	{
	  noo->next = NULL;
	  *s = noo;
	}
    }
  return;
}

static void
gdk_add_rectangles (Display           *disp,
		    Window             win,
		    struct _gdk_span **spans,
		    gint               basew,
		    gint               baseh,
		    gint               x,
		    gint               y)
{
  gint a, k;
  gint x1, y1, x2, y2;
  gint rn, ord;
  XRectangle *rl;
  
  rl = XShapeGetRectangles (disp, win, ShapeBounding, &rn, &ord);
  if (rl)
    {
      /* go through all clip rects in this window's shape */
      for (k = 0; k < rn; k++)
	{
	  /* for each clip rect, add it to each line's spans */
	  x1 = x + rl[k].x;
	  x2 = x + rl[k].x + (rl[k].width - 1);
	  y1 = y + rl[k].y;
	  y2 = y + rl[k].y + (rl[k].height - 1);
	  if (x1 < 0)
	    x1 = 0;
	  if (y1 < 0)
	    y1 = 0;
	  if (x2 >= basew)
	    x2 = basew - 1;
	  if (y2 >= baseh)
	    y2 = baseh - 1;
	  for (a = y1; a <= y2; a++)
	    {
	      if ((x2 - x1) >= 0)
		gdk_add_to_span (&spans[a], x1, x2);
	    }
	}
      XFree (rl);
    }
}

static void
gdk_propagate_shapes (Display *disp,
		      Window   win,
		      gboolean merge,
		      int      shape)
{
  Window              rt, par, *list = NULL;
  gint                i, j, num = 0, num_rects = 0;
  gint                x, y, contig;
  guint               w, h, d;
  gint                baseh, basew;
  XRectangle         *rects = NULL;
  struct _gdk_span  **spans = NULL, *ptr1, *ptr2, *ptr3;
  XWindowAttributes   xatt;
  
  XGetGeometry (disp, win, &rt, &x, &y, &w, &h, &d, &d);
  if (h <= 0)
    return;
  basew = w;
  baseh = h;
  spans = g_malloc (sizeof (struct _gdk_span *) * h);
  
  for (i = 0; i < h; i++)
    spans[i] = NULL;
  XQueryTree (disp, win, &rt, &par, &list, (unsigned int *)&num);
  if (list)
    {
      /* go through all child windows and create/insert spans */
      for (i = 0; i < num; i++)
	{
	  if (XGetWindowAttributes (disp, list[i], &xatt) && (xatt.map_state != IsUnmapped))
	    if (XGetGeometry (disp, list[i], &rt, &x, &y, &w, &h, &d, &d))
	      gdk_add_rectangles (disp, list[i], spans, basew, baseh, x, y);
	}
      if (merge)
	gdk_add_rectangles (disp, win, spans, basew, baseh, x, y);
      
      /* go through the spans list and build a list of rects */
      rects = g_malloc (sizeof (XRectangle) * 256);
      num_rects = 0;
      for (i = 0; i < baseh; i++)
	{
	  ptr1 = spans[i];
	  /* go through the line for all spans */
	  while (ptr1)
	    {
	      rects[num_rects].x = ptr1->start;
	      rects[num_rects].y = i;
	      rects[num_rects].width = ptr1->end - ptr1->start + 1;
	      rects[num_rects].height = 1;
	      j = i + 1;
	      /* if there are more lines */
	      contig = 1;
	      /* while contigous rects (same start/end coords) exist */
	      while ((contig) && (j < baseh))
		{
		  /* search next line for spans matching this one */
		  contig = 0;
		  ptr2 = spans[j];
		  ptr3 = NULL;
		  while (ptr2)
		    {
		      /* if we have an exact span match set contig */
		      if ((ptr2->start == ptr1->start) &&
			  (ptr2->end == ptr1->end))
			{
			  contig = 1;
			  /* remove the span - not needed */
			  if (ptr3)
			    {
			      ptr3->next = ptr2->next;
			      g_free (ptr2);
			      ptr2 = NULL;
			    }
			  else
			    {
			      spans[j] = ptr2->next;
			      g_free (ptr2);
			      ptr2 = NULL;
			    }
			  break;
			}
		      /* gone past the span point no point looking */
		      else if (ptr2->start < ptr1->start)
			break;
		      if (ptr2)
			{
			  ptr3 = ptr2;
			  ptr2 = ptr2->next;
			}
		    }
		  /* if a contiguous span was found increase the rect h */
		  if (contig)
		    {
		      rects[num_rects].height++;
		      j++;
		    }
		}
	      /* up the rect count */
	      num_rects++;
	      /* every 256 new rects increase the rect array */
	      if ((num_rects % 256) == 0)
		rects = g_realloc (rects, sizeof (XRectangle) * (num_rects + 256));
	      ptr1 = ptr1->next;
	    }
	}
      /* set the rects as the shape mask */
      if (rects)
	{
	  XShapeCombineRectangles (disp, win, shape, 0, 0, rects, num_rects,
				   ShapeSet, YXSorted);
	  g_free (rects);
	}
      XFree (list);
    }
  /* free up all the spans we made */
  for (i = 0; i < baseh; i++)
    {
      ptr1 = spans[i];
      while (ptr1)
	{
	  ptr2 = ptr1;
	  ptr1 = ptr1->next;
	  g_free (ptr2);
	}
    }
  g_free (spans);
}

#endif /* HAVE_SHAPE_EXT */

/**
 * gdk_window_set_child_shapes:
 * @window: a #GdkWindow
 * 
 * Sets the shape mask of @window to the union of shape masks
 * for all children of @window, ignoring the shape mask of @window
 * itself. Contrast with gdk_window_merge_child_shapes() which includes
 * the shape mask of @window in the masks to be merged.
 **/
void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  
#ifdef HAVE_SHAPE_EXT
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window)))
    gdk_propagate_shapes (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), FALSE, ShapeBounding);
#endif   
}

/**
 * gdk_window_merge_child_shapes:
 * @window: a #GdkWindow
 * 
 * Merges the shape masks for any child windows into the
 * shape mask for @window. i.e. the union of all masks
 * for @window and its children will become the new mask
 * for @window. See gdk_window_shape_combine_mask().
 *
 * This function is distinct from gdk_window_set_child_shapes()
 * because it includes @window's shape mask in the set of shapes to
 * be merged.
 **/
void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window)))
    gdk_propagate_shapes (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), TRUE, ShapeBounding);
#endif   
}

/**
 * gdk_window_set_child_input_shapes:
 * @window: a #GdkWindow
 * 
 * Sets the input shape mask of @window to the union of input shape masks
 * for all children of @window, ignoring the input shape mask of @window
 * itself. Contrast with gdk_window_merge_child_input_shapes() which includes
 * the input shape mask of @window in the masks to be merged.
 *
 * Since: 2.10
 **/
void 
gdk_window_set_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
#ifdef ShapeInput
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
    gdk_propagate_shapes (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), FALSE, ShapeInput);
#endif
#endif   
}

/**
 * gdk_window_merge_child_input_shapes:
 * @window: a #GdkWindow
 * 
 * Merges the input shape masks for any child windows into the
 * input shape mask for @window. i.e. the union of all input masks
 * for @window and its children will become the new input mask
 * for @window. See gdk_window_input_shape_combine_mask().
 *
 * This function is distinct from gdk_window_set_child_input_shapes()
 * because it includes @window's input shape mask in the set of 
 * shapes to be merged.
 *
 * Since: 2.10
 **/
void 
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
#ifdef ShapeInput
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
    gdk_propagate_shapes (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), TRUE, ShapeInput);
#endif
#endif   
}


static void
gdk_window_set_static_bit_gravity (GdkWindow *window, gboolean on)
{
  XSetWindowAttributes xattributes;
  GdkWindowObject *private;
  guint xattributes_mask = 0;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  if (private->input_only)
    return;
  
  xattributes.bit_gravity = StaticGravity;
  xattributes_mask |= CWBitGravity;
  xattributes.bit_gravity = on ? StaticGravity : ForgetGravity;
  XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			   GDK_WINDOW_XID (window),
			   CWBitGravity,  &xattributes);
}

static void
gdk_window_set_static_win_gravity (GdkWindow *window, gboolean on)
{
  XSetWindowAttributes xattributes;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  xattributes.win_gravity = on ? StaticGravity : NorthWestGravity;
  
  XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			   GDK_WINDOW_XID (window),
			   CWWinGravity,  &xattributes);
}

/**
 * gdk_window_set_static_gravities:
 * @window: a #GdkWindow
 * @use_static: %TRUE to turn on static gravity
 *
 * Set the bit gravity of the given window to static, and flag it so
 * all children get static subwindow gravity. This is used if you are
 * implementing scary features that involve deep knowledge of the
 * windowing system. Don't worry about it unless you have to.
 * 
 * Return value: %TRUE if the server supports static gravity
 **/
gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GList *tmp_list;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!use_static == !private->guffaw_gravity)
    return TRUE;

  private->guffaw_gravity = use_static;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      gdk_window_set_static_bit_gravity (window, use_static);
      
      tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_set_static_win_gravity (tmp_list->data, use_static);
	  
	  tmp_list = tmp_list->next;
	}
    }
  
  return TRUE;
}

static void
wmspec_moveresize (GdkWindow *window,
                   gint       direction,
                   gint       root_x,
                   gint       root_y,
                   guint32    timestamp)     
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  
  XClientMessageEvent xclient;

  /* Release passive grab */
  gdk_display_pointer_ungrab (display, timestamp);

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type =
    gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_MOVERESIZE");
  xclient.format = 32;
  xclient.data.l[0] = root_x;
  xclient.data.l[1] = root_y;
  xclient.data.l[2] = direction;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;
  
  XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XROOTWIN (window), False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      (XEvent *)&xclient);
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;
  
  GdkWindow *moveresize_window;
  GdkWindow *moveresize_emulation_window;
  gboolean is_resize;
  GdkWindowEdge resize_edge;
  gint moveresize_button;
  gint moveresize_x;
  gint moveresize_y;
  gint moveresize_orig_x;
  gint moveresize_orig_y;
  gint moveresize_orig_width;
  gint moveresize_orig_height;
  GdkWindowHints moveresize_geom_mask;
  GdkGeometry moveresize_geometry;
  Time moveresize_process_time;
  XEvent *moveresize_pending_event;
};

/* From the WM spec */
#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8

static void
wmspec_resize_drag (GdkWindow     *window,
                    GdkWindowEdge  edge,
                    gint           button,
                    gint           root_x,
                    gint           root_y,
                    guint32        timestamp)
{
  gint direction;
  
  /* Let the compiler turn a switch into a table, instead
   * of doing the table manually, this way is easier to verify.
   */
  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
      break;

    case GDK_WINDOW_EDGE_NORTH:
      direction = _NET_WM_MOVERESIZE_SIZE_TOP;
      break;

    case GDK_WINDOW_EDGE_NORTH_EAST:
      direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
      break;

    case GDK_WINDOW_EDGE_WEST:
      direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
      break;

    case GDK_WINDOW_EDGE_EAST:
      direction = _NET_WM_MOVERESIZE_SIZE_RIGHT;
      break;

    case GDK_WINDOW_EDGE_SOUTH_WEST:
      direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
      break;

    case GDK_WINDOW_EDGE_SOUTH:
      direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
      break;

    case GDK_WINDOW_EDGE_SOUTH_EAST:
      direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
      break;

    default:
      g_warning ("gdk_window_begin_resize_drag: bad resize edge %d!",
                 edge);
      return;
      break;
    }
  
  wmspec_moveresize (window, direction, root_x, root_y, timestamp);
}

static MoveResizeData *
get_move_resize_data (GdkDisplay *display,
		      gboolean    create)
{
  MoveResizeData *mv_resize;
  static GQuark move_resize_quark = 0;

  if (!move_resize_quark)
    move_resize_quark = g_quark_from_static_string ("gdk-window-moveresize");
  
  mv_resize = g_object_get_qdata (G_OBJECT (display), move_resize_quark);

  if (!mv_resize && create)
    {
      mv_resize = g_new0 (MoveResizeData, 1);
      mv_resize->display = display;
      
      g_object_set_qdata (G_OBJECT (display), move_resize_quark, mv_resize);
    }

  return mv_resize;
}

static void
update_pos (MoveResizeData *mv_resize,
	    gint            new_root_x,
	    gint            new_root_y)
{
  gint dx, dy;

  dx = new_root_x - mv_resize->moveresize_x;
  dy = new_root_y - mv_resize->moveresize_y;

  if (mv_resize->is_resize)
    {
      gint x, y, w, h;

      x = mv_resize->moveresize_orig_x;
      y = mv_resize->moveresize_orig_y;

      w = mv_resize->moveresize_orig_width;
      h = mv_resize->moveresize_orig_height;

      switch (mv_resize->resize_edge)
	{
	case GDK_WINDOW_EDGE_NORTH_WEST:
	  x += dx;
	  y += dy;
	  w -= dx;
	  h -= dy;
	  break;
	case GDK_WINDOW_EDGE_NORTH:
	  y += dy;
	  h -= dy;
	  break;
	case GDK_WINDOW_EDGE_NORTH_EAST:
	  y += dy;
	  h -= dy;
	  w += dx;
	  break;
	case GDK_WINDOW_EDGE_SOUTH_WEST:
	  h += dy;
	  x += dx;
	  w -= dx;
	  break;
	case GDK_WINDOW_EDGE_SOUTH_EAST:
	  w += dx;
	  h += dy;
	  break;
	case GDK_WINDOW_EDGE_SOUTH:
	  h += dy;
	  break;
	case GDK_WINDOW_EDGE_EAST:
	  w += dx;
	  break;
	case GDK_WINDOW_EDGE_WEST:
	  x += dx;
	  w -= dx;
	  break;
	}

      x = MAX (x, 0);
      y = MAX (y, 0);
      w = MAX (w, 1);
      h = MAX (h, 1);

      if (mv_resize->moveresize_geom_mask)
	{
	  gdk_window_constrain_size (&mv_resize->moveresize_geometry,
				     mv_resize->moveresize_geom_mask,
				     w, h, &w, &h);
	}

      gdk_window_move_resize (mv_resize->moveresize_window, x, y, w, h);
    }
  else
    {
      gint x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_window_move (mv_resize->moveresize_window, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_window_destroy (mv_resize->moveresize_emulation_window);
  mv_resize->moveresize_emulation_window = NULL;
  g_object_unref (mv_resize->moveresize_window);
  mv_resize->moveresize_window = NULL;

  if (mv_resize->moveresize_pending_event)
    {
      g_free (mv_resize->moveresize_pending_event);
      mv_resize->moveresize_pending_event = NULL;
    }
}

static int
lookahead_motion_predicate (Display *xdisplay,
			    XEvent  *event,
			    XPointer arg)
{
  gboolean *seen_release = (gboolean *)arg;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (*seen_release)
    return False;

  switch (event->xany.type)
    {
    case ButtonRelease:
      *seen_release = TRUE;
      break;
    case MotionNotify:
      mv_resize->moveresize_process_time = event->xmotion.time;
      break;
    default:
      break;
    }

  return False;
}

static gboolean
moveresize_lookahead (MoveResizeData *mv_resize,
		      XEvent         *event)
{
  XEvent tmp_event;
  gboolean seen_release = FALSE;

  if (mv_resize->moveresize_process_time)
    {
      if (event->xmotion.time == mv_resize->moveresize_process_time)
	{
	  mv_resize->moveresize_process_time = 0;
	  return TRUE;
	}
      else
	return FALSE;
    }

  XCheckIfEvent (event->xany.display, &tmp_event,
		 lookahead_motion_predicate, (XPointer) & seen_release);

  return mv_resize->moveresize_process_time == 0;
}
	
gboolean
_gdk_moveresize_handle_event (XEvent *event)
{
  guint button_mask = 0;
  GdkWindowObject *window_private;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (event->xany.display);
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || !mv_resize->moveresize_window)
    return FALSE;

  window_private = (GdkWindowObject *) mv_resize->moveresize_window;

  button_mask = GDK_BUTTON1_MASK << (mv_resize->moveresize_button - 1);

  switch (event->xany.type)
    {
    case MotionNotify:
      if (window_private->resize_count > 0)
	{
	  if (mv_resize->moveresize_pending_event)
	    *mv_resize->moveresize_pending_event = *event;
	  else
	    mv_resize->moveresize_pending_event =
	      g_memdup (event, sizeof (XEvent));

	  break;
	}
      if (!moveresize_lookahead (mv_resize, event))
	break;

      update_pos (mv_resize,
		  event->xmotion.x_root,
		  event->xmotion.y_root);

      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->xmotion.state & button_mask) == 0)
	finish_drag (mv_resize);
      break;

    case ButtonRelease:
      update_pos (mv_resize,
		  event->xbutton.x_root,
		  event->xbutton.y_root);

      if (event->xbutton.button == mv_resize->moveresize_button)
	finish_drag (mv_resize);
      break;
    }
  return TRUE;
}

gboolean 
_gdk_moveresize_configure_done (GdkDisplay *display,
				GdkWindow  *window)
{
  XEvent *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);
  
  if (!mv_resize || window != mv_resize->moveresize_window)
    return FALSE;

  if (mv_resize->moveresize_pending_event)
    {
      tmp_event = mv_resize->moveresize_pending_event;
      mv_resize->moveresize_pending_event = NULL;
      _gdk_moveresize_handle_event (tmp_event);
      g_free (tmp_event);
    }
  
  return TRUE;
}

static void
create_moveresize_window (MoveResizeData *mv_resize,
			  guint32         timestamp)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGrabStatus status;

  g_assert (mv_resize->moveresize_emulation_window == NULL);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  mv_resize->moveresize_emulation_window = 
    gdk_window_new (gdk_screen_get_root_window (gdk_display_get_default_screen (mv_resize->display)),
		    &attributes,
		    attributes_mask);

  gdk_window_show (mv_resize->moveresize_emulation_window);

  status = gdk_pointer_grab (mv_resize->moveresize_emulation_window,
                             FALSE,
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK,
                             NULL,
                             NULL,
                             timestamp);

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the window
       * already.
       */
      finish_drag (mv_resize);
    }

  mv_resize->moveresize_process_time = 0;
}

/* 
   Calculate mv_resize->moveresize_orig_x and mv_resize->moveresize_orig_y
   so that calling XMoveWindow with these coordinates will not move the 
   window.
   Note that this depends on the WM to implement ICCCM-compliant reference
   point handling.
*/
static void 
calculate_unmoving_origin (MoveResizeData *mv_resize)
{
  GdkRectangle rect;
  gint width, height;

  if (mv_resize->moveresize_geom_mask & GDK_HINT_WIN_GRAVITY &&
      mv_resize->moveresize_geometry.win_gravity == GDK_GRAVITY_STATIC)
    {
      gdk_window_get_origin (mv_resize->moveresize_window,
			     &mv_resize->moveresize_orig_x,
			     &mv_resize->moveresize_orig_y);
    }
  else
    {
      gdk_window_get_frame_extents (mv_resize->moveresize_window, &rect);
      gdk_window_get_geometry (mv_resize->moveresize_window, 
			       NULL, NULL, &width, &height, NULL);
      
      switch (mv_resize->moveresize_geometry.win_gravity) 
	{
	case GDK_GRAVITY_NORTH_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_NORTH:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;	  
	case GDK_GRAVITY_NORTH_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_CENTER:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_SOUTH_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	case GDK_GRAVITY_SOUTH:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	case GDK_GRAVITY_SOUTH_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	default:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y;
	  break; 
	}
    }  
}

static void
emulate_resize_drag (GdkWindow     *window,
                     GdkWindowEdge  edge,
                     gint           button,
                     gint           root_x,
                     gint           root_y,
                     guint32        timestamp)
{
  MoveResizeData *mv_resize = get_move_resize_data (GDK_WINDOW_DISPLAY (window), TRUE);

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_window = g_object_ref (window);

  gdk_drawable_get_size (window,
			 &mv_resize->moveresize_orig_width,
			 &mv_resize->moveresize_orig_height);

  mv_resize->moveresize_geom_mask = 0;
  gdk_window_get_geometry_hints (window,
				 &mv_resize->moveresize_geometry,
				 &mv_resize->moveresize_geom_mask);

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static void
emulate_move_drag (GdkWindow     *window,
                   gint           button,
                   gint           root_x,
                   gint           root_y,
                   guint32        timestamp)
{
  MoveResizeData *mv_resize = get_move_resize_data (GDK_WINDOW_DISPLAY (window), TRUE);
  
  mv_resize->is_resize = FALSE;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;

  mv_resize->moveresize_window = g_object_ref (window);

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

/**
 * gdk_window_begin_resize_drag:
 * @window: a toplevel #GdkWindow
 * @edge: the edge or corner from which the drag is started
 * @button: the button being used to drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use gdk_event_get_time())
 *
 * Begins a window resize operation (for a toplevel window).
 * You might use this function to implement a "window resize grip," for
 * example; in fact #GtkStatusbar uses it. The function works best
 * with window managers that support the <ulink url="http://www.freedesktop.org/Standards/wm-spec">Extended Window Manager Hints</ulink>, but has a 
 * fallback implementation for other window managers.
 * 
 **/
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

  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
					   gdk_atom_intern_static_string ("_NET_WM_MOVERESIZE")))
    wmspec_resize_drag (window, edge, button, root_x, root_y, timestamp);
  else
    emulate_resize_drag (window, edge, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_begin_move_drag:
 * @window: a toplevel #GdkWindow
 * @button: the button being used to drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag
 *
 * Begins a window move operation (for a toplevel window).  You might
 * use this function to implement a "window move grip," for
 * example. The function works best with window managers that support
 * the <ulink url="http://www.freedesktop.org/Standards/wm-spec">Extended 
 * Window Manager Hints</ulink>, but has a fallback implementation for
 * other window managers.
 * 
 **/
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

  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
					   gdk_atom_intern_static_string ("_NET_WM_MOVERESIZE")))
    wmspec_moveresize (window, _NET_WM_MOVERESIZE_MOVE, root_x, root_y,
		       timestamp);
  else
    emulate_move_drag (window, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_enable_synchronized_configure:
 * @window: a toplevel #GdkWindow
 * 
 * Indicates that the application will cooperate with the window
 * system in synchronizing the window repaint with the window
 * manager during resizing operations. After an application calls
 * this function, it must call gdk_window_configure_finished() every
 * time it has finished all processing associated with a set of
 * Configure events. Toplevel GTK+ windows automatically use this
 * protocol.
 * 
 * On X, calling this function makes @window participate in the
 * _NET_WM_SYNC_REQUEST window manager protocol.
 * 
 * Since: 2.6
 **/
void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_X11 (private->impl);
	  
  if (!impl->use_synchronized_configure)
    {
      impl->use_synchronized_configure = TRUE;
      ensure_sync_counter (window);
    }
}

/**
 * gdk_window_configure_finished:
 * @window: a toplevel #GdkWindow
 * 
 * Signal to the window system that the application has finished
 * handling Configure events it has received. Window Managers can
 * use this to better synchronize the frame repaint with the
 * application. GTK+ applications will automatically call this
 * function when appropriate.
 *
 * This function can only be called if gdk_window_enable_synchronized_configure()
 * was called previously.
 *
 * Since: 2.6
 **/
void
gdk_window_configure_finished (GdkWindow *window)
{
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_X11 (((GdkWindowObject *)window)->impl);
  if (!impl->use_synchronized_configure)
    return;
  
#ifdef HAVE_XSYNC
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
      GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);

      if (toplevel && toplevel->update_counter != None &&
	  GDK_DISPLAY_X11 (display)->use_sync &&
	  !XSyncValueIsZero (toplevel->current_counter_value))
	{
	  XSyncSetCounter (GDK_WINDOW_XDISPLAY (window), 
			   toplevel->update_counter,
			   toplevel->current_counter_value);
	  
	  XSyncIntToValue (&toplevel->current_counter_value, 0);
	}
    }
#endif
}

#define __GDK_WINDOW_X11_C__
#include "gdkaliasdef.c"
