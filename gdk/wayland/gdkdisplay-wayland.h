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
  GdkScreen *default_screen;
  GdkScreen **screens;

  gint grab_count;

  /* Keyboard related information */

  gint xkb_event_type;
  gboolean use_xkb;
  
  /* Whether we were able to turn on detectable-autorepeat using
   * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
   * to checking the next event with XPending(). */
  gboolean have_xkb_autorepeat;

  GdkKeymap *keymap;
  guint	    keymap_serial;

  gboolean have_xfixes;
  gint xfixes_event_base;

  gboolean have_xcomposite;
  gboolean have_xdamage;
  gint xdamage_event_base;

  gboolean have_randr13;
  gint xrandr_event_base;

  /* If the SECURITY extension is in place, whether this client holds 
   * a trusted authorization and so is allowed to make various requests 
   * (grabs, properties etc.) Otherwise always TRUE. */
  gboolean trusted_client;

  /* drag and drop information */
  GdkDragContext *current_dest_drag;

  /* data needed for MOTIF DnD */

  Window motif_drag_window;
  GdkWindow *motif_drag_gdk_window;
  GList **motif_target_lists;
  gint motif_n_target_lists;

  /* Mapping to/from virtual atoms */

  GHashTable *atom_from_virtual;
  GHashTable *atom_to_virtual;

  /* list of filters for client messages */
  GList *client_filters;

  /* List of functions to go from extension event => X window */
  GSList *event_types;
  
  /* X ID hashtable */
  GHashTable *xid_ht;

  /* translation queue */
  GQueue *translate_queue;

  /* Input device */
  /* input GdkDevice list */
  GList *input_devices;

  /* input GdkWindow list */
  GList *input_windows;

  /* Startup notification */
  gchar *startup_notification_id;

  /* Time of most recent user interaction. */
  gulong user_time;

  /* Sets of atoms for DND */
  guint base_dnd_atoms_precached : 1;
  guint xdnd_atoms_precached : 1;
  guint motif_atoms_precached : 1;
  guint use_sync : 1;

  guint have_shapes : 1;
  guint have_input_shapes : 1;
  gint shape_event_base;

  /* The offscreen window that has the pointer in it (if any) */
  GdkWindow *active_offscreen_window;

  /* Wayland fields below */
  struct wl_display *wl_display;
  struct wl_egl_display *native_display;
  struct wl_compositor *compositor;
  struct wl_shell *shell;
  struct wl_output *output;
  struct wl_input_device *input_device;
  GSource *event_source;
  EGLDisplay egl_display;
  EGLContext egl_context;
  cairo_device_t *cairo_device;

  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
  PFNEGLCREATEIMAGEKHRPROC create_image;
  PFNEGLDESTROYIMAGEKHRPROC destroy_image;
};

struct _GdkDisplayWaylandClass
{
  GdkDisplayClass parent_class;
};

GType      _gdk_display_wayland_get_type            (void);
GdkScreen *_gdk_wayland_display_screen_for_xrootwin (GdkDisplay *display,
						     Window      xrootwin);

G_END_DECLS

#endif				/* __GDK_DISPLAY_WAYLAND__ */
