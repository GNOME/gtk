/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gdk/gdkdisplayprivate.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "gtkdropprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtktooltipprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwindowgroup.h"
#include "gtkroot.h"
#include "gtknative.h"
#include "gtkpopcountprivate.h"

#include "inspector/window.h"

#include "gdk/gdkseatprivate.h"
#include "gdk/gdkeventsprivate.h"

#define GDK_ARRAY_ELEMENT_TYPE GtkWidget *
#define GDK_ARRAY_TYPE_NAME GtkWidgetStack
#define GDK_ARRAY_NAME gtk_widget_stack
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_PREALLOC 16
#include "gdk/gdkarrayimpl.c"

static GtkWindowGroup *gtk_main_get_window_group (GtkWidget   *widget);

static GList *current_events = NULL;

static GdkEvent *
rewrite_event_for_surface (GdkEvent  *event,
			   GdkSurface *new_surface)
{
  GdkEventType type;
  double x = -G_MAXDOUBLE, y = -G_MAXDOUBLE;
  double dx, dy;

  type = gdk_event_get_event_type (event);

  switch ((guint) type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return gdk_button_event_new (type,
                                   new_surface,
                                   gdk_event_get_device (event),
                                   gdk_event_get_device_tool (event),
                                   gdk_event_get_time (event),
                                   gdk_event_get_modifier_state (event),
                                   gdk_button_event_get_button (event),
                                   x, y,
                                   gdk_event_dup_axes (event));
    case GDK_MOTION_NOTIFY:
      return gdk_motion_event_new (new_surface,
                                   gdk_event_get_device (event),
                                   gdk_event_get_device_tool (event),
                                   gdk_event_get_time (event),
                                   gdk_event_get_modifier_state (event),
                                   x, y,
                                   gdk_event_dup_axes (event));
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      return gdk_touch_event_new (type,
                                  gdk_event_get_event_sequence (event),
                                  new_surface,
                                  gdk_event_get_device (event),
                                  gdk_event_get_time (event),
                                  gdk_event_get_modifier_state (event),
                                  x, y,
                                  gdk_event_dup_axes (event),
                                  gdk_touch_event_get_emulating_pointer (event));
    case GDK_TOUCHPAD_SWIPE:
      gdk_touchpad_event_get_deltas (event, &dx, &dy);
      return gdk_touchpad_event_new_swipe (new_surface,
                                           gdk_event_get_event_sequence (event),
                                           gdk_event_get_device (event),
                                           gdk_event_get_time (event),
                                           gdk_event_get_modifier_state (event),
                                           gdk_touchpad_event_get_gesture_phase (event),
                                           x, y,
                                           gdk_touchpad_event_get_n_fingers (event),
                                           dx, dy);
    case GDK_TOUCHPAD_PINCH:
      gdk_touchpad_event_get_deltas (event, &dx, &dy);
      return gdk_touchpad_event_new_pinch (new_surface,
                                           gdk_event_get_event_sequence (event),
                                           gdk_event_get_device (event),
                                           gdk_event_get_time (event),
                                           gdk_event_get_modifier_state (event),
                                           gdk_touchpad_event_get_gesture_phase (event),
                                           x, y,
                                           gdk_touchpad_event_get_n_fingers (event),
                                           dx, dy,
                                           gdk_touchpad_event_get_pinch_scale (event),
                                           gdk_touchpad_event_get_pinch_angle_delta (event));
    case GDK_TOUCHPAD_HOLD:
      return gdk_touchpad_event_new_hold (new_surface,
                                          gdk_event_get_event_sequence (event),
                                          gdk_event_get_device (event),
                                          gdk_event_get_time (event),
                                          gdk_event_get_modifier_state (event),
                                          gdk_touchpad_event_get_gesture_phase (event),
                                          x, y,
                                          gdk_touchpad_event_get_n_fingers (event));
    default:
      break;
    }

  return NULL;
}

/* If there is a seat grab in effect what GDK does is deliver the event normally
 * if it was going to this client, otherwise, delivers it in terms of the grab
 * surface.
 *
 * This function rewrites events to the effect that events going to the same
 * window group are delivered normally, otherwise, the event is delivered in
 * terms of the grab surface.
 */
static GdkEvent *
rewrite_event_for_grabs (GdkEvent *event)
{
  GdkSurface *grab_surface;
  GtkWidget *event_widget, *grab_widget;
  GdkSeat *seat;

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_MOTION_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_HOLD:
      seat = gdk_event_get_seat (event);
      grab_surface = gdk_seat_get_topmost_grab_surface (seat);

      if (!grab_surface)
        return NULL;
      break;
    default:
      return NULL;
    }

  event_widget = gtk_get_event_widget (event);
  grab_widget = GTK_WIDGET (gtk_native_get_for_surface (grab_surface));

  if (!grab_widget)
    return NULL;

  /* Events in client surfaces get forwarded
   * as normal, but we consider other window groups foreign surfaces.
   */
  if (gtk_main_get_window_group (grab_widget) == gtk_main_get_window_group (event_widget))
    return NULL;

  return rewrite_event_for_surface (event, grab_surface);
}

static GdkEvent *
rewrite_event_for_toplevel (GdkEvent *event)
{
  GdkSurface *surface;
  GdkEventType event_type;
  GdkTranslatedKey *key, *key_no_lock;

  surface = gdk_event_get_surface (event);
  if (!surface->parent)
    return NULL;

  event_type = gdk_event_get_event_type (event);
  if (event_type != GDK_KEY_PRESS &&
      event_type != GDK_KEY_RELEASE)
    return NULL;

  while (surface->parent)
    surface = surface->parent;

  key = gdk_key_event_get_translated_key (event, FALSE);
  key_no_lock = gdk_key_event_get_translated_key (event, TRUE);

  return gdk_key_event_new (gdk_event_get_event_type (event),
                            surface,
                            gdk_event_get_device (event),
                            gdk_event_get_time (event),
                            gdk_key_event_get_keycode (event),
                            gdk_event_get_modifier_state (event),
                            gdk_key_event_is_modifier (event),
                            key, key_no_lock,
                            gdk_key_event_get_compose_sequence (event));
}

static gboolean
translate_coordinates (double     event_x,
                       double     event_y,
                       double    *x,
                       double    *y,
                       GtkWidget *widget)
{
  GtkNative *native;
  graphene_point_t p;

  *x = *y = 0;
  native = gtk_widget_get_native (widget);

  if (!gtk_widget_compute_point (GTK_WIDGET (native),
                                 widget,
                                 &GRAPHENE_POINT_INIT (event_x, event_y),
                                 &p))
    return FALSE;

  *x = p.x;
  *y = p.y;

  return TRUE;
}

void
gtk_synthesize_crossing_events (GtkRoot         *toplevel,
                                GtkCrossingType  crossing_type,
                                GtkWidget       *old_target,
                                GtkWidget       *new_target,
                                double           surface_x,
                                double           surface_y,
                                GdkCrossingMode  mode,
                                GdkDrop         *drop)
{
  GtkCrossingData crossing;
  GtkWidget *ancestor;
  GtkWidget *widget;
  double x, y;
  GtkWidget *prev;
  gboolean seen_ancestor;
  GtkWidgetStack target_array;
  int i;

  if (old_target == new_target)
    return;

  if (old_target && new_target)
    ancestor = gtk_widget_common_ancestor (old_target, new_target);
  else
    ancestor = NULL;

  crossing.type = crossing_type;
  crossing.mode = mode;
  crossing.old_target = old_target ? g_object_ref (old_target) : NULL;
  crossing.old_descendent = NULL;
  crossing.new_target = new_target ? g_object_ref (new_target) : NULL;
  crossing.new_descendent = NULL;
  crossing.drop = drop;

  crossing.direction = GTK_CROSSING_OUT;

  prev = NULL;
  seen_ancestor = FALSE;
  widget = old_target;
  while (widget)
    {
      crossing.old_descendent = prev;
      if (seen_ancestor)
        {
          crossing.new_descendent = new_target ? prev : NULL;
        }
      else if (widget == ancestor)
        {
          GtkWidget *w;

          crossing.new_descendent = NULL;
          for (w = new_target; w != ancestor; w = _gtk_widget_get_parent (w))
            crossing.new_descendent = w;

          seen_ancestor = TRUE;
        }
      else
        {
          crossing.new_descendent = NULL;
        }
      check_crossing_invariants (widget, &crossing);
      translate_coordinates (surface_x, surface_y, &x, &y, widget);
      gtk_widget_handle_crossing (widget, &crossing, x, y);
      if (crossing_type == GTK_CROSSING_POINTER)
        gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);
      prev = widget;
      widget = _gtk_widget_get_parent (widget);
    }

  gtk_widget_stack_init (&target_array);
  for (widget = new_target; widget; widget = _gtk_widget_get_parent (widget))
    gtk_widget_stack_append (&target_array, g_object_ref (widget));

  crossing.direction = GTK_CROSSING_IN;

  seen_ancestor = FALSE;
  for (i = gtk_widget_stack_get_size (&target_array) - 1; i >= 0; i--)
    {
      widget = gtk_widget_stack_get (&target_array, i);

      if (i > 0)
        crossing.new_descendent = gtk_widget_stack_get (&target_array, i - 1);
      else
        crossing.new_descendent = NULL;

      if (seen_ancestor)
        {
          crossing.old_descendent = NULL;
        }
      else if (widget == ancestor)
        {
          GtkWidget *w;

          crossing.old_descendent = NULL;
          for (w = old_target; w != ancestor; w = _gtk_widget_get_parent (w))
            crossing.old_descendent = w;

          seen_ancestor = TRUE;
        }
      else
        {
          crossing.old_descendent = (old_target && ancestor) ? crossing.new_descendent : NULL;
        }

      check_crossing_invariants (widget, &crossing);
      translate_coordinates (surface_x, surface_y, &x, &y, widget);
      gtk_widget_handle_crossing (widget, &crossing, x, y);
      if (crossing_type == GTK_CROSSING_POINTER)
        gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
    }

  g_clear_object (&crossing.old_target);
  g_clear_object (&crossing.new_target);

  gtk_widget_stack_clear (&target_array);
}

static GtkWidget *
update_pointer_focus_state (GtkWindow *toplevel,
                            GdkEvent  *event,
                            GtkWidget *new_target)
{
  GtkWidget *old_target = NULL;
  GdkEventSequence *sequence;
  GdkDevice *device;
  GtkWidget *event_widget;
  graphene_point_t p;
  double x, y;
  double nx, ny;

  device = gdk_event_get_device (event);
  sequence = gdk_event_get_event_sequence (event);
  old_target = gtk_window_lookup_pointer_focus_widget (toplevel, device, sequence);
  if (old_target == new_target)
    return old_target;

  gdk_event_get_position (event, &x, &y);
  p = GRAPHENE_POINT_INIT (x, y);

  event_widget  = gtk_get_event_widget (event);
  if (!gtk_widget_compute_point (event_widget, GTK_WIDGET (toplevel), &p, &p))
    return old_target;

  gtk_native_get_surface_transform (GTK_NATIVE (toplevel), &nx, &ny);
  p.x -= nx;
  p.y -= ny;

  gtk_window_update_pointer_focus (toplevel, device, sequence,
                                   new_target, p.x, p.y);

  return old_target;
}

static gboolean
is_pointing_event (GdkEvent *event)
{
  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_SCROLL:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_HOLD:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      return TRUE;

    case GDK_GRAB_BROKEN:
      return gdk_device_get_source (gdk_event_get_device (event)) != GDK_SOURCE_KEYBOARD;

    default:
      return FALSE;
    }
}

static gboolean
is_key_event (GdkEvent *event)
{
  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return TRUE;
      break;
    case GDK_GRAB_BROKEN:
      return gdk_device_get_source (gdk_event_get_device (event)) == GDK_SOURCE_KEYBOARD;
    default:
      return FALSE;
    }
}

static GtkWidget *
handle_pointing_event (GdkEvent *event)
{
  GtkWidget *target = NULL, *old_target = NULL, *event_widget;
  GtkWindow *toplevel;
  GdkEventSequence *sequence;
  GdkDevice *device;
  double x, y;
  double native_x, native_y;
  GtkWidget *native;
  GdkEventType type;
  gboolean has_implicit;
  GdkModifierType modifiers;

  event_widget = gtk_get_event_widget (event);
  device = gdk_event_get_device (event);
  gdk_event_get_position (event, &x, &y);

  toplevel = GTK_WINDOW (gtk_widget_get_root (event_widget));
  native = GTK_WIDGET (gtk_widget_get_native (event_widget));

  gtk_native_get_surface_transform (GTK_NATIVE (native), &native_x, &native_y);
  x -= native_x;
  y -= native_y;

  type = gdk_event_get_event_type (event);
  sequence = gdk_event_get_event_sequence (event);

  if (type == GDK_SCROLL && !gdk_event_get_device_tool (event))
    {
      /* A bit of a kludge, resolve target lookups for scrolling devices
       * on the seat pointer.
       */
      device = gdk_seat_get_pointer (gdk_event_get_seat (event));
    }
  else if (type == GDK_TOUCHPAD_PINCH ||
           type == GDK_TOUCHPAD_SWIPE ||
           type == GDK_TOUCHPAD_HOLD)
    {
      /* Another bit of a kludge, touchpad gesture sequences do not
       * reflect on the pointer focus lookup.
       */
      sequence = NULL;
    }

  switch ((guint) type)
    {
    case GDK_LEAVE_NOTIFY:
      if (gdk_crossing_event_get_mode (event) == GDK_CROSSING_GRAB)
        {
          GtkWidget *grab_widget;

          grab_widget =
            gtk_window_lookup_pointer_focus_implicit_grab (toplevel,
                                                           device,
                                                           sequence);
          if (grab_widget)
            {
              gtk_window_set_pointer_focus_grab (toplevel, device,
                                                 sequence, NULL);
            }
        }

      old_target = update_pointer_focus_state (toplevel, event, NULL);
      gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_POINTER, old_target, NULL,
                                      x, y, gdk_crossing_event_get_mode (event), NULL);
      break;
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      old_target = update_pointer_focus_state (toplevel, event, NULL);
      gtk_window_set_pointer_focus_grab (toplevel, device,
                                         sequence, NULL);
      break;
    case GDK_DRAG_LEAVE:
      {
        GdkDrop *drop = gdk_dnd_event_get_drop (event);
        old_target = update_pointer_focus_state (toplevel, event, NULL);
        gtk_drop_begin_event (drop, GDK_DRAG_LEAVE);
        gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_DROP, old_target, NULL,
                                        x, y, GDK_CROSSING_NORMAL, drop);
        gtk_drop_end_event (drop);
      }
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_MOTION_NOTIFY:
      target = gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device, sequence);

      if (!target)
        target = gtk_widget_pick (native, x, y, GTK_PICK_DEFAULT);

      if (!target)
        target = GTK_WIDGET (native);

      old_target = update_pointer_focus_state (toplevel, event, target);

      if (type == GDK_MOTION_NOTIFY || type == GDK_ENTER_NOTIFY)
        {
          if (!gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device,
                                                              sequence))
            {
              gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_POINTER, old_target, target,
                                              x, y, GDK_CROSSING_NORMAL, NULL);
            }

          gtk_window_maybe_update_cursor (toplevel, NULL, device);
        }
      else if ((old_target != target) &&
               (type == GDK_DRAG_ENTER || type == GDK_DRAG_MOTION || type == GDK_DROP_START))
        {
          GdkDrop *drop = gdk_dnd_event_get_drop (event);
          gtk_drop_begin_event (drop, type);
          gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_DROP, old_target, target,
                                          x, y, GDK_CROSSING_NORMAL, gdk_dnd_event_get_drop (event));
          gtk_drop_end_event (drop);
        }
      else if (type == GDK_TOUCH_BEGIN)
        {
          gtk_window_set_pointer_focus_grab (toplevel, device, sequence, target);
        }

      /* Let it take the effective pointer focus anyway, as it may change due
       * to implicit grabs.
       */
      target = NULL;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      target = gtk_window_lookup_effective_pointer_focus_widget (toplevel,
                                                                 device,
                                                                 sequence);
      has_implicit =
        gtk_window_lookup_pointer_focus_implicit_grab (toplevel,
                                                       device,
                                                       sequence) != NULL;
      modifiers = gdk_event_get_modifier_state (event);

      if (type == GDK_BUTTON_RELEASE &&
          gtk_popcount (modifiers & (GDK_BUTTON1_MASK |
                                     GDK_BUTTON2_MASK |
                                     GDK_BUTTON3_MASK |
                                     GDK_BUTTON4_MASK |
                                     GDK_BUTTON5_MASK)) == 1)
        {
          GtkWidget *new_target = gtk_widget_pick (native, x, y, GTK_PICK_DEFAULT);

          gtk_window_set_pointer_focus_grab (toplevel, device, sequence, NULL);

          if (new_target == NULL)
            new_target = GTK_WIDGET (toplevel);

          gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_POINTER, target, new_target,
                                          x, y, GDK_CROSSING_UNGRAB, NULL);
          gtk_window_maybe_update_cursor (toplevel, NULL, device);
          update_pointer_focus_state (toplevel, event, new_target);
        }
      else if (type == GDK_BUTTON_PRESS &&
               !has_implicit &&
               (modifiers & (GDK_BUTTON1_MASK |
                             GDK_BUTTON2_MASK |
                             GDK_BUTTON3_MASK |
                             GDK_BUTTON4_MASK |
                             GDK_BUTTON5_MASK)) == 0)
        {
          gtk_window_set_pointer_focus_grab (toplevel, device,
                                             sequence, target);
        }

      break;
    case GDK_SCROLL:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_HOLD:
      break;
    case GDK_GRAB_BROKEN:
      if (gdk_grab_broken_event_get_implicit (event))
        {
          target = gtk_window_lookup_effective_pointer_focus_widget (toplevel,
                                                                     device,
                                                                     sequence);
          if (target)
            {
              gtk_window_set_pointer_focus_grab (toplevel, device,
                                                 sequence, NULL);
            }
        }
      break;
    default:
      g_assert_not_reached ();
    }

  if (!target)
    target = gtk_window_lookup_effective_pointer_focus_widget (toplevel,
                                                               device,
                                                               sequence);
  return target ? target : old_target;
}

static GtkWidget *
handle_key_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *focus_widget;

  event_widget = gtk_get_event_widget (event);

  focus_widget = gtk_root_get_focus (gtk_widget_get_root (event_widget));
  return focus_widget ? focus_widget : event_widget;
}

static gboolean
is_transient_for (GtkWindow *child,
                  GtkWindow *parent)
{
  GtkWindow *transient_for;

  transient_for = gtk_window_get_transient_for (child);

  while (transient_for)
    {
      if (transient_for == parent)
        return TRUE;

      transient_for = gtk_window_get_transient_for (transient_for);
    }

  return FALSE;
}

gboolean
gtk_main_do_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *target_widget;
  GtkWidget *grab_widget = NULL;
  GtkWindowGroup *window_group;
  GdkEvent *rewritten_event = NULL;
  GList *tmp_list;
  gboolean handled_event = FALSE;

  if (gtk_inspector_handle_event (event))
    return FALSE;

  /* Find the widget which got the event. We store the widget
   * in the user_data field of GdkSurface's. Ignore the event
   * if we don't have a widget for it.
   */
  event_widget = gtk_get_event_widget (event);
  if (!event_widget)
    return FALSE;

  target_widget = event_widget;

  /* We propagate key events from the root, even if they are
   * delivered to a popup surface.
   *
   * If pointer or keyboard grabs are in effect, munge the events
   * so that each window group looks like a separate app.
   */
  if (is_key_event (event))
    rewritten_event = rewrite_event_for_toplevel (event);
  else
    rewritten_event = rewrite_event_for_grabs (event);
  if (rewritten_event)
    {
      event = rewritten_event;
      target_widget = gtk_get_event_widget (event);
    }

  /* Push the event onto a stack of current events for
   * gtk_current_event_get().
   */
  current_events = g_list_prepend (current_events, event);

  if (is_pointing_event (event))
    {
      target_widget = handle_pointing_event (event);
    }
  else if (is_key_event (event))
    {
      target_widget = handle_key_event (event);
    }

  if (!target_widget)
    goto cleanup;

  window_group = gtk_main_get_window_group (target_widget);

  /* check whether there is a grab in effect... */
  grab_widget = gtk_window_group_get_current_grab (window_group);

  /* If the grab widget is an ancestor of the event widget
   * then we send the event to the original event widget.
   * This is the key to implementing modality. This also applies
   * across windows that are directly or indirectly transient-for
   * the modal one.
   */
  if (!grab_widget ||
      ((gtk_widget_is_sensitive (target_widget) || gdk_event_get_event_type (event) == GDK_SCROLL) &&
       gtk_widget_is_ancestor (target_widget, grab_widget)) ||
      (GTK_IS_WINDOW (grab_widget) &&
       GTK_IS_WINDOW (event_widget) &&
       grab_widget != event_widget &&
       is_transient_for (GTK_WINDOW (event_widget), GTK_WINDOW (grab_widget))))
    grab_widget = target_widget;

  g_object_ref (target_widget);

  /* Not all events get sent to the grabbing widget.
   * The delete, destroy, expose, focus change and resize
   * events still get sent to the event widget because
   * 1) these events have no meaning for the grabbing widget
   * and 2) redirecting these events to the grabbing widget
   * could cause the display to be messed up.
   *
   * Drag events are also not redirected, since it isn't
   * clear what the semantics of that would be.
   */
  switch ((guint)gdk_event_get_event_type (event))
    {
    case GDK_DELETE:
      if (!gtk_window_group_get_current_grab (window_group) ||
          GTK_WIDGET (gtk_widget_get_root (gtk_window_group_get_current_grab (window_group))) == target_widget)
        {
          if (GTK_IS_WINDOW (target_widget) &&
              !gtk_window_emit_close_request (GTK_WINDOW (target_widget)))
            gtk_window_destroy (GTK_WINDOW (target_widget));
          handled_event = TRUE;
        }
      break;

    case GDK_FOCUS_CHANGE:
      {
        GtkWidget *root = GTK_WIDGET (gtk_widget_get_root (target_widget));
        if (!_gtk_widget_captured_event (root, event, root))
          handled_event = gtk_widget_event (root, event, root);
      }
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_TOUCH_BEGIN:
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_RELEASE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_HOLD:
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
    case GDK_PAD_DIAL:
    case GDK_PAD_GROUP_MODE:
    case GDK_GRAB_BROKEN:
      handled_event = gtk_propagate_event (grab_widget, event);
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
      /* Crossing event propagation happens during picking */
      handled_event = TRUE;
      break;

    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
        GdkDrop *drop = gdk_dnd_event_get_drop (event);
        gtk_drop_begin_event (drop, gdk_event_get_event_type (event));
        handled_event = gtk_propagate_event (grab_widget, event);
        gtk_drop_end_event (drop);
      }
      break;

    case GDK_EVENT_LAST:
    default:
      g_assert_not_reached ();
      break;
    }

  _gtk_tooltip_handle_event (target_widget, event);

  g_object_unref (target_widget);

 cleanup:
  tmp_list = current_events;
  current_events = g_list_remove_link (current_events, tmp_list);
  g_list_free_1 (tmp_list);

  if (rewritten_event)
    gdk_event_unref (rewritten_event);
  return handled_event;
}

static GtkWindowGroup *
gtk_main_get_window_group (GtkWidget *widget)
{
  GtkWidget *toplevel = NULL;

  if (widget)
    toplevel = GTK_WIDGET (gtk_widget_get_root (widget));

  if (GTK_IS_WINDOW (toplevel))
    return gtk_window_get_group (GTK_WINDOW (toplevel));
  else
    return gtk_window_get_group (NULL);
}

static void
gtk_grab_notify (GtkWindowGroup *group,
                 GtkWidget      *old_grab_widget,
                 GtkWidget      *new_grab_widget,
                 gboolean        from_grab)
{
  GList *toplevels;

  if (old_grab_widget == new_grab_widget)
    return;

  g_object_ref (group);

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);

  while (toplevels)
    {
      GtkWindow *toplevel = toplevels->data;
      toplevels = g_list_delete_link (toplevels, toplevels);

      gtk_window_grab_notify (toplevel,
                              old_grab_widget,
                              new_grab_widget,
                              from_grab);
      g_object_unref (toplevel);
    }

  g_object_unref (group);
}

/**
 * gtk_grab_add: (method)
 * @widget: The widget that grabs keyboard and pointer events
 *
 * Makes @widget the current grabbed widget.
 *
 * This means that interaction with other widgets in the same
 * application is blocked and mouse as well as keyboard events
 * are delivered to this widget.
 *
 * If @widget is not sensitive, it is not set as the current
 * grabbed widget and this function does nothing.
 */
void
gtk_grab_add (GtkWidget *widget)
{
  GtkWindowGroup *group;
  GtkWidget *old_grab_widget;

  g_return_if_fail (widget != NULL);

  if (!gtk_widget_has_grab (widget) && gtk_widget_is_sensitive (widget))
    {
      _gtk_widget_set_has_grab (widget, TRUE);

      group = gtk_main_get_window_group (widget);

      old_grab_widget = gtk_window_group_get_current_grab (group);

      g_object_ref (widget);
      _gtk_window_group_add_grab (group, widget);

      gtk_grab_notify (group, old_grab_widget, widget, TRUE);
    }
}

/**
 * gtk_grab_remove: (method)
 * @widget: The widget which gives up the grab
 *
 * Removes the grab from the given widget.
 *
 * You have to pair calls to gtk_grab_add() and gtk_grab_remove().
 *
 * If @widget does not have the grab, this function does nothing.
 */
void
gtk_grab_remove (GtkWidget *widget)
{
  GtkWindowGroup *group;
  GtkWidget *new_grab_widget;

  g_return_if_fail (widget != NULL);

  if (gtk_widget_has_grab (widget))
    {
      _gtk_widget_set_has_grab (widget, FALSE);

      group = gtk_main_get_window_group (widget);
      _gtk_window_group_remove_grab (group, widget);
      new_grab_widget = gtk_window_group_get_current_grab (group);

      gtk_grab_notify (group, widget, new_grab_widget, FALSE);

      g_object_unref (widget);
    }
}

guint32
gtk_get_current_event_time (void)
{
  if (current_events)
    return gdk_event_get_time (current_events->data);
  else
    return GDK_CURRENT_TIME;
}

/**
 * gtk_get_event_widget:
 * @event: a `GdkEvent`
 *
 * If @event is %NULL or the event was not associated with any widget,
 * returns %NULL, otherwise returns the widget that received the event
 * originally.
 *
 * Returns: (transfer none) (nullable): the widget that originally
 *   received @event
 */
GtkWidget *
gtk_get_event_widget (GdkEvent *event)
{
  GdkSurface *surface;

  surface = gdk_event_get_surface (event);
  if (surface && !gdk_surface_is_destroyed (surface))
    return GTK_WIDGET (gtk_native_get_for_surface (surface));

  return NULL;
}

gboolean
gtk_propagate_event_internal (GtkWidget *widget,
                              GdkEvent  *event,
                              GtkWidget *topmost)
{
  int handled_event = FALSE;
  GtkWidget *target = widget;
  GtkWidgetStack widget_array;
  int i;

  /* First, propagate event down */
  gtk_widget_stack_init (&widget_array);
  gtk_widget_stack_append (&widget_array, g_object_ref (widget));

  for (;;)
    {
      widget = _gtk_widget_get_parent (widget);
      if (!widget)
        break;

      gtk_widget_stack_append (&widget_array, g_object_ref (widget));

      if (widget == topmost)
        break;
    }

  i = gtk_widget_stack_get_size (&widget_array) - 1;
  for (;;)
    {
      widget = gtk_widget_stack_get (&widget_array, i);

      if (!_gtk_widget_is_sensitive (widget))
        {
          /* stop propagating on SCROLL, but don't handle the event, so it
           * can propagate up again and reach its handling widget
           */
          if (gdk_event_get_event_type (event) == GDK_SCROLL)
            break;
          else
            handled_event = TRUE;
        }
      else if (_gtk_widget_get_realized (widget))
        handled_event = _gtk_widget_captured_event (widget, event, target);

      handled_event |= !_gtk_widget_get_realized (widget);

      if (handled_event)
        break;

      if (i == 0)
        break;

      i--;
    }

  /* If not yet handled, also propagate back up */
  if (!handled_event)
    {
      /* Propagate event up the widget tree so that
       * parents can see the button and motion
       * events of the children.
       */
      for (i = 0; i < gtk_widget_stack_get_size (&widget_array); i++)
        {
          widget = gtk_widget_stack_get (&widget_array, i);

          /* Scroll events are special cased here because it
           * feels wrong when scrolling a GtkViewport, say,
           * to have children of the viewport eat the scroll
           * event
           */
          if (!_gtk_widget_is_sensitive (widget))
            handled_event = gdk_event_get_event_type (event) != GDK_SCROLL;
          else if (_gtk_widget_get_realized (widget))
            handled_event = gtk_widget_event (widget, event, target);

          handled_event |= !_gtk_widget_get_realized (widget);

          if (handled_event)
            break;
        }
    }

  gtk_widget_stack_clear (&widget_array);
  return handled_event;
}

/**
 * gtk_propagate_event:
 * @widget: a `GtkWidget`
 * @event: an event
 *
 * Sends an event to a widget, propagating the event to parent widgets
 * if the event remains unhandled. This function will emit the event
 * through all the hierarchy of @widget through all propagation phases.
 *
 * Events received by GTK from GDK normally begin at a `GtkRoot` widget.
 * Depending on the type of event, existence of modal dialogs, grabs, etc.,
 * the event may be propagated; if so, this function is used.
 *
 * All that said, you most likely don’t want to use any of these
 * functions; synthesizing events is rarely needed. There are almost
 * certainly better ways to achieve your goals. For example, use
 * gtk_widget_queue_draw() instead
 * of making up expose events.
 *
 * Returns: %TRUE if the event was handled
 */
gboolean
gtk_propagate_event (GtkWidget *widget,
                     GdkEvent  *event)
{
  GtkWindowGroup *window_group;
  GtkWidget *event_widget, *topmost = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  event_widget = gtk_get_event_widget (event);
  window_group = gtk_main_get_window_group (event_widget);

  /* check whether there is a grab in effect... */
  topmost = gtk_window_group_get_current_grab (window_group);

  return gtk_propagate_event_internal (widget, event, topmost);
}

gboolean
gtk_event_treat_as_touch (GdkEvent *event)
{
  switch ((unsigned int) gdk_device_get_source (gdk_event_get_device (event)))
    {
    case GDK_SOURCE_TOUCHSCREEN:
      return TRUE;
    case GDK_SOURCE_KEYBOARD:
      return FALSE;
    default:
      return GTK_DISPLAY_DEBUG_CHECK (gdk_event_get_display (event), TOUCHSCREEN);
    }
}
