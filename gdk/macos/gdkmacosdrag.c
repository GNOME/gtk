/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkmacosdrag-private.h"

#include "gdkmacosdevice-private.h"
#include "gdkmacoscursor-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosdragsurface-private.h"

#include "gdk/gdkdeviceprivate.h"
#include "gdk/gdkeventsprivate.h"
#include "gdk/gdkintl.h"
#include "gdk/gdkseatprivate.h"
#include "gdk/gdk-private.h"

#define BIG_STEP 20
#define SMALL_STEP 1
#define ANIM_TIME 500000 /* .5 seconds */

typedef struct
{
  GdkMacosDrag  *drag;
  GdkFrameClock *frame_clock;
  gint64         start_time;
} GdkMacosZoomback;

G_DEFINE_TYPE (GdkMacosDrag, gdk_macos_drag, GDK_TYPE_DRAG)

enum {
  PROP_0,
  PROP_DRAG_SURFACE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static void
gdk_macos_zoomback_destroy (GdkMacosZoomback *zb)
{
  gdk_surface_hide (GDK_SURFACE (zb->drag->drag_surface));
  g_clear_object (&zb->drag);
  g_slice_free (GdkMacosZoomback, zb);
}

static gboolean
gdk_macos_zoomback_timeout (gpointer data)
{
  GdkMacosZoomback *zb = data;
  GdkFrameClock *frame_clock;
  GdkMacosDrag *drag;
  gint64 current_time;
  double f;
  double t;

  g_assert (zb != NULL);
  g_assert (GDK_IS_MACOS_DRAG (zb->drag));

  drag = zb->drag;
  frame_clock = zb->frame_clock;

  if (!frame_clock)
    return G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);
  f = (current_time - zb->start_time) / (double) ANIM_TIME;
  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  _gdk_macos_surface_move (GDK_MACOS_SURFACE (drag->drag_surface),
                           (drag->last_x - drag->hot_x) +
                           (drag->start_x - drag->last_x) * t,
                           (drag->last_y - drag->hot_y) +
                           (drag->start_y - drag->last_y) * t);
  _gdk_macos_surface_set_opacity (GDK_MACOS_SURFACE (drag->drag_surface), 1.0 - f);

  /* Make sure we're topmost */
  _gdk_macos_surface_show (GDK_MACOS_SURFACE (drag->drag_surface));

  return G_SOURCE_CONTINUE;
}

static GdkSurface *
gdk_macos_drag_get_drag_surface (GdkDrag *drag)
{
  return GDK_SURFACE (GDK_MACOS_DRAG (drag)->drag_surface);
}

static void
gdk_macos_drag_set_hotspot (GdkDrag *drag,
                            int      hot_x,
                            int      hot_y)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  int change_x;
  int change_y;

  g_assert (GDK_IS_MACOS_DRAG (self));

  change_x = hot_x - self->hot_x;
  change_y = hot_y - self->hot_y;

  self->hot_x = hot_x;
  self->hot_y = hot_y;

  if (change_x || change_y)
    _gdk_macos_surface_move (GDK_MACOS_SURFACE (self->drag_surface),
                             GDK_SURFACE (self->drag_surface)->x + change_x,
                             GDK_SURFACE (self->drag_surface)->y + change_y);
}

static void
gdk_macos_drag_drop_done (GdkDrag  *drag,
                          gboolean  success)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  GdkMacosZoomback *zb;
  guint id;

  g_assert (GDK_IS_MACOS_DRAG (self));

  if (success)
    {
      gdk_surface_hide (GDK_SURFACE (self->drag_surface));
      g_object_unref (drag);
      return;
    }

  /* Apple HIG suggests doing a "zoomback" animation of the surface back
   * towards the original position.
   */
  zb = g_slice_new0 (GdkMacosZoomback);
  zb->drag = g_object_ref (self);
  zb->frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self->drag_surface));
  zb->start_time = gdk_frame_clock_get_frame_time (zb->frame_clock);

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 17,
                           gdk_macos_zoomback_timeout,
                           zb,
                           (GDestroyNotify) gdk_macos_zoomback_destroy);
  gdk_source_set_static_name_by_id (id, "[gtk] gdk_macos_zoomback_timeout");
  g_object_unref (drag);
}

static void
gdk_macos_drag_set_cursor (GdkDrag   *drag,
                           GdkCursor *cursor)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  NSCursor *nscursor;

  g_assert (GDK_IS_MACOS_DRAG (self));
  g_assert (!cursor || GDK_IS_CURSOR (cursor));

  g_set_object (&self->cursor, cursor);

  nscursor = _gdk_macos_cursor_get_ns_cursor (cursor);

  if (nscursor != NULL)
    [nscursor set];
}

static gboolean
drag_grab (GdkMacosDrag *self)
{
  GdkSeat *seat;

  g_assert (GDK_IS_MACOS_DRAG (self));

  seat = gdk_device_get_seat (gdk_drag_get_device (GDK_DRAG (self)));

  if (gdk_seat_grab (seat,
                     GDK_SURFACE (self->drag_surface),
                     GDK_SEAT_CAPABILITY_ALL_POINTING,
                     FALSE,
                     self->cursor,
                     NULL,
                     NULL,
                     NULL) != GDK_GRAB_SUCCESS)
    return FALSE;

  g_set_object (&self->drag_seat, seat);

  return TRUE;
}

static void
drag_ungrab (GdkMacosDrag *self)
{
  GdkDisplay *display;

  g_assert (GDK_IS_MACOS_DRAG (self));

  display = gdk_drag_get_display (GDK_DRAG (self));
  _gdk_macos_display_break_all_grabs (GDK_MACOS_DISPLAY (display), GDK_CURRENT_TIME);
}

static void
gdk_macos_drag_cancel (GdkDrag             *drag,
                       GdkDragCancelReason  reason)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  if (self->cancelled)
    return;

  self->cancelled = TRUE;
  drag_ungrab (self);
  gdk_drag_drop_done (drag, FALSE);
}

static void
gdk_macos_drag_drop_performed (GdkDrag *drag,
                               guint32  time)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  g_object_ref (self);
  drag_ungrab (self);
  g_signal_emit_by_name (drag, "dnd-finished");
  gdk_drag_drop_done (drag, TRUE);
  g_object_unref (self);
}

static void
gdk_drag_get_current_actions (GdkModifierType  state,
                              int              button,
                              GdkDragAction    actions,
                              GdkDragAction   *suggested_action,
                              GdkDragAction   *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
    {
      *suggested_action = GDK_ACTION_ASK;
      *possible_actions = actions;
    }
  else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    {
      if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
        {
          if (actions & GDK_ACTION_LINK)
            {
              *suggested_action = GDK_ACTION_LINK;
              *possible_actions = GDK_ACTION_LINK;
            }
        }
      else if (state & GDK_CONTROL_MASK)
        {
          if (actions & GDK_ACTION_COPY)
            {
              *suggested_action = GDK_ACTION_COPY;
              *possible_actions = GDK_ACTION_COPY;
            }
        }
      else
        {
          if (actions & GDK_ACTION_MOVE)
            {
              *suggested_action = GDK_ACTION_MOVE;
              *possible_actions = GDK_ACTION_MOVE;
            }
        }
    }
  else
    {
      *possible_actions = actions;

      if ((state & (GDK_ALT_MASK)) && (actions & GDK_ACTION_ASK))
        *suggested_action = GDK_ACTION_ASK;
      else if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

static void
gdk_drag_update (GdkDrag         *drag,
                 double           x_root,
                 double           y_root,
                 GdkModifierType  mods,
                 guint32          evtime)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  GdkDragAction suggested_action;
  GdkDragAction possible_actions;

  g_assert (GDK_IS_MACOS_DRAG (self));

  self->last_x = x_root;
  self->last_y = y_root;

  gdk_drag_get_current_actions (mods,
                                GDK_BUTTON_PRIMARY,
                                gdk_drag_get_actions (drag),
                                &suggested_action,
                                &possible_actions);

  if (GDK_IS_MACOS_SURFACE (self->drag_surface))
    _gdk_macos_surface_move (GDK_MACOS_SURFACE (self->drag_surface),
                             x_root - self->hot_x,
                             y_root - self->hot_y);

  if (!self->did_update)
    {
      self->start_x = self->last_x;
      self->start_y = self->last_y;
      self->did_update = TRUE;
    }

  gdk_drag_set_actions (drag, possible_actions);
}

static gboolean
gdk_dnd_handle_motion_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  double x, y;
  int x_root, y_root;

  g_assert (GDK_IS_MACOS_DRAG (drag));
  g_assert (event != NULL);

  /* Ignore motion while doing zoomback */
  if (GDK_MACOS_DRAG (drag)->cancelled)
    return FALSE;

  gdk_event_get_position (event, &x, &y);
  x_root = event->surface->x + x;
  y_root = event->surface->y + y;
  gdk_drag_update (drag, x_root, y_root,
                   gdk_event_get_modifier_state (event),
                   gdk_event_get_time (event));

  return TRUE;
}

static gboolean
gdk_dnd_handle_grab_broken_event (GdkDrag  *drag,
                                  GdkEvent *event)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (drag);
  gboolean is_implicit = gdk_grab_broken_event_get_implicit (event);
  GdkSurface *grab_surface = gdk_grab_broken_event_get_grab_surface (event);

  /* Don't cancel if we break the implicit grab from the initial button_press. */
  if (is_implicit || grab_surface == (GdkSurface *)self->drag_surface)
    return FALSE;

  if (gdk_event_get_device (event) != gdk_drag_get_device (drag))
    return FALSE;

  gdk_drag_cancel (drag, GDK_DRAG_CANCEL_ERROR);

  return TRUE;
}

static gboolean
gdk_dnd_handle_button_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (drag);

  g_assert (GDK_IS_MACOS_DRAG (self));
  g_assert (event != NULL);

#if 0
  /* FIXME: Check the button matches */
  if (event->button != self->button)
    return FALSE;
#endif

  if (gdk_drag_get_selected_action (drag) != 0)
    g_signal_emit_by_name (drag, "drop-performed");
  else
    gdk_drag_cancel (drag, GDK_DRAG_CANCEL_NO_TARGET);

  return TRUE;
}

static gboolean
gdk_dnd_handle_key_event (GdkDrag  *drag,
                          GdkEvent *event)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (drag);
  GdkModifierType state;
  GdkDevice *pointer;
  GdkSeat *seat;
  int dx, dy;

  dx = dy = 0;
  state = gdk_event_get_modifier_state (event);
  seat = gdk_event_get_seat (event);
  pointer = gdk_seat_get_pointer (seat);

  if (event->event_type == GDK_KEY_PRESS)
    {
      guint keyval = gdk_key_event_get_keyval (event);

      switch (keyval)
        {
        case GDK_KEY_Escape:
          gdk_drag_cancel (drag, GDK_DRAG_CANCEL_USER_CANCELLED);
          return TRUE;

        case GDK_KEY_space:
        case GDK_KEY_Return:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_KP_Space:
          if (gdk_drag_get_selected_action (drag) != 0)
            g_signal_emit_by_name (drag, "drop-performed");
          else
            gdk_drag_cancel (drag, GDK_DRAG_CANCEL_NO_TARGET);

          return TRUE;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          dy = (state & GDK_ALT_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          dy = (state & GDK_ALT_MASK) ? BIG_STEP : SMALL_STEP;
          break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
          dx = (state & GDK_ALT_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
          dx = (state & GDK_ALT_MASK) ? BIG_STEP : SMALL_STEP;
          break;

        default:
          break;
        }
    }

  /* The state is not yet updated in the event, so we need
   * to query it here. We could use XGetModifierMapping, but
   * that would be overkill.
   */
  gdk_macos_device_query_state (pointer, NULL, NULL, NULL, NULL, &state);

  if (dx != 0 || dy != 0)
    {
      GdkDisplay *display = gdk_event_get_display ((GdkEvent *)event);

      self->last_x += dx;
      self->last_y += dy;

      _gdk_macos_display_warp_pointer (GDK_MACOS_DISPLAY (display),
                                       self->last_x,
                                       self->last_y);
    }

  gdk_drag_update (drag,
                   self->last_x, self->last_y,
                   state,
                   gdk_event_get_time (event));

  return TRUE;
}

static gboolean
gdk_macos_drag_handle_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  g_assert (GDK_IS_MACOS_DRAG (drag));
  g_assert (event != NULL);

  switch ((guint) event->event_type)
    {
    case GDK_MOTION_NOTIFY:
      return gdk_dnd_handle_motion_event (drag, event);

    case GDK_BUTTON_RELEASE:
      return gdk_dnd_handle_button_event (drag, event);

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return gdk_dnd_handle_key_event (drag, event);

    case GDK_GRAB_BROKEN:
      return gdk_dnd_handle_grab_broken_event (drag, event);

    default:
      return FALSE;
    }
}

static void
gdk_macos_drag_finalize (GObject *object)
{
  GdkMacosDrag *self = (GdkMacosDrag *)object;
  GdkMacosDragSurface *drag_surface = g_steal_pointer (&self->drag_surface);

  g_clear_object (&self->cursor);
  g_clear_object (&self->drag_seat);

  G_OBJECT_CLASS (gdk_macos_drag_parent_class)->finalize (object);

  if (drag_surface)
    gdk_surface_destroy (GDK_SURFACE (drag_surface));
}

static void
gdk_macos_drag_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (object);

  switch (prop_id)
    {
    case PROP_DRAG_SURFACE:
      g_value_set_object (value, self->drag_surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_macos_drag_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (object);

  switch (prop_id)
    {
    case PROP_DRAG_SURFACE:
      self->drag_surface = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_macos_drag_class_init (GdkMacosDragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragClass *drag_class = GDK_DRAG_CLASS (klass);

  object_class->finalize = gdk_macos_drag_finalize;
  object_class->get_property = gdk_macos_drag_get_property;
  object_class->set_property = gdk_macos_drag_set_property;

  drag_class->get_drag_surface = gdk_macos_drag_get_drag_surface;
  drag_class->set_hotspot = gdk_macos_drag_set_hotspot;
  drag_class->drop_done = gdk_macos_drag_drop_done;
  drag_class->set_cursor = gdk_macos_drag_set_cursor;
  drag_class->cancel = gdk_macos_drag_cancel;
  drag_class->drop_performed = gdk_macos_drag_drop_performed;
  drag_class->handle_event = gdk_macos_drag_handle_event;

  properties [PROP_DRAG_SURFACE] =
    g_param_spec_object ("drag-surface", NULL, NULL,
                         GDK_TYPE_MACOS_DRAG_SURFACE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gdk_macos_drag_init (GdkMacosDrag *self)
{
}

gboolean
_gdk_macos_drag_begin (GdkMacosDrag *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_DRAG (self), FALSE);

  _gdk_macos_surface_show (GDK_MACOS_SURFACE (self->drag_surface));

  return drag_grab (self);
}
