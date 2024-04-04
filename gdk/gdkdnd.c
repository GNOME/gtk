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
#include "gdkdisplay.h"
#include "gdkwindow.h"
#include "gdkintl.h"
#include "gdkenumtypes.h"
#include "gdkcursor.h"

static struct {
  GdkDragAction action;
  const gchar  *name;
  GdkCursor    *cursor;
} drag_cursors[] = {
  { GDK_ACTION_DEFAULT, NULL, NULL },
  { GDK_ACTION_ASK,     "dnd-ask", NULL },
  { GDK_ACTION_COPY,    "copy", NULL },
  { GDK_ACTION_MOVE,    "dnd-move", NULL },
  { GDK_ACTION_LINK,    "dnd-link", NULL },
  { 0,                  "default", NULL },
};

enum {
  CANCEL,
  DROP_PERFORMED,
  DND_FINISHED,
  ACTION_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };
static GList *contexts = NULL;

/**
 * SECTION:dnd
 * @title: Drag And Drop
 * @short_description: Functions for controlling drag and drop handling
 *
 * These functions provide a low level interface for drag and drop.
 * The X backend of GDK supports both the Xdnd and Motif drag and drop
 * protocols transparently, the Win32 backend supports the WM_DROPFILES
 * protocol.
 *
 * GTK+ provides a higher level abstraction based on top of these functions,
 * and so they are not normally needed in GTK+ applications.
 * See the [Drag and Drop][gtk3-Drag-and-Drop] section of
 * the GTK+ documentation for more information.
 */

/**
 * gdk_drag_context_list_targets:
 * @context: a #GdkDragContext
 *
 * Retrieves the list of targets of the context.
 *
 * Returns: (transfer none) (element-type GdkAtom): a #GList of targets
 *
 * Since: 2.22
 **/
GList *
gdk_drag_context_list_targets (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->targets;
}

/**
 * gdk_drag_context_get_actions:
 * @context: a #GdkDragContext
 *
 * Determines the bitmask of actions proposed by the source if
 * gdk_drag_context_get_suggested_action() returns %GDK_ACTION_ASK.
 *
 * Returns: the #GdkDragAction flags
 *
 * Since: 2.22
 **/
GdkDragAction
gdk_drag_context_get_actions (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_ACTION_DEFAULT);

  return context->actions;
}

/**
 * gdk_drag_context_get_suggested_action:
 * @context: a #GdkDragContext
 *
 * Determines the suggested drag action of the context.
 *
 * Returns: a #GdkDragAction value
 *
 * Since: 2.22
 **/
GdkDragAction
gdk_drag_context_get_suggested_action (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), 0);

  return context->suggested_action;
}

/**
 * gdk_drag_context_get_selected_action:
 * @context: a #GdkDragContext
 *
 * Determines the action chosen by the drag destination.
 *
 * Returns: a #GdkDragAction value
 *
 * Since: 2.22
 **/
GdkDragAction
gdk_drag_context_get_selected_action (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), 0);

  return context->action;
}

/**
 * gdk_drag_context_get_source_window:
 * @context: a #GdkDragContext
 *
 * Returns the #GdkWindow where the DND operation started.
 *
 * Returns: (transfer none): a #GdkWindow
 *
 * Since: 2.22
 **/
GdkWindow *
gdk_drag_context_get_source_window (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->source_window;
}

/**
 * gdk_drag_context_get_dest_window:
 * @context: a #GdkDragContext
 *
 * Returns the destination window for the DND operation.
 *
 * Returns: (transfer none): a #GdkWindow
 *
 * Since: 3.0
 **/
GdkWindow *
gdk_drag_context_get_dest_window (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->dest_window;
}

/**
 * gdk_drag_context_get_protocol:
 * @context: a #GdkDragContext
 *
 * Returns the drag protocol that is used by this context.
 *
 * Returns: the drag protocol
 *
 * Since: 3.0
 */
GdkDragProtocol
gdk_drag_context_get_protocol (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_DRAG_PROTO_NONE);

  return context->protocol;
}

/**
 * gdk_drag_context_set_device:
 * @context: a #GdkDragContext
 * @device: a #GdkDevice
 *
 * Associates a #GdkDevice to @context, so all Drag and Drop events
 * for @context are emitted as if they came from this device.
 */
void
gdk_drag_context_set_device (GdkDragContext *context,
                             GdkDevice      *device)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (context->device)
    g_object_unref (context->device);

  context->device = device;

  if (context->device)
    g_object_ref (context->device);
}

/**
 * gdk_drag_context_get_device:
 * @context: a #GdkDragContext
 *
 * Returns the #GdkDevice associated to the drag context.
 *
 * Returns: (transfer none): The #GdkDevice associated to @context.
 **/
GdkDevice *
gdk_drag_context_get_device (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->device;
}

G_DEFINE_TYPE (GdkDragContext, gdk_drag_context, G_TYPE_OBJECT)

static void
gdk_drag_context_init (GdkDragContext *context)
{
  contexts = g_list_prepend (contexts, context);
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  contexts = g_list_remove (contexts, context);
  g_list_free (context->targets);

  if (context->source_window)
    g_object_unref (context->source_window);

  if (context->dest_window)
    g_object_unref (context->dest_window);

  G_OBJECT_CLASS (gdk_drag_context_parent_class)->finalize (object);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_drag_context_finalize;

  /**
   * GdkDragContext::cancel:
   * @context: The object on which the signal is emitted
   * @reason: The reason the context was cancelled
   *
   * The drag and drop operation was cancelled.
   *
   * This signal will only be emitted if the #GdkDragContext manages
   * the drag and drop operation. See gdk_drag_context_manage_dnd()
   * for more information.
   *
   * Since: 3.20
   */
  signals[CANCEL] =
    g_signal_new ("cancel",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragContextClass, cancel),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_DRAG_CANCEL_REASON);

  /**
   * GdkDragContext::drop-performed:
   * @context: The object on which the signal is emitted
   * @time: the time at which the drop happened.
   *
   * The drag and drop operation was performed on an accepting client.
   *
   * This signal will only be emitted if the #GdkDragContext manages
   * the drag and drop operation. See gdk_drag_context_manage_dnd()
   * for more information.
   *
   * Since: 3.20
   */
  signals[DROP_PERFORMED] =
    g_signal_new ("drop-performed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragContextClass, drop_performed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  /**
   * GdkDragContext::dnd-finished:
   * @context: The object on which the signal is emitted
   *
   * The drag and drop operation was finished, the drag destination
   * finished reading all data. The drag source can now free all
   * miscellaneous data.
   *
   * This signal will only be emitted if the #GdkDragContext manages
   * the drag and drop operation. See gdk_drag_context_manage_dnd()
   * for more information.
   *
   * Since: 3.20
   */
  signals[DND_FINISHED] =
    g_signal_new ("dnd-finished",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragContextClass, dnd_finished),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkDragContext::action-changed:
   * @context: The object on which the signal is emitted
   * @action: The action currently chosen
   *
   * A new action is being chosen for the drag and drop operation.
   *
   * This signal will only be emitted if the #GdkDragContext manages
   * the drag and drop operation. See gdk_drag_context_manage_dnd()
   * for more information.
   *
   * Since: 3.20
   */
  signals[ACTION_CHANGED] =
    g_signal_new ("action-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragContextClass, action_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_DRAG_ACTION);
}

/**
 * gdk_drag_find_window_for_screen:
 * @context: a #GdkDragContext
 * @drag_window: a window which may be at the pointer position, but
 *     should be ignored, since it is put up by the drag source as an icon
 * @screen: the screen where the destination window is sought
 * @x_root: the x position of the pointer in root coordinates
 * @y_root: the y position of the pointer in root coordinates
 * @dest_window: (out): location to store the destination window in
 * @protocol: (out): location to store the DND protocol in
 *
 * Finds the destination window and DND protocol to use at the
 * given pointer position.
 *
 * This function is called by the drag source to obtain the
 * @dest_window and @protocol parameters for gdk_drag_motion().
 *
 * Since: 2.2
 */
void
gdk_drag_find_window_for_screen (GdkDragContext  *context,
                                 GdkWindow       *drag_window,
                                 GdkScreen       *screen,
                                 gint             x_root,
                                 gint             y_root,
                                 GdkWindow      **dest_window,
                                 GdkDragProtocol *protocol)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  *dest_window = GDK_DRAG_CONTEXT_GET_CLASS (context)
      ->find_window (context, drag_window, screen, x_root, y_root, protocol);
}

/**
 * gdk_drag_status:
 * @context: a #GdkDragContext
 * @action: the selected action which will be taken when a drop happens,
 *    or 0 to indicate that a drop will not be accepted
 * @time_: the timestamp for this operation
 *
 * Selects one of the actions offered by the drag source.
 *
 * This function is called by the drag destination in response to
 * gdk_drag_motion() called by the drag source.
 */
void
gdk_drag_status (GdkDragContext *context,
                 GdkDragAction   action,
                 guint32         time_)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  GDK_DRAG_CONTEXT_GET_CLASS (context)->drag_status (context, action, time_);
}

/**
 * gdk_drag_motion:
 * @context: a #GdkDragContext
 * @dest_window: the new destination window, obtained by
 *     gdk_drag_find_window()
 * @protocol: the DND protocol in use, obtained by gdk_drag_find_window()
 * @x_root: the x position of the pointer in root coordinates
 * @y_root: the y position of the pointer in root coordinates
 * @suggested_action: the suggested action
 * @possible_actions: the possible actions
 * @time_: the timestamp for this operation
 *
 * Updates the drag context when the pointer moves or the
 * set of actions changes.
 *
 * This function is called by the drag source.
 *
 * This function does not need to be called in managed drag and drop
 * operations. See gdk_drag_context_manage_dnd() for more information.
 *
 * Returns:
 */
gboolean
gdk_drag_motion (GdkDragContext *context,
                 GdkWindow      *dest_window,
                 GdkDragProtocol protocol,
                 gint            x_root,
                 gint            y_root,
                 GdkDragAction   suggested_action,
                 GdkDragAction   possible_actions,
                 guint32         time_)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);

  return GDK_DRAG_CONTEXT_GET_CLASS (context)
       ->drag_motion (context,
                      dest_window,
                      protocol,
                      x_root,
                      y_root,
                      suggested_action,
                      possible_actions,
                      time_);
}

/**
 * gdk_drag_abort:
 * @context: a #GdkDragContext
 * @time_: the timestamp for this operation
 *
 * Aborts a drag without dropping.
 *
 * This function is called by the drag source.
 *
 * This function does not need to be called in managed drag and drop
 * operations. See gdk_drag_context_manage_dnd() for more information.
 */
void
gdk_drag_abort (GdkDragContext *context,
                guint32         time_)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  GDK_DRAG_CONTEXT_GET_CLASS (context)->drag_abort (context, time_);
}

/**
 * gdk_drag_drop:
 * @context: a #GdkDragContext
 * @time_: the timestamp for this operation
 *
 * Drops on the current destination.
 *
 * This function is called by the drag source.
 *
 * This function does not need to be called in managed drag and drop
 * operations. See gdk_drag_context_manage_dnd() for more information.
 */
void
gdk_drag_drop (GdkDragContext *context,
               guint32         time_)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  GDK_DRAG_CONTEXT_GET_CLASS (context)->drag_drop (context, time_);
}

/**
 * gdk_drop_reply:
 * @context: a #GdkDragContext
 * @accepted: %TRUE if the drop is accepted
 * @time_: the timestamp for this operation
 *
 * Accepts or rejects a drop.
 *
 * This function is called by the drag destination in response
 * to a drop initiated by the drag source.
 */
void
gdk_drop_reply (GdkDragContext *context,
                gboolean        accepted,
                guint32         time_)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  GDK_DRAG_CONTEXT_GET_CLASS (context)->drop_reply (context, accepted, time_);
}

/**
 * gdk_drop_finish:
 * @context: a #GdkDragContext
 * @success: %TRUE if the data was successfully received
 * @time_: the timestamp for this operation
 *
 * Ends the drag operation after a drop.
 *
 * This function is called by the drag destination.
 */
void
gdk_drop_finish (GdkDragContext *context,
                 gboolean        success,
                 guint32         time_)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  GDK_DRAG_CONTEXT_GET_CLASS (context)->drop_finish (context, success, time_);
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
 * Returns: %TRUE if the drop was successful.
 *
 * Since: 2.6
 **/
gboolean
gdk_drag_drop_succeeded (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);

  return GDK_DRAG_CONTEXT_GET_CLASS (context)->drop_status (context);
}

/**
 * gdk_drag_get_selection:
 * @context: a #GdkDragContext.
 *
 * Returns the selection atom for the current source window.
 *
 * Returns: (transfer none): the selection atom, or %GDK_NONE
 */
GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_NONE);

  return GDK_DRAG_CONTEXT_GET_CLASS (context)->get_selection (context);
}

/**
 * gdk_drag_context_get_drag_window:
 * @context: a #GdkDragContext
 *
 * Returns the window on which the drag icon should be rendered
 * during the drag operation. Note that the window may not be
 * available until the drag operation has begun. GDK will move
 * the window in accordance with the ongoing drag operation.
 * The window is owned by @context and will be destroyed when
 * the drag operation is over.
 *
 * Returns: (nullable) (transfer none): the drag window, or %NULL
 *
 * Since: 3.20
 */
GdkWindow *
gdk_drag_context_get_drag_window (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  if (GDK_DRAG_CONTEXT_GET_CLASS (context)->get_drag_window)
    return GDK_DRAG_CONTEXT_GET_CLASS (context)->get_drag_window (context);

  return NULL;
}

/**
 * gdk_drag_context_set_hotspot:
 * @context: a #GdkDragContext
 * @hot_x: x coordinate of the drag window hotspot
 * @hot_y: y coordinate of the drag window hotspot
 *
 * Sets the position of the drag window that will be kept
 * under the cursor hotspot. Initially, the hotspot is at the
 * top left corner of the drag window.
 *
 * Since: 3.20
 */
void
gdk_drag_context_set_hotspot (GdkDragContext *context,
                              gint            hot_x,
                              gint            hot_y)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  if (GDK_DRAG_CONTEXT_GET_CLASS (context)->set_hotspot)
    GDK_DRAG_CONTEXT_GET_CLASS (context)->set_hotspot (context, hot_x, hot_y);
}

/**
 * gdk_drag_drop_done:
 * @context: a #GdkDragContext
 * @success: whether the drag was ultimatively successful
 *
 * Inform GDK if the drop ended successfully. Passing %FALSE
 * for @success may trigger a drag cancellation animation.
 *
 * This function is called by the drag source, and should
 * be the last call before dropping the reference to the
 * @context.
 *
 * The #GdkDragContext will only take the first gdk_drag_drop_done()
 * call as effective, if this function is called multiple times,
 * all subsequent calls will be ignored.
 *
 * Since: 3.20
 */
void
gdk_drag_drop_done (GdkDragContext *context,
                    gboolean        success)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  if (context->drop_done)
    return;

  context->drop_done = TRUE;

  if (GDK_DRAG_CONTEXT_GET_CLASS (context)->drop_done)
    GDK_DRAG_CONTEXT_GET_CLASS (context)->drop_done (context, success);
}

/**
 * gdk_drag_context_manage_dnd:
 * @context: a #GdkDragContext
 * @ipc_window: Window to use for IPC messaging/events
 * @actions: the actions supported by the drag source
 *
 * Requests the drag and drop operation to be managed by @context.
 * When a drag and drop operation becomes managed, the #GdkDragContext
 * will internally handle all input and source-side #GdkEventDND events
 * as required by the windowing system.
 *
 * Once the drag and drop operation is managed, the drag context will
 * emit the following signals:
 * - The #GdkDragContext::action-changed signal whenever the final action
 *   to be performed by the drag and drop operation changes.
 * - The #GdkDragContext::drop-performed signal after the user performs
 *   the drag and drop gesture (typically by releasing the mouse button).
 * - The #GdkDragContext::dnd-finished signal after the drag and drop
 *   operation concludes (after all #GdkSelection transfers happen).
 * - The #GdkDragContext::cancel signal if the drag and drop operation is
 *   finished but doesn't happen over an accepting destination, or is
 *   cancelled through other means.
 *
 * Returns: #TRUE if the drag and drop operation is managed.
 *
 * Since: 3.20
 **/
gboolean
gdk_drag_context_manage_dnd (GdkDragContext *context,
                             GdkWindow      *ipc_window,
                             GdkDragAction   actions)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (ipc_window), FALSE);

  if (GDK_DRAG_CONTEXT_GET_CLASS (context)->manage_dnd)
    return GDK_DRAG_CONTEXT_GET_CLASS (context)->manage_dnd (context, ipc_window, actions);

  return FALSE;
}

void
gdk_drag_context_set_cursor (GdkDragContext *context,
                             GdkCursor      *cursor)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  if (GDK_DRAG_CONTEXT_GET_CLASS (context)->set_cursor)
    GDK_DRAG_CONTEXT_GET_CLASS (context)->set_cursor (context, cursor);
}

void
gdk_drag_context_cancel (GdkDragContext      *context,
                         GdkDragCancelReason  reason)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_signal_emit (context, signals[CANCEL], 0, reason);
}

GList *
gdk_drag_context_list (void)
{
  return contexts;
}

gboolean
gdk_drag_context_handle_source_event (GdkEvent *event)
{
  GdkDragContext *context;
  GList *l;

  for (l = contexts; l; l = l->next)
    {
      context = l->data;

      if (!context->is_source)
        continue;

      if (!GDK_DRAG_CONTEXT_GET_CLASS (context)->handle_event)
        continue;

      if (GDK_DRAG_CONTEXT_GET_CLASS (context)->handle_event (context, event))
        return TRUE;
    }

  return FALSE;
}

GdkCursor *
gdk_drag_get_cursor (GdkDragContext *context,
                     GdkDragAction   action)
{
  gint i;

  for (i = 0 ; i < G_N_ELEMENTS (drag_cursors) - 1; i++)
    if (drag_cursors[i].action == action)
      break;

  if (drag_cursors[i].cursor == NULL)
    drag_cursors[i].cursor = gdk_cursor_new_from_name (context->display,
                                                       drag_cursors[i].name);
  return drag_cursors[i].cursor;
}

static void
gdk_drag_context_commit_drag_status (GdkDragContext *context)
{
  GdkDragContextClass *context_class;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (!context->is_source);

  context_class = GDK_DRAG_CONTEXT_GET_CLASS (context);

  if (context_class->commit_drag_status)
    context_class->commit_drag_status (context);
}

gboolean
gdk_drag_context_handle_dest_event (GdkEvent *event)
{
  GdkDragContext *context = NULL;
  GList *l;

  switch (event->type)
    {
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      context = event->dnd.context;
      break;
    case GDK_SELECTION_NOTIFY:
      for (l = contexts; l; l = l->next)
        {
          GdkDragContext *c = l->data;

          if (!c->is_source &&
              event->selection.selection == gdk_drag_get_selection (c))
            {
              context = c;
              break;
            }
        }
      break;
    default:
      return FALSE;
    }

  if (!context)
    return FALSE;

  gdk_drag_context_commit_drag_status (context);
  return TRUE;;
}
