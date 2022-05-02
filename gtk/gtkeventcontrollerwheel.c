/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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
 * GtkEventControllerWheel:
 *
 * `GtkEventControllerWheel` is an event controller that handles mouse scroll
 * events.
 *
 * It is capable of handling both discrete and continuous scroll
 * events from mice or touchpads, abstracting them both with the
 * [signal@Gtk.EventControllerScroll::scroll] signal. Deltas in
 * the discrete case are multiples of 1.
 *
 * In the case of continuous scroll events, `GtkEventControllerScroll`
 * encloses all [signal@Gtk.EventControllerScroll::scroll] emissions
 * between two [signal@Gtk.EventControllerScroll::scroll-begin] and
 * [signal@Gtk.EventControllerScroll::scroll-end] signals.
 *
 * The behavior of the event controller can be modified by the flags
 * given at creation time, or modified at a later point through
 * [method@Gtk.EventControllerScroll.set_flags] (e.g. because the scrolling
 * conditions of the widget changed).
 *
 * The controller can be set up to emit motion for either/both vertical
 * and horizontal scroll events through %GTK_EVENT_CONTROLLER_SCROLL_VERTICAL,
 * %GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL and %GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES.
 * If any axis is disabled, the respective [signal@Gtk.EventControllerScroll::scroll]
 * delta will be 0. Vertical scroll events will be translated to horizontal
 * motion for the devices incapable of horizontal scrolling.
 *
 * The event controller can also be forced to emit discrete events on all
 * devices through %GTK_EVENT_CONTROLLER_SCROLL_DISCRETE. This can be used
 * to implement discrete actions triggered through scroll events (e.g.
 * switching across combobox options).
 *
 * The %GTK_EVENT_CONTROLLER_SCROLL_KINETIC flag toggles the emission of the
 * [signal@Gtk.EventControllerScroll::decelerate] signal, emitted at the end
 * of scrolling with two X/Y velocity arguments that are consistent with the
 * motion that was received.
 *
 * Since: 4.8
 */
#include "config.h"

#include "gtkintl.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollerwheel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

struct _GtkEventControllerWheel
{
  GtkEventController parent_instance;

  GdkScrollUnit cur_unit;
};

struct _GtkEventControllerWheelClass
{
  GtkEventControllerClass parent_class;
};

enum {
  SCROLL,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkEventControllerWheel, gtk_event_controller_wheel,
               GTK_TYPE_EVENT_CONTROLLER)

static GtkFilterEventStatus
gtk_event_controller_wheel_filter_event (GtkEventController *controller,
                                         GdkEvent           *event)
{
  GdkEventType event_type = gdk_event_get_event_type (event);

  if (event_type == GDK_SCROLL) {
    GdkDevice *source_device = gdk_event_get_device (event);
    GdkInputSource source = gdk_device_get_source (source_device);

    if (source == GDK_SOURCE_MOUSE)
      return GTK_EVENT_HANDLE;
  }

  return GTK_EVENT_SKIP;
}

static gboolean
gtk_event_controller_wheel_handle_event (GtkEventController *controller,
                                         GdkEvent           *event,
                                         double              x,
                                         double              y)
{
  GtkEventControllerWheel *self = GTK_EVENT_CONTROLLER_WHEEL (controller);
  GdkScrollDirection direction = GDK_SCROLL_SMOOTH;
  double dx = 0, dy = 0;
  gboolean handled = GDK_EVENT_PROPAGATE;

  /* FIXME: Handle device changes */
  direction = gdk_scroll_event_get_direction (event);
  if (direction == GDK_SCROLL_SMOOTH)
    gdk_scroll_event_get_deltas (event, &dx, &dy);
  else
    {
      switch (direction)
        {
        case GDK_SCROLL_UP:
          dy -= 1;
          break;
        case GDK_SCROLL_DOWN:
          dy += 1;
          break;
        case GDK_SCROLL_LEFT:
          dx -= 1;
          break;
        case GDK_SCROLL_RIGHT:
          dx += 1;
          break;
        case GDK_SCROLL_SMOOTH:
        default:
          g_assert_not_reached ();
          break;
        }
    }

  if (dx != 0 || dy != 0)
    g_signal_emit (controller, signals[SCROLL], 0, dx, dy, &handled);

  self->cur_unit = gdk_scroll_event_get_unit (event);

  return handled;
}

static void
gtk_event_controller_wheel_class_init (GtkEventControllerWheelClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  controller_class->filter_event = gtk_event_controller_wheel_filter_event;
  controller_class->handle_event = gtk_event_controller_wheel_handle_event;

  /**
   * GtkEventControllerWheel:scroll:
   * @controller: The object that received the signal
   * @dx: X delta
   * @dy: Y delta
   *
   * Signals that the widget should scroll by the
   * amount specified by @dx and @dy.
   *
   * Returns: %TRUE if the scroll event was handled,
   *   %FALSE otherwise.
   */
  signals[SCROLL] =
    g_signal_new (I_("scroll"),
                  GTK_TYPE_EVENT_CONTROLLER_WHEEL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _gtk_marshal_BOOLEAN__DOUBLE_DOUBLE,
                  G_TYPE_BOOLEAN, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[SCROLL],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__DOUBLE_DOUBLEv);
}

static void
gtk_event_controller_wheel_init (GtkEventControllerWheel *self)
{
}

/**
 * gtk_event_controller_wheel_new:
 *
 * Creates a new event controller that will handle scroll events.
 *
 * Returns: a new `GtkEventControllerWheel`
 *
 * Since: 4.8
 */
GtkEventController *
gtk_event_controller_wheel_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_WHEEL,
                       NULL);
}

/**
 * gtk_event_controller_wheel_get_unit:
 * @self: a `GtkEventControllerWheel`.
 *
 * Gets the scroll unit of the last
 * [signal@Gtk.GtkEventControllerWheel::scroll] signal received.
 *
 * Always returns %GDK_SCROLL_UNIT_WHEEL if the
 * %GTK_EVENT_CONTROLLER_SCROLL_DISCRETE flag is set.
 *
 * Returns: the scroll unit.
 *
 * Since: 4.8
 */
GdkScrollUnit
gtk_event_controller_wheel_get_unit (GtkEventControllerWheel *self)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_WHEEL (self),
                        GDK_SCROLL_UNIT_WHEEL);

  return self->cur_unit;
}
