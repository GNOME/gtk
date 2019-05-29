/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
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
 * SECTION:gtkgestureclick
 * @Short_description: Multipress gesture
 * @Title: GtkGestureClick
 *
 * #GtkGestureClick is a #GtkGesture implementation able to recognize
 * multiple clicks on a nearby zone, which can be listened for through
 * the #GtkGestureClick::pressed signal. Whenever time or distance
 * between clicks exceed the GTK+ defaults, #GtkGestureClick::stopped
 * is emitted, and the click counter is reset.
 *
 * Callers may also restrict the area that is considered valid for a >1
 * touch/button press through gtk_gesture_click_set_area(), so any
 * click happening outside that area is considered to be a first click
 * of its own.
 */

#include "config.h"
#include "gtkgestureprivate.h"
#include "gtkgestureclick.h"
#include "gtkgestureclickprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef struct _GtkGestureClickPrivate GtkGestureClickPrivate;

struct _GtkGestureClickPrivate
{
  GdkRectangle rect;
  GdkDevice *current_device;
  gdouble initial_press_x;
  gdouble initial_press_y;
  guint double_click_timeout_id;
  guint n_presses;
  guint n_release;
  guint current_button;
  guint rect_is_set : 1;
};

enum {
  PRESSED,
  RELEASED,
  STOPPED,
  UNPAIRED_RELEASE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureClick, gtk_gesture_click, GTK_TYPE_GESTURE_SINGLE)

static void
gtk_gesture_click_finalize (GObject *object)
{
  GtkGestureClickPrivate *priv;
  GtkGestureClick *gesture;

  gesture = GTK_GESTURE_CLICK (object);
  priv = gtk_gesture_click_get_instance_private (gesture);

  if (priv->double_click_timeout_id)
    {
      g_source_remove (priv->double_click_timeout_id);
      priv->double_click_timeout_id = 0;
    }

  G_OBJECT_CLASS (gtk_gesture_click_parent_class)->finalize (object);
}

static gboolean
gtk_gesture_click_check (GtkGesture *gesture)
{
  GtkGestureClick *click;
  GtkGestureClickPrivate *priv;
  GList *sequences;
  gboolean active;

  click = GTK_GESTURE_CLICK (gesture);
  priv = gtk_gesture_click_get_instance_private (click);
  sequences = gtk_gesture_get_sequences (gesture);

  active = g_list_length (sequences) == 1 || priv->double_click_timeout_id;
  g_list_free (sequences);

  return active;
}

static void
_gtk_gesture_click_stop (GtkGestureClick *gesture)
{
  GtkGestureClickPrivate *priv;

  priv = gtk_gesture_click_get_instance_private (gesture);

  if (priv->n_presses == 0)
    return;

  priv->current_device = NULL;
  priv->current_button = 0;
  priv->n_presses = 0;
  g_signal_emit (gesture, signals[STOPPED], 0);
  _gtk_gesture_check (GTK_GESTURE (gesture));
}

static gboolean
_double_click_timeout_cb (gpointer user_data)
{
  GtkGestureClick *gesture = user_data;
  GtkGestureClickPrivate *priv;

  priv = gtk_gesture_click_get_instance_private (gesture);
  priv->double_click_timeout_id = 0;
  _gtk_gesture_click_stop (gesture);

  return FALSE;
}

static void
_gtk_gesture_click_update_timeout (GtkGestureClick *gesture)
{
  GtkGestureClickPrivate *priv;
  guint double_click_time;
  GtkSettings *settings;
  GtkWidget *widget;

  priv = gtk_gesture_click_get_instance_private (gesture);

  if (priv->double_click_timeout_id)
    g_source_remove (priv->double_click_timeout_id);

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  settings = gtk_widget_get_settings (widget);
  g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);

  priv->double_click_timeout_id = g_timeout_add (double_click_time, _double_click_timeout_cb, gesture);
  g_source_set_name_by_id (priv->double_click_timeout_id, "[gtk] _double_click_timeout_cb");
}

static gboolean
_gtk_gesture_click_check_within_threshold (GtkGestureClick *gesture,
                                           double           x,
                                           double           y)
{
  GtkGestureClickPrivate *priv;
  guint double_click_distance;
  GtkSettings *settings;
  GtkWidget *widget;

  priv = gtk_gesture_click_get_instance_private (gesture);

  if (priv->n_presses == 0)
    return TRUE;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  settings = gtk_widget_get_settings (widget);
  g_object_get (settings,
                "gtk-double-click-distance", &double_click_distance,
                NULL);

  if (ABS (priv->initial_press_x - x) < double_click_distance &&
      ABS (priv->initial_press_y - y) < double_click_distance)
    {
      if (!priv->rect_is_set ||
          (x >= priv->rect.x && x < priv->rect.x + priv->rect.width &&
           y >= priv->rect.y && y < priv->rect.y + priv->rect.height))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_gesture_click_begin (GtkGesture       *gesture,
                         GdkEventSequence *sequence)
{
  GtkGestureClick *click;
  GtkGestureClickPrivate *priv;
  guint n_presses, button = 1;
  GdkEventSequence *current;
  const GdkEvent *event;
  GdkEventType event_type;
  GdkDevice *device;
  gdouble x, y;

  if (!gtk_gesture_handles_sequence (gesture, sequence))
    return;

  click = GTK_GESTURE_CLICK (gesture);
  priv = gtk_gesture_click_get_instance_private (click);
  event = gtk_gesture_get_last_event (gesture, sequence);
  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  device = gdk_event_get_source_device (event);
  event_type = gdk_event_get_event_type (event);

  if (event_type == GDK_BUTTON_PRESS)
    gdk_event_get_button (event, &button);
  else if (event_type == GDK_TOUCH_BEGIN)
    button = 1;
  else
    return;

  /* Reset the gesture if the button number changes mid-recognition */
  if (priv->n_presses > 0 &&
      priv->current_button != button)
    _gtk_gesture_click_stop (click);

  /* Reset also if the device changed */
  if (priv->current_device && priv->current_device != device)
    _gtk_gesture_click_stop (click);

  priv->current_device = device;
  priv->current_button = button;
  _gtk_gesture_click_update_timeout (click);
  gtk_gesture_get_point (gesture, current, &x, &y);

  if (!_gtk_gesture_click_check_within_threshold (click, x, y))
    _gtk_gesture_click_stop (click);

  /* Increment later the real counter, just if the gesture is
   * reset on the pressed handler */
  n_presses = priv->n_release = priv->n_presses + 1;

  g_signal_emit (gesture, signals[PRESSED], 0, n_presses, x, y);

  if (priv->n_presses == 0)
    {
      priv->initial_press_x = x;
      priv->initial_press_y = y;
    }

  priv->n_presses++;
}

static void
gtk_gesture_click_update (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  GtkGestureClick *click;
  GdkEventSequence *current;
  gdouble x, y;

  click = GTK_GESTURE_CLICK (gesture);
  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_get_point (gesture, current, &x, &y);

  if (!_gtk_gesture_click_check_within_threshold (click, x, y))
    _gtk_gesture_click_stop (click);
}

static void
gtk_gesture_click_end (GtkGesture       *gesture,
                       GdkEventSequence *sequence)
{
  GtkGestureClick *click;
  GtkGestureClickPrivate *priv;
  GdkEventSequence *current;
  gdouble x, y;
  gboolean interpreted;
  GtkEventSequenceState state;

  click = GTK_GESTURE_CLICK (gesture);
  priv = gtk_gesture_click_get_instance_private (click);
  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  interpreted = gtk_gesture_get_point (gesture, current, &x, &y);
  state = gtk_gesture_get_sequence_state (gesture, current);

  if (state != GTK_EVENT_SEQUENCE_DENIED && interpreted)
    g_signal_emit (gesture, signals[RELEASED], 0, priv->n_release, x, y);

  priv->n_release = 0;
}

static void
gtk_gesture_click_cancel (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  _gtk_gesture_click_stop (GTK_GESTURE_CLICK (gesture));
  GTK_GESTURE_CLASS (gtk_gesture_click_parent_class)->cancel (gesture, sequence);
}

static void
gtk_gesture_click_reset (GtkEventController *controller)
{
  _gtk_gesture_click_stop (GTK_GESTURE_CLICK (controller));
  GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_click_parent_class)->reset (controller);
}

static gboolean
gtk_gesture_click_handle_event (GtkEventController *controller,
                                const GdkEvent     *event)
{
  GtkEventControllerClass *parent_controller;
  GtkGestureClickPrivate *priv;
  GdkEventSequence *sequence;
  guint button;
  gdouble x, y;

  priv = gtk_gesture_click_get_instance_private (GTK_GESTURE_CLICK (controller));
  parent_controller = GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_click_parent_class);
  sequence = gdk_event_get_event_sequence (event);

  if (priv->n_presses == 0 &&
      !gtk_gesture_handles_sequence (GTK_GESTURE (controller), sequence) &&
      (gdk_event_get_event_type (event) == GDK_BUTTON_RELEASE ||
       gdk_event_get_event_type (event) == GDK_TOUCH_END))
    {
      if (!gdk_event_get_button (event, &button))
        button = 0;
      gdk_event_get_coords (event, &x, &y);
      g_signal_emit (controller, signals[UNPAIRED_RELEASE], 0,
                     x, y, button, sequence);
    }

  return parent_controller->handle_event (controller, event);
}

static void
gtk_gesture_click_class_init (GtkGestureClickClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->finalize = gtk_gesture_click_finalize;

  gesture_class->check = gtk_gesture_click_check;
  gesture_class->begin = gtk_gesture_click_begin;
  gesture_class->update = gtk_gesture_click_update;
  gesture_class->end = gtk_gesture_click_end;
  gesture_class->cancel = gtk_gesture_click_cancel;

  controller_class->reset = gtk_gesture_click_reset;
  controller_class->handle_event = gtk_gesture_click_handle_event;

  /**
   * GtkGestureClick::pressed:
   * @gesture: the object which received the signal
   * @n_press: how many touch/button presses happened with this one
   * @x: The X coordinate, in widget allocation coordinates
   * @y: The Y coordinate, in widget allocation coordinates
   *
   * This signal is emitted whenever a button or touch press happens.
   */
  signals[PRESSED] =
    g_signal_new (I_("pressed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClickClass, pressed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_INT,
                  G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  /**
   * GtkGestureClick::released:
   * @gesture: the object which received the signal
   * @n_press: number of press that is paired with this release
   * @x: The X coordinate, in widget allocation coordinates
   * @y: The Y coordinate, in widget allocation coordinates
   *
   * This signal is emitted when a button or touch is released. @n_press
   * will report the number of press that is paired to this event, note
   * that #GtkGestureClick::stopped may have been emitted between the
   * press and its release, @n_press will only start over at the next press.
   */
  signals[RELEASED] =
    g_signal_new (I_("released"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClickClass, released),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_INT,
                  G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  /**
   * GtkGestureClick::stopped:
   * @gesture: the object which received the signal
   *
   * This signal is emitted whenever any time/distance threshold has
   * been exceeded.
   */
  signals[STOPPED] =
    g_signal_new (I_("stopped"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClickClass, stopped),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkGestureClick::unpaired-release
   * @gesture: the object which received the signal
   * @x: X coordinate of the event
   * @y: Y coordinate of the event
   * @button: Button being released
   * @sequence: Sequence being released
   *
   * This signal is emitted whenever the gesture receives a release
   * event that had no previous corresponding press. Due to implicit
   * grabs, this can only happen on situations where input is grabbed
   * elsewhere mid-press or the pressed widget voluntarily relinquishes
   * its implicit grab.
   */
  signals[UNPAIRED_RELEASE] =
    g_signal_new (I_("unpaired-release"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 4,
                  G_TYPE_DOUBLE, G_TYPE_DOUBLE,
                  G_TYPE_UINT, GDK_TYPE_EVENT_SEQUENCE);
}

static void
gtk_gesture_click_init (GtkGestureClick *gesture)
{
}

/**
 * gtk_gesture_click_new:
 *
 * Returns a newly created #GtkGesture that recognizes single and multiple
 * presses.
 *
 * Returns: a newly created #GtkGestureClick
 **/
GtkGesture *
gtk_gesture_click_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_CLICK,
                       NULL);
}

/**
 * gtk_gesture_click_set_area:
 * @gesture: a #GtkGestureClick
 * @rect: (allow-none): rectangle to receive coordinates on
 *
 * If @rect is non-%NULL, the press area will be checked to be
 * confined within the rectangle, otherwise the button count
 * will be reset so the press is seen as being the first one.
 * If @rect is %NULL, the area will be reset to an unrestricted
 * state.
 *
 * Note: The rectangle is only used to determine whether any
 * non-first click falls within the expected area. This is not
 * akin to an input shape.
 **/
void
gtk_gesture_click_set_area (GtkGestureClick    *gesture,
                            const GdkRectangle *rect)
{
  GtkGestureClickPrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_CLICK (gesture));

  priv = gtk_gesture_click_get_instance_private (gesture);

  if (!rect)
    priv->rect_is_set = FALSE;
  else
    {
      priv->rect_is_set = TRUE;
      priv->rect = *rect;
    }
}

/**
 * gtk_gesture_click_get_area:
 * @gesture: a #GtkGestureClick
 * @rect: (out): return location for the press area
 *
 * If an area was set through gtk_gesture_click_set_area(),
 * this function will return %TRUE and fill in @rect with the
 * press area. See gtk_gesture_click_set_area() for more
 * details on what the press area represents.
 *
 * Returns: %TRUE if @rect was filled with the press area
 **/
gboolean
gtk_gesture_click_get_area (GtkGestureClick *gesture,
                            GdkRectangle    *rect)
{
  GtkGestureClickPrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_CLICK (gesture), FALSE);

  priv = gtk_gesture_click_get_instance_private (gesture);

  if (rect)
    {
      if (priv->rect_is_set)
        *rect = priv->rect;
      else
        {
          rect->x = rect->y = G_MININT;
          rect->width = rect->height = G_MAXINT;
        }
    }

  return priv->rect_is_set;
}
