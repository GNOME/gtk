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

const int gdk_event_mask_table[21] =
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
const int gdk_nevent_masks = sizeof (gdk_event_mask_table) / sizeof (int);

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

  if (!GDK_WINDOW_DESTROYED (wrapper))
    {
      gdk_xid_table_remove (draw_impl->xid);
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

  g_assert (gdk_parent_root == NULL);
  
  XGetGeometry (gdk_display, gdk_root_window, &gdk_root_window,
		&x, &y, &width, &height, &border_width, &depth);
  XGetWindowAttributes (gdk_display, gdk_root_window, &xattributes);

  gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)gdk_parent_root;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  
  draw_impl->xdisplay = gdk_display;
  draw_impl->xid = gdk_root_window;
  draw_impl->wrapper = GDK_DRAWABLE (private);
  
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = depth;
  impl->width = width;
  impl->height = height;
  
  gdk_xid_table_insert (&gdk_root_window, gdk_parent_root);
}

static GdkAtom wm_client_leader_atom = GDK_NONE;

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowObject *parent_private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  
  GdkVisual *visual;
  Window xparent;
  Visual *xvisual;

  XSetWindowAttributes xattributes;
  long xattributes_mask;
  XSizeHints size_hints;
  XWMHints wm_hints;
  XClassHint *class_hint;
  int x, y, depth;
  
  unsigned int class;
  char *title;
  int i;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent)
    parent = gdk_parent_root;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  parent_private = (GdkWindowObject*) parent;
  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;
  
  xparent = GDK_WINDOW_XID (parent);
  
  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  
  draw_impl->xdisplay = GDK_WINDOW_XDISPLAY (parent); 
  
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
  for (i = 0; i < gdk_nevent_masks; i++)
    {
      if (attributes->event_mask & (1 << (i + 1)))
	xattributes.event_mask |= gdk_event_mask_table[i];
    }
  
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

  if (parent_private && parent_private->guffaw_gravity)
    {
      xattributes.win_gravity = StaticGravity;
      xattributes_mask |= CWWinGravity;
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
      
      private->bg_color.pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes.background_pixel = private->bg_color.pixel;

      private->bg_pixmap = NULL;
      
      xattributes.border_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes_mask |= CWBorderPixel | CWBackPixel;

      if (private->guffaw_gravity)
	xattributes.bit_gravity = StaticGravity;
      else
	xattributes.bit_gravity = NorthWestGravity;
      
      xattributes_mask |= CWBitGravity;
  
      switch (private->window_type)
	{
	case GDK_WINDOW_TOPLEVEL:
	  xattributes.colormap = GDK_COLORMAP_XCOLORMAP (draw_impl->colormap);
	  xattributes_mask |= CWColormap;
	  
	  xparent = gdk_root_window;
	  break;
	  
	case GDK_WINDOW_CHILD:
	  xattributes.colormap = GDK_COLORMAP_XCOLORMAP (draw_impl->colormap);
	  xattributes_mask |= CWColormap;
	  break;
	  
	case GDK_WINDOW_DIALOG:
	  xattributes.colormap = GDK_COLORMAP_XCOLORMAP (draw_impl->colormap);
	  xattributes_mask |= CWColormap;
	  
	  xparent = gdk_root_window;
	  break;
	  
	case GDK_WINDOW_TEMP:
	  xattributes.colormap = GDK_COLORMAP_XCOLORMAP (draw_impl->colormap);
	  xattributes_mask |= CWColormap;
	  
	  xparent = gdk_root_window;
	  
	  xattributes.save_under = True;
	  xattributes.override_redirect = True;
	  xattributes.cursor = None;
	  xattributes_mask |= CWSaveUnder | CWOverrideRedirect;
	  break;
	case GDK_WINDOW_ROOT:
	  g_error ("cannot make windows of type GDK_WINDOW_ROOT");
	  break;
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

  draw_impl->xid = XCreateWindow (GDK_WINDOW_XDISPLAY (parent),
                                  xparent,
                                  impl->position_info.x, impl->position_info.y,
                                  impl->position_info.width, impl->position_info.height,
                                  0, depth, class, xvisual,
                                  xattributes_mask, &xattributes);

  gdk_drawable_ref (window);
  gdk_xid_table_insert (&GDK_WINDOW_XID (window), window);
  
  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);
  
  switch (GDK_WINDOW_TYPE (private))
    {
    case GDK_WINDOW_DIALOG:
      XSetTransientForHint (GDK_WINDOW_XDISPLAY (window),
			    GDK_WINDOW_XID (window),
			    xparent);
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      XSetWMProtocols (GDK_WINDOW_XDISPLAY (window),
		       GDK_WINDOW_XID (window),
		       gdk_wm_window_protocols, 3);
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
  
  size_hints.flags = PSize;
  size_hints.width = impl->width;
  size_hints.height = impl->height;
  
  wm_hints.flags = InputHint | StateHint | WindowGroupHint;
  wm_hints.window_group = gdk_leader_window;
  wm_hints.input = True;
  wm_hints.initial_state = NormalState;
  
  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &size_hints);
  
  XSetWMHints (GDK_WINDOW_XDISPLAY (window),
	       GDK_WINDOW_XID (window),
	       &wm_hints);
  
  if (!wm_client_leader_atom)
    wm_client_leader_atom = gdk_atom_intern ("WM_CLIENT_LEADER", FALSE);
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
	   	   wm_client_leader_atom,
		   XA_WINDOW, 32, PropModeReplace,
		   (guchar*) &gdk_leader_window, 1);
  
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
      XSetClassHint (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     class_hint);
      XFree (class_hint);
    }
  
  return window;
}

GdkWindow *
gdk_window_foreign_new (GdkNativeWindow anid)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowObject *parent_private;
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
  
  parent_private = (GdkWindowObject *)private->parent;
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);

  draw_impl->xid = anid;
  draw_impl->xdisplay = gdk_display;

  private->x = attrs.x;
  private->y = attrs.y;
  impl->width = attrs.width;
  impl->height = attrs.height;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;

  if (attrs.map_state == IsUnmapped)
    private->state = GDK_WINDOW_STATE_WITHDRAWN;
  else
    private->state = 0;

  private->depth = attrs.depth;
  
  gdk_drawable_ref (window);
  gdk_xid_table_insert (&GDK_WINDOW_XID (window), window);
  
  return window;
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
	  xevent.message_type = gdk_wm_protocols;
	  xevent.format = 32;
	  xevent.data.l[0] = gdk_wm_delete_window;
	  xevent.data.l[1] = CurrentTime;
	  
	  XSendEvent (GDK_WINDOW_XDISPLAY (window),
		      GDK_WINDOW_XID (window),
		      False, 0, (XEvent *)&xevent);
	  gdk_flush ();
	  gdk_error_trap_pop ();
	}
    }
  else if (!recursing && !foreign_destroy)
    XDestroyWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %#lx unexpectedly destroyed", GDK_WINDOW_XID (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  gdk_xid_table_remove (GDK_WINDOW_XID (window));
  gdk_drawable_unref (window);
}

static void
set_initial_hints (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkAtom atoms[5];
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
      atoms[i] = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE);
      ++i;
      atoms[i] = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE);
      ++i;
    }

  if (private->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[i] = gdk_atom_intern ("_NET_WM_STATE_STICKY", FALSE);
      ++i;
    }

  if (i > 0)
    {
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       gdk_atom_intern ("_NET_WM_STATE", FALSE),
                       XA_ATOM, 32, PropModeReplace,
                       (guchar*) atoms, i);
    }

  if (private->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[0] = 0xFFFFFFFF;
      XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       gdk_atom_intern ("_NET_WM_DESKTOP", FALSE),
                       XA_CARDINAL, 32, PropModeReplace,
                       (guchar*) atoms, 1);
    }
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
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

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);

  private = (GdkWindowObject*) window;

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
	  XResizeWindow (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 width, height);
	  private->resize_count += 1;
	}
    }
}

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
	  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
			     GDK_WINDOW_XID (window),
			     x, y, width, height);
	  private->resize_count += 1;
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent != NULL);
  g_return_if_fail (GDK_IS_WINDOW (new_parent));
  
  if (!new_parent)
    new_parent = gdk_parent_root;
  
  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject*)window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  
  if (!GDK_WINDOW_DESTROYED (window) && !GDK_WINDOW_DESTROYED (new_parent))
    XReparentWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     GDK_WINDOW_XID (new_parent),
		     x, y);
  
  window_private->parent = (GdkWindowObject *)new_parent;
  
  if (old_parent_private)
    old_parent_private->children = g_list_remove (old_parent_private->children, window);
  
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

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XRaiseWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    XLowerWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

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
      xev.xclient.message_type = gdk_atom_intern ("_NET_ACTIVE_WINDOW", FALSE);
      xev.xclient.format = 32;
      xev.xclient.data.l[0] = 0;
      
      XSendEvent (gdk_display, gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
    }
  else
    {
      XSetInputFocus (GDK_DISPLAY (),
                      GDK_WINDOW_XWINDOW (window),
                      RevertToNone,
                      timestamp);
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
  
  /* FIXME: Would it be better to delete this property of
   *        flags == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &size_hints);
}

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
      size_hints.width_inc = geometry->win_gravity;
    }
  
  /* FIXME: Would it be better to delete this property of
   *        geom_mask == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &size_hints);
}

static gboolean
utf8_is_latin1 (const gchar *str)
{
  const char *p = str;

  while (*p)
    {
      gunichar ch = g_utf8_get_char (p);

      if (ch >= 0xff)
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
		   GdkAtom      property,
		   const gchar *utf8_str)
{
  guchar *prop_text = NULL;
  GdkAtom prop_type;
  gint prop_length;
  gint prop_format;
  
  if (utf8_is_latin1 (utf8_str))
    {
      prop_type = GDK_TARGET_STRING;
      prop_text = gdk_utf8_to_string_target (utf8_str);
      prop_length = strlen (prop_text);
      prop_format = 8;
    }
  else
    {
      gdk_utf8_to_compound_text (utf8_str, &prop_type, &prop_format,
				 &prop_text, &prop_length);
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

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   gdk_atom_intern ("_NET_WM_NAME", FALSE),
		   gdk_atom_intern ("UTF8_STRING", FALSE), 8,
		   PropModeReplace, title,
		   strlen (title));

  set_text_property (window, gdk_atom_intern ("WM_NAME", FALSE), title);
  if (!gdk_window_icon_name_set (window))
    set_text_property (window, gdk_atom_intern ("WM_ICON_NAME", FALSE), title);
}

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
			 gdk_atom_intern ("WM_WINDOW_ROLE", FALSE), XA_STRING,
			 8, PropModeReplace, role, strlen (role));
      else
	XDeleteProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
			 gdk_atom_intern ("WM_WINDOW_ROLE", FALSE));
    }
}

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
    {
      gdk_pixmap_unref (private->bg_pixmap);
      private->bg_pixmap = NULL;
    }
}

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
    window = gdk_parent_root;
  
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
					  gdk_root_window,
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
	atom = gdk_atom_intern ("ENLIGHTENMENT_DESKTOP", FALSE);
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

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkWindowObject *private;
  Window xwindow;
  Window xparent;
  Window root;
  Window *children;
  unsigned int nchildren;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  if (x)
    *x = 0;
  if (y)
    *y = 0;

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
	  if (x)
	    *x = wx;
	  if (y)
	    *y = wy;
	}
    }
}

GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
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
    window = gdk_parent_root;
  
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
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
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
  
  XGrabServer (xdisplay);
  while (xwindow)
    {
      xwindow_last = xwindow;
      XQueryPointer (xdisplay, xwindow,
		     &root, &xwindow,
		     &rootx, &rooty,
		     &winx, &winy,
		     &xmask);
    }
  XUngrabServer (xdisplay);
  
  window = gdk_window_lookup (xwindow_last);
  
  if (win_x)
    *win_x = window ? winx : -1;
  if (win_y)
    *win_y = window ? winy : -1;
  
  return window;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  XWindowAttributes attrs;
  GdkEventMask event_mask;
  int i;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    {
      XGetWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			    GDK_WINDOW_XID (window), 
			    &attrs);
      
      event_mask = 0;
      for (i = 0; i < gdk_nevent_masks; i++)
	{
	  if (attrs.your_event_mask & gdk_event_mask_table[i])
	    event_mask |= 1 << (i + 1);
	}
  
      return event_mask;
    }
}

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
      xevent_mask = StructureNotifyMask;
      for (i = 0; i < gdk_nevent_masks; i++)
	{
	  if (event_mask & (1 << (i + 1)))
	    xevent_mask |= gdk_event_mask_table[i];
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

  set_text_property (window, gdk_atom_intern ("WM_ICON_NAME", FALSE), name);
}

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
      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_atom_intern ("_NET_WM_STATE", FALSE);
      xev.xclient.format = 32;

      xev.xclient.data.l[0] = gdk_atom_intern ("_NET_WM_STATE_ADD", FALSE);
      xev.xclient.data.l[1] = gdk_atom_intern ("_NET_WM_STATE_STICKY", FALSE);
      xev.xclient.data.l[2] = 0;
      
      XSendEvent (gdk_display, gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);

      /* Request desktop 0xFFFFFFFF */
      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_atom_intern ("_NET_WM_DESKTOP", FALSE);
      xev.xclient.format = 32;

      xev.xclient.data.l[0] = 0xFFFFFFFF;
      
      XSendEvent (gdk_display, gdk_root_window, False,
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
      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_atom_intern ("_NET_WM_STATE", FALSE);
      xev.xclient.format = 32;

      xev.xclient.data.l[0] = gdk_atom_intern ("_NET_WM_STATE_REMOVE", FALSE);
      xev.xclient.data.l[1] = gdk_atom_intern ("_NET_WM_STATE_STICKY", FALSE);
      xev.xclient.data.l[2] = 0;
      
      XSendEvent (gdk_display, gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);

      /* Get current desktop, then set it; this is a race, but not
       * one that matters much in practice.
       */
      XGetWindowProperty (gdk_display, gdk_root_window,
                          gdk_atom_intern ("_NET_CURRENT_DESKTOP", FALSE),
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
          xev.xclient.message_type = gdk_atom_intern ("_NET_WM_DESKTOP", FALSE);
          xev.xclient.format = 32;

          xev.xclient.data.l[0] = *current_desktop;
      
          XSendEvent (gdk_display, gdk_root_window, False,
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

void
gdk_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_atom_intern ("_NET_WM_STATE", FALSE);
      xev.xclient.format = 32;

      xev.xclient.data.l[0] = gdk_atom_intern ("_NET_WM_STATE_ADD", FALSE);
      xev.xclient.data.l[1] = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE);
      xev.xclient.data.l[2] = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE);
      
      XSendEvent (gdk_display, gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_MAXIMIZED);
    }
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.display = gdk_display;
      xev.xclient.message_type = gdk_atom_intern ("_NET_WM_STATE", FALSE);
      xev.xclient.format = 32;

      xev.xclient.data.l[0] = gdk_atom_intern ("_NET_WM_STATE_REMOVE", FALSE);
      xev.xclient.data.l[1] = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE);
      xev.xclient.data.l[2] = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE);
      
      XSendEvent (gdk_display, gdk_root_window, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_MAXIMIZED,
                                   0);
    }
}

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
 * @Returns: TRUE if the window has decorations set, FALSE otherwise.
 *
 * Returns the decorations set on the GdkWindow with #gdk_window_set_decorations
 * 
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

  window = gdk_parent_root;
  xdisplay = GDK_WINDOW_XDISPLAY (window);
  root = GDK_WINDOW_XID (window);
  num = g_list_length (excludes);
  
  XGrabServer (xdisplay);
  if (!XQueryTree (xdisplay, root, &root_win, &parent_win, &list, &num))
    {
      XUngrabServer (xdisplay);
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
		  XUngrabServer (xdisplay);
		  return child;
		}
	    }
	  else
	    {
	      XFree (list);
	      XUngrabServer (xdisplay);
	      return child;
	    }
	} while (--i > 0);
      XFree (list);
    }
  XUngrabServer (xdisplay);
  return root;
}

