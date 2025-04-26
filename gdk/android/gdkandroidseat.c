/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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

#include <math.h>

#include "gdkdevicetoolprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroiddevice-private.h"
#include "gdkandroidtoplevel-private.h"

#include "gdkandroidseat-private.h"

/**
 * GdkAndroidSeat:
 *
 * The Android implementation of [class@Gdk.Seat].
 *
 * Since: 4.18
 */
G_DEFINE_TYPE (GdkAndroidSeat, gdk_android_seat, GDK_TYPE_SEAT)

static void
gdk_android_seat_constructed (GObject *object)
{
  GdkAndroidSeat *self = GDK_ANDROID_SEAT (object);
  GdkSeat *seat = (GdkSeat *) self;
  G_OBJECT_CLASS (gdk_android_seat_parent_class)->constructed (object);
  GdkDisplay *display = gdk_seat_get_display (seat);

  self->logical_pointer = g_object_new (GDK_TYPE_ANDROID_DEVICE,
                                        "name", "Android Pointer",
                                        "source", GDK_SOURCE_MOUSE,
                                        "has-cursor", TRUE,
                                        "display", display,
                                        "seat", seat,
                                        NULL);
  self->logical_touchscreen = g_object_new (GDK_TYPE_ANDROID_DEVICE,
                                            "name", "Android Touchscreen",
                                            "source", GDK_SOURCE_TOUCHSCREEN,
                                            "has-cursor", FALSE,
                                            "display", display,
                                            "seat", seat,
                                            NULL);
  self->logical_keyboard = g_object_new (GDK_TYPE_ANDROID_DEVICE,
                                         "name", "Android Keyboard",
                                         "source", GDK_SOURCE_KEYBOARD,
                                         "has-cursor", FALSE,
                                         "display", display,
                                         "seat", seat,
                                         NULL);
}

static void
gdk_android_seat_finalize (GObject *object)
{
  GdkAndroidSeat *self = GDK_ANDROID_SEAT (object);
  JNIEnv *env = gdk_android_get_env();
  if (self->active_grab_view)
    (*env)->DeleteGlobalRef(env, self->active_grab_view);
  g_object_unref (self->logical_pointer);
  g_object_unref (self->logical_touchscreen);
  g_object_unref (self->logical_keyboard);
  g_object_unref (self->stylus);
  g_object_unref (self->eraser);
  g_object_unref (self->mouse);
  G_OBJECT_CLASS (gdk_android_seat_parent_class)->finalize (object);
}

static GdkSeatCapabilities
gdk_android_seat_get_capabilities (GdkSeat *seat)
{
  return GDK_SEAT_CAPABILITY_ALL;
}

#define KEYBOARD_EVENTS (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | \
                         GDK_FOCUS_CHANGE_MASK)
#define POINTER_EVENTS  (GDK_POINTER_MOTION_MASK |                  \
                         GDK_BUTTON_PRESS_MASK |                    \
                         GDK_BUTTON_RELEASE_MASK |                  \
                         GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | \
                         GDK_ENTER_NOTIFY_MASK |                    \
                         GDK_LEAVE_NOTIFY_MASK |                    \
                         GDK_PROXIMITY_IN_MASK |                    \
                         GDK_PROXIMITY_OUT_MASK |                   \
                         GDK_TOUCHPAD_GESTURE_MASK)
#define TOUCH_EVENTS    (GDK_TOUCH_MASK)

static GdkGrabStatus
gdk_android_seat_grab (GdkSeat               *seat,
                       GdkSurface            *surface,
                       GdkSeatCapabilities    capabilities,
                       gboolean               owner_events,
                       GdkCursor             *cursor,
                       GdkEvent              *event,
                       GdkSeatGrabPrepareFunc prepare_func,
                       gpointer               prepare_func_data)
{
  GdkAndroidSeat *self = (GdkAndroidSeat *) seat;
  GdkAndroidSurface *surface_impl = (GdkAndroidSurface *)surface;
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;

  GdkGrabStatus status = GDK_GRAB_SUCCESS;
  gboolean grabbed_pointer = FALSE;
  gboolean grabbed_touchscreen = FALSE;
  gboolean grabbed_keyboard = FALSE;

  gboolean was_visible = gdk_surface_get_mapped (surface);

  if (prepare_func)
    (prepare_func) (seat, surface, prepare_func_data);

  JNIEnv *env = gdk_android_get_env();

  if (!gdk_surface_get_mapped (surface))
    {
      g_critical ("Surface %p has not been mapped in GdkSeatGrabPrepareFunc",
                  surface);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  if (capabilities & (GDK_SEAT_CAPABILITY_POINTER | GDK_SEAT_CAPABILITY_TABLET_STYLUS))
    {
      status = gdk_device_grab (self->logical_pointer, surface,
                                owner_events,
                                POINTER_EVENTS, cursor,
                                evtime);
      if (status != GDK_GRAB_SUCCESS)
        goto failure;

      (*env)->PushLocalFrame (env, 1);
      GdkAndroidToplevel *toplevel = gdk_android_surface_get_toplevel (surface_impl);
      jobject view = (*env)->GetObjectField (env, toplevel->activity, gdk_android_get_java_cache ()->toplevel.toplevel_view);
      if (self->active_grab_view)
        (*env)->DeleteGlobalRef (env, self->active_grab_view);
      self->active_grab_view = (*env)->NewGlobalRef (env, view);
      (*env)->CallVoidMethod (env, view, gdk_android_get_java_cache ()->toplevel_view.set_grabbed_surface, surface_impl->surface);
      (*env)->PopLocalFrame (env, NULL);

      grabbed_pointer = TRUE;
    }

  if (status == GDK_GRAB_SUCCESS &&
      capabilities & GDK_SEAT_CAPABILITY_TOUCH)
    {
      status = gdk_device_grab (self->logical_touchscreen, surface,
                                owner_events,
                                TOUCH_EVENTS, cursor,
                                evtime);
      if (status != GDK_GRAB_SUCCESS)
        goto failure;
      grabbed_touchscreen = TRUE;
    }

  if (status == GDK_GRAB_SUCCESS &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      status = gdk_device_grab (self->logical_keyboard, surface,
                                owner_events,
                                KEYBOARD_EVENTS, cursor,
                                evtime);
      if (status != GDK_GRAB_SUCCESS)
        goto failure;
      grabbed_keyboard = TRUE;
    }

  return status;
failure:
  if (grabbed_pointer)
    gdk_device_ungrab (self->logical_pointer, evtime);
  if (grabbed_touchscreen)
    gdk_device_ungrab (self->logical_touchscreen, evtime);
  if (grabbed_keyboard)
    gdk_device_ungrab (self->logical_keyboard, evtime);

  if (!was_visible)
    gdk_surface_hide (surface);
  return status;
}

static void
gdk_android_seat_ungrab (GdkSeat *seat)
{
  GdkAndroidSeat *self = (GdkAndroidSeat *) seat;

  if (self->active_grab_view)
    {
      JNIEnv *env = gdk_android_get_env ();
      (*env)->CallVoidMethod (env, self->active_grab_view, gdk_android_get_java_cache ()->toplevel_view.set_grabbed_surface, NULL);
      (*env)->DeleteGlobalRef (env, self->active_grab_view);
      self->active_grab_view = NULL;
    }

  gdk_device_ungrab (self->logical_pointer, GDK_CURRENT_TIME);
  gdk_device_ungrab (self->logical_touchscreen, GDK_CURRENT_TIME);
  gdk_device_ungrab (self->logical_keyboard, GDK_CURRENT_TIME);
}

static GdkDevice *
gdk_default_android_get_logical_device (GdkSeat            *seat,
                                        GdkSeatCapabilities capability)
{
  GdkAndroidSeat *self = (GdkAndroidSeat *) seat;

  if (capability & (GDK_SEAT_CAPABILITY_POINTER | GDK_SEAT_CAPABILITY_TABLET_STYLUS))
    return self->logical_pointer;
  if (capability & GDK_SEAT_CAPABILITY_TOUCH)
    return self->logical_touchscreen;
  else if (capability & GDK_SEAT_CAPABILITY_KEYBOARD)
    return self->logical_keyboard;

  g_warning ("AndroidSeat: Unhandled capability %x", capability);
  return NULL;
}

static GList *
gdk_android_seat_get_devices (GdkSeat            *seat,
                              GdkSeatCapabilities capabilities)
{
  GdkAndroidSeat *self = (GdkAndroidSeat *) seat;

  GList *devices = NULL;
  if (capabilities & (GDK_SEAT_CAPABILITY_POINTER | GDK_SEAT_CAPABILITY_TABLET_STYLUS))
    devices = g_list_prepend (devices, self->logical_pointer);
  if (capabilities & GDK_SEAT_CAPABILITY_TOUCH)
    devices = g_list_prepend (devices, self->logical_touchscreen);
  if (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    devices = g_list_prepend (devices, self->logical_keyboard);

  return g_list_reverse (devices);
}

static GList *
gdk_android_seat_get_tools (GdkSeat *seat)
{
  GdkAndroidSeat *self = (GdkAndroidSeat *) seat;

  GList *tools = NULL;
  tools = g_list_prepend (tools, self->mouse);
  tools = g_list_prepend (tools, self->stylus);
  tools = g_list_prepend (tools, self->eraser);

  return g_list_reverse (tools);
}

static void
gdk_android_seat_class_init (GdkAndroidSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = (GdkSeatClass *) klass;

  object_class->constructed = gdk_android_seat_constructed;
  object_class->finalize = gdk_android_seat_finalize;
  seat_class->get_capabilities = gdk_android_seat_get_capabilities;
  seat_class->grab = gdk_android_seat_grab;
  seat_class->ungrab = gdk_android_seat_ungrab;
  seat_class->get_logical_device = gdk_default_android_get_logical_device;
  seat_class->get_devices = gdk_android_seat_get_devices;
  seat_class->get_tools = gdk_android_seat_get_tools;
}

static const GdkAxisFlags gdk_android_seat_motion_flags = GDK_AXIS_FLAG_X | GDK_AXIS_FLAG_Y;
static const GdkAxisFlags gdk_android_seat_stylus_flags = gdk_android_seat_motion_flags | GDK_AXIS_FLAG_PRESSURE | GDK_AXIS_FLAG_DISTANCE | GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT;

static void
gdk_android_seat_init (GdkAndroidSeat *self)
{
  self->stylus = gdk_device_tool_new (AMOTION_EVENT_TOOL_TYPE_STYLUS, 0,
                                      GDK_DEVICE_TOOL_TYPE_PEN,
                                      gdk_android_seat_stylus_flags);
  self->eraser = gdk_device_tool_new (AMOTION_EVENT_TOOL_TYPE_ERASER, 0,
                                      GDK_DEVICE_TOOL_TYPE_ERASER,
                                      gdk_android_seat_stylus_flags);
  self->mouse = gdk_device_tool_new (AMOTION_EVENT_TOOL_TYPE_MOUSE, 0,
                                     GDK_DEVICE_TOOL_TYPE_MOUSE,
                                     gdk_android_seat_motion_flags);
}

GdkAndroidSeat *
gdk_android_seat_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_ANDROID_SEAT, "display", display, NULL);
}

GdkDeviceTool *
gdk_android_seat_get_device_tool (GdkAndroidSeat *self, gint32 tool_type)
{
  switch (tool_type)
    {
    case AMOTION_EVENT_TOOL_TYPE_STYLUS:
      return self->stylus;
    case AMOTION_EVENT_TOOL_TYPE_ERASER:
      return self->eraser;
    case AMOTION_EVENT_TOOL_TYPE_MOUSE:
      return self->mouse;
    default:
      return NULL;
    }
}

gboolean
gdk_android_seat_normalize_range (JNIEnv *env, jobject device,
                                  const AInputEvent *event, size_t pointer_index,
                                  guint32 mask,
                                  gfloat from, gfloat to,
                                  gdouble *out)
{
  g_return_val_if_fail (out != NULL, FALSE);

  jobject axis = (*env)->CallObjectMethod (env,
                                           device,
                                           gdk_android_get_java_cache ()->a_input_device.get_motion_range,
                                           mask);
  if (!axis)
    return FALSE;

  gfloat min = (*env)->CallFloatMethod (env,
                                        axis,
                                        gdk_android_get_java_cache ()->a_motion_range.get_min);
  gfloat max = (*env)->CallFloatMethod (env,
                                        axis,
                                        gdk_android_get_java_cache ()->a_motion_range.get_max);
  gfloat value = AMotionEvent_getAxisValue (event, mask, pointer_index);

  // $v_\text{new}=from+\frac{(v-min)*(to-from)}{max-min}$
  *out = from + ((value - min) * (to - from)) / (max - min);
  return TRUE;
}

gdouble *
gdk_android_seat_create_axes_from_motion_event (const AInputEvent *event, size_t pointer_index)
{
  gdouble axes[GDK_AXIS_LAST] = { 0 };
  axes[0] = AMotionEvent_getX (event, pointer_index);
  axes[1] = AMotionEvent_getY (event, pointer_index);

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);
  jobject device = (*env)->CallStaticObjectMethod (env,
                                                   gdk_android_get_java_cache ()->a_input_device.klass,
                                                   gdk_android_get_java_cache ()->a_input_device.get_device_from_id,
                                                   AInputEvent_getDeviceId (event));
  if (device)
    {
      // The values from the from/to parameters are from the switch statement in
      // _gdk_device_add_axis. As the _gdk_device_* family of functions is not
      // used, ensure they are kept in sync manually.

      if (!gdk_android_seat_normalize_range (env, device, event, pointer_index, AMOTION_EVENT_AXIS_PRESSURE, 0.f, 1.f, &axes[GDK_AXIS_PRESSURE]))
        axes[GDK_AXIS_PRESSURE] = 0.;
      if (!gdk_android_seat_normalize_range (env, device, event, pointer_index, AMOTION_EVENT_AXIS_DISTANCE, 0.f, 1.f, &axes[GDK_AXIS_DISTANCE]))
        axes[GDK_AXIS_DISTANCE] = 0.;

      gdouble orientation, tilt;
      if (gdk_android_seat_normalize_range (env, device, event, pointer_index, AMOTION_EVENT_AXIS_ORIENTATION, -G_PI, G_PI, &orientation) && gdk_android_seat_normalize_range (env, device, event, pointer_index, AMOTION_EVENT_AXIS_TILT, 0.f, G_PI / 2.f, &tilt))
        {
          // Taken and modified from Termux-x11. Unlike the other axes, x/y-tilt are
          // in [-1;1], which are the bounds of $\frac{\arcsin(x)}{0.5*\pi}$.
          axes[GDK_AXIS_XTILT] = asinf (-sinf (orientation) * sinf (tilt)) / G_PI_2;
          axes[GDK_AXIS_YTILT] = asinf (cosf (orientation) * sinf (tilt)) / G_PI_2;
        }
      else
        {
          axes[GDK_AXIS_XTILT] = 0.;
          axes[GDK_AXIS_YTILT] = 0.;
        }
    }

  (*env)->PopLocalFrame (env, NULL);

  return g_memdup2 (axes, sizeof axes);
}

void
gdk_android_seat_consume_event (GdkDisplay *display, GdkEvent *event)
{
  GList *node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event,
                            _gdk_display_get_next_serial (display));
}
