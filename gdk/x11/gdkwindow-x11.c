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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <netinet/in.h>
#include <unistd.h>
#include "gdk.h"
#include "config.h"

#include "gdkwindow.h"
#include "gdkinputprivate.h"
#include "gdkprivate-x11.h"
#include "gdkregion.h"
#include "gdkinternals.h"
#include "MwmUtil.h"
#include "gdkwindow-x11.h"

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
static gboolean gdk_window_gravity_works          (void);
static void     gdk_window_set_static_win_gravity (GdkWindow *window,
						   gboolean   on);
static gboolean gdk_window_have_shape_ext         (void);
static gboolean gdk_window_icon_name_set          (GdkWindow *window);

static GdkColormap* gdk_window_impl_x11_get_colormap (GdkDrawable *drawable);
static void         gdk_window_impl_x11_set_colormap (GdkDrawable *drawable,
						      GdkColormap *cmap);
static void         gdk_window_impl_x11_get_size    (GdkDrawable *drawable,
						     gint *width,
						     gint *height);
static GdkRegion*  gdk_window_impl_x11_get_visible_region (GdkDrawable *drawable);
static void gdk_window_impl_x11_init       (GdkWindowImplX11      *window);
static void gdk_window_impl_x11_class_init (GdkWindowImplX11Class *klass);
static void gdk_window_impl_x11_finalize   (GObject            *object);

static gpointer parent_class = NULL;

GType
gdk_window_impl_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkWindowImplX11Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_window_impl_x11_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkWindowImplX11),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_window_impl_x11_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_X11,
                                            "GdkWindowImplX11",
                                            &object_info, 0);
    }
  
  return object_type;
}

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
}

static void
gdk_window_impl_x11_class_init (GdkWindowImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

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
      gdk_xid_table_remove (draw_impl->xid);
      if (window_impl->focus_window)
	gdk_xid_table_remove (window_impl->focus_window);
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GdkColormap*
gdk_window_impl_x11_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *drawable_impl;
  GdkWindowImplX11 *window_impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW_IMPL_X11 (drawable), NULL);

  drawable_impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  window_impl = GDK_WINDOW_IMPL_X11 (drawable);

  if (!((GdkWindowObject *) drawable_impl->wrapper)->input_only && 
      drawable_impl->colormap == NULL)
    {
      XWindowAttributes window_attributes;

      XGetWindowAttributes (drawable_impl->xdisplay,
                            drawable_impl->xid,
                            &window_attributes);
      drawable_impl->colormap =
        gdk_colormap_lookup (window_attributes.colormap);
    }
  
  return drawable_impl->colormap;
}

static void
gdk_window_impl_x11_set_colormap (GdkDrawable *drawable,
                                  GdkColormap *cmap)
{
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (drawable));

  impl = GDK_WINDOW_IMPL_X11 (drawable);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  /* chain up */
  GDK_DRAWABLE_CLASS (parent_class)->set_colormap (drawable, cmap);

  if (cmap)
    {
      XSetWindowColormap (draw_impl->xdisplay,
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
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  XWindowAttributes xattributes;
  unsigned int width;
  unsigned int height;
  unsigned int border_width;
  unsigned int depth;
  int x, y;

  g_assert (_gdk_parent_root == NULL);
  
  XGetGeometry (gdk_display, _gdk_root_window, &_gdk_root_window,
		&x, &y, &width, &height, &border_width, &depth);
  XGetWindowAttributes (gdk_display, _gdk_root_window, &xattributes);

  _gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)_gdk_parent_root;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  
  draw_impl->xdisplay = gdk_display;
  draw_impl->xid = _gdk_root_window;
  draw_impl->wrapper = GDK_DRAWABLE (private);
  
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = depth;
  impl->width = width;
  impl->height = height;
  
  gdk_xid_table_insert (&_gdk_root_window, _gdk_parent_root);
}

static Atom wm_client_leader_atom = None;

/**
 * gdk_window_new:
 * @parent: a #GdkWindow
 * @attributes: attributes of the new window
 * @attributes_mask: mask indicating which fields in @attributes are valid
 * 
 * Creates a new #GdkWindow using the attributes from @attributes. See
 * #GdkWindowAttr and #GdkWindowAttributesType for more details.
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
  
  GdkVisual *visual;
  Window xparent;
  Visual *xvisual;
  Display *xdisplay;
  Window xid;

  XSetWindowAttributes xattributes;
  long xattributes_mask;
  XSizeHints size_hints;
  XWMHints wm_hints;
  XClassHint *class_hint;
  int x, y, depth;
  
  unsigned int class;
  char *title;
  int i;
  long pid;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent)
    parent = _gdk_parent_root;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;
  
  xparent = GDK_WINDOW_XID (parent);
  
  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  xdisplay = draw_impl->xdisplay = GDK_WINDOW_XDISPLAY (parent); 

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = _gdk_parent_root;
  
  private->parent = (GdkWindowObject *)parent;

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

  _gdk_window_init_position (GDK_WINDOW (private));
  if (impl->position_info.big)
    private->guffaw_gravity = TRUE;
  
  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;
  
  xattributes.event_mask = StructureNotifyMask;
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
	  xparent = _gdk_root_window;
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
          gdk_colormap_ref (attributes->colormap);
        }
      else
	{
	  if ((((GdkVisualPrivate*)gdk_visual_get_system ())->xvisual) == xvisual)
            {
              draw_impl->colormap =
                gdk_colormap_get_system ();
              gdk_colormap_ref (draw_impl->colormap);
            }
	  else
            {
              draw_impl->colormap =
                gdk_colormap_new (visual, FALSE);
            }
	}
      
      private->bg_color.pixel = BlackPixel (gdk_display, _gdk_screen);
      xattributes.background_pixel = private->bg_color.pixel;

      private->bg_pixmap = NULL;
      
      xattributes.border_pixel = BlackPixel (gdk_display, _gdk_screen);
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
	}
    }
  else
    {
      depth = 0;
      private->depth = 0;
      class = InputOnly;
      private->input_only = TRUE;
      draw_impl->colormap = gdk_colormap_get_system ();
      gdk_colormap_ref (draw_impl->colormap);
    }

  xid = draw_impl->xid = XCreateWindow (xdisplay, xparent,
					impl->position_info.x, impl->position_info.y,
					impl->position_info.width, impl->position_info.height,
					0, depth, class, xvisual,
					xattributes_mask, &xattributes);

  gdk_drawable_ref (window);
  gdk_xid_table_insert (&draw_impl->xid, window);
  
  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));
  
  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);
  
  switch (GDK_WINDOW_TYPE (private))
    {
    case GDK_WINDOW_DIALOG:
      XSetTransientForHint (xdisplay, xid, xparent);
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      XSetWMProtocols (xdisplay, xid,
		       _gdk_wm_window_protocols, 3);
      break;
    case GDK_WINDOW_CHILD:
      if ((attributes->wclass == GDK_INPUT_OUTPUT) &&
	  (draw_impl->colormap != gdk_colormap_get_system ()) &&
	  (draw_impl->colormap != gdk_window_get_colormap (gdk_window_get_toplevel (window))))
	{
	  GDK_NOTE (MISC, g_message ("adding colormap window\n"));
	  gdk_window_add_colormap_windows (window);
	}
      
      return window;
    default:
      
      return window;
    }

  if (class != InputOnly)
    {
      /* The focus window is off the visible area, and serves to receive key
       * press events so they don't get sent to child windows.
       */
      impl->focus_window = XCreateSimpleWindow (xdisplay, xid,
						-1, -1, 1, 1, 0,
						xattributes.background_pixel,
						xattributes.background_pixel);
      /* FIXME: probably better to actually track the requested event mask for the toplevel
       */
      XSelectInput (xdisplay, impl->focus_window,
		    KeyPressMask | KeyReleaseMask | FocusChangeMask);
      
      XMapWindow (xdisplay, impl->focus_window);
      gdk_xid_table_insert (&impl->focus_window, window);
    }

  size_hints.flags = PSize;
  size_hints.width = impl->width;
  size_hints.height = impl->height;
  
  wm_hints.flags = StateHint | WindowGroupHint;
  wm_hints.window_group = _gdk_leader_window;
  wm_hints.input = True;
  wm_hints.initial_state = NormalState;
  
  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  XSetWMNormalHints (xdisplay, xid, &size_hints);
  
  XSetWMHints (xdisplay, xid, &wm_hints);
  
  if (!wm_client_leader_atom)
    wm_client_leader_atom = gdk_x11_get_xatom_by_name ("WM_CLIENT_LEADER");

  /* This will set WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (xdisplay, xid, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  pid = getpid ();
  XChangeProperty (xdisplay, xid,
		   gdk_x11_get_xatom_by_name ("_NET_WM_PID"),
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *)&pid, 1);
  
  XChangeProperty (xdisplay, xid,
	   	   wm_client_leader_atom,
		   XA_WINDOW, 32, PropModeReplace,
		   (guchar*) &_gdk_leader_window, 1);
  
  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = g_get_prgname ();

  gdk_window_set_title (window, title);
  
  if (attributes_mask & GDK_WA_WMCLASS)
    {
      class_hint = XAllocClassHint ();
      class_hint->res_name = attributes->wmclass_name;
      class_hint->res_class = attributes->wmclass_class;
      XSetClassHint (xdisplay, xid, class_hint);
      XFree (class_hint);
    }
  
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
 * gdk_window_foreign_new:
 * @anid: a native window handle.
 * 
 * Wraps a native window in a #GdkWindow.
 * This may fail if the window has been destroyed.
 *
 * For example in the X backend, a native window handle is an Xlib
 * <type>XID</type>.
 * 
 * Return value: the newly-created #GdkWindow wrapper for the 
 *    native window or %NULL if the window has been destroyed.
 **/
GdkWindow *
gdk_window_foreign_new (GdkNativeWindow anid)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  XWindowAttributes attrs;
  Window root, parent;
  Window *children = NULL;
  guint nchildren;
  gboolean result;

  gdk_error_trap_push ();
  result = XGetWindowAttributes (gdk_display, anid, &attrs);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  /* FIXME: This is pretty expensive. Maybe the caller should supply
   *        the parent */
  gdk_error_trap_push ();
  result = XQueryTree (gdk_display, anid, &root, &parent, &children, &nchildren);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  if (children)
    XFree (children);
  
  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  
  private->parent = gdk_xid_table_lookup (parent);
  if (!private->parent || GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_FOREIGN)
    private->parent = (GdkWindowObject *)_gdk_parent_root;
  
  private->parent->children = g_list_prepend (private->parent->children, window);

  draw_impl->xid = anid;
  draw_impl->xdisplay = gdk_display;

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

  gdk_drawable_ref (window);
  gdk_xid_table_insert (&GDK_WINDOW_XID (window), window);
  
  return window;
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

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  _gdk_selection_window_destroyed (window);
  
  if (private->extension_events != 0)
    gdk_input_window_destroy (window);

#ifdef HAVE_XFT  
  {
    GdkDrawableImplX11 *draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);

    if (draw_impl->picture)
      XRenderFreePicture (draw_impl->xdisplay, draw_impl->picture);
  }
#endif /* HAVE_XFT */  

  if (private->window_type == GDK_WINDOW_FOREIGN)
    {
      if (!foreign_destroy && (private->parent != NULL))
	{
	  /* It's somebody else's window, but in our heirarchy,
	   * so reparent it to the root window, and then send
	   * it a delete event, as if we were a WM
	   */
	  XClientMessageEvent xevent;
	  
	  gdk_error_trap_push ();
	  gdk_window_hide (window);
	  gdk_window_reparent (window, NULL, 0, 0);
	  
	  xevent.type = ClientMessage;
	  xevent.window = GDK_WINDOW_XID (window);
	  xevent.message_type = gdk_x11_get_xatom_by_name ("WM_PROTOCOLS");
	  xevent.format = 32;
	  xevent.data.l[0] = gdk_x11_get_xatom_by_name ("WM_DELETE_WINDOW");
	  xevent.data.l[1] = CurrentTime;
	  
	  XSendEvent (GDK_WINDOW_XDISPLAY (window),
		      GDK_WINDOW_XID (window),
		      False, 0, (XEvent *)&xevent);
	  gdk_flush ();
	  gdk_error_trap_pop ();
	}
    }
  else if (!recursing && !foreign_destroy)
    {
      XDestroyWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
    }
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  GdkWindowImplX11 *window_impl;

  g_return_if_fail (window != NULL);
  
  window_impl = GDK_WINDOW_IMPL_X11 (((GdkWindowObject *)window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %#lx unexpectedly destroyed", GDK_WINDOW_XID (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  gdk_xid_table_remove (GDK_WINDOW_XID (window));
  if (window_impl->focus_window)
    gdk_xid_table_remove (window_impl->focus_window);

  _gdk_xgrab_check_destroy (window);
  
  gdk_drawable_unref (window);
}

static void
set_initial_hints (GdkWindow *window)
{
  GdkWindowObject *private;
  Atom atoms[5];
  gint i;
  
  private = (GdkWindowObject*) window;

  if (private->state & GDK_WINDOW_STATE_ICONIFIED)
    {
      XWMHints *wm_hints;
      
      wm_hints = XGetWMHints (GDK_WINDOW_XDISPLAY (window),
                              GDK_WINDOW_XID (window));
      if (!wm_hints)
        wm_hints = XAllocWMHints ();

      wm_hints->flags |= StateHint;
      wm_hints->initial_state = IconicState;
      
      XSetWMHints (GDK_WINDOW_XDISPLAY (window),
                   GDK_WINDOW_XID (window), wm_hints);
      XFree (wm_hints);
    }

  /* We set the spec hints regardless of whether the spec is supported,
   * since it can't hurt and it's kind of expensive to check whether
   * it's supported.
   */
  
  i = 0;

  if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      atoms[i] = gdk_x11_get_xatom_by_name ("_NET_WM_STATE_MAXIMIZED_VERT");
      ++i;
      atoms[i] = gdk_x11_get_xatom_by_name ("_NET_WM_STATE_MAXIMIZED_HORZ");
      ++i;
    }

  if (private->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[i] = gdk_x11_get_xatom_by_name ("_NET_WM_STATE_STICKY");
      ++i;
    }

  if (private->modal_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name ("_NET_WM_STATE_MODAL");
      ++i;
    }

  if (i > 0)
    {
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name ("_NET_WM_STATE"),
                       XA_ATOM, 32, PropModeReplace,
                       (guchar*) atoms, i);
    }

  if (private->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[0] = 0xFFFFFFFF;
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name ("_NET_WM_DESKTOP"),
                       XA_CARDINAL, 32, PropModeReplace,
                       (guchar*) atoms, 1);
    }
}

static void
show_window_internal (GdkWindow *window,
                      gboolean   raise)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      if (raise)
        XRaiseWindow (GDK_WINDOW_XDISPLAY (window),
                      GDK_WINDOW_XID (window));

      if (!GDK_WINDOW_IS_MAPPED (window))
        {
          set_initial_hints (window);
          
          gdk_synthesize_window_state (window,
                                       GDK_WINDOW_STATE_WITHDRAWN,
                                       0);
        }
      
      g_assert (GDK_WINDOW_IS_MAPPED (window));
      
      if (GDK_WINDOW_IMPL_X11 (private->impl)->position_info.mapped)
        XMapWindow (GDK_WINDOW_XDISPLAY (window),
                    GDK_WINDOW_XID (window));
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
  
  g_return_if_fail (window != NULL);

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
      
      XUnmapWindow (GDK_WINDOW_XDISPLAY (window),
                    GDK_WINDOW_XID (window));
    }
}

/**
 * gdk_window_withdraw:
 * @window: a toplevel #GdkWindow
 * 
 * Withdraws a window (unmaps it and asks the window manager to forget about it).
 * Normally done automatically by gtk_widget_hide() called on a #GtkWindow.
 * 
 **/
void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_WITHDRAWN);

      g_assert (!GDK_WINDOW_IS_MAPPED (window));
      
      XWithdrawWindow (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window), 0);
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
 * 
 **/
void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplX11 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_X11 (private->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, x, y,
				       impl->width, impl->height);
      else
	{
	  XMoveWindow (GDK_WINDOW_XDISPLAY (window),
		       GDK_WINDOW_XID (window),
		       x, y);
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
 * 
 **/
void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowObject*) window;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, private->x, private->y,
				       width, height);
      else
	{
	  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);
	  
	  if (width != impl->width || height != impl->height)
	    private->resize_count += 1;

	  XResizeWindow (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 width, height);
	}
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
 * 
 **/
void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  
  private = (GdkWindowObject*) window;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, x, y, width, height);
      else
	{
	  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (private->impl);
	  
	  if (width != impl->width || height != impl->height)
	    private->resize_count += 1;
	  
	  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
			     GDK_WINDOW_XID (window),
			     x, y, width, height);
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);
  
  if (!new_parent)
    new_parent = _gdk_parent_root;
  
  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject*)window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  
  if (!GDK_WINDOW_DESTROYED (window) && !GDK_WINDOW_DESTROYED (new_parent))
    XReparentWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     GDK_WINDOW_XID (new_parent),
		     x, y);

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    new_parent = _gdk_parent_root;
  
  window_private->parent = (GdkWindowObject *)new_parent;

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_FOREIGN:
      /* Now a toplevel */
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	{
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
	  XSetWMProtocols (GDK_WINDOW_XDISPLAY (window),
			   GDK_WINDOW_XID (window),
			   _gdk_wm_window_protocols, 3);
	}
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&
	  GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  /* If we were being sophisticated, we'd save the old window type
	   * here, and restore it if we were reparented back to the
	   * toplevel. However, the difference between different types
	   * of toplevels only really matters on creation anyways.
	   */
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
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
  g_return_if_fail (window != NULL);
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
  g_return_if_fail (window != NULL);
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
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_raise() only
 * requests the restack, does not guarantee it.
 * 
 **/
void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
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
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XLowerWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

/**
 * gdk_window_focus:
 * @window: a #GdkWindow
 * @timestamp: timestamp of the event triggering the window focus
 *
 * Sets keyboard focus to @window. If @window is not onscreen this
 * will not work. In most cases, gtk_window_present() should be used on
 * a #GtkWindow, rather than calling this function.
 * 
 **/
void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (gdk_net_wm_supports (gdk_atom_intern ("_NET_ACTIVE_WINDOW", FALSE)))
    {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW");
      xev.xclient.format = 32;
      xev.xclient.data.l[0] = 0;
      
      XSendEvent (gdk_display, _gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
    }
  else
    {
      XRaiseWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));

      /* There is no way of knowing reliably whether we are viewable so we need
       * to trap errors so we don't cause a BadMatch.
       */
      gdk_error_trap_push ();
      XSetInputFocus (GDK_WINDOW_XDISPLAY (window),
                      GDK_WINDOW_XWINDOW (window),
                      RevertToParent,
                      timestamp);
      XSync (GDK_WINDOW_XDISPLAY (window), False);
      gdk_error_trap_pop ();
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
  
  g_return_if_fail (window != NULL);
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
 * @window: A #GdkWindow
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
  Atom atom;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      atom = gdk_x11_get_xatom_by_name ("_NET_WM_WINDOW_TYPE_DIALOG");
      break;
    case GDK_WINDOW_TYPE_HINT_MENU:
      atom = gdk_x11_get_xatom_by_name ("_NET_WM_WINDOW_TYPE_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      atom = gdk_x11_get_xatom_by_name ("_NET_WM_WINDOW_TYPE_TOOLBAR");
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_window_set_type_hint", hint);
      /* Fall thru */
    case GDK_WINDOW_TYPE_HINT_NORMAL:
      atom = gdk_x11_get_xatom_by_name ("_NET_WM_WINDOW_TYPE_NORMAL");
      break;
    }

  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   gdk_x11_get_xatom_by_name ("_NET_WM_WINDOW_TYPE"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&atom, 1);
}


static void
gdk_wmspec_change_state (gboolean   add,
			 GdkWindow *window,
			 GdkAtom    state1,
			 GdkAtom    state2)
{
  XEvent xev;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = GDK_WINDOW_XID (window);
  xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_WM_STATE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xev.xclient.data.l[1] = gdk_x11_atom_to_xatom (state1);
  xev.xclient.data.l[2] = gdk_x11_atom_to_xatom (state2);
  
  XSendEvent (gdk_display, _gdk_root_window, False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
}
/**
 * gdk_window_set_modal_hint:
 * @window: A #GdkWindow
 * @modal: TRUE if the window is modal, FALSE otherwise.
 *
 * The application can use this hint to tell the window manager
 * that a certain window has modal behaviour. The window manager
 * can use this information to handle modal windows in a special
 * way.
 *
 * You should only use this on windows for which you have
 * previously called #gdk_window_set_transient_for()
 **/
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
    gdk_wmspec_change_state (modal, window,
			     gdk_atom_intern ("_NET_WM_STATE_MODAL", FALSE),
			     0);
}

/**
 * gdk_window_set_geometry_hints:
 * @window: a #GdkWindow
 * @geometry: geometry hints
 * @geom_mask: bitmask indicating fields of @geometry to pay attention to
 *
 * Sets the geometry hints for @window. Hints flagged in @geom_mask
 * are set, hints not flagged in @geom_mask are unset.
 * To unset all hints, use a @geom_mask of 0 and a @geometry of %NULL.
 * 
 **/
void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  XSizeHints size_hints;
  
  g_return_if_fail (window != NULL);
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
  XSizeHints size_hints;  
  glong junk_size_mask = 0;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (geometry != NULL);
  g_return_if_fail (geom_mask != NULL);

  *geom_mask = 0;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (!XGetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
                          GDK_WINDOW_XID (window),
                          &size_hints,
                          &junk_size_mask))
    return;                   

  if (size_hints.flags & PMinSize)
    {
      *geom_mask |= GDK_HINT_MIN_SIZE;
      geometry->min_width = size_hints.min_width;
      geometry->min_height = size_hints.min_height;
    }

  if (size_hints.flags & PMaxSize)
    {
      *geom_mask |= GDK_HINT_MAX_SIZE;
      geometry->max_width = MAX (size_hints.max_width, 1);
      geometry->max_height = MAX (size_hints.max_height, 1);
    }

  if (size_hints.flags & PResizeInc)
    {
      *geom_mask |= GDK_HINT_RESIZE_INC;
      geometry->width_inc = size_hints.width_inc;
      geometry->height_inc = size_hints.height_inc;
    }

  if (size_hints.flags & PAspect)
    {
      *geom_mask |= GDK_HINT_ASPECT;

      geometry->min_aspect = (gdouble) size_hints.min_aspect.x / (gdouble) size_hints.min_aspect.y;
      geometry->max_aspect = (gdouble) size_hints.max_aspect.x / (gdouble) size_hints.max_aspect.y;
    }

  if (size_hints.flags & PWinGravity)
    {
      *geom_mask |= GDK_HINT_WIN_GRAVITY;
      geometry->win_gravity = size_hints.win_gravity;
    }
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
set_text_property (GdkWindow   *window,
		   Atom         property,
		   const gchar *utf8_str)
{
  guchar *prop_text = NULL;
  Atom prop_type;
  gint prop_length;
  gint prop_format;
  
  if (utf8_is_latin1 (utf8_str))
    {
      prop_type = XA_STRING;
      prop_text = gdk_utf8_to_string_target (utf8_str);
      prop_length = strlen (prop_text);
      prop_format = 8;
    }
  else
    {
      GdkAtom gdk_type;
      
      gdk_utf8_to_compound_text (utf8_str, &gdk_type, &prop_format,
				 &prop_text, &prop_length);
      prop_type = gdk_x11_atom_to_xatom (gdk_type);
    }

  if (prop_text)
    {
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		       GDK_WINDOW_XID (window),
		       property,
		       prop_type, prop_format,
		       PropModeReplace, prop_text,
		       prop_length);

      g_free (prop_text);
    }
}

/**
 * gdk_window_set_title:
 * @window: a #GdkWindow
 * @title: title of @window
 *
 * Sets the title of a toplevel window, to be displayed in the titlebar.
 * If you haven't explicitly set the icon name for the window
 * (using gdk_window_set_icon_name()), the icon name will be set to
 * @title as well. @title must be in UTF-8 encoding (as with all
 * user-readable strings in GDK/GTK+). @title may not be %NULL.
 * 
 **/
void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   gdk_x11_get_xatom_by_name ("_NET_WM_NAME"),
		   gdk_x11_get_xatom_by_name ("UTF8_STRING"), 8,
		   PropModeReplace, title,
		   strlen (title));

  set_text_property (window, gdk_x11_get_xatom_by_name ("WM_NAME"), title);
  if (!gdk_window_icon_name_set (window))
    {
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		       GDK_WINDOW_XID (window),
		       gdk_x11_get_xatom_by_name ("_NET_WM_ICON_NAME"),
		       gdk_x11_get_xatom_by_name ("UTF8_STRING"), 8,
		       PropModeReplace, title,
		       strlen (title));
      set_text_property (window, gdk_x11_get_xatom_by_name ("WM_ICON_NAME"), title);
    }
}

/**
 * gdk_window_set_role:
 * @window: a #GdkWindow
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
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (role)
	XChangeProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
			 gdk_x11_get_xatom_by_name ("WM_WINDOW_ROLE"), XA_STRING,
			 8, PropModeReplace, role, strlen (role));
      else
	XDeleteProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
			 gdk_x11_get_xatom_by_name ("WM_WINDOW_ROLE"));
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
  GdkWindowObject *private;
  GdkWindowObject *parent_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  parent_private = (GdkWindowObject*) parent;
  
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
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XSetWindowBackground (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), color->pixel);

  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    gdk_pixmap_unref (private->bg_pixmap);
  
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    gdk_pixmap_unref (private->bg_pixmap);

  if (parent_relative)
    {
      xpixmap = ParentRelative;
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
    }
  else
    {
      if (pixmap)
	{
	  gdk_pixmap_ref (pixmap);
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
  GdkCursorPrivate *cursor_private;
  Cursor xcursor;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!cursor)
    xcursor = None;
  else
    xcursor = cursor_private->xcursor;
  
  if (!GDK_WINDOW_DESTROYED (window))
    XDefineCursor (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   xcursor);
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
    window = _gdk_parent_root;
  
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
  
  g_return_val_if_fail (window != NULL, 0);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      return_val = XTranslateCoordinates (GDK_WINDOW_XDISPLAY (window),
					  GDK_WINDOW_XID (window),
					  _gdk_root_window,
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
 * @window: a #GdkWindow
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
  static Atom atom = 0;
  gulong number_return, bytes_after_return;
  guchar *data_return;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (!atom)
	atom = gdk_x11_get_xatom_by_name ("ENLIGHTENMENT_DESKTOP");
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
 * @window: a #GdkWindow
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
 * @window: a #GdkWindow
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
  GdkWindowObject *private;
  Window xwindow;
  Window xparent;
  Window root;
  Window *children;
  unsigned int nchildren;
  
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
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  xparent = GDK_WINDOW_XID (window);
  do
    {
      xwindow = xparent;
      if (!XQueryTree (GDK_WINDOW_XDISPLAY (window), xwindow,
		       &root, &xparent,
		       &children, &nchildren))
	return;
      
      if (children)
	XFree (children);
    }
  while (xparent != root);
  
  if (xparent == root)
    {
      unsigned int ww, wh, wb, wd;
      int wx, wy;
      
      if (XGetGeometry (GDK_WINDOW_XDISPLAY (window), xwindow, &root, &wx, &wy, &ww, &wh, &wb, &wd))
	{
          rect->x = wx;
          rect->y = wy;
          rect->width = ww;
          rect->height = wh;
	}
    }
}

GdkWindow*
_gdk_windowing_window_get_pointer (GdkWindow       *window,
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
  
  if (!window)
    window = _gdk_parent_root;
  
  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);

  return_val = NULL;
  if (!GDK_WINDOW_DESTROYED (window) &&
      XQueryPointer (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &root, &child, &rootx, &rooty, &winx, &winy, &xmask))
    {
      if (child)
	return_val = gdk_window_lookup (child);
    }
  
  if (x)
    *x = winx + xoffset;
  if (y)
    *y = winy + yoffset;
  if (mask)
    *mask = xmask;
  
  return return_val;
}

GdkWindow*
_gdk_windowing_window_at_pointer (GdkScreen *screen,
                                  gint      *win_x,
				  gint      *win_y)
{
  GdkWindow *window;
  Window root;
  Window xwindow;
  Window xwindow_last = 0;
  Display *xdisplay;
  int rootx = -1, rooty = -1;
  int winx, winy;
  unsigned int xmask;
  
  xwindow = GDK_ROOT_WINDOW ();
  xdisplay = GDK_DISPLAY ();

  gdk_x11_grab_server ();
  while (xwindow)
    {
      xwindow_last = xwindow;
      XQueryPointer (xdisplay, xwindow,
		     &root, &xwindow,
		     &rootx, &rooty,
		     &winx, &winy,
		     &xmask);
    }
  gdk_x11_ungrab_server ();
  
  window = gdk_window_lookup (xwindow_last);
  
  if (win_x)
    *win_x = window ? winx : -1;
  if (win_y)
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
  
  g_return_val_if_fail (window != NULL, 0);
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_WINDOW_OBJECT (window)->event_mask = event_mask;
      xevent_mask = StructureNotifyMask;
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

void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  GdkWindow *toplevel;
  Window *old_windows;
  Window *new_windows;
  int i, count;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  toplevel = gdk_window_get_toplevel (window);
  if (GDK_WINDOW_DESTROYED (toplevel))
    return;
  
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

static gboolean
gdk_window_have_shape_ext (void)
{
  enum { UNKNOWN, NO, YES };
  static gint have_shape = UNKNOWN;
  
  if (have_shape == UNKNOWN)
    {
      int ignore;
      if (XQueryExtension (gdk_display, "SHAPE", &ignore, &ignore, &ignore))
	have_shape = YES;
      else
	have_shape = NO;
    }
  
  return (have_shape == YES);
}

#define WARN_SHAPE_TOO_BIG() g_warning ("GdkWindow is too large to allow the use of shape masks or shape regions.")

/*
 * This needs the X11 shape extension.
 * If not available, shaped windows will look
 * ugly, but programs still work.    Stefan Wille
 */
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
 **/
void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  Pixmap pixmap;
  gint xoffset, yoffset;
  
  g_return_if_fail (window != NULL);
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
  
  if (gdk_window_have_shape_ext ())
    {
      if (mask)
	{
	  pixmap = GDK_PIXMAP_XID (mask);
	}
      else
	{
	  x = 0;
	  y = 0;
	  pixmap = None;
	}
      
      XShapeCombineMask (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 ShapeBounding,
			 x, y,
			 pixmap,
			 ShapeSet);
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
 **/
void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape_region,
                                 gint       offset_x,
                                 gint       offset_y)
{
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
  
  if (gdk_window_have_shape_ext ())
    {
      gint n_rects = 0;
      XRectangle *xrects = NULL;

      _gdk_region_get_xrectangles (shape_region,
                                   0, 0,
                                   &xrects, &n_rects);
      
      XShapeCombineRectangles (GDK_WINDOW_XDISPLAY (window),
                               GDK_WINDOW_XID (window),
                               ShapeBounding,
                               offset_x, offset_y,
                               xrects, n_rects,
                               ShapeSet,
                               YXBanded);

      g_free (xrects);
    }
#endif /* HAVE_SHAPE_EXT */
}


/**
 * gdk_window_set_override_redirect:
 * @window: a #GdkWindow
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    {
      attr.override_redirect = (override_redirect == FALSE)?False:True;
      XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			       GDK_WINDOW_XID (window),
			       CWOverrideRedirect,
			       &attr);
    }
}


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
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  l = pixbufs;
  size = 0;
  
  while (l)
    {
      pixbuf = l->data;
      g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      
      size += 2 + width * height;

      l = g_list_next (l);
    }

  data = g_malloc (size * sizeof (gulong));

  l = pixbufs;
  p = data;
  while (l)
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
    }

  if (size > 0)
    {
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name ("_NET_WM_ICON"),
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (guchar*) data, size);
    }
  else
    {
      XDeleteProperty (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name ("_NET_WM_ICON"));
    }
  
  g_free (data);
}

/**
 * gdk_window_set_icon:
 * @window: a #GdkWindow
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
  XWMHints *wm_hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  wm_hints = XGetWMHints (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window));
  if (!wm_hints)
    wm_hints = XAllocWMHints ();

  if (icon_window != NULL)
    {
      wm_hints->flags |= IconWindowHint;
      wm_hints->icon_window = GDK_WINDOW_XID (icon_window);
    }
  
  if (pixmap != NULL)
    {
      wm_hints->flags |= IconPixmapHint;
      wm_hints->icon_pixmap = GDK_PIXMAP_XID (pixmap);
    }
  
  if (mask != NULL)
    {
      wm_hints->flags |= IconMaskHint;
      wm_hints->icon_mask = GDK_PIXMAP_XID (mask);
    }

  XSetWMHints (GDK_WINDOW_XDISPLAY (window),
	       GDK_WINDOW_XID (window), wm_hints);
  XFree (wm_hints);
}

static gboolean
gdk_window_icon_name_set (GdkWindow *window)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (window),
					       g_quark_from_static_string ("gdk-icon-name-set")));
}

/**
 * gdk_window_set_icon_name:
 * @window: a #GdkWindow
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
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
		      GUINT_TO_POINTER (TRUE));

  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   gdk_x11_get_xatom_by_name ("_NET_WM_ICON_NAME"),
		   gdk_x11_get_xatom_by_name ("UTF8_STRING"), 8,
		   PropModeReplace, name,
		   strlen (name));
  set_text_property (window, gdk_x11_get_xatom_by_name ("WM_ICON_NAME"), name);
}

/**
 * gdk_window_iconify:
 * @window: a #GdkWindow
 * 
 * Asks to iconify (minimize) @window. The window manager may choose
 * to ignore the request, but normally will honor it. Using
 * gtk_window_iconify() is preferred, if you have a #GtkWindow widget.
 * 
 **/
void
gdk_window_iconify (GdkWindow *window)
{
  Display *display;
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = GDK_WINDOW_XDISPLAY (window);

  private = (GdkWindowObject*) window;

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      XIconifyWindow (display, GDK_WINDOW_XWINDOW (window), DefaultScreen (display));

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
 * @window: a #GdkWindow
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
  Display *display;
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = GDK_WINDOW_XDISPLAY (window);

  private = (GdkWindowObject*) window;

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
      
      XEvent xev;

      /* Request stick during viewport scroll */
      gdk_wmspec_change_state (TRUE, window,
			       gdk_atom_intern ("_NET_WM_STATE_STICKY", FALSE),
			       0);

      /* Request desktop 0xFFFFFFFF */
      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_WM_DESKTOP");
      xev.xclient.format = 32;

      xev.xclient.data.l[0] = 0xFFFFFFFF;
      
      XSendEvent (gdk_display, _gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
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
      XEvent xev;
      Atom type;
      gint format;
      gulong nitems;
      gulong bytes_after;
      gulong *current_desktop;
      
      /* Request unstick from viewport */
      gdk_wmspec_change_state (FALSE, window,
			       gdk_atom_intern ("_NET_WM_STATE_STICKY", FALSE),
			       0);

      /* Get current desktop, then set it; this is a race, but not
       * one that matters much in practice.
       */
      XGetWindowProperty (gdk_display, _gdk_root_window,
                          gdk_x11_get_xatom_by_name ("_NET_CURRENT_DESKTOP"),
                          0, G_MAXLONG,
                          False, XA_CARDINAL, &type, &format, &nitems,
                          &bytes_after, (guchar **)&current_desktop);

      if (type == XA_CARDINAL)
        {
          xev.xclient.type = ClientMessage;
          xev.xclient.serial = 0;
          xev.xclient.send_event = True;
          xev.xclient.window = GDK_WINDOW_XWINDOW (window);
          xev.xclient.display = gdk_display;
          xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_WM_DESKTOP");
          xev.xclient.format = 32;

          xev.xclient.data.l[0] = *current_desktop;
      
          XSendEvent (gdk_display, _gdk_root_window, False,
                      SubstructureRedirectMask | SubstructureNotifyMask,
                      &xev);

          XFree (current_desktop);
        }
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
 * @window: a #GdkWindow
 *
 * Asks the window manager to maximize @window, if the window manager supports
 * this operation. Not all window managers support this, and some deliberately
 * ignore it or don't have a concept of "maximized"; so you can't rely on the
 * maximization actually happening. But it will happen with most standard
 * window managers, and GDK makes a best effort to get it to happen.
 *
 * If the window was already maximized, then this function does nothing.
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
			     gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE),
			     gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE));
  else
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_MAXIMIZED);
}

/**
 * gdk_window_unmaximize:
 * @window: a #GdkWindow
 *
 * Asks the window manager to unmaximize @window, if the window manager supports
 * this operation. Not all window managers support this, and some deliberately
 * ignore it or don't have a concept of "maximized"; so you can't rely on the
 * unmaximization actually happening. But it will happen with most standard
 * window managers, and GDK makes a best effort to get it to happen.
 *
 * If the window wasn't maximized, then this function does nothing.
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
			     gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE),
			     gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE));
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

/**
 * gdk_window_set_group:
 * @window: a #GdkWindow
 * @leader: group leader window
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
 * The group leader window may not be changed after a window has been
 * mapped (with gdk_window_show() for example).
 * 
 **/
void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  XWMHints *wm_hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (leader != NULL);
  g_return_if_fail (GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (leader))
    return;
  
  wm_hints = XGetWMHints (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window));
  if (!wm_hints)
    wm_hints = XAllocWMHints ();

  wm_hints->flags |= WindowGroupHint;
  wm_hints->window_group = GDK_WINDOW_XID (leader);

  XSetWMHints (GDK_WINDOW_XDISPLAY (window),
	       GDK_WINDOW_XID (window), wm_hints);
  XFree (wm_hints);
}

static MotifWmHints *
gdk_window_get_mwm_hints (GdkWindow *window)
{
  static Atom hints_atom = None;
  MotifWmHints *hints;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_WINDOW_DESTROYED (window))
    return NULL;
  
  if (!hints_atom)
    hints_atom = XInternAtom (GDK_WINDOW_XDISPLAY (window), 
			      _XA_MOTIF_WM_HINTS, FALSE);
  
  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, (guchar **)&hints);

  if (type == None)
    return NULL;
  
  return hints;
}

static void
gdk_window_set_mwm_hints (GdkWindow *window,
			  MotifWmHints *new_hints)
{
  static Atom hints_atom = None;
  MotifWmHints *hints;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (!hints_atom)
    hints_atom = XInternAtom (GDK_WINDOW_XDISPLAY (window), 
			      _XA_MOTIF_WM_HINTS, FALSE);
  
  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, (guchar **)&hints);
  
  if (type == None)
    hints = new_hints;
  else
    {
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
 * @window: a #GdkWindow
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = decorations;
  
  gdk_window_set_mwm_hints (window, &hints);
}

/**
 * gdk_window_get_decorations:
 * @window: The #GdkWindow to get the decorations from
 * @decorations: The window decorations will be written here
 *
 * Returns the decorations set on the GdkWindow with #gdk_window_set_decorations
 * Returns: TRUE if the window has decorations set, FALSE otherwise.
 **/
gboolean
gdk_window_get_decorations(GdkWindow *window,
			   GdkWMDecoration *decorations)
{
  MotifWmHints *hints;
  gboolean result = FALSE;

  hints = gdk_window_get_mwm_hints (window);
  
  if (hints)
    {
      if (hints->flags & MWM_HINTS_DECORATIONS)
	{
	  *decorations = hints->decorations;
	  result = TRUE;
	}
      
      XFree (hints);
    }

  return result;
}

/**
 * gdk_window_set_functions:
 * @window: a #GdkWindow
 * @functions: bitmask of operations to allow on @window
 *
 * This function isn't really good for much. It sets the traditional
 * Motif window manager hint for which operations the window manager
 * should allow on a toplevel window. However, few window managers do
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
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
		      gboolean merge)
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
	  XShapeCombineRectangles (disp, win, ShapeBounding, 0, 0, rects, num_rects,
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
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_window_have_shape_ext ())
    gdk_propagate_shapes (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), FALSE);
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
 * 
 **/
void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_window_have_shape_ext ())
    gdk_propagate_shapes (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XID (window), TRUE);
#endif   
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
      
      /* This particular server apparently has a bug so that the test
       * works but the actual code crashes it
       */
      if ((!strcmp (XServerVendor (gdk_display), "Sun Microsystems, Inc.")) &&
	  (VendorRelease (gdk_display) == 3400))
	{
	  gravity_works = NO;
	  return FALSE;
	}
      
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
  XSetWindowAttributes xattributes;
  guint xattributes_mask = 0;
  
  g_return_if_fail (window != NULL);
  
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
  
  g_return_if_fail (window != NULL);
  
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
	  gdk_window_set_static_win_gravity (tmp_list->data, use_static);
	  
	  tmp_list = tmp_list->next;
	}
    }
  
  return TRUE;
}

/* internal function created for and used by gdk_window_xid_at_coords */
Window
gdk_window_xid_at (Window   base,
		   gint     bx,
		   gint     by,
		   gint     x,
		   gint     y, 
		   GList   *excludes,
		   gboolean excl_child)
{
  Display *xdisplay;
  Window *list = NULL;
  Window child = 0, parent_win = 0, root_win = 0;
  int i;
  unsigned int ww, wh, wb, wd, num;
  int wx, wy;
  
  xdisplay = GDK_DISPLAY ();
  if (!XGetGeometry (xdisplay, base, &root_win, &wx, &wy, &ww, &wh, &wb, &wd))
    return 0;
  wx += bx;
  wy += by;
  
  if (!((x >= wx) &&
	(y >= wy) &&
	(x < (int) (wx + ww)) &&
	(y < (int) (wy + wh))))
    return 0;
  
  if (!XQueryTree (xdisplay, base, &root_win, &parent_win, &list, &num))
    return base;
  
  if (list)
    {
      for (i = num - 1; ; i--)
	{
	  if ((!excl_child) || (!g_list_find (excludes, (gpointer *) list[i])))
	    {
	      if ((child = gdk_window_xid_at (list[i], wx, wy, x, y, excludes, excl_child)) != 0)
		{
		  XFree (list);
		  return child;
		}
	    }
	  if (!i)
	    break;
	}
      XFree (list);
    }
  return base;
}

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
Window
gdk_window_xid_at_coords (gint     x,
			  gint     y,
			  GList   *excludes,
			  gboolean excl_child)
{
  GdkWindow *window;
  Display *xdisplay;
  Window *list = NULL;
  Window root, child = 0, parent_win = 0, root_win = 0;
  unsigned int num;
  int i;

  window = _gdk_parent_root;
  xdisplay = GDK_WINDOW_XDISPLAY (window);
  root = GDK_WINDOW_XID (window);
  num = g_list_length (excludes);
  
  gdk_x11_grab_server ();
  if (!XQueryTree (xdisplay, root, &root_win, &parent_win, &list, &num))
    {
      gdk_x11_ungrab_server ();
      return root;
    }
  if (list)
    {
      i = num - 1;
      do
	{
	  XWindowAttributes xwa;
	  
	  XGetWindowAttributes (xdisplay, list [i], &xwa);
	  
	  if (xwa.map_state != IsViewable)
	    continue;
	  
	  if (excl_child && g_list_find (excludes, (gpointer *) list[i]))
	    continue;
	  
	  if ((child = gdk_window_xid_at (list[i], 0, 0, x, y, excludes, excl_child)) == 0)
	    continue;
	  
	  if (excludes)
	    {
	      if (!g_list_find (excludes, (gpointer *) child))
		{
		  XFree (list);
                  gdk_x11_ungrab_server ();
		  return child;
		}
	    }
	  else
	    {
	      XFree (list);
              gdk_x11_ungrab_server ();
	      return child;
	    }
	} while (--i > 0);
      XFree (list);
    }
  gdk_x11_ungrab_server ();
  return root;
}

static void
wmspec_moveresize (GdkWindow *window,
                   gint       direction,
                   gint       root_x,
                   gint       root_y,
                   guint32    timestamp)     
{
  XEvent xev;

  /* Release passive grab */
  gdk_pointer_ungrab (timestamp);
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = GDK_WINDOW_XID (window);
  xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = root_x;
  xev.xclient.data.l[1] = root_y;
  xev.xclient.data.l[2] = direction;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  
  XSendEvent (gdk_display, _gdk_root_window, False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
}

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

/* This is global for use in gdkevents-x11.c */
GdkWindow *_gdk_moveresize_window;

static GdkWindow *moveresize_emulation_window = NULL;
static gboolean is_resize = FALSE;
static GdkWindowEdge resize_edge;
static gint moveresize_button;
static gint moveresize_x;
static gint moveresize_y;
static gint moveresize_orig_x;
static gint moveresize_orig_y;
static gint moveresize_orig_width;
static gint moveresize_orig_height;
static GdkWindowHints moveresize_geom_mask = 0;
static GdkGeometry moveresize_geometry;
static Time moveresize_process_time;

static XEvent *moveresize_pending_event;

static void
update_pos (gint new_root_x,
            gint new_root_y)
{
  gint dx, dy;
  
  dx = new_root_x - moveresize_x;
  dy = new_root_y - moveresize_y;

  if (is_resize)
    {
      gint w, h;

      w = moveresize_orig_width;
      h = moveresize_orig_height;
      
      switch (resize_edge)
        {
        case GDK_WINDOW_EDGE_SOUTH_EAST:
          w += dx;
          h += dy;
          break;
        }

      w = MAX (w, 1);
      h = MAX (h, 1);
      
      if (moveresize_geom_mask)
        {
          gdk_window_constrain_size (&moveresize_geometry,
                                     moveresize_geom_mask,
                                     w, h,
                                     &w, &h);
        }
      
      gdk_window_resize (_gdk_moveresize_window, w, h);
    }
  else
    {
      gint x, y;

      x = moveresize_orig_x + dx;
      y = moveresize_orig_y + dy;
      
      gdk_window_move (_gdk_moveresize_window, x, y);
    }
}

static void
finish_drag (void)
{
  gdk_window_destroy (moveresize_emulation_window);
  moveresize_emulation_window = NULL;
  _gdk_moveresize_window = NULL;

  if (moveresize_pending_event)
    {
      g_free (moveresize_pending_event);
      moveresize_pending_event = NULL;
    }
}

static int
lookahead_motion_predicate (Display *display,
			    XEvent  *event,
			    XPointer arg)
{
  gboolean *seen_release = (gboolean *)arg;
  
  if (*seen_release)
    return False;

  switch (event->xany.type)
    {
    case ButtonRelease:
      *seen_release = TRUE;
      break;
    case MotionNotify:
      moveresize_process_time = event->xmotion.time;
      break;
    default:
      break;
    }

  return False;
}

static gboolean
moveresize_lookahead (XEvent *event)
{
  XEvent tmp_event;
  gboolean seen_release = FALSE;

  if (moveresize_process_time)
    {
      if (event->xmotion.time == moveresize_process_time)
	{
	  moveresize_process_time = 0;
	  return TRUE;
	}
      else
	return FALSE;
    }

  XCheckIfEvent (gdk_display, &tmp_event,
		 lookahead_motion_predicate, (XPointer)&seen_release);

  return moveresize_process_time == 0;
}
	
void
_gdk_moveresize_handle_event (XEvent *event)
{
  guint button_mask = 0;
  GdkWindowObject *window_private = (GdkWindowObject *) _gdk_moveresize_window;
  
  button_mask = GDK_BUTTON1_MASK << (moveresize_button - 1);
  
  switch (event->xany.type)
    {
    case MotionNotify:
      if (window_private->resize_count > 0)
	{
	  if (moveresize_pending_event)
	    *moveresize_pending_event = *event;
	  else
	    moveresize_pending_event = g_memdup (event, sizeof (XEvent));

	  break;
	}
      if (!moveresize_lookahead (event))
	break;
      
      update_pos (event->xmotion.x_root,
                  event->xmotion.y_root);
      
      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->xmotion.state & button_mask) == 0)
        finish_drag ();
      break;

    case ButtonRelease:
      update_pos (event->xbutton.x_root,
                  event->xbutton.y_root);
      
      if (event->xbutton.button == moveresize_button)
        finish_drag ();
      break;
    }
}

void
_gdk_moveresize_configure_done (void)
{
  XEvent *tmp_event;
  
  if (moveresize_pending_event)
    {
      tmp_event = moveresize_pending_event;
      moveresize_pending_event = NULL;
      _gdk_moveresize_handle_event (tmp_event);
      g_free (tmp_event);
    }
}

static void
create_moveresize_window (guint32 timestamp)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGrabStatus status;

  g_assert (moveresize_emulation_window == NULL);
  
  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  moveresize_emulation_window =
    gdk_window_new (NULL, &attributes, attributes_mask);

  gdk_window_show (moveresize_emulation_window);

  status = gdk_pointer_grab (moveresize_emulation_window,
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
      gdk_window_destroy (moveresize_emulation_window);
      moveresize_emulation_window = NULL;
    }

  moveresize_process_time = 0;
}

static void
emulate_resize_drag (GdkWindow     *window,
                     GdkWindowEdge  edge,
                     gint           button,
                     gint           root_x,
                     gint           root_y,
                     guint32        timestamp)
{
  is_resize = TRUE;
  moveresize_button = button;
  resize_edge = edge;
  moveresize_x = root_x;
  moveresize_y = root_y;
  _gdk_moveresize_window = GDK_WINDOW (g_object_ref (G_OBJECT (window)));

  gdk_window_get_size (window, &moveresize_orig_width, &moveresize_orig_height);
  
  moveresize_geom_mask = 0;
  gdk_window_get_geometry_hints (window,
                                 &moveresize_geometry,
                                 &moveresize_geom_mask);
  
  create_moveresize_window (timestamp);
}

static void
emulate_move_drag (GdkWindow     *window,
                   gint           button,
                   gint           root_x,
                   gint           root_y,
                   guint32        timestamp)
{
  is_resize = FALSE;
  moveresize_button = button;
  moveresize_x = root_x;
  moveresize_y = root_y;
  _gdk_moveresize_window = GDK_WINDOW (g_object_ref (G_OBJECT (window)));

  gdk_window_get_deskrelative_origin (_gdk_moveresize_window,
                                      &moveresize_orig_x,
                                      &moveresize_orig_y);
  
  create_moveresize_window (timestamp);
}

/**
 * gdk_window_begin_resize_drag:
 * @window: a #GdkWindow
 * @edge: the edge or corner from which the drag is started
 * @button: the button being used to drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use gdk_event_get_time())
 *
 * Begins a window resize operation (for a toplevel window).
 * You might use this function to implement a "window resize grip," for
 * example; in fact #GtkStatusbar uses it. The function works best
 * with window managers that support the Extended Window Manager Hints spec
 * (see http://www.freedesktop.org), but has a fallback implementation
 * for other window managers.
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
  g_return_if_fail (moveresize_emulation_window == NULL);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (gdk_net_wm_supports (gdk_atom_intern ("_NET_WM_MOVERESIZE", FALSE)))
    wmspec_resize_drag (window, edge, button, root_x, root_y, timestamp);
  else
    emulate_resize_drag (window, edge, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_begin_move_drag:
 * @window: a #GdkWindow
 * @button: the button being used to drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag
 *
 * Begins a window move operation (for a toplevel window).  You might
 * use this function to implement a "window move grip," for
 * example. The function works best with window managers that support
 * the Extended Window Manager Hints spec (see
 * http://www.freedesktop.org), but has a fallback implementation for
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
  g_return_if_fail (moveresize_emulation_window == NULL);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (gdk_net_wm_supports (gdk_atom_intern ("_NET_WM_MOVERESIZE", FALSE)))
    wmspec_moveresize (window, _NET_WM_MOVERESIZE_MOVE,
                       root_x, root_y, timestamp);
  else
    emulate_move_drag (window, button, root_x, root_y, timestamp);
}
