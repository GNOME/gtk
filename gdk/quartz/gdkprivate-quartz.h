/* gdkwindow-quartz.c
 *
 * Copyright (C) 2005-2007 Imendio AB
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_PRIVATE_QUARTZ_H__
#define __GDK_PRIVATE_QUARTZ_H__

#define GDK_QUARTZ_ALLOC_POOL NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]
#define GDK_QUARTZ_RELEASE_POOL [pool release]

#include <gdk/gdkprivate.h>
#include <gdk/quartz/gdkpixmap-quartz.h>
#include <gdk/quartz/gdkwindow-quartz.h>

#include <gdk/gdk.h>

#include "gdkinternals.h"

#include "config.h"

#define GDK_TYPE_GC_QUARTZ              (_gdk_gc_quartz_get_type ())
#define GDK_GC_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_QUARTZ, GdkGCQuartz))
#define GDK_GC_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_QUARTZ, GdkGCQuartzClass))
#define GDK_IS_GC_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_QUARTZ))
#define GDK_IS_GC_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_QUARTZ))
#define GDK_GC_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_QUARTZ, GdkGCQuartzClass))

#define GDK_DRAG_CONTEXT_PRIVATE(context) ((GdkDragContextPrivate *) GDK_DRAG_CONTEXT (context)->windowing_data)

typedef struct _GdkCursorPrivate GdkCursorPrivate;
typedef struct _GdkGCQuartz       GdkGCQuartz;
typedef struct _GdkGCQuartzClass  GdkGCQuartzClass;
typedef struct _GdkDragContextPrivate GdkDragContextPrivate;

struct _GdkGCQuartz
{
  GdkGC             parent_instance;

  GdkFont          *font;
  GdkFunction       function;
  GdkSubwindowMode  subwindow_mode;
  gboolean          graphics_exposures;

  gboolean          have_clip_region;
  gboolean          have_clip_mask;
  CGImageRef        clip_mask;

  gint              line_width;
  GdkLineStyle      line_style;
  GdkCapStyle       cap_style;
  GdkJoinStyle      join_style;

  gfloat           *dash_lengths;
  gint              dash_count;
  gfloat            dash_phase;

  CGPatternRef      ts_pattern;

  guint             is_window : 1;
};

struct _GdkGCQuartzClass
{
  GdkGCClass parent_class;
};

struct _GdkVisualClass
{
  GObjectClass parent_class;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  NSCursor *nscursor;
};

struct _GdkDragContextPrivate
{
  id <NSDraggingInfo> dragging_info;
};

extern GdkDisplay *_gdk_display;
extern GdkScreen *_gdk_screen;
extern GdkWindow *_gdk_root;

extern GdkDragContext *_gdk_quartz_drag_source_context;

/* Initialization */
void _gdk_windowing_window_init  (void);
void _gdk_events_init            (void);
void _gdk_visual_init            (void);
void _gdk_input_init             (void);
void _gdk_quartz_event_loop_init (void);

/* GC */
typedef enum {
  GDK_QUARTZ_CONTEXT_STROKE = 1 << 0,
  GDK_QUARTZ_CONTEXT_FILL   = 1 << 1,
  GDK_QUARTZ_CONTEXT_TEXT   = 1 << 2
} GdkQuartzContextValuesMask;

GType  _gdk_gc_quartz_get_type          (void);
GdkGC *_gdk_quartz_gc_new               (GdkDrawable                *drawable,
					 GdkGCValues                *values,
					 GdkGCValuesMask             values_mask);
void   _gdk_quartz_gc_update_cg_context (GdkGC                      *gc,
					 GdkDrawable                *drawable,
					 CGContextRef                context,
					 GdkQuartzContextValuesMask  mask);

/* Colormap */
void _gdk_quartz_colormap_get_rgba_from_pixel (GdkColormap *colormap,
					       guint32      pixel,
					       gfloat      *red,
					       gfloat      *green,
					       gfloat      *blue,
					       gfloat      *alpha);

/* Window */
gboolean    _gdk_quartz_window_is_ancestor          (GdkWindow *ancestor,
                                                     GdkWindow *window);
gint       _gdk_quartz_window_get_inverted_screen_y (gint       y);
GdkWindow *_gdk_quartz_window_find_child            (GdkWindow *window,
						     gint       x,
						     gint       y);
void       _gdk_quartz_window_attach_to_parent      (GdkWindow *window);
void       _gdk_quartz_window_detach_from_parent    (GdkWindow *window);
void       _gdk_quartz_window_did_become_main       (GdkWindow *window);
void       _gdk_quartz_window_did_resign_main       (GdkWindow *window);
void       _gdk_quartz_window_debug_highlight       (GdkWindow *window,
                                                     gint       number);

/* Events */
typedef enum {
  GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP,
  GDK_QUARTZ_EVENT_SUBTYPE_FAKE_CROSSING
} GdkQuartzEventSubType;

void         _gdk_quartz_events_update_focus_window    (GdkWindow *new_window,
                                                        gboolean   got_focus);
GdkWindow *  _gdk_quartz_events_get_mouse_window       (gboolean   consider_grabs);
void         _gdk_quartz_events_update_mouse_window    (GdkWindow *window);
void         _gdk_quartz_events_update_cursor          (GdkWindow *window);
void         _gdk_quartz_events_send_map_events        (GdkWindow *window);
GdkEventMask _gdk_quartz_events_get_current_event_mask (void);
void         _gdk_quartz_events_trigger_crossing_events(gboolean   defer_to_mainloop);

extern GdkWindow *_gdk_quartz_keyboard_grab_window;
extern GdkWindow *_gdk_quartz_pointer_grab_window;

/* Event loop */
gboolean   _gdk_quartz_event_loop_check_pending (void);
NSEvent *  _gdk_quartz_event_loop_get_pending   (void);
void       _gdk_quartz_event_loop_release_event (NSEvent *event);

/* FIXME: image */
GdkImage *_gdk_quartz_image_copy_to_image (GdkDrawable *drawable,
					    GdkImage    *image,
					    gint         src_x,
					    gint         src_y,
					    gint         dest_x,
					    gint         dest_y,
					    gint         width,
					    gint         height);

/* Keys */
GdkEventType _gdk_quartz_keys_event_type  (NSEvent   *event);
gboolean     _gdk_quartz_keys_is_modifier (guint      keycode);

/* Drawable */
void        _gdk_quartz_drawable_finish (GdkDrawable *drawable);

/* Geometry */
void        _gdk_quartz_window_scroll      (GdkWindow       *window,
                                            gint             dx,
                                            gint             dy);
void        _gdk_quartz_window_move_region (GdkWindow       *window,
                                            const GdkRegion *region,
                                            gint             dx,
                                            gint             dy);

#endif /* __GDK_PRIVATE_QUARTZ_H__ */
