/* gdkdnd-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "gdkdnd.h"
#include "gdkquartzdnd.h"
#include "gdkprivate-quartz.h"


G_DEFINE_TYPE (GdkQuartzDragContext, gdk_quartz_drag_context, GDK_TYPE_DRAG_CONTEXT)


GdkDragContext *_gdk_quartz_drag_source_context = NULL;

GdkDragContext *
gdk_quartz_drag_source_context ()
{
  return _gdk_quartz_drag_source_context;
}

GdkDragContext *
_gdk_quartz_window_drag_begin (GdkWindow *window,
                               GdkDevice *device,
                               GList     *targets)
{
  g_assert (_gdk_quartz_drag_source_context == NULL);

  /* Create fake context */
  _gdk_quartz_drag_source_context = g_object_new (GDK_TYPE_QUARTZ_DRAG_CONTEXT,
                                                  NULL);
  _gdk_quartz_drag_source_context->is_source = TRUE;

  _gdk_quartz_drag_source_context->targets = targets;

  gdk_drag_context_set_device (_gdk_quartz_drag_source_context, device);

  return _gdk_quartz_drag_source_context;
}

static gboolean
gdk_quartz_drag_context_drag_motion (GdkDragContext  *context,
                                     GdkWindow       *dest_window,
                                     GdkDragProtocol  protocol,
                                     gint             x_root,
                                     gint             y_root,
                                     GdkDragAction    suggested_action,
                                     GdkDragAction    possible_actions,
                                     guint32          time)
{
  /* FIXME: Implement */
  return FALSE;
}

static GdkWindow *
gdk_quartz_drag_context_find_window (GdkDragContext  *context,
                                     GdkWindow       *drag_window,
                                     GdkScreen       *screen,
                                     gint             x_root,
                                     gint             y_root,
                                     GdkDragProtocol *protocol)
{
  /* FIXME: Implement */
  return NULL;
}

static void
gdk_quartz_drag_context_drag_drop (GdkDragContext *context,
                                   guint32         time)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_drag_context_drag_abort (GdkDragContext *context,
                                    guint32         time)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_drag_context_drag_status (GdkDragContext *context,
                                     GdkDragAction   action,
                                     guint32         time)
{
  context->action = action;
}

static void
gdk_quartz_drag_context_drop_reply (GdkDragContext *context,
                                    gboolean        ok,
                                    guint32         time)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_drag_context_drop_finish (GdkDragContext *context,
                                     gboolean        success,
                                     guint32         time)
{
  /* FIXME: Implement */
}

void
_gdk_quartz_window_register_dnd (GdkWindow *window)
{
  /* FIXME: Implement */
}

static GdkAtom
gdk_quartz_drag_context_get_selection (GdkDragContext *context)
{
  /* FIXME: Implement */
  return GDK_NONE;
}

static gboolean
gdk_quartz_drag_context_drop_status (GdkDragContext *context)
{
  /* FIXME: Implement */
  return FALSE;
}

id
gdk_quartz_drag_context_get_dragging_info_libgtk_only (GdkDragContext *context)
{
  return GDK_QUARTZ_DRAG_CONTEXT (context)->dragging_info;
}

static void
gdk_quartz_drag_context_init (GdkQuartzDragContext *context)
{
}

static void
gdk_quartz_drag_context_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_quartz_drag_context_parent_class)->finalize (object);
}

static void
gdk_quartz_drag_context_class_init (GdkQuartzDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_quartz_drag_context_finalize;

  context_class->find_window = gdk_quartz_drag_context_find_window;
  context_class->drag_status = gdk_quartz_drag_context_drag_status;
  context_class->drag_motion = gdk_quartz_drag_context_drag_motion;
  context_class->drag_abort = gdk_quartz_drag_context_drag_abort;
  context_class->drag_drop = gdk_quartz_drag_context_drag_drop;
  context_class->drop_reply = gdk_quartz_drag_context_drop_reply;
  context_class->drop_finish = gdk_quartz_drag_context_drop_finish;
  context_class->drop_status = gdk_quartz_drag_context_drop_status;
  context_class->get_selection = gdk_quartz_drag_context_get_selection;
}
