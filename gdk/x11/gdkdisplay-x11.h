/*
 * gdkdisplay-x11.h
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

#ifndef __GDK_X11_DISPLAY__
#define __GDK_X11_DISPLAY__

#include "gdkdisplayprivate.h"
#include "gdkkeys.h"
#include "gdksurface.h"
#include "gdkinternals.h"
#include "gdkx11devicemanager.h"
#include "gdkx11display.h"
#include "gdkx11screen.h"
#include "gdkprivate-x11.h"

#include <X11/X.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS


struct _GdkX11Display
{
  GdkDisplay parent_instance;
  Display *xdisplay;
  GdkX11Screen *screen;
  GList *screens;
  GList *toplevels;
  GdkX11DeviceManagerXI2 *device_manager;

  GSource *event_source;

  gint grab_count;

  /* Visual infos for creating Windows */
  int window_depth;
  Visual *window_visual;
  Colormap window_colormap;

  /* Keyboard related information */
  gint xkb_event_type;
  gboolean use_xkb;

  /* Whether we were able to turn on detectable-autorepeat using
   * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
   * to checking the next event with XPending().
   */
  gboolean have_xkb_autorepeat;

  GdkKeymap *keymap;
  guint      keymap_serial;

  gboolean have_xfixes;
  gint xfixes_event_base;

  gboolean have_xcomposite;
  gboolean have_xdamage;
  gint xdamage_event_base;

  gboolean have_randr12;
  gboolean have_randr13;
  gboolean have_randr15;
  gint xrandr_event_base;

  /* If the SECURITY extension is in place, whether this client holds
   * a trusted authorization and so is allowed to make various requests
   * (grabs, properties etc.) Otherwise always TRUE.
   */
  gboolean trusted_client;

  /* drag and drop information */
  GdkDrop *current_drop;

  /* Mapping to/from virtual atoms */
  GHashTable *atom_from_virtual;
  GHashTable *atom_to_virtual;

  /* Session Management leader window see ICCCM */
  char *program_class;
  Window leader_window;
  GdkSurface *leader_gdk_surface;
  gboolean leader_window_title_set;

  /* List of functions to go from extension event => X window */
  GSList *event_types;

  /* X ID hashtable */
  GHashTable *xid_ht;

  /* streams reading selections */
  GSList *streams;

  /* input GdkSurface list */
  GList *input_surfaces;

  /* GdkCursor => XCursor */
  GHashTable *cursors;

  GPtrArray *monitors;
  int primary_monitor;

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

  GSList *error_traps;

  gint wm_moveresize_button;

  /* GLX information */
  gint glx_version;
  gint glx_error_base;
  gint glx_event_base;

  /* Translation between X server time and system-local monotonic time */
  gint64 server_time_query_time;
  gint64 server_time_offset;

  guint server_time_is_monotonic_time : 1;

  guint have_glx : 1;

  /* GLX extensions we check */
  guint has_glx_swap_interval : 1;
  guint has_glx_create_context : 1;
  guint has_glx_texture_from_pixmap : 1;
  guint has_glx_video_sync : 1;
  guint has_glx_buffer_age : 1;
  guint has_glx_sync_control : 1;
  guint has_glx_multisample : 1;
  guint has_glx_visual_rating : 1;
  guint has_glx_create_es2_context : 1;
};

struct _GdkX11DisplayClass
{
  GdkDisplayClass parent_class;

  gboolean              (* xevent)                              (GdkX11Display          *display,
                                                                 const XEvent           *event);
};

GdkX11Screen *  _gdk_x11_display_screen_for_xrootwin            (GdkDisplay             *display,
                                                                 Window                  xrootwin);
void            _gdk_x11_display_error_event                    (GdkDisplay             *display,
                                                                 XErrorEvent            *error);
gsize           gdk_x11_display_get_max_request_size            (GdkDisplay             *display);
gboolean        gdk_x11_display_request_selection_notification  (GdkDisplay             *display,
                                                                 const char             *selection);

GdkFilterReturn _gdk_wm_protocols_filter        (const XEvent   *xevent,
                                                 GdkEvent       *event,
                                                 gpointer        data);

G_END_DECLS

#endif  /* __GDK_X11_DISPLAY__ */
