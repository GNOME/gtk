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

#include "gdkdragprivate.h"
#include "gdkdisplay.h"
#include "gdksurface.h"
#include "gdkintl.h"
#include "gdkcontentformats.h"
#include "gdkcontentprovider.h"
#include "gdkcontentserializer.h"
#include "gdkcursor.h"
#include "gdkenumtypes.h"
#include "gdkeventsprivate.h"

static struct {
  GdkDragAction action;
  const gchar  *name;
  GdkCursor    *cursor;
} drag_cursors[] = {
  { GDK_ACTION_ASK,     "dnd-ask",  NULL },
  { GDK_ACTION_COPY,    "dnd-copy", NULL },
  { GDK_ACTION_MOVE,    "dnd-move", NULL },
  { GDK_ACTION_LINK,    "dnd-link", NULL },
  { 0,                  "dnd-none", NULL },
};

enum {
  PROP_0,
  PROP_CONTENT,
  PROP_DEVICE,
  PROP_DISPLAY,
  PROP_FORMATS,
  PROP_SELECTED_ACTION,
  PROP_ACTIONS,
  N_PROPERTIES
};

enum {
  CANCEL,
  DROP_PERFORMED,
  DND_FINISHED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };
static guint signals[N_SIGNALS] = { 0 };
static GList *drags = NULL;

G_DEFINE_ABSTRACT_TYPE (GdkDrag, gdk_drag, G_TYPE_OBJECT)

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
 * GdkDrag:
 *
 * The GdkDrag struct contains only private fields and
 * should not be accessed directly.
 */

/**
 * gdk_drag_get_display:
 * @drag: a #GdkDrag
 *
 * Gets the #GdkDisplay that the drag object was created for.
 *
 * Returns: (transfer none): a #GdkDisplay
 **/
GdkDisplay *
gdk_drag_get_display (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  return drag->display;
}

/**
 * gdk_drag_get_formats:
 * @drag: a #GdkDrag
 *
 * Retrieves the formats supported by this GdkDrag object.
 *
 * Returns: (transfer none): a #GdkContentFormats
 **/
GdkContentFormats *
gdk_drag_get_formats (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  return drag->formats;
}

/**
 * gdk_drag_get_actions:
 * @drag: a #GdkDrag
 *
 * Determines the bitmask of actions proposed by the source if
 * gdk_drag_get_suggested_action() returns %GDK_ACTION_ASK.
 *
 * Returns: the #GdkDragAction flags
 **/
GdkDragAction
gdk_drag_get_actions (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), 0);

  return drag->actions;
}

/**
 * gdk_drag_get_suggested_action:
 * @drag: a #GdkDrag
 *
 * Determines the suggested drag action of the GdkDrag object.
 *
 * Returns: a #GdkDragAction value
 **/
GdkDragAction
gdk_drag_get_suggested_action (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), 0);

  return drag->suggested_action;
}

/**
 * gdk_drag_get_selected_action:
 * @drag: a #GdkDrag
 *
 * Determines the action chosen by the drag destination.
 *
 * Returns: a #GdkDragAction value
 **/
GdkDragAction
gdk_drag_get_selected_action (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), 0);

  return drag->selected_action;
}

/**
 * gdk_drag_get_device:
 * @drag: a #GdkDrag
 *
 * Returns the #GdkDevice associated to the GdkDrag object.
 *
 * Returns: (transfer none): The #GdkDevice associated to @drag.
 **/
GdkDevice *
gdk_drag_get_device (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  return drag->device;
}

static void
gdk_drag_init (GdkDrag *drag)
{
  drags = g_list_prepend (drags, drag);
}

static void
gdk_drag_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GdkDrag *drag = GDK_DRAG (gobject);

  switch (prop_id)
    {
    case PROP_CONTENT:
      drag->content = g_value_dup_object (value);
      if (drag->content)
        {
          g_assert (drag->formats == NULL);
          drag->formats = gdk_content_provider_ref_formats (drag->content);
        }
      break;

    case PROP_DEVICE:
      drag->device = g_value_dup_object (value);
      g_assert (drag->device != NULL);
      drag->display = gdk_device_get_display (drag->device);
      break;

    case PROP_FORMATS:
      if (drag->formats)
        {
          GdkContentFormats *override = g_value_dup_boxed (value);
          if (override)
            {
              gdk_content_formats_unref (drag->formats);
              drag->formats = override;
            }
        }
      else
        {
          drag->formats = g_value_dup_boxed (value);
          g_assert (drag->formats != NULL);
        }
      break;

    case PROP_SELECTED_ACTION:
      {
        GdkDragAction action = g_value_get_flags (value);
        gdk_drag_set_selected_action (drag, action);
      }
    break;

    case PROP_ACTIONS:
      {
        GdkDragAction actions = g_value_get_flags (value);
        gdk_drag_set_actions (drag, actions, drag->suggested_action);
      }
    break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_drag_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GdkDrag *drag = GDK_DRAG (gobject);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_object (value, drag->content);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, drag->device);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, drag->display);
      break;

    case PROP_FORMATS:
      g_value_set_boxed (value, drag->formats);
      break;

    case PROP_SELECTED_ACTION:
      g_value_set_flags (value, drag->selected_action);
      break;

    case PROP_ACTIONS:
      g_value_set_flags (value, drag->actions);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_drag_finalize (GObject *object)
{
  GdkDrag *drag = GDK_DRAG (object);

  drags = g_list_remove (drags, drag);

  g_clear_object (&drag->content);
  g_clear_pointer (&drag->formats, gdk_content_formats_unref);

  if (drag->source_surface)
    g_object_unref (drag->source_surface);

  G_OBJECT_CLASS (gdk_drag_parent_class)->finalize (object);
}

static void
gdk_drag_class_init (GdkDragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gdk_drag_get_property;
  object_class->set_property = gdk_drag_set_property;
  object_class->finalize = gdk_drag_finalize;

  /**
   * GdkDrag:content:
   *
   * The #GdkContentProvider.
   */
  properties[PROP_CONTENT] =
    g_param_spec_object ("content",
                         "Content",
                         "The content being dragged",
                         GDK_TYPE_CONTENT_PROVIDER,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrag:device:
   *
   * The #GdkDevice that is performing the drag.
   */
  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The device performing the drag",
                         GDK_TYPE_DEVICE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrag:display:
   *
   * The #GdkDisplay that the drag belongs to.
   */
  properties[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "Display this drag belongs to",
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrag:formats:
   *
   * The possible formats that the drag can provide its data in.
   */
  properties[PROP_FORMATS] =
    g_param_spec_boxed ("formats",
                        "Formats",
                        "The possible formats for data",
                        GDK_TYPE_CONTENT_FORMATS,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SELECTED_ACTION] =
    g_param_spec_flags ("selected-action",
                        "Selected action",
                        "The currently selected action",
                        GDK_TYPE_DRAG_ACTION,
                        0,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ACTIONS] =
    g_param_spec_flags ("actions",
                        "Actions",
                        "The possible actions",
                        GDK_TYPE_DRAG_ACTION,
                        0,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);
  /**
   * GdkDrag::cancel:
   * @drag: The object on which the signal is emitted
   * @reason: The reason the drag was cancelled
   *
   * The drag operation was cancelled.
   */
  signals[CANCEL] =
    g_signal_new (g_intern_static_string ("cancel"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragClass, cancel),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1, GDK_TYPE_DRAG_CANCEL_REASON);

  /**
   * GdkDrag::drop-performed:
   * @drag: The object on which the signal is emitted
   *
   * The drag operation was performed on an accepting client.
   */
  signals[DROP_PERFORMED] =
    g_signal_new (g_intern_static_string ("drop-performed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragClass, drop_performed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkDrag::dnd-finished:
   * @drag: The object on which the signal is emitted
   *
   * The drag operation was finished, the destination
   * finished reading all data. The drag object can now
   * free all miscellaneous data.
   */
  signals[DND_FINISHED] =
    g_signal_new (g_intern_static_string ("dnd-finished"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDragClass, dnd_finished),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/*
 * gdk_drag_abort:
 * @drag: a #GdkDrag
 * @time_: the timestamp for this operation
 *
 * Aborts a drag without dropping.
 *
 * This function is called by the drag source.
 */
void
gdk_drag_abort (GdkDrag *drag,
                guint32  time_)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  GDK_DRAG_GET_CLASS (drag)->drag_abort (drag, time_);
}

/*
 * gdk_drag_drop:
 * @drag: a #GdkDrag
 * @time_: the timestamp for this operation
 *
 * Drops on the current destination.
 *
 * This function is called by the drag source.
 */
void
gdk_drag_drop (GdkDrag *drag,
               guint32  time_)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  GDK_DRAG_GET_CLASS (drag)->drag_drop (drag, time_);
}

static void
gdk_drag_write_done (GObject      *content,
                     GAsyncResult *result,
                     gpointer      task)
{
  GError *error = NULL;

  if (gdk_content_provider_write_mime_type_finish (GDK_CONTENT_PROVIDER (content), result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_object_unref (task);
}

static void
gdk_drag_write_serialize_done (GObject      *content,
                               GAsyncResult *result,
                               gpointer      task)
{
  GError *error = NULL;

  if (gdk_content_serialize_finish (result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_object_unref (task);
}

void
gdk_drag_write_async (GdkDrag             *drag,
                      const char          *mime_type,
                      GOutputStream       *stream,
                      int                  io_priority,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GdkContentFormats *formats, *mime_formats;
  GTask *task;
  GType gtype;

  g_return_if_fail (GDK_IS_DRAG (drag));
  g_return_if_fail (drag->content);
  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (mime_type == g_intern_string (mime_type));
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  task = g_task_new (drag, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_drag_write_async);

  formats = gdk_content_provider_ref_formats (drag->content);
  if (gdk_content_formats_contain_mime_type (formats, mime_type))
    {
      gdk_content_provider_write_mime_type_async (drag->content,
                                                  mime_type,
                                                  stream,
                                                  io_priority,
                                                  cancellable,
                                                  gdk_drag_write_done,
                                                  task);
      gdk_content_formats_unref (formats);
      return;
    }

  mime_formats = gdk_content_formats_new ((const gchar *[2]) { mime_type, NULL }, 1);
  mime_formats = gdk_content_formats_union_serialize_gtypes (mime_formats);
  gtype = gdk_content_formats_match_gtype (formats, mime_formats);
  if (gtype != G_TYPE_INVALID)
    {
      GValue value = G_VALUE_INIT;
      GError *error = NULL;

      g_assert (gtype != G_TYPE_INVALID);
      
      g_value_init (&value, gtype);
      if (gdk_content_provider_get_value (drag->content, &value, &error))
        {
          gdk_content_serialize_async (stream,
                                       mime_type,
                                       &value,
                                       io_priority,
                                       cancellable,
                                       gdk_drag_write_serialize_done,
                                       g_object_ref (task));
        }
      else
        {
          g_task_return_error (task, error);
        }
      
      g_value_unset (&value);
    }
  else
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible formats to transfer clipboard contents."));
    }

  gdk_content_formats_unref (mime_formats);
  gdk_content_formats_unref (formats);
  g_object_unref (task);
}

gboolean
gdk_drag_write_finish (GdkDrag       *drag,
                       GAsyncResult  *result,
                       GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, drag), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_drag_write_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error); 
}

void
gdk_drag_set_actions (GdkDrag       *drag,
                      GdkDragAction  actions,
                      GdkDragAction  suggested_action)
{
  drag->suggested_action = suggested_action;

  if (drag->actions == actions)
    return;

  drag->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (drag), properties[PROP_ACTIONS]);
}

void
gdk_drag_set_selected_action (GdkDrag       *drag,
                              GdkDragAction  action)
{
  GdkCursor *cursor;

  if (drag->selected_action == action)
    return;

  drag->selected_action = action;

  cursor = gdk_drag_get_cursor (drag, action);
  gdk_drag_set_cursor (drag, cursor);

  g_object_notify_by_pspec (G_OBJECT (drag), properties[PROP_SELECTED_ACTION]);
}

/**
 * gdk_drag_get_drag_surface:
 * @drag: a #GdkDrag
 *
 * Returns the surface on which the drag icon should be rendered
 * during the drag operation. Note that the surface may not be
 * available until the drag operation has begun. GDK will move
 * the surface in accordance with the ongoing drag operation.
 * The surface is owned by @drag and will be destroyed when
 * the drag operation is over.
 *
 * Returns: (nullable) (transfer none): the drag surface, or %NULL
 */
GdkSurface *
gdk_drag_get_drag_surface (GdkDrag *drag)
{
  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  if (GDK_DRAG_GET_CLASS (drag)->get_drag_surface)
    return GDK_DRAG_GET_CLASS (drag)->get_drag_surface (drag);

  return NULL;
}

/**
 * gdk_drag_set_hotspot:
 * @drag: a #GdkDrag
 * @hot_x: x coordinate of the drag surface hotspot
 * @hot_y: y coordinate of the drag surface hotspot
 *
 * Sets the position of the drag surface that will be kept
 * under the cursor hotspot. Initially, the hotspot is at the
 * top left corner of the drag surface.
 */
void
gdk_drag_set_hotspot (GdkDrag *drag,
                      gint     hot_x,
                      gint     hot_y)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  if (GDK_DRAG_GET_CLASS (drag)->set_hotspot)
    GDK_DRAG_GET_CLASS (drag)->set_hotspot (drag, hot_x, hot_y);
}

/**
 * gdk_drag_drop_done:
 * @drag: a #GdkDrag
 * @success: whether the drag was ultimatively successful
 *
 * Inform GDK if the drop ended successfully. Passing %FALSE
 * for @success may trigger a drag cancellation animation.
 *
 * This function is called by the drag source, and should
 * be the last call before dropping the reference to the
 * @drag.
 *
 * The #GdkDrag will only take the first gdk_drag_drop_done()
 * call as effective, if this function is called multiple times,
 * all subsequent calls will be ignored.
 */
void
gdk_drag_drop_done (GdkDrag  *drag,
                    gboolean  success)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  if (drag->drop_done)
    return;

  drag->drop_done = TRUE;

  if (GDK_DRAG_GET_CLASS (drag)->drop_done)
    GDK_DRAG_GET_CLASS (drag)->drop_done (drag, success);
}

void
gdk_drag_set_cursor (GdkDrag   *drag,
                     GdkCursor *cursor)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  if (GDK_DRAG_GET_CLASS (drag)->set_cursor)
    GDK_DRAG_GET_CLASS (drag)->set_cursor (drag, cursor);
}

void
gdk_drag_cancel (GdkDrag             *drag,
                 GdkDragCancelReason  reason)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  g_signal_emit (drag, signals[CANCEL], 0, reason);
}

gboolean
gdk_drag_handle_source_event (GdkEvent *event)
{
  GdkDrag *drag;
  GList *l;

  for (l = drags; l; l = l->next)
    {
      drag = l->data;

      if (!GDK_DRAG_GET_CLASS (drag)->handle_event)
        continue;

      if (GDK_DRAG_GET_CLASS (drag)->handle_event (drag, event))
        return TRUE;
    }

  return FALSE;
}

GdkCursor *
gdk_drag_get_cursor (GdkDrag       *drag,
                     GdkDragAction  action)
{
  gint i;

  for (i = 0 ; i < G_N_ELEMENTS (drag_cursors) - 1; i++)
    if (drag_cursors[i].action == action)
      break;

  if (drag_cursors[i].cursor == NULL)
    drag_cursors[i].cursor = gdk_cursor_new_from_name (drag_cursors[i].name, NULL);
                                                       
  return drag_cursors[i].cursor;
}

/**
 * gdk_drag_action_is_unique:
 * @action: a #GdkDragAction
 *
 * Checks if @action represents a single action or if it
 * includes multiple flags that can be selected from.
 *
 * When @action is 0 - ie no action was given, %TRUE
 * is returned.
 *
 * Returns: %TRUE if exactly one action was given
 **/
gboolean
gdk_drag_action_is_unique (GdkDragAction action)
{
  return (action & (action - 1)) == 0;
}
