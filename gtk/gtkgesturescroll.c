/* GTK - The GIMP Toolkit
 * Copyright (C) 202, Purism SPC
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
 * Author(s): Alexander Mikhaylenko <alexm@gnome.org>
 */

/**
 * GtkGestureScroll:
 *
 * `GtkGestureScroll` is a `GtkGesture` implementation for scrolls.
 *
 * The scroll operation itself can be tracked throughout the
 * [signal@Gtk.GestureScroll::scroll-begin],
 * [signal@Gtk.GestureScroll::scroll-update] and
 * [signal@Gtk.GestureScroll::scroll-end] signals, and the relevant
 * coordinates can be extracted through
 * [method@Gtk.GestureScroll.get_offset] and
 * [method@Gtk.GestureScroll.get_start_point].
 */
#include "config.h"
#include "gtkgestureprivate.h"
#include "gtkgesturescrollprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"

typedef struct _GtkGestureScrollPrivate GtkGestureScrollPrivate;
typedef struct _EventData EventData;

struct _GtkGestureScrollPrivate
{
  double start_x;
  double start_y;
  double last_x;
  double last_y;
};

enum {
  SCROLL_BEGIN,
  SCROLL,
  SCROLL_END,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureScroll, gtk_gesture_scroll, GTK_TYPE_GESTURE)

static GtkFilterEventStatus
gtk_gesture_scroll_filter_event (GtkEventController *controller,
                                 GdkEvent           *event)
{
  GdkEventType event_type = gdk_event_get_event_type (event);

  if (event_type == GDK_TOUCHPAD_HOLD)
    {
      int n_fingers = gdk_touchpad_event_get_n_fingers (event);

      if (n_fingers == 1 || n_fingers == 2)
        return GTK_EVENT_HANDLE;
    }

  if (event_type == GDK_SCROLL ||
      event_type == GDK_GRAB_BROKEN)
    return GTK_EVENT_HANDLE;

  return GTK_EVENT_SKIP;
}

static void
gtk_gesture_scroll_begin (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  GtkGestureScrollPrivate *priv;
//  GdkEventSequence *current;

//  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  priv = gtk_gesture_scroll_get_instance_private (GTK_GESTURE_SCROLL (gesture));
  gtk_gesture_get_point (gesture, NULL, &priv->last_x, &priv->last_y);

  g_signal_emit (gesture, signals[SCROLL_BEGIN], 0);
}

static void
gtk_gesture_scroll_update (GtkGesture       *gesture,
                           GdkEventSequence *sequence)
{
  GtkGestureScrollPrivate *priv;
  double x, y;

  priv = gtk_gesture_scroll_get_instance_private (GTK_GESTURE_SCROLL (gesture));
  gtk_gesture_get_point (gesture, NULL, &x, &y);

  g_signal_emit (gesture, signals[SCROLL], 0, x - priv->last_x, y - priv->last_y);

  priv->last_x = x;
  priv->last_y = y;
}

static void
gtk_gesture_scroll_end (GtkGesture       *gesture,
                        GdkEventSequence *sequence)
{
  g_signal_emit (gesture, signals[SCROLL_END], 0);
}

static GObject *
gtk_gesture_scroll_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
  GObject *object;

  object = G_OBJECT_CLASS (gtk_gesture_scroll_parent_class)->constructor (type,
                                                                          n_construct_properties,
                                                                          construct_properties);
  g_object_set (object, "n-points", 2, NULL);

  return object;
}

static void
gtk_gesture_scroll_class_init (GtkGestureScrollClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->constructor = gtk_gesture_scroll_constructor;

  event_controller_class->filter_event = gtk_gesture_scroll_filter_event;

  gesture_class->begin = gtk_gesture_scroll_begin;
  gesture_class->update = gtk_gesture_scroll_update;
  gesture_class->end = gtk_gesture_scroll_end;

  /**
   * GtkGestureScroll::scroll-begin:
   * @gesture: the object which received the signal
   *
   * Emitted whenever scrollging starts.
   */
  signals[SCROLL_BEGIN] =
    g_signal_new (I_("scroll-begin"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * GtkGestureScroll::scroll:
   * @gesture: the object which received the signal
   * @dx: X delta
   * @dy: Y delta
   *
   * Signals that the widget should scroll by the
   * amount specified by @dx and @dy.
   *
   * For the representation unit of the deltas, see
   * [method@Gtk.EventControllerScroll.get_unit].
   */
  signals[SCROLL] =
    g_signal_new (I_("scroll"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[SCROLL],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
  /**
   * GtkGestureScroll::scroll-end:
   * @gesture: the object which received the signal
   *
   * Emitted whenever the scrollging is finished.
   */
  signals[SCROLL_END] =
    g_signal_new (I_("scroll-end"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_gesture_scroll_init (GtkGestureScroll *gesture)
{
}

/**
 * gtk_gesture_scroll_new:
 *
 * Returns a newly created `GtkGesture` that recognizes scrolls.
 *
 * Returns: a newly created `GtkGestureScroll`
 **/
GtkGesture *
gtk_gesture_scroll_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_SCROLL,
                       NULL);
}
