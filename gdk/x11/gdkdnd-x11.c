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

#include "gdk-private.h"
#include "gdkasync.h"
#include "gdkclipboardprivate.h"
#include "gdkclipboard-x11.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkdndprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkproperty.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkselectioninputstream-x11.h"
#include "gdkselectionoutputstream-x11.h"

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

/*
 * GdkDragProtocol:
 * @GDK_DRAG_PROTO_NONE: no protocol.
 * @GDK_DRAG_PROTO_XDND: The Xdnd protocol.
 * @GDK_DRAG_PROTO_ROOTWIN: An extension to the Xdnd protocol for
 *  unclaimed root window drops.
 *
 * Used in #GdkDragContext to indicate the protocol according to
 * which DND is done.
 */
typedef enum
{
  GDK_DRAG_PROTO_NONE = 0,
  GDK_DRAG_PROTO_XDND,
  GDK_DRAG_PROTO_ROOTWIN
} GdkDragProtocol;

typedef struct {
  guint32 xid;
  gint x, y, width, height;
  gboolean mapped;
  gboolean shape_selected;
  gboolean shape_valid;
  cairo_region_t *shape;
} GdkCacheChild;

struct _GdkSurfaceCache {
  GList *children;
  GHashTable *child_hash;
  guint old_event_mask;
  GdkDisplay *display;
  gint ref_count;
};


struct _GdkX11DragContext
{
  GdkDragContext context;

  GdkDragProtocol protocol;

  gint start_x;                /* Where the drag started */
  gint start_y;
  guint16 last_x;              /* Coordinates from last event */
  guint16 last_y;
  gulong timestamp;            /* Timestamp we claimed the DND selection with */
  GdkDragAction old_action;    /* The last action we sent to the source */
  GdkDragAction old_actions;   /* The last actions we sent to the source */
  GdkDragAction xdnd_actions;  /* What is currently set in XdndActionList */
  guint version;               /* Xdnd protocol version */

  GdkSurfaceCache *cache;

  GdkSurface *drag_surface;

  GdkSurface *ipc_surface;
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

static GdkSurfaceCache *gdk_surface_cache_ref   (GdkSurfaceCache *cache);
static void            gdk_surface_cache_unref (GdkSurfaceCache *cache);

static GdkFilterReturn xdnd_enter_filter    (const XEvent *xevent,
                                             GdkEvent     *event,
                                             gpointer      data);
static GdkFilterReturn xdnd_leave_filter    (const XEvent *xevent,
                                             GdkEvent     *event,
                                             gpointer      data);
static GdkFilterReturn xdnd_position_filter (const XEvent *xevent,
                                             GdkEvent     *event,
                                             gpointer      data);
static GdkFilterReturn xdnd_status_filter   (const XEvent *xevent,
                                             GdkEvent     *event,
                                             gpointer      data);
static GdkFilterReturn xdnd_finished_filter (const XEvent *xevent,
                                             GdkEvent     *event,
                                             gpointer      data);
static GdkFilterReturn xdnd_drop_filter     (const XEvent *xevent,
                                             GdkEvent     *event,
                                             gpointer      data);

static void   xdnd_manage_source_filter (GdkDragContext *context,
                                         GdkSurface      *surface,
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
static GdkSurface * gdk_x11_drag_context_find_surface (GdkDragContext  *context,
                                                     GdkSurface       *drag_surface,
                                                     gint             x_root,
                                                     gint             y_root,
                                                     GdkDragProtocol *protocol);
static gboolean    gdk_x11_drag_context_drag_motion (GdkDragContext  *context,
                                                     GdkSurface       *dest_surface,
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
static GdkSurface * gdk_x11_drag_context_get_drag_surface (GdkDragContext *context);
static void        gdk_x11_drag_context_set_hotspot (GdkDragContext  *context,
                                                     gint             hot_x,
                                                     gint             hot_y);
static void        gdk_x11_drag_context_drop_done     (GdkDragContext  *context,
                                                       gboolean         success);
static void        gdk_x11_drag_context_set_cursor     (GdkDragContext *context,
                                                        GdkCursor      *cursor);
static void        gdk_x11_drag_context_cancel         (GdkDragContext      *context,
                                                        GdkDragCancelReason  reason);
static void        gdk_x11_drag_context_drop_performed (GdkDragContext *context,
                                                        guint32         time);

static void
gdk_x11_drag_context_read_got_stream (GObject      *source,
                                      GAsyncResult *res,
                                      gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  GInputStream *stream;
  const char *type;
  int format;
  
  stream = gdk_x11_selection_input_stream_new_finish (res, &type, &format, &error);
  if (stream == NULL)
    {
      GSList *targets, *next;
      
      targets = g_task_get_task_data (task);
      next = targets->next;
      if (next)
        {
          GdkDragContext *context = GDK_DRAG_CONTEXT (g_task_get_source_object (task));

          GDK_DISPLAY_NOTE (gdk_drag_context_get_display (context), DND, g_printerr ("reading %s failed, trying %s next\n",
                                     (char *) targets->data, (char *) next->data));
          targets->next = NULL;
          g_task_set_task_data (task, next, (GDestroyNotify) g_slist_free);
          gdk_x11_selection_input_stream_new_async (gdk_drag_context_get_display (context),
                                                    "XdndSelection",
                                                    next->data,
                                                    CurrentTime,
                                                    g_task_get_priority (task),
                                                    g_task_get_cancellable (task),
                                                    gdk_x11_drag_context_read_got_stream,
                                                    task);
          g_error_free (error);
          return;
        }

      g_task_return_error (task, error);
    }
  else
    {
      const char *mime_type = ((GSList *) g_task_get_task_data (task))->data;
#if 0
      gsize i;

      for (i = 0; i < G_N_ELEMENTS (special_targets); i++)
        {
          if (g_str_equal (mime_type, special_targets[i].x_target))
            {
              g_assert (special_targets[i].mime_type != NULL);

              GDK_DISPLAY_NOTE (CLIPBOARD, g_printerr ("%s: reading with converter from %s to %s\n",
                                              cb->selection, mime_type, special_targets[i].mime_type));
              mime_type = g_intern_string (special_targets[i].mime_type);
              g_task_set_task_data (task, g_slist_prepend (NULL, (gpointer) mime_type), (GDestroyNotify) g_slist_free);
              stream = special_targets[i].convert (cb, stream, type, format);
              break;
            }
        }
#endif

      GDK_NOTE (DND, g_printerr ("reading DND as %s now\n",
                                mime_type));
      g_task_return_pointer (task, stream, g_object_unref);
    }

  g_object_unref (task);
}

static void
gdk_x11_drag_context_read_async (GdkDragContext      *context,
                                 GdkContentFormats   *formats,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GSList *targets;
  GTask *task;

  task = g_task_new (context, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_x11_drag_context_read_async);

  targets = gdk_x11_clipboard_formats_to_targets (formats);
  g_task_set_task_data (task, targets, (GDestroyNotify) g_slist_free);
  if (targets == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      return;
    }

  GDK_DISPLAY_NOTE (gdk_drag_context_get_display (context), DND, g_printerr ("new read for %s (%u other options)\n",
                            (char *) targets->data, g_slist_length (targets->next)));
  gdk_x11_selection_input_stream_new_async (gdk_drag_context_get_display (context),
                                            "XdndSelection",
                                            targets->data,
                                            CurrentTime,
                                            io_priority,
                                            cancellable,
                                            gdk_x11_drag_context_read_got_stream,
                                            task);
}

static GInputStream *
gdk_x11_drag_context_read_finish (GdkDragContext  *context,
                                  const char     **out_mime_type,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (context)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_x11_drag_context_read_async, NULL);

  if (out_mime_type)
    {
      GSList *targets;

      targets = g_task_get_task_data (task);
      *out_mime_type = targets ? targets->data : NULL;
    }

  return g_task_propagate_pointer (task, error);
}

static void
gdk_x11_drag_context_class_init (GdkX11DragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_x11_drag_context_finalize;

  context_class->drag_status = gdk_x11_drag_context_drag_status;
  context_class->drag_abort = gdk_x11_drag_context_drag_abort;
  context_class->drag_drop = gdk_x11_drag_context_drag_drop;
  context_class->drop_reply = gdk_x11_drag_context_drop_reply;
  context_class->drop_finish = gdk_x11_drag_context_drop_finish;
  context_class->drop_status = gdk_x11_drag_context_drop_status;
  context_class->read_async = gdk_x11_drag_context_read_async;
  context_class->read_finish = gdk_x11_drag_context_read_finish;
  context_class->get_drag_surface = gdk_x11_drag_context_get_drag_surface;
  context_class->set_hotspot = gdk_x11_drag_context_set_hotspot;
  context_class->drop_done = gdk_x11_drag_context_drop_done;
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
  GdkSurface *drag_surface, *ipc_surface;

  if (context->source_surface)
    {
      if ((x11_context->protocol == GDK_DRAG_PROTO_XDND) && !context->is_source)
        xdnd_manage_source_filter (context, context->source_surface, FALSE);
    }

  if (x11_context->cache)
    gdk_surface_cache_unref (x11_context->cache);

  contexts = g_list_remove (contexts, context);

  drag_surface = context->drag_surface;
  ipc_surface = x11_context->ipc_surface;

  G_OBJECT_CLASS (gdk_x11_drag_context_parent_class)->finalize (object);

  if (drag_surface)
    gdk_surface_destroy (drag_surface);
  if (ipc_surface)
    gdk_surface_destroy (ipc_surface);
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

      if ((context->source_surface && gdk_surface_get_display (context->source_surface) != display) ||
          (context->dest_surface && gdk_surface_get_display (context->dest_surface) != display))
        continue;

      context_dest_xid = context->dest_surface
                            ? (context_x11->drop_xid
                                  ? context_x11->drop_xid
                                  : GDK_SURFACE_XID (context->dest_surface))
                            : None;

      if ((!context->is_source == !is_source) &&
          ((source_xid == None) || (context->source_surface &&
            (GDK_SURFACE_XID (context->source_surface) == source_xid))) &&
          ((dest_xid == None) || (context_dest_xid == dest_xid)))
        return context;
    }

  return NULL;
}

static void
precache_target_list (GdkDragContext *context)
{
  if (context->formats)
    {
      const char * const *atoms;
      gsize n_atoms;

      atoms = gdk_content_formats_get_mime_types (context->formats, &n_atoms);

      _gdk_x11_precache_atoms (gdk_drag_context_get_display (context),
                               (const gchar **) atoms,
                               n_atoms);
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
gdk_surface_cache_add (GdkSurfaceCache *cache,
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

GdkFilterReturn
gdk_surface_cache_shape_filter (const XEvent *xevent,
                               GdkEvent     *event,
                               gpointer      data)
{
  GdkSurfaceCache *cache = data;

  GdkX11Display *display = GDK_X11_DISPLAY (cache->display);

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

GdkFilterReturn
gdk_surface_cache_filter (const XEvent *xevent,
                         GdkEvent     *event,
                         gpointer      data)
{
  GdkSurfaceCache *cache = data;

  switch (xevent->type)
    {
    case CirculateNotify:
      break;
    case ConfigureNotify:
      {
        const XConfigureEvent *xce = &xevent->xconfigure;
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
        const XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

        if (!g_hash_table_lookup (cache->child_hash,
                                  GUINT_TO_POINTER (xcwe->window)))
          gdk_surface_cache_add (cache, xcwe->window,
                                xcwe->x, xcwe->y, xcwe->width, xcwe->height,
                                FALSE);
        break;
      }
    case DestroyNotify:
      {
        const XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;
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
        const XMapEvent *xme = &xevent->xmap;
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
        const XMapEvent *xume = &xevent->xmap;
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

static GdkSurfaceCache *
gdk_surface_cache_new (GdkDisplay *display)
{
  XWindowAttributes xwa;
  GdkX11Screen *screen = GDK_X11_DISPLAY (display)->screen;
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  Window xroot_window = GDK_DISPLAY_XROOTWIN (display);
  GdkChildInfoX11 *children;
  guint nchildren, i;
#ifdef HAVE_XCOMPOSITE
  Window cow;
#endif

  GdkSurfaceCache *result = g_new (GdkSurfaceCache, 1);

  result->children = NULL;
  result->child_hash = g_hash_table_new (g_direct_hash, NULL);
  result->display = display;
  result->ref_count = 1;

  XGetWindowAttributes (xdisplay, xroot_window, &xwa);
  result->old_event_mask = xwa.your_event_mask;

  if (G_UNLIKELY (!GDK_X11_DISPLAY (display)->trusted_client))
    {
      GList *toplevel_windows, *list;
      GdkSurface *surface;
      GdkSurfaceImplX11 *impl;
      gint x, y, width, height;

      toplevel_windows = gdk_x11_display_get_toplevel_windows (display);
      for (list = toplevel_windows; list; list = list->next)
        {
          surface = GDK_SURFACE (list->data);
	  impl = GDK_SURFACE_IMPL_X11 (surface->impl);
          gdk_surface_get_geometry (surface, &x, &y, &width, &height);
          gdk_surface_cache_add (result, GDK_SURFACE_XID (surface),
                                x * impl->surface_scale, y * impl->surface_scale, 
				width * impl->surface_scale, 
				height * impl->surface_scale,
                                gdk_surface_is_visible (surface));
        }
      return result;
    }

  XSelectInput (xdisplay, xroot_window, result->old_event_mask | SubstructureNotifyMask);

  if (!_gdk_x11_get_window_child_info (display,
                                       xroot_window,
                                       FALSE, NULL,
                                       &children, &nchildren))
    return result;

  for (i = 0; i < nchildren ; i++)
    {
      gdk_surface_cache_add (result, children[i].window,
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
  if (gdk_display_is_composited (display))
    {
      cow = XCompositeGetOverlayWindow (xdisplay, xroot_window);
      gdk_surface_cache_add (result, cow, 0, 0,
			    WidthOfScreen (GDK_X11_SCREEN (screen)->xscreen) * GDK_X11_SCREEN (screen)->surface_scale,
			    HeightOfScreen (GDK_X11_SCREEN (screen)->xscreen) * GDK_X11_SCREEN (screen)->surface_scale,
			    TRUE);
      XCompositeReleaseOverlayWindow (xdisplay, xroot_window);
    }
#endif

  return result;
}

static void
gdk_surface_cache_destroy (GdkSurfaceCache *cache)
{
  XSelectInput (GDK_DISPLAY_XDISPLAY (cache->display),
                GDK_DISPLAY_XROOTWIN (cache->display),
                cache->old_event_mask);

  gdk_x11_display_error_trap_push (cache->display);
  g_list_foreach (cache->children, (GFunc)free_cache_child, cache->display);
  gdk_x11_display_error_trap_pop_ignored (cache->display);

  g_list_free (cache->children);
  g_hash_table_destroy (cache->child_hash);

  g_free (cache);
}

static GdkSurfaceCache *
gdk_surface_cache_ref (GdkSurfaceCache *cache)
{
  cache->ref_count += 1;

  return cache;
}

static void
gdk_surface_cache_unref (GdkSurfaceCache *cache)
{
  g_assert (cache->ref_count > 0);

  cache->ref_count -= 1;

  if (cache->ref_count == 0)
    {
      window_caches = g_slist_remove (window_caches, cache);
      gdk_surface_cache_destroy (cache);
    }
}

GdkSurfaceCache *
gdk_surface_cache_get (GdkDisplay *display)
{
  GSList *list;
  GdkSurfaceCache *cache;

  for (list = window_caches; list; list = list->next)
    {
      cache = list->data;
      if (cache->display == display)
        return gdk_surface_cache_ref (cache);
    }

  cache = gdk_surface_cache_new (display);

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
get_client_window_at_coords (GdkSurfaceCache *cache,
                             Window          ignore,
                             gint            x_root,
                             gint            y_root)
{
  GList *tmp_list;
  Window retval = None;
  GdkDisplay *display;

  display = cache->display;

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
    return GDK_DISPLAY_XROOTWIN (display);
}

#ifdef G_ENABLE_DEBUG
static void
print_target_list (GdkContentFormats *formats)
{
  gchar *name = gdk_content_formats_to_string (formats);
  g_message ("DND formats: %s", name);
  g_free (name);
}
#endif /* G_ENABLE_DEBUG */

/*************************************************************
 ***************************** XDND **************************
 *************************************************************/

/* Utility functions */

static struct {
  const gchar *name;
  GdkDragAction action;
} xdnd_actions_table[] = {
    { "XdndActionCopy",    GDK_ACTION_COPY },
    { "XdndActionMove",    GDK_ACTION_MOVE },
    { "XdndActionLink",    GDK_ACTION_LINK },
    { "XdndActionAsk",     GDK_ACTION_ASK  },
    { "XdndActionPrivate", GDK_ACTION_COPY },
  };

static const gint xdnd_n_actions = G_N_ELEMENTS (xdnd_actions_table);

static GdkDragAction
xdnd_action_from_atom (GdkDisplay *display,
                       Atom        xatom)
{
  const char *name;
  gint i;

  if (xatom == None)
    return 0;

  name = gdk_x11_get_xatom_name_for_display (display, xatom);

  for (i = 0; i < xdnd_n_actions; i++)
    if (g_str_equal (name, xdnd_actions_table[i].name))
      return xdnd_actions_table[i].action;

  return 0;
}

static Atom
xdnd_action_to_atom (GdkDisplay    *display,
                     GdkDragAction  action)
{
  gint i;

  for (i = 0; i < xdnd_n_actions; i++)
    if (action == xdnd_actions_table[i].action)
      return gdk_x11_get_xatom_by_name_for_display (display, xdnd_actions_table[i].name);

  return None;
}

/* Source side */

static GdkFilterReturn
xdnd_status_filter (const XEvent *xevent,
                    GdkEvent     *event,
                    gpointer      data)
{
  GdkDisplay *display;
  guint32 dest_surface = xevent->xclient.data.l[0];
  guint32 flags = xevent->xclient.data.l[1];
  Atom action = xevent->xclient.data.l[4];
  GdkDragContext *context;

  if (!event->any.surface ||
      gdk_surface_get_surface_type (event->any.surface) == GDK_SURFACE_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  display = gdk_surface_get_display (event->any.surface);
  context = gdk_drag_context_find (display, TRUE, xevent->xclient.window, dest_surface);

  GDK_DISPLAY_NOTE (display, DND,
            g_message ("XdndStatus: dest_surface: %#x  action: %ld",
                       dest_surface, action));

  if (context)
    {
      GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
      if (context_x11->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
        context_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      if (!(action != 0) != !(flags & 1))
        {
          GDK_DISPLAY_NOTE (display, DND,
                    g_warning ("Received status event with flags not corresponding to action!"));
          action = 0;
        }

      context->action = xdnd_action_from_atom (display, action);

      if (context->action != context_x11->current_action)
        {
          context_x11->current_action = action;
          g_signal_emit_by_name (context, "action-changed", action);
        }
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
xdnd_finished_filter (const XEvent *xevent,
                      GdkEvent     *event,
                      gpointer      data)
{
  GdkDisplay *display;
  guint32 dest_surface = xevent->xclient.data.l[0];
  GdkDragContext *context;
  GdkX11DragContext *context_x11;

  if (!event->any.surface ||
      gdk_surface_get_surface_type (event->any.surface) == GDK_SURFACE_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  display = gdk_surface_get_display (event->any.surface);
  context = gdk_drag_context_find (display, TRUE, xevent->xclient.window, dest_surface);

  GDK_DISPLAY_NOTE (display, DND,
            g_message ("XdndFinished: dest_surface: %#x", dest_surface));

  if (context)
    {
      g_object_ref (context);

      context_x11 = GDK_X11_DRAG_CONTEXT (context);
      if (context_x11->version == 5)
        context_x11->drop_failed = xevent->xclient.data.l[1] == 0;

      g_signal_emit_by_name (context, "dnd-finished");
      gdk_drag_drop_done (context, !context_x11->drop_failed);

      g_object_unref (context);
    }

  return GDK_FILTER_REMOVE;
}

static void
xdnd_set_targets (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  Atom *atomlist;
  const char * const *atoms;
  gsize i, n_atoms;
  GdkDisplay *display = gdk_drag_context_get_display (context);

  atoms = gdk_content_formats_get_mime_types (context->formats, &n_atoms);
  atomlist = g_new (Atom, n_atoms);
  for (i = 0; i < n_atoms; i++)
    atomlist[i] = gdk_x11_get_xatom_by_name_for_display (display, atoms[i]);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (context_x11->ipc_surface),
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
  GdkDisplay *display = gdk_drag_context_get_display (context);

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
          atomlist[n_atoms] = gdk_x11_get_xatom_by_name_for_display (display, xdnd_actions_table[i].name);
          n_atoms++;
        }
    }

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (context_x11->ipc_surface),
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
  GDK_DISPLAY_NOTE (gdk_drag_context_get_display (context), DND,
            g_message ("Got async callback for #%lx, success = %d",
                       window, success));

  /* On failure, we immediately continue with the protocol
   * so we don't end up blocking for a timeout
   */
  if (!success &&
      context->dest_surface &&
      window == GDK_SURFACE_XID (context->dest_surface))
    {
      GdkX11DragContext *context_x11 = data;

      g_object_unref (context->dest_surface);
      context->dest_surface = NULL;
      context->action = 0;
      if (context->action != context_x11->current_action)
        {
          context_x11->current_action = 0;
          g_signal_emit_by_name (context, "action-changed", 0);
        }
      context_x11->drag_status = GDK_DRAG_STATUS_DRAG;
    }

  g_object_unref (context);
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
                  GdkSurface         *surface,
                  gboolean           propagate,
                  XEvent            *event_send)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = gdk_drag_context_get_display (context);
  Window xwindow;
  glong event_mask;

  g_assert (event_send->xany.type == ClientMessage);

  /* We short-circuit messages to ourselves */
  if (gdk_surface_get_surface_type (surface) != GDK_SURFACE_FOREIGN)
    {
      gint i;

      for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
        {
          if (gdk_x11_get_xatom_by_name_for_display (display, xdnd_filters[i].atom_name) ==
              event_send->xclient.message_type)
            {
              GdkEvent *temp_event;

              temp_event = gdk_event_new (GDK_NOTHING);
              temp_event->any.surface = g_object_ref (surface);

              if ((*xdnd_filters[i].func) (event_send, temp_event, NULL) == GDK_FILTER_TRANSLATE)
                gdk_display_put_event (display, temp_event);

              g_object_unref (temp_event);

              return TRUE;
            }
        }
    }

  xwindow = GDK_SURFACE_XID (surface);

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
  GdkDisplay *display = gdk_drag_context_get_display (context);
  const char * const *atoms;
  gsize i, n_atoms;
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndEnter");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_SURFACE_XID (context->dest_surface);
  xev.xclient.data.l[0] = GDK_SURFACE_XID (context_x11->ipc_surface);
  xev.xclient.data.l[1] = (context_x11->version << 24); /* version */
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  GDK_DISPLAY_NOTE (display, DND,
           g_message ("Sending enter source window %#lx XDND protocol version %d\n",
                      GDK_SURFACE_XID (context_x11->ipc_surface), context_x11->version));
  atoms = gdk_content_formats_get_mime_types (context->formats, &n_atoms);

  if (n_atoms > 3)
    {
      if (!context_x11->xdnd_targets_set)
        xdnd_set_targets (context_x11);
      xev.xclient.data.l[1] |= 1;
    }
  else
    {
      for (i = 0; i < n_atoms; i++)
        {
          xev.xclient.data.l[i + 2] = gdk_x11_atom_to_xatom_for_display (display, atoms[i]);
        }
    }

  if (!xdnd_send_xevent (context_x11, context->dest_surface, FALSE, &xev))
    {
      GDK_DISPLAY_NOTE (display, DND,
                g_message ("Send event to %lx failed",
                           GDK_SURFACE_XID (context->dest_surface)));
      g_object_unref (context->dest_surface);
      context->dest_surface = NULL;
    }
}

static void
xdnd_send_leave (GdkX11DragContext *context_x11)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = gdk_drag_context_get_display (context);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndLeave");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_SURFACE_XID (context->dest_surface);
  xev.xclient.data.l[0] = GDK_SURFACE_XID (context_x11->ipc_surface);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent (context_x11, context->dest_surface, FALSE, &xev))
    {
      GDK_DISPLAY_NOTE (display, DND,
                g_message ("Send event to %lx failed",
                           GDK_SURFACE_XID (context->dest_surface)));
      g_object_unref (context->dest_surface);
      context->dest_surface = NULL;
    }
}

static void
xdnd_send_drop (GdkX11DragContext *context_x11,
                guint32            time)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);
  GdkDisplay *display = gdk_drag_context_get_display (context);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndDrop");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_SURFACE_XID (context->dest_surface);
  xev.xclient.data.l[0] = GDK_SURFACE_XID (context_x11->ipc_surface);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = time;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent (context_x11, context->dest_surface, FALSE, &xev))
    {
      GDK_DISPLAY_NOTE (display, DND,
                g_message ("Send event to %lx failed",
                           GDK_SURFACE_XID (context->dest_surface)));
      g_object_unref (context->dest_surface);
      context->dest_surface = NULL;
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
  GdkDisplay *display = gdk_drag_context_get_display (context);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndPosition");
  xev.xclient.format = 32;
  xev.xclient.window = context_x11->drop_xid
                           ? context_x11->drop_xid
                           : GDK_SURFACE_XID (context->dest_surface);
  xev.xclient.data.l[0] = GDK_SURFACE_XID (context_x11->ipc_surface);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (x_root << 16) | y_root;
  xev.xclient.data.l[3] = time;
  xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);

  if (!xdnd_send_xevent (context_x11, context->dest_surface, FALSE, &xev))
    {
      GDK_DISPLAY_NOTE (display, DND,
                g_message ("Send event to %lx failed",
                           GDK_SURFACE_XID (context->dest_surface)));
      g_object_unref (context->dest_surface);
      context->dest_surface = NULL;
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
              GDK_DISPLAY_NOTE (display, DND,
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
              GDK_DISPLAY_NOTE (display, DND,
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
  GdkDisplay *display = gdk_drag_context_get_display (context);
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;
  gint i;

  context_x11->xdnd_have_actions = FALSE;

  if (gdk_surface_get_surface_type (context->source_surface) == GDK_SURFACE_FOREIGN)
    {
      /* Get the XdndActionList, if set */

      gdk_x11_display_error_trap_push (display);
      if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                              GDK_SURFACE_XID (context->source_surface),
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
          if (GDK_DISPLAY_DEBUG_CHECK (display, DND))
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
                                              GDK_SURFACE_XID (context->source_surface),
                                              GDK_SURFACE_XID (context->dest_surface));

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
GdkFilterReturn
xdnd_source_surface_filter (const XEvent *xevent,
                           GdkEvent     *event,
                           gpointer      data)
{
  GdkX11DragContext *context_x11;
  GdkDisplay *display;

  if (!data)
    return GDK_FILTER_CONTINUE;

  context_x11 = data;
  display = gdk_drag_context_get_display (GDK_DRAG_CONTEXT (context_x11));

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
                           GdkSurface      *surface,
                           gboolean        add_filter)
{
  if (!GDK_SURFACE_DESTROYED (surface) &&
      gdk_surface_get_surface_type (surface) == GDK_SURFACE_FOREIGN)
    {
      GdkDisplay *display = gdk_drag_context_get_display (context);

      gdk_x11_display_error_trap_push (display);

      if (add_filter)
        {
          gdk_surface_set_events (surface,
                                 gdk_surface_get_events (surface) |
                                 GDK_PROPERTY_CHANGE_MASK);
          g_object_set_data (G_OBJECT (surface), "xdnd-source-context", context);
        }
      else
        {
          g_object_set_data (G_OBJECT (surface), "xdnd-source-context", NULL);
          /* Should we remove the GDK_PROPERTY_NOTIFY mask?
           * but we might want it for other reasons. (Like
           * INCR selection transactions).
           */
        }

      gdk_x11_display_error_trap_pop_ignored (display);
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
xdnd_enter_filter (const XEvent *xevent,
                   GdkEvent     *event,
                   gpointer      cb_data)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;
  GdkSeat *seat;
  gint i;
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;
  GPtrArray *formats;
  guint32 source_surface;
  gboolean get_types;
  gint version;

  if (!event->any.surface ||
      gdk_surface_get_surface_type (event->any.surface) == GDK_SURFACE_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  source_surface = xevent->xclient.data.l[0];
  get_types = ((xevent->xclient.data.l[1] & 1) != 0);
  version = (xevent->xclient.data.l[1] & 0xff000000) >> 24;

  display = GDK_SURFACE_DISPLAY (event->any.surface);
  display_x11 = GDK_X11_DISPLAY (display);

  xdnd_precache_atoms (display);

  GDK_DISPLAY_NOTE (display, DND,
            g_message ("XdndEnter: source_surface: %#x, version: %#x",
                       source_surface, version));

  if (version < 3)
    {
      /* Old source ignore */
      GDK_DISPLAY_NOTE (display, DND, g_message ("Ignored old XdndEnter message"));
      return GDK_FILTER_REMOVE;
    }

  if (display_x11->current_dest_drag != NULL)
    {
      g_object_unref (display_x11->current_dest_drag);
      display_x11->current_dest_drag = NULL;
    }

  seat = gdk_display_get_default_seat (display);
  context_x11 = g_object_new (GDK_TYPE_X11_DRAG_CONTEXT,
                              "device", gdk_seat_get_pointer (seat),
                              NULL);
  context = (GdkDragContext *)context_x11;

  context_x11->protocol = GDK_DRAG_PROTO_XDND;
  context_x11->version = version;

  /* FIXME: Should extend DnD protocol to have device info */

  context->source_surface = gdk_x11_surface_foreign_new_for_display (display, source_surface);
  if (!context->source_surface)
    {
      g_object_unref (context);
      return GDK_FILTER_REMOVE;
    }
  context->dest_surface = event->any.surface;
  g_object_ref (context->dest_surface);

  formats = g_ptr_array_new ();
  if (get_types)
    {
      gdk_x11_display_error_trap_push (display);
      XGetWindowProperty (GDK_SURFACE_XDISPLAY (event->any.surface),
                          source_surface,
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
        g_ptr_array_add (formats, 
                         (gpointer) gdk_x11_get_xatom_name_for_display (display, atoms[i]));

      XFree (atoms);
    }
  else
    {
      for (i = 0; i < 3; i++)
        if (xevent->xclient.data.l[2 + i])
          g_ptr_array_add (formats, 
                           (gpointer) gdk_x11_get_xatom_name_for_display (display,
                                                                          xevent->xclient.data.l[2 + i]));
    }
  context->formats = gdk_content_formats_new ((const char **) formats->pdata, formats->len);
  g_ptr_array_unref (formats);

#ifdef G_ENABLE_DEBUG
  if (GDK_DISPLAY_DEBUG_CHECK (display, DND))
    print_target_list (context->formats);
#endif /* G_ENABLE_DEBUG */

  xdnd_manage_source_filter (context, context->source_surface, TRUE);
  xdnd_read_actions (context_x11);

  event->any.type = GDK_DRAG_ENTER;
  event->dnd.context = context;
  gdk_event_set_device (event, gdk_drag_context_get_device (context));
  g_object_ref (context);

  display_x11->current_dest_drag = context;

  return GDK_FILTER_TRANSLATE;
}

static GdkFilterReturn
xdnd_leave_filter (const XEvent *xevent,
                   GdkEvent     *event,
                   gpointer      data)
{
  guint32 source_surface = xevent->xclient.data.l[0];
  GdkDisplay *display;
  GdkX11Display *display_x11;

  if (!event->any.surface ||
      gdk_surface_get_surface_type (event->any.surface) == GDK_SURFACE_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  display = GDK_SURFACE_DISPLAY (event->any.surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_NOTE (display, DND,
            g_message ("XdndLeave: source_surface: %#x",
                       source_surface));

  xdnd_precache_atoms (display);

  if ((display_x11->current_dest_drag != NULL) &&
      (GDK_X11_DRAG_CONTEXT (display_x11->current_dest_drag)->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_SURFACE_XID (display_x11->current_dest_drag->source_surface) == source_surface))
    {
      event->any.type = GDK_DRAG_LEAVE;
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
xdnd_position_filter (const XEvent *xevent,
                      GdkEvent     *event,
                      gpointer      data)
{
  GdkSurfaceImplX11 *impl;
  guint32 source_surface = xevent->xclient.data.l[0];
  gint16 x_root = xevent->xclient.data.l[2] >> 16;
  gint16 y_root = xevent->xclient.data.l[2] & 0xffff;
  guint32 time = xevent->xclient.data.l[3];
  Atom action = xevent->xclient.data.l[4];
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;

   if (!event->any.surface ||
       gdk_surface_get_surface_type (event->any.surface) == GDK_SURFACE_FOREIGN)
     return GDK_FILTER_CONTINUE;                        /* Not for us */

  display = GDK_SURFACE_DISPLAY (event->any.surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_NOTE (display, DND,
            g_message ("XdndPosition: source_surface: %#x position: (%d, %d)  time: %d  action: %ld",
                       source_surface, x_root, y_root, time, action));

  xdnd_precache_atoms (display);

  context = display_x11->current_dest_drag;

  if ((context != NULL) &&
      (GDK_X11_DRAG_CONTEXT (context)->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_SURFACE_XID (context->source_surface) == source_surface))
    {
      impl = GDK_SURFACE_IMPL_X11 (event->any.surface->impl);

      context_x11 = GDK_X11_DRAG_CONTEXT (context);

      event->any.type = GDK_DRAG_MOTION;
      event->dnd.context = context;
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      g_object_ref (context);

      event->dnd.time = time;

      context->suggested_action = xdnd_action_from_atom (display, action);

      if (!context_x11->xdnd_have_actions)
        context->actions = context->suggested_action;

      event->dnd.x_root = x_root / impl->surface_scale;
      event->dnd.y_root = y_root / impl->surface_scale;

      context_x11->last_x = x_root / impl->surface_scale;
      context_x11->last_y = y_root / impl->surface_scale;

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
xdnd_drop_filter (const XEvent *xevent,
                  GdkEvent     *event,
                  gpointer      data)
{
  guint32 source_surface = xevent->xclient.data.l[0];
  guint32 time = xevent->xclient.data.l[2];
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDragContext *context;
  GdkX11DragContext *context_x11;

  if (!event->any.surface ||
      gdk_surface_get_surface_type (event->any.surface) == GDK_SURFACE_FOREIGN)
    return GDK_FILTER_CONTINUE;                 /* Not for us */

  display = GDK_SURFACE_DISPLAY (event->any.surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_NOTE (display, DND,
            g_message ("XdndDrop: source_surface: %#x  time: %d",
                       source_surface, time));

  xdnd_precache_atoms (display);

  context = display_x11->current_dest_drag;

  if ((context != NULL) &&
      (GDK_X11_DRAG_CONTEXT (context)->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_SURFACE_XID (context->source_surface) == source_surface))
    {
      context_x11 = GDK_X11_DRAG_CONTEXT (context);
      event->any.type = GDK_DROP_START;

      event->dnd.context = context;
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      g_object_ref (context);

      event->dnd.time = time;
      event->dnd.x_root = context_x11->last_x;
      event->dnd.y_root = context_x11->last_y;

      gdk_x11_surface_set_user_time (event->any.surface, time);

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

GdkFilterReturn
_gdk_x11_dnd_filter (const XEvent *xevent,
                     GdkEvent     *event,
                     gpointer      data)
{
  GdkDisplay *display;
  int i;

  if (!GDK_IS_X11_SURFACE (event->any.surface))
    return GDK_FILTER_CONTINUE;

  if (xevent->type != ClientMessage)
    return GDK_FILTER_CONTINUE;

  display = GDK_SURFACE_DISPLAY (event->any.surface);

  for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
    {
      if (xevent->xclient.message_type != gdk_x11_get_xatom_by_name_for_display (display, xdnd_filters[i].atom_name))
        continue;

      return xdnd_filters[i].func (xevent, event, data);
    }

  return GDK_FILTER_CONTINUE;
}

/* Source side */

static void
gdk_drag_do_leave (GdkX11DragContext *context_x11,
                   guint32            time)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_x11);

  if (context->dest_surface)
    {
      switch (context_x11->protocol)
        {
        case GDK_DRAG_PROTO_XDND:
          xdnd_send_leave (context_x11);
          break;
        case GDK_DRAG_PROTO_ROOTWIN:
        case GDK_DRAG_PROTO_NONE:
        default:
          break;
        }

      g_object_unref (context->dest_surface);
      context->dest_surface = NULL;
    }
}

static GdkSurface *
create_drag_surface (GdkDisplay *display)
{
  GdkSurface *surface;

  surface = gdk_surface_new_popup (display, &(GdkRectangle) { 0, 0, 100, 100 });

  gdk_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_DND);
  
  return surface;
}

static Window
_gdk_x11_display_get_drag_protocol (GdkDisplay      *display,
                                    Window           xid,
                                    GdkDragProtocol *protocol,
                                    guint           *version)

{
  GdkSurface *surface;
  Window retval;

  base_precache_atoms (display);

  /* Check for a local drag */
  surface = gdk_x11_surface_lookup_for_display (display, xid);
  if (surface && gdk_surface_get_surface_type (surface) != GDK_SURFACE_FOREIGN)
    {
      if (g_object_get_data (G_OBJECT (surface), "gdk-dnd-registered") != NULL)
        {
          *protocol = GDK_DRAG_PROTO_XDND;
          *version = 5;
          xdnd_precache_atoms (display);
          GDK_DISPLAY_NOTE (display, DND, g_message ("Entering local Xdnd window %#x\n", (guint) xid));
          return xid;
        }
      else if (_gdk_x11_display_is_root_window (display, xid))
        {
          *protocol = GDK_DRAG_PROTO_ROOTWIN;
          GDK_DISPLAY_NOTE (display, DND, g_message ("Entering root window\n"));
          return xid;
        }
    }
  else if ((retval = xdnd_check_dest (display, xid, version)))
    {
      *protocol = GDK_DRAG_PROTO_XDND;
      xdnd_precache_atoms (display);
      GDK_DISPLAY_NOTE (display, DND, g_message ("Entering Xdnd window %#x\n", (guint) xid));
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
          GDK_DISPLAY_NOTE (display, DND, g_message ("Entering root window\n"));
          *protocol = GDK_DRAG_PROTO_ROOTWIN;
          return xid;
        }
    }

  *protocol = GDK_DRAG_PROTO_NONE;

  return 0; /* a.k.a. None */
}

static GdkSurfaceCache *
drag_context_find_window_cache (GdkX11DragContext *context_x11,
                                GdkDisplay        *display)
{
  if (!context_x11->cache)
    context_x11->cache = gdk_surface_cache_get (display);

  return context_x11->cache;
}

static GdkSurface *
gdk_x11_drag_context_find_surface (GdkDragContext  *context,
                                   GdkSurface       *drag_surface,
                                   gint             x_root,
                                   gint             y_root,
                                   GdkDragProtocol *protocol)
{
  GdkX11Screen *screen_x11 = GDK_X11_SCREEN(GDK_X11_DISPLAY (context->display)->screen);
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
  GdkSurfaceCache *window_cache;
  GdkDisplay *display;
  Window dest;
  GdkSurface *dest_surface;

  display = gdk_drag_context_get_display (context);

  window_cache = drag_context_find_window_cache (context_x11, display);

  dest = get_client_window_at_coords (window_cache,
                                      drag_surface && GDK_SURFACE_IS_X11 (drag_surface) ?
                                      GDK_SURFACE_XID (drag_surface) : None,
                                      x_root * screen_x11->surface_scale,
				      y_root * screen_x11->surface_scale);

  if (context_x11->dest_xid != dest)
    {
      Window recipient;
      context_x11->dest_xid = dest;

      /* Check if new destination accepts drags, and which protocol */

      /* There is some ugliness here. We actually need to pass
       * _three_ pieces of information to drag_motion - dest_surface,
       * protocol, and the XID of the unproxied window. The first
       * two are passed explicitly, the third implicitly through
       * protocol->dest_xid.
       */
      recipient = _gdk_x11_display_get_drag_protocol (display,
                                                      dest,
                                                      protocol,
                                                      &context_x11->version);

      if (recipient != None)
        dest_surface = gdk_x11_surface_foreign_new_for_display (display, recipient);
      else
        dest_surface = NULL;
    }
  else
    {
      dest_surface = context->dest_surface;
      if (dest_surface)
        g_object_ref (dest_surface);
      *protocol = context_x11->protocol;
    }

  return dest_surface;
}

static void
move_drag_surface (GdkDragContext *context,
                  guint           x_root,
                  guint           y_root)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);

  gdk_surface_move (context_x11->drag_surface,
                   x_root - context_x11->hot_x,
                   y_root - context_x11->hot_y);
  gdk_surface_raise (context_x11->drag_surface);
}

static gboolean
gdk_x11_drag_context_drag_motion (GdkDragContext *context,
                                  GdkSurface      *dest_surface,
                                  GdkDragProtocol protocol,
                                  gint            x_root,
                                  gint            y_root,
                                  GdkDragAction   suggested_action,
                                  GdkDragAction   possible_actions,
                                  guint32         time)
{
  GdkX11DragContext *context_x11 = GDK_X11_DRAG_CONTEXT (context);
  GdkSurfaceImplX11 *impl;

  if (context_x11->drag_surface)
    move_drag_surface (context, x_root, y_root);

  context_x11->old_actions = context->actions;
  context->actions = possible_actions;

  if (context_x11->old_actions != possible_actions)
    context_x11->xdnd_actions_set = FALSE;

  if (protocol == GDK_DRAG_PROTO_XDND && context_x11->version == 0)
    {
      /* This ugly hack is necessary since GTK+ doesn't know about
       * the XDND protocol version, and in particular doesn't know
       * that gdk_drag_find_window() has the side-effect
       * of setting context_x11->version, and therefore sometimes call
       * gdk_x11_drag_context_drag_motion() without a prior call to
       * gdk_drag_find_window(). This happens, e.g.
       * when GTK+ is proxying DND events to embedded windows.
       */
      if (dest_surface)
        {
          GdkDisplay *display = GDK_SURFACE_DISPLAY (dest_surface);

          xdnd_check_dest (display,
                           GDK_SURFACE_XID (dest_surface),
                           &context_x11->version);
        }
    }

  /* When we have a Xdnd target, make sure our XdndActionList
   * matches the current actions;
   */
  if (protocol == GDK_DRAG_PROTO_XDND && !context_x11->xdnd_actions_set)
    {
      if (dest_surface)
        {
          if (gdk_surface_get_surface_type (dest_surface) == GDK_SURFACE_FOREIGN)
            xdnd_set_actions (context_x11);
          else if (context->dest_surface == dest_surface)
            {
              GdkDisplay *display = GDK_SURFACE_DISPLAY (dest_surface);
              GdkDragContext *dest_context;

              dest_context = gdk_drag_context_find (display, FALSE,
                                                    GDK_SURFACE_XID (context->source_surface),
                                                    GDK_SURFACE_XID (dest_surface));

              if (dest_context)
                {
                  dest_context->actions = context->actions;
                  GDK_X11_DRAG_CONTEXT (dest_context)->xdnd_have_actions = TRUE;
                }
            }
        }
    }

  if (context->dest_surface != dest_surface)
    {
      /* Send a leave to the last destination */
      gdk_drag_do_leave (context_x11, time);
      context_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */

      if (dest_surface)
        {
          context->dest_surface = dest_surface;
          context_x11->drop_xid = context_x11->dest_xid;
          g_object_ref (context->dest_surface);
          context_x11->protocol = protocol;

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
          context->dest_surface = NULL;
          context_x11->drop_xid = None;
          context->action = 0;
        }

      /* Push a status event, to let the client know that
       * the drag changed
       */
      if (context->action != context_x11->current_action)
        {
          context_x11->current_action = context->action;
          g_signal_emit_by_name (context, "action-changed", context->action);
        }
    }
  else
    {
      context_x11->old_action = context->suggested_action;
      context->suggested_action = suggested_action;
    }

  /* Send a drag-motion event */

  context_x11->last_x = x_root;
  context_x11->last_y = y_root;

  if (context->dest_surface)
    {
      impl = GDK_SURFACE_IMPL_X11 (context->dest_surface->impl);

      if (context_x11->drag_status == GDK_DRAG_STATUS_DRAG)
        {
          switch (context_x11->protocol)
            {
            case GDK_DRAG_PROTO_XDND:
              xdnd_send_motion (context_x11, x_root * impl->surface_scale, y_root * impl->surface_scale, suggested_action, time);
              break;

            case GDK_DRAG_PROTO_ROOTWIN:
              {
                /* GTK+ traditionally has used application/x-rootwin-drop,
                 * but the XDND spec specifies x-rootwindow-drop.
                 */
                if (gdk_content_formats_contain_mime_type (context->formats, "application/x-rootwindow-drop") ||
                    gdk_content_formats_contain_mime_type (context->formats, "application/x-rootwin-drop"))
                  context->action = context->suggested_action;
                else
                  context->action = 0;

                if (context->action != context_x11->current_action)
                  {
                    context_x11->current_action = context->action;
                    g_signal_emit_by_name (context, "action-changed", context->action);
                  }
              }
              break;
            case GDK_DRAG_PROTO_NONE:
              g_warning ("Invalid drag protocol %u in gdk_x11_drag_context_drag_motion()", context_x11->protocol);
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

  if (context->dest_surface)
    {
      switch (context_x11->protocol)
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
          g_warning ("Drag protocol %u is not valid in gdk_drag_drop()", context_x11->protocol);
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

  display = gdk_drag_context_get_display (context);

  context->action = action;

  if (context_x11->protocol == GDK_DRAG_PROTO_XDND)
    {
      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndStatus");
      xev.xclient.format = 32;
      xev.xclient.window = GDK_SURFACE_XID (context->source_surface);

      xev.xclient.data.l[0] = GDK_SURFACE_XID (context->dest_surface);
      xev.xclient.data.l[1] = (action != 0) ? (2 | 1) : 0;
      xev.xclient.data.l[2] = 0;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);
      if (!xdnd_send_xevent (context_x11, context->source_surface, FALSE, &xev))
        {
          GDK_DISPLAY_NOTE (display, DND,
                    g_message ("Send event to %lx failed",
                               GDK_SURFACE_XID (context->source_surface)));
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
  if (GDK_X11_DRAG_CONTEXT (context)->protocol == GDK_DRAG_PROTO_XDND)
    {
      GdkDisplay *display = gdk_drag_context_get_display (context);
      XEvent xev;

      if (success && gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE)
        {
          XConvertSelection (GDK_DISPLAY_XDISPLAY (display),
                             gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection"),
                             gdk_x11_get_xatom_by_name_for_display (display, "DELETE"),
                             gdk_x11_get_xatom_by_name_for_display (display, "GDK_SELECTION"),
                             GDK_SURFACE_XID (context->source_surface),
                             time);
          /* XXX: Do we need to wait for a reply here before sending the next message? */
        }

      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndFinished");
      xev.xclient.format = 32;
      xev.xclient.window = GDK_SURFACE_XID (context->source_surface);

      xev.xclient.data.l[0] = GDK_SURFACE_XID (context->dest_surface);
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

      if (!xdnd_send_xevent (GDK_X11_DRAG_CONTEXT (context), context->source_surface, FALSE, &xev))
        {
          GDK_DISPLAY_NOTE (display, DND,
                    g_message ("Send event to %lx failed",
                               GDK_SURFACE_XID (context->source_surface)));
        }
    }
}

void
_gdk_x11_surface_register_dnd (GdkSurface *surface)
{
  static const gulong xdnd_version = 5;
  GdkDisplay *display = gdk_surface_get_display (surface);

  g_return_if_fail (surface != NULL);

  base_precache_atoms (display);

  if (g_object_get_data (G_OBJECT (surface), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (surface), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));

  /* Set XdndAware */

  /* The property needs to be of type XA_ATOM, not XA_INTEGER. Blech */
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (surface),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndAware"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)&xdnd_version, 1);
}

static gboolean
gdk_x11_drag_context_drop_status (GdkDragContext *context)
{
  return ! GDK_X11_DRAG_CONTEXT (context)->drop_failed;
}

static GdkSurface *
gdk_x11_drag_context_get_drag_surface (GdkDragContext *context)
{
  return GDK_X11_DRAG_CONTEXT (context)->drag_surface;
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
      move_drag_surface (context, x11_context->last_x, x11_context->last_y);
    }
}

static void
gdk_x11_drag_context_default_output_done (GObject      *context,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  GError *error = NULL;

  if (!gdk_drag_context_write_finish (GDK_DRAG_CONTEXT (context), result, &error))
    {
      GDK_DISPLAY_NOTE (gdk_drag_context_get_display (GDK_DRAG_CONTEXT (context)), DND, g_printerr ("failed to write stream: %s\n", error->message));
      g_error_free (error);
    }
}

static void
gdk_x11_drag_context_default_output_handler (GOutputStream   *stream,
                                             const char      *mime_type,
                                             gpointer         user_data)
{
  gdk_drag_context_write_async (GDK_DRAG_CONTEXT (user_data),
                                mime_type,
                                stream,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                gdk_x11_drag_context_default_output_done,
                                NULL);
  g_object_unref (stream);
}

static gboolean
gdk_x11_drag_context_xevent (GdkDisplay   *display,
                             const XEvent *xevent,
                             gpointer      data)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (data);
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  Window xwindow;
  Atom xselection;

  xwindow = GDK_SURFACE_XID (x11_context->ipc_surface);
  xselection = gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection");

  if (xevent->xany.window != xwindow)
    return FALSE;

  switch (xevent->type)
  {
    case SelectionClear:
      if (xevent->xselectionclear.selection != xselection)
        return FALSE;

      if (xevent->xselectionclear.time < x11_context->timestamp)
        {
          GDK_DISPLAY_NOTE (display, CLIPBOARD, g_printerr ("ignoring SelectionClear with too old timestamp (%lu vs %lu)\n",
                                          xevent->xselectionclear.time, x11_context->timestamp));
          return FALSE;
        }

      GDK_DISPLAY_NOTE (display, CLIPBOARD, g_printerr ("got SelectionClear, aborting DND\n"));
      gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_ERROR);
      return TRUE;

    case SelectionRequest:
      {
        const char *target, *property;

        if (xevent->xselectionrequest.selection != xselection)
          return FALSE;

        target = gdk_x11_get_xatom_name_for_display (display, xevent->xselectionrequest.target);
        if (xevent->xselectionrequest.property == None)
          property = target;
        else
          property = gdk_x11_get_xatom_name_for_display (display, xevent->xselectionrequest.property);

        if (xevent->xselectionrequest.requestor == None)
          {
            GDK_DISPLAY_NOTE (display, CLIPBOARD, g_printerr ("got SelectionRequest for %s @ %s with NULL window, ignoring\n",
                                            target, property));
            return TRUE;
          }
        
        GDK_DISPLAY_NOTE (display, CLIPBOARD, g_printerr ("got SelectionRequest for %s @ %s\n",
                                        target, property));

        gdk_x11_selection_output_streams_create (display,
                                                 gdk_drag_context_get_formats (context),
                                                 xevent->xselectionrequest.requestor,
                                                 xevent->xselectionrequest.selection,
                                                 xevent->xselectionrequest.target,
                                                 xevent->xselectionrequest.property ? xevent->xselectionrequest.property
                                                                                    : xevent->xselectionrequest.target,
                                                 xevent->xselectionrequest.time,
                                                 gdk_x11_drag_context_default_output_handler,
                                                 context);
        return TRUE;
      }

    default:
      return FALSE;
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

  gdk_surface_show (context->drag_surface);
  gdk_surface_move (context->drag_surface,
                   context->last_x + (context->start_x - context->last_x) * t,
                   context->last_y + (context->start_y - context->last_y) * t);
  gdk_surface_set_opacity (context->drag_surface, 1.0 - f);

  return G_SOURCE_CONTINUE;
}

static void
gdk_x11_drag_context_release_selection (GdkDragContext *context)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  Atom xselection;

  display = gdk_drag_context_get_display (context);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  xselection = gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection");
  xwindow = GDK_SURFACE_XID (x11_context->ipc_surface);

  if (XGetSelectionOwner (xdisplay, xselection) == xwindow)
    XSetSelectionOwner (xdisplay, xselection, None, CurrentTime);
}

static void
gdk_x11_drag_context_drop_done (GdkDragContext *context,
                                gboolean        success)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDragAnim *anim;
/*
  cairo_surface_t *win_surface;
  cairo_surface_t *surface;
  cairo_t *cr;
*/
  guint id;

  gdk_x11_drag_context_release_selection (context);

  g_signal_handlers_disconnect_by_func (gdk_drag_context_get_display (context),
                                        gdk_x11_drag_context_xevent,
                                        context);
  if (success)
    {
      gdk_surface_hide (x11_context->drag_surface);
      return;
    }

/*
  win_surface = _gdk_surface_ref_cairo_surface (x11_context->drag_surface);
  surface = gdk_surface_create_similar_surface (x11_context->drag_surface,
                                               cairo_surface_get_content (win_surface),
                                               gdk_surface_get_width (x11_context->drag_surface),
                                               gdk_surface_get_height (x11_context->drag_surface));
  cr = cairo_create (surface);
  cairo_set_source_surface (cr, win_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy (win_surface);

  pattern = cairo_pattern_create_for_surface (surface);

  gdk_surface_set_background_pattern (x11_context->drag_surface, pattern);

  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);
*/

  anim = g_slice_new0 (GdkDragAnim);
  anim->context = g_object_ref (x11_context);
  anim->frame_clock = gdk_surface_get_frame_clock (x11_context->drag_surface);
  anim->start_time = gdk_frame_clock_get_frame_time (anim->frame_clock);

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 17,
                           gdk_drag_anim_timeout, anim,
                           (GDestroyNotify) gdk_drag_anim_destroy);
  g_source_set_name_by_id (id, "[gtk+] gdk_drag_anim_timeout");
}

static gboolean
drag_context_grab (GdkDragContext *context)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDevice *device = gdk_drag_context_get_device (context);
  GdkSeatCapabilities capabilities;
  GdkDisplay *display;
  Window root;
  GdkSeat *seat;
  gint keycode, i;
  GdkCursor *cursor;

  if (!x11_context->ipc_surface)
    return FALSE;

  display = gdk_drag_context_get_display (context);
  root = GDK_DISPLAY_XROOTWIN (display);
  seat = gdk_device_get_seat (gdk_drag_context_get_device (context));

#ifdef XINPUT_2
  if (GDK_IS_X11_DEVICE_XI2 (device))
    capabilities = GDK_SEAT_CAPABILITY_ALL_POINTING;
  else
#endif
    capabilities = GDK_SEAT_CAPABILITY_ALL;

  cursor = gdk_drag_get_cursor (context, x11_context->current_action);
  g_set_object (&x11_context->cursor, cursor);

  if (gdk_seat_grab (seat, x11_context->ipc_surface,
                     capabilities, FALSE,
                     x11_context->cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    return FALSE;

  g_set_object (&x11_context->grab_seat, seat);

  gdk_x11_display_error_trap_push (context->display);

  for (i = 0; i < G_N_ELEMENTS (grab_keys); ++i)
    {
      keycode = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY (display),
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

          XIGrabKeycode (GDK_DISPLAY_XDISPLAY (display),
                         deviceid,
                         keycode,
                         root,
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
          XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                    keycode, grab_keys[i].modifiers,
                    root,
                    FALSE,
                    GrabModeAsync,
                    GrabModeAsync);
        }
    }

  gdk_x11_display_error_trap_pop_ignored (context->display);

  return TRUE;
}

static void
drag_context_ungrab (GdkDragContext *context)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);
  GdkDisplay *display;
  GdkDevice *keyboard;
  Window root;
  gint keycode, i;

  if (!x11_context->grab_seat)
    return;

  gdk_seat_ungrab (x11_context->grab_seat);

  display = gdk_drag_context_get_display (context);
  keyboard = gdk_seat_get_keyboard (x11_context->grab_seat);
  root = GDK_DISPLAY_XROOTWIN (display);
  g_clear_object (&x11_context->grab_seat);

  for (i = 0; i < G_N_ELEMENTS (grab_keys); ++i)
    {
      keycode = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY (display),
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

          XIUngrabKeycode (GDK_DISPLAY_XDISPLAY (display),
                           gdk_x11_device_get_id (keyboard),
                           keycode,
                           root,
                           num_mods,
                           &mods);
        }
      else
#endif /* XINPUT_2 */
        {
          XUngrabKey (GDK_DISPLAY_XDISPLAY (display),
                      keycode, grab_keys[i].modifiers,
                      root);
        }
    }
}

GdkDragContext *
_gdk_x11_surface_drag_begin (GdkSurface          *surface,
                            GdkDevice          *device,
                            GdkContentProvider *content,
                            GdkDragAction       actions,
                            gint                dx,
                            gint                dy)
{
  GdkX11DragContext *x11_context;
  GdkDragContext *context;
  GdkDisplay *display;
  int x_root, y_root;
  Atom xselection;

  display = gdk_surface_get_display (surface);

  context = (GdkDragContext *) g_object_new (GDK_TYPE_X11_DRAG_CONTEXT,
                                             "device", device,
                                             "content", content,
                                             NULL);
  x11_context = GDK_X11_DRAG_CONTEXT (context);

  context->is_source = TRUE;

  g_signal_connect (display, "xevent", G_CALLBACK (gdk_x11_drag_context_xevent), context);

  precache_target_list (context);

  gdk_device_get_position (device, &x_root, &y_root);
  x_root += dx;
  y_root += dy;

  x11_context->start_x = x_root;
  x11_context->start_y = y_root;
  x11_context->last_x = x_root;
  x11_context->last_y = y_root;

  x11_context->protocol = GDK_DRAG_PROTO_XDND;
  x11_context->actions = actions;
  x11_context->ipc_surface = gdk_surface_new_popup (display, &(GdkRectangle) { -99, -99, 1, 1 });
  if (gdk_surface_get_group (surface))
    gdk_surface_set_group (x11_context->ipc_surface, surface);
  gdk_surface_show (x11_context->ipc_surface);

  context->source_surface = x11_context->ipc_surface;
  g_object_ref (context->source_surface);

  x11_context->drag_surface = create_drag_surface (display);

  if (!drag_context_grab (context))
    {
      g_object_unref (context);
      return NULL;
    }
  
  move_drag_surface (context, x_root, y_root);

  x11_context->timestamp = gdk_display_get_last_seen_time (display);
  xselection = gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection");
  XSetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
                      xselection,
                      GDK_SURFACE_XID (x11_context->ipc_surface),
                      x11_context->timestamp);
  if (XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display), xselection) != GDK_SURFACE_XID (x11_context->ipc_surface))
    {
      GDK_DISPLAY_NOTE (display, DND, g_printerr ("failed XSetSelectionOwner() on \"XdndSelection\", aborting DND\n"));
      g_object_unref (context);
      return NULL;
    }

  return context;
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
                       x11_context->ipc_surface,
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
  gdk_drag_drop (context, time_);
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
  GdkSurface *dest_surface;
  GdkDragProtocol protocol;

  gdk_drag_get_current_actions (mods, GDK_BUTTON_PRIMARY, x11_context->actions,
                                &action, &possible_actions);

  dest_surface = gdk_x11_drag_context_find_surface (context,
                                                    x11_context->drag_surface,
                                                    x_root, y_root, &protocol);

  gdk_x11_drag_context_drag_motion (context, dest_surface, protocol, x_root, y_root,
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
  GdkDevice *pointer;
  gint dx, dy;

  dx = dy = 0;
  state = event->state;
  pointer = gdk_device_get_associated_device (gdk_event_get_device ((GdkEvent *) event));

  if (event->any.type == GDK_KEY_PRESS)
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
              (gdk_drag_context_get_dest_surface (context) != NULL))
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

        default:
          break;
        }
    }

  /* The state is not yet updated in the event, so we need
   * to query it here. We could use XGetModifierMapping, but
   * that would be overkill.
   */
  _gdk_device_query_state (pointer, NULL, NULL, NULL, NULL, NULL, NULL, &state);

  if (dx != 0 || dy != 0)
    {
      x11_context->last_x += dx;
      x11_context->last_y += dy;
      gdk_device_warp (pointer, x11_context->last_x, x11_context->last_y);
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
      event->grab_surface == x11_context->drag_surface ||
      event->grab_surface == x11_context->ipc_surface)
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
      (gdk_drag_context_get_dest_surface (context) != NULL))
    {
      g_signal_emit_by_name (context, "drop-performed",
                             gdk_event_get_time ((GdkEvent *) event));
    }
  else
    gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_NO_TARGET);

  return TRUE;
}

gboolean
gdk_x11_drag_context_handle_event (GdkDragContext *context,
                                   const GdkEvent *event)
{
  GdkX11DragContext *x11_context = GDK_X11_DRAG_CONTEXT (context);

  if (!context->is_source)
    return FALSE;
  if (!x11_context->grab_seat)
    return FALSE;

  switch ((guint) event->any.type)
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
