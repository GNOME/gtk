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
#include "gdkdisplay.h"
#include "gdkeventsprivate.h"
#include "gdksurfaceprivate.h"
#include "gdkenumtypes.h"
#include "gdkdragprivate.h"
#include "gdkkeysprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkseatprivate.h"

G_BEGIN_DECLS

/**********************
 * General Facilities * 
 **********************/

/* Debugging support */

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
  GDK_DEBUG_GL_DEBUG        = 1 << 17,
  GDK_DEBUG_VULKAN_DISABLE  = 1 << 18,
  GDK_DEBUG_VULKAN_VALIDATE = 1 << 19,
  GDK_DEBUG_DEFAULT_SETTINGS= 1 << 20
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

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0,

  /* When we are ready to draw a frame, we pause event delivery,
   * mark all events in the queue with this flag, and deliver
   * only those events until we finish the frame.
   */
  GDK_EVENT_FLUSHED = 1 << 2
} GdkEventFlags;

typedef struct _GdkSurfacePaint GdkSurfacePaint;

#define GDK_SURFACE_DESTROYED(d) (((GdkSurface *)(d))->destroyed)

GdkEvent* _gdk_event_unqueue (GdkDisplay *display);

void   _gdk_event_emit               (GdkEvent   *event);
GList* _gdk_event_queue_find_first   (GdkDisplay *display);
void   _gdk_event_queue_remove_link  (GdkDisplay *display,
                                      GList      *node);
GList* _gdk_event_queue_append       (GdkDisplay *display,
                                      GdkEvent   *event);

void    _gdk_event_queue_handle_motion_compression (GdkDisplay *display);
void    gdk_event_queue_handle_scroll_compression  (GdkDisplay *display);
void    _gdk_event_queue_flush                     (GdkDisplay       *display);

gboolean        _gdk_cairo_surface_extents       (cairo_surface_t *surface,
                                                  GdkRectangle    *extents);

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

void       _gdk_surface_destroy           (GdkSurface      *surface,
                                           gboolean        foreign_destroy);
void       gdk_surface_invalidate_rect    (GdkSurface           *surface,
                                           const GdkRectangle   *rect);
void       gdk_surface_invalidate_region  (GdkSurface           *surface,
                                           const cairo_region_t *region);
void       _gdk_surface_clear_update_area (GdkSurface      *surface);
void       _gdk_surface_update_size       (GdkSurface      *surface);
GdkGLContext * gdk_surface_get_paint_gl_context (GdkSurface *surface,
                                                 GError   **error);
void gdk_surface_get_unscaled_size (GdkSurface *surface,
                                    int *unscaled_width,
                                    int *unscaled_height);
gboolean gdk_surface_handle_event (GdkEvent       *event);
GdkSeat * gdk_surface_get_seat_from_event (GdkSurface *surface,
                                           GdkEvent    *event);

void       gdk_surface_enter_monitor (GdkSurface *surface,
                                      GdkMonitor *monitor);
void       gdk_surface_leave_monitor (GdkSurface *surface,
                                      GdkMonitor *monitor);

/*****************************************
 * Interfaces provided by windowing code *
 *****************************************/

void _gdk_windowing_got_event                (GdkDisplay       *display,
                                              GList            *event_link,
                                              GdkEvent         *event,
                                              gulong            serial);

#define GDK_SURFACE_IS_MAPPED(surface) (((surface)->state & GDK_SURFACE_STATE_WITHDRAWN) == 0)

void _gdk_synthesize_crossing_events (GdkDisplay                 *display,
                                      GdkSurface                  *src,
                                      GdkSurface                  *dest,
                                      GdkDevice                  *device,
                                      GdkDevice                  *source_device,
                                      GdkCrossingMode             mode,
                                      double                      toplevel_x,
                                      double                      toplevel_y,
                                      GdkModifierType             mask,
                                      guint32                     time_,
                                      GdkEvent                   *event_in_queue,
                                      gulong                      serial,
                                      gboolean                    non_linear);
void _gdk_display_set_surface_under_pointer (GdkDisplay *display,
                                             GdkDevice  *device,
                                             GdkSurface  *surface);

void gdk_surface_destroy_notify       (GdkSurface *surface);

void gdk_synthesize_surface_state (GdkSurface     *surface,
                                   GdkSurfaceState unset_flags,
                                   GdkSurfaceState set_flags);

void gdk_surface_get_root_coords (GdkSurface *surface,
                                  int         x,
                                  int         y,
                                  int        *root_x,
                                  int        *root_y);
void gdk_surface_get_origin      (GdkSurface *surface,
                                  int        *x,
                                  int        *y);


void gdk_surface_get_geometry (GdkSurface *surface,
                               int        *x,
                               int        *y,
                               int        *width,
                               int        *height);

GdkGLContext *gdk_surface_get_shared_data_gl_context (GdkSurface *surface);

typedef enum
{
  GDK_HINT_MIN_SIZE    = 1 << 1,
  GDK_HINT_MAX_SIZE    = 1 << 2,
} GdkSurfaceHints;

typedef struct _GdkGeometry GdkGeometry;

struct _GdkGeometry
{
  int min_width;
  int min_height;
  int max_width;
  int max_height;
};

GDK_AVAILABLE_IN_ALL
void       gdk_surface_constrain_size      (GdkGeometry    *geometry,
                                            GdkSurfaceHints  flags,
                                            int             width,
                                            int             height,
                                            int            *new_width,
                                            int            *new_height);

GdkSurface *   gdk_surface_new_temp             (GdkDisplay    *display,
                                                 const GdkRectangle *position);

GdkKeymap *  gdk_display_get_keymap  (GdkDisplay *display);

void gdk_surface_begin_resize_drag            (GdkSurface     *surface,
                                               GdkSurfaceEdge  edge,
                                               GdkDevice      *device,
                                               int             button,
                                               int             x,
                                               int             y,
                                               guint32         timestamp);

void gdk_surface_begin_move_drag              (GdkSurface     *surface,
                                               GdkDevice      *device,
                                               int             button,
                                               int             x,
                                               int             y,
                                               guint32         timestamp);

void       gdk_surface_freeze_updates      (GdkSurface    *surface);
void       gdk_surface_thaw_updates        (GdkSurface    *surface);


G_END_DECLS

#endif /* __GDK_INTERNALS_H__ */
