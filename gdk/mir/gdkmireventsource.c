/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkinternals.h"
#include "gdkdisplayprivate.h"
#include "gdkmir.h"
#include "gdkmir-private.h"

#define NANO_TO_MILLI(x) ((x) / 1000000)

struct _GdkMirWindowReference {
  GdkMirEventSource *source;
  GdkWindow         *window;
  gint               ref_count;
};

typedef struct {
  GdkMirWindowReference *window_ref;
  const MirEvent        *event;
} GdkMirQueuedEvent;

struct _GdkMirEventSource
{
  GSource parent_instance;

  GMutex mir_event_lock;
  GQueue mir_events;
  gboolean log_events;

  GdkDisplay *display;
};

static void
send_event (GdkWindow *window, GdkDevice *device, GdkEvent *event)
{
  GdkDisplay *display;
  GList *node;

  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);
  gdk_event_set_screen (event, gdk_display_get_screen (gdk_window_get_display (window), 0));
  event->any.window = g_object_ref (window);

  display = gdk_window_get_display (window);
  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event, _gdk_display_get_next_serial (display));
}

static void
set_key_event_string (GdkEventKey *event)
{
  gunichar c = 0;

  if (event->keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      gchar buf[7];
      gint len;
      gsize bytes_written;

      /* Apply the control key - Taken from Xlib
       */
      if (event->state & GDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
          else if (c == '2')
            {
              event->string = g_memdup ("\0\0", 2);
              event->length = 1;
              buf[0] = '\0';
              return;
            }
          else if (c >= '3' && c <= '7') c -= ('3' - '\033');
          else if (c == '8') c = '\177';
          else if (c == '/') c = '_' & 0x1F;
        }

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->string = g_locale_from_utf8 (buf, len,
                                          NULL, &bytes_written,
                                          NULL);
      if (event->string)
        event->length = bytes_written;
    }
  else if (event->keyval == GDK_KEY_Escape)
    {
      event->length = 1;
      event->string = g_strdup ("\033");
    }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_KP_Enter)
    {
      event->length = 1;
      event->string = g_strdup ("\r");
    }

  if (!event->string)
    {
      event->length = 0;
      event->string = g_strdup ("");
    }
}

static void
generate_key_event (GdkWindow *window, GdkEventType type, guint state, guint keyval, guint16 keycode, gboolean is_modifier, guint32 event_time)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->key.state = state;
  event->key.keyval = keyval;
  event->key.hardware_keycode = keycode + 8;
  event->key.is_modifier = is_modifier;
  event->key.time = event_time;
  set_key_event_string (&event->key);

  send_event (window, _gdk_mir_device_manager_get_keyboard (gdk_display_get_device_manager (gdk_window_get_display (window))), event);
}

static GdkDevice *
get_pointer (GdkWindow *window)
{
  return gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (gdk_window_get_display (window)));
}

static void
generate_button_event (GdkWindow *window, GdkEventType type, gdouble x, gdouble y, guint button, guint state, guint32 event_time)
{
  GdkEvent *event;

  g_print ("%s %lf %lf\n", G_STRFUNC, x, y);

  event = gdk_event_new (type);
  event->button.x = x;
  event->button.y = y;
  event->button.state = state;
  event->button.button = button;
  event->button.time = event_time;

  send_event (window, get_pointer (window), event);
}

static void
generate_scroll_event (GdkWindow *window, gdouble x, gdouble y, gdouble delta_x, gdouble delta_y, guint state, guint32 event_time)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_SCROLL);
  event->scroll.x = x;
  event->scroll.y = y;
  event->scroll.state = state;
  event->scroll.time = event_time;

  if (ABS (delta_x) == 1 && delta_y == 0)
    {
      event->scroll.direction = (delta_x < 0) ? GDK_SCROLL_LEFT : GDK_SCROLL_RIGHT;
    }
  else if (ABS (delta_y) == 1 && delta_x == 0)
    {
      event->scroll.direction = (delta_y < 0) ? GDK_SCROLL_DOWN : GDK_SCROLL_UP;
    }
  else
    {
      event->scroll.direction = GDK_SCROLL_SMOOTH;
      event->scroll.delta_x = -delta_x;
      event->scroll.delta_y = -delta_y;
    }

  send_event (window, get_pointer (window), event);
}

static void
generate_motion_event (GdkWindow *window, gdouble x, gdouble y, guint state, guint32 event_time)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.state = state;
  event->motion.is_hint = FALSE;
  event->motion.time = event_time;

  send_event (window, get_pointer (window), event);
}

static void
generate_crossing_event (GdkWindow *window, GdkEventType type, gdouble x, gdouble y, guint32 event_time)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.time = event_time;

  send_event (window, get_pointer (window), event);
}

static void
generate_focus_event (GdkWindow *window, gboolean focused)
{
  GdkEvent *event;

  if (focused)
    gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FOCUSED);
  else
    gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FOCUSED, 0);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = focused;

  send_event (window, get_pointer (window), event);
}

static guint
get_modifier_state (unsigned int modifiers, unsigned int button_state)
{
  guint modifier_state = 0;

  if ((modifiers & (mir_input_event_modifier_alt |
                    mir_input_event_modifier_alt_left |
                    mir_input_event_modifier_alt_right)) != 0)
    modifier_state |= GDK_MOD1_MASK;
  if ((modifiers & (mir_input_event_modifier_shift |
                    mir_input_event_modifier_shift_left |
                    mir_input_event_modifier_shift_right)) != 0)
    modifier_state |= GDK_SHIFT_MASK;
  if ((modifiers & (mir_input_event_modifier_ctrl |
                    mir_input_event_modifier_ctrl_left |
                    mir_input_event_modifier_ctrl_right)) != 0)
    modifier_state |= GDK_CONTROL_MASK;
  if ((modifiers & (mir_input_event_modifier_meta |
                    mir_input_event_modifier_meta_left |
                    mir_input_event_modifier_meta_right)) != 0)
    modifier_state |= GDK_META_MASK;
  if ((modifiers & mir_input_event_modifier_caps_lock) != 0)
    modifier_state |= GDK_LOCK_MASK;
  if ((button_state & mir_pointer_button_primary) != 0)
    modifier_state |= GDK_BUTTON1_MASK;
  if ((button_state & mir_pointer_button_secondary) != 0)
    modifier_state |= GDK_BUTTON2_MASK;
  if ((button_state & mir_pointer_button_tertiary) != 0)
    modifier_state |= GDK_BUTTON3_MASK;
  if ((button_state & mir_pointer_button_back) != 0)
    modifier_state |= GDK_BUTTON4_MASK;
  if ((button_state & mir_pointer_button_forward) != 0)
    modifier_state |= GDK_BUTTON5_MASK;

  return modifier_state;
}

static void
handle_key_event (GdkWindow *window, const MirInputEvent *event)
{
  const MirKeyboardEvent *keyboard_event = mir_input_event_get_keyboard_event (event);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkKeymap *keymap;
  guint modifier_state;
  guint button_state;

  if (!keyboard_event)
    return;

  switch (mir_keyboard_event_action (keyboard_event))
    {
    case mir_keyboard_action_up:
    case mir_keyboard_action_down:
      // FIXME: Convert keycode
      _gdk_mir_window_impl_get_cursor_state (impl, NULL, NULL, NULL, &button_state);
      modifier_state = get_modifier_state (mir_keyboard_event_modifiers (keyboard_event), button_state);
      keymap = gdk_keymap_get_for_display (gdk_window_get_display (window));

      generate_key_event (window,
                          mir_keyboard_event_action (keyboard_event) == mir_keyboard_action_down ? GDK_KEY_PRESS : GDK_KEY_RELEASE,
                          modifier_state,
                          mir_keyboard_event_key_code (keyboard_event),
                          mir_keyboard_event_scan_code (keyboard_event),
                          _gdk_mir_keymap_key_is_modifier (keymap, mir_keyboard_event_key_code (keyboard_event)),
                          NANO_TO_MILLI (mir_input_event_get_event_time (event)));
      break;
    default:
    //case mir_key_action_multiple:
      // FIXME
      break;
    }
}

static guint
mir_pointer_event_get_button_state (const MirPointerEvent *event)
{
  guint button_state = 0;

  if (mir_pointer_event_button_state (event, mir_pointer_button_primary))
    button_state |= mir_pointer_button_primary;
  if (mir_pointer_event_button_state (event, mir_pointer_button_secondary))
    button_state |= mir_pointer_button_secondary;
  if (mir_pointer_event_button_state (event, mir_pointer_button_tertiary))
    button_state |= mir_pointer_button_tertiary;
  if (mir_pointer_event_button_state (event, mir_pointer_button_back))
    button_state |= mir_pointer_button_back;
  if (mir_pointer_event_button_state (event, mir_pointer_button_forward))
    button_state |= mir_pointer_button_forward;

  return button_state;
}

static void
handle_motion_event (GdkWindow *window, const MirInputEvent *event)
{
  const MirPointerEvent *pointer_event = mir_input_event_get_pointer_event (event);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  gdouble x, y;
  gboolean cursor_inside;
  guint button_state;
  guint modifier_state;
  guint32 event_time;
  GdkEventType event_type;
  guint changed_button_state;

  if (!pointer_event)
    return;

  _gdk_mir_window_impl_get_cursor_state (impl, &x, &y, &cursor_inside, &button_state);
  modifier_state = get_modifier_state (mir_pointer_event_modifiers (pointer_event), mir_pointer_event_get_button_state (pointer_event));
  event_time = NANO_TO_MILLI (mir_input_event_get_event_time (event));

  if (window)
    {
      gdouble new_x;
      gdouble new_y;
      gdouble hscroll;
      gdouble vscroll;

      /* Update which window has focus */
      _gdk_mir_pointer_set_location (get_pointer (window), x, y, window, modifier_state);
      switch (mir_pointer_event_action (pointer_event))
        {
        case mir_pointer_action_button_up:
        case mir_pointer_action_button_down:
          g_print ("%s button up/down\n", G_STRFUNC);

          event_type = mir_pointer_event_action (pointer_event) == mir_pointer_action_button_down ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;
          changed_button_state = button_state ^ mir_pointer_event_get_button_state (pointer_event);
          if (changed_button_state == 0 || (changed_button_state & mir_pointer_button_primary) != 0)
            generate_button_event (window, event_type, x, y, GDK_BUTTON_PRIMARY, modifier_state, event_time);
          if ((changed_button_state & mir_pointer_button_secondary) != 0)
            generate_button_event (window, event_type, x, y, GDK_BUTTON_SECONDARY, modifier_state, event_time);
          if ((changed_button_state & mir_pointer_button_tertiary) != 0)
            generate_button_event (window, event_type, x, y, GDK_BUTTON_MIDDLE, modifier_state, event_time);
          button_state = mir_pointer_event_get_button_state (pointer_event);
          break;
        case mir_pointer_action_motion:
          new_x = mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_x);
          new_y = mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_y);
          hscroll = mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_hscroll);
          vscroll = mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_vscroll);

          if (hscroll > 0.5 || vscroll > 0.5)
            generate_scroll_event (window, x, y, hscroll, vscroll, modifier_state, event_time);
          if (ABS (new_x - x) > 0.5 || ABS (new_y - y) > 0.5)
            {
              generate_motion_event (window, new_x, new_y, modifier_state, event_time);
              x = new_x;
              y = new_y;
            }

          break;
        case mir_pointer_action_leave:
          if (cursor_inside)
            {
              cursor_inside = FALSE;
              generate_crossing_event (window, GDK_LEAVE_NOTIFY, x, y, event_time);
            }
          break;
        default:
          break;
        }

      _gdk_mir_window_impl_set_cursor_state (impl, x, y, cursor_inside, button_state);
    }
}

static void
handle_surface_event (GdkWindow *window, const MirSurfaceEvent *event)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  switch (mir_surface_event_get_attribute (event))
    {
    case mir_surface_attrib_type:
      _gdk_mir_window_impl_set_surface_type (impl, mir_surface_event_get_attribute_value (event));
      break;
    case mir_surface_attrib_state:
      _gdk_mir_window_impl_set_surface_state (impl, mir_surface_event_get_attribute_value (event));
      // FIXME: notify
      break;
    case mir_surface_attrib_swapinterval:
      break;
    case mir_surface_attrib_focus:
      generate_focus_event (window, mir_surface_event_get_attribute_value (event) != 0);
      break;
    default:
      break;
    }
}

static void
generate_configure_event (GdkWindow *window,
                          gint       width,
                          gint       height)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.send_event = FALSE;
  event->configure.width = width;
  event->configure.height = height;

  send_event (window, get_pointer (window), event);
}

static void
handle_resize_event (GdkWindow            *window,
                     const MirResizeEvent *event)
{
  window->width = mir_resize_event_get_width (event);
  window->height = mir_resize_event_get_height (event);
  _gdk_window_update_size (window);

  generate_configure_event (window, mir_resize_event_get_width (event), mir_resize_event_get_height (event));
}

static void
handle_close_event (GdkWindow                  *window,
                    const MirCloseSurfaceEvent *event)
{
  send_event (window, get_pointer (window), gdk_event_new (GDK_DESTROY));
  gdk_window_destroy_notify (window);
}

typedef struct
{
  GdkWindow *window;
  MirEvent *event;
} EventData;

static void
gdk_mir_event_source_queue_event (GdkDisplay     *display,
                                  GdkWindow      *window,
                                  const MirEvent *event)
{
  const MirInputEvent *input_event;

  // FIXME: Only generate events if the window wanted them?
  switch (mir_event_get_type (event))
    {
    case mir_event_type_input:
      input_event = mir_event_get_input_event (event);

      switch (mir_input_event_get_type (input_event))
        {
        case mir_input_event_type_key:
          handle_key_event (window, input_event);
          break;
        case mir_input_event_type_touch:
          handle_motion_event (window, input_event);
          break;
        case mir_input_event_type_pointer:
          handle_motion_event (window, input_event);
          break;
        }

      break;
    case mir_event_type_key:
      handle_key_event (window, mir_event_get_input_event (event));
      break;
    case mir_event_type_motion:
      handle_motion_event (window, mir_event_get_input_event (event));
      break;
    case mir_event_type_surface:
      handle_surface_event (window, mir_event_get_surface_event (event));
      break;
    case mir_event_type_resize:
      handle_resize_event (window, mir_event_get_resize_event (event));
      break;
    case mir_event_type_prompt_session_state_change:
      // FIXME?
      break;
    case mir_event_type_orientation:
      // FIXME?
      break;
    case mir_event_type_close_surface:
      handle_close_event (window, mir_event_get_close_surface_event (event));
      break;
    default:
      g_warning ("Ignoring unknown Mir event %d", mir_event_get_type (event));
      // FIXME?
      break;
    }
}

static GdkMirQueuedEvent *
gdk_mir_event_source_take_queued_event (GdkMirEventSource *source)
{
  GdkMirQueuedEvent *queued_event;

  g_mutex_lock (&source->mir_event_lock);
  queued_event = g_queue_pop_head (&source->mir_events);
  g_mutex_unlock (&source->mir_event_lock);

  return queued_event;
}

static void
gdk_mir_queued_event_free (GdkMirQueuedEvent *event)
{
  _gdk_mir_window_reference_unref (event->window_ref);
  g_slice_free (GdkMirQueuedEvent, event);
}

static void
gdk_mir_event_source_convert_events (GdkMirEventSource *source)
{
  GdkMirQueuedEvent *event;

  while ((event = gdk_mir_event_source_take_queued_event (source)))
    {
      GdkWindow *window = event->window_ref->window;

      /* The window may have been destroyed in the main thread while the
       * event was being dispatched...
       */
      if (window != NULL)
        {
          if (source->log_events)
            _gdk_mir_print_event (event->event);

          gdk_mir_event_source_queue_event (source->display, window, event->event);
        }
      else
        g_warning ("window was destroyed before event arrived...");

      gdk_mir_queued_event_free (event);
    }
}

static gboolean
gdk_mir_event_source_prepare (GSource *g_source,
                              gint    *timeout)
{
  GdkMirEventSource *source = (GdkMirEventSource *) g_source;
  gboolean mir_events_in_queue;

  if (_gdk_event_queue_find_first (source->display))
   return TRUE;

  g_mutex_lock (&source->mir_event_lock);
  mir_events_in_queue = g_queue_get_length (&source->mir_events) > 0;
  g_mutex_unlock (&source->mir_event_lock);

  return mir_events_in_queue;
}

static gboolean
gdk_mir_event_source_check (GSource *g_source)
{
  return gdk_mir_event_source_prepare (g_source, NULL);
}

static gboolean
gdk_mir_event_source_dispatch (GSource     *g_source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  GdkMirEventSource *source = (GdkMirEventSource *) g_source;
  GdkEvent *event;

  /* First, run the queue of events from the thread */
  gdk_mir_event_source_convert_events (source);

  /* Next, dispatch one single event from the display's queue.
   *
   * If there is more than one event then we will soon find ourselves
   * back here again.
   */

  gdk_threads_enter ();

  event = gdk_display_get_event (source->display);

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_free (event);
    }

  gdk_threads_leave ();

  return TRUE;
}

static void
gdk_mir_event_source_finalize (GSource *g_source)
{
  GdkMirEventSource *source = (GdkMirEventSource *) g_source;
  GdkMirQueuedEvent *event;

  while ((event = gdk_mir_event_source_take_queued_event (source)))
    gdk_mir_queued_event_free (event);

  g_mutex_clear (&source->mir_event_lock);
}

static GSourceFuncs gdk_mir_event_source_funcs = {
  gdk_mir_event_source_prepare,
  gdk_mir_event_source_check,
  gdk_mir_event_source_dispatch,
  gdk_mir_event_source_finalize
};

GdkMirEventSource *
_gdk_mir_event_source_new (GdkDisplay *display)
{
  GdkMirEventSource *source;
  GSource *g_source;

  g_source = g_source_new (&gdk_mir_event_source_funcs, sizeof (GdkMirEventSource));
  g_source_attach (g_source, NULL);

  source = (GdkMirEventSource *) g_source;
  g_mutex_init (&source->mir_event_lock);
  source->display = display;
  source->log_events = (g_getenv ("GDK_MIR_LOG_EVENTS") != NULL);

  return source;
}

GdkMirWindowReference *
_gdk_mir_event_source_get_window_reference (GdkWindow *window)
{
  static GQuark win_ref_quark;
  GdkMirWindowReference *ref;

  if G_UNLIKELY (!win_ref_quark)
    win_ref_quark = g_quark_from_string ("GdkMirEventSource window reference");

  ref = g_object_get_qdata (G_OBJECT (window), win_ref_quark);

  if (!ref)
    {
      GdkMirEventSource *source;

      source = _gdk_mir_display_get_event_source (gdk_window_get_display (window));
      g_source_ref ((GSource *) source);

      ref = g_slice_new (GdkMirWindowReference);
      ref->window = window;
      ref->source = source;
      ref->ref_count = 0;
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &ref->window);

      g_object_set_qdata_full (G_OBJECT (window), win_ref_quark,
                               ref, (GDestroyNotify) _gdk_mir_window_reference_unref);
    }

  g_atomic_int_inc (&ref->ref_count);

  return ref;
}

void
_gdk_mir_window_reference_unref (GdkMirWindowReference *ref)
{
  if (g_atomic_int_dec_and_test (&ref->ref_count))
    {
      if (ref->window)
        g_object_remove_weak_pointer (G_OBJECT (ref->window), (gpointer *) &ref->window);

      g_source_unref ((GSource *) ref->source);

      g_slice_free (GdkMirWindowReference, ref);
    }
}

void
_gdk_mir_event_source_queue (GdkMirWindowReference *window_ref,
                             const MirEvent        *event)
{
  GdkMirEventSource *source = window_ref->source;
  GdkMirQueuedEvent *queued_event;

  /* We are in the wrong thread right now.  We absolutely cannot touch
   * the window.
   *
   * We can do pretty much anything we want with the source, though...
   */

  queued_event = g_slice_new (GdkMirQueuedEvent);
  g_atomic_int_inc (&window_ref->ref_count);
  queued_event->window_ref = window_ref;
  queued_event->event = event;

  g_mutex_lock (&source->mir_event_lock);
  g_queue_push_tail (&source->mir_events, queued_event);
  g_mutex_unlock (&source->mir_event_lock);

  g_main_context_wakeup (NULL);
}
