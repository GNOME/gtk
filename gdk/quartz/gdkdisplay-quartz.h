/*
 * gdkdisplay-quartz.h
 *
 * Copyright 2017 Tom Schoonjans 
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

#ifndef __GDK_QUARTZ_DISPLAY__
#define __GDK_QUARTZ_DISPLAY__

#include "gdkdisplayprivate.h"
#include "gdkkeys.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS


struct _GdkQuartzDisplay
{
  GdkDisplay parent_instance;
  /*Display *xdisplay;
  GdkScreen *screen;
  GList *screens;

  GSource *event_source;

  gint grab_count;
  */
  /* Visual infos for creating Windows */
  /*int window_depth;
  Visual *window_visual;
  Colormap window_colormap;
  */
  /* Keyboard related information */
  /*gint xkb_event_type;
  gboolean use_xkb;
  */
  /* Whether we were able to turn on detectable-autorepeat using
   * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
   * to checking the next event with XPending().
   */
  /*gboolean have_xkb_autorepeat;

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
  */
  /* If the SECURITY extension is in place, whether this client holds
   * a trusted authorization and so is allowed to make various requests
   * (grabs, properties etc.) Otherwise always TRUE.
   */
  /*gboolean trusted_client;
  */
  /* drag and drop information */
  /*GdkDragContext *current_dest_drag;
  */
  /* Mapping to/from virtual atoms */
  /*GHashTable *atom_from_virtual;
  GHashTable *atom_to_virtual;
  */
  /* Session Management leader window see ICCCM */
  /*Window leader_window;
  GdkWindow *leader_gdk_window;
  gboolean leader_window_title_set;
  */
  /* List of functions to go from extension event => X window */
  /*GSList *event_types;
  */
  /* X ID hashtable */
  /*GHashTable *xid_ht;
  */
  /* translation queue */
  /*GQueue *translate_queue;
  */
  /* input GdkWindow list */
  /*GList *input_windows;
  */
  GPtrArray *monitors;

  /* Startup notification */
  /*gchar *startup_notification_id;
  */
  /* Time of most recent user interaction. */
  /*gulong user_time;
  */
  /* Sets of atoms for DND */
  /*guint base_dnd_atoms_precached : 1;
  guint xdnd_atoms_precached : 1;
  guint motif_atoms_precached : 1;
  guint use_sync : 1;

  guint have_shapes : 1;
  guint have_input_shapes : 1;
  gint shape_event_base;

  GSList *error_traps;

  gint wm_moveresize_button;
  */
  /* GLX information */
  /*gint glx_version;
  gint glx_error_base;
  gint glx_event_base;
  */
  /* Translation between X server time and system-local monotonic time */
  /*gint64 server_time_query_time;
  gint64 server_time_offset;
  */
  /*guint server_time_is_monotonic_time : 1;

  guint have_glx : 1;
  */
  /* GLX extensions we check */
  /*guint has_glx_swap_interval : 1;
  guint has_glx_create_context : 1;
  guint has_glx_texture_from_pixmap : 1;
  guint has_glx_video_sync : 1;
  guint has_glx_buffer_age : 1;
  guint has_glx_sync_control : 1;
  guint has_glx_multisample : 1;
  guint has_glx_visual_rating : 1;
  guint has_glx_create_es2_context : 1;*/
};

struct _GdkQuartzDisplayClass
{
  GdkDisplayClass parent_class;
};

/* Display methods - events */
void     _gdk_quartz_display_queue_events (GdkDisplay *display);
gboolean _gdk_quartz_display_has_pending  (GdkDisplay *display);

void       _gdk_quartz_display_event_data_copy (GdkDisplay     *display,
                                                const GdkEvent *src,
                                                GdkEvent       *dst);
void       _gdk_quartz_display_event_data_free (GdkDisplay     *display,
                                                GdkEvent       *event);

/* Display methods - cursor */
GdkCursor *_gdk_quartz_display_get_cursor_for_type     (GdkDisplay      *display,
                                                        GdkCursorType    type);
GdkCursor *_gdk_quartz_display_get_cursor_for_name     (GdkDisplay      *display,
                                                        const gchar     *name);
GdkCursor *_gdk_quartz_display_get_cursor_for_surface  (GdkDisplay      *display,
                                                        cairo_surface_t *surface,
                                                        gdouble          x,
                                                        gdouble          y);
gboolean   _gdk_quartz_display_supports_cursor_alpha   (GdkDisplay    *display);
gboolean   _gdk_quartz_display_supports_cursor_color   (GdkDisplay    *display);
void       _gdk_quartz_display_get_default_cursor_size (GdkDisplay *display,
                                                        guint      *width,
                                                        guint      *height);
void       _gdk_quartz_display_get_maximal_cursor_size (GdkDisplay *display,
                                                        guint      *width,
                                                        guint      *height);

/* Display methods - window */
void       _gdk_quartz_display_before_process_all_updates (GdkDisplay *display);
void       _gdk_quartz_display_after_process_all_updates  (GdkDisplay *display);
void       _gdk_quartz_display_create_window_impl (GdkDisplay    *display,
                                                   GdkWindow     *window,
                                                   GdkWindow     *real_parent,
                                                   GdkScreen     *screen,
                                                   GdkEventMask   event_mask,
                                                   GdkWindowAttr *attributes);

/* Display methods - keymap */
GdkKeymap * _gdk_quartz_display_get_keymap (GdkDisplay *display);

/* Display methods - selection */
gboolean    _gdk_quartz_display_set_selection_owner (GdkDisplay *display,
                                                     GdkWindow  *owner,
                                                     GdkAtom     selection,
                                                     guint32     time,
                                                     gboolean    send_event);
GdkWindow * _gdk_quartz_display_get_selection_owner (GdkDisplay *display,
                                                     GdkAtom     selection);
gint        _gdk_quartz_display_get_selection_property (GdkDisplay     *display,
                                                        GdkWindow      *requestor,
                                                        guchar        **data,
                                                        GdkAtom        *ret_type,
                                                        gint           *ret_format);
void        _gdk_quartz_display_convert_selection      (GdkDisplay     *display,
                                                        GdkWindow      *requestor,
                                                        GdkAtom         selection,
                                                        GdkAtom         target,
                                                        guint32         time);
gint        _gdk_quartz_display_text_property_to_utf8_list (GdkDisplay     *display,
                                                            GdkAtom         encoding,
                                                            gint            format,
                                                            const guchar   *text,
                                                            gint            length,
                                                            gchar        ***list);
gchar *     _gdk_quartz_display_utf8_to_string_target      (GdkDisplay     *displayt,
                                                            const gchar    *str);
/*
GdkScreen *_gdk_x11_display_screen_for_xrootwin (GdkDisplay  *display,
                                                 Window       xrootwin);
void       _gdk_x11_display_error_event         (GdkDisplay  *display,
                                                 XErrorEvent *error);

GdkFilterReturn _gdk_wm_protocols_filter        (GdkXEvent   *xev,
                                                 GdkEvent    *event,
                                                 gpointer     data);
*/
G_END_DECLS

#endif  /* __GDK_QUARTZ_DISPLAY__ */

