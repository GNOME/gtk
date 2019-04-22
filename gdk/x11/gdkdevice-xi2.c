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

#include "gdkx11device-xi2.h"
#include "gdkdeviceprivate.h"

#include "gdkintl.h"
#include "gdkasync.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include <math.h>

typedef struct _ScrollValuator ScrollValuator;

struct _ScrollValuator
{
  guint n_valuator       : 4;
  guint direction        : 4;
  guint last_value_valid : 1;
  gdouble last_value;
  gdouble increment;
};

struct _GdkX11DeviceXI2
{
  GdkDevice parent_instance;

  gint device_id;
  GArray *scroll_valuators;
  gdouble *last_axes;
};

struct _GdkX11DeviceXI2Class
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkX11DeviceXI2, gdk_x11_device_xi2, GDK_TYPE_DEVICE)


static void gdk_x11_device_xi2_finalize     (GObject      *object);
static void gdk_x11_device_xi2_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void gdk_x11_device_xi2_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void gdk_x11_device_xi2_get_state (GdkDevice       *device,
                                          GdkSurface       *surface,
                                          gdouble         *axes,
                                          GdkModifierType *mask);
static void gdk_x11_device_xi2_set_surface_cursor (GdkDevice *device,
                                                  GdkSurface *surface,
                                                  GdkCursor *cursor);
static void gdk_x11_device_xi2_query_state (GdkDevice        *device,
                                            GdkSurface        *surface,
                                            GdkSurface       **child_surface,
                                            gdouble          *root_x,
                                            gdouble          *root_y,
                                            gdouble          *win_x,
                                            gdouble          *win_y,
                                            GdkModifierType  *mask);

static GdkGrabStatus gdk_x11_device_xi2_grab   (GdkDevice     *device,
                                                GdkSurface     *surface,
                                                gboolean       owner_events,
                                                GdkEventMask   event_mask,
                                                GdkSurface     *confine_to,
                                                GdkCursor     *cursor,
                                                guint32        time_);
static void          gdk_x11_device_xi2_ungrab (GdkDevice     *device,
                                                guint32        time_);

static GdkSurface * gdk_x11_device_xi2_surface_at_position (GdkDevice       *device,
                                                          gdouble         *win_x,
                                                          gdouble         *win_y,
                                                          GdkModifierType *mask,
                                                          gboolean         get_toplevel);


enum {
  PROP_0,
  PROP_DEVICE_ID
};

static void
gdk_x11_device_xi2_class_init (GdkX11DeviceXI2Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  object_class->finalize = gdk_x11_device_xi2_finalize;
  object_class->get_property = gdk_x11_device_xi2_get_property;
  object_class->set_property = gdk_x11_device_xi2_set_property;

  device_class->get_state = gdk_x11_device_xi2_get_state;
  device_class->set_surface_cursor = gdk_x11_device_xi2_set_surface_cursor;
  device_class->query_state = gdk_x11_device_xi2_query_state;
  device_class->grab = gdk_x11_device_xi2_grab;
  device_class->ungrab = gdk_x11_device_xi2_ungrab;
  device_class->surface_at_position = gdk_x11_device_xi2_surface_at_position;

  g_object_class_install_property (object_class,
                                   PROP_DEVICE_ID,
                                   g_param_spec_int ("device-id",
                                                     P_("Device ID"),
                                                     P_("Device identifier"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
}

static void
gdk_x11_device_xi2_init (GdkX11DeviceXI2 *device)
{
  device->scroll_valuators = g_array_new (FALSE, FALSE, sizeof (ScrollValuator));
}

static void
gdk_x11_device_xi2_finalize (GObject *object)
{
  GdkX11DeviceXI2 *device = GDK_X11_DEVICE_XI2 (object);

  g_array_free (device->scroll_valuators, TRUE);
  g_free (device->last_axes);

  G_OBJECT_CLASS (gdk_x11_device_xi2_parent_class)->finalize (object);
}

static void
gdk_x11_device_xi2_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_int (value, device_xi2->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_xi2_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      device_xi2->device_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_xi2_get_state (GdkDevice       *device,
                              GdkSurface       *surface,
                              gdouble         *axes,
                              GdkModifierType *mask)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);

  if (axes)
    {
      GdkDisplay *display;
      XIDeviceInfo *info;
      gint i, j, ndevices;
      Screen *xscreen;

      display = gdk_device_get_display (device);
      xscreen = GDK_X11_SCREEN (GDK_X11_DISPLAY (display)->screen)->xscreen;

      gdk_x11_display_error_trap_push (display);
      info = XIQueryDevice (GDK_DISPLAY_XDISPLAY (display),
                            device_xi2->device_id, &ndevices);
      gdk_x11_display_error_trap_pop_ignored (display);

      for (i = 0, j = 0; info && i < info->num_classes; i++)
        {
          XIAnyClassInfo *class_info = info->classes[i];
          GdkAxisUse use;
          gdouble value;

          if (class_info->type != XIValuatorClass)
            continue;

          value = ((XIValuatorClassInfo *) class_info)->value;
          use = gdk_device_get_axis_use (device, j);

          switch ((guint) use)
            {
            case GDK_AXIS_X:
            case GDK_AXIS_Y:
            case GDK_AXIS_IGNORE:
              if (gdk_device_get_mode (device) == GDK_MODE_SURFACE)
                _gdk_device_translate_surface_coord (device, surface, j, value, &axes[j]);
              else
                {
                  gint root_x, root_y;

                  /* FIXME: Maybe root coords chaching should happen here */
                  gdk_surface_get_origin (surface, &root_x, &root_y);
                  _gdk_device_translate_screen_coord (device, surface,
                                                      root_x, root_y,
                                                      WidthOfScreen (xscreen),
                                                      HeightOfScreen (xscreen),
                                                      j, value,
                                                      &axes[j]);
                }
              break;
            default:
              _gdk_device_translate_axis (device, j, value, &axes[j]);
              break;
            }

          j++;
        }

      if (info)
        XIFreeDeviceInfo (info);
    }

  if (mask)
    gdk_x11_device_xi2_query_state (device, surface,
                                    NULL,
                                    NULL, NULL,
                                    NULL, NULL,
                                    mask);
}

static void
gdk_x11_device_xi2_set_surface_cursor (GdkDevice *device,
                                      GdkSurface *surface,
                                      GdkCursor *cursor)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);

  /* Non-master devices don't have a cursor */
  if (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER)
    return;

  if (cursor)
    XIDefineCursor (GDK_SURFACE_XDISPLAY (surface),
                    device_xi2->device_id,
                    GDK_SURFACE_XID (surface),
                    gdk_x11_display_get_xcursor (GDK_SURFACE_DISPLAY (surface), cursor));
  else
    XIUndefineCursor (GDK_SURFACE_XDISPLAY (surface),
                      device_xi2->device_id,
                      GDK_SURFACE_XID (surface));
}

static void
gdk_x11_device_xi2_query_state (GdkDevice        *device,
                                GdkSurface        *surface,
                                GdkSurface       **child_surface,
                                gdouble          *root_x,
                                gdouble          *root_y,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkDisplay *display;
  GdkX11Screen *default_screen;
  Window xroot_window, xchild_window, xwindow;
  gdouble xroot_x, xroot_y, xwin_x, xwin_y;
  XIButtonState button_state;
  XIModifierState mod_state;
  XIGroupState group_state;
  int scale;

  display = gdk_device_get_display (device);
  default_screen = GDK_X11_DISPLAY (display)->screen;
  if (surface == NULL)
    {
      xwindow = GDK_DISPLAY_XROOTWIN (display);
      scale = default_screen->surface_scale;
    }
  else
    {
      xwindow = GDK_SURFACE_XID (surface);
      scale = GDK_X11_SURFACE (surface)->surface_scale;
    }

  if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_SLAVE)
    {
      GdkDevice *master = gdk_device_get_associated_device (device);

      if (master)
        _gdk_device_query_state (master, surface, child_surface,
                                 root_x, root_y, win_x, win_y, mask);
      return;
    }

  if (!GDK_X11_DISPLAY (display)->trusted_client ||
      !XIQueryPointer (GDK_DISPLAY_XDISPLAY (display),
                       device_xi2->device_id,
                       xwindow,
                       &xroot_window,
                       &xchild_window,
                       &xroot_x, &xroot_y,
                       &xwin_x, &xwin_y,
                       &button_state,
                       &mod_state,
                       &group_state))
    {
      XSetWindowAttributes attributes;
      Display *xdisplay;
      Window w;

      /* FIXME: untrusted clients not multidevice-safe */
      xdisplay = GDK_SCREEN_XDISPLAY (default_screen);
      xwindow = GDK_SCREEN_XROOTWIN (default_screen);

      w = XCreateWindow (xdisplay, xwindow, 0, 0, 1, 1, 0,
                         CopyFromParent, InputOnly, CopyFromParent,
                         0, &attributes);
      XIQueryPointer (xdisplay, device_xi2->device_id,
                      w,
                      &xroot_window,
                      &xchild_window,
                      &xroot_x, &xroot_y,
                      &xwin_x, &xwin_y,
                      &button_state,
                      &mod_state,
                      &group_state);
      XDestroyWindow (xdisplay, w);
    }

  if (child_surface)
    *child_surface = gdk_x11_surface_lookup_for_display (display, xchild_window);

  if (root_x)
    *root_x = xroot_x / scale;

  if (root_y)
    *root_y = xroot_y / scale;

  if (win_x)
    *win_x = xwin_x / scale;

  if (win_y)
    *win_y = xwin_y / scale;

  if (mask)
    *mask = _gdk_x11_device_xi2_translate_state (&mod_state, &button_state, &group_state);

  free (button_state.mask);
}

static GdkGrabStatus
gdk_x11_device_xi2_grab (GdkDevice    *device,
                         GdkSurface    *surface,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkSurface    *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkX11DeviceManagerXI2 *device_manager_xi2;
  GdkDisplay *display;
  XIEventMask mask;
  Window xwindow;
  Cursor xcursor;
  gint status;

  display = gdk_device_get_display (device);
  device_manager_xi2 = GDK_X11_DEVICE_MANAGER_XI2 (GDK_X11_DISPLAY (display)->device_manager);

  /* FIXME: confine_to is actually unused */

  xwindow = GDK_SURFACE_XID (surface);

  if (!cursor)
    xcursor = None;
  else
    {
      xcursor = gdk_x11_display_get_xcursor (display, cursor);
    }

  mask.deviceid = device_xi2->device_id;
  mask.mask = _gdk_x11_device_xi2_translate_event_mask (device_manager_xi2,
                                                        event_mask,
                                                        &mask.mask_len);

#ifdef G_ENABLE_DEBUG
  if (GDK_DISPLAY_DEBUG_CHECK (display, NOGRABS))
    status = GrabSuccess;
  else
#endif
    status = XIGrabDevice (GDK_DISPLAY_XDISPLAY (display),
                           device_xi2->device_id,
                           xwindow,
                           time_,
                           xcursor,
                           GrabModeAsync, GrabModeAsync,
                           owner_events,
                           &mask);

  g_free (mask.mask);

  _gdk_x11_display_update_grab_info (display, device, status);

  return _gdk_x11_convert_grab_status (status);
}

static void
gdk_x11_device_xi2_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkDisplay *display;
  gulong serial;

  display = gdk_device_get_display (device);
  serial = NextRequest (GDK_DISPLAY_XDISPLAY (display));

  XIUngrabDevice (GDK_DISPLAY_XDISPLAY (display), device_xi2->device_id, time_);

  _gdk_x11_display_update_grab_info_ungrab (display, device, time_, serial);
}

static GdkSurface *
gdk_x11_device_xi2_surface_at_position (GdkDevice       *device,
                                       gdouble         *win_x,
                                       gdouble         *win_y,
                                       GdkModifierType *mask,
                                       gboolean         get_toplevel)
{
  GdkX11Surface *impl;
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkDisplay *display;
  GdkX11Screen *screen;
  Display *xdisplay;
  GdkSurface *surface;
  Window xwindow, root, child, last = None;
  gdouble xroot_x, xroot_y, xwin_x, xwin_y;
  XIButtonState button_state = { 0 };
  XIModifierState mod_state;
  XIGroupState group_state;
  Bool retval;

  display = gdk_device_get_display (device);
  screen = GDK_X11_DISPLAY (display)->screen;

  gdk_x11_display_error_trap_push (display);

  /* This function really only works if the mouse pointer is held still
   * during its operation. If it moves from one leaf window to another
   * than we'll end up with inaccurate values for win_x, win_y
   * and the result.
   */
  gdk_x11_display_grab (display);

  xdisplay = GDK_SCREEN_XDISPLAY (screen);
  xwindow = GDK_SCREEN_XROOTWIN (screen);

  if (G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    {
      XIQueryPointer (xdisplay,
                      device_xi2->device_id,
                      xwindow,
                      &root, &child,
                      &xroot_x, &xroot_y,
                      &xwin_x, &xwin_y,
                      &button_state,
                      &mod_state,
                      &group_state);

      if (root == xwindow)
        xwindow = child;
      else
        xwindow = root;
    }
  else
    {
      gint width, height;
      GList *toplevels, *list;
      Window pointer_window;

      /* FIXME: untrusted clients case not multidevice-safe */
      pointer_window = None;

      toplevels = gdk_x11_display_get_toplevel_windows (display);
      for (list = toplevels; list != NULL; list = list->next)
        {
          surface = GDK_SURFACE (list->data);
          xwindow = GDK_SURFACE_XID (surface);

          /* Free previous button mask, if any */
          g_free (button_state.mask);

          retval = XIQueryPointer (xdisplay,
                                   device_xi2->device_id,
                                   xwindow,
                                   &root, &child,
                                   &xroot_x, &xroot_y,
                                   &xwin_x, &xwin_y,
                                   &button_state,
                                   &mod_state,
                                   &group_state);
          if (!retval)
            continue;

          if (child != None)
            {
              pointer_window = child;
              break;
            }
          gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);
          if (xwin_x >= 0 && xwin_y >= 0 && xwin_x < width && xwin_y < height)
            {
              /* A childless toplevel, or below another window? */
              XSetWindowAttributes attributes;
              Window w;

              free (button_state.mask);

              w = XCreateWindow (xdisplay, xwindow, (int)xwin_x, (int)xwin_y, 1, 1, 0,
                                 CopyFromParent, InputOnly, CopyFromParent,
                                 0, &attributes);
              XMapWindow (xdisplay, w);
              XIQueryPointer (xdisplay,
                              device_xi2->device_id,
                              xwindow,
                              &root, &child,
                              &xroot_x, &xroot_y,
                              &xwin_x, &xwin_y,
                              &button_state,
                              &mod_state,
                              &group_state);
              XDestroyWindow (xdisplay, w);
              if (child == w)
                {
                  pointer_window = xwindow;
                  break;
                }
            }

          if (pointer_window != None)
            break;
        }

      xwindow = pointer_window;
    }

  while (xwindow)
    {
      last = xwindow;
      free (button_state.mask);

      retval = XIQueryPointer (xdisplay,
                               device_xi2->device_id,
                               xwindow,
                               &root, &xwindow,
                               &xroot_x, &xroot_y,
                               &xwin_x, &xwin_y,
                               &button_state,
                               &mod_state,
                               &group_state);
      if (!retval)
        break;

      if (get_toplevel && last != root &&
          (surface = gdk_x11_surface_lookup_for_display (display, last)) != NULL)
        {
          xwindow = last;
          break;
        }
    }

  gdk_x11_display_ungrab (display);

  if (gdk_x11_display_error_trap_pop (display) == 0)
    {
      surface = gdk_x11_surface_lookup_for_display (display, last);
      impl = NULL;
      if (surface)
        impl = GDK_X11_SURFACE (surface);

      if (mask)
        *mask = _gdk_x11_device_xi2_translate_state (&mod_state, &button_state, &group_state);

      free (button_state.mask);
    }
  else
    {
      surface = NULL;

      if (mask)
        *mask = 0;
    }

  if (win_x)
    *win_x = (surface) ? (xwin_x / impl->surface_scale) : -1;

  if (win_y)
    *win_y = (surface) ? (xwin_y / impl->surface_scale) : -1;


  return surface;
}

guchar *
_gdk_x11_device_xi2_translate_event_mask (GdkX11DeviceManagerXI2 *device_manager_xi2,
                                          GdkEventMask            event_mask,
                                          gint                   *len)
{
  guchar *mask;
  gint minor;

  g_object_get (device_manager_xi2, "minor", &minor, NULL);

  *len = XIMaskLen (XI_LASTEVENT);
  mask = g_new0 (guchar, *len);

  if (event_mask & GDK_POINTER_MOTION_MASK)
    XISetMask (mask, XI_Motion);

  if (event_mask & GDK_BUTTON_MOTION_MASK ||
      event_mask & GDK_BUTTON1_MOTION_MASK ||
      event_mask & GDK_BUTTON2_MOTION_MASK ||
      event_mask & GDK_BUTTON3_MOTION_MASK)
    {
      XISetMask (mask, XI_ButtonPress);
      XISetMask (mask, XI_ButtonRelease);
      XISetMask (mask, XI_Motion);
    }

  if (event_mask & GDK_SCROLL_MASK)
    {
      XISetMask (mask, XI_ButtonPress);
      XISetMask (mask, XI_ButtonRelease);
    }

  if (event_mask & GDK_BUTTON_PRESS_MASK)
    XISetMask (mask, XI_ButtonPress);

  if (event_mask & GDK_BUTTON_RELEASE_MASK)
    XISetMask (mask, XI_ButtonRelease);

  if (event_mask & GDK_KEY_PRESS_MASK)
    XISetMask (mask, XI_KeyPress);

  if (event_mask & GDK_KEY_RELEASE_MASK)
    XISetMask (mask, XI_KeyRelease);

  if (event_mask & GDK_ENTER_NOTIFY_MASK)
    XISetMask (mask, XI_Enter);

  if (event_mask & GDK_LEAVE_NOTIFY_MASK)
    XISetMask (mask, XI_Leave);

  if (event_mask & GDK_FOCUS_CHANGE_MASK)
    {
      XISetMask (mask, XI_FocusIn);
      XISetMask (mask, XI_FocusOut);
    }

#ifdef XINPUT_2_2
  /* XInput 2.2 includes multitouch support */
  if (minor >= 2 &&
      event_mask & GDK_TOUCH_MASK)
    {
      XISetMask (mask, XI_TouchBegin);
      XISetMask (mask, XI_TouchUpdate);
      XISetMask (mask, XI_TouchEnd);
    }
#endif /* XINPUT_2_2 */

  return mask;
}

guint
_gdk_x11_device_xi2_translate_state (XIModifierState *mods_state,
                                     XIButtonState   *buttons_state,
                                     XIGroupState    *group_state)
{
  guint state = 0;

  if (mods_state)
    state = mods_state->effective;

  if (buttons_state)
    {
      gint len, i;

      /* We're only interested in the first 3 buttons */
      len = MIN (3, buttons_state->mask_len * 8);

      for (i = 1; i <= len; i++)
        {
          if (!XIMaskIsSet (buttons_state->mask, i))
            continue;

          switch (i)
            {
            case 1:
              state |= GDK_BUTTON1_MASK;
              break;
            case 2:
              state |= GDK_BUTTON2_MASK;
              break;
            case 3:
              state |= GDK_BUTTON3_MASK;
              break;
            default:
              break;
            }
        }
    }

  if (group_state)
    state |= (group_state->effective) << 13;

  return state;
}

void
_gdk_x11_device_xi2_add_scroll_valuator (GdkX11DeviceXI2    *device,
                                         guint               n_valuator,
                                         GdkScrollDirection  direction,
                                         gdouble             increment)
{
  ScrollValuator scroll;

  g_return_if_fail (GDK_IS_X11_DEVICE_XI2 (device));
  g_return_if_fail (n_valuator < gdk_device_get_n_axes (GDK_DEVICE (device)));

  scroll.n_valuator = n_valuator;
  scroll.direction = direction;
  scroll.last_value_valid = FALSE;
  scroll.increment = increment;

  g_array_append_val (device->scroll_valuators, scroll);
}

gboolean
_gdk_x11_device_xi2_get_scroll_delta (GdkX11DeviceXI2    *device,
                                      guint               n_valuator,
                                      gdouble             valuator_value,
                                      GdkScrollDirection *direction_ret,
                                      gdouble            *delta_ret)
{
  guint i;

  g_return_val_if_fail (GDK_IS_X11_DEVICE_XI2 (device), FALSE);
  g_return_val_if_fail (n_valuator < gdk_device_get_n_axes (GDK_DEVICE (device)), FALSE);

  for (i = 0; i < device->scroll_valuators->len; i++)
    {
      ScrollValuator *scroll;

      scroll = &g_array_index (device->scroll_valuators, ScrollValuator, i);

      if (scroll->n_valuator == n_valuator)
        {
          if (direction_ret)
            *direction_ret = scroll->direction;

          if (delta_ret)
            *delta_ret = 0;

          if (scroll->last_value_valid)
            {
              if (delta_ret)
                *delta_ret = (valuator_value - scroll->last_value) / scroll->increment;

              scroll->last_value = valuator_value;
            }
          else
            {
              scroll->last_value = valuator_value;
              scroll->last_value_valid = TRUE;
            }

          return TRUE;
        }
    }

  return FALSE;
}

void
_gdk_device_xi2_reset_scroll_valuators (GdkX11DeviceXI2 *device)
{
  guint i;

  for (i = 0; i < device->scroll_valuators->len; i++)
    {
      ScrollValuator *scroll;

      scroll = &g_array_index (device->scroll_valuators, ScrollValuator, i);
      scroll->last_value_valid = FALSE;
    }
}

void
_gdk_device_xi2_unset_scroll_valuators (GdkX11DeviceXI2 *device)
{
  if (device->scroll_valuators->len > 0)
    g_array_remove_range (device->scroll_valuators, 0,
                          device->scroll_valuators->len);
}

gint
_gdk_x11_device_xi2_get_id (GdkX11DeviceXI2 *device)
{
  g_return_val_if_fail (GDK_IS_X11_DEVICE_XI2 (device), 0);

  return device->device_id;
}

gdouble
gdk_x11_device_xi2_get_last_axis_value (GdkX11DeviceXI2 *device,
                                        gint             n_axis)
{
  if (n_axis >= gdk_device_get_n_axes (GDK_DEVICE (device)))
    return 0;

  if (!device->last_axes)
    return 0;

  return device->last_axes[n_axis];
}

void
gdk_x11_device_xi2_store_axes (GdkX11DeviceXI2 *device,
                               gdouble         *axes,
                               gint             n_axes)
{
  g_free (device->last_axes);

  if (axes && n_axes)
    device->last_axes = g_memdup (axes, sizeof (gdouble) * n_axes);
  else
    device->last_axes = NULL;
}
