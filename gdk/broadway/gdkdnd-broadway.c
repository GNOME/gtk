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

#include "config.h"

#include "gdkdnd.h"

#include "gdkmain.h"
#include "gdkproperty.h"
#include "gdkprivate-broadway.h"
#include "gdkinternals.h"
#include "gdkscreen-broadway.h"
#include "gdkdisplay-broadway.h"

#include <string.h>

typedef struct _GdkDragContextPrivateX11 GdkDragContextPrivateX11;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivateX11 {
  GdkDragContext context;
};

#define PRIVATE_DATA(context) ((GdkDragContextPrivateX11 *) GDK_DRAG_CONTEXT (context)->windowing_data)

static void gdk_drag_context_finalize (GObject *object);

static GList *contexts;

G_DEFINE_TYPE (GdkDragContext, gdk_drag_context, G_TYPE_OBJECT)

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  GdkDragContextPrivateX11 *private;

  private = G_TYPE_INSTANCE_GET_PRIVATE (dragcontext,
					 GDK_TYPE_DRAG_CONTEXT,
					 GdkDragContextPrivateX11);

  dragcontext->windowing_data = private;

  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_drag_context_finalize;

  g_type_class_add_private (object_class, sizeof (GdkDragContextPrivateX11));
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (gdk_drag_context_parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDragContext *
gdk_drag_context_new (void)
{
  return g_object_new (GDK_TYPE_DRAG_CONTEXT, NULL);
}

void
gdk_drag_context_set_device (GdkDragContext *context,
                             GdkDevice      *device)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GDK_IS_DEVICE (device));
}

GdkDevice *
gdk_drag_context_get_device (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return NULL;
}

GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
  GdkDragContext *new_context;

  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_WINDOW_IS_X11 (window), NULL);

  new_context = gdk_drag_context_new ();

  return new_context;
}

GdkNativeWindow
gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				   GdkNativeWindow  xid,
				   GdkDragProtocol *protocol)
{
  return 0;
}

void
gdk_drag_find_window_for_screen (GdkDragContext  *context,
				 GdkWindow       *drag_window,
				 GdkScreen       *screen,
				 gint             x_root,
				 gint             y_root,
				 GdkWindow      **dest_window,
				 GdkDragProtocol *protocol)
{
  g_return_if_fail (context != NULL);
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
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (dest_window == NULL || GDK_WINDOW_IS_X11 (dest_window), FALSE);

  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);
}

/* Destination side */

void
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  g_return_if_fail (context != NULL);
}

void
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
  g_return_if_fail (context != NULL);
}

void
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
  g_return_if_fail (context != NULL);
}

void
gdk_window_register_dnd (GdkWindow      *window)
{
}

GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  return GDK_NONE;
}

gboolean
gdk_drag_drop_succeeded (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return FALSE;
}

void
_gdk_dnd_init (GdkDisplay *display)
{
}
