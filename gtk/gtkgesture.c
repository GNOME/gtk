/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */

/**
 * SECTION:gtkgesture
 * @Short_description: Base class for gestures
 * @Title: GtkGesture
 * @See_also: #GtkEventController, #GtkGestureSingle
 *
 * #GtkGesture is the base object for gesture recognition, although this
 * object is quite generalized to serve as a base for multi-touch gestures,
 * it is suitable to implement single-touch and pointer-based gestures (using
 * the special %NULL #GdkEventSequence value for these).
 *
 * The number of touches that a #GtkGesture need to be recognized is controlled
 * by the #GtkGesture:n-points property, if a gesture is keeping track of less
 * or more than that number of sequences, it won't check wether the gesture
 * is recognized.
 *
 * As soon as the gesture has the expected number of touches, the gesture will
 * run the #GtkGesture::check signal regularly on input events until the gesture
 * is recognized, the criteria to consider a gesture as "recognized" is left to
 * #GtkGesture subclasses.
 *
 * A recognized gesture will then emit the following signals:
 * - #GtkGesture::begin when the gesture is recognized.
 * - A number of #GtkGesture::update, whenever an input event is processed.
 * - #GtkGesture::end when the gesture is no longer recognized.
 *
 * ## Event propagation
 *
 * In order to receive events, a gesture needs to either set a propagation phase
 * through gtk_event_controller_set_propagation_phase(), or feed those manually
 * through gtk_event_controller_handle_event().
 *
 * In the capture phase, events are propagated from the toplevel down to the
 * target widget, and gestures that are attached to containers above the widget
 * get a chance to interact with the event before it reaches the target.
 *
 * After the capture phase, GTK+ emits the traditional #GtkWidget::button-press,
 * #GtkWidget::button-release, #GtkWidget::touch-event, etc signals. Gestures 
 * with the %GTK_PHASE_TARGET phase are fed events from the default #GtkWidget::event
 * handlers.
 *
 * In the bubble phase, events are propagated up from the target widget to the
 * toplevel, and gestures that are attached to containers above the widget get
 * a chance to interact with events that have not been handled yet.
 *
 * ## States of a sequence # {#touch-sequence-states}
 *
 * Whenever input interaction happens, a single event may trigger a cascade of
 * #GtkGestures, both across the parents of the widget receiving the event and
 * in parallel within an individual widget. It is a responsibility of the
 * widgets using those gestures to set the state of touch sequences accordingly
 * in order to enable cooperation of gestures around the #GdkEventSequences
 * triggering those.
 *
 * Within a widget, gestures can be grouped through gtk_gesture_group(),
 * grouped gestures synchronize the state of sequences, so calling
 * gtk_gesture_set_sequence_state() on one will effectively propagate
 * the state throughout the group.
 *
 * By default, all sequences start out in the #GTK_EVENT_SEQUENCE_NONE state,
 * sequences in this state trigger the gesture event handler, but event
 * propagation will continue unstopped by gestures.
 *
 * If a sequence enters into the #GTK_EVENT_SEQUENCE_DENIED state, the gesture
 * group will effectively ignore the sequence, letting events go unstopped
 * through the gesture, but the "slot" will still remain occupied while
 * the touch is active.
 *
 * If a sequence enters in the #GTK_EVENT_SEQUENCE_CLAIMED state, the gesture
 * group will grab all interaction on the sequence, by:
 * - Setting the same sequence to #GTK_EVENT_SEQUENCE_DENIED on every other gesture
 *   group within the widget, and every gesture on parent widgets in the propagation
 *   chain.
 * - calling #GtkGesture::cancel on every gesture in widgets underneath in the
 *   propagation chain.
 * - Stopping event propagation after the gesture group handles the event.
 *
 * Note: if a sequence is set early to #GTK_EVENT_SEQUENCE_CLAIMED on
 * #GDK_TOUCH_BEGIN/#GDK_BUTTON_PRESS (so those events are captured before
 * reaching the event widget, this implies #GTK_PHASE_CAPTURE), one similar
 * event will emulated if the sequence changes to #GTK_EVENT_SEQUENCE_DENIED.
 * This way event coherence is preserved before event propagation is unstopped
 * again.
 *
 * Sequence states can't be changed freely, see gtk_gesture_set_sequence_state()
 * to know about the possible lifetimes of a #GdkEventSequence.
 */

#include "config.h"
#include "gtkgesture.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkgestureprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkintl.h"

typedef struct _GtkGesturePrivate GtkGesturePrivate;
typedef struct _PointData PointData;

enum {
  PROP_N_POINTS = 1,
  PROP_WINDOW
};

enum {
  BEGIN,
  END,
  UPDATE,
  CANCEL,
  SEQUENCE_STATE_CHANGED,
  N_SIGNALS
};

struct _PointData
{
  GdkEvent *event;
  gdouble widget_x;
  gdouble widget_y;
  guint press_handled : 1;
  guint state : 2;
};

struct _GtkGesturePrivate
{
  GHashTable *points;
  GdkEventSequence *last_sequence;
  GdkWindow *user_window;
  GdkWindow *window;
  GdkDevice *device;
  GList *group_link;
  guint n_points;
  guint recognized : 1;
};

static guint signals[N_SIGNALS] = { 0 };

#define BUTTONS_MASK (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)

GList * _gtk_gesture_get_group_link (GtkGesture *gesture);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkGesture, gtk_gesture, GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_gesture_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  switch (prop_id)
    {
    case PROP_N_POINTS:
      g_value_set_uint (value, priv->n_points);
      break;
    case PROP_WINDOW:
      g_value_set_object (value, priv->user_window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  switch (prop_id)
    {
    case PROP_N_POINTS:
      priv->n_points = g_value_get_uint (value);
      break;
    case PROP_WINDOW:
      gtk_gesture_set_window (GTK_GESTURE (object),
                              g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_finalize (GObject *object)
{
  GtkGesture *gesture = GTK_GESTURE (object);
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);

  gtk_gesture_ungroup (gesture);
  g_list_free (priv->group_link);

  g_hash_table_destroy (priv->points);

  G_OBJECT_CLASS (gtk_gesture_parent_class)->finalize (object);
}

static guint
_gtk_gesture_effective_n_points (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  GHashTableIter iter;
  guint n_points = 0;
  PointData *data;

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      if (data->state == GTK_EVENT_SEQUENCE_DENIED ||
          data->event->type == GDK_TOUCH_END ||
          data->event->type == GDK_BUTTON_RELEASE)
        continue;

      n_points++;
    }

  return n_points;
}

static gboolean
gtk_gesture_check_impl (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  guint n_points;

  priv = gtk_gesture_get_instance_private (gesture);
  n_points = _gtk_gesture_effective_n_points (gesture);

  return n_points == priv->n_points;
}

static void
_gtk_gesture_set_recognized (GtkGesture       *gesture,
                             gboolean          recognized,
                             GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->recognized == recognized)
    return;

  priv->recognized = recognized;

  if (recognized)
    g_signal_emit (gesture, signals[BEGIN], 0, sequence);
  else
    g_signal_emit (gesture, signals[END], 0, sequence);
}

static gboolean
_gtk_gesture_do_check (GtkGesture *gesture)
{
  GtkGestureClass *gesture_class;
  gboolean retval = FALSE;

  gesture_class = GTK_GESTURE_GET_CLASS (gesture);

  if (!gesture_class->check)
    return retval;

  retval = gesture_class->check (gesture);
  return retval;
}

static gboolean
_gtk_gesture_has_matching_touchpoints (GtkGesture *gesture)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);
  guint current_n_points;

  current_n_points = _gtk_gesture_effective_n_points (gesture);

  return (current_n_points == priv->n_points &&
          g_hash_table_size (priv->points) == priv->n_points);
}

static gboolean
_gtk_gesture_check_recognized (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);
  gboolean has_matching_touchpoints;

  has_matching_touchpoints = _gtk_gesture_has_matching_touchpoints (gesture);

  if (priv->recognized && !has_matching_touchpoints)
    _gtk_gesture_set_recognized (gesture, FALSE, sequence);
  else if (!priv->recognized && has_matching_touchpoints &&
           _gtk_gesture_do_check (gesture))
    _gtk_gesture_set_recognized (gesture, TRUE, sequence);

  return priv->recognized;
}

/* Finds the first window pertaining to the controller's widget */
static GdkWindow *
_find_widget_window (GtkGesture *gesture,
                     GdkWindow  *window)
{
  GtkWidget *widget, *window_widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  while (window)
    {
      gdk_window_get_user_data (window, (gpointer*) &window_widget);

      if (window_widget == widget ||
          gtk_widget_get_window (widget) == window)
        return window;

      window = gdk_window_get_effective_parent (window);
    }

  return NULL;
}

static void
_update_widget_coordinates (GtkGesture *gesture,
                            PointData  *data)
{
  GdkWindow *window, *event_widget_window;
  GtkWidget *event_widget, *widget;
  GtkAllocation allocation;
  gdouble event_x, event_y;
  gint wx, wy, x, y;

  event_widget = gtk_get_event_widget (data->event);
  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  event_widget_window = gtk_widget_get_window (event_widget);
  gdk_event_get_coords (data->event, &event_x, &event_y);
  window = data->event->any.window;

  while (window && window != event_widget_window)
    {
      gdk_window_get_position (window, &wx, &wy);
      event_x += wx;
      event_y += wy;
      window = gdk_window_get_effective_parent (window);
    }

  if (!window)
    return;

  if (!gtk_widget_get_has_window (event_widget))
    {
      gtk_widget_get_allocation (event_widget, &allocation);
      event_x -= allocation.x;
      event_y -= allocation.y;
    }

  gtk_widget_translate_coordinates (event_widget, widget,
                                    event_x, event_y, &x, &y);
  data->widget_x = x;
  data->widget_y = y;
}

static GtkEventSequenceState
gtk_gesture_get_group_state (GtkGesture       *gesture,
                             GdkEventSequence *sequence)
{
  GtkEventSequenceState state = GTK_EVENT_SEQUENCE_NONE;
  GList *group_elem;

  group_elem = g_list_first (_gtk_gesture_get_group_link (gesture));

  for (; group_elem; group_elem = group_elem->next)
    {
      if (group_elem->data == gesture)
        continue;
      if (!gtk_gesture_handles_sequence (group_elem->data, sequence))
        continue;

      state = gtk_gesture_get_sequence_state (group_elem->data, sequence);
      break;
    }

  return state;
}

static gboolean
_gtk_gesture_update_point (GtkGesture     *gesture,
                           const GdkEvent *event,
                           gboolean        add)
{
  GdkEventSequence *sequence;
  GdkWindow *widget_window;
  GtkGesturePrivate *priv;
  GdkDevice *device;
  gboolean existed;
  PointData *data;
  gdouble x, y;

  if (!gdk_event_get_coords (event, &x, &y))
    return FALSE;

  device = gdk_event_get_device (event);

  if (!device)
    return FALSE;

  priv = gtk_gesture_get_instance_private (gesture);
  widget_window = _find_widget_window (gesture, event->any.window);

  if (!widget_window)
    return FALSE;

  if (add)
    {
      /* If the event happens with the wrong device, or
       * on the wrong window, ignore.
       */
      if (priv->device && priv->device != device)
        return FALSE;
      if (priv->window && priv->window != widget_window)
        return FALSE;
      if (priv->user_window && priv->user_window != widget_window)
        return FALSE;
    }
  else if (!priv->device || !priv->window)
    return FALSE;

  sequence = gdk_event_get_event_sequence (event);
  existed = g_hash_table_lookup_extended (priv->points, sequence,
                                          NULL, (gpointer *) &data);
  if (!existed)
    {
      GtkEventSequenceState group_state;

      if (!add)
        return FALSE;

      if (g_hash_table_size (priv->points) == 0)
        {
          priv->window = widget_window;
          priv->device = device;
        }

      data = g_new0 (PointData, 1);
      g_hash_table_insert (priv->points, sequence, data);

      group_state = gtk_gesture_get_group_state (gesture, sequence);
      gtk_gesture_set_sequence_state (gesture, sequence, group_state);
    }

  if (data->event)
    gdk_event_free (data->event);

  data->event = gdk_event_copy (event);
  _update_widget_coordinates (gesture, data);

  /* Deny the sequence right away if the expected
   * number of points is exceeded, so this sequence
   * can be tracked with gtk_gesture_handles_sequence().
   */
  if (!existed && g_hash_table_size (priv->points) > priv->n_points)
    gtk_gesture_set_sequence_state (gesture, sequence,
                                    GTK_EVENT_SEQUENCE_DENIED);

  return TRUE;
}

static void
_gtk_gesture_check_empty (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (g_hash_table_size (priv->points) == 0)
    {
      priv->window = NULL;
      priv->device = NULL;
    }
}

static void
_gtk_gesture_remove_point (GtkGesture     *gesture,
                           const GdkEvent *event)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *device;

  sequence = gdk_event_get_event_sequence (event);
  device = gdk_event_get_device (event);
  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->device != device)
    return;

  g_hash_table_remove (priv->points, sequence);
  _gtk_gesture_check_empty (gesture);
}

static void
_gtk_gesture_cancel_all (GtkGesture *gesture)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GHashTableIter iter;

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer*) &sequence, NULL))
    {
      g_signal_emit (gesture, signals[CANCEL], 0, sequence);
      g_hash_table_iter_remove (&iter);
      _gtk_gesture_check_recognized (gesture, sequence);
    }

  _gtk_gesture_check_empty (gesture);
}

static gboolean
gesture_within_window (GtkGesture *gesture,
                       GdkWindow  *parent)
{
  GdkWindow *window;
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  window = gtk_widget_get_window (widget);

  while (window)
    {
      if (window == parent)
        return TRUE;

      window = gdk_window_get_effective_parent (window);
    }

  return FALSE;
}

static gboolean
gtk_gesture_handle_event (GtkEventController *controller,
                          const GdkEvent     *event)
{
  GtkGesture *gesture = GTK_GESTURE (controller);
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *source_device;
  gboolean was_recognized;

  source_device = gdk_event_get_source_device (event);

  if (!source_device)
    return FALSE;

  priv = gtk_gesture_get_instance_private (gesture);
  sequence = gdk_event_get_event_sequence (event);
  was_recognized = gtk_gesture_is_recognized (gesture);

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_DENIED)
    priv->last_sequence = sequence;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_TOUCH_BEGIN:
      if (_gtk_gesture_update_point (gesture, event, TRUE))
        {
          gboolean triggered_recognition;

          triggered_recognition =
            !was_recognized && _gtk_gesture_has_matching_touchpoints (gesture);

          if (_gtk_gesture_check_recognized (gesture, sequence))
            {
              PointData *data;

              data = g_hash_table_lookup (priv->points, sequence);

              /* If the sequence was claimed early, the press event will be consumed */
              if (gtk_gesture_get_sequence_state (gesture, sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
                data->press_handled = TRUE;
            }
          else if (triggered_recognition && g_hash_table_size (priv->points) == 0)
            {
              /* Recognition was triggered, but the gesture reset during
               * ::begin emission. Still, recognition was strictly triggered,
               * so the event should be consumed.
               */
              return TRUE;
            }
        }

      break;
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_END:
      if (_gtk_gesture_update_point (gesture, event, FALSE))
        {
          if (was_recognized &&
              _gtk_gesture_check_recognized (gesture, sequence))
            g_signal_emit (gesture, signals[UPDATE], 0, sequence);

          _gtk_gesture_remove_point (gesture, event);
        }
      break;
    case GDK_MOTION_NOTIFY:
      if ((event->motion.state & BUTTONS_MASK) == 0)
        break;

      if (event->motion.is_hint)
        gdk_event_request_motions (&event->motion);

      /* Fall through */
    case GDK_TOUCH_UPDATE:
      if (_gtk_gesture_update_point (gesture, event, FALSE) &&
          _gtk_gesture_check_recognized (gesture, sequence))
        g_signal_emit (gesture, signals[UPDATE], 0, sequence);
      break;
    case GDK_TOUCH_CANCEL:
      _gtk_gesture_cancel_sequence (gesture, sequence);
      break;
    case GDK_GRAB_BROKEN:
      if (!event->grab_broken.grab_window ||
          !gesture_within_window (gesture, event->grab_broken.grab_window))
        _gtk_gesture_cancel_all (gesture);

      return FALSE;
    default:
      /* Unhandled event */
      return FALSE;
    }

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_CLAIMED)
    return FALSE;

  return priv->recognized;
}

static void
gtk_gesture_reset (GtkEventController *controller)
{
  _gtk_gesture_cancel_all (GTK_GESTURE (controller));
}

static void
gtk_gesture_class_init (GtkGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->get_property = gtk_gesture_get_property;
  object_class->set_property = gtk_gesture_set_property;
  object_class->finalize = gtk_gesture_finalize;

  controller_class->handle_event = gtk_gesture_handle_event;
  controller_class->reset = gtk_gesture_reset;

  klass->check = gtk_gesture_check_impl;

  /**
   * GtkGesture:n-points:
   *
   * The number of touch points that trigger recognition on this gesture,
   * 
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_N_POINTS,
                                   g_param_spec_uint ("n-points",
                                                      P_("Number of points"),
                                                      P_("Number of points needed "
                                                         "to trigger the gesture"),
                                                      1, G_MAXUINT, 1,
                                                      GTK_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT_ONLY));
  /**
   * GtkGesture:window:
   *
   * If non-%NULL, the gesture will only listen for events that happen on
   * this #GdkWindow, or a child of it.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_object ("window",
                                                        P_("GdkWindow to receive events about"),
                                                        P_("GdkWindow to receive events about"),
                                                        GDK_TYPE_WINDOW,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkGesture::begin:
   * @gesture: the object which received the signal
   * @sequence: the #GdkEventSequence that made the gesture to be recognized
   *
   * This signal is emitted when the gesture is recognized. This means the
   * number of touch sequences matches #GtkGesture:n-points, and the #GtkGesture::check
   * handler(s) returned #TRUE.
   *
   * Note: These conditions may also happen when an extra touch (eg. a third touch
   * on a 2-touches gesture) is lifted, in that situation @sequence won't pertain
   * to the current set of active touches, so don't rely on this being true.
   *
   * Since: 3.14
   */
  signals[BEGIN] =
    g_signal_new ("begin",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, begin),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);
  /**
   * GtkGesture::end:
   * @gesture: the object which received the signal
   * @sequence: the #GdkEventSequence that made gesture recognition to finish
   *
   * This signal is emitted when @gesture either stopped recognizing the event
   * sequences as something to be handled (the #GtkGesture::check handler returned
   * %FALSE), or the number of touch sequences became higher or lower than
   * #GtkGesture:n-points.
   *
   * Note: @sequence might not pertain to the group of sequences that were
   * previously triggering recognition on @gesture (ie. a just pressed touch
   * sequence that exceeds #GtkGesture:n-points). This situation may be detected
   * by checking through gtk_gesture_handles_sequence().
   *
   * Since: 3.14
   */
  signals[END] =
    g_signal_new ("end",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, end),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);
  /**
   * GtkGesture::update:
   * @gesture: the object which received the signal
   * @sequence: the #GdkEventSequence that was updated
   *
   * This signal is emitted whenever an event is handled while the gesture is
   * recognized. @sequence is guaranteed to pertain to the set of active touches.
   *
   * Since: 3.14
   */
  signals[UPDATE] =
    g_signal_new ("update",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, update),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);
  /**
   * GtkGesture::cancel:
   * @gesture: the object which received the signal
   * @sequence: the #GdkEventSequence that was cancelled
   *
   * This signal is emitted whenever a sequence is cancelled. This usually
   * happens on active touches when gtk_event_controller_reset() is called
   * on @gesture (manually, due to grabs...), or the individual @sequence
   * was claimed by parent widgets' controllers (see gtk_gesture_set_sequence_state()).
   *
   * @gesture must forget everything about @sequence as a reaction to this signal.
   *
   * Since: 3.14
   */
  signals[CANCEL] =
    g_signal_new ("cancel",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, cancel),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT_SEQUENCE);
  /**
   * GtkGesture::sequence-state-changed:
   * @gesture: the object which received the signal
   * @sequence: the #GdkEventSequence that was cancelled
   * @state: the new sequence state
   *
   * This signal is emitted whenever a sequence state changes. See
   * gtk_gesture_set_sequence_state() to know more about the expectable
   * sequence lifetimes.
   *
   * Since: 3.14
   */
  signals[SEQUENCE_STATE_CHANGED] =
    g_signal_new ("sequence-state-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, sequence_state_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, GDK_TYPE_EVENT_SEQUENCE,
                  GTK_TYPE_EVENT_SEQUENCE_STATE);
}

static void
gtk_gesture_init (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);
  priv->points = g_hash_table_new_full (NULL, NULL, NULL,
                                        (GDestroyNotify) g_free);
  gtk_event_controller_set_event_mask (GTK_EVENT_CONTROLLER (gesture),
                                       GDK_TOUCH_MASK);

  priv->group_link = g_list_prepend (NULL, gesture);
}

/**
 * gtk_gesture_get_device:
 * @gesture: a #GtkGesture
 *
 * Returns the master #GdkDevice that is currently operating
 * on @gesture, or %NULL if the gesture is not being interacted.
 *
 * Returns: (transfer none) (allow-none): a #GdkDevice, or %NULL
 *
 * Since: 3.14
 **/
GdkDevice *
gtk_gesture_get_device (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->device;
}

/**
 * gtk_gesture_get_sequence_state:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Returns the @sequence state, as seen by @gesture.
 *
 * Returns: The sequence state in @gesture
 *
 * Since: 3.14
 **/
GtkEventSequenceState
gtk_gesture_get_sequence_state (GtkGesture       *gesture,
                                GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture),
                        GTK_EVENT_SEQUENCE_NONE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return GTK_EVENT_SEQUENCE_NONE;

  return data->state;
}

/**
 * gtk_gesture_set_sequence_state:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 * @state: the sequence state
 *
 * Sets the state of @sequence in @gesture. Sequences start
 * in state #GTK_EVENT_SEQUENCE_NONE, and whenever they change
 * state, they can never go back to that state. Likewise,
 * sequences in state #GTK_EVENT_SEQUENCE_DENIED cannot turn
 * back to a not denied state. With these rules, the lifetime
 * of an event sequence is constrained to the next four:
 *
 * * None
 * * None → Denied
 * * None → Claimed
 * * None → Claimed → Denied
 *
 * Note: Due to event handling ordering, it may be unsafe to
 * set the state on another gesture within a #GtkGesture::begin
 * signal handler, as the callback might be executed before
 * the other gesture knows about the sequence. A safe way to
 * perform this could be:
 *
 * |[
 * static void
 * first_gesture_begin_cb (GtkGesture       *first_gesture,
 *                         GdkEventSequence *sequence,
 *                         gpointer          user_data)
 * {
 *   gtk_gesture_set_sequence_state (first_gesture, sequence, GTK_EVENT_SEQUENCE_ACCEPTED);
 *   gtk_gesture_set_sequence_state (second_gesture, sequence, GTK_EVENT_SEQUENCE_DENIED);
 * }
 *
 * static void
 * second_gesture_begin_cb (GtkGesture       *second_gesture,
 *                          GdkEventSequence *sequence,
 *                          gpointer          user_data)
 * {
 *   if (gtk_gesture_get_sequence_state (first_gesture, sequence) == GTK_EVENT_SEQUENCE_ACCEPTED)
 *     gtk_gesture_set_sequence_state (second_gesture, sequence, GTK_EVENT_SEQUENCE_DENIED);
 * }
 * ]|
 *
 * If both gestures are in the same group, just set the state on
 * the gesture emitting the event, the sequence will be already
 * be initialized to the group's global state when the second
 * gesture processes the event.
 *
 * Returns: %TRUE if @sequence is handled by @gesture,
 *          and the state is changed successfully
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_set_sequence_state (GtkGesture            *gesture,
                                GdkEventSequence      *sequence,
                                GtkEventSequenceState  state)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (state >= GTK_EVENT_SEQUENCE_NONE &&
                        state <= GTK_EVENT_SEQUENCE_DENIED, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  if (data->state == state)
    return FALSE;

  /* denied sequences remain denied */
  if (data->state == GTK_EVENT_SEQUENCE_DENIED)
    return FALSE;

  /* Sequences can't go from claimed/denied to none */
  if (state == GTK_EVENT_SEQUENCE_NONE &&
      data->state != GTK_EVENT_SEQUENCE_NONE)
    return FALSE;

  data->state = state;
  g_signal_emit (gesture, signals[SEQUENCE_STATE_CHANGED], 0,
                 sequence, state);

  if (state == GTK_EVENT_SEQUENCE_DENIED)
    _gtk_gesture_check_recognized (gesture, sequence);

  return TRUE;
}

/**
 * gtk_gesture_set_state:
 * @gesture: a #GtkGesture
 * @state: the sequence state
 *
 * Sets the state of all sequences that @gesture is currently
 * interacting with. See gtk_gesture_set_sequence_state()
 * for more details on sequence states.
 *
 * Returns: %TRUE if the state of at least one sequence
 *     was changed successfully
 *
 * Since: 3.14
 */
gboolean
gtk_gesture_set_state (GtkGesture            *gesture,
                       GtkEventSequenceState  state)
{
  gboolean handled = FALSE;
  GtkGesturePrivate *priv;
  GList *sequences, *l;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (state >= GTK_EVENT_SEQUENCE_NONE &&
                        state <= GTK_EVENT_SEQUENCE_DENIED, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  sequences = g_hash_table_get_keys (priv->points);

  for (l = sequences; l; l = l->next)
    handled |= gtk_gesture_set_sequence_state (gesture, l->data, state);

  g_list_free (sequences);

  return handled;
}

/**
 * gtk_gesture_get_sequences:
 * @gesture: a #GtkGesture
 *
 * Returns the list of #GdkEventSequences currently being interpreted
 * by @gesture.
 *
 * Returns: (transfer container) (element-type GdkEventSequence): A list
 *          of #GdkEventSequences, the list elements are owned by GTK+
 *          and must not be freed or modified, the list itself must be deleted
 *          through g_list_free()
 *
 * Since: 3.14
 **/
GList *
gtk_gesture_get_sequences (GtkGesture *gesture)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GList *sequences = NULL;
  GHashTableIter iter;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer *) &sequence, (gpointer *) &data))
    {
      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;
      if (data->event->type == GDK_TOUCH_END ||
          data->event->type == GDK_BUTTON_RELEASE)
        continue;

      sequences = g_list_prepend (sequences, sequence);
    }

  return sequences;
}

/**
 * gtk_gesture_get_last_updated_sequence:
 * @gesture: a #GtkGesture
 *
 * Returns the #GdkEventSequence that was last updated on @gesture.
 *
 * Returns: The last updated sequence
 *
 * Since: 3.14
 **/
GdkEventSequence *
gtk_gesture_get_last_updated_sequence (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->last_sequence;
}

/**
 * gtk_gesture_get_last_event:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Returns the last event that was processed for @sequence.
 *
 * Returns: (transfer none): The last event from @sequence
 **/
const GdkEvent *
gtk_gesture_get_last_event (GtkGesture       *gesture,
                            GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return NULL;

  return data->event;
}

/**
 * gtk_gesture_get_point:
 * @gesture: a #GtkGesture
 * @sequence: (allow-none): a #GdkEventSequence, or %NULL for pointer events
 * @x: (out) (allow-none): return location for X axis of the sequence coordinates
 * @y: (out) (allow-none): return location for Y axis of the sequence coordinates
 *
 * If @sequence is currently being interpreted by @gesture, this
 * function returns %TRUE and fills in @x and @y with the last coordinates
 * stored for that event sequence. The coordinates are always relative to the
 * widget allocation.
 *
 * Returns: %TRUE if @sequence is currently interpreted
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_point (GtkGesture       *gesture,
                       GdkEventSequence *sequence,
                       gdouble          *x,
                       gdouble          *y)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return FALSE;

  if (x)
    *x = data->widget_x;
  if (y)
    *y = data->widget_y;

  return TRUE;
}

gboolean
_gtk_gesture_get_last_update_time (GtkGesture       *gesture,
                                   GdkEventSequence *sequence,
                                   guint32          *evtime)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return FALSE;

  if (evtime)
    *evtime = gdk_event_get_time (data->event);

  return TRUE;
};

/**
 * gtk_gesture_get_bounding_box:
 * @gesture: a #GtkGesture
 * @rect: (out): bounding box containing all active touches.
 *
 * If there are touch sequences being currently handled by @gesture,
 * this function returns %TRUE and fills in @rect with the bounding
 * box containing all active touches. Otherwise, %FALSE will be
 * returned.
 *
 * Returns: %TRUE if there are active touches, %FALSE otherwise
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_bounding_box (GtkGesture   *gesture,
                              GdkRectangle *rect)
{
  GtkGesturePrivate *priv;
  gdouble x1, y1, x2, y2;
  GHashTableIter iter;
  guint n_points = 0;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  x1 = y1 = G_MAXDOUBLE;
  x2 = y2 = -G_MAXDOUBLE;

  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      gdouble x, y;

      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;
      if (data->event->type == GDK_TOUCH_END ||
          data->event->type == GDK_BUTTON_RELEASE)
        continue;

      gdk_event_get_coords (data->event, &x, &y);
      n_points++;
      x1 = MIN (x1, x);
      y1 = MIN (y1, y);
      x2 = MAX (x2, x);
      y2 = MAX (y2, y);
    }

  if (n_points == 0)
    return FALSE;

  rect->x = x1;
  rect->y = y1;
  rect->width = x2 - x1;
  rect->height = y2 - y1;

  return TRUE;
}


/**
 * gtk_gesture_get_bounding_box_center:
 * @gesture: a #GtkGesture
 * @x: (out): X coordinate for the bounding box center
 * @y: (out): Y coordinate for the bounding box center
 *
 * If there are touch sequences being currently handled by @gesture,
 * this function returns %TRUE and fills in @x and @y with the center
 * of the bounding box containing all active touches. Otherwise, %FALSE
 * will be returned.
 *
 * Returns: %FALSE if no active touches are present, %TRUE otherwise
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_bounding_box_center (GtkGesture *gesture,
                                     gdouble    *x,
                                     gdouble    *y)
{
  GdkRectangle rect;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (x != NULL && y != NULL, FALSE);

  if (!gtk_gesture_get_bounding_box (gesture, &rect))
    return FALSE;

  *x = rect.x + rect.width / 2;
  *y = rect.y + rect.height / 2;
  return TRUE;
}

/**
 * gtk_gesture_is_active:
 * @gesture: a #GtkGesture
 *
 * Returns %TRUE if the gesture is currently active.
 * A gesture is active meanwhile there are touch sequences
 * interacting with it.
 *
 * Returns: %TRUE if gesture is active
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_is_active (GtkGesture *gesture)
{
  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  return _gtk_gesture_effective_n_points (gesture) != 0;
}

/**
 * gtk_gesture_is_recognized:
 * @gesture: a #GtkGesture
 *
 * Returns %TRUE if the gesture is currently recognized.
 * A gesture is recognized if there are as many interacting
 * touch sequences as required by @gesture, and #GtkGesture::check
 * returned %TRUE for the sequences being currently interpreted.
 *
 * Returns: %TRUE if gesture is recognized
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_is_recognized (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->recognized;
}

gboolean
_gtk_gesture_check (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  return _gtk_gesture_check_recognized (gesture, priv->last_sequence);
}

/**
 * gtk_gesture_handles_sequence:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Returns %TRUE if @gesture is currently handling events corresponding to
 * @sequence.
 *
 * Returns: %TRUE if @gesture is handling @sequence
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_handles_sequence (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  if (data->state == GTK_EVENT_SEQUENCE_DENIED)
    return FALSE;

  return TRUE;
}

gboolean
_gtk_gesture_cancel_sequence (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  g_signal_emit (gesture, signals[CANCEL], 0, sequence);
  _gtk_gesture_check_recognized (gesture, sequence);
  _gtk_gesture_remove_point (gesture, data->event);
  return TRUE;
}

/**
 * gtk_gesture_get_window:
 * @gesture: a #GtkGesture
 *
 * Returns the user-defined window that receives the events
 * handled by @gesture. See gtk_gesture_set_window() for more
 * information.
 *
 * Returns: (transfer none) (allow-none): the user defined window, or %NULL if none
 *
 * Since: 3.14
 **/
GdkWindow *
gtk_gesture_get_window (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->user_window;
}

/**
 * gtk_gesture_set_window:
 * @gesture: a #GtkGesture
 * @window: (allow-none): a #GdkWindow, or %NULL
 *
 * Sets a specific window to receive events about, so @gesture
 * will effectively handle only events targeting @window, or
 * a child of it. @window must pertain to gtk_event_controller_get_widget().
 *
 * Since: 3.14
 **/
void
gtk_gesture_set_window (GtkGesture *gesture,
                        GdkWindow  *window)
{
  GtkGesturePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  priv = gtk_gesture_get_instance_private (gesture);

  if (window)
    {
      GtkWidget *window_widget;

      gdk_window_get_user_data (window, (gpointer*) &window_widget);
      g_return_if_fail (window_widget ==
                        gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));
    }

  if (priv->user_window == window)
    return;

  priv->user_window = window;
  g_object_notify (G_OBJECT (gesture), "window");
}

GList *
_gtk_gesture_get_group_link (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->group_link;
}

/**
 * gtk_gesture_group:
 * @gesture: a #GtkGesture
 * @group_gesture: #GtkGesture to group @gesture with
 *
 * Adds @gesture to the same group than @group_gesture. Gestures
 * are by default isolated in their own groups.
 *
 * When gestures are grouped, the state of #GdkEventSequences
 * is kept in sync for all of those, so calling gtk_gesture_set_sequence_state(),
 * on one will transfer the same value to the others.
 *
 * Groups also perform an "implicit grabbing" of sequences, if a
 * #GdkEventSequence state is set to #GTK_EVENT_SEQUENCE_CLAIMED on one group,
 * every other gesture group attached to the same #GtkWidget will switch the
 * state for that sequence to #GTK_EVENT_SEQUENCE_DENIED.
 *
 * Since: 3.14
 **/
void
gtk_gesture_group (GtkGesture *gesture,
                   GtkGesture *group_gesture)
{
  GList *link, *group_link, *next;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (GTK_IS_GESTURE (group_gesture));
  g_return_if_fail (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (group_gesture)) ==
                    gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  link = _gtk_gesture_get_group_link (gesture);

  if (link->prev || link->next)
    {
      if (gtk_gesture_is_grouped_with (gesture, group_gesture))
        return;
      gtk_gesture_ungroup (gesture);
    }

  group_link = _gtk_gesture_get_group_link (group_gesture);
  next = group_link->next;

  /* Rewire link so it's inserted after the group_gesture elem */
  link->prev = group_link;
  link->next = next;
  group_link->next = link;
  if (next)
    next->prev = link;
}

/**
 * gtk_gesture_ungroup:
 * @gesture: a #GtkGesture
 *
 * Separates @gesture into an isolated group.
 *
 * Since: 3.14
 **/
void
gtk_gesture_ungroup (GtkGesture *gesture)
{
  GList *link, *prev, *next;

  g_return_if_fail (GTK_IS_GESTURE (gesture));

  link = _gtk_gesture_get_group_link (gesture);
  prev = link->prev;
  next = link->next;

  /* Detach link from the group chain */
  if (prev)
    prev->next = next;
  if (next)
    next->prev = prev;

  link->next = link->prev = NULL;
}

/**
 * gtk_gesture_get_group:
 * @gesture: a #GtkGesture
 *
 * Returns all gestures in the group of @gesture
 *
 * Returns: (element-type GtkGesture) (transfer container): The list
 *   of #GtkGestures, free with g_list_free()
 *
 * Since: 3.14
 **/
GList *
gtk_gesture_get_group (GtkGesture *gesture)
{
  GList *link;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  link = _gtk_gesture_get_group_link (gesture);

  return g_list_copy (g_list_first (link));
}

/**
 * gtk_gesture_is_grouped_with:
 * @gesture: a #GtkGesture
 * @other: another #GtkGesture
 *
 * Returns %TRUE if both gestures pertain to the same group.
 *
 * Returns: whether the gestures are grouped
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_is_grouped_with (GtkGesture *gesture,
                             GtkGesture *other)
{
  GList *link;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (GTK_IS_GESTURE (other), FALSE);

  link = _gtk_gesture_get_group_link (gesture);
  link = g_list_first (link);

  return g_list_find (link, other) != NULL;
}

gboolean
_gtk_gesture_handled_sequence_press (GtkGesture       *gesture,
                                     GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  return data->press_handled;
}

gboolean
_gtk_gesture_get_pointer_emulating_sequence (GtkGesture        *gesture,
                                             GdkEventSequence **sequence)
{
  GtkGesturePrivate *priv;
  GdkEventSequence *seq;
  GHashTableIter iter;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer*) &seq, (gpointer*) &data))
    {
      switch (data->event->type)
        {
        case GDK_TOUCH_BEGIN:
        case GDK_TOUCH_UPDATE:
        case GDK_TOUCH_END:
          if (!data->event->touch.emulating_pointer)
            continue;
          /* Fall through */
        case GDK_BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
        case GDK_MOTION_NOTIFY:
          *sequence = seq;
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}
