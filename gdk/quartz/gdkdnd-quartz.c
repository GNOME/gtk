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

#include "config.h"
#include "gdkdnd.h"
#include "gdkquartzdnd.h"
#include "gdkprivate-quartz.h"


G_DEFINE_TYPE (GdkQuartzDragContext, gdk_quartz_drag_context, GDK_TYPE_DRAG_CONTEXT)


GdkDragContext *_gdk_quartz_drag_source_context = NULL;

GdkDragContext *
gdk_quartz_drag_source_context_libgtk_only ()
{
  return _gdk_quartz_drag_source_context;
}

GdkDragContext *
_gdk_quartz_surface_drag_begin (GdkSurface *window,
                               GdkDevice *device,
                               GList     *targets,
                               gint       dx,
                               gint       dy)
{
  g_assert (_gdk_quartz_drag_source_context == NULL);

  /* Create fake context */
  _gdk_quartz_drag_source_context = g_object_new (GDK_TYPE_QUARTZ_DRAG_CONTEXT,
                                                  "device", device,
                                                  NULL);

  _gdk_quartz_drag_source_context->source_surface = window;
  g_object_ref (window);

  _gdk_quartz_drag_source_context->targets = targets;

  return _gdk_quartz_drag_source_context;
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

  context_class->drag_abort = gdk_quartz_drag_context_drag_abort;
  context_class->drag_drop = gdk_quartz_drag_context_drag_drop;
}
