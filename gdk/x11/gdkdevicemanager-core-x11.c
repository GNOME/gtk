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

#include "gdkx11devicemanager-core.h"
#include "gdkdevicemanagerprivate-core.h"
#include "gdkx11device-core.h"

#include "gdkdeviceprivate.h"
#include "gdkseatdefaultprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventtranslator.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkkeysyms.h"



#define APPEARS_FOCUSED(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_focus_window || (toplevel)->has_pointer_focus)

static void    gdk_x11_device_manager_core_finalize    (GObject *object);
static void    gdk_x11_device_manager_core_constructed (GObject *object);

static void     gdk_x11_device_manager_event_translator_init (GdkEventTranslatorIface *iface);

static gboolean gdk_x11_device_manager_core_translate_event  (GdkEventTranslator *translator,
                                                              GdkDisplay         *display,
                                                              GdkEvent           *event,
                                                              const XEvent       *xevent);


G_DEFINE_TYPE_WITH_CODE (GdkX11DeviceManagerCore, gdk_x11_device_manager_core, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_x11_device_manager_event_translator_init))

enum {
  PROP_0,
  PROP_DISPLAY
};

static void
gdk_device_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DISPLAY:
      GDK_X11_DEVICE_MANAGER_CORE (object)->display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_manager_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, GDK_X11_DEVICE_MANAGER_CORE (object)->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_manager_core_class_init (GdkX11DeviceManagerCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_x11_device_manager_core_finalize;
  object_class->constructed = gdk_x11_device_manager_core_constructed;
  object_class->set_property = gdk_device_manager_set_property;
  object_class->get_property = gdk_device_manager_get_property;

  g_object_class_install_property (object_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        "Display",
                                                        "Display for the device manager",
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
gdk_x11_device_manager_event_translator_init (GdkEventTranslatorIface *iface)
{
  iface->translate_event = gdk_x11_device_manager_core_translate_event;
}

static GdkDevice *
create_core_pointer (GdkX11DeviceManagerCore *device_manager,
                     GdkDisplay              *display)
{
  return g_object_new (GDK_TYPE_X11_DEVICE_CORE,
                       "name", "Core Pointer",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       NULL);
}

static GdkDevice *
create_core_keyboard (GdkX11DeviceManagerCore *device_manager,
                      GdkDisplay              *display)
{
  return g_object_new (GDK_TYPE_X11_DEVICE_CORE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       NULL);
}

static void
gdk_x11_device_manager_core_init (GdkX11DeviceManagerCore *device_manager)
{
}

static void
gdk_x11_device_manager_core_finalize (GObject *object)
{
  GdkX11DeviceManagerCore *device_manager_core;

  device_manager_core = GDK_X11_DEVICE_MANAGER_CORE (object);

  g_object_unref (device_manager_core->core_pointer);
  g_object_unref (device_manager_core->core_keyboard);

  G_OBJECT_CLASS (gdk_x11_device_manager_core_parent_class)->finalize (object);
}

static void
gdk_x11_device_manager_core_constructed (GObject *object)
{
  GdkX11DeviceManagerCore *device_manager;
  GdkDisplay *display;

  device_manager = GDK_X11_DEVICE_MANAGER_CORE (object);
  display = device_manager->display;
  device_manager->core_pointer = create_core_pointer (device_manager, display);
  device_manager->core_keyboard = create_core_keyboard (device_manager, display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);

  /* We expect subclasses to handle their own seats */
  if (G_OBJECT_TYPE (object) == GDK_TYPE_X11_DEVICE_MANAGER_CORE)
    {
      GdkSeat *seat;

      seat = gdk_seat_default_new_for_master_pair (device_manager->core_pointer,
                                                   device_manager->core_keyboard);

      gdk_display_add_seat (display, seat);
      g_object_unref (seat);
  }
}

static void
translate_key_event (GdkDisplay              *display,
                     GdkX11DeviceManagerCore *device_manager,
                     GdkEvent                *event,
                     const XEvent            *xevent)
{
  GdkKeymap *keymap = gdk_display_get_keymap (display);
  GdkModifierType consumed, state;

  event->any.type = xevent->xany.type == KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
  event->key.time = xevent->xkey.time;
  gdk_event_set_device (event, device_manager->core_keyboard);

  event->key.state = (GdkModifierType) xevent->xkey.state;
  event->key.group = gdk_x11_keymap_get_group_for_state (keymap, xevent->xkey.state);
  event->key.hardware_keycode = xevent->xkey.keycode;
  gdk_event_set_scancode (event, xevent->xkey.keycode);

  event->key.keyval = GDK_KEY_VoidSymbol;

  gdk_keymap_translate_keyboard_state (keymap,
                                       event->key.hardware_keycode,
                                       event->key.state,
                                       event->key.group,
                                       &event->key.keyval,
                                       NULL, NULL, &consumed);

  state = event->key.state & ~consumed;
  _gdk_x11_keymap_add_virt_mods (keymap, &state);
  event->key.state |= state;

  event->key.is_modifier = gdk_x11_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

  _gdk_x11_event_translate_keyboard_string (&event->key);

#ifdef G_ENABLE_DEBUG
  if (GDK_DISPLAY_DEBUG_CHECK (display, EVENTS))
    {
      g_message ("%s:\t\twindow: %ld     key: %12s  %d",
                 event->any.type == GDK_KEY_PRESS ? "key press  " : "key release",
                 xevent->xkey.window,
                 event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
                 event->key.keyval);

      if (event->key.length > 0)
        g_message ("\t\tlength: %4d string: \"%s\"",
                   event->key.length, event->key.string);
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
set_user_time (GdkSurface *surface,
               GdkEvent  *event)
{
  g_return_if_fail (event != NULL);

  surface = gdk_surface_get_toplevel (event->any.surface);
  g_return_if_fail (GDK_IS_SURFACE (surface));

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    gdk_x11_surface_set_user_time (gdk_surface_get_toplevel (surface),
                                  gdk_event_get_time (event));
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
      return GDK_CROSSING_NORMAL;
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
      return GDK_NOTIFY_UNKNOWN;
    }
}

static gboolean
is_parent_of (GdkSurface *parent,
              GdkSurface *child)
{
  GdkSurface *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
        return TRUE;

      w = gdk_surface_get_parent (w);
    }

  return FALSE;
}

static GdkSurface *
get_event_surface (GdkEventTranslator *translator,
                  const XEvent       *xevent)
{
  GdkDisplay *display;
  GdkSurface *surface;

  display = GDK_X11_DEVICE_MANAGER_CORE (translator)->display;
  surface = gdk_x11_surface_lookup_for_display (display, xevent->xany.window);

  /* Apply keyboard grabs to non-native windows */
  if (xevent->type == KeyPress || xevent->type == KeyRelease)
    {
      GdkDeviceGrabInfo *info;
      gulong serial;

      serial = _gdk_display_get_next_serial (display);
      info = _gdk_display_has_device_grab (display,
                                           GDK_X11_DEVICE_MANAGER_CORE (translator)->core_keyboard,
                                           serial);
      if (info &&
          (!is_parent_of (info->surface, surface) ||
           !info->owner_events))
        {
          /* Report key event against grab surface */
          surface = info->surface;
        }
    }

  return surface;
}

static gboolean
gdk_x11_device_manager_core_translate_event (GdkEventTranslator *translator,
                                             GdkDisplay         *display,
                                             GdkEvent           *event,
                                             const XEvent       *xevent)
{
  GdkSurfaceImplX11 *impl;
  GdkX11DeviceManagerCore *device_manager;
  GdkSurface *surface;
  gboolean return_val;
  int scale;
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  device_manager = GDK_X11_DEVICE_MANAGER_CORE (translator);

  surface = get_event_surface (translator, xevent);

  scale = 1;
  if (surface)
    {
      if (GDK_SURFACE_DESTROYED (surface) || !GDK_IS_SURFACE (surface))
        return FALSE;

      g_object_ref (surface);
      impl = GDK_SURFACE_IMPL_X11 (surface->impl);
      scale = impl->surface_scale;
    }

  event->any.surface = surface;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;

  if (surface && GDK_SURFACE_DESTROYED (surface))
    {
      if (xevent->type != DestroyNotify)
        {
          return_val = FALSE;
          goto done;
        }
    }

  if (surface &&
      (xevent->type == MotionNotify ||
       xevent->type == ButtonRelease))
    {
      if (_gdk_x11_moveresize_handle_event (xevent))
        {
          return_val = FALSE;
          goto done;
        }
    }

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  switch (xevent->type)
    {
    case KeyPress:
      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }
      translate_key_event (display, device_manager, event, xevent);
      set_user_time (surface, event);
      break;

    case KeyRelease:
      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* Emulate detectable auto-repeat by checking to see
       * if the next event is a key press with the same
       * keycode and timestamp, and if so, ignoring the event.
       */

      if (!display_x11->have_xkb_autorepeat && XPending (xevent->xkey.display))
        {
          XEvent next_event;

          XPeekEvent (xevent->xkey.display, &next_event);

          if (next_event.type == KeyPress &&
              next_event.xkey.keycode == xevent->xkey.keycode &&
              next_event.xkey.time == xevent->xkey.time)
            {
              return_val = FALSE;
              break;
            }
        }

      translate_key_event (display, device_manager, event, xevent);
      break;

    case ButtonPress:
      GDK_DISPLAY_NOTE (display, EVENTS,
                g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
                           xevent->xbutton.window,
                           xevent->xbutton.x, xevent->xbutton.y,
                           xevent->xbutton.button));

      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* If we get a ButtonPress event where the button is 4 or 5,
         it's a Scroll event */
      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
          event->any.type = GDK_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = GDK_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = GDK_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = GDK_SCROLL_LEFT;
          else
            event->scroll.direction = GDK_SCROLL_RIGHT;

          event->any.surface = surface;
          event->scroll.time = xevent->xbutton.time;
          event->scroll.x = (gdouble) xevent->xbutton.x / scale;
          event->scroll.y = (gdouble) xevent->xbutton.y / scale;
          event->scroll.x_root = (gdouble) xevent->xbutton.x_root / scale;
          event->scroll.y_root = (gdouble) xevent->xbutton.y_root / scale;
          event->scroll.state = (GdkModifierType) xevent->xbutton.state;
          gdk_event_set_device (event, device_manager->core_pointer);

          event->scroll.delta_x = 0;
          event->scroll.delta_y = 0;

          gdk_event_set_display (event, display);

          break;

        default:
          event->any.type = GDK_BUTTON_PRESS;
          event->any.surface = surface;
          event->button.time = xevent->xbutton.time;
          event->button.x = (gdouble) xevent->xbutton.x / scale;
          event->button.y = (gdouble) xevent->xbutton.y / scale;
          event->button.x_root = (gdouble) xevent->xbutton.x_root / scale;
          event->button.y_root = (gdouble) xevent->xbutton.y_root / scale;
          event->button.axes = NULL;
          event->button.state = (GdkModifierType) xevent->xbutton.state;
          event->button.button = xevent->xbutton.button;
          gdk_event_set_device (event, device_manager->core_pointer);

          gdk_event_set_display (event, display);

          break;
        }

      set_user_time (surface, event);

      break;

    case ButtonRelease:
      GDK_DISPLAY_NOTE (display, EVENTS,
                g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
                           xevent->xbutton.window,
                           xevent->xbutton.x, xevent->xbutton.y,
                           xevent->xbutton.button));

      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* We treat button presses as scroll wheel events, so ignore the release */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 || xevent->xbutton.button == 7)
        {
          return_val = FALSE;
          break;
        }

      event->any.type = GDK_BUTTON_RELEASE;
      event->any.surface = surface;
      event->button.time = xevent->xbutton.time;
      event->button.x = (gdouble) xevent->xbutton.x / scale;
      event->button.y = (gdouble) xevent->xbutton.y / scale;
      event->button.x_root = (gdouble) xevent->xbutton.x_root / scale;
      event->button.y_root = (gdouble) xevent->xbutton.y_root / scale;
      event->button.axes = NULL;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      gdk_event_set_device (event, device_manager->core_pointer);

      gdk_event_set_display (event, display);

      break;

    case MotionNotify:
      GDK_DISPLAY_NOTE (display, EVENTS,
                g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s",
                           xevent->xmotion.window,
                           xevent->xmotion.x, xevent->xmotion.y,
                           (xevent->xmotion.is_hint) ? "true" : "false"));

      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }

      event->any.type = GDK_MOTION_NOTIFY;
      event->any.surface = surface;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = (gdouble) xevent->xmotion.x / scale;
      event->motion.y = (gdouble) xevent->xmotion.y / scale;
      event->motion.x_root = (gdouble) xevent->xmotion.x_root / scale;
      event->motion.y_root = (gdouble) xevent->xmotion.y_root / scale;
      event->motion.axes = NULL;
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      gdk_event_set_device (event, device_manager->core_pointer);

      gdk_event_set_display (event, display);

      break;

    case EnterNotify:
      GDK_DISPLAY_NOTE (display, EVENTS,
                g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail,
                           xevent->xcrossing.subwindow));

      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }

      gdk_event_set_display (event, display);

      event->any.type = GDK_ENTER_NOTIFY;
      event->any.surface = surface;
      gdk_event_set_device (event, device_manager->core_pointer);

      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkSurface.
       */
      if (xevent->xcrossing.subwindow != None)
        event->crossing.child_surface = gdk_x11_surface_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
        event->crossing.child_surface = NULL;

      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = (gdouble) xevent->xcrossing.x / scale;
      event->crossing.y = (gdouble) xevent->xcrossing.y / scale;
      event->crossing.x_root = (gdouble) xevent->xcrossing.x_root / scale;
      event->crossing.y_root = (gdouble) xevent->xcrossing.y_root / scale;

      event->crossing.mode = translate_crossing_mode (xevent->xcrossing.mode);
      event->crossing.detail = translate_notify_type (xevent->xcrossing.detail);

      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;

      break;

    case LeaveNotify:
      GDK_DISPLAY_NOTE (display, EVENTS,
                g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail, xevent->xcrossing.subwindow));

      if (surface == NULL)
        {
          return_val = FALSE;
          break;
        }

      gdk_event_set_display (event, display);

      event->any.type = GDK_LEAVE_NOTIFY;
      event->any.surface = surface;
      gdk_event_set_device (event, device_manager->core_pointer);

      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkSurface.
       */
      if (xevent->xcrossing.subwindow != None)
        event->crossing.child_surface = gdk_x11_surface_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
        event->crossing.child_surface = NULL;

      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = (gdouble) xevent->xcrossing.x / scale;
      event->crossing.y = (gdouble) xevent->xcrossing.y / scale;
      event->crossing.x_root = (gdouble) xevent->xcrossing.x_root / scale;
      event->crossing.y_root = (gdouble) xevent->xcrossing.y_root / scale;

      event->crossing.mode = translate_crossing_mode (xevent->xcrossing.mode);
      event->crossing.detail = translate_notify_type (xevent->xcrossing.detail);

      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;

      break;

    case FocusIn:
    case FocusOut:
      if (surface)
        _gdk_device_manager_core_handle_focus (surface,
                                               xevent->xfocus.window,
                                               device_manager->core_keyboard,
                                               NULL,
                                               xevent->type == FocusIn,
                                               xevent->xfocus.detail,
                                               xevent->xfocus.mode);
      return_val = FALSE;
      break;
                                              
    default:
        return_val = FALSE;
    }

 done:
  if (return_val)
    {
      if (event->any.surface)
        g_object_ref (event->any.surface);

      if (((event->any.type == GDK_ENTER_NOTIFY) ||
           (event->any.type == GDK_LEAVE_NOTIFY)) &&
          (event->crossing.child_surface != NULL))
        g_object_ref (event->crossing.child_surface);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.surface = NULL;
      event->any.type = GDK_NOTHING;
    }

  if (surface)
    g_object_unref (surface);

  return return_val;
}

void
_gdk_x11_event_translate_keyboard_string (GdkEventKey *event)
{
  gunichar c = 0;
  gchar buf[7];

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->string = NULL;

  if (event->keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

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

/* We only care about focus events that indicate that _this_
 * surface (not a ancestor or child) got or lost the focus
 */
void
_gdk_device_manager_core_handle_focus (GdkSurface *surface,
                                       Window     original,
                                       GdkDevice *device,
                                       GdkDevice *source_device,
                                       gboolean   focus_in,
                                       int        detail,
                                       int        mode)
{
  GdkToplevelX11 *toplevel;
  GdkX11Screen *x11_screen;
  gboolean had_focus;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (source_device == NULL || GDK_IS_DEVICE (source_device));

  GDK_DISPLAY_NOTE (gdk_surface_get_display (surface), EVENTS,
            g_message ("focus out:\t\twindow: %ld, detail: %s, mode: %s",
                       GDK_SURFACE_XID (surface),
                       notify_details[detail],
                       notify_modes[mode]));

  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (!toplevel)
    return;

  if (toplevel->focus_window == original)
    return;

  had_focus = APPEARS_FOCUSED (toplevel);
  x11_screen = GDK_X11_SCREEN (GDK_SURFACE_SCREEN (surface));

  switch (detail)
    {
    case NotifyAncestor:
    case NotifyVirtual:
      /* When the focus moves from an ancestor of the window to
       * the window or a descendent of the window, *and* the
       * pointer is inside the window, then we were previously
       * receiving keystroke events in the has_pointer_focus
       * case and are now receiving them in the
       * has_focus_window case.
       */
      if (toplevel->has_pointer &&
          !x11_screen->wmspec_check_window &&
          mode != NotifyGrab &&
#ifdef XINPUT_2
	  mode != XINotifyPassiveGrab &&
	  mode != XINotifyPassiveUngrab &&
#endif /* XINPUT_2 */
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (focus_in) ? FALSE : TRUE;

      /* fall through */
    case NotifyNonlinear:
    case NotifyNonlinearVirtual:
      if (mode != NotifyGrab &&
#ifdef XINPUT_2
	  mode != XINotifyPassiveGrab &&
	  mode != XINotifyPassiveUngrab &&
#endif /* XINPUT_2 */
          mode != NotifyUngrab)
        toplevel->has_focus_window = (focus_in) ? TRUE : FALSE;
      /* We pretend that the focus moves to the grab
       * window, so we pay attention to NotifyGrab
       * NotifyUngrab, and ignore NotifyWhileGrabbed
       */
      if (mode != NotifyWhileGrabbed)
        toplevel->has_focus = (focus_in) ? TRUE : FALSE;
      break;
    case NotifyPointer:
      /* The X server sends NotifyPointer/NotifyGrab,
       * but the pointer focus is ignored while a
       * grab is in effect
       */
      if (!x11_screen->wmspec_check_window &&
          mode != NotifyGrab &&
#ifdef XINPUT_2
	  mode != XINotifyPassiveGrab &&
	  mode != XINotifyPassiveUngrab &&
#endif /* XINPUT_2 */
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (focus_in) ? TRUE : FALSE;
      break;
    case NotifyInferior:
    case NotifyPointerRoot:
    case NotifyDetailNone:
    default:
      break;
    }

  if (APPEARS_FOCUSED (toplevel) != had_focus)
    {
      GdkEvent *event;

      event = gdk_event_new (GDK_FOCUS_CHANGE);
      event->any.surface = g_object_ref (surface);
      event->any.send_event = FALSE;
      event->focus_change.in = focus_in;
      gdk_event_set_device (event, device);
      if (source_device)
        gdk_event_set_source_device (event, source_device);

      gdk_display_put_event (gdk_surface_get_display (surface), event);
      g_object_unref (event);
    }
}

