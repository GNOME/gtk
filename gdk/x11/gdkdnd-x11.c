/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "gdk.h"          /* For gdk_flush() */
#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

typedef struct _GdkDragContextPrivateX11 GdkDragContextPrivateX11;

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

typedef struct {
  guint32 xid;
  gint x, y, width, height;
  gboolean mapped;
} GdkCacheChild;

typedef struct {
  GList *children;
  GHashTable *child_hash;
  guint old_event_mask;
  GdkDisplay *display;
} GdkWindowCache;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivateX11 {
  GdkDragContext context;

  GdkAtom motif_selection;
  GdkAtom xdnd_selection;
  guint   ref_count;

  guint16 last_x;		/* Coordinates from last event */
  guint16 last_y;
  GdkDragAction old_action;	  /* The last action we sent to the source */
  GdkDragAction old_actions;	  /* The last actions we sent to the source */
  GdkDragAction xdnd_actions;     /* What is currently set in XdndActionList */

  Window dest_xid;              /* The last window we looked up */
  Window drop_xid;            /* The (non-proxied) window that is receiving drops */
  guint xdnd_targets_set : 1;   /* Whether we've already set XdndTypeList */
  guint xdnd_actions_set : 1;   /* Whether we've already set XdndActionList */
  guint xdnd_have_actions : 1; /* Whether an XdndActionList was provided */
  guint motif_targets_set : 1;  /* Whether we've already set motif initiator info */
  guint drag_status : 4;	/* current status of drag */

  GdkWindowCache *window_cache;
};

#define PRIVATE_DATA(context) ((GdkDragContextPrivateX11 *) GDK_DRAG_CONTEXT (context)->windowing_data)
/* get the X impl of the Display dnd default screen */
#define GDK_DISPLAY_DND_SCREEN(dpy) (GDK_SCREEN_IMPL_X11(GDK_DISPLAY_IMPL_X11(dpy)->dnd_default_screen))

/* Forward declarations */

static void gdk_window_cache_destroy (GdkWindowCache *cache);

static void motif_read_target_table_for_display (GdkDisplay *dpy);

static GdkFilterReturn motif_dnd_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

static GdkFilterReturn xdnd_enter_filter (GdkXEvent *xev,
					  GdkEvent  *event,
					  gpointer   data);

static GdkFilterReturn xdnd_leave_filter (GdkXEvent *xev,
					  GdkEvent  *event,
					  gpointer   data);

static GdkFilterReturn xdnd_position_filter (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);

static GdkFilterReturn xdnd_status_filter (GdkXEvent *xev,
					   GdkEvent  *event,
					   gpointer   data);

static GdkFilterReturn xdnd_finished_filter (GdkXEvent *xev,
					    GdkEvent  *event,
					    gpointer   data);

static GdkFilterReturn xdnd_drop_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

static void   xdnd_manage_source_filter (GdkDragContext *context,
					 GdkWindow      *window,
					 gboolean        add_filter);

static void gdk_drag_context_init       (GdkDragContext      *dragcontext);
static void gdk_drag_context_class_init (GdkDragContextClass *klass);
static void gdk_drag_context_finalize   (GObject              *object);

static gpointer parent_class = NULL;
static GList  * contexts;

GType
gdk_drag_context_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDragContextClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drag_context_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDragContext),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_drag_context_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDragContext",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  GdkDragContextPrivateX11 *private;

  private = g_new0 (GdkDragContextPrivateX11, 1);

  dragcontext->windowing_data = private;

  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drag_context_finalize;
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  
  g_list_free (context->targets);

  if (context->source_window)
    {
      if ((context->protocol == GDK_DRAG_PROTO_XDND) &&
          !context->is_source)
        xdnd_manage_source_filter (context, context->source_window, FALSE);
      
      gdk_window_unref (context->source_window);
    }
  
  if (context->dest_window)
    gdk_window_unref (context->dest_window);
  
  if (private->window_cache)
    gdk_window_cache_destroy (private->window_cache);
  
  contexts = g_list_remove (contexts, context);

  g_free (private);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDragContext *
gdk_drag_context_new        (void)
{
  return g_object_new (gdk_drag_context_get_type (), NULL);
}

void            
gdk_drag_context_ref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_ref (G_OBJECT (context));
}

void            
gdk_drag_context_unref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_unref (G_OBJECT (context));
}

static GdkDragContext *
gdk_drag_context_find (gboolean is_source,
		       Window   source_xid,
		       Window   dest_xid)
{
  GList *tmp_list = contexts;
  GdkDragContext *context;
  GdkDragContextPrivateX11 *private;
  Window context_dest_xid;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;
      private = PRIVATE_DATA (context);

      context_dest_xid = context->dest_window ? 
                            (private->drop_xid ?
                              private->drop_xid :
                              GDK_DRAWABLE_XID (context->dest_window)) :
	                     None;

      if ((!context->is_source == !is_source) &&
	  ((source_xid == None) || (context->source_window &&
	    (GDK_DRAWABLE_XID (context->source_window) == source_xid))) &&
	  ((dest_xid == None) || (context_dest_xid == dest_xid)))
	return context;
      
      tmp_list = tmp_list->next;
    }
  
  return NULL;
}

/* Utility functions */

static void
gdk_window_cache_add (GdkWindowCache *cache,
		      guint32 xid,
		      gint x, gint y, gint width, gint height, 
		      gboolean mapped)
{
  GdkCacheChild *child = g_new (GdkCacheChild, 1);

  child->xid = xid;
  child->x = x;
  child->y = y;
  child->width = width;
  child->height = height;
  child->mapped = mapped;

  cache->children = g_list_prepend (cache->children, child);
  g_hash_table_insert (cache->child_hash, GUINT_TO_POINTER (xid), 
		       cache->children);
}

static GdkFilterReturn
gdk_window_cache_filter (GdkXEvent *xev,
			 GdkEvent  *event,
			 gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkWindowCache *cache = data;

  switch (xevent->type)
    {
    case CirculateNotify:
      break;
    case ConfigureNotify:
      {
	XConfigureEvent *xce = &xevent->xconfigure;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xce->window));
	if (node) 
	  {
	    GdkCacheChild *child = node->data;
	    child->x = xce->x; 
	    child->y = xce->y;
	    child->width = xce->width; 
	    child->height = xce->height;
	    if (xce->above == None && (node->next))
	      {
		GList *last = g_list_last (cache->children);
		cache->children = g_list_remove_link (cache->children, node);
		last->next = node;
		node->next = NULL;
		node->prev = last;
	      }
	    else
	      {
		GList *above_node = g_hash_table_lookup (cache->child_hash, 
							 GUINT_TO_POINTER (xce->above));
		if (above_node && node->prev != above_node)
		  {
		    cache->children = g_list_remove_link (cache->children, node);
		    node->next = above_node->next;
		    if (node->next)
		      node->next->prev = node;
		    node->prev = above_node;
		    above_node->next = node;
		  }
	      }
	  }
	break;
      }
    case CreateNotify:
      {
	XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

	if (!g_hash_table_lookup (cache->child_hash, 
				  GUINT_TO_POINTER (xcwe->window))) 
	  gdk_window_cache_add (cache, xcwe->window, 
				xcwe->x, xcwe->y, xcwe->width, xcwe->height,
				FALSE);
	break;
      }
    case DestroyNotify:
      {
	XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xdwe->window));
	if (node) 
	  {
	    g_hash_table_remove (cache->child_hash,
				 GUINT_TO_POINTER (xdwe->window));
	    cache->children = g_list_remove_link (cache->children, node);
	    g_free (node->data);
	    g_list_free_1 (node);
	  }
	break;
      }
    case MapNotify:
      {
	XMapEvent *xme = &xevent->xmap;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xme->window));
	if (node) 
	  {
	    GdkCacheChild *child = node->data;
	    child->mapped = TRUE;
	  }
	break;
      }
    case ReparentNotify:
      break;
    case UnmapNotify:
      {
	XMapEvent *xume = &xevent->xmap;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xume->window));
	if (node) 
	  {
	    GdkCacheChild *child = node->data;
	    child->mapped = FALSE;
	  }
	break;
      }
    default:
      return GDK_FILTER_CONTINUE;
    }
  return GDK_FILTER_REMOVE;
}

static GdkWindowCache *
gdk_window_cache_new_for_display (GdkDisplay *dpy){
  XWindowAttributes xwa;
  Window root, parent, *children;
  unsigned int nchildren;
  int i;
  
  gint old_warnings = gdk_error_warnings;
  GdkScreenImplX11 *scr_impl = GDK_DISPLAY_DND_SCREEN(dpy);
  
  GdkWindowCache *result = g_new (GdkWindowCache, 1);

  result->children = NULL;
  result->child_hash = g_hash_table_new (g_direct_hash, NULL);
  result->display = dpy;

  XGetWindowAttributes (scr_impl->xdisplay,scr_impl->root_window,&xwa);
  result->old_event_mask = xwa.your_event_mask;
  XSelectInput (scr_impl->xdisplay,scr_impl->root_window,
		result->old_event_mask | SubstructureNotifyMask);
  gdk_window_add_filter (scr_impl->parent_root, 
			 gdk_window_cache_filter, result);
  
  gdk_error_code = 0;
  gdk_error_warnings = 0;

  if (XQueryTree(scr_impl->xdisplay,scr_impl->root_window, 
		 &root, &parent, &children, &nchildren) == 0)
    return result;
  
  for (i = 0; i < nchildren ; i++)
    {
      XGetWindowAttributes (scr_impl->xdisplay, children[i], &xwa);

      gdk_window_cache_add (result, children[i],
			    xwa.x, xwa.y, xwa.width, xwa.height,
			    xwa.map_state != IsUnmapped);

      if (gdk_error_code)
	gdk_error_code = 0;
      else
	{
	  gdk_window_cache_add (result, children[i],
				xwa.x, xwa.y, xwa.width, xwa.height,
				(xwa.map_state != IsUnmapped));
	}
    }

  XFree (children);

  gdk_error_warnings = old_warnings;

  return result;
}

static void
gdk_window_cache_destroy (GdkWindowCache *cache)
{
  XSelectInput (GDK_DISPLAY_DND_SCREEN(cache->display)->xdisplay,
		GDK_DISPLAY_DND_SCREEN(cache->display)->root_window, cache->old_event_mask);
  gdk_window_remove_filter (GDK_DISPLAY_DND_SCREEN(cache->display)->parent_root, 
			    gdk_window_cache_filter, cache);

  g_list_foreach (cache->children, (GFunc)g_free, NULL);
  g_list_free (cache->children);
  g_hash_table_destroy (cache->child_hash);

  g_free (cache);
}

static Window
get_client_window_at_coords_recurse_for_display (Window  win,
						 GdkDisplay *dpy,
						 gint    x,
						 gint    y){
  Window root, tmp_parent, *children;
  unsigned int nchildren;
  int i;
  Window child = None;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  
  static Atom wm_state_atom = None;

  if (!wm_state_atom)
    wm_state_atom = gdk_display_atom (dpy, "WM_STATE", FALSE);

    
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY(dpy), win, 
		      wm_state_atom, 0, 0, False, AnyPropertyType,
		      &type, &format, &nitems, &after, &data);
  
  if (gdk_error_code)
    {
      gdk_error_code = 0;

      return None;
    }

  if (type != None)
    {
      XFree (data);
      return win;
    }

#if 0
  /* This is beautiful! Damn Enlightenment and click-to-focus */
  XTranslateCoordinates (gdk_display, gdk_root_window, win,
			 x_root, y_root, &dest_x, &dest_y, &child);

  if (gdk_error_code)
    {
      gdk_error_code = 0;

      return None;
    }
  
#else
  if (XQueryTree(GDK_DISPLAY_XDISPLAY(dpy), win,
		 &root, &tmp_parent, &children, &nchildren) == 0)
    return 0;

  if (!gdk_error_code)
    {
      for (i = nchildren - 1; (i >= 0) && (child == None); i--)
	{
	  XWindowAttributes xwa;
	  
	  XGetWindowAttributes (GDK_DISPLAY_XDISPLAY(dpy), children[i], &xwa);
	  
	  if (gdk_error_code)
	    gdk_error_code = 0;
	  else if ((xwa.map_state == IsViewable) && (xwa.class == InputOutput) &&
		   (x >= xwa.x) && (x < xwa.x + (gint)xwa.width) &&
		   (y >= xwa.y) && (y < xwa.y + (gint)xwa.height))
	    {
	      x -= xwa.x;
	      y -= xwa.y;
	      child = children[i];
	    }
	}
      
      XFree (children);
    }
  else
    gdk_error_code = 0;
#endif  

  if (child)
    return get_client_window_at_coords_recurse_for_display (child, dpy, x, y);
  else
    return None;
}


static Window 
get_client_window_at_coords (GdkWindowCache *cache,
			     Window          ignore,
			     gint            x_root,
			     gint            y_root)
{
  GList *tmp_list;
  Window retval = None;

  gint old_warnings = gdk_error_warnings;
  
  gdk_error_code = 0;
  gdk_error_warnings = 0;

  tmp_list = cache->children;

  while (tmp_list && !retval)
    {
      GdkCacheChild *child = tmp_list->data;

      if ((child->xid != ignore) && (child->mapped))
	{
	  if ((x_root >= child->x) && (x_root < child->x + child->width) &&
	      (y_root >= child->y) && (y_root < child->y + child->height))
	    {
	      retval = get_client_window_at_coords_recurse_for_display (child->xid,
							    cache->display,
							    x_root - child->x, 
							    y_root - child->y);
	      if (!retval)
		retval = child->xid;
	    }
	  
	}
      tmp_list = tmp_list->next;
    }

  gdk_error_warnings = old_warnings;
  if (retval)
    return retval;
  else
    return GDK_DISPLAY_DND_SCREEN(cache->display)->root_window;
}

#if 0
static Window
get_client_window_at_coords_recurse (Window  win,
				     gint    x_root,
				     gint    y_root)
{
  Window child;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  int dest_x, dest_y;
  
  static Atom wm_state_atom = None;

  if (!wm_state_atom)
    wm_state_atom = gdk_atom_intern ("WM_STATE", FALSE);
    
  XGetWindowProperty (gdk_display, win, 
		      wm_state_atom, 0, 0, False, AnyPropertyType,
		      &type, &format, &nitems, &after, &data);
  
  if (gdk_error_code)
    {
      gdk_error_code = 0;

      return None;
    }

  if (type != None)
    {
      XFree (data);
      return win;
    }

  XTranslateCoordinates (gdk_display, gdk_root_window, win,
			 x_root, y_root, &dest_x, &dest_y, &child);

  if (gdk_error_code)
    {
      gdk_error_code = 0;

      return None;
    }

  if (child)
    return get_client_window_at_coords_recurse (child, x_root, y_root);
  else
    return None;
}

static Window 
get_client_window_at_coords (Window  ignore,
			     gint    x_root,
			     gint    y_root)
{
  Window root, parent, *children;
  unsigned int nchildren;
  int i;
  Window retval = None;
  
  gint old_warnings = gdk_error_warnings;
  
  gdk_error_code = 0;
  gdk_error_warnings = 0;

  if (XQueryTree(gdk_display, gdk_root_window, 
		 &root, &parent, &children, &nchildren) == 0)
    return 0;

  for (i = nchildren - 1; (i >= 0) && (retval == None); i--)
    {
      if (children[i] != ignore)
	{
	  XWindowAttributes xwa;

	  XGetWindowAttributes (gdk_display, children[i], &xwa);

	  if (gdk_error_code)
	    gdk_error_code = 0;
	  else if ((xwa.map_state == IsViewable) &&
		   (x_root >= xwa.x) && (x_root < xwa.x + (gint)xwa.width) &&
		   (y_root >= xwa.y) && (y_root < xwa.y + (gint)xwa.height))
	    {
	      retval = get_client_window_at_coords_recurse (children[i],
							    x_root, y_root);
	      if (!retval)
		retval = children[i];
	    }
	}
    }

  XFree (children);

  gdk_error_warnings = old_warnings;
  if (retval)
    return retval;
  else
    return gdk_root_window;
}
#endif

/*************************************************************
 ***************************** MOTIF *************************
 *************************************************************/

/* values used in the message type for Motif DND */
enum {
    XmTOP_LEVEL_ENTER,
    XmTOP_LEVEL_LEAVE,
    XmDRAG_MOTION,
    XmDROP_SITE_ENTER,
    XmDROP_SITE_LEAVE,
    XmDROP_START,
    XmDROP_FINISH,
    XmDRAG_DROP_FINISH,
    XmOPERATION_CHANGED
};

/* Values used to specify type of protocol to use */
enum {
    XmDRAG_NONE,
    XmDRAG_DROP_ONLY,
    XmDRAG_PREFER_PREREGISTER,
    XmDRAG_PREREGISTER,
    XmDRAG_PREFER_DYNAMIC,
    XmDRAG_DYNAMIC,
    XmDRAG_PREFER_RECEIVER
};

/* Operation codes */
enum {
  XmDROP_NOOP,
  XmDROP_MOVE = 0x01,
  XmDROP_COPY = 0x02,
  XmDROP_LINK = 0x04
};

/* Drop site status */
enum {
  XmNO_DROP_SITE = 0x01,
  XmDROP_SITE_INVALID = 0x02,
  XmDROP_SITE_VALID = 0x03
};

/* completion status */
enum {
  XmDROP,
  XmDROP_HELP,
  XmDROP_CANCEL,
  XmDROP_INTERRUPT
};

/* Byte swapping routines. The motif specification leaves it
 * up to us to save a few bytes in the client messages
 */
static gchar local_byte_order = '\0';

#ifdef G_ENABLE_DEBUG
static void
print_target_list (GList *targets, GdkDisplay *display)
{
  while (targets)
    {
      gchar *name = gdk_display_atom_name (display, GPOINTER_TO_INT (targets->data));

      g_message ("\t%s", name);
      g_free (name);
      targets = targets->next;
    }
}
#endif /* G_ENABLE_DEBUG */

static void
init_byte_order (void)
{
  guint32 myint = 0x01020304;
  local_byte_order = (*(gchar *)&myint == 1) ? 'B' : 'l';
}

static guint16
card16_to_host (guint16 x, gchar byte_order) {
  if (byte_order == local_byte_order)
    return x;
  else
    return (x << 8) | (x >> 8);
}

static guint32
card32_to_host (guint32 x, gchar byte_order) {
  if (byte_order == local_byte_order)
    return x;
  else
    return (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
}

/* Motif packs together fields of varying length into the
 * client message. We can't rely on accessing these
 * through data.s[], data.l[], etc, because on some architectures
 * (i.e., Alpha) these won't be valid for format == 8. 
 */

#define MOTIF_XCLIENT_BYTE(xevent,i) \
  (xevent)->xclient.data.b[i]
#define MOTIF_XCLIENT_SHORT(xevent,i) \
  ((gint16 *)&((xevent)->xclient.data.b[0]))[i]
#define MOTIF_XCLIENT_LONG(xevent,i) \
  ((gint32 *)&((xevent)->xclient.data.b[0]))[i]

#define MOTIF_UNPACK_BYTE(xevent,i) MOTIF_XCLIENT_BYTE(xevent,i)
#define MOTIF_UNPACK_SHORT(xevent,i) \
  card16_to_host (MOTIF_XCLIENT_SHORT(xevent,i), MOTIF_XCLIENT_BYTE(xevent, 1))
#define MOTIF_UNPACK_LONG(xevent,i) \
  card32_to_host (MOTIF_XCLIENT_LONG(xevent,i), MOTIF_XCLIENT_BYTE(xevent, 1))

/***** Dest side ***********/

/* Property placed on source windows */
typedef struct _MotifDragInitiatorInfo {
  guint8 byte_order;
  guint8 protocol_version;
  guint16 targets_index;
  guint32 selection_atom;
} MotifDragInitiatorInfo;

/* Header for target table on the drag window */
typedef struct _MotifTargetTableHeader {
  guchar byte_order;
  guchar protocol_version;
  guint16 n_lists;
  guint32 total_size;
} MotifTargetTableHeader;

/* Property placed on target windows */
typedef struct _MotifDragReceiverInfo {
  guint8 byte_order;
  guint8 protocol_version;
  guint8 protocol_style;
  guint8 pad;
  guint32 proxy_window;
  guint16 num_drop_sites;
  guint16 padding;
  guint32 total_size;
} MotifDragReceiverInfo;

/* Target table handling */

static GdkFilterReturn
motif_drag_window_filter (GdkXEvent *xevent,
			  GdkEvent  *event,
			  gpointer data)
{
  XEvent *xev = (XEvent *)xevent;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_WINDOW_DISPLAY(event->any.window)); 

  switch (xev->xany.type)
    {
    case DestroyNotify:
      dpy_impl->motif_drag_window = None;
      dpy_impl->motif_drag_gdk_window = NULL;
      break;
    case PropertyNotify:
      if (dpy_impl->motif_target_lists &&
	  dpy_impl->motif_drag_targets_atom &&
	  (xev->xproperty.atom == dpy_impl->motif_drag_targets_atom))
	motif_read_target_table_for_display(GDK_WINDOW_DISPLAY(event->any.window));
      break;
    }
  return GDK_FILTER_REMOVE;
}

static Window
motif_lookup_drag_window_for_display (GdkDisplay *dpy){
  Window retval = None;
  gulong bytes_after, nitems;
  GdkAtom type;
  gint format;
  guchar *data;
  GdkScreenImplX11 *scr_impl = GDK_DISPLAY_DND_SCREEN(dpy);

  XGetWindowProperty (scr_impl->xdisplay,
		      scr_impl->root_window, 
		      GDK_DISPLAY_IMPL_X11(dpy)->motif_drag_window_atom,
		      0, 1, FALSE,
		      XA_WINDOW, &type, &format, &nitems, &bytes_after,
		      &data);
  
  if ((format == 32) && (nitems == 1) && (bytes_after == 0))
    {
      retval = *(Window *)data;
      GDK_NOTE(DND, 
	       g_message ("Found drag window %#lx\n",
		       GDK_DISPLAY_IMPL_X11(dpy)->motif_drag_window));
    }

  if (type != None)
    XFree (data);

  return retval;
}
/* Finds the window where global Motif drag information is stored.
 * If it doesn't exist and 'create' is TRUE, create one.
 */

static Window 
motif_find_drag_window_for_display (gboolean create, GdkDisplay *dpy)
{
  GdkScreenImplX11 * scr_impl = GDK_DISPLAY_DND_SCREEN(dpy);
  GdkDisplayImplX11 * dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  if (!dpy_impl->motif_drag_window)
    {
      if (!dpy_impl->motif_drag_window_atom)
	dpy_impl->motif_drag_window_atom = 
		gdk_display_atom (dpy, "_MOTIF_DRAG_WINDOW", TRUE);


      dpy_impl->motif_drag_window = motif_lookup_drag_window_for_display (dpy);
      
      if (!dpy_impl->motif_drag_window && create)
	{
	  /* Create a persistant window. (Copied from LessTif) */
	  
	  Display *display;
	  XSetWindowAttributes attr;
	  display = XOpenDisplay (gdk_x11_display_impl_get_display_name(dpy));
	  XSetCloseDownMode (dpy_impl->xdisplay, RetainPermanent);

	  XGrabServer (dpy_impl->xdisplay);
	  
	   dpy_impl->motif_drag_window = motif_lookup_drag_window_for_display (dpy);

	  if (!dpy_impl->motif_drag_window)
	    {
	      attr.override_redirect = True;
	      attr.event_mask = PropertyChangeMask;
	      
	       dpy_impl->motif_drag_window = 
		XCreateWindow( dpy_impl->xdisplay, scr_impl->root_window,
			      -100, -100, 10, 10, 0, 0,
			      InputOnly, CopyFromParent,
			      (CWOverrideRedirect | CWEventMask), &attr);
	      
	      GDK_NOTE (DND,
			g_message ("Created drag window %#lx\n", dpy_impl->motif_drag_window));
	      
	      XChangeProperty (dpy_impl->xdisplay, scr_impl->root_window,
			       dpy_impl->motif_drag_window_atom, XA_WINDOW,
			       32, PropModeReplace,
			       (guchar *)&dpy_impl->motif_drag_window_atom, 1);

	    }
	  XUngrabServer (dpy_impl->xdisplay);
	  XCloseDisplay (dpy_impl->xdisplay);
	}

      /* There is a miniscule race condition here if the drag window
       * gets destroyed exactly now.
       */
      if (dpy_impl->motif_drag_window)
	{
	  dpy_impl->motif_drag_gdk_window = 
		  gdk_window_foreign_new_for_display (dpy, dpy_impl->motif_drag_window);
	  gdk_window_add_filter (dpy_impl->motif_drag_gdk_window,
				 motif_drag_window_filter,
				 NULL);
	}
    }

  return dpy_impl->motif_drag_window;
}

static void 
motif_read_target_table_for_display (GdkDisplay *dpy)
{
  gulong bytes_after, nitems;
  GdkAtom type;
  gint format;
  gint i, j;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);

  if (!dpy_impl->motif_drag_targets_atom)
    dpy_impl->motif_drag_targets_atom = 
	    gdk_display_atom (dpy, "_MOTIF_DRAG_TARGETS", FALSE);


  if (dpy_impl->motif_target_lists)
    {
      for (i=0; i<dpy_impl->motif_n_target_lists; i++)
	g_list_free (dpy_impl->motif_target_lists[i]);
      
      g_free (dpy_impl->motif_target_lists);
      dpy_impl->motif_target_lists = NULL;
      dpy_impl->motif_n_target_lists = 0;
    }

  if (motif_find_drag_window_for_display (FALSE, dpy))
    {
      MotifTargetTableHeader *header = NULL;
      guchar *target_bytes = NULL;
      guchar *p;
      gboolean success = FALSE;

      gdk_error_trap_push ();
      XGetWindowProperty (dpy_impl->xdisplay,dpy_impl->motif_drag_window, 
			  dpy_impl->motif_drag_targets_atom,
			  0, (sizeof(MotifTargetTableHeader)+3)/4, FALSE,
			  dpy_impl->motif_drag_targets_atom, 
			  &type, &format, &nitems, &bytes_after,
			  (guchar **)&header);

      if (gdk_error_trap_pop () || (format != 8) || (nitems < sizeof (MotifTargetTableHeader)))
	goto error;

      header->n_lists = card16_to_host (header->n_lists, header->byte_order);
      header->total_size = card32_to_host (header->total_size, header->byte_order);

      gdk_error_trap_push ();
      XGetWindowProperty (dpy_impl->xdisplay, dpy_impl->motif_drag_window, 
		          dpy_impl->motif_drag_targets_atom,
			  (sizeof(MotifTargetTableHeader)+3)/4, 
			  (header->total_size + 3)/4 - (sizeof(MotifTargetTableHeader) + 3)/4,
			  FALSE,
			  dpy_impl->motif_drag_targets_atom, &type, &format, &nitems, 
			  &bytes_after, &target_bytes);
      
      if (gdk_error_trap_pop () || (format != 8) || (bytes_after != 0) || 
	  (nitems != header->total_size - sizeof(MotifTargetTableHeader)))
	  goto error;

      dpy_impl->motif_n_target_lists = header->n_lists;
      dpy_impl->motif_target_lists = g_new0 (GList *, dpy_impl->motif_n_target_lists);

      p = target_bytes;
      for (i=0; i<header->n_lists; i++)
	{
	  gint n_targets;
	  guint32 *targets;
	  
	  if (p + sizeof(guint16) - target_bytes > nitems)
	    goto error;

	  n_targets = card16_to_host (*(gushort *)p, header->byte_order);

	  /* We need to make a copy of the targets, since it may
	   * be unaligned
	   */
	  targets = g_new (guint32, n_targets);
	  memcpy (targets, p + sizeof(guint16), sizeof(guint32) * n_targets);

	  p +=  sizeof(guint16) + n_targets * sizeof(guint32);
	  if (p - target_bytes > nitems)
	    goto error;

	  for (j=0; j<n_targets; j++)
	    dpy_impl->motif_target_lists[i] = 
	      g_list_prepend (dpy_impl->motif_target_lists[i],
			      GUINT_TO_POINTER (card32_to_host (targets[j], 
								header->byte_order)));
	  g_free (targets);
	  dpy_impl->motif_target_lists[i] = g_list_reverse (dpy_impl->motif_target_lists[i]);
	}

      success = TRUE;
      
    error:
      if (header)
	XFree (header);
      
      if (target_bytes)
	XFree (target_bytes);

      if (!success)
	{
	  if (dpy_impl->motif_target_lists)
	    {
	      g_free (dpy_impl->motif_target_lists);
	      dpy_impl->motif_target_lists = NULL;
	      dpy_impl->motif_n_target_lists = 0;
	    }
	  g_warning ("Error reading Motif target table\n");
	}
    }
}
	

static gint
targets_sort_func (gconstpointer a, gconstpointer b)
{
  return (GPOINTER_TO_UINT (a) < GPOINTER_TO_UINT (b)) ?
    -1 : ((GPOINTER_TO_UINT (a) > GPOINTER_TO_UINT (b)) ? 1 : 0);
}

/* Check if given (sorted) list is in the targets table */
static gboolean
motif_target_table_check_for_display (GList *sorted, GdkDisplay *dpy)
{
  GList *tmp_list1, *tmp_list2;
  gint i;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);

  for (i=0; i<dpy_impl->motif_n_target_lists; i++)
    {
      tmp_list1 = dpy_impl->motif_target_lists[i];
      tmp_list2 = sorted;
      
      while (tmp_list1 && tmp_list2)
	{
	  if (tmp_list1->data != tmp_list2->data)
	    break;

	  tmp_list1 = tmp_list1->next;
	  tmp_list2 = tmp_list2->next;
	}
      if (!tmp_list1 && !tmp_list2)	/* Found it */
	return i;
    }

  return -1;
}


static gint
motif_add_to_target_table_for_display (GList *targets, GdkDisplay *dpy)
{
  GList *sorted = NULL;
  gint index = -1;
  gint i;
  GList *tmp_list;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  
  /* make a sorted copy of the list */
  
  while (targets)
    {
      sorted = g_list_insert_sorted (sorted, targets->data, targets_sort_func);
      targets = targets->next;
    }

  /* First check if it is their already */

  if (dpy_impl->motif_target_lists)
    index = motif_target_table_check_for_display (sorted, dpy);

  /* We need to grab the server while doing this, to ensure
   * atomiticity. Ugh
   */

  if (index < 0)
    {
      /* We need to make sure that it exists _before_ we grab the
       * server, since we can't open a new connection after we
       * grab the server. 
       */
      motif_find_drag_window_for_display (TRUE, dpy);

      XGrabServer(dpy_impl->xdisplay);
      motif_read_target_table_for_display (dpy);
    
      /* Check again, in case it was added in the meantime */
      
      if (dpy_impl->motif_target_lists)
	index =  motif_target_table_check_for_display (sorted, dpy);

      if (index < 0)
	{
	  guint32 total_size = 0;
	  guchar *data;
	  guchar *p;
	  guint16 *p16;
	  MotifTargetTableHeader *header;
	  
	  if (!dpy_impl->motif_target_lists)
	    {
	      dpy_impl->motif_target_lists = g_new (GList *, 1);
	      dpy_impl->motif_n_target_lists = 1;
	    }
	  else
	    {
	      dpy_impl->motif_n_target_lists++;
	      dpy_impl->motif_target_lists = g_realloc (dpy_impl->motif_target_lists,
					  sizeof(GList *) * dpy_impl->motif_n_target_lists);
	    }
	  dpy_impl->motif_target_lists[dpy_impl->motif_n_target_lists - 1] = sorted;
	  sorted = NULL;
	  index = dpy_impl->motif_n_target_lists - 1;

	  total_size = sizeof (MotifTargetTableHeader);
	  for (i = 0; i < dpy_impl->motif_n_target_lists ; i++)
	    total_size += sizeof(guint16) + 
		    sizeof(guint32) * g_list_length (dpy_impl->motif_target_lists[i]);

	  data = g_malloc (total_size);

	  header = (MotifTargetTableHeader *)data;
	  p = data + sizeof(MotifTargetTableHeader);

	  header->byte_order = local_byte_order;
	  header->protocol_version = 0;
	  header->n_lists = dpy_impl->motif_n_target_lists;
	  header->total_size = total_size;

	  for (i = 0; i < dpy_impl->motif_n_target_lists ; i++)
	    {
	      guint16 n_targets = g_list_length (dpy_impl->motif_target_lists[i]);
	      guint32 *targets = g_new (guint32, n_targets);
	      guint32 *p32 = targets;
	      
	      tmp_list = dpy_impl->motif_target_lists[i];
	      while (tmp_list)
		{
		  *p32 = GPOINTER_TO_UINT (tmp_list->data);
		  
		  tmp_list = tmp_list->next;
		  p32++;
		}

	      p16 = (guint16 *)p;
	      p += sizeof(guint16);

	      memcpy (p, targets, n_targets * sizeof(guint32));

	      *p16 = n_targets;
	      p += sizeof(guint32) * n_targets;
	      g_free (targets);
	    }

	  XChangeProperty (dpy_impl->xdisplay, dpy_impl->motif_drag_window,
			   dpy_impl->motif_drag_targets_atom,
			   dpy_impl->motif_drag_targets_atom,
			   8, PropModeReplace,
			   data, total_size);
	}
      XUngrabServer(dpy_impl->xdisplay);
    }

  g_list_free (sorted);
  return index;
}

/* Translate flags */

static void
motif_dnd_translate_flags (GdkDragContext *context, guint16 flags)
{
  guint recommended_op = flags & 0x000f;
  guint possible_ops = (flags & 0x0f0) >> 4;
  
  switch (recommended_op)
    {
    case XmDROP_MOVE:
      context->suggested_action = GDK_ACTION_MOVE;
      break;
    case XmDROP_COPY:
      context->suggested_action = GDK_ACTION_COPY;
      break;
    case XmDROP_LINK:
      context->suggested_action = GDK_ACTION_LINK;
      break;
    default:
      context->suggested_action = GDK_ACTION_COPY;
      break;
    }

  context->actions = 0;
  if (possible_ops & XmDROP_MOVE)
    context->actions |= GDK_ACTION_MOVE;
  if (possible_ops & XmDROP_COPY)
    context->actions |= GDK_ACTION_COPY;
  if (possible_ops & XmDROP_LINK)
    context->actions |= GDK_ACTION_LINK;
}

static guint16
motif_dnd_get_flags (GdkDragContext *context)
{
  guint16 flags = 0;
  
  switch (context->suggested_action)
    {
    case GDK_ACTION_MOVE:
      flags = XmDROP_MOVE;
      break;
    case GDK_ACTION_COPY:
      flags = XmDROP_COPY;
      break;
    case GDK_ACTION_LINK:
      flags = XmDROP_LINK;
      break;
    default:
      flags = XmDROP_NOOP;
      break;
    }
  
  if (context->actions & GDK_ACTION_MOVE)
    flags |= XmDROP_MOVE << 8;
  if (context->actions & GDK_ACTION_COPY)
    flags |= XmDROP_COPY << 8;
  if (context->actions & GDK_ACTION_LINK)
    flags |= XmDROP_LINK << 8;

  return flags;
}

/* Source Side */

static void
motif_set_targets (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  MotifDragInitiatorInfo info;
  gint i;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(private->window_cache->display);
  static GdkAtom motif_drag_initiator_info = GDK_NONE;

  if (!motif_drag_initiator_info)
    motif_drag_initiator_info = gdk_display_atom (private->window_cache->display, "_MOTIF_DRAG_INITIATOR_INFO", FALSE);


  info.byte_order = local_byte_order;
  info.protocol_version = 0;
  
  info.targets_index = motif_add_to_target_table_for_display (context->targets, 
						private->window_cache->display);

  for (i=0; ; i++)
    {
      gchar buf[20];
      g_snprintf(buf, 20, "_GDK_SELECTION_%d", i);
      
      private->motif_selection = gdk_display_atom (private->window_cache->display, buf, FALSE);

      if (!XGetSelectionOwner (dpy_impl->xdisplay, private->motif_selection))
	break;
    }

  info.selection_atom = private->motif_selection;

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		   GDK_DRAWABLE_XID (context->source_window),
		   private->motif_selection,
		   motif_drag_initiator_info, 8, PropModeReplace,
		   (guchar *)&info, sizeof (info));

  private->motif_targets_set = 1;
}

static guint32
motif_check_dest_for_display (Window win, GdkDisplay *dpy)
{
  gboolean retval = FALSE;
  MotifDragReceiverInfo *info;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);

  if (!dpy_impl->motif_drag_receiver_info_atom)
    dpy_impl->motif_drag_receiver_info_atom = 
	    gdk_display_atom (dpy, "_MOTIF_DRAG_RECEIVER_INFO", FALSE);


  gdk_error_trap_push ();
  XGetWindowProperty (dpy_impl->xdisplay, win, 
		      dpy_impl->motif_drag_receiver_info_atom, 
		      0, (sizeof(*info)+3)/4, False, AnyPropertyType,
		      &type, &format, &nitems, &after, 
		      (guchar **)&info);

  if (gdk_error_trap_pop() == 0)
    {
      if (type != None)
	{
	  if ((format == 8) && (nitems == sizeof(*info)))
	    {
	      if ((info->protocol_version == 0) &&
		  ((info->protocol_style == XmDRAG_PREFER_PREREGISTER) ||
		   (info->protocol_style == XmDRAG_PREFER_DYNAMIC) ||
		   (info->protocol_style == XmDRAG_DYNAMIC)))
		retval = TRUE;
	    }
	  else
	    {
	      GDK_NOTE (DND, 
			g_warning ("Invalid Motif drag receiver property on window %ld\n", win));
	    }
	  
	  XFree (info);
	}
    }

  return retval ? win : GDK_NONE;
  
}

static void
motif_send_enter (GdkDragContext  *context,
		  guint32          time)
{
  XEvent xev;
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = 
	  gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE);

  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->source_window);

  MOTIF_XCLIENT_BYTE (&xev, 0) = XmTOP_LEVEL_ENTER;
  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = 0;
  MOTIF_XCLIENT_LONG (&xev, 1) = time;
  MOTIF_XCLIENT_LONG (&xev, 2) = GDK_DRAWABLE_XID (context->source_window);

  if (!private->motif_targets_set)
    motif_set_targets (context);

  MOTIF_XCLIENT_LONG (&xev, 3) = private->motif_selection;

  if (!gdk_send_xevent (GDK_DRAWABLE_XID (context->dest_window),
			FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));
}

static void
motif_send_leave (GdkDragContext  *context,
		  guint32          time)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = 
	  gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE);

  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 0) = XmTOP_LEVEL_LEAVE;
  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = 0;
  MOTIF_XCLIENT_LONG (&xev, 1) = time;
  MOTIF_XCLIENT_LONG (&xev, 2) = GDK_DRAWABLE_XID (context->source_window);
  MOTIF_XCLIENT_LONG (&xev, 3) = 0;

  if (!gdk_send_xevent (GDK_DRAWABLE_XID (context->dest_window),
			FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));
}

static gboolean
motif_send_motion (GdkDragContext  *context,
		    gint            x_root, 
		    gint            y_root,
		    GdkDragAction   action,
		    guint32         time)
{
  gboolean retval;
  XEvent xev;
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = 
	  gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE);

  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = motif_dnd_get_flags (context);
  MOTIF_XCLIENT_LONG (&xev, 1) = time;

  if ((context->suggested_action != private->old_action) ||
      (context->actions != private->old_actions))
    {
      MOTIF_XCLIENT_BYTE (&xev, 0) = XmOPERATION_CHANGED;

      /* private->drag_status = GDK_DRAG_STATUS_ACTION_WAIT; */
      retval = TRUE;
    }
  else
    {
      MOTIF_XCLIENT_BYTE (&xev, 0) = XmDRAG_MOTION;

      MOTIF_XCLIENT_SHORT (&xev, 4) = x_root;
      MOTIF_XCLIENT_SHORT (&xev, 5) = y_root;
      
      private->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
      retval = FALSE;
    }

  if (!gdk_send_xevent (GDK_DRAWABLE_XID (context->dest_window),
			FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));

  return retval;
}

static void
motif_send_drop (GdkDragContext *context, guint32 time)
{
  XEvent xev;
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = 
	  gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE);

  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_START;
  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = motif_dnd_get_flags (context);
  MOTIF_XCLIENT_LONG (&xev, 1)  = time;

  MOTIF_XCLIENT_SHORT (&xev, 4) = private->last_x;
  MOTIF_XCLIENT_SHORT (&xev, 5) = private->last_y;

  MOTIF_XCLIENT_LONG (&xev, 3)  = private->motif_selection;
  MOTIF_XCLIENT_LONG (&xev, 4)  = GDK_DRAWABLE_XID (context->source_window);

  if (!gdk_send_xevent (GDK_DRAWABLE_XID (context->dest_window),
			FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));
}

/* Target Side */

static gboolean
motif_read_initiator_info_for_display (Window source_window, 
				       GdkDisplay *dpy,
				       Atom atom,
				       GList  **targets,
				       GdkAtom *selection)
{
  GList *tmp_list;
  static GdkAtom motif_drag_initiator_info = GDK_NONE;
  GdkAtom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  MotifDragInitiatorInfo *initiator_info;

  if (!motif_drag_initiator_info)
    motif_drag_initiator_info = 
	    gdk_display_atom (dpy, "_MOTIF_DRAG_INITIATOR_INFO", FALSE);


  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY(dpy), source_window, atom,
		      0, sizeof(*initiator_info), FALSE,
		      motif_drag_initiator_info, 
		      &type, &format, &nitems, &bytes_after,
		      (guchar **)&initiator_info);

  if (gdk_error_trap_pop () || (format != 8) || (nitems != sizeof (MotifDragInitiatorInfo)) || (bytes_after != 0))
    {
      g_warning ("Error reading initiator info\n");
      return FALSE;
    }

  motif_read_target_table_for_display (dpy);

  initiator_info->targets_index = 
    card16_to_host (initiator_info->targets_index, initiator_info->byte_order);
  initiator_info->selection_atom = 
    card32_to_host (initiator_info->selection_atom, initiator_info->byte_order);
  
  if (initiator_info->targets_index >= dpy_impl->motif_n_target_lists)
    {
      g_warning ("Invalid target index in TOP_LEVEL_ENTER MESSAGE");
      XFree (initiator_info);
      return GDK_FILTER_REMOVE;
    }

  tmp_list = g_list_last (dpy_impl->motif_target_lists[initiator_info->targets_index]);

  *targets = NULL;
  while (tmp_list)
    {
      *targets = g_list_prepend (*targets,
				 tmp_list->data);
      tmp_list = tmp_list->prev;
    }

#ifdef G_ENABLE_DEBUG
  if (gdk_debug_flags & GDK_DEBUG_DND)
    print_target_list (*targets, dpy);
#endif /* G_ENABLE_DEBUG */

  *selection = initiator_info->selection_atom;

  XFree (initiator_info);

  return TRUE;
}

static GdkDragContext *
motif_drag_context_new (GdkWindow *dest_window,
			guint32    timestamp,
			guint32    source_window,
			guint32    atom)
{
  GdkDragContext *new_context;
  GdkDragContextPrivateX11 *private;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(dest_window));

  /* FIXME, current_dest_drag really shouldn't be NULL'd
   * if we error below.
   */
  if (dpy_impl->current_dest_drag != NULL)
    {
      if (timestamp >= dpy_impl->current_dest_drag->start_time)
	{
	  gdk_drag_context_unref (dpy_impl->current_dest_drag);
	  dpy_impl->current_dest_drag = NULL;
	}
      else
	return NULL;
    }

  new_context = gdk_drag_context_new ();
  private = PRIVATE_DATA (new_context);

  new_context->protocol = GDK_DRAG_PROTO_MOTIF;
  new_context->is_source = FALSE;

  new_context->source_window = gdk_window_lookup (source_window);
  if (new_context->source_window)
    gdk_window_ref (new_context->source_window);
  else
    {
      new_context->source_window = 
      	gdk_window_foreign_new_for_display (GDK_DRAWABLE_DISPLAY(dest_window),
      					    source_window);
      if (!new_context->source_window)
	{
	  gdk_drag_context_unref (new_context);
	  return NULL;
	}
    }

  new_context->dest_window = dest_window;
  gdk_window_ref (dest_window);
  new_context->start_time = timestamp;
  /* using dest_window to get the display as dest_window and source_window
   * are sharing the same display */

  if (!motif_read_initiator_info_for_display(source_window,
					     GDK_WINDOW_DISPLAY(dest_window),
					     atom,
					    &new_context->targets,
					    &private->motif_selection))
    {
      gdk_drag_context_unref (new_context);
      return NULL;
    }

  return new_context;
}

/*
 * The MOTIF drag protocol has no real provisions for distinguishing
 * multiple simultaneous drops. If the sources grab the pointer
 * when doing drags, that shouldn't happen, in any case. If it
 * does, we can't do much except hope for the best.
 */

static GdkFilterReturn
motif_top_level_enter (GdkEvent *event,
		       guint16   flags, 
		       guint32   timestamp, 
		       guint32   source_window, 
		       guint32   atom)
{
  GdkDragContext *new_context;

  GDK_NOTE(DND, g_message ("Motif DND top level enter: flags: %#4x time: %d source_widow: %#4x atom: %d",
			   flags, timestamp, source_window, atom));

  new_context = motif_drag_context_new (event->any.window, timestamp, source_window, atom);
  if (!new_context)
    return GDK_FILTER_REMOVE;

  event->dnd.type = GDK_DRAG_ENTER;
  event->dnd.context = new_context;
  gdk_drag_context_ref (new_context);

  GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window))->current_dest_drag = new_context;

  return GDK_FILTER_TRANSLATE;
}

static GdkFilterReturn
motif_top_level_leave (GdkEvent *event,
		       guint16   flags, 
		       guint32   timestamp)
{
  GdkDisplayImplX11 * dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  GDK_NOTE(DND, g_message ("Motif DND top level leave: flags: %#4x time: %d",
			   flags, timestamp));

  if ((dpy_impl->current_dest_drag != NULL) &&
      (dpy_impl->current_dest_drag->protocol == GDK_DRAG_PROTO_MOTIF) &&
      (timestamp >= dpy_impl->current_dest_drag->start_time))
    {
      event->dnd.type = GDK_DRAG_LEAVE;
      /* Pass ownership of context to the event */
      event->dnd.context = dpy_impl->current_dest_drag;

      dpy_impl->current_dest_drag = NULL;

      return GDK_FILTER_TRANSLATE;
    }
  else
    return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_motion (GdkEvent *event,
	      guint16   flags, 
	      guint32   timestamp,
	      gint16    x_root,
	      gint16    y_root)
{
  GdkDragContextPrivateX11 *private;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  GDK_NOTE(DND, g_message ("Motif DND motion: flags: %#4x time: %d (%d, %d)",
			   flags, timestamp, x_root, y_root));

  if ((dpy_impl->current_dest_drag != NULL) &&
      (dpy_impl->current_dest_drag->protocol == GDK_DRAG_PROTO_MOTIF) &&
      (timestamp >= dpy_impl->current_dest_drag->start_time))
    {
      private = PRIVATE_DATA (dpy_impl->current_dest_drag);

      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.context = dpy_impl->current_dest_drag;
      gdk_drag_context_ref (dpy_impl->current_dest_drag);

      event->dnd.time = timestamp;

      motif_dnd_translate_flags (dpy_impl->current_dest_drag, flags);

      event->dnd.x_root = x_root;
      event->dnd.y_root = y_root;

      private->last_x = x_root;
      private->last_y = y_root;

      private->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_operation_changed (GdkEvent *event,
			 guint16   flags, 
			 guint32   timestamp)
{
  GdkDragContextPrivateX11 *private;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  GDK_NOTE(DND, g_message ("Motif DND operation changed: flags: %#4x time: %d",
			   flags, timestamp));

  if ((dpy_impl->current_dest_drag != NULL) &&
      (dpy_impl->current_dest_drag->protocol == GDK_DRAG_PROTO_MOTIF) &&
      (timestamp >= dpy_impl->current_dest_drag->start_time))
    {
      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.send_event = FALSE;
      event->dnd.context = dpy_impl->current_dest_drag;
      gdk_drag_context_ref (dpy_impl->current_dest_drag);

      event->dnd.time = timestamp;
      private = PRIVATE_DATA (dpy_impl->current_dest_drag);

      motif_dnd_translate_flags (dpy_impl->current_dest_drag, flags);

      event->dnd.x_root = private->last_x;
      event->dnd.y_root = private->last_y;

      private->drag_status = GDK_DRAG_STATUS_ACTION_WAIT;

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_drop_start (GdkEvent *event,
		  guint16   flags,
		  guint32   timestamp,
		  guint32   source_window,
		  guint32   atom,
		  gint16    x_root,
		  gint16    y_root)
{
  GdkDragContext *new_context;


  GDK_NOTE(DND, g_message ("Motif DND drop start: flags: %#4x time: %d (%d, %d) source_widow: %#4x atom: %d",
			   flags, timestamp, x_root, y_root, source_window, atom));

  new_context = motif_drag_context_new (event->any.window, timestamp, source_window, atom);
  if (!new_context)
    return GDK_FILTER_REMOVE;

  motif_dnd_translate_flags (new_context, flags);

  event->dnd.type = GDK_DROP_START;
  event->dnd.context = new_context;
  event->dnd.time = timestamp;
  event->dnd.x_root = x_root;
  event->dnd.y_root = y_root;

  gdk_drag_context_ref (new_context);
  GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window))->current_dest_drag = new_context;

  return GDK_FILTER_TRANSLATE;
}  

static GdkFilterReturn
motif_drag_status (GdkEvent *event,
		   guint16   flags,
		   guint32   timestamp)
{
  GdkDragContext *context;
  
  GDK_NOTE (DND, 
	    g_message ("Motif status message: flags %x", flags));

  context = gdk_drag_context_find (TRUE, 
				   GDK_DRAWABLE_XID (event->any.window), 
				   None);

  if (context)
    {
      GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
      if ((private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT) ||
	  (private->drag_status == GDK_DRAG_STATUS_ACTION_WAIT))
	private->drag_status = GDK_DRAG_STATUS_DRAG;
      
      event->dnd.type = GDK_DRAG_STATUS;
      event->dnd.send_event = FALSE;
      event->dnd.context = context;
      gdk_drag_context_ref (context);

      event->dnd.time = timestamp;

      if ((flags & 0x00f0) >> 4 == XmDROP_SITE_VALID)
	{
	  switch (flags & 0x000f)
	    {
	    case XmDROP_NOOP:
	      context->action = 0;
	      break;
	    case XmDROP_MOVE:
		context->action = GDK_ACTION_MOVE;
		break;
	    case XmDROP_COPY:
	      context->action = GDK_ACTION_COPY;
	      break;
	    case XmDROP_LINK:
	      context->action = GDK_ACTION_LINK;
	      break;
	    }
	}
      else
	context->action = 0;

      return GDK_FILTER_TRANSLATE;
    }
  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_dnd_filter (GdkXEvent *xev,
		  GdkEvent  *event,
		  gpointer data)
{
  XEvent *xevent = (XEvent *)xev;

  guint8 reason;
  guint16 flags;
  guint32 timestamp;
  guint32 source_window;
  GdkAtom atom;
  gint16 x_root, y_root;
  gboolean is_reply;
  
  /* First read some fields common to all Motif DND messages */

  reason = MOTIF_UNPACK_BYTE (xevent, 0);
  flags = MOTIF_UNPACK_SHORT (xevent, 1);
  timestamp = MOTIF_UNPACK_LONG (xevent, 1);

  is_reply = ((reason & 0x80) != 0);

  switch (reason & 0x7f)
    {
    case XmTOP_LEVEL_ENTER:
      source_window = MOTIF_UNPACK_LONG (xevent, 2);
      atom = MOTIF_UNPACK_LONG (xevent, 3);
      return motif_top_level_enter (event, flags, timestamp, source_window, atom);
    case XmTOP_LEVEL_LEAVE:
      return motif_top_level_leave (event, flags, timestamp);

    case XmDRAG_MOTION:
      x_root = MOTIF_UNPACK_SHORT (xevent, 4);
      y_root = MOTIF_UNPACK_SHORT (xevent, 5);
      
      if (!is_reply)
	return motif_motion (event, flags, timestamp, x_root, y_root);
      else
	return motif_drag_status (event, flags, timestamp);

    case XmDROP_SITE_ENTER:
      return motif_drag_status (event, flags, timestamp);

    case XmDROP_SITE_LEAVE:
      return motif_drag_status (event,
				XmNO_DROP_SITE << 8 | XmDROP_NOOP, 
				timestamp);
    case XmDROP_START:
      x_root = MOTIF_UNPACK_SHORT (xevent, 4);
      y_root = MOTIF_UNPACK_SHORT (xevent, 5);
      atom = MOTIF_UNPACK_LONG (xevent, 3);
      source_window = MOTIF_UNPACK_LONG (xevent, 4);

      if (!is_reply)
	return motif_drop_start (event, flags, timestamp, source_window, atom, x_root, y_root);
      
     break;
    case XmOPERATION_CHANGED:
      if (!is_reply)
	return motif_operation_changed (event, flags, timestamp);
      else
	return motif_drag_status (event, flags, timestamp);

      break;
      /* To the best of my knowledge, these next two messages are 
       * not part of the protocol, though they are defined in
       * the header files.
       */
    case XmDROP_FINISH:
    case XmDRAG_DROP_FINISH:
      break;
    }

  return GDK_FILTER_REMOVE;
}

/*************************************************************
 ***************************** XDND **************************
 *************************************************************/

/* Utility functions */

#define XDND_ACTION_COPY    "XdndActionCopy"
#define XDND_ACTION_MOVE    "XdndActionMove"
#define XDND_ACTION_LINK    "XdndActionLink"
#define XDND_ACTION_ASK	    "XdndActionAsk"
#define XDND_ACTION_PRIVATE "XdndActionPrivate"
#define XDND_N_ACTIONS	    5
static void
xdnd_initialize_actions_for_display (GdkDisplay *dpy)
{
  gint i;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  dpy_impl->xdnd_actions_initialized = TRUE;
  dpy_impl->xdnd_n_actions = XDND_N_ACTIONS;
  dpy_impl->xdnd_actions_table = g_new(Xdnd_Action,XDND_N_ACTIONS);
  dpy_impl->xdnd_actions_table[0].name = g_strdup(XDND_ACTION_COPY);
  dpy_impl->xdnd_actions_table[1].name = g_strdup(XDND_ACTION_MOVE);
  dpy_impl->xdnd_actions_table[2].name = g_strdup(XDND_ACTION_LINK);
  dpy_impl->xdnd_actions_table[3].name = g_strdup(XDND_ACTION_ASK);
  dpy_impl->xdnd_actions_table[4].name = g_strdup(XDND_ACTION_PRIVATE);
  dpy_impl->xdnd_actions_table[0].action = GDK_ACTION_COPY;
  dpy_impl->xdnd_actions_table[1].action = GDK_ACTION_MOVE;
  dpy_impl->xdnd_actions_table[2].action = GDK_ACTION_LINK;
  dpy_impl->xdnd_actions_table[3].action = GDK_ACTION_ASK;
  dpy_impl->xdnd_actions_table[4].action = GDK_ACTION_PRIVATE;
  
  for (i=0; i < dpy_impl->xdnd_n_actions; i++)
    dpy_impl->xdnd_actions_table[i].atom = 
	    gdk_display_atom (dpy, dpy_impl->xdnd_actions_table[i].name, FALSE);

}

static GdkDragAction
xdnd_action_from_atom_for_display (GdkAtom atom, GdkDisplay * dpy)
{
  gint i;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  if (!dpy_impl->xdnd_actions_initialized)
    xdnd_initialize_actions_for_display (dpy);

  for (i=0; i<dpy_impl->xdnd_n_actions; i++)
    if (atom == dpy_impl->xdnd_actions_table[i].atom)
      return dpy_impl->xdnd_actions_table[i].action;

  return 0;
}

static GdkAtom
xdnd_action_to_atom_for_display (GdkDragAction action, GdkDisplay *dpy)
{
  gint i;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  if (!dpy_impl->xdnd_actions_initialized)
    xdnd_initialize_actions_for_display (dpy);

  for (i=0; i<dpy_impl->xdnd_n_actions; i++)
    if (action == dpy_impl->xdnd_actions_table[i].action)
      return dpy_impl->xdnd_actions_table[i].atom;

  return GDK_NONE;
}

/* Source side */

static GdkFilterReturn 
xdnd_status_filter (GdkXEvent *xev,
		    GdkEvent  *event,
		    gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 dest_window = xevent->xclient.data.l[0];
  guint32 flags = xevent->xclient.data.l[1];
  GdkAtom action = xevent->xclient.data.l[4];
  GdkDragContext *context;
  
  GDK_NOTE (DND, 
	    g_message ("XdndStatus: dest_window: %#x  action: %ld",
		       dest_window, action));

  
  context = gdk_drag_context_find (TRUE, xevent->xclient.window, dest_window);
  if (context)
    {
      GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
      if (private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
	private->drag_status = GDK_DRAG_STATUS_DRAG;
      
      event->dnd.send_event = FALSE;
      event->dnd.type = GDK_DRAG_STATUS;
      event->dnd.context = context;
      gdk_drag_context_ref (context);

      event->dnd.time = GDK_CURRENT_TIME; /* FIXME? */
      if (!(action != 0) != !(flags & 1))
	{
	  GDK_NOTE (DND,
		    g_warning ("Received status event with flags not corresponding to action!\n"));
	  action = 0;
	}

      context->action = xdnd_action_from_atom_for_display (action,
			    GDK_WINDOW_DISPLAY(event->any.window));

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn 
xdnd_finished_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 dest_window = xevent->xclient.data.l[0];
  GdkDragContext *context;
  
  GDK_NOTE (DND, 
	    g_message ("XdndFinished: dest_window: %#x", dest_window));

  context = gdk_drag_context_find (TRUE, xevent->xclient.window, dest_window);
  if (context)
    {
      event->dnd.type = GDK_DROP_FINISHED;
      event->dnd.context = context;
      gdk_drag_context_ref (context);

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static void
xdnd_set_targets (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkAtom *atomlist;
  GList *tmp_list = context->targets;
  gint i;
  gint n_atoms = g_list_length (context->targets);

  atomlist = g_new (GdkAtom, n_atoms);
  i = 0;
  while (tmp_list)
    {
      atomlist[i] = GPOINTER_TO_INT (tmp_list->data);
      tmp_list = tmp_list->next;
      i++;
    }

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		   GDK_DRAWABLE_XID (context->source_window),
		   gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->source_window), "XdndTypeList", FALSE),

		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  private->xdnd_targets_set = 1;
}

static void
xdnd_set_actions (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkAtom *atomlist;
  gint i;
  gint n_atoms;
  guint actions;
  GdkDisplayImplX11 *dpy_impl = 
	  GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY (context->source_window));

  if (!dpy_impl->xdnd_actions_initialized)
    xdnd_initialize_actions_for_display(GDK_DRAWABLE_DISPLAY (context->source_window));
  
  actions = context->actions;
  n_atoms = 0;
  for (i=0; i<dpy_impl->xdnd_n_actions; i++)
    {
      if (actions & dpy_impl->xdnd_actions_table[i].action)
	{
	  actions &= ~dpy_impl->xdnd_actions_table[i].action;
	  n_atoms++;
	}
    }

  atomlist = g_new (GdkAtom, n_atoms);

  actions = context->actions;
  n_atoms = 0;
  for (i=0; i<dpy_impl->xdnd_n_actions; i++)
    {
      if (actions & dpy_impl->xdnd_actions_table[i].action)
	{
	  actions &= ~dpy_impl->xdnd_actions_table[i].action;
	  atomlist[n_atoms] = dpy_impl->xdnd_actions_table[i].atom;
	  n_atoms++;
	}
    }

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		   GDK_DRAWABLE_XID (context->source_window),
		   gdk_display_atom (GDK_DRAWABLE_DISPLAY (context->source_window), "XdndActionList", FALSE),

		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  private->xdnd_actions_set = 1;
  private->xdnd_actions = context->actions;
}

/*************************************************************
 * xdnd_send_xevent_for_display:
 *     Like gdk_send_event, but if the target is the root
 *     window, sets an event mask of ButtonPressMask, otherwise
 *     an event mask of 0.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static gint
xdnd_send_xevent_for_display (Window window, GdkDisplay* display, gboolean propagate, 
		  XEvent *event_send)
{
  if (window == GDK_DISPLAY_DND_SCREEN(display)->root_window)
    return gdk_send_xevent (window, propagate, ButtonPressMask, event_send);
  else
    return gdk_send_xevent (window, propagate, 0, event_send);
}

static void
xdnd_send_enter (GdkDragContext *context)
{
  XEvent xev;
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "XdndEnter", FALSE);

  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = (3 << 24); /* version */
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!private->xdnd_selection)
    private->xdnd_selection = gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "XdndSelection", FALSE);


  if (g_list_length (context->targets) > 3)
    {
      if (!private->xdnd_targets_set)
	xdnd_set_targets (context);
      xev.xclient.data.l[1] |= 1;
    }
  else
    {
      GList *tmp_list = context->targets;
      gint i = 2;

      while (tmp_list)
	{
	  xev.xclient.data.l[i] = GPOINTER_TO_INT (tmp_list->data);
	  tmp_list = tmp_list->next;
	  i++;
	}
    }

  if (!xdnd_send_xevent_for_display (GDK_DRAWABLE_XID (context->dest_window),
				     GDK_DRAWABLE_DISPLAY (context->source_window),
				     FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_leave (GdkDragContext *context)
{
  XEvent xev;

  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_display_atom (GDK_DRAWABLE_DISPLAY (context->dest_window), "XdndLeave", FALSE);

  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent_for_display (GDK_DRAWABLE_XID (context->dest_window),
				     GDK_DRAWABLE_DISPLAY (context->source_window),
				     FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_drop (GdkDragContext *context, guint32 time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_display_atom (GDK_DRAWABLE_DISPLAY (context->dest_window), "XdndDrop", FALSE);

  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = time;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent_for_display (GDK_DRAWABLE_XID (context->dest_window),
				     GDK_DRAWABLE_DISPLAY (context->source_window),
				     FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_motion (GdkDragContext *context,
		  gint            x_root, 
		  gint            y_root,
		  GdkDragAction   action,
		  guint32         time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_display_atom (GDK_DRAWABLE_DISPLAY (context->dest_window), "XdndPosition", FALSE);

  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (x_root << 16) | y_root;
  xev.xclient.data.l[3] = time;
  xev.xclient.data.l[4] = xdnd_action_to_atom_for_display (action,
		               GDK_DRAWABLE_DISPLAY(context->dest_window));

  if (!xdnd_send_xevent_for_display (GDK_DRAWABLE_XID (context->dest_window),
				     GDK_DRAWABLE_DISPLAY (context->source_window),
				     FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
  private->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
}

static guint32
xdnd_check_dest_for_display (Window win, GdkDisplay *display)
{
  gboolean retval = FALSE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  GdkAtom *version;
  Window *proxy_data;
  Window proxy;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(display);

  gint old_warnings = gdk_error_warnings;

  if (!dpy_impl->xdnd_proxy_atom)
    dpy_impl->xdnd_proxy_atom = gdk_display_atom (display, "XdndProxy", FALSE);


  if (!dpy_impl->xdnd_aware_atom)
    dpy_impl->xdnd_aware_atom = gdk_display_atom (display, "XdndAware", FALSE);


  proxy = GDK_NONE;
  
  gdk_error_code = 0;
  gdk_error_warnings = 0;

  XGetWindowProperty (dpy_impl->xdisplay, win, 
		      dpy_impl->xdnd_proxy_atom, 0, 
		      1, False, AnyPropertyType,
		      &type, &format, &nitems, &after, 
		      (guchar **)&proxy_data);

  if (!gdk_error_code)
    {
      if (type != None)
	{
	  if ((format == 32) && (nitems == 1))
	    {
	      proxy = *proxy_data;
	    }
	  else
	    GDK_NOTE (DND, 
		      g_warning ("Invalid XdndOwner property on window %ld\n", win));
	  
	  XFree (proxy_data);
	}
      
      XGetWindowProperty (dpy_impl->xdisplay, proxy ? proxy : win,
			  dpy_impl->xdnd_aware_atom, 0, 
			  1, False, AnyPropertyType,
			  &type, &format, &nitems, &after, 
			  (guchar **)&version);
      
      if (!gdk_error_code && type != None)
	{
	  if ((format == 32) && (nitems == 1))
	    {
	      if (*version >= 3)
		retval = TRUE;
	    }
	  else
	    GDK_NOTE (DND, 
		      g_warning ("Invalid XdndAware property on window %ld\n", win));
	  
	  XFree (version);
	}
      
    }

  gdk_error_warnings = old_warnings;
  gdk_error_code = 0;
  
  return retval ? (proxy ? proxy : win) : GDK_NONE;
}

/* Target side */

static void
xdnd_read_actions (GdkDragContext *context)
{
  Atom type;
  int format;
  gulong nitems, after;
  Atom *data;

  gint i;

  gint old_warnings = gdk_error_warnings;

  gdk_error_code = 0;
  gdk_error_warnings = 0;

  /* Get the XdndActionList, if set */

  XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		      GDK_DRAWABLE_XID (context->source_window),
		      gdk_display_atom (GDK_DRAWABLE_DISPLAY (context->source_window), "XdndActionList", FALSE),

		      0, 65536,
		      False, XA_ATOM, &type, &format, &nitems,
		      &after, (guchar **)&data);
  
  if (!gdk_error_code && (format == 32) && (type == XA_ATOM))
    {
      context->actions = 0;

      for (i=0; i<nitems; i++)
	context->actions |= xdnd_action_from_atom_for_display (data[i],
			    GDK_DRAWABLE_DISPLAY (context->source_window) );

      (PRIVATE_DATA (context))->xdnd_have_actions = TRUE;

#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_DND)
	{
	  GString *action_str = g_string_new (NULL);
	  if (context->actions & GDK_ACTION_MOVE)
	    g_string_append(action_str, "MOVE ");
	  if (context->actions & GDK_ACTION_COPY)
	    g_string_append(action_str, "COPY ");
	  if (context->actions & GDK_ACTION_LINK)
	    g_string_append(action_str, "LINK ");
	  if (context->actions & GDK_ACTION_ASK)
	    g_string_append(action_str, "ASK ");
	  
	  g_message("Xdnd actions = %s", action_str->str);
	  g_string_free (action_str, TRUE);
	}
#endif /* G_ENABLE_DEBUG */

      XFree(data);
    }

  gdk_error_warnings = old_warnings;
  gdk_error_code = 0;
}

/* We have to make sure that the XdndActionList we keep internally
 * is up to date with the XdndActionList on the source window
 * because we get no notification, because Xdnd wasn't meant
 * to continually send actions. So we select on PropertyChangeMask
 * and add this filter.
 */
static GdkFilterReturn 
xdnd_source_window_filter (GdkXEvent *xev,
			   GdkEvent  *event,
			   gpointer   cb_data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkDragContext *context = cb_data;

  if ((xevent->xany.type == PropertyNotify) &&
      (xevent->xproperty.atom == gdk_display_atom (GDK_WINDOW_DISPLAY(event->any.window), "XdndActionList", FALSE)))

    {
      xdnd_read_actions (context);

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

static void
xdnd_manage_source_filter (GdkDragContext *context,
			   GdkWindow      *window,
			   gboolean        add_filter)
{
  gdk_error_trap_push ();

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (add_filter)
	{
	  gdk_window_set_events (window,
				 gdk_window_get_events (window) |
				 GDK_PROPERTY_CHANGE_MASK);
	  gdk_window_add_filter (window, xdnd_source_window_filter, context);

	}
      else
	{
	  gdk_window_remove_filter (window,
				    xdnd_source_window_filter,
				    context);
	  /* Should we remove the GDK_PROPERTY_NOTIFY mask?
	   * but we might want it for other reasons. (Like
	   * INCR selection transactions).
	   */
	}
    }

  gdk_display_sync (gdk_window_get_display(window));
  gdk_error_trap_pop ();  
}

static GdkFilterReturn 
xdnd_enter_filter (GdkXEvent *xev,
		   GdkEvent  *event,
		   gpointer   cb_data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkDragContext *new_context;
  gint i;
  
  Atom type;
  int format;
  gulong nitems, after;
  Atom *data;

  guint32 source_window = xevent->xclient.data.l[0];
  gboolean get_types = ((xevent->xclient.data.l[1] & 1) != 0);
  gint version = (xevent->xclient.data.l[1] & 0xff000000) >> 24;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  
  GDK_NOTE (DND, 
	    g_message ("XdndEnter: source_window: %#x, version: %#x",
		       source_window, version));

  if (version != 3)
    {
      /* Old source ignore */
      GDK_NOTE (DND, g_message ("Ignored old XdndEnter message"));
      return GDK_FILTER_REMOVE;
    }
  
  if (dpy_impl->current_dest_drag != NULL)
    {
      gdk_drag_context_unref (dpy_impl->current_dest_drag);
      dpy_impl->current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol = GDK_DRAG_PROTO_XDND;
  new_context->is_source = FALSE;

  new_context->source_window = gdk_window_lookup (source_window);
  if (new_context->source_window)
    gdk_window_ref (new_context->source_window);
  else
    {
      new_context->source_window = 
        gdk_window_foreign_new_for_display (GDK_DRAWABLE_DISPLAY(event->any.window),
					    source_window);
      if (!new_context->source_window)
	{
	  gdk_drag_context_unref (new_context);
	  return GDK_FILTER_REMOVE;
	}
    }
  new_context->dest_window = event->any.window;
  gdk_window_ref (new_context->dest_window);

  new_context->targets = NULL;
  if (get_types)
    {
      gdk_error_trap_push ();
      XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (event->any.window), 
			  source_window, 
			  gdk_display_atom (GDK_DRAWABLE_DISPLAY (event->any.window), "XdndTypeList", FALSE), 

			  0, 65536,
			  False, XA_ATOM, &type, &format, &nitems,
			  &after, (guchar **)&data);

      if (gdk_error_trap_pop () || (format != 32) || (type != XA_ATOM))
	{
	  gdk_drag_context_unref (new_context);
	  return GDK_FILTER_REMOVE;
	}

      for (i=0; i<nitems; i++)
	new_context->targets = g_list_append (new_context->targets,
					      GUINT_TO_POINTER (data[i]));

      XFree(data);
    }
  else
    {
      for (i=0; i<3; i++)
	if (xevent->xclient.data.l[2+i])
	  new_context->targets = g_list_append (new_context->targets,
						GUINT_TO_POINTER (xevent->xclient.data.l[2+i]));
    }

#ifdef G_ENABLE_DEBUG
  if (gdk_debug_flags & GDK_DEBUG_DND)
    print_target_list (new_context->targets,GDK_DRAWABLE_DISPLAY(event->any.window));
#endif /* G_ENABLE_DEBUG */

  xdnd_manage_source_filter (new_context, new_context->source_window, TRUE);
  xdnd_read_actions (new_context);

  event->dnd.type = GDK_DRAG_ENTER;
  event->dnd.context = new_context;
  gdk_drag_context_ref (new_context);

  GDK_DISPLAY_IMPL_X11(
		GDK_DRAWABLE_DISPLAY(event->any.window))->current_dest_drag = new_context;

  (PRIVATE_DATA (new_context))->xdnd_selection = gdk_display_atom (GDK_DRAWABLE_DISPLAY(event->any.window), "XdndSelection", FALSE);


  return GDK_FILTER_TRANSLATE;
}

static GdkFilterReturn 
xdnd_leave_filter (GdkXEvent *xev,
		   GdkEvent  *event,
		   gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  GDK_NOTE (DND, 
	    g_message ("XdndLeave: source_window: %#x",
		       source_window));

  if ((dpy_impl->current_dest_drag != NULL) &&
      (dpy_impl->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_DRAWABLE_XID (dpy_impl->current_dest_drag->source_window) == source_window))
    {
      event->dnd.type = GDK_DRAG_LEAVE;
      /* Pass ownership of context to the event */
      event->dnd.context = dpy_impl->current_dest_drag;

      dpy_impl->current_dest_drag = NULL;

      return GDK_FILTER_TRANSLATE;
    }
  else
    return GDK_FILTER_REMOVE;
}

static GdkFilterReturn 
xdnd_position_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  gint16 x_root = xevent->xclient.data.l[2] >> 16;
  gint16 y_root = xevent->xclient.data.l[2] & 0xffff;
  guint32 time = xevent->xclient.data.l[3];
  GdkAtom action = xevent->xclient.data.l[4];
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  GDK_NOTE (DND, 
	    g_message ("XdndPosition: source_window: %#x  position: (%d, %d)  time: %d  action: %ld",
		       source_window, x_root, y_root, time, action));

  
  if ((dpy_impl->current_dest_drag != NULL) &&
      (dpy_impl->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_DRAWABLE_XID (dpy_impl->current_dest_drag->source_window) == source_window))
    {
      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.context = dpy_impl->current_dest_drag;
      gdk_drag_context_ref (dpy_impl->current_dest_drag);

      event->dnd.time = time;

      dpy_impl->current_dest_drag->suggested_action = 
         xdnd_action_from_atom_for_display (action,GDK_DRAWABLE_DISPLAY(event->any.window));
      if (!(PRIVATE_DATA (dpy_impl->current_dest_drag))->xdnd_have_actions)
	dpy_impl->current_dest_drag->actions = dpy_impl->current_dest_drag->suggested_action;

      event->dnd.x_root = x_root;
      event->dnd.y_root = y_root;

      (PRIVATE_DATA (dpy_impl->current_dest_drag))->last_x = x_root;
      (PRIVATE_DATA (dpy_impl->current_dest_drag))->last_y = y_root;
      
      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn 
xdnd_drop_filter (GdkXEvent *xev,
		  GdkEvent  *event,
		  gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  guint32 time = xevent->xclient.data.l[2];
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(event->any.window));
  GDK_NOTE (DND, 
	    g_message ("XdndDrop: source_window: %#x  time: %d",
		       source_window, time));

  if ((dpy_impl->current_dest_drag != NULL) &&
      (dpy_impl->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_DRAWABLE_XID (dpy_impl->current_dest_drag->source_window) == source_window))
    {
      GdkDragContextPrivateX11 *private;
      private = PRIVATE_DATA (dpy_impl->current_dest_drag);

      event->dnd.type = GDK_DROP_START;

      event->dnd.context = dpy_impl->current_dest_drag;
      gdk_drag_context_ref (dpy_impl->current_dest_drag);

      event->dnd.time = time;
      event->dnd.x_root = private->last_x;
      event->dnd.y_root = private->last_y;
      
      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

/*************************************************************
 ************************** Public API ***********************
 *************************************************************/
void
gdk_dnd_init (GdkDisplay* display){
  init_byte_order();

  gdk_add_client_message_filter (
	gdk_display_atom (display, "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE),

	motif_dnd_filter, NULL);
  gdk_add_client_message_filter (
	gdk_display_atom (display, "XdndEnter", FALSE),

	xdnd_enter_filter, NULL);
  gdk_add_client_message_filter (
	gdk_display_atom (display, "XdndLeave", FALSE),

	xdnd_leave_filter, NULL);
  gdk_add_client_message_filter (
	gdk_display_atom (display, "XdndPosition", FALSE),

	xdnd_position_filter, NULL);
  gdk_add_client_message_filter (
	gdk_display_atom (display, "XdndStatus", FALSE),

	xdnd_status_filter, NULL);
  gdk_add_client_message_filter (
	gdk_display_atom (display, "XdndFinished", FALSE),

	xdnd_finished_filter, NULL);
  gdk_add_client_message_filter (
	gdk_display_atom (display, "XdndDrop", FALSE),

	xdnd_drop_filter, NULL);
}		      

/* Source side */

static void
gdk_drag_do_leave (GdkDragContext *context, guint32 time)
{
  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_MOTIF:
	  motif_send_leave (context, time);
	  break;
	case GDK_DRAG_PROTO_XDND:
	  xdnd_send_leave (context);
	  break;
	case GDK_DRAG_PROTO_ROOTWIN:
	case GDK_DRAG_PROTO_NONE:
	default:
	  break;
	}

      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
  GList *tmp_list;
  GdkDragContext *new_context;
  
  g_return_val_if_fail (window != NULL, NULL);

  new_context = gdk_drag_context_new ();
  new_context->is_source = TRUE;
  new_context->source_window = window;
  gdk_window_ref (window);

  tmp_list = g_list_last (targets);
  new_context->targets = NULL;
  while (tmp_list)
    {
      new_context->targets = g_list_prepend (new_context->targets,
					     tmp_list->data);
      tmp_list = tmp_list->prev;
    }

  new_context->actions = 0;

  return new_context;
}


guint32
gdk_drag_get_protocol (GdkDisplay *display,
		       guint32 xid,
		       GdkDragProtocol *protocol)
{
  guint32 retval;
  GdkScreenImplX11 *dnd_scr_impl = GDK_DISPLAY_DND_SCREEN(display);
  
  if ((retval = xdnd_check_dest_for_display (xid, display)))
    {
      *protocol = GDK_DRAG_PROTO_XDND;
      GDK_NOTE (DND, g_message ("Entering dnd window %#x\n", xid));
      return retval;
    }
  else if ((retval = motif_check_dest_for_display (xid, display)))
    {
      *protocol = GDK_DRAG_PROTO_MOTIF;
      GDK_NOTE (DND, g_message ("Entering motif window %#x\n", xid));
      return retval;
    }
  else
    {
      /* Check if this is a root window */

      gboolean rootwin = FALSE;
      gint old_warnings = gdk_error_warnings;
      Atom type = None;
      int format;
      unsigned long nitems, after;
      unsigned char *data;

      if (gdk_x11_display_impl_is_root_window(display,(Window) xid))
	rootwin = TRUE;

      gdk_error_warnings = 0;
      
      if (!rootwin)
	{
	  gdk_error_code = 0;

	  XGetWindowProperty (dnd_scr_impl->xdisplay, xid,
			  gdk_display_atom (display, "ENLIGHTENMENT_DESKTOP", FALSE),

			      0, 0, False, AnyPropertyType,
			      &type, &format, &nitems, &after, &data);
	  if ((gdk_error_code == 0) && type != None)
	    {
	      XFree (data);
	      rootwin = TRUE;
	    }
	}

      /* Until I find out what window manager the next one is for,
       * I'm leaving it commented out. It's supported in the
       * xscreensaver sources, though.
       */
#if 0
      if (!rootwin)
	{
	  gdk_error_code = 0;
	  
	  XGetWindowProperty (gdk_display, win,
			      gdk_atom_intern ("__SWM_VROOT", FALSE),
			      0, 0, False, AnyPropertyType,
			      &type, &format, &nitems, &data);
	  if ((gdk_error_code == 0) && type != None)
	    rootwin = TRUE;
	}
#endif      

      gdk_error_warnings = old_warnings;

      if (rootwin)
	{
	  *protocol = GDK_DRAG_PROTO_ROOTWIN;
	  return xid;
	}
    }

  *protocol = GDK_DRAG_PROTO_NONE;
  return GDK_NONE;
}

void
gdk_drag_find_window (GdkDragContext  *context,
		      GdkWindow       *drag_window,
		      gint             x_root,
		      gint             y_root,
		      GdkWindow      **dest_window,
		      GdkDragProtocol *protocol)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  Window dest;

  g_return_if_fail (context != NULL);

  if (!private->window_cache)
    private->window_cache = 
	    gdk_window_cache_new_for_display(GDK_WINDOW_DISPLAY(context->source_window));

  dest = get_client_window_at_coords (private->window_cache,
				      drag_window ? 
				      GDK_DRAWABLE_XID (drag_window) : GDK_NONE,
				      x_root, y_root);

  if (private->dest_xid != dest)
    {
      Window recipient;
      private->dest_xid = dest;

      /* Check if new destination accepts drags, and which protocol */

      /* There is some ugliness here. We actually need to pass
       * _three_ pieces of information to drag_motion - dest_window,
       * protocol, and the XID of the unproxied window. The first
       * two are passed explicitely, the third implicitly through
       * protocol->dest_xid.
       */
      if ((recipient = gdk_drag_get_protocol (GDK_WINDOW_DISPLAY(context->source_window), 
			 		      dest,
			 		      protocol)))
	{
	  *dest_window = gdk_window_lookup (recipient);
	  if (*dest_window)
	    gdk_window_ref (*dest_window);
	  else
	    *dest_window = 
	      gdk_window_foreign_new_for_display (GDK_WINDOW_DISPLAY(context->source_window),
	    					  recipient);
	}
      else
	*dest_window = NULL;
    }
  else
    {
      *dest_window = context->dest_window;
      if (*dest_window)
	gdk_window_ref (*dest_window);
      *protocol = context->protocol;
    }
}

gboolean        
gdk_drag_motion (GdkDragContext *context,
		 GdkWindow      *dest_window,
		 GdkDragProtocol protocol,
		 gint            x_root, 
		 gint            y_root,
		 GdkDragAction   suggested_action,
		 GdkDragAction   possible_actions,
		 guint32         time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  g_return_val_if_fail (context != NULL, FALSE);

  /* When we have a Xdnd target, make sure our XdndActionList
   * matches the current actions;
   */
  private->old_actions = context->actions;
  context->actions = possible_actions;
  
  if ((protocol == GDK_DRAG_PROTO_XDND) &&
      (!private->xdnd_actions_set ||
       private->xdnd_actions != possible_actions))
    xdnd_set_actions (context);

  if (context->dest_window != dest_window)
    {
      GdkEvent temp_event;

      /* Send a leave to the last destination */
      gdk_drag_do_leave (context, time);
      private->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */

      if (dest_window)
	{
	  context->dest_window = dest_window;
	  private->drop_xid = private->dest_xid;
	  gdk_window_ref (context->dest_window);
	  context->protocol = protocol;

	  switch (protocol)
	    {
	    case GDK_DRAG_PROTO_MOTIF:
	      motif_send_enter (context, time);
	      break;

	    case GDK_DRAG_PROTO_XDND:
	      xdnd_send_enter (context);
	      break;

	    case GDK_DRAG_PROTO_ROOTWIN:
	    case GDK_DRAG_PROTO_NONE:
	    default:
	      break;
	    }
	  private->old_action = suggested_action;
	  context->suggested_action = suggested_action;
	  private->old_actions = possible_actions;
	}
      else
	{
	  context->dest_window = NULL;
	  private->drop_xid = None;
	  context->action = 0;
	}

      /* Push a status event, to let the client know that
       * the drag changed 
       */

      temp_event.dnd.type = GDK_DRAG_STATUS;
      temp_event.dnd.window = context->source_window;
      /* We use this to signal a synthetic status. Perhaps
       * we should use an extra field...
       */
      temp_event.dnd.send_event = TRUE;

      temp_event.dnd.context = context;
      temp_event.dnd.time = time;

      gdk_event_put (&temp_event);
    }
  else
    {
      private->old_action = context->suggested_action;
      context->suggested_action = suggested_action;
    }

  /* Send a drag-motion event */

  private->last_x = x_root;
  private->last_y = y_root;
      
  if (context->dest_window)
    {
      if (private->drag_status == GDK_DRAG_STATUS_DRAG)
	{
	  switch (context->protocol)
	    {
	    case GDK_DRAG_PROTO_MOTIF:
	      motif_send_motion (context, x_root, y_root, suggested_action, time);
	      break;
	      
	    case GDK_DRAG_PROTO_XDND:
	      xdnd_send_motion (context, x_root, y_root, suggested_action, time);
	      break;

	    case GDK_DRAG_PROTO_ROOTWIN:
	      {
		GdkEvent temp_event;

		if (g_list_find (context->targets,
				 GUINT_TO_POINTER (gdk_display_atom (GDK_WINDOW_DISPLAY(context->source_window), "application/x-rootwin-drop", FALSE))))

		  context->action = context->suggested_action;
		else
		  context->action = 0;

		temp_event.dnd.type = GDK_DRAG_STATUS;
		temp_event.dnd.window = context->source_window;
		temp_event.dnd.send_event = FALSE;
		temp_event.dnd.context = context;
		temp_event.dnd.time = time;

		gdk_event_put (&temp_event);
	      }
	      break;
	    case GDK_DRAG_PROTO_NONE:
	      g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_motion()");
	      break;
	    default:
	      break;
	    }
	}
      else
	return TRUE;
    }

  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_MOTIF:
	  motif_send_leave (context, time);
	  motif_send_drop (context, time);
	  break;
	  
	case GDK_DRAG_PROTO_XDND:
	  xdnd_send_drop (context, time);
	  break;

	case GDK_DRAG_PROTO_ROOTWIN:
	  g_warning ("Drops for GDK_DRAG_PROTO_ROOTWIN must be handled internally");
	  break;
	case GDK_DRAG_PROTO_NONE:
	  g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_drop()");
	  break;
	default:
	  break;
	}
    }
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  gdk_drag_do_leave (context, time);
}

/* Destination side */

void             
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  GdkDragContextPrivateX11 *private;
  XEvent xev;
  GdkDisplay *display;

  g_return_if_fail (context != NULL);

  private = PRIVATE_DATA (context);

  context->action = action;

  display = GDK_DRAWABLE_DISPLAY(context->source_window);
  
  if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_display_atom (display, "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE);

      xev.xclient.format = 8;
      xev.xclient.window = GDK_DRAWABLE_XID (context->source_window);
      xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->source_window);

      if (private->drag_status == GDK_DRAG_STATUS_ACTION_WAIT)
	{
	  MOTIF_XCLIENT_BYTE (&xev, 0) = XmOPERATION_CHANGED | 0x80;
	}
      else
	{
	  if ((action != 0) != (private->old_action != 0))
	    {
	      if (action != 0)
		MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_SITE_ENTER | 0x80;
	      else
		MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_SITE_LEAVE | 0x80;
	    }
	  else
	    MOTIF_XCLIENT_BYTE (&xev, 0) = XmDRAG_MOTION | 0x80;
	}

      MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;

      switch (action)
	{
	case GDK_ACTION_MOVE:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_MOVE;
	  break;
	case GDK_ACTION_COPY:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_COPY;
	  break;
	case GDK_ACTION_LINK:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_LINK;
	  break;
	default:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_NOOP;
	  break;
	}

      if (action)
	MOTIF_XCLIENT_SHORT (&xev, 1) |= (XmDROP_SITE_VALID << 4);
      else
	MOTIF_XCLIENT_SHORT (&xev, 1) |= (XmNO_DROP_SITE << 4);

      MOTIF_XCLIENT_LONG (&xev, 1) = time;
      MOTIF_XCLIENT_SHORT (&xev, 4) = private->last_x;
      MOTIF_XCLIENT_SHORT (&xev, 5) = private->last_y;

      if (!gdk_send_xevent (GDK_DRAWABLE_XID (context->source_window),
		       FALSE, 0, &xev))
	GDK_NOTE (DND, 
		  g_message ("Send event to %lx failed",
			     GDK_DRAWABLE_XID (context->source_window)));
    }
  else if (context->protocol == GDK_DRAG_PROTO_XDND)
    {
      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_display_atom (display, "XdndStatus", FALSE);

      xev.xclient.format = 32;
      xev.xclient.window = GDK_DRAWABLE_XID (context->source_window);
      xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->source_window);

      xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->dest_window);
      xev.xclient.data.l[1] = (action != 0) ? (2 | 1) : 0;
      xev.xclient.data.l[2] = 0;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = xdnd_action_to_atom_for_display (
			    action,GDK_DRAWABLE_DISPLAY(context->source_window));
      if (!xdnd_send_xevent_for_display (GDK_DRAWABLE_XID (context->source_window),
					 GDK_DRAWABLE_DISPLAY (context->source_window),
					 FALSE, &xev))
	GDK_NOTE (DND, 
		  g_message ("Send event to %lx failed",
			     GDK_DRAWABLE_XID (context->source_window)));
    }

  private->old_action = action;
}

void 
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
  GdkDragContextPrivateX11 *private;

  g_return_if_fail (context != NULL);

  private = PRIVATE_DATA (context);
  
  if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->source_window);
      xev.xclient.message_type = gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->source_window), "_MOTIF_DRAG_AND_DROP_MESSAGE", FALSE);

      xev.xclient.format = 8;

      MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_START | 0x80;
      MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
      if (ok)
	MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_COPY | 
	                               (XmDROP_SITE_VALID << 4) |
	                               (XmDROP_NOOP << 8) |
	                               (XmDROP << 12);
      else
	MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_NOOP | 
	                               (XmNO_DROP_SITE << 4) |
	                               (XmDROP_NOOP << 8) |
	                               (XmDROP_CANCEL << 12);
      MOTIF_XCLIENT_SHORT (&xev, 2) = private->last_x;
      MOTIF_XCLIENT_SHORT (&xev, 3) = private->last_y;
      
      gdk_send_xevent (GDK_DRAWABLE_XID (context->source_window),
		       FALSE, 0, &xev);
    }
}

void             
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
  g_return_if_fail (context != NULL);

  if (context->protocol == GDK_DRAG_PROTO_XDND)
    {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_display_atom (GDK_DRAWABLE_DISPLAY(context->dest_window), "XdndFinished", FALSE);

      xev.xclient.format = 32;
      xev.xclient.window = GDK_DRAWABLE_XID (context->source_window);
      xev.xclient.display = GDK_DRAWABLE_XDISPLAY(context->dest_window);
      xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->dest_window);
      xev.xclient.data.l[1] = 0;
      xev.xclient.data.l[2] = 0;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = 0;

      if (!xdnd_send_xevent_for_display (GDK_DRAWABLE_XID (context->source_window),
					 GDK_DRAWABLE_DISPLAY(context->source_window),
					 FALSE, &xev))
	GDK_NOTE (DND, 
		  g_message ("Send event to %lx failed",
			     GDK_DRAWABLE_XID (context->source_window)));
    }
}


void            
gdk_window_register_dnd (GdkWindow      *window)
{
  static gulong xdnd_version = 3;
  MotifDragReceiverInfo info;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(GDK_DRAWABLE_DISPLAY(window));

  g_return_if_fail (window != NULL);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    return;
  else
    gdk_drawable_set_data (window, "gdk-dnd-registered", GINT_TO_POINTER(TRUE), NULL);
  
  /* Set Motif drag receiver information property */

  if (!dpy_impl->motif_drag_receiver_info_atom)
    dpy_impl->motif_drag_receiver_info_atom = 
	    gdk_display_atom (GDK_DRAWABLE_DISPLAY(window), "_MOTIF_DRAG_RECEIVER_INFO", FALSE);


  info.byte_order = local_byte_order;
  info.protocol_version = 0;
  info.protocol_style = XmDRAG_DYNAMIC;
  info.proxy_window = GDK_NONE;
  info.num_drop_sites = 0;
  info.total_size = sizeof(info);

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
		   dpy_impl->motif_drag_receiver_info_atom,
		   dpy_impl->motif_drag_receiver_info_atom,
		   8, PropModeReplace,
		   (guchar *)&info,
		   sizeof (info));

  /* Set XdndAware */

  if (!dpy_impl->xdnd_aware_atom)
    dpy_impl->xdnd_aware_atom = gdk_display_atom (GDK_DRAWABLE_DISPLAY(window), "XdndAware", FALSE);


  /* The property needs to be of type XA_ATOM, not XA_INTEGER. Blech */
  XChangeProperty (GDK_DRAWABLE_XDISPLAY (window), 
		   GDK_DRAWABLE_XID (window),
		   dpy_impl->xdnd_aware_atom, XA_ATOM,
		   32, PropModeReplace,
		   (guchar *)&xdnd_version, 1);
}

/*************************************************************
 * gdk_drag_get_selection:
 *     Returns the selection atom for the current source window
 *   arguments:
 *     
 *   results:
 *************************************************************/

GdkAtom       
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    return (PRIVATE_DATA (context))->motif_selection;
  else if (context->protocol == GDK_DRAG_PROTO_XDND)
    return (PRIVATE_DATA (context))->xdnd_selection;
  else 
    return GDK_NONE;
}

