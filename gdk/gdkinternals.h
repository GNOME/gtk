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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* Uninstalled header defining types and functions internal to GDK */

#ifndef __GDK_INTERNALS_H__
#define __GDK_INTERNALS_H__

#include <gio/gio.h>
#include "gdkwindowimpl.h"
#include "gdkdisplay.h"
#include "gdkprivate.h"

G_BEGIN_DECLS

/**********************
 * General Facilities * 
 **********************/

/* Debugging support */

typedef struct _GdkColorInfo           GdkColorInfo;
typedef struct _GdkEventFilter         GdkEventFilter;
typedef struct _GdkClientFilter        GdkClientFilter;

typedef enum {
  GDK_COLOR_WRITEABLE = 1 << 0
} GdkColorInfoFlags;

struct _GdkColorInfo
{
  GdkColorInfoFlags flags;
  guint ref_count;
};

typedef enum {
  GDK_EVENT_FILTER_REMOVED = 1 << 0
} GdkEventFilterFlags;

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
  GdkEventFilterFlags flags;
  guint ref_count;
};

struct _GdkClientFilter {
  GdkAtom       type;
  GdkFilterFunc function;
  gpointer      data;
};

typedef enum {
  GDK_DEBUG_MISC          = 1 <<  0,
  GDK_DEBUG_EVENTS        = 1 <<  1,
  GDK_DEBUG_DND           = 1 <<  2,
  GDK_DEBUG_XIM           = 1 <<  3,
  GDK_DEBUG_NOGRABS       = 1 <<  4,
  GDK_DEBUG_INPUT         = 1 <<  5,
  GDK_DEBUG_CURSOR        = 1 <<  6,
  GDK_DEBUG_MULTIHEAD     = 1 <<  7,
  GDK_DEBUG_XINERAMA      = 1 <<  8,
  GDK_DEBUG_DRAW          = 1 <<  9,
  GDK_DEBUG_EVENTLOOP     = 1 << 10,
  GDK_DEBUG_FRAMES        = 1 << 11,
  GDK_DEBUG_SETTINGS      = 1 << 12,
  GDK_DEBUG_OPENGL        = 1 << 13
} GdkDebugFlag;

typedef enum {
  GDK_RENDERING_MODE_SIMILAR = 0,
  GDK_RENDERING_MODE_IMAGE,
  GDK_RENDERING_MODE_RECORDING
} GdkRenderingMode;

extern GList            *_gdk_default_filters;
extern GdkWindow        *_gdk_parent_root;

extern guint _gdk_debug_flags;
extern GdkRenderingMode    _gdk_rendering_mode;

#ifdef G_ENABLE_DEBUG

#define GDK_NOTE(type,action)                G_STMT_START { \
    if (_gdk_debug_flags & GDK_DEBUG_##type)                \
       { action; };                          } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GDK_NOTE(type,action)

#endif /* G_ENABLE_DEBUG */

/* Arg parsing */

typedef enum 
{
  GDK_ARG_STRING,
  GDK_ARG_INT,
  GDK_ARG_BOOL,
  GDK_ARG_NOBOOL,
  GDK_ARG_CALLBACK
} GdkArgType;

typedef struct _GdkArgContext GdkArgContext;
typedef struct _GdkArgDesc GdkArgDesc;

typedef void (*GdkArgFunc) (const char *name, const char *arg, gpointer data);

struct _GdkArgContext
{
  GPtrArray *tables;
  gpointer cb_data;
};

struct _GdkArgDesc
{
  const char *name;
  GdkArgType type;
  gpointer location;
  GdkArgFunc callback;
};

/* Event handling */

typedef struct _GdkEventPrivate GdkEventPrivate;

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0,

  /* The following flag is set for:
   * 1) touch events emulating pointer events
   * 2) pointer events being emulated by a touch sequence.
   */
  GDK_EVENT_POINTER_EMULATED = 1 << 1,

  /* When we are ready to draw a frame, we pause event delivery,
   * mark all events in the queue with this flag, and deliver
   * only those events until we finish the frame.
   */
  GDK_EVENT_FLUSHED = 1 << 2
} GdkEventFlags;

struct _GdkEventPrivate
{
  GdkEvent   event;
  guint      flags;
  GdkScreen *screen;
  gpointer   windowing_data;
  GdkDevice *device;
  GdkDevice *source_device;
};

typedef struct _GdkWindowPaint GdkWindowPaint;

struct _GdkWindow
{
  GObject parent_instance;

  GdkWindowImpl *impl; /* window-system-specific delegate object */

  GdkWindow *parent;
  GdkVisual *visual;

  gpointer user_data;

  gint x;
  gint y;

  GdkEventMask event_mask;
  guint8 window_type;

  guint8 depth;
  guint8 resize_count;

  gint8 toplevel_window_type;

  GList *filters;
  GList *children;
  GList *native_children;

  cairo_pattern_t *background;

  struct {
    cairo_region_t *region;
    cairo_surface_t *surface;
    gboolean surface_needs_composite;
  } current_paint;

  cairo_region_t *update_area;
  guint update_freeze_count;

  GdkWindowState state;

  guint8 alpha;
  guint8 fullscreen_mode;

  guint guffaw_gravity : 1;
  guint input_only : 1;
  guint modal_hint : 1;
  guint composited : 1;
  guint has_alpha_background : 1;

  guint destroyed : 2;

  guint accept_focus : 1;
  guint focus_on_map : 1;
  guint shaped : 1;
  guint support_multidevice : 1;
  guint synthesize_crossing_event_queued : 1;
  guint effective_visibility : 2;
  guint visibility : 2; /* The visibility wrt the toplevel (i.e. based on clip_region) */
  guint native_visibility : 2; /* the native visibility of a impl windows */
  guint viewable : 1; /* mapped and all parents mapped */
  guint applied_shape : 1;
  guint in_update : 1;
  guint geometry_dirty : 1;
  guint event_compression : 1;

  /* The GdkWindow that has the impl, ref:ed if another window.
   * This ref is required to keep the wrapper of the impl window alive
   * for as long as any GdkWindow references the impl. */
  GdkWindow *impl_window;

  guint update_and_descendants_freeze_count;

  gint abs_x, abs_y; /* Absolute offset in impl */
  gint width, height;

  guint num_offscreen_children;

  /* The clip region is the part of the window, in window coordinates
     that is fully or partially (i.e. semi transparently) visible in
     the window hierarchy from the toplevel and down */
  cairo_region_t *clip_region;
  /* This is the clip region, with additionally all the opaque
     child windows removed */
  cairo_region_t *clip_region_with_children;
  /* The layered region is the subset of clip_region that
     is covered by non-opaque sibling or ancestor sibling window. */
  cairo_region_t *layered_region;

  GdkCursor *cursor;
  GHashTable *device_cursor;

  cairo_region_t *shape;
  cairo_region_t *input_shape;

  GList *devices_inside;
  GHashTable *device_events;

  GHashTable *source_event_masks;
  gulong device_added_handler_id;
  gulong device_changed_handler_id;

  GdkFrameClock *frame_clock; /* NULL to use from parent or default */
  GdkWindowInvalidateHandlerFunc invalidate_handler;
};

#define GDK_WINDOW_TYPE(d) ((((GdkWindow *)(d)))->window_type)
#define GDK_WINDOW_DESTROYED(d) (((GdkWindow *)(d))->destroyed)

extern gchar     *_gdk_display_name;
extern gint       _gdk_screen_number;
extern gchar     *_gdk_display_arg_name;
extern gboolean   _gdk_disable_multidevice;

GdkEvent* _gdk_event_unqueue (GdkDisplay *display);

void _gdk_event_filter_unref        (GdkWindow      *window,
				     GdkEventFilter *filter);

void     _gdk_event_set_pointer_emulated (GdkEvent *event,
                                          gboolean  emulated);
gboolean _gdk_event_get_pointer_emulated (GdkEvent *event);

void   _gdk_event_emit               (GdkEvent   *event);
GList* _gdk_event_queue_find_first   (GdkDisplay *display);
void   _gdk_event_queue_remove_link  (GdkDisplay *display,
                                      GList      *node);
GList* _gdk_event_queue_prepend      (GdkDisplay *display,
                                      GdkEvent   *event);
GList* _gdk_event_queue_append       (GdkDisplay *display,
                                      GdkEvent   *event);
GList* _gdk_event_queue_insert_after (GdkDisplay *display,
                                      GdkEvent   *after_event,
                                      GdkEvent   *event);
GList* _gdk_event_queue_insert_before(GdkDisplay *display,
                                      GdkEvent   *after_event,
                                      GdkEvent   *event);

void    _gdk_event_queue_handle_motion_compression (GdkDisplay *display);
void    _gdk_event_queue_flush                     (GdkDisplay       *display);

void   _gdk_event_button_generate    (GdkDisplay *display,
                                      GdkEvent   *event);

void _gdk_windowing_event_data_copy (const GdkEvent *src,
                                     GdkEvent       *dst);
void _gdk_windowing_event_data_free (GdkEvent       *event);

void _gdk_set_window_state (GdkWindow *window,
                            GdkWindowState new_state);

gboolean _gdk_cairo_surface_extents (cairo_surface_t *surface,
                                     GdkRectangle *extents);

/*************************************
 * Interfaces used by windowing code *
 *************************************/

cairo_surface_t *
           _gdk_window_ref_cairo_surface (GdkWindow *window);

void       _gdk_window_destroy           (GdkWindow      *window,
                                          gboolean        foreign_destroy);
void       _gdk_window_clear_update_area (GdkWindow      *window);
void       _gdk_window_update_size       (GdkWindow      *window);
gboolean   _gdk_window_update_viewable   (GdkWindow      *window);

void       _gdk_window_process_updates_recurse (GdkWindow *window,
                                                cairo_region_t *expose_region);

void       _gdk_screen_set_resolution    (GdkScreen      *screen,
                                          gdouble         dpi);
void       _gdk_screen_close             (GdkScreen      *screen);

/*****************************************
 * Interfaces provided by windowing code *
 *****************************************/

/* Font/string functions implemented in module-specific code */

void _gdk_cursor_destroy (GdkCursor *cursor);

extern const GOptionEntry _gdk_windowing_args[];

void _gdk_windowing_got_event                (GdkDisplay       *display,
                                              GList            *event_link,
                                              GdkEvent         *event,
                                              gulong            serial);

#define GDK_WINDOW_IS_MAPPED(window) (((window)->state & GDK_WINDOW_STATE_WITHDRAWN) == 0)

void _gdk_window_invalidate_for_expose (GdkWindow       *window,
                                        cairo_region_t       *region);

GdkWindow * _gdk_window_find_child_at (GdkWindow *window,
                                       double x, double y);
GdkWindow * _gdk_window_find_descendant_at (GdkWindow *toplevel,
                                            double x, double y,
                                            double *found_x,
                                            double *found_y);

GdkEvent * _gdk_make_event (GdkWindow    *window,
                            GdkEventType  type,
                            GdkEvent     *event_in_queue,
                            gboolean      before_event);
gboolean _gdk_window_event_parent_of (GdkWindow *parent,
                                      GdkWindow *child);

void _gdk_synthesize_crossing_events (GdkDisplay                 *display,
                                      GdkWindow                  *src,
                                      GdkWindow                  *dest,
                                      GdkDevice                  *device,
                                      GdkDevice                  *source_device,
				      GdkCrossingMode             mode,
				      gdouble                     toplevel_x,
				      gdouble                     toplevel_y,
				      GdkModifierType             mask,
				      guint32                     time_,
				      GdkEvent                   *event_in_queue,
				      gulong                      serial,
				      gboolean                    non_linear);
void _gdk_display_set_window_under_pointer (GdkDisplay *display,
                                            GdkDevice  *device,
                                            GdkWindow  *window);


void _gdk_synthesize_crossing_events_for_geometry_change (GdkWindow *changed_window);

gboolean    _gdk_window_has_impl (GdkWindow *window);
GdkWindow * _gdk_window_get_impl_window (GdkWindow *window);

/*****************************
 * offscreen window routines *
 *****************************/
GType gdk_offscreen_window_get_type (void);
void       _gdk_offscreen_window_new                 (GdkWindow     *window,
                                                      GdkWindowAttr *attributes,
                                                      gint           attributes_mask);
cairo_surface_t * _gdk_offscreen_window_create_surface (GdkWindow *window,
                                                        gint       width,
                                                        gint       height);

G_END_DECLS

#endif /* __GDK_INTERNALS_H__ */
