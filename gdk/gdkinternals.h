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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdksurfaceimpl.h"
#include "gdkdisplay.h"
#include "gdkeventsprivate.h"
#include "gdkenumtypes.h"
#include "gdkdragprivate.h"

G_BEGIN_DECLS

/**********************
 * General Facilities * 
 **********************/

/* Debugging support */

typedef struct _GdkSurfaceAttr          GdkSurfaceAttr;

typedef enum {
  GDK_DEBUG_MISC            = 1 <<  0,
  GDK_DEBUG_EVENTS          = 1 <<  1,
  GDK_DEBUG_DND             = 1 <<  2,
  GDK_DEBUG_INPUT           = 1 <<  3,
  GDK_DEBUG_EVENTLOOP       = 1 <<  4,
  GDK_DEBUG_FRAMES          = 1 <<  5,
  GDK_DEBUG_SETTINGS        = 1 <<  6,
  GDK_DEBUG_OPENGL          = 1 <<  7,
  GDK_DEBUG_VULKAN          = 1 <<  8,
  GDK_DEBUG_SELECTION       = 1 <<  9,
  GDK_DEBUG_CLIPBOARD       = 1 << 10,
  /* flags below are influencing behavior */
  GDK_DEBUG_NOGRABS         = 1 << 11,
  GDK_DEBUG_GL_DISABLE      = 1 << 12,
  GDK_DEBUG_GL_SOFTWARE     = 1 << 13,
  GDK_DEBUG_GL_TEXTURE_RECT = 1 << 14,
  GDK_DEBUG_GL_LEGACY       = 1 << 15,
  GDK_DEBUG_GL_GLES         = 1 << 16,
  GDK_DEBUG_VULKAN_DISABLE  = 1 << 17,
  GDK_DEBUG_VULKAN_VALIDATE = 1 << 18
} GdkDebugFlags;

extern guint _gdk_debug_flags;

GdkDebugFlags    gdk_display_get_debug_flags    (GdkDisplay       *display);
void             gdk_display_set_debug_flags    (GdkDisplay       *display,
                                                 GdkDebugFlags     flags);

#ifdef G_ENABLE_DEBUG

#define GDK_DISPLAY_DEBUG_CHECK(display,type) \
    G_UNLIKELY (gdk_display_get_debug_flags (display) & GDK_DEBUG_##type)
#define GDK_DISPLAY_NOTE(display,type,action)          G_STMT_START {     \
    if (GDK_DISPLAY_DEBUG_CHECK (display,type))                           \
       { action; };                            } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GDK_DISPLAY_DEBUG_CHECK(display,type) 0
#define GDK_DISPLAY_NOTE(display,type,action)

#endif /* G_ENABLE_DEBUG */

#define GDK_DEBUG_CHECK(type) GDK_DISPLAY_DEBUG_CHECK (NULL,type)
#define GDK_NOTE(type,action) GDK_DISPLAY_NOTE (NULL,type,action)

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

typedef struct _GdkSurfacePaint GdkSurfacePaint;

typedef enum
{
  GDK_INPUT_OUTPUT,
  GDK_INPUT_ONLY
} GdkSurfaceSurfaceClass;


struct _GdkSurfaceAttr
{
  gint x, y;
  gint width;
  gint height;
  GdkSurfaceSurfaceClass wclass;
  GdkSurfaceType surface_type;
  GdkSurfaceTypeHint type_hint;
};

struct _GdkSurface
{
  GObject parent_instance;

  GdkDisplay *display;

  GdkSurfaceImpl *impl; /* window-system-specific delegate object */

  GdkSurface *parent;
  GdkSurface *transient_for;

  gpointer user_data;

  gint x;
  gint y;

  guint8 surface_type;

  guint8 resize_count;

  GList *children;
  GList children_list_node;

  GdkGLContext *gl_paint_context;

  cairo_region_t *update_area;
  guint update_freeze_count;
  /* This is the update_area that was in effect when the current expose
     started. It may be smaller than the expose area if we'e painting
     more than we have to, but it represents the "true" damage. */
  cairo_region_t *active_update_area;

  GdkSurfaceState old_state;
  GdkSurfaceState state;

  guint8 alpha;
  guint8 fullscreen_mode;

  guint input_only : 1;
  guint pass_through : 1;
  guint modal_hint : 1;

  guint destroyed : 2;

  guint accept_focus : 1;
  guint focus_on_map : 1;
  guint support_multidevice : 1;
  guint viewable : 1; /* mapped and all parents mapped */
  guint in_update : 1;
  guint frame_clock_events_paused : 1;

  /* The GdkSurface that has the impl, ref:ed if another surface.
   * This ref is required to keep the wrapper of the impl surface alive
   * for as long as any GdkSurface references the impl. */
  GdkSurface *impl_surface;

  guint update_and_descendants_freeze_count;

  gint abs_x, abs_y; /* Absolute offset in impl */
  gint width, height;
  gint shadow_top;
  gint shadow_left;
  gint shadow_right;
  gint shadow_bottom;

  GdkCursor *cursor;
  GHashTable *device_cursor;

  cairo_region_t *input_shape;

  GList *devices_inside;

  GdkFrameClock *frame_clock; /* NULL to use from parent or default */

  GSList *draw_contexts;
  GdkDrawContext *paint_context;

  cairo_region_t *opaque_region;
};

#define GDK_SURFACE_TYPE(d) ((((GdkSurface *)(d)))->surface_type)
#define GDK_SURFACE_DESTROYED(d) (((GdkSurface *)(d))->destroyed)

extern gint       _gdk_screen_number;

GdkEvent* _gdk_event_unqueue (GdkDisplay *display);

void     gdk_event_set_pointer_emulated (GdkEvent *event,
                                         gboolean  emulated);

void     gdk_event_set_scancode        (GdkEvent *event,
                                        guint16 scancode);

void   _gdk_event_emit               (GdkEvent   *event);
GList* _gdk_event_queue_find_first   (GdkDisplay *display);
void   _gdk_event_queue_remove_link  (GdkDisplay *display,
                                      GList      *node);
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

void gdk_surface_set_state (GdkSurface      *surface,
                            GdkSurfaceState  new_state);

gboolean        _gdk_cairo_surface_extents       (cairo_surface_t *surface,
                                                  GdkRectangle    *extents);
void            gdk_gl_texture_from_surface      (cairo_surface_t *surface,
                                                  cairo_region_t  *region);

typedef struct {
  float x1, y1, x2, y2;
  float u1, v1, u2, v2;
} GdkTexturedQuad;

void           gdk_gl_texture_quads               (GdkGLContext *paint_context,
                                                   guint texture_target,
                                                   int n_quads,
                                                   GdkTexturedQuad *quads,
                                                   gboolean flip_colors);

void            gdk_cairo_surface_paint_pixbuf   (cairo_surface_t *surface,
                                                  const GdkPixbuf *pixbuf);

cairo_region_t *gdk_cairo_region_from_clip       (cairo_t         *cr);

/*************************************
 * Interfaces used by windowing code *
 *************************************/

GdkSurface* gdk_surface_new               (GdkDisplay     *display,
                                           GdkSurface      *parent,
                                           GdkSurfaceAttr  *attributes);
void       _gdk_surface_destroy           (GdkSurface      *surface,
                                           gboolean        foreign_destroy);
void       _gdk_surface_clear_update_area (GdkSurface      *surface);
void       _gdk_surface_update_size       (GdkSurface      *surface);
gboolean   _gdk_surface_update_viewable   (GdkSurface      *surface);
GdkGLContext * gdk_surface_get_paint_gl_context (GdkSurface *surface,
                                                 GError   **error);
void gdk_surface_get_unscaled_size (GdkSurface *surface,
                                    int *unscaled_width,
                                    int *unscaled_height);

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

#define GDK_SURFACE_IS_MAPPED(surface) (((surface)->state & GDK_SURFACE_STATE_WITHDRAWN) == 0)

GdkSurface * _gdk_surface_find_child_at (GdkSurface *surface,
                                         double x, double y);
GdkSurface * _gdk_surface_find_descendant_at (GdkSurface *toplevel,
                                              double x, double y,
                                              double *found_x,
                                              double *found_y);

GdkEvent * _gdk_make_event (GdkSurface    *surface,
                            GdkEventType  type,
                            GdkEvent     *event_in_queue,
                            gboolean      before_event);
gboolean _gdk_surface_event_parent_of (GdkSurface *parent,
                                       GdkSurface *child);

void _gdk_synthesize_crossing_events (GdkDisplay                 *display,
                                      GdkSurface                  *src,
                                      GdkSurface                  *dest,
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
void _gdk_display_set_surface_under_pointer (GdkDisplay *display,
                                             GdkDevice  *device,
                                             GdkSurface  *surface);

gboolean    _gdk_surface_has_impl (GdkSurface *surface);
GdkSurface * _gdk_surface_get_impl_surface (GdkSurface *surface);

void gdk_surface_destroy_notify       (GdkSurface *surface);

void gdk_synthesize_surface_state (GdkSurface     *surface,
                                   GdkSurfaceState unset_flags,
                                   GdkSurfaceState set_flags);


G_END_DECLS

#endif /* __GDK_INTERNALS_H__ */
