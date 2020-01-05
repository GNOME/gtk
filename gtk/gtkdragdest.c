/* GTK - The GIMP Toolkit
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

#include "gtkdragdest.h"
#include "gtkdragdestprivate.h"

#include "gtkdndprivate.h"
#include "gtkintl.h"
#include "gtknative.h"
#include "gtktypebuiltins.h"
#include "gtkeventcontroller.h"
#include "gtkmarshalers.h"
#include "gtkselectionprivate.h"


/**
 * SECTION:gtkdroptarget
 * @Short_description: An object to receive DND drops
 * @Title: GtkDropTarget
 *
 * GtkDropTarget is an auxiliary object that is used to receive
 * Drag-and-Drop operations.
 *
 * To use a GtkDropTarget to receive drops on a widget, you create
 * a GtkDropTarget object, connect to its signals, and then attach
 * it to the widget with gtk_drop_target_attach().
 */

struct _GtkDropTarget
{
  GObject parent_instance;

  GdkContentFormats *formats;
  GdkDragAction actions;
  GtkDestDefaults defaults;
  gboolean track_motion;

  GtkWidget *widget;
  GdkDrop *drop;
  gboolean armed;
};

struct _GtkDropTargetClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FORMATS = 1,
  PROP_ACTIONS,
  PROP_DEFAULTS,
  PROP_TRACK_MOTION,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  DRAG_LEAVE,
  DRAG_MOTION,
  DRAG_DROP,
  DRAG_DATA_RECEIVED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE (GtkDropTarget, gtk_drop_target, G_TYPE_OBJECT);

static void
gtk_drop_target_init (GtkDropTarget *dest)
{
  dest->defaults = GTK_DEST_DEFAULT_ALL;
}

static void
gtk_drop_target_finalize (GObject *object)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (object);

  g_clear_pointer (&dest->formats, gdk_content_formats_unref);

  G_OBJECT_CLASS (gtk_drop_target_parent_class)->finalize (object);
}

static void
gtk_drop_target_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (object);

  switch (prop_id)
    {
    case PROP_FORMATS:
      gtk_drop_target_set_formats (dest, g_value_get_boxed (value));
      break;

    case PROP_ACTIONS:
      gtk_drop_target_set_actions (dest, g_value_get_flags (value));
      break;

    case PROP_DEFAULTS:
      gtk_drop_target_set_defaults (dest, g_value_get_flags (value));
      break;

    case PROP_TRACK_MOTION:
      gtk_drop_target_set_track_motion (dest, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drop_target_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (object);

  switch (prop_id)
    {
    case PROP_FORMATS:
      g_value_set_boxed (value, gtk_drop_target_get_formats (dest));
      break;

    case PROP_ACTIONS:
      g_value_set_flags (value, gtk_drop_target_get_actions (dest));
      break;

    case PROP_DEFAULTS:
      g_value_set_flags (value, gtk_drop_target_get_defaults (dest));
      break;

    case PROP_TRACK_MOTION:
      g_value_set_boolean (value, gtk_drop_target_get_track_motion (dest));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drop_target_class_init (GtkDropTargetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_drop_target_finalize;
  object_class->set_property = gtk_drop_target_set_property;
  object_class->get_property = gtk_drop_target_get_property;

  /**
   * GtkDropTarget:formats:
   *
   * The #GdkContentFormats that determines the supported data formats
   */
  properties[PROP_FORMATS] =
       g_param_spec_boxed ("formats", P_("Formats"), P_("Formats"),
                           GDK_TYPE_CONTENT_FORMATS,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:actions:
   *
   * The #GdkDragActions that this drop target supports
   */ 
  properties[PROP_ACTIONS] =
       g_param_spec_flags ("actions", P_("Actions"), P_("Actions"),
                           GDK_TYPE_DRAG_ACTION, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTargets:defaults:
   *
   * Flags that determine the default behavior 
   */
  properties[PROP_DEFAULTS] =
       g_param_spec_flags ("defaults", P_("Defaults"), P_("Defaults"),
                           GTK_TYPE_DEST_DEFAULTS, GTK_DEST_DEFAULT_ALL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:track-motion:
   *
   * Whether the drop target should emit #GtkDropTarget::drag-motion signals
   * unconditionally
   */
  properties[PROP_TRACK_MOTION] =
       g_param_spec_boolean ("track-motion", P_("Track motion"), P_("Track motion"),
                             FALSE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkDropTarget::drag-leave:
   * @dest: the #GtkDropTarget
   *
   * The ::drag-leave signal is emitted on the drop site when the cursor
   * leaves the widget. A typical reason to connect to this signal is to
   * undo things done in #GtkDropTarget::drag-motion, e.g. undo highlighting.
   *
   * Likewise, the #GtkWidget::drag-leave signal is also emitted before the 
   * #GtkDropTarget::drag-drop signal, for instance to allow cleaning up of
   * a preview item created in the #GtkDropTarget::drag-motion signal handler.
   */
  signals[DRAG_LEAVE] =
      g_signal_new (I_("drag-leave"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

 /**
   * GtkWidget::drag-motion:
   * @dest: the #GtkDropTarget
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   *
   * The ::drag-motion signal is emitted on the drop site when the user
   * moves the cursor over the widget during a drag. The signal handler
   * must determine whether the cursor position is in a drop zone or not.
   * If it is not in a drop zone, it returns %FALSE and no further processing
   * is necessary. Otherwise, the handler returns %TRUE. In this case, the
   * handler is responsible for providing the necessary information for
   * displaying feedback to the user, by calling gdk_drag_status().
   *
   * If the decision whether the drop will be accepted or rejected can't be
   * made based solely on the cursor position and the type of the data, the
   * handler may inspect the dragged data by calling one of the #GdkDrop
   * read functions, and defer the gdk_drag_status() call to when it has
   * received the data.
   *
   * Note that you must pass #GTK_DEST_DEFAULT_MOTION to gtk_drop_target_attach()
   * when using the ::drag-motion signal that way.
   *
   * Also note that there is no drag-enter signal. The drag receiver has to
   * keep track of whether he has received any drag-motion signals since the
   * last #GtkWidget::drag-leave and if not, treat the drag-motion signal as
   * an "enter" signal. Upon an "enter", the handler will typically highlight
   * the drop site with gtk_drag_highlight().
   *
   * Returns: whether the cursor position is in a drop zone
   */
  signals[DRAG_MOTION] =
      g_signal_new (I_("drag-motion"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 2,
                    G_TYPE_INT, G_TYPE_INT);

  /**
   * GtkDropTarget::drag-drop:
   * @dest: the #GtkDropTarget
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   *
   * The ::drag-drop signal is emitted on the drop site when the user drops
   * the data onto the widget. The signal handler must determine whether
   * the cursor position is in a drop zone or not. If it is not in a drop
   * zone, it returns %FALSE and no further processing is necessary.
   *
   * Otherwise, the handler returns %TRUE. In this case, the handler must
   * ensure that gdk_drop_finish() is called to let the source know that
   * the drop is done. The call to gtk_drag_finish() can be done either
   * directly or after receiving the data.
   *
   * To receive the data, use one of the read functions provides by #GtkDrop
   * and #GtkDragDest: gdk_drop_read_async(), gdk_drop_read_value_async(),
   * gdk_drop_read_text_async(), gtk_drop_target_read_selection().
   *
   * You can use gtk_drop_target_get_drop() to obtain the #GtkDrop object
   * for the ongoing operation in your signal handler. If you call one of the
   * read functions in your handler, GTK will ensure that the #GtkDrop object
   * stays alive until the read is completed. If you delay obtaining the data
   * (e.g. to handle #GDK_ACTION_ASK by showing a #GtkPopover), you need to
   * hold a reference on the #GtkDrop.
   *
   * Returns: whether the cursor position is in a drop zone
   */
  signals[DRAG_DROP] =
      g_signal_new (I_("drag-drop"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 2,
                    G_TYPE_INT, G_TYPE_INT);
}

/**
 * gtk_drop_target_new:
 * @defaults: flags determining the default behaviour
 * @formats: (nullable): the supported data formats
 * @actions: the supported actions
 *
 * Creates a new #GtkDropTarget object
 *
 * Returns: the new #GtkDropTarget
 */
GtkDropTarget *
gtk_drop_target_new (GtkDestDefaults    defaults,
                     GdkContentFormats *formats,
                     GdkDragAction      actions)
{
  return g_object_new (GTK_TYPE_DROP_TARGET,
                       "defaults", defaults,
                       "formats", formats,
                       "actions", actions,
                       NULL);
}

/**
 * gtk_drop_target_set_formats:
 * @dest: a #GtkDropTarget
 * @formats: (nullable): the supported data formats
 *
 * Sets th data formats that this drop target will accept.
 */
void
gtk_drop_target_set_formats (GtkDropTarget     *dest,
                             GdkContentFormats *formats)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  if (dest->formats == formats)
    return;

  if (dest->formats)
    gdk_content_formats_unref (dest->formats);

  dest->formats = formats;

  if (dest->formats)
    gdk_content_formats_ref (dest->formats);

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_FORMATS]);
}

/**
 * gtk_drop_target_get_formats:
 * @dest: a #GtkDropTarget
 *
 * Gets the data formats that this drop target accepts.
 *
 * Returns: the supported data formats
 */
GdkContentFormats *
gtk_drop_target_get_formats (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);
  
  return dest->formats;
}

/**
 * gtk_drop_target_set_actions:
 * @dest: a #GtkDropTarget
 * @actions: the supported actions
 *
 * Sets the actions that this drop target supports.
 */
void
gtk_drop_target_set_actions (GtkDropTarget *dest,
                             GdkDragAction  actions)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  
  if (dest->actions == actions)
    return;

  dest->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_ACTIONS]);
}

/**
 * gtk_drop_target_get_actions:
 * @dst: a #GtkDropTarget
 *
 * Gets the actions that this drop target supports.
 *
 * Returns: the actions that this drop target supports
 */
GdkDragAction
gtk_drop_target_get_actions (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), 0);

  return dest->actions;
}

/**
 * gtk_drop_target_set_defaults:
 * @dest: a #GtkDropTarget
 * @defaults: flags determining the default behaviour
 *
 * Sets the flags determining the behavior of the drop target.
 */
void
gtk_drop_target_set_defaults (GtkDropTarget   *dest,
                              GtkDestDefaults  defaults)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  
  if (dest->defaults == defaults)
    return;

  dest->defaults = defaults;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_DEFAULTS]);
}

/**
 * gtk_drop_target_get_defaults:
 * @dest: a #GtkDropTarget
 *
 * Gets the flags determining the behavior of the drop target.
 *
 * Returns: flags determining the behaviour of the drop target
 */
GtkDestDefaults
gtk_drop_target_get_defaults (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), GTK_DEST_DEFAULT_ALL);

  return dest->defaults;
}

/**
 * gtk_drop_target_set_track_motion:
 * @dest: a #GtkDropTarget
 * @track_motion: whether to accept all targets
 *
 * Tells the drop target to emit #GtkDropTarget::drag-motion and
 * #GtkDropTarget::drag-leave events regardless of the targets and
 * the %GTK_DEST_DEFAULT_MOTION flag.
 *
 * This may be used when a drop target wants to do generic
 * actions regardless of the targets that the source offers.
 */
void
gtk_drop_target_set_track_motion (GtkDropTarget *dest,
                                  gboolean       track_motion)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  if (dest->track_motion == track_motion)
    return;

  dest->track_motion = track_motion;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_TRACK_MOTION]);
}

/**
 * gtk_drop_target_get_track_motion:
 * @dest: a #GtkDropTarget
 *
 * Gets the value of the #GtkDropTarget::track-motion property.
 *
 * Returns: whether to accept all targets
 */
gboolean
gtk_drop_target_get_track_motion (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), FALSE);

  return dest->track_motion;
}

static void
gtk_drag_dest_realized (GtkWidget *widget)
{
  GtkNative *native = gtk_widget_get_native (widget);

  gdk_surface_register_dnd (gtk_native_get_surface (native));
}

static void
gtk_drag_dest_hierarchy_changed (GtkWidget  *widget,
                                 GParamSpec *pspec,
                                 gpointer    data)
{
  GtkNative *native = gtk_widget_get_native (widget);

  if (native && gtk_widget_get_realized (GTK_WIDGET (native)))
    gdk_surface_register_dnd (gtk_native_get_surface (native));
}

GtkDropTarget *
gtk_drop_target_get (GtkWidget *widget)
{
  return g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));
}

/**
 * gtk_drop_target_attach:
 * @dest: (transfer full): a #GtkDropTarget
 * @widget the widget to attach @dest to
 *
 * Attaches the @dest to widget and makes it accept drops
 * on the widgets.
 *
 * To undo the effect of this call, use gtk_drop_target_detach().
 */
void
gtk_drop_target_attach (GtkDropTarget *dest,
                        GtkWidget     *widget)
{
  GtkDropTarget *old_dest;

  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  g_return_if_fail (dest->widget == NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_dest = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));
  if (old_dest)
    {
      g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_realized, old_dest);
      g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_hierarchy_changed, old_dest);
    }

  if (gtk_widget_get_realized (widget))
    gtk_drag_dest_realized (widget);

  dest->widget = widget;

  g_signal_connect (widget, "realize", G_CALLBACK (gtk_drag_dest_realized), dest);
  g_signal_connect (widget, "notify::root", G_CALLBACK (gtk_drag_dest_hierarchy_changed), dest);

  g_object_set_data_full (G_OBJECT (widget), I_("gtk-drag-dest"), dest, g_object_unref);
}

/**
 * gtk_drop_target_detach:
 * @dest: a #GtkDropTarget
 *
 * Undoes the effect of a prior gtk_drop_target_attach() call.
 */
void
gtk_drop_target_detach (GtkDropTarget *dest)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  if (dest->widget)
    {
      g_signal_handlers_disconnect_by_func (dest->widget, gtk_drag_dest_realized, dest);
      g_signal_handlers_disconnect_by_func (dest->widget, gtk_drag_dest_hierarchy_changed, dest);

      g_object_set_data (G_OBJECT (dest->widget), I_("gtk-drag-dest"), NULL);

      dest->widget = NULL;
    }
}

/**
 * gtk_drop_target_get_target:
 * @dest: a #GtkDropTarget
 *
 * Gets the widget that the drop target is attached to.
 *
 * Returns: (nullable): get the widget that @dest is attached to
 */
GtkWidget *
gtk_drop_target_get_target (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return dest->widget;
}

/**
 * gtk_drop_target_get_drop:
 * @dest: a #GtkDropTarget
 *
 * Returns the underlying #GtkDrop object for an ongoing drag.
 *
 * Returns: (nullable): the #GtkDrop of the current drag operation, or %NULL
 */
GdkDrop *
gtk_drop_target_get_drop (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return dest->drop;
}

const char *
gtk_drop_target_match (GtkDropTarget *dest,
                       GdkDrop       *drop)
{
  GdkContentFormats *formats;
  const char *match;

  formats = gdk_content_formats_ref (dest->formats);
  formats = gdk_content_formats_union_deserialize_mime_types (formats);

  match = gdk_content_formats_match_mime_type (formats, gdk_drop_get_formats (drop));

  gdk_content_formats_unref (formats);

  return match;
}

/**
 * gtk_drop_target_find_mimetype:
 * @dest: a #GtkDropTarget
 *
 * Returns a mimetype that is supported both by @dest and the ongoing
 * drag.
 *
 * Returns: (nullable): a matching mimetype for the ongoing drag, or %NULL
 */
const char *
gtk_drop_target_find_mimetype (GtkDropTarget *dest)
{
  if (!dest->drop)
    return NULL;

  return gtk_drop_target_match (dest, dest->drop);
}

static void
set_drop (GtkDropTarget *dest,
          GdkDrop       *drop)
{
  if (dest->drop == drop)
    return;

  if (dest->drop)
    g_object_remove_weak_pointer (G_OBJECT (dest->drop), (gpointer *)&dest->drop);

  dest->drop = drop;

  if (dest->drop)
    g_object_add_weak_pointer (G_OBJECT (dest->drop), (gpointer *)&dest->drop);
}

void
gtk_drop_target_emit_drag_leave (GtkDropTarget    *dest,
                                 GdkDrop          *drop,
                                 guint             time)
{
  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_LEAVE], 0, time);
  set_drop (dest, NULL);
}

gboolean
gtk_drop_target_emit_drag_motion (GtkDropTarget    *dest,
                                  GdkDrop          *drop,
                                  int               x,
                                  int               y)
{
  gboolean result = FALSE;

  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_MOTION], 0, x, y, &result);

  return result;
}

gboolean
gtk_drop_target_emit_drag_drop (GtkDropTarget    *dest,
                                GdkDrop          *drop,
                                int               x,
                                int               y)
{
  gboolean result = FALSE;

  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_DROP], 0, x, y, &result);

  return result;
}

void
gtk_drop_target_set_armed (GtkDropTarget *target,
                           gboolean       armed)
{
  target->armed = armed;
}

gboolean
gtk_drop_target_get_armed (GtkDropTarget *target)
{
  return target->armed;
}

/**
 * gtk_drag_highlight: (method)
 * @widget: a widget to highlight
 *
 * Highlights a widget as a currently hovered drop target.
 * To end the highlight, call gtk_drag_unhighlight().
 *
 * GTK calls this automatically if %GTK_DEST_DEFAULT_HIGHLIGHT is set.
 */
void
gtk_drag_highlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
}

/**
 * gtk_drag_unhighlight: (method)
 * @widget: a widget to remove the highlight from
 *
 * Removes a highlight set by gtk_drag_highlight() from a widget.
 */
void
gtk_drag_unhighlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
}


static void
gtk_drag_get_data_got_data (GObject      *source,
                            GAsyncResult *result,
                            gpointer      data)
{
  GTask *task = data;
  gssize written;
  GError *error = NULL;
  guchar *bytes;
  gsize size;
  GtkSelectionData *sdata;
  GtkDropTarget *dest;
  GdkDrop *drop;
  GdkDisplay *display;

  written = g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, &error);
  if (written < 0)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  bytes = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (source));
  size = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (source));

  dest = GTK_DROP_TARGET (g_task_get_source_object (task));
  drop = GDK_DROP (g_object_get_data (G_OBJECT (task), "drop"));
  display = GDK_DISPLAY (g_object_get_data (G_OBJECT (task), "display"));

  sdata = g_slice_new0 (GtkSelectionData);
  sdata->target = g_task_get_task_data (task);
  sdata->type = g_task_get_task_data (task);
  sdata->format = 8;
  sdata->length = size;
  sdata->data = bytes ? bytes : (guchar *)g_strdup ("");
  sdata->display = display;

  set_drop (dest, drop);
  g_task_return_pointer (task, sdata, NULL);
  set_drop (dest, NULL);

  g_object_unref (task);
}

static void
gtk_drag_get_data_got_stream (GObject      *source,
                              GAsyncResult *result,
                              gpointer      data)
{
  GTask *task = data;
  GInputStream *input_stream;
  GOutputStream *output_stream;
  GError *error = NULL;
  const char *mime_type;

  input_stream = gdk_drop_read_finish (GDK_DROP (source), result, &mime_type, &error);
  if (input_stream == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_task_set_task_data (task, (gpointer)g_intern_string (mime_type), NULL);

  output_stream = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (output_stream,
                                input_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                gtk_drag_get_data_got_data,
                                task);
  g_object_unref (output_stream);
  g_object_unref (input_stream);
}

/**
 * gtk_drop_target_read_selection:
 * @dest: a #GtkDropTarget
 * @target: the data format to read
 * @cancellable: (nullable): a cancellable
 * @callback: callback to call on completion
 * @user_data: data to pass to @callback
 *
 * Asynchronously reads the dropped data from an ongoing
 * drag on a #GtkDropTarget, and returns the data in a 
 * #GtkSelectionData object.
 *
 * This function is meant for cases where a #GtkSelectionData
 * object is needed, such as when using the #GtkTreeModel DND
 * support. In most other cases, the #GdkDrop async read
 * APIs that return in input stream or #GValue are more
 * convenient and should be preferred.
 */
void
gtk_drop_target_read_selection (GtkDropTarget       *dest,
                                GdkAtom              target,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  task = g_task_new (dest, NULL, callback, user_data);
  g_object_set_data_full (G_OBJECT (task), "drop", g_object_ref (dest->drop), g_object_unref);
  if (dest->widget)
    g_object_set_data (G_OBJECT (task), "display", gtk_widget_get_display (dest->widget));

  gdk_drop_read_async (dest->drop,
                       (const char *[2]) { target, NULL },
                       G_PRIORITY_DEFAULT,
                       NULL,
                       gtk_drag_get_data_got_stream,
                       task);
}

/**
 * gtk_drop_target_read_selection_finish:
 * @dest: a #GtkDropTarget
 * @result: a #GAsyncResult
 * @error: (allow-none): location to store error information on failure, or %NULL
 *
 * Finishes an async drop read operation, see gtk_drop_target_read_selection().
 *
 * Returns: (nullable) (transfer full): the #GtkSelectionData, or %NULL
 */
GtkSelectionData *
gtk_drop_target_read_selection_finish (GtkDropTarget  *dest,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
