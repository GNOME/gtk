/* gdkdrmseat.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkdeviceprivate.h"
#include "gdkdevicetoolprivate.h"

#include "gdkdrmdevice.h"
#include "gdkdrmseat-private.h"

#include "gdkprivate.h"

struct _GdkDrmSeat
{
  GdkSeat parent_instance;

  GdkDrmDisplay *display;

  GdkDevice *logical_pointer;
  GdkDevice *logical_keyboard;
};

struct _GdkDrmSeatClass
{
  GdkSeatClass parent_class;
};

G_DEFINE_FINAL_TYPE (GdkDrmSeat, gdk_drm_seat, GDK_TYPE_SEAT)

#define KEYBOARD_EVENTS (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |    \
                         GDK_FOCUS_CHANGE_MASK)
#define TOUCH_EVENTS    (GDK_TOUCH_MASK)
#define POINTER_EVENTS  (GDK_POINTER_MOTION_MASK |                      \
                         GDK_BUTTON_PRESS_MASK |                        \
                         GDK_BUTTON_RELEASE_MASK |                      \
                         GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK |     \
                         GDK_ENTER_NOTIFY_MASK |                        \
                         GDK_LEAVE_NOTIFY_MASK |                        \
                         GDK_PROXIMITY_IN_MASK |                        \
                         GDK_PROXIMITY_OUT_MASK)

static void
gdk_drm_seat_dispose (GObject *object)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (object);

  if (self->logical_pointer)
    {
      gdk_seat_device_removed (GDK_SEAT (self), self->logical_pointer);
      g_clear_object (&self->logical_pointer);
    }

  if (self->logical_keyboard)
    {
      gdk_seat_device_removed (GDK_SEAT (self), self->logical_keyboard);
      g_clear_object (&self->logical_keyboard);
    }

  G_OBJECT_CLASS (gdk_drm_seat_parent_class)->dispose (object);
}

static GdkSeatCapabilities
gdk_drm_seat_get_capabilities (GdkSeat *seat)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (seat);
  GdkSeatCapabilities caps = 0;

  if (self->logical_pointer)
    caps |= GDK_SEAT_CAPABILITY_POINTER;
  if (self->logical_keyboard)
    caps |= GDK_SEAT_CAPABILITY_KEYBOARD;

  return caps;
}

static GdkGrabStatus
gdk_drm_seat_grab (GdkSeat                *seat,
                   GdkSurface             *surface,
                   GdkSeatCapabilities     capabilities,
                   gboolean                owner_events,
                   GdkCursor              *cursor,
                   GdkEvent               *event,
                   GdkSeatGrabPrepareFunc  prepare_func,
                   gpointer                prepare_func_data)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (seat);
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
  GdkGrabStatus status = GDK_GRAB_SUCCESS;
  gboolean was_visible;

  was_visible = gdk_surface_get_mapped (surface);

  if (prepare_func)
    (prepare_func) (seat, surface, prepare_func_data);

  if (!gdk_surface_get_mapped (surface))
    {
      g_critical ("Surface %p has not been mapped in GdkSeatGrabPrepareFunc",
                  surface);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (capabilities & GDK_SEAT_CAPABILITY_ALL_POINTING)
    {
      /* ALL_POINTING spans 3 capabilities; get the mask for the ones we have */
      GdkEventMask pointer_evmask = 0;

      /* We let tablet styli take over the pointer cursor */
      if (capabilities & (GDK_SEAT_CAPABILITY_POINTER |
                          GDK_SEAT_CAPABILITY_TABLET_STYLUS))
        pointer_evmask |= POINTER_EVENTS;

      if (capabilities & GDK_SEAT_CAPABILITY_TOUCH)
        pointer_evmask |= TOUCH_EVENTS;

      status = gdk_device_grab (self->logical_pointer, surface,
                                owner_events,
                                pointer_evmask, cursor,
                                evtime);
    }

  if (status == GDK_GRAB_SUCCESS &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      status = gdk_device_grab (self->logical_keyboard, surface,
                                owner_events,
                                KEYBOARD_EVENTS, cursor,
                                evtime);

      if (status != GDK_GRAB_SUCCESS)
        {
          if (capabilities & ~GDK_SEAT_CAPABILITY_KEYBOARD)
            gdk_device_ungrab (self->logical_pointer, evtime);
        }
    }

  if (status != GDK_GRAB_SUCCESS && !was_visible)
    gdk_surface_hide (surface);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return status;
}

static void
gdk_drm_seat_ungrab (GdkSeat *seat)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (seat);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gdk_device_ungrab (self->logical_pointer, GDK_CURRENT_TIME);
  gdk_device_ungrab (self->logical_keyboard, GDK_CURRENT_TIME);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static GdkDevice *
gdk_drm_seat_get_logical_device (GdkSeat             *seat,
                                 GdkSeatCapabilities  capability)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (seat);

  /* There must be only one flag set */
  switch ((guint) capability)
    {
    case GDK_SEAT_CAPABILITY_POINTER:
    case GDK_SEAT_CAPABILITY_TOUCH:
      return self->logical_pointer;
    case GDK_SEAT_CAPABILITY_KEYBOARD:
      return self->logical_keyboard;
    default:
      g_warning ("Unhandled capability %x", capability);
      break;
    }

  return NULL;
}

static GList *
gdk_drm_seat_get_devices (GdkSeat             *seat,
                          GdkSeatCapabilities  capabilities)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (seat);
  GList *physical_devices = NULL;

  if (self->logical_pointer && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    physical_devices = g_list_prepend (physical_devices, self->logical_pointer);

  if (self->logical_keyboard && (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD))
    physical_devices = g_list_prepend (physical_devices, self->logical_keyboard);

  if (capabilities & GDK_SEAT_CAPABILITY_TABLET_STYLUS)
    {
      /* TODO: */
    }

  return physical_devices;
}

static GList *
gdk_drm_seat_get_tools (GdkSeat *seat)
{
  GdkDrmSeat *self = GDK_DRM_SEAT (seat);
  GdkDeviceTool *tool;
  GList *tools = NULL;

  return tools;
}

static void
gdk_drm_seat_class_init (GdkDrmSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->dispose = gdk_drm_seat_dispose;

  seat_class->get_capabilities = gdk_drm_seat_get_capabilities;
  seat_class->grab = gdk_drm_seat_grab;
  seat_class->ungrab = gdk_drm_seat_ungrab;
  seat_class->get_logical_device = gdk_drm_seat_get_logical_device;
  seat_class->get_devices = gdk_drm_seat_get_devices;
  seat_class->get_tools = gdk_drm_seat_get_tools;
}

static void
gdk_drm_seat_init (GdkDrmSeat *self)
{
}

static void
init_devices (GdkDrmSeat *self)
{
  /* pointer */
  self->logical_pointer = g_object_new (GDK_TYPE_DRM_DEVICE,
                                        "name", "Core Pointer",
                                        "source", GDK_SOURCE_MOUSE,
                                        "has-cursor", TRUE,
                                        "display", self->display,
                                        "seat", self,
                                        NULL);

  /* keyboard */
  self->logical_keyboard = g_object_new (GDK_TYPE_DRM_DEVICE,
                                         "name", "Core Keyboard",
                                         "source", GDK_SOURCE_KEYBOARD,
                                         "has-cursor", FALSE,
                                         "display", self->display,
                                         "seat", self,
                                         NULL);

  /* link both */
  _gdk_device_set_associated_device (self->logical_pointer, self->logical_keyboard);
  _gdk_device_set_associated_device (self->logical_keyboard, self->logical_pointer);

  gdk_seat_device_added (GDK_SEAT (self), self->logical_pointer);
  gdk_seat_device_added (GDK_SEAT (self), self->logical_keyboard);
}

GdkSeat *
_gdk_drm_seat_new (GdkDrmDisplay *display)
{
  GdkDrmSeat *self;

  g_return_val_if_fail (GDK_IS_DRM_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE_DRM_SEAT,
                       "display", display,
                       NULL);

  self->display = display;

  init_devices (self);

  return GDK_SEAT (g_steal_pointer (&self));
}
