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

#include "gtkdragsource.h"

#include "gtkgesturedrag.h"
#include "gtkgesturesingleprivate.h"
#include "gtkimagedefinitionprivate.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtkstylecontext.h"
#include "gtkimageprivate.h"
#include "gtkdragiconprivate.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkicontheme.h"
#include "gtkpicture.h"
#include "gtksettingsprivate.h"
#include "gtkgesturesingle.h"

/**
 * SECTION:gtkdragsource
 * @Short_description: An object to initiate DND operations
 * @Title: GtkDragSource
 *
 * GtkDragSource is an auxiliary object that is used to initiate
 * Drag-And-Drop operations. It can be set up with the necessary
 * ingredients for a DND operation ahead of time. This includes
 * the source for the data that is being transferred, in the form
 * of a #GdkContentProvider, the desired action, and the icon to
 * use during the drag operation.
 *
 * GtkDragSource can be used in two ways:
 * - for static drag-source configuration
 * - for one-off drag operations
 *
 * To configure a widget as a permanent source for DND operations,
 * set up the GtkDragSource, then call gtk_drag_source_attach().
 * This sets up a drag gesture on the widget that will trigger
 * DND actions.
 *
 * To initiate a on-off drag operation, set up the GtkDragSource,
 * then call gtk_drag_source_drag_begin(). GTK keeps a reference
 * on the drag source until the DND operation is done, so you
 * can unref the source after calling drag_being().
 *
 * During the DND operation, GtkDragSource emits signals that
 * can be used to obtain updates about the status of the operation,
 * but it is not normally necessary to connect to any signals,
 * except for one case: when the supported actions include
 * %GDK_DRAG_MOVE, you need to listen for the
 * #GtkDragSource::drag-data-deleted signal and delete the
 * drag data after it has been transferred.
 */

struct _GtkDragSource
{
  GtkGestureSingle parent_instance;

  GdkContentProvider *content;
  GdkDragAction actions;

  GdkPaintable *paintable;
  int hot_x;
  int hot_y;

  gdouble start_x;
  gdouble start_y;
  gdouble last_x;
  gdouble last_y;

  GdkDrag *drag;
};

struct _GtkDragSourceClass
{
  GtkGestureSingleClass parent_class;

  gboolean (* prepare) (GtkDragSource *source,
                        double         x,
                        double         y);
};

enum {
  PROP_CONTENT = 1,
  PROP_ACTIONS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  PREPARE,
  DRAG_BEGIN,
  DRAG_END,
  DRAG_FAILED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

static void gtk_drag_source_dnd_finished_cb   (GdkDrag             *drag,
                                               GtkDragSource       *source);
static void gtk_drag_source_cancel_cb         (GdkDrag             *drag,
                                               GdkDragCancelReason  reason,
                                               GtkDragSource       *source);

static gboolean gtk_drag_source_prepare (GtkDragSource *source,
                                         double         x,
                                         double         y);

G_DEFINE_TYPE (GtkDragSource, gtk_drag_source, GTK_TYPE_GESTURE_SINGLE);

static void
gtk_drag_source_init (GtkDragSource *source)
{
  source->actions = GDK_ACTION_COPY;
}

static void
gtk_drag_source_finalize (GObject *object)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  g_clear_object (&source->content);
  g_clear_object (&source->paintable);

  G_OBJECT_CLASS (gtk_drag_source_parent_class)->finalize (object);
}

static void
gtk_drag_source_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);
  
  switch (prop_id)
    {
    case PROP_CONTENT:
      gtk_drag_source_set_content (source, g_value_get_object (value));
      break;

    case PROP_ACTIONS:
      gtk_drag_source_set_actions (source, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drag_source_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_object (value, gtk_drag_source_get_content (source));
      break;

    case PROP_ACTIONS:
      g_value_set_flags (value, gtk_drag_source_get_actions (source));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
gtk_drag_source_filter_event (GtkEventController *controller,
                              const GdkEvent     *event)
{
  /* Let touchpad swipe events go through, only if they match n-points  */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE)
    {
      guint n_points;
      guint n_fingers;

      g_object_get (G_OBJECT (controller), "n-points", &n_points, NULL);
      gdk_event_get_touchpad_gesture_n_fingers (event, &n_fingers);

      if (n_fingers == n_points)
        return FALSE;
      else
        return TRUE;
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_drag_source_parent_class)->filter_event (controller, event);
}

static void
gtk_drag_source_begin (GtkGesture       *gesture,
                       GdkEventSequence *sequence)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (gesture);
  GdkEventSequence *current;

  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  gtk_gesture_get_point (gesture, current, &source->start_x, &source->start_y);
  source->last_x = source->start_x;
  source->last_y = source->start_y;
}

static void
gtk_drag_source_update (GtkGesture       *gesture,
                        GdkEventSequence *sequence)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (gesture);
  GtkWidget *widget;
  GdkDevice *device;

  gtk_gesture_get_point (gesture, sequence, &source->last_x, &source->last_y);
  if (!gtk_gesture_is_recognized (gesture))
    return;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  device = gtk_gesture_get_device (gesture);

  if (gtk_drag_check_threshold (widget,
                                source->start_x, source->start_y,
                                source->last_x, source->last_y))
    {
      gtk_drag_source_drag_begin (source, widget, device, source->start_x, source->start_y);
   }
}

static void
gtk_drag_source_class_init (GtkDragSourceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (class);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (class);

  object_class->finalize = gtk_drag_source_finalize;
  object_class->set_property = gtk_drag_source_set_property;
  object_class->get_property = gtk_drag_source_get_property;

  controller_class->filter_event = gtk_drag_source_filter_event;

  gesture_class->begin = gtk_drag_source_begin;
  gesture_class->update = gtk_drag_source_update;
  gesture_class->end = NULL;

  class->prepare = gtk_drag_source_prepare;

  /**
   * GtkDragSource:content:
   *
   * The data that is offered by drag operations from this source,
   * in the form of a #GdkContentProvider.
   */ 
  properties[PROP_CONTENT] =
       g_param_spec_object ("content",
                            P_("Content"),
                            P_("The content provider for the dragged data"),
                           GDK_TYPE_CONTENT_PROVIDER,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDragSource:actions:
   *
   * The actions that are supported by drag operations from the source.
   *
   * Note that you must handle the #GtkDragSource::drag-data-deleted signal
   * if the actions include %GDK_ACTION_MOVE.
   */ 
  properties[PROP_ACTIONS] =
       g_param_spec_flags ("actions",
                           P_("Actions"),
                           P_("Supported actions"),
                           GDK_TYPE_DRAG_ACTION, GDK_ACTION_COPY,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkDragSource::prepare:
   * @source: the #GtkDragSource
   * @x: the X coordinate of the drag starting point
   * @y: the Y coordinate fo the drag starting point
   *
   * The ::prepare signal is emitted when a drag is about to be initiated. It can
   * be used to set up #GtkDragSource:content and #GtkDragSource:actions just in time,
   * or to start the drag conditionally.
   *
   * Returns: %TRUE to start the drag
   */
  signals[PREPARE] =
      g_signal_new (I_("prepare"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDragSourceClass, prepare),
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 2,
                    G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  /**
   * GtkDragSource::drag-begin:
   * @source: the #GtkDragSource
   * @drag: the #GtkDrag object
   *
   * The ::drag-begin signal is emitted on the drag source when a drag
   * is started. It can be used to e.g. set a custom drag icon with
   * gtk_drag_source_set_icon().
   */
  signals[DRAG_BEGIN] =
      g_signal_new (I_("drag-begin"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1,
                    GDK_TYPE_DRAG);

  /**
   * GtkDragSource::drag-end:
   * @source: the #GtkDragSource
   * @drag: the #GtkDrag object
   * @delete_data: %TRUE if the drag was performing %GDK_ACTION_MOVE,
   *    and the data should be deleted
   *
   * The ::drag-end signal is emitted on the drag source when a drag is
   * finished. A typical reason to connect to this signal is to undo
   * things done in #GtkDragSource::drag-begin.
   */ 
  signals[DRAG_END] =
      g_signal_new (I_("drag-end"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 2,
                    GDK_TYPE_DRAG,
                    G_TYPE_BOOLEAN);

  /**
   * GtkDragSource::drag-failed:
   * @source: the #GtkDragSource
   * @drag: the #GtkDrag object
   * @reason: information on why the drag failed
   *
   * The ::drag-failed signal is emitted on the drag source when a drag has
   * failed. The signal handler may handle a failed drag operation based on
   * the type of error. It should return %TRUE if the failure has been handled
   * and the default "drag operation failed" should not be shown.
   *
   * Returns: %TRUE if the failed drag operation has been already handled
   */
  signals[DRAG_FAILED] =
      g_signal_new (I_("drag-failed"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    _gtk_boolean_handled_accumulator, NULL,
                    _gtk_marshal_BOOLEAN__OBJECT_ENUM,
                    G_TYPE_BOOLEAN, 2,
                    GDK_TYPE_DRAG,
                    GDK_TYPE_DRAG_CANCEL_REASON);
}

static gboolean
gtk_drag_source_prepare (GtkDragSource *source,
                         double         x,
                         double         y)
{
  return source->content != NULL && source->actions != 0;
}

static void
drag_end (GtkDragSource *source,
          gboolean       success)
{
  gboolean delete_data;

  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_dnd_finished_cb, source);
  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_cancel_cb, source);

  delete_data = success && gdk_drag_get_selected_action (source->drag) == GDK_ACTION_MOVE;

  g_signal_emit (source, signals[DRAG_END], 0, source->drag, delete_data);

  gdk_drag_drop_done (source->drag, success);

  g_object_set_data (G_OBJECT (source->drag), I_("gtk-drag-source"), NULL);
  g_clear_object (&source->drag);
  g_object_unref (source);
}

static void
gtk_drag_source_dnd_finished_cb (GdkDrag       *drag,
                                 GtkDragSource *source)
{
  drag_end (source, TRUE);
}

static void
gtk_drag_source_cancel_cb (GdkDrag             *drag,
                           GdkDragCancelReason  reason,
                           GtkDragSource       *source)
{
  gboolean success = FALSE;

  g_signal_emit (source, signals[DRAG_FAILED], 0, source->drag, reason, &success);
  drag_end (source, FALSE);
}

/**
 * gtk_drag_source_drag_begin:
 * @source: a #GtkDragSource
 * @widget: the widget where the drag operation originates
 * @device: the device that is driving the drag operation
 * @x: start point X coordinate
 * @y: start point Y xoordinate
 *
 * Starts a DND operation with @source.
 *
 * The start point coordinates are relative to @widget.
 *
 * GTK keeps a reference on @source for the duration of
 * the DND operation, so it is safe to unref @source
 * after this call.
 */
void
gtk_drag_source_drag_begin (GtkDragSource *source,
                            GtkWidget     *widget,
                            GdkDevice     *device,
                            int            x,
                            int            y)
{
  GtkNative *native;
  GdkSurface *surface;
  double px, py;
  int dx, dy;
  gboolean start_drag = FALSE;

  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (source->content != NULL);
  g_return_if_fail (source->actions != 0);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  native = gtk_widget_get_native (widget);
  surface = gtk_native_get_surface (native);

  gtk_widget_translate_coordinates (widget, GTK_WIDGET (native), x, y, &x, &y);
  gdk_surface_get_device_position (surface, device, &px, &py, NULL);

  dx = round (px) - x;
  dy = round (py) - y;

  g_signal_emit (source, signals[PREPARE], 0, x, y, &start_drag);

  if (!start_drag)
    return;

  source->drag = gdk_drag_begin (surface, device, source->content, source->actions, dx, dy);

  if (source->drag == NULL)
    {
      g_print ("no drag :(\n");
      return;
    }

  g_object_set_data (G_OBJECT (source->drag), I_("gtk-drag-source"), source);

  gtk_widget_reset_controllers (widget);

  /* We hold a ref until ::drag-end is emitted */
  g_object_ref (source);

  g_signal_emit (source, signals[DRAG_BEGIN], 0, source->drag);

  if (!source->paintable)
    {
      GtkIconTheme *theme;
 
      theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (widget));
      source->paintable = gtk_icon_theme_load_icon (theme, "text-x-generic", 32, 0, NULL);
      source->hot_x = 0;
      source->hot_y = 0;
    }

  gdk_drag_set_hotspot (source->drag, source->hot_x, source->hot_y);
  gtk_drag_icon_set_from_paintable (source->drag, source->paintable, source->hot_x, source->hot_y);

  g_signal_connect (source->drag, "dnd-finished",
                    G_CALLBACK (gtk_drag_source_dnd_finished_cb), source);
  g_signal_connect (source->drag, "cancel",
                    G_CALLBACK (gtk_drag_source_cancel_cb), source);
}

/**
 * gtk_drag_source_new:
 *
 * Creates a new #GtkDragSource object.
 *
 * Returns: the new #GtkDragSource
 */
GtkDragSource *
gtk_drag_source_new (void)
{
  return g_object_new (GTK_TYPE_DRAG_SOURCE, NULL);
}

/**
 * gtk_drag_source_get_content:
 * @source: a #GtkDragSource
 *
 * Gets the current content provider of a #GtkDragSource.
 *
 * Returns: the #GtkContentProvider of @source
 */
GdkContentProvider *
gtk_drag_source_get_content (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->content;  
}

/**
 * gtk_drag_source_set_content:
 * @source: a #GtkDragSource
 * @content: (nullable): a #GtkContntProvider, or %NULL
 *
 * Sets a content provider on a #GtkDragSource.
 *
 * When the data is requested in the cause of a
 * DND operation, it will be obtained from the
 * content provider.
 */
void
gtk_drag_source_set_content (GtkDragSource      *source,
                             GdkContentProvider *content)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (!g_set_object (&source->content, content))
    return;

  g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_CONTENT]);
}

/**
 * gtk_drag_source_get_actions:
 * @source: a #GtkDragSource
 *
 * Gets the actions that are currently set on the #GtkDragSource.
 *
 * Returns: the actions set on @source
 */
GdkDragAction
gtk_drag_source_get_actions (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), 0);

  return source->actions;
}

/**
 * gtk_drag_source_set_actions:
 * @source: a #GtkDragSource
 * @actions: the actions to offer
 *
 * Sets the actions on the #GtkDragSource.
 *
 * During a DND operation, the actions are offered
 * to potential drop targets.
 */
void
gtk_drag_source_set_actions (GtkDragSource *source,
                             GdkDragAction  actions)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->actions == actions)
    return;

  source->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_ACTIONS]);
}

/**
 * gtk_drag_source_set_icon:
 * @source: a #GtkDragSource
 * @paintable: (nullable): the #GtkPaintable to use as icon, or %NULL
 * @hot_x: the hotspot X coordinate on the icon
 * @hot_y: the hotspot Y coordinate on the icon
 *
 * Sets a paintable to use as icon during DND operations.
 *
 * The hotspot coordinates determine the point on the icon
 * that gets aligned with the hotspot of the cursor.
 *
 * If @paintable is %NULL, a default icon is used.
 */
void
gtk_drag_source_set_icon (GtkDragSource *source,
                          GdkPaintable  *paintable,
                          int            hot_x,
                          int            hot_y)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  g_set_object (&source->paintable, paintable);

  source->hot_x = hot_x;
  source->hot_y = hot_y;
}

/**
 * gtk_drag_source_get_drag:
 * @source: a #GtkDragSource
 *
 * Returns the underlying #GtkDrag object for an ongoing drag.
 *
 * Returns: (nullable): the #GdkDrag of the current drag operation, or %NULL
 */
GdkDrag *
gtk_drag_source_get_drag (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->drag;
}

/**
 * gtk_drag_source_drag_cancel:
 * @source: a #GtkDragSource
 *
 * Cancels a currently ongoing drag operation.
 */
void
gtk_drag_source_drag_cancel (GtkDragSource *source)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->drag)
    {
      gboolean success = FALSE;

      g_signal_emit (source, signals[DRAG_FAILED], 0, source->drag, GDK_DRAG_CANCEL_ERROR, &success);

      gdk_drag_drop_done (source->drag, success);
    }
}

/**
 * gtk_drag_check_threshold: (method)
 * @widget: a #GtkWidget
 * @start_x: X coordinate of start of drag
 * @start_y: Y coordinate of start of drag
 * @current_x: current X coordinate
 * @current_y: current Y coordinate
 * 
 * Checks to see if a mouse drag starting at (@start_x, @start_y) and ending
 * at (@current_x, @current_y) has passed the GTK drag threshold, and thus
 * should trigger the beginning of a drag-and-drop operation.
 *
 * Returns: %TRUE if the drag threshold has been passed.
 */
gboolean
gtk_drag_check_threshold (GtkWidget *widget,
                          int        start_x,
                          int        start_y,
                          int        current_x,
                          int        current_y)
{
  gint drag_threshold;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  drag_threshold = gtk_settings_get_dnd_drag_threshold (gtk_widget_get_settings (widget));

  return (ABS (current_x - start_x) > drag_threshold ||
          ABS (current_y - start_y) > drag_threshold);
}
