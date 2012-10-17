/*
 * gdkdisplay-wayland.h
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_WAYLAND_DISPLAY__
#define __GDK_WAYLAND_DISPLAY__

#include <config.h>
#include <stdint.h>
#include <wayland-client.h>
#include <wayland-cursor.h>

#ifdef GDK_WAYLAND_USE_EGL
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <cairo-gl.h>
#endif

#include <glib.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdk.h>		/* For gdk_get_program_class() */

#include "gdkdisplayprivate.h"

G_BEGIN_DECLS

typedef struct _GdkWaylandDisplay GdkWaylandDisplay;
typedef struct _GdkWaylandDisplayClass GdkWaylandDisplayClass;

#define GDK_TYPE_WAYLAND_DISPLAY              (_gdk_wayland_display_get_type())
#define GDK_WAYLAND_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_DISPLAY, GdkWaylandDisplay))
#define GDK_WAYLAND_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_DISPLAY, GdkWaylandDisplayClass))
#define GDK_IS_WAYLAND_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DISPLAY))
#define GDK_IS_WAYLAND_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_DISPLAY))
#define GDK_WAYLAND_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_DISPLAY, GdkWaylandDisplayClass))

struct _GdkWaylandDisplay
{
  GdkDisplay parent_instance;
  GdkScreen *screen;

  /* Keyboard related information */
  GdkKeymap *keymap;

  /* input GdkDevice list */
  GList *input_devices;

  /* Startup notification */
  gchar *startup_notification_id;

  /* Time of most recent user interaction and most recent serial */
  gulong user_time;
  guint32 serial;

  /* Wayland fields below */
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct wl_shell *shell;
  struct wl_output *output;
  struct wl_input_device *input_device;
  struct wl_data_device_manager *data_device_manager;

  struct wl_cursor_theme *cursor_theme;

  GSource *event_source;

  struct xkb_context *xkb_context;

#ifdef GDK_WAYLAND_USE_EGL
  EGLDisplay egl_display;
  EGLContext egl_context;
  cairo_device_t *cairo_device;
#endif

#ifdef GDK_WAYLAND_USE_EGL
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
  PFNEGLCREATEIMAGEKHRPROC create_image;
  PFNEGLDESTROYIMAGEKHRPROC destroy_image;
#endif
};

struct _GdkWaylandDisplayClass
{
  GdkDisplayClass parent_class;
};

GType      _gdk_wayland_display_get_type            (void);

G_END_DECLS

#endif				/* __GDK_WAYLAND_DISPLAY__ */
