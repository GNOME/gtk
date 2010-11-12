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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkdevicemanager-broadway.h"

#include "gdktypes.h"
#include "gdkdevicemanager.h"
#include "gdkdevice-broadway.h"
#include "gdkkeysyms.h"
#include "gdkprivate-broadway.h"
#include "gdkx.h"

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif


#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void    gdk_device_manager_core_finalize    (GObject *object);
static void    gdk_device_manager_core_constructed (GObject *object);

static GList * gdk_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                                     GdkDeviceType     type);
static GdkDevice * gdk_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager);

G_DEFINE_TYPE (GdkDeviceManagerCore, gdk_device_manager_core, GDK_TYPE_DEVICE_MANAGER)

static void
gdk_device_manager_core_class_init (GdkDeviceManagerCoreClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_manager_core_finalize;
  object_class->constructed = gdk_device_manager_core_constructed;
  device_manager_class->list_devices = gdk_device_manager_core_list_devices;
  device_manager_class->get_client_pointer = gdk_device_manager_core_get_client_pointer;
}

static GdkDevice *
create_core_pointer (GdkDeviceManager *device_manager,
                     GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_DEVICE_CORE,
                       "name", "Core Pointer",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static GdkDevice *
create_core_keyboard (GdkDeviceManager *device_manager,
                      GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_DEVICE_CORE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static void
gdk_device_manager_core_init (GdkDeviceManagerCore *device_manager)
{
}

static void
gdk_device_manager_core_finalize (GObject *object)
{
  GdkDeviceManagerCore *device_manager_core;

  device_manager_core = GDK_DEVICE_MANAGER_CORE (object);

  g_object_unref (device_manager_core->core_pointer);
  g_object_unref (device_manager_core->core_keyboard);

  G_OBJECT_CLASS (gdk_device_manager_core_parent_class)->finalize (object);
}

static void
gdk_device_manager_core_constructed (GObject *object)
{
  GdkDeviceManagerCore *device_manager;
  GdkDisplay *display;

  device_manager = GDK_DEVICE_MANAGER_CORE (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (GDK_DEVICE_MANAGER (device_manager), display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);
}

static void
translate_key_event (GdkDisplay           *display,
                     GdkDeviceManagerCore *device_manager,
		     GdkEvent             *event,
		     XEvent               *xevent)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (display);
  GdkModifierType consumed, state;
  gunichar c = 0;
  gchar buf[7];

  event->key.type = xevent->xany.type == KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
  event->key.time = xevent->xkey.time;
  gdk_event_set_device (event, device_manager->core_keyboard);

  event->key.state = (GdkModifierType) xevent->xkey.state;
  event->key.group = _gdk_x11_get_group_for_state (display, xevent->xkey.state);
  event->key.hardware_keycode = xevent->xkey.keycode;

  event->key.keyval = GDK_KEY_VoidSymbol;

  gdk_keymap_translate_keyboard_state (keymap,
				       event->key.hardware_keycode,
				       event->key.state,
				       event->key.group,
				       &event->key.keyval,
                                       NULL, NULL, &consumed);

  state = event->key.state & ~consumed;
  _gdk_keymap_add_virtual_modifiers_compat (keymap, &state);
  event->key.state |= state;

  event->key.is_modifier = _gdk_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->key.string = NULL;

  if (event->key.keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->key.keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib
       */
      if (event->key.state & GDK_CONTROL_MASK)
	{
	  if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
	  else if (c == '2')
	    {
	      event->key.string = g_memdup ("\0\0", 2);
	      event->key.length = 1;
	      buf[0] = '\0';
	      goto out;
	    }
	  else if (c >= '3' && c <= '7') c -= ('3' - '\033');
	  else if (c == '8') c = '\177';
	  else if (c == '/') c = '_' & 0x1F;
	}

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->key.string = g_locale_from_utf8 (buf, len,
					      NULL, &bytes_written,
					      NULL);
      if (event->key.string)
	event->key.length = bytes_written;
    }
  else if (event->key.keyval == GDK_KEY_Escape)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\033");
    }
  else if (event->key.keyval == GDK_KEY_Return ||
	  event->key.keyval == GDK_KEY_KP_Enter)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\r");
    }

  if (!event->key.string)
    {
      event->key.length = 0;
      event->key.string = g_strdup ("");
    }

 out:
#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      g_message ("%s:\t\twindow: %ld	 key: %12s  %d",
		 event->type == GDK_KEY_PRESS ? "key press  " : "key release",
		 xevent->xkey.window,
		 event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
		 event->key.keyval);

      if (event->key.length > 0)
	g_message ("\t\tlength: %4d string: \"%s\"",
		   event->key.length, buf);
    }
#endif /* G_ENABLE_DEBUG */
  return;
}

#ifdef G_ENABLE_DEBUG
static const char notify_modes[][19] = {
  "NotifyNormal",
  "NotifyGrab",
  "NotifyUngrab",
  "NotifyWhileGrabbed"
};

static const char notify_details[][23] = {
  "NotifyAncestor",
  "NotifyVirtual",
  "NotifyInferior",
  "NotifyNonlinear",
  "NotifyNonlinearVirtual",
  "NotifyPointer",
  "NotifyPointerRoot",
  "NotifyDetailNone"
};
#endif

static void
set_user_time (GdkWindow *window,
	       GdkEvent  *event)
{
  g_return_if_fail (event != NULL);

  window = gdk_window_get_toplevel (event->client.window);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
                                  gdk_event_get_time (event));
}

static void
generate_focus_event (GdkDeviceManagerCore *device_manager,
                      GdkWindow            *window,
		      gboolean              in)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (window);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = in;
  gdk_event_set_device (event, device_manager->core_keyboard);

  gdk_event_put (event);
  gdk_event_free (event);
}

static gboolean
set_screen_from_root (GdkDisplay *display,
		      GdkEvent   *event,
		      Window      xrootwin)
{
  GdkScreen *screen;

  screen = _gdk_x11_display_screen_for_xrootwin (display, xrootwin);

  if (screen)
    {
      gdk_event_set_screen (event, screen);

      return TRUE;
    }

  return FALSE;
}

static GdkCrossingMode
translate_crossing_mode (int mode)
{
  switch (mode)
    {
    case NotifyNormal:
      return GDK_CROSSING_NORMAL;
    case NotifyGrab:
      return GDK_CROSSING_GRAB;
    case NotifyUngrab:
      return GDK_CROSSING_UNGRAB;
    default:
      g_assert_not_reached ();
    }
}

static GdkNotifyType
translate_notify_type (int detail)
{
  switch (detail)
    {
    case NotifyInferior:
      return GDK_NOTIFY_INFERIOR;
    case NotifyAncestor:
      return GDK_NOTIFY_ANCESTOR;
    case NotifyVirtual:
      return GDK_NOTIFY_VIRTUAL;
    case NotifyNonlinear:
      return GDK_NOTIFY_NONLINEAR;
    case NotifyNonlinearVirtual:
      return GDK_NOTIFY_NONLINEAR_VIRTUAL;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
is_parent_of (GdkWindow *parent,
              GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static GList *
gdk_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                      GdkDeviceType     type)
{
  GdkDeviceManagerCore *device_manager_core;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      device_manager_core = (GdkDeviceManagerCore *) device_manager;
      devices = g_list_prepend (devices, device_manager_core->core_keyboard);
      devices = g_list_prepend (devices, device_manager_core->core_pointer);
    }

  return devices;
}

static GdkDevice *
gdk_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkDeviceManagerCore *device_manager_core;

  device_manager_core = (GdkDeviceManagerCore *) device_manager;
  return device_manager_core->core_pointer;
}

GdkDeviceManager *
_gdk_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_DEVICE_MANAGER_CORE,
		       "display", display,
		       NULL);
}
