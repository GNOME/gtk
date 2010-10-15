/* GDK - The GIMP Drawing Kit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkwindow.h"

#include <stdlib.h>


/* Addition used for extension_events mask */
#define GDK_ALL_DEVICES_MASK (1<<30)

struct _GdkInputWindow
{
  GList *windows; /* GdkWindow:s with extension_events set */

  /* gdk window */
  GdkWindow *impl_window; /* an impl window */
};


/**
 * gdk_devices_list:
 *
 * Returns the list of available input devices for the default display.
 * The list is statically allocated and should not be freed.
 *
 * Return value: (transfer none) (element-type GdkDevice): a list of #GdkDevice
 *
 * Deprecated: 3.0: Use gdk_device_manager_list_devices() instead.
 **/
GList *
gdk_devices_list (void)
{
  return gdk_display_list_devices (gdk_display_get_default ());
}

static void
_gdk_input_select_device_events (GdkWindow *impl_window,
                                 GdkDevice *device)
{
  guint event_mask;
  GdkWindowObject *w;
  GdkInputWindow *iw;
  GdkInputMode mode;
  gboolean has_cursor;
  GdkDeviceType type;
  GList *l;

  event_mask = 0;
  iw = ((GdkWindowObject *)impl_window)->input_window;

  g_object_get (device,
                "type", &type,
                "input-mode", &mode,
                "has-cursor", &has_cursor,
                NULL);

  if (iw == NULL ||
      mode == GDK_MODE_DISABLED ||
      type == GDK_DEVICE_TYPE_MASTER)
    return;

  for (l = iw->windows; l != NULL; l = l->next)
    {
      w = l->data;

      if (has_cursor || (w->extension_events & GDK_ALL_DEVICES_MASK))
        {
          event_mask = w->extension_events;

          if (event_mask)
            event_mask |= GDK_PROXIMITY_OUT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

          gdk_window_set_device_events ((GdkWindow *) w, device, event_mask);
        }
    }
}

static void
unset_extension_events (GdkWindow *window)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GdkDisplayX11 *display_x11;
  GdkInputWindow *iw;

  window_private = (GdkWindowObject*) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);
  iw = impl_window->input_window;

  display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  if (window_private->extension_events != 0)
    {
      g_assert (iw != NULL);
      g_assert (g_list_find (iw->windows, window) != NULL);

      iw->windows = g_list_remove (iw->windows, window);
      if (iw->windows == NULL)
	{
	  impl_window->input_window = NULL;
	  display_x11->input_windows = g_list_remove (display_x11->input_windows, iw);
	  g_free (iw);
	}
    }

  window_private->extension_events = 0;
}

/**
 * gdk_input_set_extension_events:
 * @window: a #GdkWindow.
 * @mask: the event mask
 * @mode: the type of extension events that are desired.
 *
 * Turns extension events on or off for a particular window,
 * and specifies the event mask for extension events.
 *
 * Deprecated: 3.0: Use gdk_window_set_device_events() instead.
 **/
void
gdk_input_set_extension_events (GdkWindow        *window,
                                gint              mask,
				GdkExtensionMode  mode)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GdkInputWindow *iw;
  GdkDisplayX11 *display_x11;
#ifndef XINPUT_NONE
  GList *tmp_list;
#endif

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_WINDOW_IS_X11 (window));

  window_private = (GdkWindowObject*) window;
  display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);

  if (mode == GDK_EXTENSION_EVENTS_ALL && mask != 0)
    mask |= GDK_ALL_DEVICES_MASK;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  iw = impl_window->input_window;

  if (mask != 0)
    {
      if (!iw)
	{
	  iw = g_new0 (GdkInputWindow,1);

	  iw->impl_window = (GdkWindow *)impl_window;

	  iw->windows = NULL;

	  display_x11->input_windows = g_list_append (display_x11->input_windows, iw);
	  impl_window->input_window = iw;
	}

      if (window_private->extension_events == 0)
	iw->windows = g_list_append (iw->windows, window);
      window_private->extension_events = mask;
    }
  else
    {
      unset_extension_events (window);
    }

#ifndef XINPUT_NONE
  for (tmp_list = display_x11->input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevice *dev = tmp_list->data;
      _gdk_input_select_device_events (GDK_WINDOW (impl_window), dev);
    }
#endif /* !XINPUT_NONE */
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
  unset_extension_events (window);
}

void
_gdk_input_check_extension_events (GdkDevice *device)
{
  GdkDisplayX11 *display_impl;
  GdkInputWindow *input_window;
  GList *tmp_list;

  display_impl = GDK_DISPLAY_X11 (gdk_device_get_display (device));

  for (tmp_list = display_impl->input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      input_window = tmp_list->data;
      _gdk_input_select_device_events (input_window->impl_window, device);
    }
}
