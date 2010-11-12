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
#include "gdkx.h"
#include "gdkasync.h"
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

/**
 * gdk_drag_context_new:
 * 
 * Creates a new #GdkDragContext.
 * 
 * Return value: the newly created #GdkDragContext.
 **/
GdkDragContext *
gdk_drag_context_new (void)
{
  return g_object_new (GDK_TYPE_DRAG_CONTEXT, NULL);
}

/**
 * gdk_drag_context_set_device:
 * @context: a #GdkDragContext
 * @device: a #GdkDevice
 *
 * Associates a #GdkDevice to @context, so all Drag and Drop events
 * for @context are emitted as if they came from this device.
 **/
void
gdk_drag_context_set_device (GdkDragContext *context,
                             GdkDevice      *device)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GDK_IS_DEVICE (device));
}

/**
 * gdk_drag_context_get_device:
 * @context: a #GdkDragContext
 *
 * Returns the #GdkDevice associated to the drag context.
 *
 * Returns: The #GdkDevice associated to @context.
 **/
GdkDevice *
gdk_drag_context_get_device (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return NULL;
}

/**
 * gdk_drag_begin:
 * @window: the source window for this drag.
 * @targets: (transfer none) (element-type GdkAtom): the offered targets,
 *     as list of #GdkAtom<!-- -->s
 * 
 * Starts a drag and creates a new drag context for it.
 *
 * This function is called by the drag source.
 * 
 * Return value: a newly created #GdkDragContext.
 **/
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


/**
 * gdk_drag_get_protocol_for_display:
 * @display: the #GdkDisplay where the destination window resides
 * @xid: the windowing system id of the destination window.
 * @protocol: location where the supported DND protocol is returned.
 * @returns: the windowing system id of the window where the drop should happen. This 
 *     may be @xid or the id of a proxy window, or zero if @xid doesn't
 *     support Drag and Drop.
 *
 * Finds out the DND protocol supported by a window.
 *
 * Since: 2.2
 */ 
GdkNativeWindow
gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				   GdkNativeWindow  xid,
				   GdkDragProtocol *protocol)
{
  return 0;
}

/**
 * gdk_drag_find_window_for_screen:
 * @context: a #GdkDragContext
 * @drag_window: a window which may be at the pointer position, but
 * should be ignored, since it is put up by the drag source as an icon.
 * @screen: the screen where the destination window is sought. 
 * @x_root: the x position of the pointer in root coordinates.
 * @y_root: the y position of the pointer in root coordinates.
 * @dest_window: (out): location to store the destination window in.
 * @protocol: (out): location to store the DND protocol in.
 *
 * Finds the destination window and DND protocol to use at the
 * given pointer position.
 *
 * This function is called by the drag source to obtain the 
 * @dest_window and @protocol parameters for gdk_drag_motion().
 *
 * Since: 2.2
 **/
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

/**
 * gdk_drag_motion:
 * @context: a #GdkDragContext.
 * @dest_window: the new destination window, obtained by 
 *     gdk_drag_find_window().
 * @protocol: the DND protocol in use, obtained by gdk_drag_find_window().
 * @x_root: the x position of the pointer in root coordinates.
 * @y_root: the y position of the pointer in root coordinates.
 * @suggested_action: the suggested action.
 * @possible_actions: the possible actions.
 * @time_: the timestamp for this operation.
 * 
 * Updates the drag context when the pointer moves or the 
 * set of actions changes.
 *
 * This function is called by the drag source.
 * 
 * Return value: FIXME
 **/
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

/**
 * gdk_drag_drop:
 * @context: a #GdkDragContext.
 * @time_: the timestamp for this operation.
 * 
 * Drops on the current destination.
 * 
 * This function is called by the drag source.
 **/
void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);
}

/**
 * gdk_drag_abort:
 * @context: a #GdkDragContext.
 * @time_: the timestamp for this operation.
 * 
 * Aborts a drag without dropping. 
 *
 * This function is called by the drag source.
 **/
void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);
}

/* Destination side */

/**
 * gdk_drag_status:
 * @context: a #GdkDragContext.
 * @action: the selected action which will be taken when a drop happens, 
 *    or 0 to indicate that a drop will not be accepted.
 * @time_: the timestamp for this operation.
 * 
 * Selects one of the actions offered by the drag source.
 *
 * This function is called by the drag destination in response to
 * gdk_drag_motion() called by the drag source.
 **/
void             
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  g_return_if_fail (context != NULL);
}

/**
 * gdk_drop_reply:
 * @context: a #GdkDragContext.
 * @ok: %TRUE if the drop is accepted.
 * @time_: the timestamp for this operation.
 * 
 * Accepts or rejects a drop. 
 *
 * This function is called by the drag destination in response
 * to a drop initiated by the drag source.
 **/
void 
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
  g_return_if_fail (context != NULL);
}

/**
 * gdk_drop_finish:
 * @context: a #GtkDragContext.
 * @success: %TRUE if the data was successfully received.
 * @time_: the timestamp for this operation.
 * 
 * Ends the drag operation after a drop.
 *
 * This function is called by the drag destination.
 **/
void             
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
  g_return_if_fail (context != NULL);
}


/**
 * gdk_window_register_dnd:
 * @window: a #GdkWindow.
 *
 * Registers a window as a potential drop destination.
 */
void            
gdk_window_register_dnd (GdkWindow      *window)
{
}

/**
 * gdk_drag_get_selection:
 * @context: a #GdkDragContext.
 * 
 * Returns the selection atom for the current source window.
 * 
 * Return value: the selection atom.
 **/
GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  return GDK_NONE;
}

/**
 * gdk_drag_drop_succeeded:
 * @context: a #GdkDragContext
 * 
 * Returns whether the dropped data has been successfully 
 * transferred. This function is intended to be used while 
 * handling a %GDK_DROP_FINISHED event, its return value is
 * meaningless at other times.
 * 
 * Return value: %TRUE if the drop was successful.
 *
 * Since: 2.6
 **/
gboolean 
gdk_drag_drop_succeeded (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return FALSE;
}
