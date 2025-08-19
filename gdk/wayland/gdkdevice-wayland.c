/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include "gdkdevice-wayland-private.h"
#include "gdksurface-wayland-private.h"
#include "gdkcursor-wayland.h"
#include "gdkkeymap-wayland.h"
#include "gdkcursorprivate.h"
#include "gdkeventsource.h"
#include "gdkeventsprivate.h"

#define  GTK_SHELL_FIXED_WL_SURFACE_OFFSET_VERSION 6

#define GDK_SLOT_TO_EVENT_SEQUENCE(s) ((GdkEventSequence *) GUINT_TO_POINTER((s) + 1))
#define GDK_EVENT_SEQUENCE_TO_SLOT(s) (GPOINTER_TO_UINT(s) - 1)

typedef struct
{
  GdkWaylandTouchData *emulating_touch; /* Only used on wd->logical_touch */
  GdkWaylandPointerData *pointer;
} GdkWaylandDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdkWaylandDevice, gdk_wayland_device, GDK_TYPE_DEVICE)

static void
gdk_wayland_device_set_surface_cursor (GdkDevice  *device,
                                       GdkSurface *surface,
                                       GdkCursor  *cursor)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);

  if (device == seat->logical_touch)
    return;

  if (seat->grab_cursor)
    cursor = seat->grab_cursor;

  if (pointer->cursor != NULL &&
      cursor != NULL &&
      gdk_cursor_equal (cursor, pointer->cursor))
    return;

  if (cursor == NULL)
    {
      if (!pointer->cursor_is_default)
        {
          g_clear_object (&pointer->cursor);
          pointer->cursor = gdk_cursor_new_from_name ("default", NULL);
          pointer->cursor_is_default = TRUE;

          gdk_wayland_device_update_surface_cursor (device);
        }
      else
        {
          /* Nothing to do, we'already using the default cursor */
        }
    }
  else
    {
      g_set_object (&pointer->cursor, cursor);
      pointer->cursor_is_default = FALSE;

      gdk_wayland_device_update_surface_cursor (device);
    }
}

static GdkGrabStatus
gdk_wayland_device_grab (GdkDevice    *device,
                         GdkSurface   *surface,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkSurface   *confine_to,
                         GdkCursor    *cursor,
                         uint32_t      time_)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);

  if (GDK_IS_DRAG_SURFACE (surface) &&
      gdk_surface_get_mapped (surface))
    {
      g_warning ("Surface %p is already mapped at the time of grabbing. "
                 "gdk_seat_grab() should be used to simultaneously grab input "
                 "and show this popup. You may find oddities ahead.",
                 surface);
    }

  gdk_wayland_device_maybe_emit_grab_crossing (device, surface, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      gdk_wayland_surface_inhibit_shortcuts (surface,
                                             gdk_device_get_seat (device));
      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */
      if (pointer->grab_surface != NULL &&
          time_ != 0 && pointer->grab_time > time_)
        {
          return GDK_GRAB_ALREADY_GRABBED;
        }

      if (time_ == 0)
        time_ = pointer->time;

      pointer->grab_surface = surface;
      pointer->grab_time = time_;
      _gdk_wayland_surface_set_grab_seat (surface, GDK_SEAT (wayland_seat));

      g_clear_object (&wayland_seat->cursor);

      if (cursor)
        wayland_seat->cursor = g_object_ref (cursor);

      gdk_wayland_device_update_surface_cursor (device);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_device_ungrab (GdkDevice *device,
                           uint32_t   time_)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);
  GdkSurface *prev_focus;

  prev_focus = gdk_wayland_device_maybe_emit_ungrab_crossing (device, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      if (prev_focus)
        gdk_wayland_surface_restore_shortcuts (prev_focus,
                                              gdk_device_get_seat (device));
    }
  else
    {
      /* Device is a pointer */
      gdk_wayland_device_update_surface_cursor (device);

      if (pointer->grab_surface)
        _gdk_wayland_surface_set_grab_seat (pointer->grab_surface,
                                           NULL);
    }
}

static GdkSurface *
gdk_wayland_device_surface_at_position (GdkDevice       *device,
                                        double          *win_x,
                                        double          *win_y,
                                        GdkModifierType *mask)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;

  pointer = gdk_wayland_device_get_pointer (wayland_device);

  if (!pointer)
    return NULL;

  if (win_x)
    *win_x = pointer->surface_x;
  if (win_y)
    *win_y = pointer->surface_y;
  if (mask)
    *mask = gdk_wayland_device_get_modifiers (device);

  return pointer->focus;
}

static void
gdk_wayland_device_class_init (GdkWaylandDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_wayland_device_set_surface_cursor;
  device_class->grab = gdk_wayland_device_grab;
  device_class->ungrab = gdk_wayland_device_ungrab;
  device_class->surface_at_position = gdk_wayland_device_surface_at_position;
}

static void
gdk_wayland_device_init (GdkWaylandDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_AXIS_Y, 0, 0, 1);
}

GdkWaylandPointerData *
gdk_wayland_device_get_pointer (GdkWaylandDevice *wayland_device)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  return priv->pointer;
}

void
gdk_wayland_device_set_pointer (GdkWaylandDevice      *wayland_device,
                                GdkWaylandPointerData *pointer)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  priv->pointer = pointer;
}

GdkWaylandTouchData *
gdk_wayland_device_get_emulating_touch (GdkWaylandDevice *wayland_device)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  return priv->emulating_touch;
}

void
gdk_wayland_device_set_emulating_touch (GdkWaylandDevice    *wayland_device,
                                        GdkWaylandTouchData *touch)
{
  GdkWaylandDevicePrivate *priv =
    gdk_wayland_device_get_instance_private (wayland_device);

  priv->emulating_touch = touch;
}


static const struct
{
  const char *cursor_name;
  unsigned int shape;
  unsigned int version;
} shape_map[] = {
  { "default", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, 1 },
  { "context-menu", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU, 1 },
  { "help", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP, 1 },
  { "pointer", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER, 1 },
  { "progress", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS },
  { "wait", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT, 1 },
  { "cell", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL, 1 },
  { "crosshair", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR, 1 },
  { "text", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT, 1 },
  { "vertical-text", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT, 1 },
  { "alias", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS, 1 },
  { "copy", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY, 1 },
  { "move", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE, 1 },
  { "dnd-move", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE, 1 },
  { "no-drop", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP, 1 },
  { "not-allowed", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED, 1 },
  { "grab", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB, 1 },
  { "grabbing", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING, 1 },
  { "e-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE, 1 },
  { "n-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE, 1 },
  { "ne-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE, 1 },
  { "nw-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE, 1 },
  { "s-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE, 1 },
  { "se-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE, 1 },
  { "sw-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE, 1 },
  { "w-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE, 1 },
  { "ew-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE, 1 },
  { "ns-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE, 1 },
  { "nesw-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE, 1 },
  { "nwse-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE, 1 },
  { "col-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE, 1 },
  { "row-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE, 1 },
  { "all-scroll", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL, 1 },
  { "zoom-in", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN, 1 },
  { "zoom-out", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT, 1 },
  { "all-scroll", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL, 1 },
  /* the following a v2 additions, with a fallback for v1 */
  { "dnd-ask", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK, 2 },
  { "dnd-ask", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU, 1 },
  { "all-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE, 2 },
  { "all-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE, 1 },
};

static unsigned int
_gdk_wayland_cursor_get_shape (GdkCursor *cursor,
                               int        version)
{
  gsize i;
  const char *cursor_name;

  cursor_name = gdk_cursor_get_name (cursor);
  if (cursor_name == NULL ||
      g_str_equal (cursor_name, "none"))
    return 0;

  for (i = 0; i < G_N_ELEMENTS (shape_map); i++)
    {
      if (g_str_equal (shape_map[i].cursor_name, cursor_name) &&
          version >= shape_map[i].version)
        return shape_map[i].shape;
    }

  return 0;
}

void
gdk_wayland_device_update_surface_cursor (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer = gdk_wayland_device_get_pointer (wayland_device);
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (seat->display);
  struct wl_buffer *buffer;
  int x, y, w, h;
  double preferred_scale, scale;
  GdkWaylandTabletData *tablet;
  unsigned int shape;
  gboolean use_surface_offset;

  tablet = gdk_wayland_seat_find_tablet (seat, device);

  if (!pointer->cursor)
    return;

  if (tablet && !tablet->current_tool)
    return;

  if (GDK_WAYLAND_DISPLAY (seat->display)->cursor_shape)
    {
      shape = _gdk_wayland_cursor_get_shape (pointer->cursor,
                                             wp_cursor_shape_manager_v1_get_version (GDK_WAYLAND_DISPLAY (seat->display)->cursor_shape));
      if (shape != 0)
        {
          if (pointer->cursor_shape == shape)
            return;

          if (tablet && tablet->current_tool->shape_device)
            {
              pointer->has_cursor_surface = FALSE;
              pointer->cursor_shape = shape;
              wp_cursor_shape_device_v1_set_shape (tablet->current_tool->shape_device, pointer->enter_serial, shape);
              return;
            }
          else if (seat->wl_pointer && pointer->shape_device)
            {
              pointer->has_cursor_surface = FALSE;
              pointer->cursor_shape = shape;
              wp_cursor_shape_device_v1_set_shape (pointer->shape_device, pointer->enter_serial, shape);
              return;
            }
        }
    }

  preferred_scale = gdk_fractional_scale_to_double (&pointer->preferred_scale);

  buffer = _gdk_wayland_cursor_get_buffer (GDK_WAYLAND_DISPLAY (seat->display),
                                           pointer->cursor,
                                           preferred_scale,
                                           &x, &y, &w, &h,
                                           &scale);

  use_surface_offset =
    pointer->has_cursor_surface &&
    wl_surface_get_version (pointer->pointer_surface) >=
    WL_SURFACE_OFFSET_SINCE_VERSION &&
    (!wayland_display->gtk_shell ||
     gtk_shell1_get_version (wayland_display->gtk_shell) >=
     GTK_SHELL_FIXED_WL_SURFACE_OFFSET_VERSION);

  if (use_surface_offset)
    {
      /* We already have the surface attached to the cursor, change the
       * offset to adapt to the new buffer.
       */
      wl_surface_offset (pointer->pointer_surface,
                         pointer->cursor_hotspot_x - x,
                         pointer->cursor_hotspot_y - y);
    }

  if (buffer)
    {
      wl_surface_attach (pointer->pointer_surface, buffer, 0, 0);
      if (pointer->pointer_surface_viewport)
        {
          wp_viewport_set_source (pointer->pointer_surface_viewport,
                                  wl_fixed_from_int (0),
                                  wl_fixed_from_int (0),
                                  wl_fixed_from_double (w),
                                  wl_fixed_from_double (h));
          wp_viewport_set_destination (pointer->pointer_surface_viewport, w * scale, h * scale);
        }
      wl_surface_damage (pointer->pointer_surface,  0, 0, w, h);
      wl_surface_commit (pointer->pointer_surface);
    }
  else
    {
      wl_surface_attach (pointer->pointer_surface, NULL, 0, 0);
      wl_surface_commit (pointer->pointer_surface);
    }

  if (!use_surface_offset)
    {
      if (tablet)
        {
          zwp_tablet_tool_v2_set_cursor (tablet->current_tool->wp_tablet_tool,
                                         pointer->enter_serial,
                                         pointer->pointer_surface,
                                         x, y);
        }
      else if (seat->wl_pointer)
        {
          wl_pointer_set_cursor (seat->wl_pointer,
                                 pointer->enter_serial,
                                 pointer->pointer_surface,
                                 x, y);
        }
      else
        return;

      pointer->has_cursor_surface = TRUE;
      pointer->cursor_shape = 0;
    }

  pointer->cursor_hotspot_x = x;
  pointer->cursor_hotspot_y = y;
}

GdkModifierType
gdk_wayland_device_get_modifiers (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer =
    gdk_wayland_device_get_pointer (wayland_device);
  GdkModifierType mask;

  mask = seat->key_modifiers;

  if (pointer)
    mask |= pointer->button_modifiers;

  return mask;
}

void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkSurface       *surface,
                                double           *win_x,
                                double           *win_y,
                                GdkModifierType  *mask)
{
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;
  double x, y;

  if (mask)
    *mask = gdk_wayland_device_get_modifiers (device);

  pointer = gdk_wayland_device_get_pointer (wayland_device);

  if (pointer->focus == surface)
    {
      x = pointer->surface_x;
      y = pointer->surface_y;
    }
  else
    {
      x = y = -1;
    }

  if (win_x)
    *win_x = x;
  if (win_y)
    *win_y = y;
}

GdkSurface *
gdk_wayland_device_get_focus (GdkDevice *device)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandDevice *wayland_device = GDK_WAYLAND_DEVICE (device);
  GdkWaylandPointerData *pointer;

  if (device == wayland_seat->logical_keyboard)
    return wayland_seat->keyboard_focus;
  else
    {
      pointer = gdk_wayland_device_get_pointer (wayland_device);

      if (pointer)
        return pointer->focus;
    }

  return NULL;
}

static void
emulate_touch_crossing (GdkSurface           *surface,
                        GdkSurface           *child_surface,
                        GdkDevice           *device,
                        GdkDevice           *source,
                        GdkWaylandTouchData *touch,
                        GdkEventType         type,
                        GdkCrossingMode      mode,
                        uint32_t             time_)
{
  GdkEvent *event;

  event = gdk_crossing_event_new (type,
                                  surface,
                                  device,
                                  time_,
                                  0,
                                  touch->x, touch->y,
                                  mode,
                                  GDK_NOTIFY_NONLINEAR);

  _gdk_wayland_display_deliver_event (gdk_surface_get_display (surface), event);
}

void
gdk_wayland_device_unset_touch_grab (GdkDevice        *gdk_device,
                                     GdkEventSequence *sequence)
{
  GdkWaylandSeat *seat;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  touch = gdk_wayland_seat_get_touch (seat,
                                      GDK_EVENT_SEQUENCE_TO_SLOT (sequence));

  if (touch ==
      gdk_wayland_device_get_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch)))
    {
      gdk_wayland_device_set_emulating_touch (GDK_WAYLAND_DEVICE (seat->logical_touch),
                                              NULL);
      emulate_touch_crossing (touch->surface, NULL,
                              seat->logical_touch, seat->touch,
                              touch, GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL,
                              GDK_CURRENT_TIME);
    }

  event = gdk_touch_event_new (GDK_TOUCH_CANCEL,
                               GDK_SLOT_TO_EVENT_SEQUENCE (touch->id),
                               touch->surface,
                               seat->logical_touch,
                               GDK_CURRENT_TIME,
                               gdk_wayland_device_get_modifiers (seat->logical_touch),
                               touch->x, touch->y,
                               NULL,
                               touch->initial_touch);
  _gdk_wayland_display_deliver_event (seat->display, event);
}

/**
 * gdk_wayland_device_get_wl_seat: (skip)
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_seat` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_seat`
 */
struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_seat;
}

/**
 * gdk_wayland_device_get_wl_pointer: (skip)
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_pointer` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_pointer`
 */
struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_pointer;
}

/**
 * gdk_wayland_device_get_wl_keyboard: (skip)
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_keyboard` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_keyboard`
 */
struct wl_keyboard *
gdk_wayland_device_get_wl_keyboard (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_keyboard;
}

/**
 * gdk_wayland_device_get_xkb_keymap:
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the `xkb_keymap` of a `GdkDevice`.
 *
 * Returns: (transfer none): a `struct xkb_keymap`
 *
 * Since: 4.4
 */
struct xkb_keymap *
gdk_wayland_device_get_xkb_keymap (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return _gdk_wayland_keymap_get_xkb_keymap (seat->keymap);
}

GdkKeymap *
_gdk_wayland_device_get_keymap (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->keymap;
}

/**
 * gdk_wayland_device_get_data_device: (skip)
 * @gdk_device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the Wayland `wl_data_device` of a `GdkDevice`.
 *
 * Returns: (transfer none): a Wayland `wl_data_device`
 */
struct wl_data_device *
gdk_wayland_device_get_data_device (GdkDevice *gdk_device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  return seat->data_device;
}

/**
 * gdk_wayland_device_set_selection: (skip)
 * @gdk_device: (type GdkWaylandDevice): a `GdkDevice`
 * @source: the data source for the selection
 *
 * Sets the selection of the `GdkDevice.
 *
 * This is calling wl_data_device_set_selection() on
 * the `wl_data_device` of @gdk_device.
 */
void
gdk_wayland_device_set_selection (GdkDevice             *gdk_device,
                                  struct wl_data_source *source)
{
  GdkWaylandSeat *seat;
  uint32_t serial;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL);
  wl_data_device_set_selection (seat->data_device, source, serial);
}

/**
 * gdk_wayland_device_get_node_path:
 * @device: (type GdkWaylandDevice): a `GdkDevice`
 *
 * Returns the `/dev/input/event*` path of this device.
 *
 * For `GdkDevice`s that possibly coalesce multiple hardware
 * devices (eg. mouse, keyboard, touch,...), this function
 * will return %NULL.
 *
 * This is most notably implemented for devices of type
 * %GDK_SOURCE_PEN, %GDK_SOURCE_TABLET_PAD.
 *
 * Returns: (nullable) (transfer none): the `/dev/input/event*`
 *   path of this device
 */
const char *
gdk_wayland_device_get_node_path (GdkDevice *device)
{
  GdkWaylandTabletData *tablet;
  GdkWaylandTabletPadData *pad;

  GdkSeat *seat;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  seat = gdk_device_get_seat (device);
  tablet = gdk_wayland_seat_find_tablet (GDK_WAYLAND_SEAT (seat), device);
  if (tablet)
    return tablet->path;

  pad = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat), device);
  if (pad)
    return pad->path;

  return NULL;
}

static void
emulate_focus (GdkSurface *surface,
               GdkDevice *device,
               gboolean   focus_in,
               uint32_t   time_)
{
  GdkEvent *event = gdk_focus_event_new (surface, device, focus_in);

  _gdk_wayland_display_deliver_event (gdk_surface_get_display (surface), event);
}

static void
emulate_crossing (GdkSurface       *surface,
                  GdkSurface       *child_surface,
                  GdkDevice       *device,
                  GdkEventType     type,
                  GdkCrossingMode  mode,
                  uint32_t         time_)
{
  GdkEvent *event;
  GdkModifierType state;
  double x, y;

  gdk_surface_get_device_position (surface, device, &x, &y, &state);
  event = gdk_crossing_event_new (type,
                                  surface,
                                  device,
                                  time_,
                                  state,
                                  x, y,
                                  mode,
                                  GDK_NOTIFY_NONLINEAR);

  _gdk_wayland_display_deliver_event (gdk_surface_get_display (surface), event);
}

static void
device_emit_grab_crossing (GdkDevice       *device,
                           GdkSurface       *from,
                           GdkSurface       *to,
                           GdkCrossingMode  mode,
                           uint32_t         time_)
{
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      if (from)
        emulate_focus (from, device, FALSE, time_);
      if (to)
        emulate_focus (to, device, TRUE, time_);
    }
  else
    {
      if (from)
        emulate_crossing (from, to, device, GDK_LEAVE_NOTIFY, mode, time_);
      if (to)
        emulate_crossing (to, from, device, GDK_ENTER_NOTIFY, mode, time_);
    }
}

void
gdk_wayland_device_maybe_emit_grab_crossing (GdkDevice  *device,
                                             GdkSurface *window,
                                             uint32_t    time)
{
  GdkSurface *surface = gdk_wayland_device_get_focus (device);
  GdkSurface *focus = window;

  if (focus != surface)
    device_emit_grab_crossing (device, focus, window, GDK_CROSSING_GRAB, time);
}

GdkSurface *
gdk_wayland_device_maybe_emit_ungrab_crossing (GdkDevice *device,
                                               uint32_t   time_)
{
  GdkDeviceGrabInfo *grab;
  GdkSurface *focus = NULL;
  GdkSurface *surface = NULL;
  GdkSurface *prev_focus = NULL;

  focus = gdk_wayland_device_get_focus (device);
  grab = _gdk_display_get_last_device_grab (gdk_device_get_display (device), device);

  if (grab)
    {
      prev_focus = grab->surface;
      surface = grab->surface;
    }

  if (focus != surface)
    device_emit_grab_crossing (device, prev_focus, focus, GDK_CROSSING_UNGRAB, time_);

  return prev_focus;
}
