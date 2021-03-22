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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkx11dnd.h"
#include "gdkdndprivate.h"

#include "gdkmain.h"
#include "gdkinternals.h"
#include "gdkasync.h"
#include "gdkproperty.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif

#include <string.h>

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
  gboolean shape_selected;
  gboolean shape_valid;
  cairo_region_t *shape;
} GdkCacheChild;

typedef struct {
  GList *children;
  GHashTable *child_hash;
  guint old_event_mask;
  GdkScreen *screen;
  gint ref_count;
} GdkWindowCache;


struct _GdkX11DragContext
{
  GdkDragContext context;

  gint start_x;                /* Where the drag started */
  gint start_y;
  guint16 last_x;              /* Coordinates from last event */
  guint16 last_y;
  GdkDragAction old_action;    /* The last action we sent to the source */
  GdkDragAction old_actions;   /* The last actions we sent to the source */
  GdkDragAction xdnd_actions;  /* What is currently set in XdndActionList */
  guint version;               /* Xdnd protocol version */

  GSList *window_caches;

  GdkWindow *drag_window;

  GdkWindow *ipc_window;
  GdkCursor *cursor;
  GdkSeat *grab_seat;
  GdkDragAction actions;
  GdkDragAction current_action;

  gint hot_x;
  gint hot_y;

  Window dest_xid;             /* The last window we looked up */
  Window drop_xid;             /* The (non-proxied) window that is receiving drops */
  guint xdnd_targets_set  : 1; /* Whether we've already set XdndTypeList */
  guint xdnd_actions_set  : 1; /* Whether we've already set XdndActionList */
  guint xdnd_have_actions : 1; /* Whether an XdndActionList was provided */
  guint drag_status       : 4; /* current status of drag */
  guint drop_failed       : 1; /* Whether the drop was unsuccessful */
};

struct _GdkX11DragContextClass
{
  GdkDragContextClass parent_class;
};

typedef struct {
  gint keysym;
  gint modifiers;
} GrabKey;

static GrabKey grab_keys[] = {
  { XK_Escape, 0 },
  { XK_space, 0 },
  { XK_KP_Space, 0 },
  { XK_Return, 0 },
  { XK_KP_Enter, 0 },
  { XK_Up, 0 },
  { XK_Up, Mod1Mask },
  { XK_Down, 0 },
  { XK_Down, Mod1Mask },
  { XK_Left, 0 },
  { XK_Left, Mod1Mask },
  { XK_Right, 0 },
  { XK_Right, Mod1Mask },
  { XK_KP_Up, 0 },
  { XK_KP_Up, Mod1Mask },
  { XK_KP_Down, 0 },
  { XK_KP_Down, Mod1Mask },
  { XK_KP_Left, 0 },
  { XK_KP_Left, Mod1Mask },
  { XK_KP_Right, 0 },
  { XK_KP_Right, Mod1Mask }
};

/* Forward declarations */

static GdkWindowCache *gdk_window_cache_get   (GdkScreen      *screen);
static GdkWindowCache *gdk_window_cache_ref   (GdkWindowCache *cache);
static void            gdk_window_cache_unref (GdkWindowCache *cache);

static GdkFilterReturn xdnd_enter_filter    (GdkXEvent *xev,
                                             GdkEvent  *event,
                                             gpointer   data);
static GdkFilterReturn xdnd_leave_filter    (GdkXEvent *xev,
                                             GdkEvent  *event,
                                             gpointer   data);
static GdkFilterReturn xdnd_position_filter (GdkXEvent *xev,
                                             GdkEvent  *event,
                                             gpointer   data);
static GdkFilterReturn xdnd_status_filter   (GdkXEvent *xev,
                                             GdkEvent  *event,
                                             gpointer   data);
static GdkFilterReturn xdnd_finished_filter (GdkXEvent *xev,
                                             GdkEvent  *event,
                                             gpointer   data);
static GdkFilterReturn xdnd_drop_filter     (GdkXEvent *xev,
                                             GdkEvent  *event,
                                             gpointer   data);

static void   xdnd_manage_source_filter (GdkDragContext *context,
                                         GdkWindow      *window,
                                         gboolean        add_filter);

gboolean gdk_x11_drag_context_handle_event (GdkDragContext *context,
                                            const GdkEvent *event);
void     gdk_x11_drag_context_action_changed (GdkDragContext *context,
                                              GdkDragAction   action);

static GList *contexts;
static GSList *window_caches;

static const struct {
  const char *atom_name;
  GdkFilterFunc func;
} xdnd_filters[] = {
  { "XdndEnter",                    xdnd_enter_filter },
  { "XdndLeave",                    xdnd_leave_filter },
  { "XdndPosition",                 xdnd_position_filter },
  { "XdndStatus",                   xdnd_status_filter },
  { "XdndFinished",                 xdnd_finished_filter },
  { "XdndDrop",                     xdnd_drop_filter },
};


G_DEFINE_TYPE (GdkX11DragContext, gdk_x11_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_x11_drag_context_init (GdkX11DragContext *context)
{
  contexts = g_list_prepend (contexts, context);
}

static void        gdk_x11_drag_context_finalize (GObject *object);
static GdkWindow * gdk_x11_drag_context_find_window (GdkDragContext  *context,
                                                     GdkWindow       *drag_window,
                                                     GdkScreen       *screen,
                                                     gint             x_root,
                                                     gint             y_root,
                                                     GdkDragProtocol *protocol);
static gboolean    gdk_x11_drag_context_drag_motion (GdkDragContext  *context,
                                                     GdkWindow       *dest_window,
                                                     GdkDragProtocol  protocol,
                                                     gint             x_root,
                                                     gint             y_root,
                                                     GdkDragAction    suggested_action,
                                                     GdkDragAction    possible_actions,
                                                     guint32          time);
static void        gdk_x11_drag_context_drag_status (GdkDragContext  *context,
                                                     GdkDragAction    action,
                                                     guint32          time_);
static void        gdk_x11_drag_context_drag_abort  (GdkDragContext  *context,
                                                     guint32          time_);
static void        gdk_x11_drag_context_drag_drop   (GdkDragContext  *context,
                                                     guint32          time_);
static void        gdk_x11_drag_context_drop_reply  (GdkDragContext  *context,
                                                     gboolean         accept,
                                                     guint32          time_);
static void        gdk_x11_drag_context_drop_finish (GdkDragContext  *context,
                                                     gboolean         success,
                                                     guint32          time_);
static gboolean    gdk_x11_drag_context_drop_status (GdkDragContext  *context);
static GdkAtom     gdk_x11_drag_context_get_selection (GdkDragContext  *context);
static GdkWindow * gdk_x11_drag_context_get_drag_window (GdkDragContext *context);
static void        gdk_x11_drag_context_set_hotspot (GdkDragContext  *context,
                                                     gint             hot_x,
                                                     gint             hot_y);
static void        gdk_x11_drag_context_drop_done     (GdkDragContext  *context,
                                                       gboolean         success);
static gboolean    gdk_x11_drag_context_manage_dnd     (GdkDragContext *context,
                                                        GdkWindow      *window,
                                                        GdkDragAction   actions);
static void        gdk_x11_drag_context_set_cursor     (GdkDragContext *context,
                                                        GdkCursor      *cursor);
static void        gdk_x11_drag_context_cancel         (GdkDragContext      *context,
                                                        GdkDragCancelReason  reason);
static void        gdk_x11_drag_context_drop_performed (GdkDragContext *context,
                                                        guint32         time);

static void
gdk_x11_drag_context_class_init (GdkX11DragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_x11_drag_context_finalize;

  context_class->find_window = gdk_x11_drag_context_find_window;
  context_class->drag_status = gdk_x11_drag_context_drag_status;
  context_class->drag_motion = gdk_x11_drag_context_drag_motion;
  context_class->drag_abort = gdk_x11_drag_context_drag_abort;
  context_class->drag_drop = gdk_x11_drag_context_drag_drop;
  context_class->drop_reply = gdk_x11_drag_context_drop_reply;
  context_class->drop_finish = gdk_x11_drag_context_drop_finish;
  context_class->drop_status = gdk_x11_drag_context_drop_status;
  context_class->get_selection = gdk_x11_drag_context_get_selection;
  context_class->get_drag_window = gdk_x11_drag_context_get_drag_window;
  context_class->set_hotspot = gdk_x11_drag_context_set_hotspot;
  context_class->drop_done = gdk_x11_drag_context_drop_done;
  context_class->manage_dnd = gdk_x11_drag_context_manage_dnd;
  context_class->set_cursor = gdk_x11_drag_context_set_cursor;
  context_class->cancel = gdk_x11_drag_context_cancel;
  context_class->drop_performed = gdk_x11_drag_context_drop_performed;
  context_class->handle_event = gdk_x11_drag_context_handle_event;
  context_class->action_changed = gdk_x11_drag_context_action_changed;
}

static void
gdk_x11_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (object);
  GdkWindow *drag_window;

  if (context->source_window)
    {
      if ((context->protocol == GDK_DRAG_PROTO_XDND) && !context->is_source)
        xdnd_manage_source_filter (context, context->source_window, FALSE);
    }

  g_slist_free_full (x11_context->window_caches, (GDestroyNotify)gdk_window_cache_unref);
  x11_context->window_caches = NULL;

  contexts = g_list_remove (contexts, context);

  drag_window = context->drag_window;

  G_OBJECT_CLASS (gdk_x11_drag_context_parent_class)->finalize (object);

  if (drag_window)
    gdk_window_destroy (drag_window);
}

/* Drag Contexts */

static GdkDragContext *
gdk_drag_context_find (GdkDisplay *display,
                       gboolean    is_source,
                       Window      source_xid,
                       Window      dest_xid)
{
  GList *tmp_list;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;
  Window context_dest_xid;

  for (tmp_list = contexts; tmp_list; tmp_list = tmp_list->next)
    {
      context = (GdkDragContext *)tmp_list->data;
      context_x11 = (GdkX11DragContext *)context;

      if ((context->source_window && gdk_window_get_display (context->source_window) != display) ||
          (context->dest_window && gdk_window_get_display (context->dest_window) != display))
        continue;

      context_dest_xid = context->dest_window
                            ? (context_x11->drop_xid
                                  ? context_x11->drop_xid
                                  : GDK_WINDOW_XID (context->dest_window))
                            : None;

      if ((!context->is_source == !is_source) &&
          ((source_xid == None) || (context->source_window &&
            (GDK_WINDOW_XID (context->source_window) == source_xid))) &&
          ((dest_xid == None) || (context_dest_xid == dest_xid)))
        return context;
    }

  return NULL;
}

static void
precache_target_list (GdkDragContext *context)
{
  if (context->targets)
    {
      GPtrArray *targets = g_ptr_array_new ();
      GList *tmp_list;
      int i;

      for (tmp_list = context->targets; tmp_list; tmp_list = tmp_list->next)
        g_ptr_array_add (targets, gdk_atom_name (GDK_POINTER_TO_ATOM (tmp_list->data)));

      _gdk_x11_precache_atoms (GDK_WINDOW_DISPLAY (context->source_window),
                               (const gchar **)targets->pdata,
                               targets->len);

      for (i =0; i < targets->len; i++)
        g_free (targets->pdata[i]);

      g_ptr_array_free (targets, TRUE);
    }
}

/* Utility functions */

static void
free_cache_child (GdkCacheChild *child,
                  GdkDisplay    *display)
{
  if (child->shape)
    cairo_region_destroy (child->shape);

  if (child->shape_selected && display)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

      XShapeSelectInput (display_x11->xdisplay, child->xid, 0);
    }

  g_free (child);
}

static void
gdk_window_cache_add (GdkWindowCache *cache,
                      guint32         xid,
                      gint            x,
                      gint            y,
                      gint            width,
                      gint            height,
                      gboolean        mapped)
{
  GdkCacheChild *child = g_new (GdkCacheChild, 1);

  child->xid = xid;
  child->x = x;
  child->y = y;
  child->width = width;
  child->height = height;
  child->mapped = mapped;
  child->shape_selected = FALSE;
  child->shape_valid = FALSE;
  child->shape = NULL;

  cache->children = g_list_prepend (cache->children, child);
  g_hash_table_insert (cache->child_hash, GUINT_TO_POINTER (xid),
                       cache->children);
}

static GdkFilterReturn
gdk_window_cache_shape_filter (GdkXEvent *xev,
                               GdkEvent  *event,
                               gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkWindowCache *cache = data;

  GdkX11Display *display = GDK_X11_DISPLAY (gdk_screen_get_display (cache->screen));

  if (display->have_shapes &&
      xevent->type == display->shape_event_base + ShapeNotify)
    {
      XShapeEvent *xse = (XShapeEvent*)xevent;
      GList *node;

      node = g_hash_table_lookup (cache->child_hash,
                                  GUINT_TO_POINTER (xse->window));
      if (node)
        {
          GdkCacheChild *child = node->data;
          child->shape_valid = FALSE;
          if (child->shape)
            {
              cairo_region_destroy (child->shape);
              child->shape = NULL;
            }
        }

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
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
                if (above_node && node->next != above_node)
                  {
                    /* Put the window above (before in the list) above_node */
                    cache->children = g_list_remove_link (cache->children, node);
                    node->prev = above_node->prev;
                    if (node->prev)
                      node->prev->next = node;
                    else
                      cache->children = node;
                    node->next = above_node;
                    above_node->prev = node;
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
            GdkCacheChild *child = node->data;

            g_hash_table_remove (cache->child_hash,
                                 GUINT_TO_POINTER (xdwe->window));
            cache->children = g_list_remove_link (cache->children, node);
            /* window is destroyed, no need to disable ShapeNotify */
            free_cache_child (child, NULL);
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
gdk_window_cache_new (GdkScreen *screen)
{
  XWindowAttributes xwa;
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  GdkWindow *root_window = gdk_screen_get_root_window (screen);
  GdkChildInfoX11 *children;
  guint nchildren, i;
#ifdef HAVE_XCOMPOSITE
  Window cow;
#endif

  GdkWindowCache *result = g_new (GdkWindowCache, 1);

  result->children = NULL;
  result->child_hash = g_hash_table_new (g_direct_hash, NULL);
  result->screen = screen;
  result->ref_count = 1;

  XGetWindowAttributes (xdisplay, GDK_WINDOW_XID (root_window), &xwa);
  result->old_event_mask = xwa.your_event_mask;

  if (G_UNLIKELY (!GDK_X11_DISPLAY (GDK_X11_SCREEN (screen)->display)->trusted_client))
    {
      GList *toplevel_windows, *list;
      GdkWindow *window;
      GdkWindowImplX11 *impl;
      gint x, y, width, height;

      toplevel_windows = gdk_screen_get_toplevel_windows (screen);
      for (list = toplevel_windows; list; list = list->next)
        {
          window = GDK_WINDOW (list->data);
	  impl = GDK_WINDOW_IMPL_X11 (window->impl);
          gdk_window_get_geometry (window, &x, &y, &width, &height);
          gdk_window_cache_add (result, GDK_WINDOW_XID (window),
                                x * impl->window_scale, y * impl->window_scale, 
				width * impl->window_scale, 
				height * impl->window_scale,
                                gdk_window_is_visible (window));
        }
      g_list_free (toplevel_windows);
      return result;
    }

  XSelectInput (xdisplay, GDK_WINDOW_XID (root_window),
                result->old_event_mask | SubstructureNotifyMask);
  gdk_window_add_filter (root_window, gdk_window_cache_filter, result);
  gdk_window_add_filter (NULL, gdk_window_cache_shape_filter, result);

  if (!_gdk_x11_get_window_child_info (gdk_screen_get_display (screen),
                                       GDK_WINDOW_XID (root_window),
                                       FALSE, NULL,
                                       &children, &nchildren))
    return result;

  for (i = 0; i < nchildren ; i++)
    {
      gdk_window_cache_add (result, children[i].window,
                            children[i].x, children[i].y, children[i].width, children[i].height,
                            children[i].is_mapped);
    }

  g_free (children);

#ifdef HAVE_XCOMPOSITE
  /*
   * Add the composite overlay window to the cache, as this can be a reasonable
   * Xdnd proxy as well.
   * This is only done when the screen is composited in order to avoid mapping
   * the COW. We assume that the CM is using the COW (which is true for pretty
   * much any CM currently in use).
   */
  if (gdk_screen_is_composited (screen))
    {
      gdk_x11_display_error_trap_push (GDK_X11_SCREEN (screen)->display);
      cow = XCompositeGetOverlayWindow (xdisplay, GDK_WINDOW_XID (root_window));
      gdk_window_cache_add (result, cow, 0, 0, 
			    gdk_x11_screen_get_width (screen) * GDK_X11_SCREEN(screen)->window_scale, 
			    gdk_x11_screen_get_height (screen) * GDK_X11_SCREEN(screen)->window_scale, 
			    TRUE);
      XCompositeReleaseOverlayWindow (xdisplay, GDK_WINDOW_XID (root_window));
      gdk_x11_display_error_trap_pop_ignored (GDK_X11_SCREEN (screen)->display);
    }
#endif

  return result;
}

static void
gdk_window_cache_destroy (GdkWindowCache *cache)
{
  GdkWindow *root_window = gdk_screen_get_root_window (cache->screen);
  GdkDisplay *display;

  XSelectInput (GDK_WINDOW_XDISPLAY (root_window),
                GDK_WINDOW_XID (root_window),
                cache->old_event_mask);
  gdk_window_remove_filter (root_window, gdk_window_cache_filter, cache);
  gdk_window_remove_filter (NULL, gdk_window_cache_shape_filter, cache);

  display = gdk_screen_get_display (cache->screen);

  gdk_x11_display_error_trap_push (display);
  g_list_foreach (cache->children, (GFunc)free_cache_child, display);
  gdk_x11_display_error_trap_pop_ignored (display);

  g_list_free (cache->children);
  g_hash_table_destroy (cache->child_hash);

  g_free (cache);
}

static GdkWindowCache *
gdk_window_cache_ref (GdkWindowCache *cache)
{
  cache->ref_count += 1;

  return cache;
}

static void
gdk_window_cache_unref (GdkWindowCache *cache)
{
  g_assert (cache->ref_count > 0);

  cache->ref_count -= 1;

  if (cache->ref_count == 0)
    {
      window_caches = g_slist_remove (window_caches, cache);
      gdk_window_cache_destroy (cache);
    }
}

GdkWindowCache *
gdk_window_cache_get (GdkScreen *screen)
{
  GSList *list;
  GdkWindowCache *cache;

  for (list = window_caches; list; list = list->next)
    {
      cache = list->data;
      if (cache->screen == screen)
        return gdk_window_cache_ref (cache);
    }

  cache = gdk_window_cache_new (screen);

  window_caches = g_slist_prepend (window_caches, cache);

  return cache;
}

static gboolean
is_pointer_within_shape (GdkDisplay    *display,
                         GdkCacheChild *child,
                         gint           x_pos,
                         gint           y_pos)
{
  if (!child->shape_selected)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

      XShapeSelectInput (display_x11->xdisplay, child->xid, ShapeNotifyMask);
      child->shape_selected = TRUE;
    }
  if (!child->shape_valid)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
      cairo_region_t *input_shape;

      child->shape = NULL;
      if (gdk_display_supports_shapes (display))
        child->shape = _gdk_x11_xwindow_get_shape (display_x11->xdisplay,
                                                   child->xid, 1,  ShapeBounding);
#ifdef ShapeInput
      input_shape = NULL;
      if (gdk_display_supports_input_shapes (display))
        input_shape = _gdk_x11_xwindow_get_shape (display_x11->xdisplay,
                                                  child->xid, 1, ShapeInput);

      if (child->shape && input_shape)
        {
          cairo_region_intersect (child->shape, input_shape);
          cairo_region_destroy (input_shape);
        }
      else if (input_shape)
        {
          child->shape = input_shape;
        }
#endif

      child->shape_valid = TRUE;
    }

  return child->shape == NULL ||
         cairo_region_contains_point (child->shape, x_pos, y_pos);
}

static Window
get_client_window_at_coords_recurse (GdkDisplay *display,
                                     Window      win,
                                     gboolean    is_toplevel,
                                     gint        x,
                                     gint        y)
{
  GdkChildInfoX11 *children;
  unsigned int nchildren;
  int i;
  gboolean found_child = FALSE;
  GdkChildInfoX11 child = { 0, };
  gboolean has_wm_state = FALSE;

  if (!_gdk_x11_get_window_child_info (display, win, TRUE,
                                       is_toplevel? &has_wm_state : NULL,
                                       &children, &nchildren))
    return None;

  if (has_wm_state)
    {
      g_free (children);

      return win;
    }

  for (i = nchildren - 1; (i >= 0) && !found_child; i--)
    {
      GdkChildInfoX11 *cur_child = &children[i];

      if ((cur_child->is_mapped) && (cur_child->window_class == InputOutput) &&
          (x >= cur_child->x) && (x < cur_child->x + cur_child->width) &&
          (y >= cur_child->y) && (y < cur_child->y + cur_child->height))
        {
          x -= cur_child->x;
          y -= cur_child->y;
          child = *cur_child;
          found_child = TRUE;
        }
    }

  g_free (children);

  if (found_child)
    {
      if (child.has_wm_state)
        return child.window;
      else
        return get_client_window_at_coords_recurse (display, child.window, FALSE, x, y);
    }
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
  GdkDisplay *display;

  display = gdk_screen_get_display (cache->screen);

  gdk_x11_display_error_trap_push (display);

  tmp_list = cache->children;

  while (tmp_list && !retval)
    {
      GdkCacheChild *child = tmp_list->data;

      if ((child->xid != ignore) && (child->mapped))
        {
          if ((x_root >= child->x) && (x_root < child->x + child->width) &&
              (y_root >= child->y) && (y_root < child->y + child->height))
            {
              if (!is_pointer_within_shape (display, child,
                                            x_root - child->x,
                                            y_root - child->y))
                {
                  tmp_list = tmp_list->next;
                  continue;
                }

              retval = get_client_window_at_coords_recurse (display,
                  child->xid, TRUE,
                  x_root - child->x,
                  y_root - child->y);
              if (!retval)
                retval = child->xid;
            }
        }
      tmp_list = tmp_list->next;
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  if (retval)
    return retval;
  else
    return GDK_WINDOW_XID (gdk_screen_get_root_window (cache->screen));
}

#ifdef G_ENABLE_DEBUG
static void
print_target_list (GList *targets)
{
  while (targets)
    {
      gchar *name = gdk_atom_name (GDK_POINTER_TO_ATOM (targets->data));
      g_message ("\t%s", name);
      g_free (name);
      targets = targets->next;
    }
}
#endif /* G_ENABLE_DEBUG */

/*************************************************************
 ***************************** XDND **************************
 *************************************************************/

/* Utility functions */

static struct {
  const gchar *name;
  GdkAtom atom;
  GdkDragAction action;
} xdnd_actions_table[] = {
    { "XdndActionCopy",    None, GDK_ACTION_COPY },
    { "XdndActionMove",    None, GDK_ACTION_MOVE },
    { "XdndActionLink",    None, GDK_ACTION_LINK },
    { "XdndActionAsk",     None, GDK_ACTION_ASK  },
    { "XdndActionPrivate", None, GDK_ACTION_COPY },
  };

static const gint xdnd_n_actions = G_N_ELEMENTS (xdnd_actions_table);
static gboolean xdnd_actions_initialized = FALSE;

static void
xdnd_initialize_actions (void)
{
  gint i;

  xdnd_actions_initialized = TRUE;
  for (i = 0; i < xdnd_n_actions; i++)
    xdnd_actions_table[i].atom = gdk_atom_intern_static_string (xdnd_actions_table[i].name);
}

static GdkDragAction
xdnd_action_from_atom (GdkDisplay *display,
                       Atom        xatom)
{
  GdkAtom atom;
  gint i;

  if (xatom == None)
    return 0;

  atom = gdk_x11_xatom_to_atom_for_display (display, xatom);

  if (!xdnd_actions_initialized)
    xdnd_initialize_actions();

  for (i = 0; i < xdnd_n_actions; i++)
    if (atom == xdnd_actions_table[i].atom)
      return xdnd_actions_table[i].action;

  return 0;
}

static Atom
xdnd_action_to_atom (GdkDisplay    *display,
                     GdkDragAction  action)
{
  gint i;

  if (!xdnd_actions_initialized)
    xdnd_initialize_actions();

  for (i = 0; i < xdnd_n_actions; i++)
    if (action == xdnd_actions_table[i].action)
      return gdk_x11_atom_to_xatom_for_display (display, xdnd_actions_table[i].atom);

  return None;
}

/* Source side */

static GdkFilterReturn
xdnd_status_filter (GdkXEvent *xev,
                    GdkEvent  *event,
                    gpointer   data)
{
  GdkDisplay *display;
  XEvent *xevent = (XEvent *)xev;
  guint32 dest_window = xevent->xclient.data.l[0];
  guint32 flags = xevent->xclient.data.l[1];
  Atom action = xevent->xclient.data.l[4];
  GdkDragContext *context;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  GDK_NOTE (DND,
            g_message ("XdndStatus: dest_window: %#x  action: %ld",
                       dest_window, action));

  display = gdk_window_get_display (event->any.window);
  context = gdk_drag_context_find (display, TRUE, xevent->xclient.window, dest_window);

  if (context)
    {
      GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
      if (context_x11->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
        context_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      event->dnd.send_event = FALSE;
      event->dnd.type = GDK_DRAG_STATUS;
      event->dnd.context = context;
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      g_object_ref (context);

      event->dnd.time = GDK_CURRENT_TIME; /* FIXME? */
      if (!(action != 0) != !(flags & 1))
        {
          GDK_NOTE (DND,
                    g_warning ("Received status event with flags not corresponding to action!"));
          action = 0;
        }

      context->action = xdnd_action_from_atom (display, action);

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
xdnd_finished_filter (GdkXEvent *xev,
                      GdkEvent  *event,
                      gpointer   data)
{
  GdkDisplay *display;
  XEvent *xevent = (XEvent *)xev;
  guint32 dest_window = xevent->xclient.data.l[0];
  GdkDragContext *context;
  GdkX11DragContext *context_x11;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  GDK_NOTE (DND,
            g_message ("XdndFinished: dest_window: %#x", dest_window));

  display = gdk_window_get_display (event->any.window);
  context = gdk_drag_context_find (display, TRUE, xevent->xclient.window, dest_window);

  if (context)
    {
      context_x11 = GDK_X11_DRAG_CONTEXT (context);
      if (context_x11->version == 5)
        context_x11->drop_failed = xevent->xclient.data.l[1] == 0;

      event->dnd.type = GDK_DROP_FINISHED;
      event->dnd.context = context;
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      g_object_ref (context);

      event->dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static void
xdnd_set_targets (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  Atom *atomlist;
  GList *tmp_list = context->targets;
  gint i;
  gint n_atoms = g_list_length (context->targets);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);

  atomlist = g_new (Atom, n_atoms);
  i = 0;
  while (tmp_list)
    {
      atomlist[i] = gdk_x11_atom_to_xatom_for_display (display, GDK_POINTER_TO_ATOM (tmp_list->data));
      tmp_list = tmp_list->next;
      i++;
    }

  XChangeProperty (GDK_WINDOW_XDISPLAY (context->source_window),
                   GDK_WINDOW_XID (context->source_window),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndTypeList"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  context_x11->xdnd_targets_set = 1;
}

static void
xdnd_set_actions (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  Atom *atomlist;
  gint i;
  gint n_atoms;
  guint actions;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);

  if (!xdnd_actions_initialized)
    xdnd_initialize_actions();

  actions = context->actions;
  n_atoms = 0;
  for (i = 0; i < xdnd_n_actions; i++)
    {
      if (actions & xdnd_actions_table[i].action)
        {
          actions &= ~xdnd_actions_table[i].action;
          n_atoms++;
        }
    }

  atomlist = g_new (Atom, n_atoms);

  actions = context->actions;
  n_atoms = 0;
  for (i = 0; i < xdnd_n_actions; i++)
    {
      if (actions & xdnd_actions_table[i].action)
        {
          actions &= ~xdnd_actions_table[i].action;
          atomlist[n_atoms] = gdk_x11_atom_to_xatom_for_display (display, xdnd_actions_table[i].atom);
          n_atoms++;
        }
    }

  XChangeProperty (GDK_WINDOW_XDISPLAY (context->source_window),
                   GDK_WINDOW_XID (context->source_window),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  context_x11->xdnd_actions_set = TRUE;
  context_x11->xdnd_actions = context->actions;
}

static void
send_client_message_async_cb (Window   window,
                              gboolean success,
                              gpointer data)
{
  GdkDragContext *context = data;
  GDK_NOTE (DND,
            g_message ("Got async callback for #%lx, success = %d",
                       window, success));

  /* On failure, we immediately continue with the protocol
   * so we don't end up blocking for a timeout
   */
  if (!success &&
      context->dest_window &&
      window == GDK_WINDOW_XID (context->dest_window))
    {
      GdkEvent *temp_event;
      GdkX11DragContext *context_x11 = data;

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
      context->action = 0;

      context_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      temp_event = gdk_event_new (GDK_DRAG_STATUS);
      temp_event->dnd.window = g_object_ref (context->source_window);
      temp_event->dnd.send_event = TRUE;
      temp_event->dnd.context = g_object_ref (context);
      temp_event->dnd.time = GDK_CURRENT_TIME;
      gdk_event_set_device (temp_event, gdk_drag_context_get_device (context));

      gdk_event_put (temp_event);

      gdk_event_free (temp_event);
    }

  g_object_unref (context);
}


static GdkDisplay *
gdk_drag_context_get_display (GdkDragContext *context)
{
  if (context->source_window)
    return GDK_WINDOW_DISPLAY (context->source_window);
  else if (context->dest_window)
    return GDK_WINDOW_DISPLAY (context->dest_window);

  g_assert_not_reached ();
  return NULL;
}

static void
send_client_message_async (GdkDragContext      *context,
                           Window               window,
                           gboolean             propagate,
                           glong                event_mask,
                           XClientMessageEvent *event_send)
{
  GdkDisplay *display = gdk_drag_context_get_display (context);

  g_object_ref (context);

  _gdk_x11_send_client_message_async (display, window,
                                      propagate, event_mask, event_send,
                                      send_client_message_async_cb, context);
}

static gboolean
xdnd_send_xevent (GdkX11DragContext *context_x11,
                  GdkWindow         *window,
                  gboolean           propagate,
                  XEvent            *event_send)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = gdk_drag_context_get_display (context);
  Window xwindow;
  glong event_mask;

  g_assert (event_send->xany.type == ClientMessage);

  /* We short-circuit messages to ourselves */
  if (gdk_window_get_window_type (window) != GDK_WINDOW_FOREIGN)
    {
      gint i;

      for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
        {
          if (gdk_x11_get_xatom_by_name_for_display (display, xdnd_filters[i].atom_name) ==
              event_send->xclient.message_type)
            {
              GdkEvent *temp_event;

              temp_event = gdk_event_new (GDK_NOTHING);
              temp_event->any.window = g_object_ref (window);

              if ((*xdnd_filters[i].func) (event_send, temp_event, NULL) == GDK_FILTER_TRANSLATE)
                gdk_event_put (temp_event);

              gdk_event_free (temp_event);

              return TRUE;
            }
        }
    }

  xwindow = GDK_WINDOW_XID (window);

  if (_gdk_x11_display_is_root_window (display, xwindow))
    event_mask = ButtonPressMask;
  else
    event_mask = 0;

  send_client_message_async (context, xwindow, propagate, event_mask,
                             &event_send->xclient);

  return TRUE;
}

static void
xdnd_send_enter (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->dest_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndEnter");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_WINDOW_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_WINDOW_XID (context->source_window);
  xev.xclient.data.l[1] = (context_x11->version << 24); /* version */
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  GDK_NOTE(DND,
           g_message ("Sending enter source window %#lx XDND protocol version %d\n",
                      GDK_WINDOW_XID (context->source_window), context_x11->version));
  if (g_list_length (context->targets) > 3)
    {
      if (!context_x11->xdnd_targets_set)
        xdnd_set_targets (context_x11);
      xev.xclient.data.l[1] |= 1;
    }
  else
    {
      GList *tmp_list = context->targets;
      gint i = 2;

      while (tmp_list)
        {
          xev.xclient.data.l[i] = gdk_x11_atom_to_xatom_for_display (display,
                                                                     GDK_POINTER_TO_ATOM (tmp_list->data));
          tmp_list = tmp_list->next;
          i++;
        }
    }

  if (!xdnd_send_xevent (context_x11, context->dest_window, FALSE, &xev))
    {
      GDK_NOTE (DND,
                g_message ("Send event to %lx failed",
                           GDK_WINDOW_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_leave (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndLeave");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_WINDOW_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_WINDOW_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent (context_x11, context->dest_window, FALSE, &xev))
    {
      GDK_NOTE (DND,
                g_message ("Send event to %lx failed",
                           GDK_WINDOW_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_drop (GdkX11DragContext *context_x11,
                guint32            time)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndDrop");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_WINDOW_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_WINDOW_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = time;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent (context_x11, context->dest_window, FALSE, &xev))
    {
      GDK_NOTE (DND,
                g_message ("Send event to %lx failed",
                           GDK_WINDOW_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_motion (GdkX11DragContext *context_x11,
                  gint               x_root,
                  gint               y_root,
                  GdkDragAction      action,
                  guint32            time)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndPosition");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_WINDOW_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_WINDOW_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (x_root << 16) | y_root;
  xev.xclient.data.l[3] = time;
  xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);

  if (!xdnd_send_xevent (context_x11, context->dest_window, FALSE, &xev))
    {
      GDK_NOTE (DND,
                g_message ("Send event to %lx failed",
                           GDK_WINDOW_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
  context_x11->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
}

static guint32
xdnd_check_dest (GdkDisplay *display,
                 Window      win,
                 guint      *xdnd_version)
{
  gboolean retval = FALSE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  guchar *data;
  Atom *version;
  Window *proxy_data;
  Window proxy;
  Atom xdnd_proxy_atom = gdk_x11_get_xatom_by_name_for_display (display, "XdndProxy");
  Atom xdnd_aware_atom = gdk_x11_get_xatom_by_name_for_display (display, "XdndAware");

  proxy = None;

  gdk_x11_display_error_trap_push (display);
  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), win,
                          xdnd_proxy_atom, 0,
                          1, False, AnyPropertyType,
                          &type, &format, &nitems, &after,
                          &data) == Success)
    {
      if (type != None)
        {
          proxy_data = (Window *)data;

          if ((format == 32) && (nitems == 1))
            {
              proxy = *proxy_data;
            }
          else
            {
              GDK_NOTE (DND,
                        g_warning ("Invalid XdndProxy property on window %ld", win));
            }

          XFree (proxy_data);
        }

      if ((XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), proxy ? proxy : win,
                               xdnd_aware_atom, 0,
                               1, False, AnyPropertyType,
                               &type, &format, &nitems, &after,
                               &data) == Success) &&
          type != None)
        {
          version = (Atom *)data;

          if ((format == 32) && (nitems == 1))
            {
              if (*version >= 3)
                retval = TRUE;
              if (xdnd_version)
                *xdnd_version = *version;
            }
          else
            {
              GDK_NOTE (DND,
                        g_warning ("Invalid XdndAware property on window %ld", win));
            }

          XFree (version);
        }
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  return retval ? (proxy ? proxy : win) : None;
}

/* Target side */

static void
xdnd_read_actions (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;
  gint i;

  context_x11->xdnd_have_actions = FALSE;

  if (gdk_window_get_window_type (context->source_window) == GDK_WINDOW_FOREIGN)
    {
      /* Get the XdndActionList, if set */

      gdk_x11_display_error_trap_push (display);
      if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                              GDK_WINDOW_XID (context->source_window),
                              gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList"),
                              0, 65536,
                              False, XA_ATOM, &type, &format, &nitems,
                              &after, &data) == Success &&
          type == XA_ATOM)
        {
          atoms = (Atom *)data;

          context->actions = 0;

          for (i = 0; i < nitems; i++)
            context->actions |= xdnd_action_from_atom (display, atoms[i]);

          context_x11->xdnd_have_actions = TRUE;

#ifdef G_ENABLE_DEBUG
          if (GDK_DEBUG_CHECK (DND))
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

        }

      if (data)
        XFree (data);

      gdk_x11_display_error_trap_pop_ignored (display);
    }
  else
    {
      /* Local drag
       */
      GdkDragContext *source_context;

      source_context = gdk_drag_context_find (display, TRUE,
                                              GDK_WINDOW_XID (context->source_window),
                                              GDK_WINDOW_XID (context->dest_window));

      if (source_context)
        {
          context->actions = source_context->actions;
          context_x11->xdnd_have_actions = TRUE;
        }
    }
}

/* We have to make sure that the XdndActionList we keep internally
 * is up to date with the XdndActionList on the source window
 * because we get no notification, because Xdnd wasn’t meant
 * to continually send actions. So we select on PropertyChangeMask
 * and add this filter.
 */
static GdkFilterReturn
xdnd_source_window_filter (GdkXEvent *xev,
                           GdkEvent  *event,
                           gpointer   cb_data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkX11DragContext *context_x11 = cb_data;
  GdkDisplay *display = GDK_WINDOW_DISPLAY(event->any.window);

  if ((xevent->xany.type == PropertyNotify) &&
      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList")))
    {
      xdnd_read_actions (context_x11);

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

static void
xdnd_manage_source_filter (GdkDragContext *context,
                           GdkWindow      *window,
                           gboolean        add_filter)
{
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_window_get_window_type (window) == GDK_WINDOW_FOREIGN)
    {
      gdk_x11_display_error_trap_push (GDK_WINDOW_DISPLAY (window));

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

      gdk_x11_display_error_trap_pop_ignored (GDK_WINDOW_DISPLAY (window));
    }
}

static void
base_precache_atoms (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (!display_x11->base_dnd_atoms_precached)
    {
      static const char *const precache_atoms[] = {
        "WM_STATE",
        "XdndAware",
        "XdndProxy"
      };

      _gdk_x11_precache_atoms (display,
                               precache_atoms, G_N_ELEMENTS (precache_atoms));

      display_x11->base_dnd_atoms_precached = TRUE;
    }
}

static void
xdnd_precache_atoms (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (!display_x11->xdnd_atoms_precached)
    {
      static const gchar *const precache_atoms[] = {
        "XdndActionAsk",
        "XdndActionCopy",
        "XdndActionLink",
        "XdndActionList",
        "XdndActionMove",
        "XdndActionPrivate",
        "XdndDrop",
        "XdndEnter",
        "XdndFinished",
        "XdndLeave",
        "XdndPosition",
        "XdndSelection",
        "XdndStatus",
        "XdndTypeList"
      };

      _gdk_x11_precache_atoms (display,
                               precache_atoms, G_N_ELEMENTS (precache_atoms));

      display_x11->xdnd_atoms_precached = TRUE;
    }
}

static GdkFilterReturn
xdnd_enter_filter (GdkXEvent *xev,
                   GdkEvent  *event,
                   gpointer   cb_data)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  XEvent *xevent = (XEvent *)xev;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;
  GdkSeat *seat;
  gint i;
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;
  guint32 source_window;
  gboolean get_types;
  gint version;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  source_window = xevent->xclient.data.l[0];
  get_types = ((xevent->xclient.data.l[1] & 1) != 0);
  version = (xevent->xclient.data.l[1] & 0xff000000) >> 24;

  display = GDK_WINDOW_DISPLAY (event->any.window);
  display_x11 = GDK_X11_DISPLAY (display);

  xdnd_precache_atoms (display);

  GDK_NOTE (DND,
            g_message ("XdndEnter: source_window: %#x, version: %#x",
                       source_window, version));

  if (version < 3)
    {
      /* Old source ignore */
      GDK_NOTE (DND, g_message ("Ignored old XdndEnter message"));
      return GDK_FILTER_REMOVE;
    }

  if (display_x11->current_dest_drag != NULL)
    {
      g_object_unref (display_x11->current_dest_drag);
      display_x11->current_dest_drag = NULL;
    }

  context_x11 = (GdkX11DragContext *)g_object_new (GDK_TYPE_X11_DRAG_CONTEXT, NULL);
  context = (GdkDragContext *)context_x11;

  context->display = display;
  context->protocol = GDK_DRAG_PROTO_XDND;
  context_x11->version = version;

  /* FIXME: Should extend DnD protocol to have device info */
  seat = gdk_display_get_default_seat (display);
  gdk_drag_context_set_device (context, gdk_seat_get_pointer (seat));

  context->source_window = gdk_x11_window_foreign_new_for_display (display, source_window);
  if (!context->source_window)
    {
      g_object_unref (context);
      return GDK_FILTER_REMOVE;
    }
  context->dest_window = event->any.window;
  g_object_ref (context->dest_window);

  context->targets = NULL;
  if (get_types)
    {
      gdk_x11_display_error_trap_push (display);
      XGetWindowProperty (GDK_WINDOW_XDISPLAY (event->any.window),
                          source_window,
                          gdk_x11_get_xatom_by_name_for_display (display, "XdndTypeList"),
                          0, 65536,
                          False, XA_ATOM, &type, &format, &nitems,
                          &after, &data);

      if (gdk_x11_display_error_trap_pop (display) || (format != 32) || (type != XA_ATOM))
        {
          g_object_unref (context);

          if (data)
            XFree (data);

          return GDK_FILTER_REMOVE;
        }

      atoms = (Atom *)data;

      for (i = 0; i < nitems; i++)
        context->targets =
          g_list_append (context->targets,
                         GDK_ATOM_TO_POINTER (gdk_x11_xatom_to_atom_for_display (display,
                                                                                 atoms[i])));

      XFree (atoms);
    }
  else
    {
      for (i = 0; i < 3; i++)
        if (xevent->xclient.data.l[2 + i])
          context->targets =
            g_list_append (context->targets,
                           GDK_ATOM_TO_POINTER (gdk_x11_xatom_to_atom_for_display (display,
                                                                                   xevent->xclient.data.l[2 + i])));
    }

#ifdef G_ENABLE_DEBUG
  if (GDK_DEBUG_CHECK (DND))
    print_target_list (context->targets);
#endif /* G_ENABLE_DEBUG */

  xdnd_manage_source_filter (context, context->source_window, TRUE);
  xdnd_read_actions (context_x11);

  event->dnd.type = GDK_DRAG_ENTER;
  event->dnd.context = context;
  gdk_event_set_device (event, gdk_drag_context_get_device (context));
  g_object_ref (context);

  display_x11->current_dest_drag = context;

  return GDK_FILTER_TRANSLATE;
}

static GdkFilterReturn
xdnd_leave_filter (GdkXEvent *xev,
                   GdkEvent  *event,
                   gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  GdkDisplay *display;
  GdkX11Display *display_x11;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  GDK_NOTE (DND,
            g_message ("XdndLeave: source_window: %#x",
                       source_window));

  display = GDK_WINDOW_DISPLAY (event->any.window);
  display_x11 = GDK_X11_DISPLAY (display);

  xdnd_precache_atoms (display);

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_WINDOW_XID (display_x11->current_dest_drag->source_window) == source_window))
    {
      event->dnd.type = GDK_DRAG_LEAVE;
      /* Pass ownership of context to the event */
      event->dnd.context = display_x11->current_dest_drag;
      gdk_event_set_device (event, gdk_drag_context_get_device (event->dnd.context));

      display_x11->current_dest_drag = NULL;

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
  GdkWindowImplX11 *impl;
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  gint16 x_root = xevent->xclient.data.l[2] >> 16;
  gint16 y_root = xevent->xclient.data.l[2] & 0xffff;
  guint32 time = xevent->xclient.data.l[3];
  Atom action = xevent->xclient.data.l[4];
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;

   if (!event->any.window ||
       gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
     return GDK_FILTER_CONTINUE;                        /* Not for us */

  GDK_NOTE (DND,
            g_message ("XdndPosition: source_window: %#x position: (%d, %d)  time: %d  action: %ld",
                       source_window, x_root, y_root, time, action));

  display = GDK_WINDOW_DISPLAY (event->any.window);
  display_x11 = GDK_X11_DISPLAY (display);

  xdnd_precache_atoms (display);

  context = display_x11->current_dest_drag;

  if ((context != NULL) &&
      (context->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_WINDOW_XID (context->source_window) == source_window))
    {
      impl = GDK_WINDOW_IMPL_X11 (event->any.window->impl);

      context_x11 = GDK_X11_DRAG_CONTEXT (context);

      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.context = context;
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      g_object_ref (context);

      event->dnd.time = time;

      context->suggested_action = xdnd_action_from_atom (display, action);

      if (!context_x11->xdnd_have_actions)
        context->actions = context->suggested_action;

      event->dnd.x_root = x_root / impl->window_scale;
      event->dnd.y_root = y_root / impl->window_scale;

      context_x11->last_x = x_root / impl->window_scale;
      context_x11->last_y = y_root / impl->window_scale;

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
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  GDK_NOTE (DND,
            g_message ("XdndDrop: source_window: %#x  time: %d",
                       source_window, time));

  display = GDK_WINDOW_DISPLAY (event->any.window);
  display_x11 = GDK_X11_DISPLAY (display);

  xdnd_precache_atoms (display);

  context = display_x11->current_dest_drag;

  if ((context != NULL) &&
      (context->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_WINDOW_XID (context->source_window) == source_window))
    {
      context_x11 = GDK_X11_DRAG_CONTEXT (context);
      event->dnd.type = GDK_DROP_START;

      event->dnd.context = context;
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      g_object_ref (context);

      event->dnd.time = time;
      event->dnd.x_root = context_x11->last_x;
      event->dnd.y_root = context_x11->last_y;

      gdk_x11_window_set_user_time (event->any.window, time);

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

GdkFilterReturn
_gdk_x11_dnd_filter (GdkXEvent *xev,
                     GdkEvent  *event,
                     gpointer   data)
{
  XEvent *xevent = (XEvent *) xev;
  GdkDisplay *display;
  int i;

  if (!GDK_IS_X11_WINDOW (event->any.window))
    return GDK_FILTER_CONTINUE;

  if (xevent->type != ClientMessage)
    return GDK_FILTER_CONTINUE;

  display = GDK_WINDOW_DISPLAY (event->any.window);

  for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
    {
      if (xevent->xclient.message_type != gdk_x11_get_xatom_by_name_for_display (display, xdnd_filters[i].atom_name))
        continue;

      return xdnd_filters[i].func (xev, event, data);
    }

  return GDK_FILTER_CONTINUE;
}

/* Source side */

static void
gdk_drag_do_leave (GdkX11DragContext *context_x11,
                   guint32            time)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);

  if (context->dest_window)
    {
      switch (context->protocol)
        {
        case GDK_DRAG_PROTO_XDND:
          xdnd_send_leave (context_x11);
          break;
        case GDK_DRAG_PROTO_ROOTWIN:
        case GDK_DRAG_PROTO_NONE:
        default:
          break;
        }

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static GdkWindow *
create_drag_window (GdkScreen *screen)
{
  GdkWindowAttr attrs = { 0 };
  guint mask;

  attrs.x = attrs.y = 0;
  attrs.width = attrs.height = 100;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.window_type = GDK_WINDOW_TEMP;
  attrs.type_hint = GDK_WINDOW_TYPE_HINT_DND;
  attrs.visual = gdk_screen_get_rgba_visual (screen);
  if (!attrs.visual)
    attrs.visual = gdk_screen_get_system_visual (screen);

  mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_TYPE_HINT;

  return gdk_window_new (gdk_screen_get_root_window (screen), &attrs, mask);
}

GdkDragContext *
_gdk_x11_window_drag_begin (GdkWindow *window,
                            GdkDevice *device,
                            GList     *targets,
                            gint       x_root,
                            gint       y_root)
{
  GdkDragContext *context;

  context = (GdkDragContext *) g_object_new (GDK_TYPE_X11_DRAG_CONTEXT, NULL);

  context->display = gdk_window_get_display (window);
  context->is_source = TRUE;
  context->source_window = window;
  g_object_ref (window);

  context->targets = g_list_copy (targets);
  precache_target_list (context);

  context->actions = 0;

  gdk_drag_context_set_device (context, device);

  GDK_X11_DRAG_CONTEXT (context)->start_x = x_root;
  GDK_X11_DRAG_CONTEXT (context)->start_y = y_root;
  GDK_X11_DRAG_CONTEXT (context)->last_x = x_root;
  GDK_X11_DRAG_CONTEXT (context)->last_y = y_root;

  GDK_X11_DRAG_CONTEXT (context)->drag_window = create_drag_window (gdk_window_get_screen (window));

  return context;
}

Window
_gdk_x11_display_get_drag_protocol (GdkDisplay      *display,
                                    Window           xid,
                                    GdkDragProtocol *protocol,
                                    guint           *version)

{
  GdkWindow *window;
  Window retval;

  base_precache_atoms (display);

  /* Check for a local drag */
  window = gdk_x11_window_lookup_for_display (display, xid);
  if (window && gdk_window_get_window_type (window) != GDK_WINDOW_FOREIGN)
    {
      if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
        {
          *protocol = GDK_DRAG_PROTO_XDND;
          *version = 5;
          xdnd_precache_atoms (display);
          GDK_NOTE (DND, g_message ("Entering local Xdnd window %#x\n", (guint) xid));
          return xid;
        }
      else if (_gdk_x11_display_is_root_window (display, xid))
        {
          *protocol = GDK_DRAG_PROTO_ROOTWIN;
          GDK_NOTE (DND, g_message ("Entering root window\n"));
          return xid;
        }
    }
  else if ((retval = xdnd_check_dest (display, xid, version)))
    {
      *protocol = GDK_DRAG_PROTO_XDND;
      xdnd_precache_atoms (display);
      GDK_NOTE (DND, g_message ("Entering Xdnd window %#x\n", (guint) xid));
      return retval;
    }
  else
    {
      /* Check if this is a root window */
      gboolean rootwin = FALSE;

      if (_gdk_x11_display_is_root_window (display, (Window) xid))
        rootwin = TRUE;

      if (rootwin)
        {
          GDK_NOTE (DND, g_message ("Entering root window\n"));
          *protocol = GDK_DRAG_PROTO_ROOTWIN;
          return xid;
        }
    }

  *protocol = GDK_DRAG_PROTO_NONE;

  return 0; /* a.k.a. None */
}

static GdkWindowCache *
drag_context_find_window_cache (GdkX11DragContext *context_x11,
                                GdkScreen         *screen)
{
  GSList *list;
  GdkWindowCache *cache;

  for (list = context_x11->window_caches; list; list = list->next)
    {
      cache = list->data;
      if (cache->screen == screen)
        return cache;
    }

  cache = gdk_window_cache_get (screen);
  context_x11->window_caches = g_slist_prepend (context_x11->window_caches, cache);

  return cache;
}

static GdkWindow *
gdk_x11_drag_context_find_window (GdkDragContext  *context,
                                  GdkWindow       *drag_window,
                                  GdkScreen       *screen,
                                  gint             x_root,
                                  gint             y_root,
                                  GdkDragProtocol *protocol)
{
  GdkX11Screen *screen_x11 = GDK_X11_SCREEN(screen);
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
  GdkWindowCache *window_cache;
  GdkDisplay *display;
  Window dest;
  GdkWindow *dest_window;

  display = GDK_WINDOW_DISPLAY (context->source_window);

  window_cache = drag_context_find_window_cache (context_x11, screen);

  dest = get_client_window_at_coords (window_cache,
                                      drag_window && GDK_WINDOW_IS_X11 (drag_window) ?
                                      GDK_WINDOW_XID (drag_window) : None,
                                      x_root * screen_x11->window_scale,
				      y_root * screen_x11->window_scale);

  if (context_x11->dest_xid != dest)
    {
      Window recipient;
      context_x11->dest_xid = dest;

      /* Check if new destination accepts drags, and which protocol */

      /* There is some ugliness here. We actually need to pass
       * _three_ pieces of information to drag_motion - dest_window,
       * protocol, and the XID of the unproxied window. The first
       * two are passed explicitly, the third implicitly through
       * protocol->dest_xid.
       */
      recipient = _gdk_x11_display_get_drag_protocol (display,
                                                      dest,
                                                      protocol,
                                                      &context_x11->version);

      if (recipient != None)
        dest_window = gdk_x11_window_foreign_new_for_display (display, recipient);
      else
        dest_window = NULL;
    }
  else
    {
      dest_window = context->dest_window;
      if (dest_window)
        g_object_ref (dest_window);
      *protocol = context->protocol;
    }

  return dest_window;
}

static void
move_drag_window (GdkDragContext *context,
                  guint           x_root,
                  guint           y_root)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);

  gdk_window_move (context_x11->drag_window,
                   x_root - context_x11->hot_x,
                   y_root - context_x11->hot_y);
  gdk_window_raise (context_x11->drag_window);
}

static gboolean
gdk_x11_drag_context_drag_motion (GdkDragContext *context,
                                  GdkWindow      *dest_window,
                                  GdkDragProtocol protocol,
                                  gint            x_root,
                                  gint            y_root,
                                  GdkDragAction   suggested_action,
                                  GdkDragAction   possible_actions,
                                  guint32         time)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
  GdkWindowImplX11 *impl;

  if (context_x11->drag_window)
    move_drag_window (context, x_root, y_root);

  context_x11->old_actions = context->actions;
  context->actions = possible_actions;

  if (context_x11->old_actions != possible_actions)
    context_x11->xdnd_actions_set = FALSE;

  if (protocol == GDK_DRAG_PROTO_XDND && context_x11->version == 0)
    {
      /* This ugly hack is necessary since GTK+ doesn't know about
       * the XDND protocol version, and in particular doesn't know
       * that gdk_drag_find_window_for_screen() has the side-effect
       * of setting context_x11->version, and therefore sometimes call
       * gdk_drag_motion() without a prior call to
       * gdk_drag_find_window_for_screen(). This happens, e.g.
       * when GTK+ is proxying DND events to embedded windows.
       */
      if (dest_window)
        {
          GdkDisplay *display = GDK_WINDOW_DISPLAY (dest_window);

          xdnd_check_dest (display,
                           GDK_WINDOW_XID (dest_window),
                           &context_x11->version);
        }
    }

  /* When we have a Xdnd target, make sure our XdndActionList
   * matches the current actions;
   */
  if (protocol == GDK_DRAG_PROTO_XDND && !context_x11->xdnd_actions_set)
    {
      if (dest_window)
        {
          if (gdk_window_get_window_type (dest_window) == GDK_WINDOW_FOREIGN)
            xdnd_set_actions (context_x11);
          else if (context->dest_window == dest_window)
            {
              GdkDisplay *display = GDK_WINDOW_DISPLAY (dest_window);
              GdkDragContext *dest_context;

              dest_context = gdk_drag_context_find (display, FALSE,
                                                    GDK_WINDOW_XID (context->source_window),
                                                    GDK_WINDOW_XID (dest_window));

              if (dest_context)
                {
                  dest_context->actions = context->actions;
                  GDK_X11_DRAG_CONTEXT (dest_context)->xdnd_have_actions = TRUE;
                }
            }
        }
    }

  if (context->dest_window != dest_window)
    {
      GdkEvent *temp_event;

      /* Send a leave to the last destination */
      gdk_drag_do_leave (context_x11, time);
      context_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */

      if (dest_window)
        {
          context->dest_window = dest_window;
          context_x11->drop_xid = context_x11->dest_xid;
          g_object_ref (context->dest_window);
          context->protocol = protocol;

          switch (protocol)
            {
            case GDK_DRAG_PROTO_XDND:
              xdnd_send_enter (context_x11);
              break;

            case GDK_DRAG_PROTO_ROOTWIN:
            case GDK_DRAG_PROTO_NONE:
            default:
              break;
            }
          context_x11->old_action = suggested_action;
          context->suggested_action = suggested_action;
          context_x11->old_actions = possible_actions;
        }
      else
        {
          context->dest_window = NULL;
          context_x11->drop_xid = None;
          context->action = 0;
        }

      /* Push a status event, to let the client know that
       * the drag changed
       */
      temp_event = gdk_event_new (GDK_DRAG_STATUS);
      temp_event->dnd.window = g_object_ref (context->source_window);
      /* We use this to signal a synthetic status. Perhaps
       * we should use an extra field...
       */
      temp_event->dnd.send_event = TRUE;

      temp_event->dnd.context = g_object_ref (context);
      temp_event->dnd.time = time;
      gdk_event_set_device (temp_event, gdk_drag_context_get_device (context));

      gdk_event_put (temp_event);
      gdk_event_free (temp_event);
    }
  else
    {
      context_x11->old_action = context->suggested_action;
      context->suggested_action = suggested_action;
    }

  /* Send a drag-motion event */

  context_x11->last_x = x_root;
  context_x11->last_y = y_root;

  if (context->dest_window)
    {
      impl = GDK_WINDOW_IMPL_X11 (context->dest_window->impl);

      if (context_x11->drag_status == GDK_DRAG_STATUS_DRAG)
        {
          switch (context->protocol)
            {
            case GDK_DRAG_PROTO_XDND:
              xdnd_send_motion (context_x11, x_root * impl->window_scale, y_root * impl->window_scale, suggested_action, time);
              break;

            case GDK_DRAG_PROTO_ROOTWIN:
              {
                GdkEvent *temp_event;
                /* GTK+ traditionally has used application/x-rootwin-drop,
                 * but the XDND spec specifies x-rootwindow-drop.
                 */
                GdkAtom target1 = gdk_atom_intern_static_string ("application/x-rootwindow-drop");
                GdkAtom target2 = gdk_atom_intern_static_string ("application/x-rootwin-drop");

                if (g_list_find (context->targets,
                                 GDK_ATOM_TO_POINTER (target1)) ||
                    g_list_find (context->targets,
                                 GDK_ATOM_TO_POINTER (target2)))
                  context->action = context->suggested_action;
                else
                  context->action = 0;

                temp_event = gdk_event_new (GDK_DRAG_STATUS);
                temp_event->dnd.window = g_object_ref (context->source_window);
                temp_event->dnd.send_event = FALSE;
                temp_event->dnd.context = g_object_ref (context);
                temp_event->dnd.time = time;
                gdk_event_set_device (temp_event, gdk_drag_context_get_device (context));

                gdk_event_put (temp_event);
                gdk_event_free (temp_event);
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

static void
gdk_x11_drag_context_drag_abort (GdkDragContext *context,
                                 guint32         time)
{
  gdk_drag_do_leave (GDK_X11_DRAG_CONTEXT (context), time);
}

static void
gdk_x11_drag_context_drag_drop (GdkDragContext *context,
                                guint32         time)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);

  if (context->dest_window)
    {
      switch (context->protocol)
        {
        case GDK_DRAG_PROTO_XDND:
          xdnd_send_drop (context_x11, time);
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

/* Destination side */

static void
gdk_x11_drag_context_drag_status (GdkDragContext *context,
                                  GdkDragAction   action,
                                  guint32         time_)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
  XEvent xev;
  GdkDisplay *display;

  display = GDK_WINDOW_DISPLAY (context->source_window);

  context->action = action;

  if (context->protocol == GDK_DRAG_PROTO_XDND)
    {
      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndStatus");
      xev.xclient.format = 32;
      xev.xclient.window = GDK_WINDOW_XID (context->source_window);

      xev.xclient.data.l[0] = GDK_WINDOW_XID (context->dest_window);
      xev.xclient.data.l[1] = (action != 0) ? (2 | 1) : 0;
      xev.xclient.data.l[2] = 0;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);
      if (!xdnd_send_xevent (context_x11, context->source_window, FALSE, &xev))
        {
          GDK_NOTE (DND,
                    g_message ("Send event to %lx failed",
                               GDK_WINDOW_XID (context->source_window)));
        }
    }

  context_x11->old_action = action;
}

static void
gdk_x11_drag_context_drop_reply (GdkDragContext *context,
                                 gboolean        accepted,
                                 guint32         time_)
{
}

static void
gdk_x11_drag_context_drop_finish (GdkDragContext *context,
                                  gboolean        success,
                                  guint32         time)
{
  if (context->protocol == GDK_DRAG_PROTO_XDND)
    {
      GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndFinished");
      xev.xclient.format = 32;
      xev.xclient.window = GDK_WINDOW_XID (context->source_window);

      xev.xclient.data.l[0] = GDK_WINDOW_XID (context->dest_window);
      if (success)
        {
          xev.xclient.data.l[1] = 1;
          xev.xclient.data.l[2] = xdnd_action_to_atom (display,
                                                       context->action);
        }
      else
        {
          xev.xclient.data.l[1] = 0;
          xev.xclient.data.l[2] = None;
        }
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = 0;

      if (!xdnd_send_xevent (GDK_X11_DRAG_CONTEXT (context), context->source_window, FALSE, &xev))
        {
          GDK_NOTE (DND,
                    g_message ("Send event to %lx failed",
                               GDK_WINDOW_XID (context->source_window)));
        }
    }
}

void
_gdk_x11_window_register_dnd (GdkWindow *window)
{
  static const gulong xdnd_version = 5;
  GdkDisplay *display = gdk_window_get_display (window);

  g_return_if_fail (window != NULL);

  if (gdk_window_get_window_type (window) == GDK_WINDOW_OFFSCREEN)
    return;

  base_precache_atoms (display);

  if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (window), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));

  /* Set XdndAware */

  /* The property needs to be of type XA_ATOM, not XA_INTEGER. Blech */
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (window),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndAware"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)&xdnd_version, 1);
}

static GdkAtom
gdk_x11_drag_context_get_selection (GdkDragContext *context)
{
  if (context->protocol == GDK_DRAG_PROTO_XDND)
    return gdk_atom_intern_static_string ("XdndSelection");
  else
    return GDK_NONE;
}

static gboolean
gdk_x11_drag_context_drop_status (GdkDragContext *context)
{
  return ! GDK_X11_DRAG_CONTEXT (context)->drop_failed;
}

static GdkWindow *
gdk_x11_drag_context_get_drag_window (GdkDragContext *context)
{
  return GDK_X11_DRAG_CONTEXT (context)->drag_window;
}

static void
gdk_x11_drag_context_set_hotspot (GdkDragContext *context,
                                  gint            hot_x,
                                  gint            hot_y)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  x11_context->hot_x = hot_x;
  x11_context->hot_y = hot_y;

  if (x11_context->grab_seat)
    {
      /* DnD is managed, update current position */
      move_drag_window (context, x11_context->last_x, x11_context->last_y);
    }
}

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}


#define ANIM_TIME 500000 /* half a second */

typedef struct _GdkDragAnim GdkDragAnim;
struct _GdkDragAnim {
  GdkX11DragContext *context;
  GdkFrameClock *frame_clock;
  gint64 start_time;
};

static void
gdk_drag_anim_destroy (GdkDragAnim *anim)
{
  g_object_unref (anim->context);
  g_slice_free (GdkDragAnim, anim);
}

static gboolean
gdk_drag_anim_timeout (gpointer data)
{
  GdkDragAnim *anim = data;
  GdkX11DragContext *context = anim->context;
  GdkFrameClock *frame_clock = anim->frame_clock;
  gint64 current_time;
  double f;
  double t;

  if (!frame_clock)
    return G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);

  f = (current_time - anim->start_time) / (double) ANIM_TIME;

  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  gdk_window_show (context->drag_window);
  gdk_window_move (context->drag_window,
                   (context->last_x - context->hot_x) +
                   (context->start_x - context->last_x) * t,
                   (context->last_y - context->hot_y) +
                   (context->start_y - context->last_y) * t);
  gdk_window_set_opacity (context->drag_window, 1.0 - f);

  return G_SOURCE_CONTINUE;
}

static void
gdk_x11_drag_context_drop_done (GdkDragContext *context,
                                gboolean        success)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDragAnim *anim;
  cairo_surface_t *win_surface;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_t *cr;

  if (success)
    {
      gdk_window_hide (x11_context->drag_window);
      return;
    }

  win_surface = _gdk_window_ref_cairo_surface (x11_context->drag_window);
  surface = gdk_window_create_similar_surface (x11_context->drag_window,
                                               cairo_surface_get_content (win_surface),
                                               gdk_window_get_width (x11_context->drag_window),
                                               gdk_window_get_height (x11_context->drag_window));
  cr = cairo_create (surface);
  cairo_set_source_surface (cr, win_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy (win_surface);

  pattern = cairo_pattern_create_for_surface (surface);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_set_background_pattern (x11_context->drag_window, pattern);
G_GNUC_END_IGNORE_DEPRECATIONS

  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);

  anim = g_slice_new0 (GdkDragAnim);
  anim->context = g_object_ref (x11_context);
  anim->frame_clock = gdk_window_get_frame_clock (x11_context->drag_window);
  anim->start_time = gdk_frame_clock_get_frame_time (anim->frame_clock);

  gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT, 17,
                                gdk_drag_anim_timeout, anim,
                                (GDestroyNotify) gdk_drag_anim_destroy);
}

static gboolean
drag_context_grab (GdkDragContext *context)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDevice *device = gdk_drag_context_get_device (context);
  GdkSeatCapabilities capabilities;
  GdkWindow *root;
  GdkSeat *seat;
  gint keycode, i;
  GdkCursor *cursor;

  if (!x11_context->ipc_window)
    return FALSE;

  root = gdk_screen_get_root_window (gdk_window_get_screen (x11_context->ipc_window));
  seat = gdk_device_get_seat (gdk_drag_context_get_device (context));

#ifdef XINPUT_2
  if (GDK_IS_X11_DEVICE_XI2 (device))
    capabilities = GDK_SEAT_CAPABILITY_ALL_POINTING;
  else
#endif
    capabilities = GDK_SEAT_CAPABILITY_ALL;

  cursor = gdk_drag_get_cursor (context, x11_context->current_action);
  g_set_object (&x11_context->cursor, cursor);

  if (gdk_seat_grab (seat, x11_context->ipc_window,
                     capabilities, FALSE,
                     x11_context->cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    return FALSE;

  g_set_object (&x11_context->grab_seat, seat);

  gdk_x11_display_error_trap_push (gdk_window_get_display (x11_context->ipc_window));

  for (i = 0; i < G_N_ELEMENTS (grab_keys); ++i)
    {
      keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (x11_context->ipc_window),
                                  grab_keys[i].keysym);
      if (keycode == NoSymbol)
        continue;

#ifdef XINPUT_2
      if (GDK_IS_X11_DEVICE_XI2 (device))
        {
          gint deviceid = gdk_x11_device_get_id (gdk_seat_get_keyboard (seat));
          unsigned char mask[XIMaskLen(XI_LASTEVENT)];
          XIGrabModifiers mods;
          XIEventMask evmask;
          gint num_mods;

          memset (mask, 0, sizeof (mask));
          XISetMask (mask, XI_KeyPress);
          XISetMask (mask, XI_KeyRelease);

          evmask.deviceid = deviceid;
          evmask.mask_len = sizeof (mask);
          evmask.mask = mask;

          num_mods = 1;
          mods.modifiers = grab_keys[i].modifiers;

          XIGrabKeycode (GDK_WINDOW_XDISPLAY (x11_context->ipc_window),
                         deviceid,
                         keycode,
                         GDK_WINDOW_XID (root),
                         GrabModeAsync,
                         GrabModeAsync,
                         False,
                         &evmask,
                         num_mods,
                         &mods);
        }
      else
#endif
        {
          XGrabKey (GDK_WINDOW_XDISPLAY (x11_context->ipc_window),
                    keycode, grab_keys[i].modifiers,
                    GDK_WINDOW_XID (root),
                    FALSE,
                    GrabModeAsync,
                    GrabModeAsync);
        }
    }

  gdk_x11_display_error_trap_pop_ignored (gdk_window_get_display (x11_context->ipc_window));

  return TRUE;
}

static void
drag_context_ungrab (GdkDragContext *context)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDevice *keyboard;
  GdkWindow *root;
  gint keycode, i;

  if (!x11_context->grab_seat)
    return;

  gdk_seat_ungrab (x11_context->grab_seat);

  keyboard = gdk_seat_get_keyboard (x11_context->grab_seat);
  root = gdk_screen_get_root_window (gdk_window_get_screen (x11_context->ipc_window));
  g_clear_object (&x11_context->grab_seat);

  for (i = 0; i < G_N_ELEMENTS (grab_keys); ++i)
    {
      keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (x11_context->ipc_window),
                                  grab_keys[i].keysym);
      if (keycode == NoSymbol)
        continue;

#ifdef XINPUT_2
      if (GDK_IS_X11_DEVICE_XI2 (keyboard))
        {
          XIGrabModifiers mods;
          gint num_mods;

          num_mods = 1;
          mods.modifiers = grab_keys[i].modifiers;

          XIUngrabKeycode (GDK_WINDOW_XDISPLAY (x11_context->ipc_window),
                           gdk_x11_device_get_id (keyboard),
                           keycode,
                           GDK_WINDOW_XID (root),
                           num_mods,
                           &mods);
        }
      else
#endif /* XINPUT_2 */
        {
          XUngrabKey (GDK_WINDOW_XDISPLAY (x11_context->ipc_window),
                      keycode, grab_keys[i].modifiers,
                      GDK_WINDOW_XID (root));
        }
    }
}

static gboolean
gdk_x11_drag_context_manage_dnd (GdkDragContext *context,
                                 GdkWindow      *ipc_window,
                                 GdkDragAction   actions)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  if (x11_context->ipc_window)
    return FALSE;

  context->protocol = GDK_DRAG_PROTO_XDND;
  x11_context->ipc_window = g_object_ref (ipc_window);

  if (drag_context_grab (context))
    {
      x11_context->actions = actions;
      move_drag_window (context, x11_context->start_x, x11_context->start_y);
      return TRUE;
    }
  else
    {
      g_clear_object (&x11_context->ipc_window);
      return FALSE;
    }
}

static void
gdk_x11_drag_context_set_cursor (GdkDragContext *context,
                                 GdkCursor      *cursor)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  if (!g_set_object (&x11_context->cursor, cursor))
    return;

  if (x11_context->grab_seat)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gdk_device_grab (gdk_seat_get_pointer (x11_context->grab_seat),
                       x11_context->ipc_window,
                       GDK_OWNERSHIP_APPLICATION, FALSE,
                       GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                       cursor, GDK_CURRENT_TIME);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
gdk_x11_drag_context_cancel (GdkDragContext      *context,
                             GdkDragCancelReason  reason)
{
  drag_context_ungrab (context);
  gdk_drag_drop_done (context, FALSE);
}

static void
gdk_x11_drag_context_drop_performed (GdkDragContext *context,
                                     guint32         time_)
{
  drag_context_ungrab (context);
}

#define BIG_STEP 20
#define SMALL_STEP 1

static void
gdk_drag_get_current_actions (GdkModifierType  state,
                              gint             button,
                              GdkDragAction    actions,
                              GdkDragAction   *suggested_action,
                              GdkDragAction   *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
    {
      *suggested_action = GDK_ACTION_ASK;
      *possible_actions = actions;
    }
  else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    {
      if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
        {
          if (actions & GDK_ACTION_LINK)
            {
              *suggested_action = GDK_ACTION_LINK;
              *possible_actions = GDK_ACTION_LINK;
            }
        }
      else if (state & GDK_CONTROL_MASK)
        {
          if (actions & GDK_ACTION_COPY)
            {
              *suggested_action = GDK_ACTION_COPY;
              *possible_actions = GDK_ACTION_COPY;
            }
        }
      else
        {
          if (actions & GDK_ACTION_MOVE)
            {
              *suggested_action = GDK_ACTION_MOVE;
              *possible_actions = GDK_ACTION_MOVE;
            }
        }
    }
  else
    {
      *possible_actions = actions;

      if ((state & (GDK_MOD1_MASK)) && (actions & GDK_ACTION_ASK))
        *suggested_action = GDK_ACTION_ASK;
      else if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

static void
gdk_drag_update (GdkDragContext  *context,
                 gdouble          x_root,
                 gdouble          y_root,
                 GdkModifierType  mods,
                 guint32          evtime)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDragAction action, possible_actions;
  GdkWindow *dest_window;
  GdkDragProtocol protocol;

  gdk_drag_get_current_actions (mods, GDK_BUTTON_PRIMARY, x11_context->actions,
                                &action, &possible_actions);

  gdk_drag_find_window_for_screen (context,
                                   x11_context->drag_window,
                                   gdk_display_get_default_screen (gdk_display_get_default ()),
                                   x_root, y_root, &dest_window, &protocol);

  gdk_drag_motion (context, dest_window, protocol, x_root, y_root,
                   action, possible_actions, evtime);
}

static gboolean
gdk_dnd_handle_motion_event (GdkDragContext       *context,
                             const GdkEventMotion *event)
{
  GdkModifierType state;

  if (!gdk_event_get_state ((GdkEvent *) event, &state))
    return FALSE;

  gdk_drag_update (context, event->x_root, event->y_root, state,
                   gdk_event_get_time ((GdkEvent *) event));
  return TRUE;
}

static gboolean
gdk_dnd_handle_key_event (GdkDragContext    *context,
                          const GdkEventKey *event)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkModifierType state;
  GdkWindow *root_window;
  GdkDevice *pointer;
  gint dx, dy;

  dx = dy = 0;
  state = event->state;
  pointer = gdk_device_get_associated_device (gdk_event_get_device ((GdkEvent *) event));

  if (event->type == GDK_KEY_PRESS)
    {
      switch (event->keyval)
        {
        case GDK_KEY_Escape:
          gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_USER_CANCELLED);
          return TRUE;

        case GDK_KEY_space:
        case GDK_KEY_Return:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_KP_Space:
          if ((gdk_drag_context_get_selected_action (context) != 0) &&
              (gdk_drag_context_get_dest_window (context) != NULL))
            {
              g_signal_emit_by_name (context, "drop-performed",
                                     gdk_event_get_time ((GdkEvent *) event));
            }
          else
            gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_NO_TARGET);

          return TRUE;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          dy = (state & GDK_MOD1_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          dy = (state & GDK_MOD1_MASK) ? BIG_STEP : SMALL_STEP;
          break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
          dx = (state & GDK_MOD1_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
          dx = (state & GDK_MOD1_MASK) ? BIG_STEP : SMALL_STEP;
          break;
        }
    }

  /* The state is not yet updated in the event, so we need
   * to query it here. We could use XGetModifierMapping, but
   * that would be overkill.
   */
  root_window = gdk_screen_get_root_window (gdk_window_get_screen (x11_context->ipc_window));
  gdk_window_get_device_position (root_window, pointer, NULL, NULL, &state);

  if (dx != 0 || dy != 0)
    {
      x11_context->last_x += dx;
      x11_context->last_y += dy;
      gdk_device_warp (pointer,
                       gdk_window_get_screen (x11_context->ipc_window),
                       x11_context->last_x, x11_context->last_y);
    }

  gdk_drag_update (context, x11_context->last_x, x11_context->last_y, state,
                   gdk_event_get_time ((GdkEvent *) event));

  return TRUE;
}

static gboolean
gdk_dnd_handle_grab_broken_event (GdkDragContext           *context,
                                  const GdkEventGrabBroken *event)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  /* Don't cancel if we break the implicit grab from the initial button_press.
   * Also, don't cancel if we re-grab on the widget or on our IPC window, for
   * example, when changing the drag cursor.
   */
  if (event->implicit ||
      event->grab_window == x11_context->drag_window ||
      event->grab_window == x11_context->ipc_window)
    return FALSE;

  if (gdk_event_get_device ((GdkEvent *) event) !=
      gdk_drag_context_get_device (context))
    return FALSE;

  gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_ERROR);
  return TRUE;
}

static gboolean
gdk_dnd_handle_button_event (GdkDragContext       *context,
                             const GdkEventButton *event)
{
#if 0
  /* FIXME: Check the button matches */
  if (event->button != x11_context->button)
    return FALSE;
#endif

  if ((gdk_drag_context_get_selected_action (context) != 0) &&
      (gdk_drag_context_get_dest_window (context) != NULL))
    {
      g_signal_emit_by_name (context, "drop-performed",
                             gdk_event_get_time ((GdkEvent *) event));
    }
  else
    gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_NO_TARGET);

  return TRUE;
}

gboolean
gdk_dnd_handle_drag_status (GdkDragContext    *context,
                            const GdkEventDND *event)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
  GdkDragAction action;

  if (context != event->context)
    return FALSE;

  action = gdk_drag_context_get_selected_action (context);

  if (action != context_x11->current_action)
    {
      context_x11->current_action = action;
      g_signal_emit_by_name (context, "action-changed", action);
    }

  return TRUE;
}

static gboolean
gdk_dnd_handle_drop_finished (GdkDragContext *context,
                              const GdkEventDND *event)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  if (context != event->context)
    return FALSE;

  g_signal_emit_by_name (context, "dnd-finished");
  gdk_drag_drop_done (context, !x11_context->drop_failed);
  return TRUE;
}

gboolean
gdk_x11_drag_context_handle_event (GdkDragContext *context,
                                   const GdkEvent *event)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  if (!context->is_source)
    return FALSE;
  if (!x11_context->grab_seat && event->type != GDK_DROP_FINISHED)
    return FALSE;

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      return gdk_dnd_handle_motion_event (context, &event->motion);
    case GDK_BUTTON_RELEASE:
      return gdk_dnd_handle_button_event (context, &event->button);
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return gdk_dnd_handle_key_event (context, &event->key);
    case GDK_GRAB_BROKEN:
      return gdk_dnd_handle_grab_broken_event (context, &event->grab_broken);
    case GDK_DRAG_STATUS:
      return gdk_dnd_handle_drag_status (context, &event->dnd);
    case GDK_DROP_FINISHED:
      return gdk_dnd_handle_drop_finished (context, &event->dnd);
    default:
      break;
    }

  return FALSE;
}

void
gdk_x11_drag_context_action_changed (GdkDragContext *context,
                                     GdkDragAction   action)
{
  GdkCursor *cursor;

  cursor = gdk_drag_get_cursor (context, action);
  gdk_drag_context_set_cursor (context, cursor);
}
