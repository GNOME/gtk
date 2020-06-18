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

#include "gdkdeviceprivate.h"
#include "gdkintl.h"

#include "gdkmacoscursor-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosdrag-private.h"
#include "gdkmacosdragsurface-private.h"

#define BIG_STEP 20
#define SMALL_STEP 1

struct _GdkMacosDrag
{
  GdkDrag parent_instance;

  GdkMacosDragSurface *drag_surface;

  int hot_x;
  int hot_y;

  int last_x;
  int last_y;
};

struct _GdkMacosDragClass
{
  GdkDragClass parent_class;
};

G_DEFINE_TYPE (GdkMacosDrag, gdk_macos_drag, GDK_TYPE_DRAG)

enum {
  PROP_0,
  PROP_DRAG_SURFACE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

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

  g_assert (GDK_IS_MACOS_DRAG (self));

  self->hot_x = hot_x;
  self->hot_y = hot_y;

  /* TODO: move/resize to take point into account */
}

static void
gdk_macos_drag_drop_done (GdkDrag  *drag,
                          gboolean  success)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  gdk_surface_hide (GDK_SURFACE (self->drag_surface));

  /* TODO: Apple HIG suggests doing a "zoomback" animation of
   * the surface back towards the original position.
   */

  g_object_unref (drag);
}

static void
gdk_macos_drag_set_cursor (GdkDrag   *drag,
                           GdkCursor *cursor)
{
  NSCursor *nscursor;

  g_assert (GDK_IS_MACOS_DRAG (drag));
  g_assert (!cursor || GDK_IS_CURSOR (cursor));

  nscursor = _gdk_macos_cursor_get_ns_cursor (cursor);

  if (nscursor != NULL)
    [nscursor set];
}

static void
gdk_macos_drag_cancel (GdkDrag             *drag,
                       GdkDragCancelReason  reason)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  g_print ("Drag cancel\n");

  gdk_drag_drop_done (drag, FALSE);

  //g_clear_pointer ((GdkSurface **)&self->drag_surface, gdk_surface_destroy);
}

static void
gdk_macos_drag_drop_performed (GdkDrag *drag,
                               guint32  time)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  //g_clear_pointer ((GdkSurface **)&self->drag_surface, gdk_surface_destroy);
}

static void
gdk_drag_get_current_actions (GdkModifierType  state,
                              gint             button,
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
                 gdouble          x_root,
                 gdouble          y_root,
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

  _gdk_macos_drag_surface_drag_motion (self->drag_surface,
                                       x_root - self->hot_x,
                                       y_root - self->hot_y,
                                       suggested_action,
                                       possible_actions,
                                       evtime);
}

static gboolean
gdk_dnd_handle_motion_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  double x, y;
  int x_root, y_root;

  g_assert (GDK_IS_MACOS_DRAG (drag));
  g_assert (event != NULL);

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
  gint dx, dy;

  dx = dy = 0;
  state = gdk_event_get_modifier_state (event);
  pointer = gdk_device_get_associated_device (gdk_event_get_device (event));

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
  _gdk_device_query_state (pointer, NULL, NULL, NULL, NULL, &state);

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
      self->last_x = GDK_SURFACE (self->drag_surface)->x;
      self->last_y = GDK_SURFACE (self->drag_surface)->y;
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
    g_param_spec_object ("drag-surface",
                         P_("Drag Surface"),
                         P_("Drag Surface"),
                         GDK_TYPE_MACOS_DRAG_SURFACE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gdk_macos_drag_init (GdkMacosDrag *self)
{
}
