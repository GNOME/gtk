/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkdropprivate.h"

#include "gdkcontentdeserializer.h"
#include "gdkcontentformats.h"
#include "gdkcontentprovider.h"
#include "gdkcontentserializer.h"
#include "gdkcursor.h"
#include "gdkdisplay.h"
#include "gdkenumtypes.h"
#include "gdkeventsprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkpipeiostreamprivate.h"
#include "gdksurface.h"

typedef struct _GdkDropPrivate GdkDropPrivate;

struct _GdkDropPrivate {
  GdkDevice *device;
  GdkDrag *drag;
  GdkContentFormats *formats;
  GdkSurface *surface;
  GdkDragAction actions;
};

enum {
  PROP_0,
  PROP_ACTIONS,
  PROP_DEVICE,
  PROP_DISPLAY,
  PROP_DRAG,
  PROP_FORMATS,
  PROP_SURFACE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkDrop, gdk_drop, G_TYPE_OBJECT)

/**
 * GdkDrop:
 *
 * The GdkDrop struct contains only private fields and
 * should not be accessed directly.
 */

static void
gdk_drop_default_status (GdkDrop       *self,
                         GdkDragAction  actions)
{
}

static void
gdk_drop_read_local_write_done (GObject      *drag,
                                GAsyncResult *result,
                                gpointer      stream)
{
  /* we don't care about the error, we just want to clean up */
  gdk_drag_write_finish (GDK_DRAG (drag), result, NULL);

  /* XXX: Do we need to close_async() here? */
  g_output_stream_close (stream, NULL, NULL);

  g_object_unref (stream);
}

static void
gdk_drop_read_local_async (GdkDrop             *self,
                           GdkContentFormats   *formats,
                           int                  io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);
  GdkContentFormats *content_formats;
  const char *mime_type;
  GTask *task;
  GdkContentProvider *content;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_drop_read_local_async);

  if (priv->drag == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("Drag’n’drop from other applications is not supported."));
      g_object_unref (task);
      return;
    }

  g_object_get (priv->drag, "content", &content, NULL);
  content_formats = gdk_content_provider_ref_formats (content);
  g_object_unref (content); 
  content_formats = gdk_content_formats_union_serialize_mime_types (content_formats);
  mime_type = gdk_content_formats_match_mime_type (content_formats, formats);

  if (mime_type != NULL)
    {
      GOutputStream *output_stream;
      GIOStream *stream;

      stream = gdk_pipe_io_stream_new ();
      output_stream = g_io_stream_get_output_stream (stream);
      gdk_drag_write_async (priv->drag,
                                    mime_type,
                                    output_stream,
                                    io_priority,
                                    cancellable,
                                    gdk_drop_read_local_write_done,
                                    g_object_ref (output_stream));
      g_task_set_task_data (task, (gpointer) mime_type, NULL);
      g_task_return_pointer (task, g_object_ref (g_io_stream_get_input_stream (stream)), g_object_unref);

      g_object_unref (stream);
    }
  else
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("No compatible formats to transfer contents."));
    }

  gdk_content_formats_unref (content_formats);
  g_object_unref (task);
}

static GInputStream *
gdk_drop_read_local_finish (GdkDrop         *self,
                            GAsyncResult    *result,
                            const char     **out_mime_type,
                            GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_drop_read_local_async, NULL);

  if (out_mime_type)
    *out_mime_type = g_task_get_task_data (G_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gdk_drop_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GdkDrop *self = GDK_DROP (gobject);
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTIONS:
      gdk_drop_set_actions (self, g_value_get_flags (value));
      break;

    case PROP_DEVICE:
      priv->device = g_value_dup_object (value);
      g_assert (priv->device != NULL);
      if (priv->surface)
        g_assert (gdk_surface_get_display (priv->surface) == gdk_device_get_display (priv->device));
      break;

    case PROP_DRAG:
      priv->drag = g_value_dup_object (value);
      break;

    case PROP_FORMATS:
      priv->formats = g_value_dup_boxed (value);
      g_assert (priv->formats != NULL);
      break;

    case PROP_SURFACE:
      priv->surface = g_value_dup_object (value);
      g_assert (priv->surface != NULL);
      if (priv->device)
        g_assert (gdk_surface_get_display (priv->surface) == gdk_device_get_display (priv->device));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_drop_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GdkDrop *self = GDK_DROP (gobject);
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTIONS:
      g_value_set_flags (value, priv->actions);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, gdk_device_get_display (priv->device));
      break;

    case PROP_DRAG:
      g_value_set_object (value, priv->drag);
      break;

    case PROP_FORMATS:
      g_value_set_boxed (value, priv->formats);
      break;

    case PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_drop_finalize (GObject *object)
{
  GdkDrop *self = GDK_DROP (object);
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_clear_object (&priv->device);
  g_clear_object (&priv->drag);

  G_OBJECT_CLASS (gdk_drop_parent_class)->finalize (object);
}

static void
gdk_drop_class_init (GdkDropClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->status = gdk_drop_default_status;

  object_class->get_property = gdk_drop_get_property;
  object_class->set_property = gdk_drop_set_property;
  object_class->finalize = gdk_drop_finalize;

  /**
   * GdkDrop:actions:
   *
   * The possible actions for this drop
   */
  properties[PROP_ACTIONS] =
    g_param_spec_flags ("actions",
                        "Actions",
                        "The possible actions for this drop",
                         GDK_TYPE_DRAG_ACTION,
                         GDK_ACTION_ALL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrop:device:
   *
   * The #GdkDevice performing the drop
   */
  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The device performing the drop",
                         GDK_TYPE_DEVICE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrop:display:
   *
   * The #GdkDisplay that the drop belongs to.
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
   * GdkDrop:drag:
   *
   * The #GdkDrag that initiated this drop
   */
  properties[PROP_DRAG] =
    g_param_spec_object ("drag",
                         "Drag",
                         "The drag that initiated this drop",
                         GDK_TYPE_DRAG,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrop:formats:
   *
   * The possible formats that the drop can provide its data in.
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

  /**
   * GdkDrop:surface:
   *
   * The #GdkSurface the drop happens on
   */
  properties[PROP_SURFACE] =
    g_param_spec_object ("surface",
                         "Surface",
                         "The surface the drop is happening on",
                         GDK_TYPE_SURFACE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
gdk_drop_init (GdkDrop *self)
{
}

/**
 * gdk_drop_get_display:
 * @self: a #GdkDrop
 *
 * Gets the #GdkDisplay that @self was created for.
 *
 * Returns: (transfer none): a #GdkDisplay
 **/
GdkDisplay *
gdk_drop_get_display (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), NULL);

  return gdk_device_get_display (priv->device);
}

/**
 * gdk_drop_get_device:
 * @self: a #GdkDrop
 *
 * Returns the #GdkDevice performing the drop.
 *
 * Returns: (transfer none): The #GdkDevice performing the drop.
 **/
GdkDevice *
gdk_drop_get_device (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), NULL);

  return priv->device;
}

/**
 * gdk_drop_get_formats:
 * @self: a #GdkDrop
 *
 * Returns the #GdkContentFormats that the drop offers the data
 * to be read in.
 *
 * Returns: (transfer none): The possible #GdkContentFormats
 **/
GdkContentFormats *
gdk_drop_get_formats (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), NULL);

  return priv->formats;
}

/**
 * gdk_drop_get_surface:
 * @self: a #GdkDrop
 *
 * Returns the #GdkSurface performing the drop.
 *
 * Returns: (transfer none): The #GdkSurface performing the drop.
 **/
GdkSurface *
gdk_drop_get_surface (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), NULL);

  return priv->surface;
}

/**
 * gdk_drop_get_actions:
 * @self: a #GdkDrop
 *
 * Returns the possible actions for this #GdkDrop. If this value
 * contains multiple actions - ie gdk_drag_action_is_unique()
 * returns %FALSE for the result - gdk_drag_finish() must choose
 * the action to use when accepting the drop.
 *
 * This value may change over the lifetime of the #GdkDrop both
 * as a response to source side actions as well as to calls to
 * gdk_drop_status() or gdk_drag_finish(). The source side will
 * not change this value anymore once a drop has started.
 *
 * Returns: The possible #GdkDragActions
 **/
GdkDragAction
gdk_drop_get_actions (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), 0);

  return priv->actions;
}

void
gdk_drop_set_actions (GdkDrop       *self,
                      GdkDragAction  actions)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_if_fail (GDK_IS_DROP (self));
  g_return_if_fail ((actions & GDK_ACTION_ASK) == 0);

  if (priv->actions == actions)
    return;

  priv->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIONS]);
}

/**
 * gdk_drop_get_drag:
 * @self: a #GdkDrop
 *
 * If this is an in-app drag-and-drop operation, returns the #GdkDrag
 * that corresponds to this drop.
 *
 * If it is not, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): the corresponding #GdkDrag
 **/
GdkDrag *
gdk_drop_get_drag (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), 0);

  return priv->drag;
}

/**
 * gdk_drop_status:
 * @self: a #GdkDrop
 * @actions: Supported actions of the destination, or 0 to indicate
 *    that a drop will not be accepted
 *
 * Selects all actions that are potentially supported by the destination.
 *
 * When calling this function, do not restrict the passed in actions to
 * the ones provided by gdk_drop_get_actions(). Those actions may
 * change in the future, even depending on the actions you provide here.
 *
 * This function should be called by drag destinations in response to
 * %GDK_DRAG_ENTER or %GDK_DRAG_MOTION events. If the destination does
 * not yet know the exact actions it supports, it should set any possible
 * actions first and then later call this function again.
 */
void
gdk_drop_status (GdkDrop       *self,
                 GdkDragAction  actions)
{
  g_return_if_fail (GDK_IS_DROP (self));

  GDK_DROP_GET_CLASS (self)->status (self, actions);
}

/**
 * gdk_drop_finish:
 * @self: a #GdkDrop
 * @action: the action performed by the destination or 0 if the drop
 *     failed
 *
 * Ends the drag operation after a drop.
 * 
 * The @action must be a single action selected from the actions
 * available via gdk_drop_get_actions().
 **/
void
gdk_drop_finish (GdkDrop       *self,
                 GdkDragAction  action)
{
  g_return_if_fail (GDK_IS_DROP (self));
  g_return_if_fail (gdk_drag_action_is_unique (action));

  GDK_DROP_GET_CLASS (self)->finish (self, action);
}

static void
gdk_drop_read_internal (GdkDrop             *self,
                        GdkContentFormats   *formats,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  if (priv->drag)
    {
      gdk_drop_read_local_async (self,
                                 formats,
                                 io_priority,
                                 cancellable,
                                 callback,
                                 user_data);
    }
  else
    {
      GDK_DROP_GET_CLASS (self)->read_async (self,
                                             formats,
                                             io_priority,
                                             cancellable,
                                             callback,
                                             user_data);
    }
}

/**
 * gdk_drop_read_async:
 * @self: a #GdkDrop
 * @mime_types: (array zero-terminated=1) (element-type utf8):
 *     pointer to an array of mime types
 * @io_priority: the io priority for the read operation
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when
 *     the request is satisfied
 * @user_data: (closure): the data to pass to @callback
 *
 * Asynchronously read the dropped data from a #GdkDrop
 * in a format that complies with one of the mime types.
 */
void
gdk_drop_read_async (GdkDrop             *self,
                     const char         **mime_types,
                     int                  io_priority,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  GdkContentFormats *formats;

  g_return_if_fail (GDK_IS_DROP (self));
  g_return_if_fail (mime_types != NULL && mime_types[0] != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  formats = gdk_content_formats_new (mime_types, g_strv_length ((char **) mime_types));

  gdk_drop_read_internal (self, formats, io_priority, cancellable, callback, user_data);

  gdk_content_formats_unref (formats);
}

/**
 * gdk_drop_read_finish:
 * @self: a #GdkDrop
 * @result: a #GAsyncResult
 * @out_mime_type: (out) (type utf8): return location for the used mime type
 * @error: (allow-none): location to store error information on failure, or %NULL
 *
 * Finishes an async drop read operation, see gdk_drop_read_async().
 *
 * Returns: (nullable) (transfer full): the #GInputStream, or %NULL
 */
GInputStream *
gdk_drop_read_finish (GdkDrop       *self,
                      GAsyncResult  *result,
                      const char   **out_mime_type,
                      GError       **error)
{
  g_return_val_if_fail (GDK_IS_DROP (self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (g_async_result_is_tagged (result, gdk_drop_read_local_async))
    {
      return gdk_drop_read_local_finish (self, result, out_mime_type, error);
    }
  else
    {
      return GDK_DROP_GET_CLASS (self)->read_finish (self, result, out_mime_type, error);
    }
}

static void
gdk_drop_read_value_done (GObject      *source,
                          GAsyncResult *result,
                          gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  GValue *value;

  value = g_task_get_task_data (task);

  if (!gdk_content_deserialize_finish (result, value, &error))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, value, NULL);

  g_object_unref (task);
}

static void
gdk_drop_read_value_got_stream (GObject      *source,
                                GAsyncResult *result,
                                gpointer      data)
{
  GInputStream *stream;
  GError *error = NULL;
  GTask *task = data;
  const char *mime_type;

  stream = gdk_drop_read_finish (GDK_DROP (source), result, &mime_type, &error);
  if (stream == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  gdk_content_deserialize_async (stream,
                                 mime_type,
                                 G_VALUE_TYPE (g_task_get_task_data (task)),
                                 g_task_get_priority (task),
                                 g_task_get_cancellable (task),
                                 gdk_drop_read_value_done,
                                 task);
  g_object_unref (stream);
}

static void
free_value (gpointer value)
{
  g_value_unset (value);
  g_slice_free (GValue, value);
}

static void
gdk_drop_read_value_internal (GdkDrop             *self,
                              GType                type,
                              gpointer             source_tag,
                              int                  io_priority,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *formats;
  GValue *value;
  GTask *task;
 
  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, source_tag);
  value = g_slice_new0 (GValue);
  g_value_init (value, type);
  g_task_set_task_data (task, value, free_value);

  if (priv->drag)
    {
      GError *error = NULL;
      GdkContentProvider *content;
      gboolean res;

      g_object_get (priv->drag, "content", &content, NULL);

      res = gdk_content_provider_get_value (content, value, &error);

      g_object_unref (content);

      if (res)
        {
          g_task_return_pointer (task, value, NULL);
          g_object_unref (task);
          return;
        }
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          g_task_return_error (task, error);
          g_object_unref (task);
          return;
        }
      else
        {
          /* fall through to regular stream transfer */
          g_clear_error (&error);
        }
    }

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_gtype (builder, type);
  formats = gdk_content_formats_builder_free_to_formats (builder);
  formats = gdk_content_formats_union_deserialize_mime_types (formats);

  gdk_drop_read_internal (self,
                          formats,
                          io_priority,
                          cancellable,
                          gdk_drop_read_value_got_stream,
                          task);

  gdk_content_formats_unref (formats);
}

/**
 * gdk_drop_read_value_async:
 * @self: a #GdkDrop
 * @type: a #GType to read
 * @io_priority: the [I/O priority][io-priority]
 *     of the request. 
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously request the drag operation's contents converted to the given
 * @type. When the operation is finished @callback will be called. 
 * You can then call gdk_drop_read_value_finish() to get the resulting
 * #GValue.
 *
 * For local drag'n'drop operations that are available in the given #GType, the
 * value will be copied directly. Otherwise, GDK will try to use
 * gdk_content_deserialize_async() to convert the data.
 **/
void
gdk_drop_read_value_async (GdkDrop             *self,
                           GType                type,
                           int                  io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_return_if_fail (GDK_IS_DROP (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  gdk_drop_read_value_internal (self,
                                type,
                                gdk_drop_read_value_async,
                                io_priority,
                                cancellable,
                                callback,
                                user_data);
}

/**
 * gdk_drop_read_value_finish:
 * @self: a #GdkDrop
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an async drop read started with
 * gdk_drop_read_value_async().
 *
 * Returns: (transfer none): a #GValue containing the result.
 **/
const GValue *
gdk_drop_read_value_finish (GdkDrop       *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_drop_read_value_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gdk_drop_read_text_async:
 * @self: a #GdkDrop
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously request the drag operation's contents converted to a string.
 * When the operation is finished @callback will be called. You can then
 * call gdk_drop_read_text_finish() to get the result.
 *
 * This is a simple wrapper around gdk_drop_read_value_async(). Use
 * that function or gdk_drop_read_async() directly if you need more
 * control over the operation.
 **/
void
gdk_drop_read_text_async (GdkDrop             *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_return_if_fail (GDK_IS_DROP (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  gdk_drop_read_value_internal (self,
                                G_TYPE_STRING,
                                gdk_drop_read_text_async,
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                callback,
                                user_data);
}

/**
 * gdk_drop_read_text_finish:
 * @self: a #GdkDrop
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an asynchronous read started with
 * gdk_drop_read_text_async().
 *
 * Returns: (transfer full) (nullable): a new string or %NULL on error.
 **/
char *
gdk_drop_read_text_finish (GdkDrop       *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  const GValue *value;

  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_drop_read_text_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  value = g_task_propagate_pointer (G_TASK (result), error);
  if (!value)
    return NULL;
  
  return g_value_dup_string (value);
}

static void
gdk_drop_do_emit_event (GdkEvent *event,
                        gboolean  dont_queue)
{
  if (dont_queue)
    {
      _gdk_event_emit (event);
      g_object_unref (event);
    }
  else
    {
      _gdk_event_queue_append (gdk_event_get_display (event), event);
    }
}
void
gdk_drop_emit_enter_event (GdkDrop  *self,
                           gboolean  dont_queue,
                           guint32   time)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);
  GdkEvent *event;

  event = gdk_event_new (GDK_DRAG_ENTER);
  event->any.surface = g_object_ref (priv->surface);
  event->dnd.drop = g_object_ref (self);
  event->dnd.time = time;
  gdk_event_set_device (event, priv->device);

  gdk_drop_do_emit_event (event, dont_queue);
}

void
gdk_drop_emit_motion_event (GdkDrop  *self,
                            gboolean  dont_queue,
                            double    x_root,
                            double    y_root,
                            guint32   time)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);
  GdkEvent *event;

  event = gdk_event_new (GDK_DRAG_MOTION);
  event->any.surface = g_object_ref (priv->surface);
  event->dnd.drop = g_object_ref (self);
  event->dnd.time = time;
  event->dnd.x_root = x_root;
  event->dnd.y_root = y_root;
  gdk_event_set_device (event, priv->device);

  gdk_drop_do_emit_event (event, dont_queue);
}

void
gdk_drop_emit_leave_event (GdkDrop  *self,
                           gboolean  dont_queue,
                           guint32   time)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);
  GdkEvent *event;

  event = gdk_event_new (GDK_DRAG_LEAVE);
  event->any.surface = g_object_ref (priv->surface);
  event->dnd.drop = g_object_ref (self);
  event->dnd.time = time;
  gdk_event_set_device (event, priv->device);

  gdk_drop_do_emit_event (event, dont_queue);
}

void
gdk_drop_emit_drop_event (GdkDrop  *self,
                          gboolean  dont_queue,
                          double    x_root,
                          double    y_root,
                          guint32   time)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);
  GdkEvent *event;

  event = gdk_event_new (GDK_DROP_START);
  event->any.surface = g_object_ref (priv->surface);
  event->dnd.drop = g_object_ref (self);
  event->dnd.time = time;
  event->dnd.x_root = x_root;
  event->dnd.y_root = y_root;
  gdk_event_set_device (event, priv->device);

  gdk_drop_do_emit_event (event, dont_queue);
}

