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


#include "gdkx11device-core.h"
#include "gdkdeviceprivate.h"

#include "gdkinternals.h"
#include "gdksurface.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkasync.h"

#include <math.h>

struct _GdkX11DeviceCore
{
  GdkDevice parent_instance;
};

struct _GdkX11DeviceCoreClass
{
  GdkDeviceClass parent_class;
};

static gboolean gdk_x11_device_core_get_history (GdkDevice       *device,
                                                 GdkSurface       *surface,
                                                 guint32          start,
                                                 guint32          stop,
                                                 GdkTimeCoord  ***events,
                                                 gint            *n_events);
static void     gdk_x11_device_core_get_state   (GdkDevice       *device,
                                                 GdkSurface       *surface,
                                                 gdouble         *axes,
                                                 GdkModifierType *mask);
static void     gdk_x11_device_core_set_surface_cursor (GdkDevice *device,
                                                       GdkSurface *surface,
                                                       GdkCursor *cursor);
static void     gdk_x11_device_core_warp (GdkDevice *device,
                                          gdouble    x,
                                          gdouble    y);
static void gdk_x11_device_core_query_state (GdkDevice        *device,
                                             GdkSurface        *surface,
                                             GdkSurface       **child_surface,
                                             gdouble          *root_x,
                                             gdouble          *root_y,
                                             gdouble          *win_x,
                                             gdouble          *win_y,
                                             GdkModifierType  *mask);
static GdkGrabStatus gdk_x11_device_core_grab   (GdkDevice     *device,
                                                 GdkSurface     *surface,
                                                 gboolean       owner_events,
                                                 GdkEventMask   event_mask,
                                                 GdkSurface     *confine_to,
                                                 GdkCursor     *cursor,
                                                 guint32        time_);
static void          gdk_x11_device_core_ungrab (GdkDevice     *device,
                                                 guint32        time_);
static GdkSurface * gdk_x11_device_core_surface_at_position (GdkDevice       *device,
                                                           gdouble         *win_x,
                                                           gdouble         *win_y,
                                                           GdkModifierType *mask,
                                                           gboolean         get_toplevel);

G_DEFINE_TYPE (GdkX11DeviceCore, gdk_x11_device_core, GDK_TYPE_DEVICE)

static void
gdk_x11_device_core_class_init (GdkX11DeviceCoreClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_x11_device_core_get_history;
  device_class->get_state = gdk_x11_device_core_get_state;
  device_class->set_surface_cursor = gdk_x11_device_core_set_surface_cursor;
  device_class->warp = gdk_x11_device_core_warp;
  device_class->query_state = gdk_x11_device_core_query_state;
  device_class->grab = gdk_x11_device_core_grab;
  device_class->ungrab = gdk_x11_device_core_ungrab;
  device_class->surface_at_position = gdk_x11_device_core_surface_at_position;
}

static void
gdk_x11_device_core_init (GdkX11DeviceCore *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, NULL, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, NULL, GDK_AXIS_Y, 0, 0, 1);
}

static gboolean
impl_coord_in_surface (GdkSurface *surface,
		      int        impl_x,
		      int        impl_y)
{
  if (impl_x < surface->abs_x ||
      impl_x >= surface->abs_x + surface->width)
    return FALSE;

  if (impl_y < surface->abs_y ||
      impl_y >= surface->abs_y + surface->height)
    return FALSE;

  return TRUE;
}

static gboolean
gdk_x11_device_core_get_history (GdkDevice      *device,
                                 GdkSurface      *surface,
                                 guint32         start,
                                 guint32         stop,
                                 GdkTimeCoord ***events,
                                 gint           *n_events)
{
  XTimeCoord *xcoords;
  GdkTimeCoord **coords;
  GdkSurface *impl_surface;
  GdkSurfaceImplX11 *impl;
  int tmp_n_events;
  int i, j;

  impl_surface = _gdk_surface_get_impl_surface (surface);
  impl =  GDK_SURFACE_IMPL_X11 (impl_surface->impl);
  xcoords = XGetMotionEvents (GDK_SURFACE_XDISPLAY (surface),
                              GDK_SURFACE_XID (impl_surface),
                              start, stop, &tmp_n_events);
  if (!xcoords)
    return FALSE;

  coords = _gdk_device_allocate_history (device, tmp_n_events);

  for (i = 0, j = 0; i < tmp_n_events; i++)
    {
      if (impl_coord_in_surface (surface,
                                xcoords[i].x / impl->surface_scale,
                                xcoords[i].y / impl->surface_scale))
        {
          coords[j]->time = xcoords[i].time;
          coords[j]->axes[0] = (double)xcoords[i].x / impl->surface_scale - surface->abs_x;
          coords[j]->axes[1] = (double)xcoords[i].y / impl->surface_scale - surface->abs_y;
          j++;
        }
    }

  XFree (xcoords);

  /* free the events we allocated too much */
  for (i = j; i < tmp_n_events; i++)
    {
      g_free (coords[i]);
      coords[i] = NULL;
    }

  tmp_n_events = j;

  if (tmp_n_events == 0)
    {
      gdk_device_free_history (coords, tmp_n_events);
      return FALSE;
    }

  if (n_events)
    *n_events = tmp_n_events;

  if (events)
    *events = coords;
  else if (coords)
    gdk_device_free_history (coords, tmp_n_events);

  return TRUE;
}

static void
gdk_x11_device_core_get_state (GdkDevice       *device,
                               GdkSurface       *surface,
                               gdouble         *axes,
                               GdkModifierType *mask)
{
  gdouble x, y;

  gdk_surface_get_device_position_double (surface, device, &x, &y, mask);

  if (axes)
    {
      axes[0] = x;
      axes[1] = y;
    }
}

static void
gdk_x11_device_core_set_surface_cursor (GdkDevice *device,
                                       GdkSurface *surface,
                                       GdkCursor *cursor)
{
  GdkDisplay *display = gdk_device_get_display (device);
  Cursor xcursor;

  if (!cursor)
    xcursor = None;
  else
    xcursor = gdk_x11_display_get_xcursor (display, cursor);

  XDefineCursor (GDK_DISPLAY_XDISPLAY (display),
                 GDK_SURFACE_XID (surface),
                 xcursor);
}

static void
gdk_x11_device_core_warp (GdkDevice *device,
                          gdouble    x,
                          gdouble    y)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window dest;
  GdkX11Screen *screen;

  display = gdk_device_get_display (device);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  screen = GDK_X11_DISPLAY (display)->screen;
  dest = GDK_SCREEN_XROOTWIN (screen);

  XWarpPointer (xdisplay, None, dest, 0, 0, 0, 0,
                round (x * screen->surface_scale),
                round (y * screen->surface_scale));
}

static void
gdk_x11_device_core_query_state (GdkDevice        *device,
                                 GdkSurface        *surface,
                                 GdkSurface       **child_surface,
                                 gdouble          *root_x,
                                 gdouble          *root_y,
                                 gdouble          *win_x,
                                 gdouble          *win_y,
                                 GdkModifierType  *mask)
{
  GdkDisplay *display;
  GdkX11Screen *screen;
  Window xwindow, w;
  Window xroot_window, xchild_window;
  int xroot_x, xroot_y, xwin_x, xwin_y;
  unsigned int xmask;
  int scale;

  display = gdk_device_get_display (device);
  screen = GDK_X11_DISPLAY (display)->screen;
  if (surface == NULL)
    {
      xwindow = GDK_SCREEN_XROOTWIN (screen);
      scale = screen->surface_scale;
    }
  else
    {
      xwindow = GDK_SURFACE_XID (surface);
      scale = GDK_SURFACE_IMPL_X11 (surface->impl)->surface_scale;
    }

  if (!GDK_X11_DISPLAY (display)->trusted_client ||
      !XQueryPointer (GDK_SURFACE_XDISPLAY (surface),
                      xwindow,
                      &xroot_window,
                      &xchild_window,
                      &xroot_x, &xroot_y,
                      &xwin_x, &xwin_y,
                      &xmask))
    {
      XSetWindowAttributes attributes;
      Display *xdisplay;

      /* FIXME: untrusted clients not multidevice-safe */
      xdisplay = GDK_SCREEN_XDISPLAY (screen);
      xwindow = GDK_SCREEN_XROOTWIN (screen);

      w = XCreateWindow (xdisplay, xwindow, 0, 0, 1, 1, 0,
                         CopyFromParent, InputOnly, CopyFromParent,
                         0, &attributes);
      XQueryPointer (xdisplay, w,
                     &xroot_window,
                     &xchild_window,
                     &xroot_x, &xroot_y,
                     &xwin_x, &xwin_y,
                     &xmask);
      XDestroyWindow (xdisplay, w);
    }

  if (child_surface)
    *child_surface = gdk_x11_surface_lookup_for_display (display, xchild_window);

  if (root_x)
    *root_x = (double)xroot_x / scale;

  if (root_y)
    *root_y = (double)xroot_y / scale;

  if (win_x)
    *win_x = (double)xwin_x / scale;

  if (win_y)
    *win_y = (double)xwin_y / scale;

  if (mask)
    *mask = xmask;
}

static GdkGrabStatus
gdk_x11_device_core_grab (GdkDevice    *device,
                          GdkSurface    *surface,
                          gboolean      owner_events,
                          GdkEventMask  event_mask,
                          GdkSurface    *confine_to,
                          GdkCursor    *cursor,
                          guint32       time_)
{
  GdkDisplay *display;
  Window xwindow, xconfine_to;
  gint status;

  display = gdk_device_get_display (device);

  xwindow = GDK_SURFACE_XID (surface);

  if (confine_to)
    confine_to = _gdk_surface_get_impl_surface (confine_to);

  if (!confine_to || GDK_SURFACE_DESTROYED (confine_to))
    xconfine_to = None;
  else
    xconfine_to = GDK_SURFACE_XID (confine_to);

#ifdef G_ENABLE_DEBUG
  if (GDK_DISPLAY_DEBUG_CHECK (display, NOGRABS))
    status = GrabSuccess;
  else
#endif
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      status = XGrabKeyboard (GDK_DISPLAY_XDISPLAY (display),
                              xwindow,
                              owner_events,
                              GrabModeAsync, GrabModeAsync,
                              time_);
    }
  else
    {
      Cursor xcursor;
      guint xevent_mask;
      gint i;

      /* Device is a pointer */
      if (!cursor)
        xcursor = None;
      else
        {
          xcursor = gdk_x11_display_get_xcursor (display, cursor);
        }

      xevent_mask = 0;

      for (i = 0; i < _gdk_x11_event_mask_table_size; i++)
        {
          if (event_mask & (1 << (i + 1)))
            xevent_mask |= _gdk_x11_event_mask_table[i];
        }

      /* We don't want to set a native motion hint mask, as we're emulating motion
       * hints. If we set a native one we just wouldn't get any events.
       */
      xevent_mask &= ~PointerMotionHintMask;

      status = XGrabPointer (GDK_DISPLAY_XDISPLAY (display),
                             xwindow,
                             owner_events,
                             xevent_mask,
                             GrabModeAsync, GrabModeAsync,
                             xconfine_to,
                             xcursor,
                             time_);
    }

  _gdk_x11_display_update_grab_info (display, device, status);

  return _gdk_x11_convert_grab_status (status);
}

static void
gdk_x11_device_core_ungrab (GdkDevice *device,
                            guint32    time_)
{
  GdkDisplay *display;
  gulong serial;

  display = gdk_device_get_display (device);
  serial = NextRequest (GDK_DISPLAY_XDISPLAY (display));

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time_);
  else
    XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time_);

  _gdk_x11_display_update_grab_info_ungrab (display, device, time_, serial);
}

static GdkSurface *
gdk_x11_device_core_surface_at_position (GdkDevice       *device,
                                        gdouble         *win_x,
                                        gdouble         *win_y,
                                        GdkModifierType *mask,
                                        gboolean         get_toplevel)
{
  GdkSurfaceImplX11 *impl;
  GdkDisplay *display;
  Display *xdisplay;
  GdkSurface *surface;
  GdkX11Screen *screen;
  Window xwindow, root, child, last;
  int xroot_x, xroot_y, xwin_x, xwin_y;
  unsigned int xmask;

  last = None;
  display = gdk_device_get_display (device);
  screen = GDK_X11_DISPLAY (display)->screen;

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
      XQueryPointer (xdisplay, xwindow,
                     &root, &child,
                     &xroot_x, &xroot_y,
                     &xwin_x, &xwin_y,
                     &xmask);

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
      int rootx = -1, rooty = -1;
      int winx, winy;

      /* FIXME: untrusted clients case not multidevice-safe */
      pointer_window = None;
      toplevels = gdk_x11_display_get_toplevel_windows (display);
      for (list = toplevels; list != NULL; list = list->next)
        {
          surface = GDK_SURFACE (list->data);
          impl = GDK_SURFACE_IMPL_X11 (surface->impl);
          xwindow = GDK_SURFACE_XID (surface);
          gdk_x11_display_error_trap_push (display);
          XQueryPointer (xdisplay, xwindow,
                         &root, &child,
                         &rootx, &rooty,
                         &winx, &winy,
                         &xmask);
          if (gdk_x11_display_error_trap_pop (display))
            continue;
          if (child != None)
            {
              pointer_window = child;
              break;
            }
          gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);
          if (winx >= 0 && winy >= 0 && winx < width * impl->surface_scale && winy < height * impl->surface_scale)
            {
              /* A childless toplevel, or below another window? */
              XSetWindowAttributes attributes;
              Window w;

              w = XCreateWindow (xdisplay, xwindow, winx, winy, 1, 1, 0,
                                 CopyFromParent, InputOnly, CopyFromParent,
                                 0, &attributes);
              XMapWindow (xdisplay, w);
              XQueryPointer (xdisplay, xwindow,
                             &root, &child,
                             &rootx, &rooty,
                             &winx, &winy,
                             &xmask);
              XDestroyWindow (xdisplay, w);
              if (child == w)
                {
                  pointer_window = xwindow;
                  break;
                }
            }
        }

      xwindow = pointer_window;
    }

  while (xwindow)
    {
      last = xwindow;
      gdk_x11_display_error_trap_push (display);
      XQueryPointer (xdisplay, xwindow,
                     &root, &xwindow,
                     &xroot_x, &xroot_y,
                     &xwin_x, &xwin_y,
                     &xmask);
      if (gdk_x11_display_error_trap_pop (display))
        break;

      if (get_toplevel && last != root &&
          (surface = gdk_x11_surface_lookup_for_display (display, last)) != NULL &&
          surface->surface_type != GDK_SURFACE_FOREIGN)
        {
          xwindow = last;
          break;
        }
    }

  gdk_x11_display_ungrab (display);

  surface = gdk_x11_surface_lookup_for_display (display, last);
  impl = NULL;
  if (surface)
    impl = GDK_SURFACE_IMPL_X11 (surface->impl);

  if (win_x)
    *win_x = (surface) ? (double)xwin_x / impl->surface_scale : -1;

  if (win_y)
    *win_y = (surface) ? (double)xwin_y / impl->surface_scale : -1;

  if (mask)
    *mask = xmask;

  return surface;
}

