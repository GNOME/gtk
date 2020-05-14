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
 * SECTION:gtkeventcontrollerscroll
 * @Short_description: Event controller for scroll events
 * @Title: GtkEventControllerScroll
 * @See_also: #GtkEventController
 *
 * #GtkEventControllerScroll is an event controller meant to handle
 * scroll events from mice and touchpads. It is capable of handling
 * both discrete and continuous scroll events, abstracting them both
 * on the #GtkEventControllerScroll::scroll signal (deltas in the
 * discrete case are multiples of 1).
 *
 * In the case of continuous scroll events, #GtkEventControllerScroll
 * encloses all #GtkEventControllerScroll::scroll events between two
 * #GtkEventControllerScroll::scroll-begin and #GtkEventControllerScroll::scroll-end
 * signals.
 *
 * The behavior of the event controller can be modified by the
 * flags given at creation time, or modified at a later point through
 * gtk_event_controller_scroll_set_flags() (e.g. because the scrolling
 * conditions of the widget changed).
 *
 * The controller can be set up to emit motion for either/both vertical
 * and horizontal scroll events through #GTK_EVENT_CONTROLLER_SCROLL_VERTICAL,
 * #GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL and #GTK_EVENT_CONTROLLER_SCROLL_BOTH.
 * If any axis is disabled, the respective #GtkEventControllerScroll::scroll
 * delta will be 0. Vertical scroll events will be translated to horizontal
 * motion for the devices incapable of horizontal scrolling.
 *
 * The event controller can also be forced to emit discrete events on all devices
 * through #GTK_EVENT_CONTROLLER_SCROLL_DISCRETE. This can be used to implement
 * discrete actions triggered through scroll events (e.g. switching across
 * combobox options).
 *
 * The #GTK_EVENT_CONTROLLER_SCROLL_KINETIC flag toggles the emission of the
 * #GtkEventControllerScroll::decelerate signal, emitted at the end of scrolling
 * with two X/Y velocity arguments that are consistent with the motion that
 * was received.
 *
 * This object was added in 3.24.
 **/
#include "config.h"

#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollerscroll.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

#include "fallback-c89.c"

#define SCROLL_CAPTURE_THRESHOLD_MS 150

typedef struct
{
  gdouble dx;
  gdouble dy;
  guint32 evtime;
} ScrollHistoryElem;

struct _GtkEventControllerScroll
{
  GtkEventController parent_instance;
  GtkEventControllerScrollFlags flags;
  GArray *scroll_history;

  /* For discrete event coalescing */
  gdouble cur_dx;
  gdouble cur_dy;

  guint active : 1;
};

struct _GtkEventControllerScrollClass
{
  GtkEventControllerClass parent_class;
};

enum {
  SCROLL_BEGIN,
  SCROLL,
  SCROLL_END,
  DECELERATE,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_FLAGS,
  N_PROPS
};

static GParamSpec *pspecs[N_PROPS] = { NULL };
static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkEventControllerScroll, gtk_event_controller_scroll,
               GTK_TYPE_EVENT_CONTROLLER)

static void
scroll_history_push (GtkEventControllerScroll *scroll,
                     gdouble                   delta_x,
                     gdouble                   delta_y,
                     guint32                   evtime)
{
  ScrollHistoryElem new_item;
  guint i;

  for (i = 0; i < scroll->scroll_history->len; i++)
    {
      ScrollHistoryElem *elem;

      elem = &g_array_index (scroll->scroll_history, ScrollHistoryElem, i);

      if (elem->evtime >= evtime - SCROLL_CAPTURE_THRESHOLD_MS)
        break;
    }

  if (i > 0)
    g_array_remove_range (scroll->scroll_history, 0, i);

  new_item.dx = delta_x;
  new_item.dy = delta_y;
  new_item.evtime = evtime;
  g_array_append_val (scroll->scroll_history, new_item);
}

static void
scroll_history_reset (GtkEventControllerScroll *scroll)
{
  if (scroll->scroll_history->len == 0)
    return;

  g_array_remove_range (scroll->scroll_history, 0,
                        scroll->scroll_history->len);
}

static void
scroll_history_finish (GtkEventControllerScroll *scroll,
                       gdouble                  *velocity_x,
                       gdouble                  *velocity_y)
{
  gdouble accum_dx = 0, accum_dy = 0;
  guint32 first = 0, last = 0;
  guint i;

  *velocity_x = 0;
  *velocity_y = 0;

  if (scroll->scroll_history->len == 0)
    return;

  for (i = 0; i < scroll->scroll_history->len; i++)
    {
      ScrollHistoryElem *elem;

      elem = &g_array_index (scroll->scroll_history, ScrollHistoryElem, i);
      accum_dx += elem->dx;
      accum_dy += elem->dy;
      last = elem->evtime;

      if (i == 0)
        first = elem->evtime;
    }

  if (last != first)
    {
      *velocity_x = (accum_dx * 1000) / (last - first);
      *velocity_y = (accum_dy * 1000) / (last - first);
    }

  scroll_history_reset (scroll);
}

static void
gtk_event_controller_scroll_finalize (GObject *object)
{
  GtkEventControllerScroll *scroll = GTK_EVENT_CONTROLLER_SCROLL (object);

  g_array_unref (scroll->scroll_history);

  G_OBJECT_CLASS (gtk_event_controller_scroll_parent_class)->finalize (object);
}

static void
gtk_event_controller_scroll_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GtkEventControllerScroll *scroll = GTK_EVENT_CONTROLLER_SCROLL (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      gtk_event_controller_scroll_set_flags (scroll, g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_event_controller_scroll_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkEventControllerScroll *scroll = GTK_EVENT_CONTROLLER_SCROLL (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      g_value_set_flags (value, scroll->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_event_controller_scroll_handle_event (GtkEventController *controller,
                                          const GdkEvent     *event)
{
  GtkEventControllerScroll *scroll = GTK_EVENT_CONTROLLER_SCROLL (controller);
  GdkScrollDirection direction = GDK_SCROLL_SMOOTH;
  gdouble dx = 0, dy = 0;

  if (gdk_event_get_event_type (event) != GDK_SCROLL)
    return FALSE;
  if ((scroll->flags & (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                        GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL)) == 0)
    return FALSE;

  /* FIXME: Handle device changes */

  if (gdk_event_get_scroll_deltas (event, &dx, &dy))
    {
      GdkDevice *device = gdk_event_get_source_device (event);
      GdkInputSource input_source = gdk_device_get_source (device);

      if (!scroll->active &&
          (input_source == GDK_SOURCE_TRACKPOINT ||
           input_source == GDK_SOURCE_TOUCHPAD))
        {
          g_signal_emit (controller, signals[SCROLL_BEGIN], 0);
          scroll_history_reset (scroll);
          scroll->active = TRUE;
        }

      if ((scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_VERTICAL) == 0)
        dy = 0;
      if ((scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL) == 0)
        dx = 0;

      if (scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_DISCRETE)
        {
          gint steps;

          scroll->cur_dx += dx;
          scroll->cur_dy += dy;
          dx = dy = 0;

          if (ABS (scroll->cur_dx) >= 1)
            {
              steps = trunc (scroll->cur_dx);
              scroll->cur_dx -= steps;
              dx = steps;
            }

          if (ABS (scroll->cur_dy) >= 1)
            {
              steps = trunc (scroll->cur_dy);
              scroll->cur_dy -= steps;
              dy = steps;
            }
        }
    }
  else if (gdk_event_get_scroll_direction (event, &direction))
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
        default:
          g_assert_not_reached ();
          break;
        }

      if ((scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_VERTICAL) == 0)
        dy = 0;
      if ((scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL) == 0)
        dx = 0;
    }

  if (dx != 0 || dy != 0)
    g_signal_emit (controller, signals[SCROLL], 0, dx, dy);

  if (direction == GDK_SCROLL_SMOOTH &&
      scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_KINETIC)
    scroll_history_push (scroll, dx, dy, gdk_event_get_time (event));

  if (scroll->active && gdk_event_is_scroll_stop_event (event))
    {
      g_signal_emit (controller, signals[SCROLL_END], 0);
      scroll->active = FALSE;

      if (scroll->flags & GTK_EVENT_CONTROLLER_SCROLL_KINETIC)
        {
          gdouble vel_x, vel_y;

          scroll_history_finish (scroll, &vel_x, &vel_y);
          g_signal_emit (controller, signals[DECELERATE], 0, vel_x, vel_y);
        }
    }

  return TRUE;
}

static void
gtk_event_controller_scroll_class_init (GtkEventControllerScrollClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_event_controller_scroll_finalize;
  object_class->set_property = gtk_event_controller_scroll_set_property;
  object_class->get_property = gtk_event_controller_scroll_get_property;

  controller_class->handle_event = gtk_event_controller_scroll_handle_event;

  /**
   * GtkEventControllerScroll:flags:
   *
   * The flags affecting event controller behavior
   *
   * Since: 3.24
   **/
  pspecs[PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        P_("Flags"),
                        P_("Flags"),
                        GTK_TYPE_EVENT_CONTROLLER_SCROLL_FLAGS,
                        GTK_EVENT_CONTROLLER_SCROLL_NONE,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEventControllerScroll::scroll-begin:
   * @controller: The object that received the signal
   *
   * Signals that a new scrolling operation has begun. It will
   * only be emitted on devices capable of it.
   **/
  signals[SCROLL_BEGIN] =
    g_signal_new (I_("scroll-begin"),
                  GTK_TYPE_EVENT_CONTROLLER_SCROLL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  /**
   * GtkEventControllerScroll::scroll:
   * @controller: The object that received the signal
   * @dx: X delta
   * @dy: Y delta
   *
   * Signals that the widget should scroll by the
   * amount specified by @dx and @dy.
   **/
  signals[SCROLL] =
    g_signal_new (I_("scroll"),
                  GTK_TYPE_EVENT_CONTROLLER_SCROLL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[SCROLL],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
  /**
   * GtkEventControllerScroll::scroll-end:
   * @controller: The object that received the signal
   *
   * Signals that a new scrolling operation has finished. It will
   * only be emitted on devices capable of it.
   **/
  signals[SCROLL_END] =
    g_signal_new (I_("scroll-end"),
                  GTK_TYPE_EVENT_CONTROLLER_SCROLL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEventControllerScroll::decelerate:
   * @controller: The object that received the signal
   * @vel_x: X velocity
   * @vel_y: Y velocity
   *
   * Emitted after scroll is finished if the #GTK_EVENT_CONTROLLER_SCROLL_KINETIC
   * flag is set. @vel_x and @vel_y express the initial velocity that was
   * imprinted by the scroll events. @vel_x and @vel_y are expressed in
   * pixels/ms.
   **/
  signals[DECELERATE] =
    g_signal_new (I_("decelerate"),
                  GTK_TYPE_EVENT_CONTROLLER_SCROLL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[DECELERATE],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  g_object_class_install_properties (object_class, N_PROPS, pspecs);
}

static void
gtk_event_controller_scroll_init (GtkEventControllerScroll *scroll)
{
  scroll->scroll_history = g_array_new (FALSE, FALSE,
                                        sizeof (ScrollHistoryElem));
  gtk_event_controller_set_event_mask (GTK_EVENT_CONTROLLER (scroll),
                                       GDK_SCROLL_MASK |
                                       GDK_SMOOTH_SCROLL_MASK);
}

/**
 * gtk_event_controller_scroll_new:
 * @widget: a #GtkWidget
 * @flags: behavior flags
 *
 * Creates a new event controller that will handle scroll events
 * for the given @widget.
 *
 * Returns: a new #GtkEventControllerScroll
 *
 * Since: 3.24
 **/
GtkEventController *
gtk_event_controller_scroll_new (GtkWidget                     *widget,
                                 GtkEventControllerScrollFlags  flags)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_SCROLL,
                       "widget", widget,
                       "flags", flags,
                       NULL);
}

/**
 * gtk_event_controller_scroll_set_flags:
 * @scroll: a #GtkEventControllerScroll
 * @flags: behavior flags
 *
 * Sets the flags conditioning scroll controller behavior.
 *
 * Since: 3.24
 **/
void
gtk_event_controller_scroll_set_flags (GtkEventControllerScroll      *scroll,
                                       GtkEventControllerScrollFlags  flags)
{
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));

  if (scroll->flags == flags)
    return;

  scroll->flags = flags;
  g_object_notify_by_pspec (G_OBJECT (scroll), pspecs[PROP_FLAGS]);
}

/**
 * gtk_event_controller_scroll_get_flags:
 * @scroll: a #GtkEventControllerScroll
 *
 * Gets the flags conditioning the scroll controller behavior.
 *
 * Returns: the controller flags.
 *
 * Since: 3.24
 **/
GtkEventControllerScrollFlags
gtk_event_controller_scroll_get_flags (GtkEventControllerScroll *scroll)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll),
                        GTK_EVENT_CONTROLLER_SCROLL_NONE);

  return scroll->flags;
}
