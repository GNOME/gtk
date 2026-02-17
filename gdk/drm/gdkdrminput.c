/*
 * Copyright © 2024 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"
#include "gdkseatprivate.h"

#include "gdkdrmdevice-private.h"
#include "gdkdrmdisplay-private.h"
#include "gdkdrmkeymap-private.h"
#include "gdkdrminput-private.h"
#include "gdkdrmseat-private.h"
#include "gdkdrmsurface-private.h"

static gboolean
gdk_drm_input_source_prepare (GSource *source,
                              gint    *timeout)
{
  struct libinput *libinput = GDK_DRM_DISPLAY (((GdkDrmInputSource *) source)->display)->libinput;

  if (libinput_dispatch (libinput) != 0)
    return TRUE;

  *timeout = -1;
  return FALSE;
}

static gboolean
gdk_drm_input_source_check (GSource *source)
{
  GdkDrmInputSource *input_source = (GdkDrmInputSource *) source;

  if ((input_source->poll_fd.revents & (G_IO_IN | G_IO_ERR | G_IO_HUP)) != 0)
    return TRUE;

  return FALSE;
}

static gboolean
gdk_drm_input_source_dispatch (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  GdkDrmInputSource *input_source = (GdkDrmInputSource *) source;
  GdkDrmDisplay *display = input_source->display;
  struct libinput *libinput = display->libinput;
  struct libinput_event *event;

  libinput_dispatch (libinput);

  while ((event = libinput_get_event (libinput)) != NULL)
    {
      _gdk_drm_input_handle_event (display, event);
      libinput_event_destroy (event);
    }

  return G_SOURCE_CONTINUE;
}

static void
gdk_drm_input_source_finalize (GSource *source)
{
  GdkDrmInputSource *input_source = (GdkDrmInputSource *) source;

  g_object_unref (input_source->display);
}

static GSourceFuncs input_source_funcs = {
  .prepare = gdk_drm_input_source_prepare,
  .check = gdk_drm_input_source_check,
  .dispatch = gdk_drm_input_source_dispatch,
  .finalize = gdk_drm_input_source_finalize,
};

static GdkSurface *
get_pointer_surface (GdkDrmDisplay *display,
                     double         *out_x,
                     double         *out_y)
{
  GdkDrmSurface *surface;
  int sx, sy;

  surface = _gdk_drm_display_get_surface_at_display_coords (display,
                                                             display->pointer_x,
                                                             display->pointer_y,
                                                             &sx, &sy);
  if (out_x)
    *out_x = sx;
  if (out_y)
    *out_y = sy;
  return GDK_SURFACE (surface);
}

static void
deliver_event (GdkDrmDisplay *display,
               GdkEvent      *event)
{
  GList *node;

  node = _gdk_event_queue_append (GDK_DISPLAY (display), event);
  _gdk_windowing_got_event (GDK_DISPLAY (display), node, event,
                            _gdk_display_get_next_serial (GDK_DISPLAY (display)));
}

static guint
libinput_button_to_gdk (uint32_t button)
{
  switch (button)
    {
    case BTN_LEFT:   return GDK_BUTTON_PRIMARY;
    case BTN_MIDDLE: return GDK_BUTTON_MIDDLE;
    case BTN_RIGHT:  return GDK_BUTTON_SECONDARY;
    default:         return button - BTN_LEFT + 1;
    }
}

void
_gdk_drm_input_handle_event (GdkDrmDisplay *display,
                             void           *event_ptr)
{
  struct libinput_event *event = event_ptr;
  GdkDisplay *gdk_display = GDK_DISPLAY (display);
  GdkSeat *seat = gdk_display_get_default_seat (gdk_display);
  GdkDevice *pointer, *keyboard;
  enum libinput_event_type type = libinput_event_get_type (event);

  pointer = gdk_seat_get_pointer (seat);
  keyboard = gdk_seat_get_keyboard (seat);
  if (!pointer || !keyboard)
    return;

  switch (type)
    {
    case LIBINPUT_EVENT_POINTER_MOTION:
      {
        struct libinput_event_pointer *pev = libinput_event_get_pointer_event (event);
        double dx = libinput_event_pointer_get_dx (pev);
        double dy = libinput_event_pointer_get_dy (pev);
        guint32 time = libinput_event_pointer_get_time (pev);
        GdkSurface *surface;
        double sx, sy;
        GdkEvent *gevent;

        display->pointer_x += dx;
        display->pointer_y += dy;
        display->pointer_x = CLAMP (display->pointer_x,
                                   display->layout_bounds.x,
                                   display->layout_bounds.x + display->layout_bounds.width - 1);
        display->pointer_y = CLAMP (display->pointer_y,
                                   display->layout_bounds.y,
                                   display->layout_bounds.y + display->layout_bounds.height - 1);

        surface = get_pointer_surface (display, &sx, &sy);
        if (!surface)
          break;

        gevent = gdk_motion_event_new (surface, pointer, NULL, time,
                                      display->keyboard_modifiers | display->mouse_modifiers,
                                      sx, sy, NULL);
        if (gevent)
          deliver_event (display, gevent);
      }
      break;

    case LIBINPUT_EVENT_POINTER_BUTTON:
      {
        struct libinput_event_pointer *pev = libinput_event_get_pointer_event (event);
        uint32_t button = libinput_event_pointer_get_button (pev);
        uint32_t state = libinput_event_pointer_get_button_state (pev);
        guint32 time = libinput_event_pointer_get_time (pev);
        GdkSurface *surface;
        double sx, sy;
        GdkEvent *gevent;
        GdkEventType etype = (state == LIBINPUT_BUTTON_STATE_PRESSED) ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;

        surface = get_pointer_surface (display, &sx, &sy);
        if (!surface)
          break;

        if (state == LIBINPUT_BUTTON_STATE_PRESSED)
          display->mouse_modifiers |= (GDK_BUTTON1_MASK << (libinput_button_to_gdk (button) - 1));
        else
          display->mouse_modifiers &= ~(GDK_BUTTON1_MASK << (libinput_button_to_gdk (button) - 1));

        gevent = gdk_button_event_new (etype, surface, pointer, NULL, time,
                                       display->keyboard_modifiers | display->mouse_modifiers,
                                       libinput_button_to_gdk (button), sx, sy, NULL);
        if (gevent)
          deliver_event (display, gevent);
      }
      break;

    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
      {
        struct libinput_event_pointer *pev = libinput_event_get_pointer_event (event);
        double v = libinput_event_pointer_get_scroll_value_v120 (pev, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
        guint32 time = libinput_event_pointer_get_time (pev);
        GdkSurface *surface;
        double sx, sy;
        GdkEvent *gevent;

        surface = get_pointer_surface (display, &sx, &sy);
        if (!surface)
          break;

        gevent = gdk_scroll_event_new (surface, pointer, NULL, time,
                                       display->keyboard_modifiers | display->mouse_modifiers,
                                       0, v > 0 ? -1 : 1, FALSE, GDK_SCROLL_UNIT_WHEEL);
        if (gevent)
          deliver_event (display, gevent);
      }
      break;

    case LIBINPUT_EVENT_KEYBOARD_KEY:
      {
        struct libinput_event_keyboard *kev = libinput_event_get_keyboard_event (event);
        uint32_t keycode = libinput_event_keyboard_get_key (kev);
        uint32_t key_state = libinput_event_keyboard_get_key_state (kev);
        guint32 time = libinput_event_keyboard_get_time (kev);
        GdkSurface *surface;
        double sx, sy;
        GdkEvent *gevent;
        GdkTranslatedKey translated, no_lock;
        GdkDrmKeymap *keymap = GDK_DRM_KEYMAP (display->keymap);

        _gdk_drm_keymap_update_key (keymap, keycode + 8, key_state == LIBINPUT_KEY_STATE_PRESSED);

        display->keyboard_modifiers = gdk_keymap_get_modifier_state (GDK_KEYMAP (keymap));

        surface = get_pointer_surface (display, &sx, &sy);
        if (!surface)
          break;

        if (!_gdk_drm_keymap_translate_key (keymap, keycode + 8,
                                           display->keyboard_modifiers | display->mouse_modifiers,
                                           &translated, &no_lock))
          break;

        gevent = gdk_key_event_new (key_state == LIBINPUT_KEY_STATE_PRESSED ? GDK_KEY_PRESS : GDK_KEY_RELEASE,
                                    surface, keyboard, time,
                                    keycode + 8,
                                    display->keyboard_modifiers | display->mouse_modifiers,
                                    FALSE,
                                    &translated, &no_lock, NULL);
        if (gevent)
          deliver_event (display, gevent);
      }
      break;

    default:
      break;
    }
}

GSource *
_gdk_drm_input_source_new (GdkDrmDisplay *display)
{
  GdkDrmInputSource *input_source;
  struct libinput *libinput = display->libinput;
  int fd;

  fd = libinput_get_fd (libinput);
  if (fd < 0)
    return NULL;

  input_source = (GdkDrmInputSource *) g_source_new (&input_source_funcs,
                                                     sizeof (GdkDrmInputSource));
  input_source->display = g_object_ref (display);
  input_source->poll_fd.fd = fd;
  input_source->poll_fd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;

  g_source_add_poll ((GSource *) input_source, &input_source->poll_fd);
  g_source_set_priority ((GSource *) input_source, G_PRIORITY_DEFAULT);

  return (GSource *) input_source;
}
