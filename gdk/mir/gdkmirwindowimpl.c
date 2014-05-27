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

#include <inttypes.h>

#include "config.h"

#include "gdk.h"
#include "gdkmir.h"
#include "gdkmir-private.h"

#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdkdisplayprivate.h"

#define GDK_TYPE_MIR_WINDOW_IMPL              (gdk_mir_window_impl_get_type ())
#define GDK_MIR_WINDOW_IMPL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImpl))
#define GDK_MIR_WINDOW_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImplClass))
#define GDK_IS_WINDOW_IMPL_MIR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_MIR))
#define GDK_IS_WINDOW_IMPL_MIR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_MIR))
#define GDK_MIR_WINDOW_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImplClass))

typedef struct _GdkMirWindowImpl GdkMirWindowImpl;
typedef struct _GdkMirWindowImplClass GdkMirWindowImplClass;

struct _GdkMirWindowImpl
{
  GdkWindowImpl parent_instance;

  /* Events to report */
  GdkEventMask event_mask;

  /* Desired surface attributes */
  int width;
  int height;
  MirSurfaceType surface_type; // FIXME
  MirSurfaceState surface_state;

  /* Pattern for background */
  cairo_pattern_t *background;

  /* Current button state for checking which buttons are being pressed / released */
  MirMotionButton button_state;

  /* Surface being rendered to (only exists when window visible) */
  MirSurface *surface;

  /* Cairo context for current frame */
  cairo_surface_t *cairo_surface;
};

struct _GdkMirWindowImplClass
{
  GdkWindowImplClass parent_class;
};

G_DEFINE_TYPE (GdkMirWindowImpl, gdk_mir_window_impl, GDK_TYPE_WINDOW_IMPL)

GdkWindowImpl *
_gdk_mir_window_impl_new (int width, int height, GdkEventMask event_mask)
{
  GdkMirWindowImpl *impl = g_object_new (GDK_TYPE_MIR_WINDOW_IMPL, NULL);

  impl->width = width;
  impl->height = height;
  impl->event_mask = event_mask;

  return GDK_WINDOW_IMPL (impl);
}

static void
gdk_mir_window_impl_init (GdkMirWindowImpl *impl)
{
}

static MirConnection *
get_connection (GdkWindow *window)
{
  return gdk_mir_display_get_mir_connection (gdk_window_get_display (window));
}

static void
set_surface_state (GdkMirWindowImpl *impl, MirSurfaceState state)
{
  impl->surface_state = state;
  if (impl->surface)
    mir_surface_set_state (impl->surface, state);
}

static void
print_modifiers (unsigned int modifiers)
{
  g_printerr (" Modifiers");
  if ((modifiers & mir_key_modifier_alt) != 0)
    g_printerr (" alt");
  if ((modifiers & mir_key_modifier_alt_left) != 0)
    g_printerr (" alt-left");
  if ((modifiers & mir_key_modifier_alt_right) != 0)
    g_printerr (" alt-right");
  if ((modifiers & mir_key_modifier_shift) != 0)
    g_printerr (" shift");
  if ((modifiers & mir_key_modifier_shift_left) != 0)
    g_printerr (" shift-left");
  if ((modifiers & mir_key_modifier_shift_right) != 0)
    g_printerr (" shift-right");
  if ((modifiers & mir_key_modifier_sym) != 0)
    g_printerr (" sym");
  if ((modifiers & mir_key_modifier_function) != 0)
    g_printerr (" function");
  if ((modifiers & mir_key_modifier_ctrl) != 0)
    g_printerr (" ctrl");
  if ((modifiers & mir_key_modifier_ctrl_left) != 0)
    g_printerr (" ctrl-left");
  if ((modifiers & mir_key_modifier_ctrl_right) != 0)
    g_printerr (" ctrl-right");
  if ((modifiers & mir_key_modifier_meta) != 0)
    g_printerr (" meta");
  if ((modifiers & mir_key_modifier_meta_left) != 0)
    g_printerr (" meta-left");
  if ((modifiers & mir_key_modifier_meta_right) != 0)
    g_printerr (" meta-right");
  if ((modifiers & mir_key_modifier_caps_lock) != 0)
    g_printerr (" caps-lock");
  if ((modifiers & mir_key_modifier_num_lock) != 0)
    g_printerr (" num-lock");
  if ((modifiers & mir_key_modifier_scroll_lock) != 0)
    g_printerr (" scroll-lock");
  g_printerr ("\n");
}

static void
print_key_event (const MirKeyEvent *event)
{
  g_printerr ("KEY\n");
  g_printerr (" Device %i\n", event->device_id);
  g_printerr (" Source %i\n", event->source_id);
  g_printerr (" Action ");
  switch (event->action)
    {
    case mir_key_action_down:
      g_printerr ("down");
      break;
    case mir_key_action_up:
      g_printerr ("up");
      break;
    case mir_key_action_multiple:
      g_printerr ("multiple");
      break;
    default:
      g_printerr ("%u", event->action);
      break;
    }
  g_printerr ("\n");
  g_printerr (" Flags");
  if ((event->flags & mir_key_flag_woke_here) != 0)
    g_printerr (" woke-here");
  if ((event->flags & mir_key_flag_soft_keyboard) != 0)
    g_printerr (" soft-keyboard");
  if ((event->flags & mir_key_flag_keep_touch_mode) != 0)
    g_printerr (" keep-touch-mode");
  if ((event->flags & mir_key_flag_from_system) != 0)
    g_printerr (" from-system");
  if ((event->flags & mir_key_flag_editor_action) != 0)
    g_printerr (" editor-action");
  if ((event->flags & mir_key_flag_canceled) != 0)
    g_printerr (" canceled");
  if ((event->flags & mir_key_flag_virtual_hard_key) != 0)
    g_printerr (" virtual-hard-key");
  if ((event->flags & mir_key_flag_long_press) != 0)
    g_printerr (" long-press");
  if ((event->flags & mir_key_flag_canceled_long_press) != 0)
    g_printerr (" canceled-long-press");
  if ((event->flags & mir_key_flag_tracking) != 0)
    g_printerr (" tracking");
  if ((event->flags & mir_key_flag_fallback) != 0)
    g_printerr (" fallback");
  g_printerr ("\n");
  print_modifiers (event->modifiers);
  g_printerr (" Key Code %i\n", event->key_code);
  g_printerr (" Scan Code %i\n", event->scan_code);
  g_printerr (" Repeat Count %i\n", event->repeat_count);
  g_printerr (" Down Time %lli\n", (long long int) event->down_time);
  g_printerr (" Event Time %lli\n", (long long int) event->event_time);
  g_printerr (" Is System Key %s\n", event->is_system_key ? "true" : "false");
}

static void
print_motion_event (const MirMotionEvent *event)
{
  size_t i;

  g_printerr ("MOTION\n");
  g_printerr (" Device %i\n", event->device_id);
  g_printerr (" Source %i\n", event->source_id);
  g_printerr (" Action ");
  switch (event->action)
    {
    case mir_motion_action_down:
      g_printerr ("down");
      break;
    case mir_motion_action_up:
      g_printerr ("up");
      break;
    case mir_motion_action_move:
      g_printerr ("move");
      break;
    case mir_motion_action_cancel:
      g_printerr ("cancel");
      break;
    case mir_motion_action_outside:
      g_printerr ("outside");
      break;
    case mir_motion_action_pointer_down:
      g_printerr ("pointer-down");
      break;
    case mir_motion_action_pointer_up:
      g_printerr ("pointer-up");
      break;
    case mir_motion_action_hover_move:
      g_printerr ("hover-move");
      break;
    case mir_motion_action_scroll:
      g_printerr ("scroll");
      break;
    case mir_motion_action_hover_enter:
      g_printerr ("hover-enter");
      break;
    case mir_motion_action_hover_exit:
      g_printerr ("hover-exit");
      break;
    default:
      g_printerr ("%u", event->action);
    }
  g_printerr ("\n");
  g_printerr (" Flags");
  switch (event->flags)
    {
    case mir_motion_flag_window_is_obscured:
      g_printerr (" window-is-obscured");
      break;
    }
  g_printerr ("\n");
  print_modifiers (event->modifiers);
  g_printerr (" Edge Flags %i\n", event->edge_flags);
  g_printerr (" Button State");
  switch (event->button_state)
    {
    case mir_motion_button_primary:
      g_printerr (" primary");
      break;
    case mir_motion_button_secondary:
      g_printerr (" secondary");
      break;
    case mir_motion_button_tertiary:
      g_printerr (" tertiary");
      break;
    case mir_motion_button_back:
      g_printerr (" back");
      break;
    case mir_motion_button_forward:
      g_printerr (" forward");
      break;
    }
  g_printerr ("\n");
  g_printerr (" Offset (%f, %f)\n", event->x_offset, event->y_offset);
  g_printerr (" Precision (%f, %f)\n", event->x_precision, event->y_precision);
  g_printerr (" Down Time %lli\n", (long long int) event->down_time);
  g_printerr (" Event Time %lli\n", (long long int) event->event_time);
  g_printerr (" Pointer Coordinates\n");
  for (i = 0; i < event->pointer_count; i++)
    {
      g_printerr ("  ID=%i location=(%f, %f) raw=(%f, %f) touch=(%f, %f) size=%f pressure=%f orientation=%f scroll=(%f, %f) tool=",
                  event->pointer_coordinates[i].id,
                  event->pointer_coordinates[i].x, event->pointer_coordinates[i].y,
                  event->pointer_coordinates[i].raw_x, event->pointer_coordinates[i].raw_y,
                  event->pointer_coordinates[i].touch_major, event->pointer_coordinates[i].touch_minor,
                  event->pointer_coordinates[i].size,
                  event->pointer_coordinates[i].pressure,
                  event->pointer_coordinates[i].orientation,
                  event->pointer_coordinates[i].hscroll, event->pointer_coordinates[i].vscroll);
      switch (event->pointer_coordinates[i].tool_type)
        {
        case mir_motion_tool_type_unknown:
          g_printerr ("unknown");
          break;
        case mir_motion_tool_type_finger:
          g_printerr ("finger");
          break;
        case mir_motion_tool_type_stylus:
          g_printerr ("stylus");
          break;
        case mir_motion_tool_type_mouse:
          g_printerr ("mouse");
          break;
        case mir_motion_tool_type_eraser:
          g_printerr ("eraser");
          break;
        default:
          g_printerr ("%u", event->pointer_coordinates[i].tool_type);
          break;
        }
      g_printerr ("\n");
    }
}

static void
print_surface_event (const MirSurfaceEvent *event)
{
  g_printerr ("SURFACE\n");
  g_printerr (" Surface %i\n", event->id);
  g_printerr (" Attribute ");
  switch (event->attrib)
    {
    case mir_surface_attrib_type:
      g_printerr ("type");
      break;
    case mir_surface_attrib_state:
      g_printerr ("state");
      break;
    case mir_surface_attrib_swapinterval:
      g_printerr ("swapinterval");
      break;
    case mir_surface_attrib_focus:
      g_printerr ("focus");
      break;
    default:
      g_printerr ("%u", event->attrib);
      break;
    }
  g_printerr ("\n");
  g_printerr (" Value %i\n", event->value);
}

static void
print_resize_event (const MirResizeEvent *event)
{
  g_printerr ("RESIZE\n");
  g_printerr (" Surface %i\n", event->surface_id);
  g_printerr (" Size (%i, %i)\n", event->width, event->height);
}

static void
print_event (const MirEvent *event)
{
  switch (event->type)
    {
    case mir_event_type_key:
      print_key_event (&event->key);
      break;
    case mir_event_type_motion:
      print_motion_event (&event->motion);
      break;
    case mir_event_type_surface:
      print_surface_event (&event->surface);
      break;
    case mir_event_type_resize:
      print_resize_event (&event->resize);
      break;
    default:
      g_printerr ("EVENT %u\n", event->type);
      break;
    }
}

static void
send_event (GdkWindow *window, GdkDevice *device, GdkEvent *event)
{
  GdkDisplay *display;
  GList *node;

  gdk_event_set_device (event, device);
  gdk_event_set_screen (event, gdk_display_get_screen (gdk_window_get_display (window), 0));
  event->any.window = g_object_ref (window);
  event->any.send_event = FALSE; // FIXME: What is this?

  display = gdk_window_get_display (window);
  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event, _gdk_display_get_next_serial (display));
}

static void
generate_key_event (GdkWindow *window, GdkEventType type, guint state, guint keyval, guint16 keycode, gboolean is_modifier)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->key.state = state;
  event->key.keyval = keyval;
  event->key.length = 0;
  event->key.string = NULL; // FIXME: Is this still needed?
  event->key.hardware_keycode = keycode;
  event->key.group = 0; // FIXME
  event->key.is_modifier = is_modifier;

  send_event (window, _gdk_mir_device_manager_get_keyboard (gdk_display_get_device_manager (gdk_window_get_display (window))), event);
}

static GdkDevice *
get_pointer (GdkWindow *window)
{
  return gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (gdk_window_get_display (window)));
}

void
generate_button_event (GdkWindow *window, GdkEventType type, gdouble x, gdouble y, guint button, guint state)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->button.x = x;
  event->button.y = y;
  event->button.state = state;
  event->button.button = button;

  send_event (window, get_pointer (window), event);
}

static void
generate_scroll_event (GdkWindow *window, gdouble x, gdouble y, gdouble delta_x, gdouble delta_y, guint state)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_SCROLL);
  event->scroll.x = x;
  event->scroll.y = y;
  event->scroll.state = state;
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;

  send_event (window, get_pointer (window), event);
}

static void
generate_motion_event (GdkWindow *window, gdouble x, gdouble y, guint state)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.state = state;
  event->motion.is_hint = FALSE;

  send_event (window, get_pointer (window), event);
}

static guint
get_modifier_state (unsigned int modifiers, unsigned int button_state)
{
  guint modifier_state = 0;

  if ((modifiers & mir_key_modifier_alt) != 0)
    modifier_state |= GDK_MOD1_MASK;
  if ((modifiers & mir_key_modifier_shift) != 0)
    modifier_state |= GDK_SHIFT_MASK;
  if ((modifiers & mir_key_modifier_ctrl) != 0)
    modifier_state |= GDK_CONTROL_MASK;
  if ((modifiers & mir_key_modifier_meta) != 0)
    modifier_state |= GDK_SUPER_MASK;
  if ((modifiers & mir_key_modifier_caps_lock) != 0)
    modifier_state |= GDK_LOCK_MASK;
  if ((button_state & mir_motion_button_primary) != 0)
    modifier_state |= GDK_BUTTON1_MASK;
  if ((button_state & mir_motion_button_secondary) != 0)
    modifier_state |= GDK_BUTTON3_MASK;
  if ((button_state & mir_motion_button_tertiary) != 0)
    modifier_state |= GDK_BUTTON2_MASK;

  return modifier_state;
}

static void
event_cb (MirSurface *surface, const MirEvent *event, void *context)
{
  GdkWindow *window = context;
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirMotionButton changed_button_state;
  GdkEventType event_type;
  gdouble x, y;
  guint modifier_state;
  gboolean is_modifier = FALSE;

  if (g_getenv ("GDK_MIR_LOG_EVENTS"))
    print_event (event);

  // FIXME: Only generate events if the window wanted them?
  switch (event->type)
    {
    case mir_event_type_key:
      modifier_state = get_modifier_state (event->key.modifiers, 0); // FIXME: Need to track button state

      switch (event->key.action)
        {
        case mir_key_action_down:
        case mir_key_action_up:
          // FIXME: Convert keycode
          // FIXME: is_modifier
          generate_key_event (window,
                              event->key.action == mir_key_action_down ? GDK_KEY_PRESS : GDK_KEY_RELEASE,
                              modifier_state,
                              event->key.key_code,
                              event->key.scan_code,
                              is_modifier);
          break;
        default:
        //case mir_key_action_multiple:
          // FIXME
          break;
        }
      break;
    case mir_event_type_motion:
      if (event->motion.pointer_count < 1)
        return;
      x = event->motion.pointer_coordinates[0].x;
      y = event->motion.pointer_coordinates[0].y;
      modifier_state = get_modifier_state (event->motion.modifiers, event->motion.button_state);

      /* Update which window has focus */
      _gdk_mir_pointer_set_location (get_pointer (window), x, y, window, modifier_state);

      switch (event->motion.action)
        {
        case mir_motion_action_down:
        case mir_motion_action_up:
          event_type = event->motion.action == mir_motion_action_down ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;
          changed_button_state = impl->button_state ^ event->motion.button_state;
          if ((event_type == GDK_BUTTON_PRESS && (impl->event_mask & GDK_BUTTON_PRESS_MASK) != 0) ||
              (event_type == GDK_BUTTON_RELEASE && (impl->event_mask & GDK_BUTTON_RELEASE_MASK) != 0))
            {
              if ((changed_button_state & mir_motion_button_primary) != 0)
                generate_button_event (window, event_type, x, y, GDK_BUTTON_PRIMARY, modifier_state);
              if ((changed_button_state & mir_motion_button_secondary) != 0)
                generate_button_event (window, event_type, x, y, GDK_BUTTON_SECONDARY, modifier_state);
              if ((changed_button_state & mir_motion_button_tertiary) != 0)
                generate_button_event (window, event_type, x, y, GDK_BUTTON_MIDDLE, modifier_state);
            }
            impl->button_state = event->motion.button_state;
          break;
        case mir_motion_action_scroll:
          if ((impl->event_mask & GDK_SMOOTH_SCROLL_MASK) != 0)
            generate_scroll_event (window, x, y, event->motion.pointer_coordinates[0].hscroll, event->motion.pointer_coordinates[0].vscroll, modifier_state);
          break;
        case mir_motion_action_move: // move with button
        case mir_motion_action_hover_move: // move without button
          if ((impl->event_mask & GDK_POINTER_MOTION_MASK) != 0 ||
              ((impl->event_mask & (GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK)) != 0 && (modifier_state & GDK_BUTTON1_MASK)) != 0 ||
              ((impl->event_mask & (GDK_BUTTON_MOTION_MASK | GDK_BUTTON2_MOTION_MASK)) != 0 && (modifier_state & GDK_BUTTON2_MASK)) != 0 ||
              ((impl->event_mask & (GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK)) != 0 && (modifier_state & GDK_BUTTON3_MASK)) != 0)
            generate_motion_event (window, x, y, modifier_state);
          break;
        }
      break;
    case mir_event_type_surface:
      switch (event->surface.attrib)
        {
        case mir_surface_attrib_type:
          break;
        case mir_surface_attrib_state:
          impl->surface_state = event->surface.value;
          // FIXME: notify
          break;
        case mir_surface_attrib_swapinterval:
          break;
        case mir_surface_attrib_focus:
          if (event->surface.value)
            gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FOCUSED);
          else
            gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FOCUSED, 0);
          break;
        default:
          break;
        }
      break;
    case mir_event_type_resize:
      // FIXME: Generate configure event
      break;
    default:
      // FIXME?
      break;
    }
}

static void
ensure_surface (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirPixelFormat formats[100], pixel_format = mir_pixel_format_invalid;
  unsigned int n_formats, i;
  MirSurfaceParameters parameters;
  MirEventDelegate event_delegate = { event_cb, window };

  if (impl->surface)
    return;

  // Should probably calculate this once?
  // Should prefer certain formats over others
  mir_connection_get_available_surface_formats (get_connection (window), formats, 100, &n_formats);
  for (i = 0; i < n_formats; i++)
    if (formats[i] == mir_pixel_format_argb_8888)
      {
        pixel_format = formats[i];
        break;
      }

  parameters.name = "GTK+ Mir";
  parameters.width = impl->width;
  parameters.height = impl->height;
  parameters.pixel_format = pixel_format;
  parameters.buffer_usage = mir_buffer_usage_software;
  parameters.output_id = mir_display_output_id_invalid;
  impl->surface = mir_connection_create_surface_sync (get_connection (window), &parameters);
  mir_surface_set_event_handler (impl->surface, &event_delegate); // FIXME: Ignore some events until shown
  set_surface_state (impl, impl->surface_state);
}

static void
send_buffer (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* Send the completed buffer to Mir */
  mir_surface_swap_buffers_sync (impl->surface);

  /* The Cairo context is no longer valid */
  if (impl->cairo_surface)
    {
      cairo_surface_destroy (impl->cairo_surface);
      impl->cairo_surface = NULL;
    }
}

static cairo_surface_t *
gdk_mir_window_impl_ref_cairo_surface (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_ref_cairo_surface\n");
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirGraphicsRegion region;
  cairo_format_t pixel_format = CAIRO_FORMAT_INVALID;
  cairo_surface_t *cairo_surface;
  cairo_t *c;

  if (impl->cairo_surface)
    {
      cairo_surface_reference (impl->cairo_surface);
      return impl->cairo_surface;
    }

  ensure_surface (window);

  mir_surface_get_graphics_region (impl->surface, &region);

  // FIXME: Should calculate this once
  switch (region.pixel_format)
    {
    case mir_pixel_format_argb_8888:
      pixel_format = CAIRO_FORMAT_ARGB32;
      break;
    default:
    case mir_pixel_format_abgr_8888:
    case mir_pixel_format_xbgr_8888:
    case mir_pixel_format_xrgb_8888:
    case mir_pixel_format_bgr_888:
        // uh-oh...
        g_printerr ("Unsupported pixel format %d\n", region.pixel_format);
        break;
    }

  cairo_surface = cairo_image_surface_create_for_data ((unsigned char *) region.vaddr,
                                                       pixel_format,
                                                       region.width,
                                                       region.height,
                                                       region.stride);
  impl->cairo_surface = cairo_surface_reference (cairo_surface);

  /* Draw background */
  c = cairo_create (impl->cairo_surface);
  if (impl->background)
    cairo_set_source (c, impl->background);
  else
    cairo_set_source_rgb (c, 1.0, 0.0, 0.0);
  cairo_paint (c);
  cairo_destroy (c);

  return cairo_surface;
}

static cairo_surface_t *
gdk_mir_window_impl_create_similar_image_surface (GdkWindow      *window,
                                                  cairo_format_t  format,
                                                  int             width,
                                                  int             height)
{
  g_printerr ("gdk_mir_window_impl_create_similar_image_surface\n");
  return cairo_image_surface_create (format, width, height);
}

static void
gdk_mir_window_impl_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_mir_window_impl_parent_class)->finalize (object);
}

static void
gdk_mir_window_impl_show (GdkWindow *window,
                          gboolean   already_mapped)
{
  cairo_surface_t *s;

  g_printerr ("gdk_mir_window_impl_show\n");

  /* Make sure there's a surface to see */
  ensure_surface (window);

  /* Make sure something is rendered and then show first frame */
  s = gdk_mir_window_impl_ref_cairo_surface (window);
  send_buffer (window);
  cairo_surface_destroy (s);
}

static void
gdk_mir_window_impl_hide (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_hide\n");
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->surface)
    {
      mir_surface_release_sync (impl->surface);
      impl->surface = NULL;
    }
}

static void
gdk_mir_window_impl_withdraw (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_withdraw\n");
}

static void
gdk_mir_window_impl_raise (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_raise\n");
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_lower (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_lower\n");
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_restack_under (GdkWindow *window,
                                   GList     *native_siblings)
{
  //g_printerr ("gdk_mir_window_impl_restack_under\n");
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_restack_toplevel (GdkWindow *window,
                                      GdkWindow *sibling,
                                      gboolean   above)
{
  //g_printerr ("gdk_mir_window_impl_restack_toplevel\n");
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_move_resize (GdkWindow *window,
                                 gboolean   with_move,
                                 gint       x,
                                 gint       y,
                                 gint       width,
                                 gint       height)
{
  g_printerr ("gdk_mir_window_impl_move_resize\n");
}

static void
gdk_mir_window_impl_set_background (GdkWindow       *window,
                                    cairo_pattern_t *pattern)
{
  //g_printerr ("gdk_mir_window_impl_set_background\n");
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->background)
    cairo_pattern_destroy (impl->background);
  impl->background = cairo_pattern_reference (pattern);
}

static GdkEventMask
gdk_mir_window_impl_get_events (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_events\n");
  return GDK_MIR_WINDOW_IMPL (window)->event_mask;
}

static void
gdk_mir_window_impl_set_events (GdkWindow    *window,
                                GdkEventMask  event_mask)
{
  //g_printerr ("gdk_mir_window_impl_set_events\n");
  GDK_MIR_WINDOW_IMPL (window->impl)->event_mask = event_mask;
}

static gboolean
gdk_mir_window_impl_reparent (GdkWindow *window,
                              GdkWindow *new_parent,
                              gint       x,
                              gint       y)
{
  g_printerr ("gdk_mir_window_impl_reparent\n");
  return FALSE;
}

static void
gdk_mir_window_impl_set_device_cursor (GdkWindow *window,
                                       GdkDevice *device,
                                       GdkCursor *cursor)
{
  //g_printerr ("gdk_mir_window_impl_set_device_cursor\n");
  /* We don't support cursors yet... */
}

static void
gdk_mir_window_impl_get_geometry (GdkWindow *window,
                                  gint      *x,
                                  gint      *y,
                                  gint      *width,
                                  gint      *height)
{
  //g_printerr ("gdk_mir_window_impl_get_geometry\n");
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (x)
    *x = 0; // FIXME
  if (y)
    *y = 0; // FIXME
  if (width)
    *width = impl->width;
  if (height)
    *height = impl->height;
}

static gint
gdk_mir_window_impl_get_root_coords (GdkWindow *window,
                                     gint       x,
                                     gint       y,
                                     gint      *root_x,
                                     gint      *root_y)
{
  //g_printerr ("gdk_mir_window_impl_get_root_coords\n");

  if (root_x)
    *root_x = x; // FIXME
  if (root_y)
    *root_y = y; // FIXME

  return 1;
}

static gboolean
gdk_mir_window_impl_get_device_state (GdkWindow       *window,
                                      GdkDevice       *device,
                                      gdouble         *x,
                                      gdouble         *y,
                                      GdkModifierType *mask)
{
  g_printerr ("gdk_mir_window_impl_get_device_state\n");
  return FALSE;
}

static gboolean
gdk_mir_window_impl_begin_paint_region (GdkWindow            *window,
                                        const cairo_region_t *region)
{
  g_printerr ("gdk_mir_window_impl_begin_paint_region\n");
  /* Indicate we are ready to be drawn onto directly? */
  return FALSE;
}

static void
gdk_mir_window_impl_end_paint (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_end_paint\n");
  send_buffer (window);
}

static cairo_region_t *
gdk_mir_window_impl_get_shape (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_shape\n");
  return NULL;
}

static cairo_region_t *
gdk_mir_window_impl_get_input_shape (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_input_shape\n");
  return NULL;
}

static void
gdk_mir_window_impl_shape_combine_region (GdkWindow            *window,
                                          const cairo_region_t *shape_region,
                                          gint                  offset_x,
                                          gint                  offset_y)
{
  g_printerr ("gdk_mir_window_impl_shape_combine_region\n");
}

static void
gdk_mir_window_impl_input_shape_combine_region (GdkWindow            *window,
                                                const cairo_region_t *shape_region,
                                                gint                  offset_x,
                                                gint                  offset_y)
{
  g_printerr ("gdk_mir_window_impl_input_shape_combine_region\n");
}

static gboolean
gdk_mir_window_impl_set_static_gravities (GdkWindow *window,
                                          gboolean   use_static)
{
  g_printerr ("gdk_mir_window_impl_set_static_gravities\n");
  return FALSE;
}

static gboolean
gdk_mir_window_impl_queue_antiexpose (GdkWindow      *window,
                                      cairo_region_t *area)
{
  g_printerr ("gdk_mir_window_impl_queue_antiexpose\n");
  return FALSE;
}

static void
gdk_mir_window_impl_destroy (GdkWindow *window,
                             gboolean   recursing,
                             gboolean   foreign_destroy)
{
  g_printerr ("gdk_mir_window_impl_destroy\n");
}

static void
gdk_mir_window_impl_destroy_foreign (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_destroy_foreign\n");
}

static cairo_surface_t *
gdk_mir_window_impl_resize_cairo_surface (GdkWindow       *window,
                                          cairo_surface_t *surface,
                                          gint             width,
                                          gint             height)
{
  g_printerr ("gdk_mir_window_impl_resize_cairo_surface\n");
  return surface;
}

static void
gdk_mir_window_impl_focus (GdkWindow *window,
                      guint32    timestamp)
{
  g_printerr ("gdk_mir_window_impl_focus\n");
}

static void
gdk_mir_window_impl_set_type_hint (GdkWindow         *window,
                                   GdkWindowTypeHint  hint)
{
  g_printerr ("gdk_mir_window_impl_set_type_hint\n");
}

static GdkWindowTypeHint
gdk_mir_window_impl_get_type_hint (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_type_hint\n");
  return GDK_WINDOW_TYPE_HINT_NORMAL;
}

void
gdk_mir_window_impl_set_modal_hint (GdkWindow *window,
                                    gboolean   modal)
{
  //g_printerr ("gdk_mir_window_impl_set_modal_hint\n");
  /* Mir doesn't support modal windows */
}

static void
gdk_mir_window_impl_set_skip_taskbar_hint (GdkWindow *window,
                                           gboolean   skips_taskbar)
{
  g_printerr ("gdk_mir_window_impl_set_skip_taskbar_hint\n");
}

static void
gdk_mir_window_impl_set_skip_pager_hint (GdkWindow *window,
                                         gboolean   skips_pager)
{
  g_printerr ("gdk_mir_window_impl_set_skip_pager_hint\n");
}

static void
gdk_mir_window_impl_set_urgency_hint (GdkWindow *window,
                                      gboolean   urgent)
{
  g_printerr ("gdk_mir_window_impl_set_urgency_hint\n");
}

static void
gdk_mir_window_impl_set_geometry_hints (GdkWindow         *window,
                                        const GdkGeometry *geometry,
                                        GdkWindowHints     geom_mask)
{
  //g_printerr ("gdk_mir_window_impl_set_geometry_hints\n");
  //FIXME: ?
}

static void
gdk_mir_window_impl_set_title (GdkWindow   *window,
                               const gchar *title)
{
  g_printerr ("gdk_mir_window_impl_set_title\n");
}

static void
gdk_mir_window_impl_set_role (GdkWindow   *window,
                              const gchar *role)
{
  g_printerr ("gdk_mir_window_impl_set_role\n");
}

static void
gdk_mir_window_impl_set_startup_id (GdkWindow   *window,
                                    const gchar *startup_id)
{
  g_printerr ("gdk_mir_window_impl_set_startup_id\n");
}

static void
gdk_mir_window_impl_set_transient_for (GdkWindow *window,
                                       GdkWindow *parent)
{
  g_printerr ("gdk_mir_window_impl_set_transient_for\n");
}

static void
gdk_mir_window_impl_get_root_origin (GdkWindow *window,
                                     gint      *x,
                                     gint      *y)
{
  g_printerr ("gdk_mir_window_impl_get_root_origin\n");
}

static void
gdk_mir_window_impl_get_frame_extents (GdkWindow    *window,
                                       GdkRectangle *rect)
{
  g_printerr ("gdk_mir_window_impl_get_frame_extents\n");
}

static void
gdk_mir_window_impl_set_override_redirect (GdkWindow *window,
                                           gboolean   override_redirect)
{
  g_printerr ("gdk_mir_window_impl_set_override_redirect\n");
}

static void
gdk_mir_window_impl_set_accept_focus (GdkWindow *window,
                                      gboolean   accept_focus)
{
  //g_printerr ("gdk_mir_window_impl_set_accept_focus\n");
  /* Mir clients cannot control focus */
}

static void
gdk_mir_window_impl_set_focus_on_map (GdkWindow *window,
                                      gboolean focus_on_map)
{
  //g_printerr ("gdk_mir_window_impl_set_focus_on_map\n");
  /* Mir clients cannot control focus */
}

static void
gdk_mir_window_impl_set_icon_list (GdkWindow *window,
                                   GList     *pixbufs)
{
  //g_printerr ("gdk_mir_window_impl_set_icon_list\n");
  // ??
}

static void
gdk_mir_window_impl_set_icon_name (GdkWindow   *window,
                                   const gchar *name)
{
  g_printerr ("gdk_mir_window_impl_set_icon_name\n");
}

static void
gdk_mir_window_impl_iconify (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_iconify\n");
  /* We don't support iconification */
}

static void
gdk_mir_window_impl_deiconify (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_deiconify\n");
  /* We don't support iconification */
}

static void
gdk_mir_window_impl_stick (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_stick\n");
  /* We do not support stick/unstick in Mir */
}

static void
gdk_mir_window_impl_unstick (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_unstick\n");
  /* We do not support stick/unstick in Mir */
}

static void
gdk_mir_window_impl_maximize (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_maximize\n");
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_maximized);
}

static void
gdk_mir_window_impl_unmaximize (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_unmaximize\n");
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_restored);
}

static void
gdk_mir_window_impl_fullscreen (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_fullscreen\n");
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_fullscreen);
}

static void
gdk_mir_window_impl_unfullscreen (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_unfullscreen\n");
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_restored);
}

static void
gdk_mir_window_impl_set_keep_above (GdkWindow *window,
                                    gboolean   setting)
{
  //g_printerr ("gdk_mir_window_impl_set_keep_above\n");
  /* We do not support keep above/below in Mir */
}

static void
gdk_mir_window_impl_set_keep_below (GdkWindow *window,
                                    gboolean setting)
{
  //g_printerr ("gdk_mir_window_impl_set_keep_below\n");
  /* We do not support keep above/below in Mir */
}

static GdkWindow *
gdk_mir_window_impl_get_group (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_group\n");
  return NULL;
}

static void
gdk_mir_window_impl_set_group (GdkWindow *window,
                               GdkWindow *leader)
{
  g_printerr ("gdk_mir_window_impl_set_group\n");
}

static void
gdk_mir_window_impl_set_decorations (GdkWindow       *window,
                                     GdkWMDecoration  decorations)
{
  g_printerr ("gdk_mir_window_impl_set_decorations (%u)\n", decorations);
}

static gboolean
gdk_mir_window_impl_get_decorations (GdkWindow       *window,
                                     GdkWMDecoration *decorations)
{
  g_printerr ("gdk_mir_window_impl_get_decorations\n");
  return FALSE;
}

static void
gdk_mir_window_impl_set_functions (GdkWindow     *window,
                                   GdkWMFunction  functions)
{
  g_printerr ("gdk_mir_window_impl_set_functions\n");
}

static void
gdk_mir_window_impl_begin_resize_drag (GdkWindow     *window,
                                       GdkWindowEdge  edge,
                                       GdkDevice     *device,
                                       gint           button,
                                       gint           root_x,
                                       gint           root_y,
                                       guint32        timestamp)
{
  g_printerr ("gdk_mir_window_impl_begin_resize_drag\n");
}

static void
gdk_mir_window_impl_begin_move_drag (GdkWindow *window,
                                     GdkDevice *device,
                                     gint       button,
                                     gint       root_x,
                                     gint       root_y,
                                     guint32    timestamp)
{
  g_printerr ("gdk_mir_window_impl_begin_move_drag\n");
}

static void
gdk_mir_window_impl_enable_synchronized_configure (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_enable_synchronized_configure\n");
}

static void
gdk_mir_window_impl_configure_finished (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_configure_finished\n");
}

static void
gdk_mir_window_impl_set_opacity (GdkWindow *window,
                                 gdouble    opacity)
{
  //g_printerr ("gdk_mir_window_impl_set_opacity\n");
  // FIXME
}

static void
gdk_mir_window_impl_set_composited (GdkWindow *window,
                                    gboolean   composited)
{
  g_printerr ("gdk_mir_window_impl_set_composited\n");
}

static void
gdk_mir_window_impl_destroy_notify (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_destroy_notify\n");
}

static GdkDragProtocol
gdk_mir_window_impl_get_drag_protocol (GdkWindow *window,
                                       GdkWindow **target)
{
  g_printerr ("gdk_mir_window_impl_get_drag_protocol\n");
  return 0;
}

static void
gdk_mir_window_impl_register_dnd (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_register_dnd\n");
}

static GdkDragContext *
gdk_mir_window_impl_drag_begin (GdkWindow *window,
                                GdkDevice *device,
                                GList     *targets)
{
  g_printerr ("gdk_mir_window_impl_drag_begin\n");
  return NULL;
}

static void
gdk_mir_window_impl_process_updates_recurse (GdkWindow      *window,
                                             cairo_region_t *region)
{
  g_printerr ("gdk_mir_window_impl_process_updates_recurse\n");
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  cairo_rectangle_int_t rectangle;

  /* We redraw the whole region, but we should track the buffers and only redraw what has changed since we sent this buffer */
  rectangle.x = 0;
  rectangle.y = 0;
  rectangle.width = impl->width;
  rectangle.height = impl->height;
  cairo_region_union_rectangle (region, &rectangle);

  _gdk_window_process_updates_recurse (window, region);
}

static void
gdk_mir_window_impl_sync_rendering (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_sync_rendering\n");
  // FIXME: Only used for benchmarking
}

static gboolean
gdk_mir_window_impl_simulate_key (GdkWindow       *window,
                                  gint             x,
                                  gint             y,
                                  guint            keyval,
                                  GdkModifierType  modifiers,
                                  GdkEventType     key_pressrelease)
{
  g_printerr ("gdk_mir_window_impl_simulate_key\n");
  return FALSE;
}

static gboolean
gdk_mir_window_impl_simulate_button (GdkWindow       *window,
                                     gint             x,
                                     gint             y,
                                     guint            button,
                                     GdkModifierType  modifiers,
                                     GdkEventType     button_pressrelease)
{
  g_printerr ("gdk_mir_window_impl_simulate_button\n");
  return FALSE;
}

static gboolean
gdk_mir_window_impl_get_property (GdkWindow   *window,
                                  GdkAtom      property,
                                  GdkAtom      type,
                                  gulong       offset,
                                  gulong       length,
                                  gint         pdelete,
                                  GdkAtom     *actual_property_type,
                                  gint        *actual_format_type,
                                  gint        *actual_length,
                                  guchar     **data)
{
  g_printerr ("gdk_mir_window_impl_get_property\n");
  return FALSE;
}

static void
gdk_mir_window_impl_change_property (GdkWindow    *window,
                                     GdkAtom       property,
                                     GdkAtom       type,
                                     gint          format,
                                     GdkPropMode   mode,
                                     const guchar *data,
                                     gint          nelements)
{
  g_printerr ("gdk_mir_window_impl_change_property\n");
}

static void
gdk_mir_window_impl_delete_property (GdkWindow *window,
                                     GdkAtom    property)
{
  g_printerr ("gdk_mir_window_impl_delete_property\n");
}

static gint
gdk_mir_window_impl_get_scale_factor (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_scale_factor\n");
  /* Don't support monitor scaling */
  return 1;
}

static void
gdk_mir_window_impl_set_opaque_region (GdkWindow      *window,
                                       cairo_region_t *region)
{
  //g_printerr ("gdk_mir_window_impl_set_opaque_region\n");
  /* FIXME: An optimisation to tell the compositor which regions of the window are fully transparent */
}

static void
gdk_mir_window_impl_class_init (GdkMirWindowImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_mir_window_impl_finalize;

  impl_class->ref_cairo_surface = gdk_mir_window_impl_ref_cairo_surface;
  impl_class->create_similar_image_surface = gdk_mir_window_impl_create_similar_image_surface;
  impl_class->show = gdk_mir_window_impl_show;
  impl_class->hide = gdk_mir_window_impl_hide;
  impl_class->withdraw = gdk_mir_window_impl_withdraw;
  impl_class->raise = gdk_mir_window_impl_raise;
  impl_class->lower = gdk_mir_window_impl_lower;
  impl_class->restack_under = gdk_mir_window_impl_restack_under;
  impl_class->restack_toplevel = gdk_mir_window_impl_restack_toplevel;
  impl_class->move_resize = gdk_mir_window_impl_move_resize;
  impl_class->set_background = gdk_mir_window_impl_set_background;
  impl_class->get_events = gdk_mir_window_impl_get_events;
  impl_class->set_events = gdk_mir_window_impl_set_events;
  impl_class->reparent = gdk_mir_window_impl_reparent;
  impl_class->set_device_cursor = gdk_mir_window_impl_set_device_cursor;
  impl_class->get_geometry = gdk_mir_window_impl_get_geometry;
  impl_class->get_root_coords = gdk_mir_window_impl_get_root_coords;
  impl_class->get_device_state = gdk_mir_window_impl_get_device_state;
  impl_class->begin_paint_region = gdk_mir_window_impl_begin_paint_region;
  impl_class->end_paint = gdk_mir_window_impl_end_paint;
  impl_class->get_shape = gdk_mir_window_impl_get_shape;
  impl_class->get_input_shape = gdk_mir_window_impl_get_input_shape;
  impl_class->shape_combine_region = gdk_mir_window_impl_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_mir_window_impl_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_mir_window_impl_set_static_gravities;
  impl_class->queue_antiexpose = gdk_mir_window_impl_queue_antiexpose;
  impl_class->destroy = gdk_mir_window_impl_destroy;
  impl_class->destroy_foreign = gdk_mir_window_impl_destroy_foreign;
  impl_class->resize_cairo_surface = gdk_mir_window_impl_resize_cairo_surface;
  impl_class->focus = gdk_mir_window_impl_focus;
  impl_class->set_type_hint = gdk_mir_window_impl_set_type_hint;
  impl_class->get_type_hint = gdk_mir_window_impl_get_type_hint;
  impl_class->set_modal_hint = gdk_mir_window_impl_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_mir_window_impl_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_mir_window_impl_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_mir_window_impl_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_mir_window_impl_set_geometry_hints;
  impl_class->set_title = gdk_mir_window_impl_set_title;
  impl_class->set_role = gdk_mir_window_impl_set_role;
  impl_class->set_startup_id = gdk_mir_window_impl_set_startup_id;
  impl_class->set_transient_for = gdk_mir_window_impl_set_transient_for;
  impl_class->get_root_origin = gdk_mir_window_impl_get_root_origin;
  impl_class->get_frame_extents = gdk_mir_window_impl_get_frame_extents;
  impl_class->set_override_redirect = gdk_mir_window_impl_set_override_redirect;
  impl_class->set_accept_focus = gdk_mir_window_impl_set_accept_focus;
  impl_class->set_focus_on_map = gdk_mir_window_impl_set_focus_on_map;
  impl_class->set_icon_list = gdk_mir_window_impl_set_icon_list;
  impl_class->set_icon_name = gdk_mir_window_impl_set_icon_name;
  impl_class->iconify = gdk_mir_window_impl_iconify;
  impl_class->deiconify = gdk_mir_window_impl_deiconify;
  impl_class->stick = gdk_mir_window_impl_stick;
  impl_class->unstick = gdk_mir_window_impl_unstick;
  impl_class->maximize = gdk_mir_window_impl_maximize;
  impl_class->unmaximize = gdk_mir_window_impl_unmaximize;
  impl_class->fullscreen = gdk_mir_window_impl_fullscreen;
  impl_class->unfullscreen = gdk_mir_window_impl_unfullscreen;
  impl_class->set_keep_above = gdk_mir_window_impl_set_keep_above;
  impl_class->set_keep_below = gdk_mir_window_impl_set_keep_below;
  impl_class->get_group = gdk_mir_window_impl_get_group;
  impl_class->set_group = gdk_mir_window_impl_set_group;
  impl_class->set_decorations = gdk_mir_window_impl_set_decorations;
  impl_class->get_decorations = gdk_mir_window_impl_get_decorations;
  impl_class->set_functions = gdk_mir_window_impl_set_functions;
  impl_class->begin_resize_drag = gdk_mir_window_impl_begin_resize_drag;
  impl_class->begin_move_drag = gdk_mir_window_impl_begin_move_drag;
  impl_class->enable_synchronized_configure = gdk_mir_window_impl_enable_synchronized_configure;
  impl_class->configure_finished = gdk_mir_window_impl_configure_finished;
  impl_class->set_opacity = gdk_mir_window_impl_set_opacity;
  impl_class->set_composited = gdk_mir_window_impl_set_composited;
  impl_class->destroy_notify = gdk_mir_window_impl_destroy_notify;
  impl_class->get_drag_protocol = gdk_mir_window_impl_get_drag_protocol;
  impl_class->register_dnd = gdk_mir_window_impl_register_dnd;
  impl_class->drag_begin = gdk_mir_window_impl_drag_begin;
  impl_class->process_updates_recurse = gdk_mir_window_impl_process_updates_recurse;
  impl_class->sync_rendering = gdk_mir_window_impl_sync_rendering;
  impl_class->simulate_key = gdk_mir_window_impl_simulate_key;
  impl_class->simulate_button = gdk_mir_window_impl_simulate_button;
  impl_class->get_property = gdk_mir_window_impl_get_property;
  impl_class->change_property = gdk_mir_window_impl_change_property;
  impl_class->delete_property = gdk_mir_window_impl_delete_property;
  impl_class->get_scale_factor = gdk_mir_window_impl_get_scale_factor;
  impl_class->set_opaque_region = gdk_mir_window_impl_set_opaque_region;
}
