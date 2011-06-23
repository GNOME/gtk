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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_DISPLAY_WAYLAND__
#define __GDK_DISPLAY_WAYLAND__

#include <stdint.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <cairo-gl.h>
#include <glib.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdk.h>		/* For gdk_get_program_class() */

#include "gdkdisplayprivate.h"

G_BEGIN_DECLS

typedef struct _GdkDisplayWayland GdkDisplayWayland;
typedef struct _GdkDisplayWaylandClass GdkDisplayWaylandClass;

#define GDK_TYPE_DISPLAY_WAYLAND              (_gdk_display_wayland_get_type())
#define GDK_DISPLAY_WAYLAND(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_WAYLAND, GdkDisplayWayland))
#define GDK_DISPLAY_WAYLAND_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_WAYLAND, GdkDisplayWaylandClass))
#define GDK_IS_DISPLAY_WAYLAND(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_WAYLAND))
#define GDK_IS_DISPLAY_WAYLAND_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_WAYLAND))
#define GDK_DISPLAY_WAYLAND_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_WAYLAND, GdkDisplayWaylandClass))

struct _GdkDisplayWayland
{
  GdkDisplay parent_instance;
  GdkScreen *screen;

  /* Keyboard related information */
  GdkKeymap *keymap;

  /* input GdkDevice list */
  GList *input_devices;

  /* Startup notification */
  gchar *startup_notification_id;

  /* Time of most recent user interaction. */
  gulong user_time;

  /* Wayland fields below */
  struct wl_display *wl_display;
  struct wl_visual *argb_visual, *premultiplied_argb_visual, *rgb_visual;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct wl_shell *shell;
  struct wl_output *output;
  struct wl_input_device *input_device;
  GSource *event_source;
  EGLDisplay egl_display;
  EGLContext egl_context;
  cairo_device_t *cairo_device;

  GdkCursor **cursors;

  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
  PFNEGLCREATEIMAGEKHRPROC create_image;
  PFNEGLDESTROYIMAGEKHRPROC destroy_image;
};

struct _GdkDisplayWaylandClass
{
  GdkDisplayClass parent_class;
};

GType      _gdk_display_wayland_get_type            (void);

G_END_DECLS

#endif				/* __GDK_DISPLAY_WAYLAND__ */
