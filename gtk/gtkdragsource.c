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
#include "gtkdragicon.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkicontheme.h"
#include "gtkpicture.h"
#include "gtksettingsprivate.h"
#include "gtkgesturesingle.h"

/**
 * SECTION:gtkdragsource
 * @Short_description: Event controller to initiate DND operations
 * @Title: GtkDragSource
 *
 * GtkDragSource is an auxiliary object that is used to initiate
 * Drag-And-Drop operations. It can be set up with the necessary
 * ingredients for a DND operation ahead of time. This includes
 * the source for the data that is being transferred, in the form
 * of a #GdkContentProvider, the desired action, and the icon to
 * use during the drag operation. After setting it up, the drag
 * source must be added to a widget as an event controller, using
 * gtk_widget_add_controller().
 *
 * Setting up the content provider and icon ahead of time only
 * makes sense when the data does not change. More commonly, you
 * will want to set them up just in time. To do so, #GtkDragSource
 * has #GtkDragSource::prepare and #GtkDragSource::drag-begin signals.
 * The ::prepare signal is emitted before a drag is started, and
 * can be used to set the content provider and actions that the
 * drag should be started with. The ::drag-begin signal is emitted
 * after the #GdkDrag object has been created, and can be used
 * to set up the drag icon.
 *
 * During the DND operation, GtkDragSource emits signals that
 * can be used to obtain updates about the status of the operation,
 * but it is not normally necessary to connect to any signals,
 * except for one case: when the supported actions include
 * %GDK_DRAG_MOVE, you need to listen for the
 * #GtkDragSource::drag-end signal and delete the
 * data after it has been transferred.
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

  GdkDrag *drag;
};

struct _GtkDragSourceClass
{
  GtkGestureSingleClass parent_class;

  GdkContentProvider *(* prepare) (GtkDragSource *source,
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
  DRAG_CANCEL,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

static void gtk_drag_source_dnd_finished_cb   (GdkDrag             *drag,
                                               GtkDragSource       *source);
static void gtk_drag_source_cancel_cb         (GdkDrag             *drag,
                                               GdkDragCancelReason  reason,
                                               GtkDragSource       *source);

static GdkContentProvider *gtk_drag_source_prepare (GtkDragSource *source,
                                                    double         x,
                                                    double         y);

static void gtk_drag_source_drag_begin (GtkDragSource *source);

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
                              GdkEvent           *event)
{
  /* Let touchpad swipe events go through, only if they match n-points  */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE)
    {
      guint n_points;
      guint n_fingers;

      g_object_get (G_OBJECT (controller), "n-points", &n_points, NULL);
      n_fingers = gdk_touchpad_event_get_n_fingers (event);

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
}

static void
gtk_drag_source_update (GtkGesture       *gesture,
                        GdkEventSequence *sequence)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (gesture);
  GtkWidget *widget;
  double x, y;

  if (!gtk_gesture_is_recognized (gesture))
    return;

  gtk_gesture_get_point (gesture, sequence, &x, &y);

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (gtk_drag_check_threshold (widget, source->start_x, source->start_y, x, y))
    {
      gtk_drag_source_drag_begin (source);
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
   * Note that you must handle the #GtkDragSource::drag-end signal
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
   * The ::prepare signal is emitted when a drag is about to be initiated.
   * It returns the * #GdkContentProvider to use for the drag that is about
   * to start. The default handler for this signal returns the value of
   * the #GtkDragSource::content property, so if you set up that property
   * ahead of time, you don't need to connect to this signal.
   *
   * Returns: (transfer full) (nullable): a #GdkContentProvider, or %NULL
   */
  signals[PREPARE] =
      g_signal_new (I_("prepare"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDragSourceClass, prepare),
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    GDK_TYPE_CONTENT_PROVIDER, 2,
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
   * things done in #GtkDragSource::prepare or #GtkDragSource::drag-begin.
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
   * GtkDragSource::drag-cancel:
   * @source: the #GtkDragSource
   * @drag: the #GtkDrag object
   * @reason: information on why the drag failed
   *
   * The ::drag-cancel signal is emitted on the drag source when a drag has
   * failed. The signal handler may handle a failed drag operation based on
   * the type of error. It should return %TRUE if the failure has been handled
   * and the default "drag operation failed" animation should not be shown.
   *
   * Returns: %TRUE if the failed drag operation has been already handled
   */
  signals[DRAG_CANCEL] =
      g_signal_new (I_("drag-cancel"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    _gtk_boolean_handled_accumulator, NULL,
                    _gtk_marshal_BOOLEAN__OBJECT_ENUM,
                    G_TYPE_BOOLEAN, 2,
                    GDK_TYPE_DRAG,
                    GDK_TYPE_DRAG_CANCEL_REASON);
}

static GdkContentProvider *
gtk_drag_source_prepare (GtkDragSource *source,
                         double         x,
                         double         y)
{
  if (source->actions == 0)
    return NULL;

  if (source->content == NULL)
    return NULL;

  return g_object_ref (source->content);
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
  g_clear_object (&source->drag);
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

  g_signal_emit (source, signals[DRAG_CANCEL], 0, source->drag, reason, &success);
  drag_end (source, FALSE);
}

static void
gtk_drag_source_drag_begin (GtkDragSource *source)
{
  GtkWidget *widget;
  GdkDevice *device;
  int x, y;
  GtkNative *native;
  GdkSurface *surface;
  double px, py;
  int dx, dy;
  GdkContentProvider *content = NULL;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  device = gtk_gesture_get_device (GTK_GESTURE (source));

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  native = gtk_widget_get_native (widget);
  surface = gtk_native_get_surface (native);

  gtk_widget_translate_coordinates (widget, GTK_WIDGET (native), source->start_x, source->start_y, &x, &y);
  gdk_surface_get_device_position (surface, device, &px, &py, NULL);

  dx = round (px) - x;
  dy = round (py) - y;

  g_signal_emit (source, signals[PREPARE], 0, source->start_x, source->start_y, &content);
  if (!content)
    return;

  source->drag = gdk_drag_begin (surface, device, content, source->actions, dx, dy);

  g_object_unref (content);

  if (source->drag == NULL)
    return;

  gtk_widget_reset_controllers (widget);

  g_signal_emit (source, signals[DRAG_BEGIN], 0, source->drag);

  if (!source->paintable)
    {
      GtkIconTheme *theme;

      theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (widget));
      source->paintable = GDK_PAINTABLE(gtk_icon_theme_lookup_icon (theme,
                                                                    "text-x-generic",
                                                                    NULL,
                                                                    32,
                                                                    1,
                                                                    gtk_widget_get_direction (widget),
                                                                    0));
      source->hot_x = 0;
      source->hot_y = 0;
    }

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
 * Returns: (transfer none): the #GtkContentProvider of @source
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
 * @content: (nullable): a #GtkContentProvider, or %NULL
 *
 * Sets a content provider on a #GtkDragSource.
 *
 * When the data is requested in the cause of a
 * DND operation, it will be obtained from the
 * content provider.
 *
 * This function can be called before a drag is started,
 * or in a handler for the #GtkDragSource::prepare signal.
 *
 * You may consider setting the content provider back to
 * %NULL in a #GTkDragSource::drag-end signal handler.
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
 * to potential drop targets. If @actions include
 * %GDK_ACTION_MOVE, you need to listen to the
 * #GtkDraGSource::drag-end signal and handle
 * @delete_data being %TRUE.
 *
 * This function can be called before a drag is started,
 * or in a handler for the #GtkDragSource::prepare signal.
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
 *
 * This function can be called before a drag is started, or in
 * a #GtkDragSource::prepare or #GtkDragSource::drag-begin signal handler.
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
 * Returns: (nullable) (transfer none): the #GdkDrag of the current drag operation, or %NULL
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

      g_signal_emit (source, signals[DRAG_CANCEL], 0, source->drag, GDK_DRAG_CANCEL_ERROR, &success);
      drag_end (source, FALSE);
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
