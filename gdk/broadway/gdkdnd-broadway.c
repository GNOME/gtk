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

#include "gdkdndprivate.h"

#include "gdkinternals.h"
#include "gdkproperty.h"
#include "gdkprivate-broadway.h"
#include "gdkinternals.h"
#include "gdkdisplay-broadway.h"

#include <string.h>

#define GDK_TYPE_BROADWAY_DRAG_CONTEXT              (gdk_broadway_drag_context_get_type ())
#define GDK_BROADWAY_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_DRAG_CONTEXT, GdkBroadwayDragContext))
#define GDK_BROADWAY_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_DRAG_CONTEXT, GdkBroadwayDragContextClass))
#define GDK_IS_BROADWAY_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_DRAG_CONTEXT))
#define GDK_IS_BROADWAY_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_DRAG_CONTEXT))
#define GDK_BROADWAY_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_DRAG_CONTEXT, GdkBroadwayDragContextClass))

#ifdef GDK_COMPILATION
typedef struct _GdkBroadwayDragContext GdkBroadwayDragContext;
#else
typedef GdkDragContext GdkBroadwayDragContext;
#endif
typedef struct _GdkBroadwayDragContextClass GdkBroadwayDragContextClass;

GType     gdk_broadway_drag_context_get_type (void);

struct _GdkBroadwayDragContext {
  GdkDragContext context;
};

struct _GdkBroadwayDragContextClass
{
  GdkDragContextClass parent_class;
};

static void gdk_broadway_drag_context_finalize (GObject *object);

static GList *contexts;

G_DEFINE_TYPE (GdkBroadwayDragContext, gdk_broadway_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_broadway_drag_context_init (GdkBroadwayDragContext *dragcontext)
{
  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_broadway_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (gdk_broadway_drag_context_parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDragContext *
_gdk_broadway_surface_drag_begin (GdkSurface         *surface,
                                  GdkDevice          *device,
                                  GdkContentProvider *content,
                                  GdkDragAction       actions,
                                  gint                dx,
                                  gint                dy)
{
  GdkDragContext *new_context;

  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (GDK_SURFACE_IS_BROADWAY (surface), NULL);

  new_context = g_object_new (GDK_TYPE_BROADWAY_DRAG_CONTEXT,
                              "device", device,
                              "content", content,
                              NULL);

  return new_context;
}

static void
gdk_broadway_drag_context_drag_drop (GdkDragContext *context,
                                     guint32         time)
{
  g_return_if_fail (context != NULL);
}

static void
gdk_broadway_drag_context_drag_abort (GdkDragContext *context,
                                      guint32         time)
{
  g_return_if_fail (context != NULL);
}

void
_gdk_broadway_surface_register_dnd (GdkSurface      *surface)
{
}

void
_gdk_broadway_display_init_dnd (GdkDisplay *display)
{
}

static void
gdk_broadway_drag_context_class_init (GdkBroadwayDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_broadway_drag_context_finalize;

  context_class->drag_abort = gdk_broadway_drag_context_drag_abort;
  context_class->drag_drop = gdk_broadway_drag_context_drag_drop;
}
