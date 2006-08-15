/* gdkwindow-quartz.c
 *
 * Copyright (C) 2005-2006 Imendio AB
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

#include <config.h>

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
  GdkGC parent_instance;

  GdkGCValuesMask values_mask;

  gboolean have_clip_region;
  gboolean have_clip_mask;
  CGImageRef clip_mask;
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

GType _gdk_gc_quartz_get_type (void);

GdkGC *_gdk_quartz_gc_new (GdkDrawable      *drawable,
			  GdkGCValues      *values,
			  GdkGCValuesMask   values_mask);

/* Initialization */
void _gdk_windowing_window_init (void);
void _gdk_events_init           (void);
void _gdk_visual_init           (void);
void _gdk_input_init            (void);

void gdk_quartz_set_context_fill_color_from_pixel (CGContextRef context, GdkColormap *colormap, guint32 pixel);
void gdk_quartz_set_context_stroke_color_from_pixel (CGContextRef context, GdkColormap *colormap, guint32 pixel);

void gdk_quartz_update_context_from_gc (CGContextRef context, GdkGC *gc);

gint        _gdk_quartz_get_inverted_screen_y      (gint y);

GdkWindow * _gdk_quartz_find_child_window_by_point (GdkWindow *toplevel,
						    int        x,
						    int        y,
						    int       *x_ret,
						    int       *y_ret);

void        _gdk_quartz_update_focus_window (GdkWindow *new_window,
					     gboolean   got_focus);
GdkWindow *_gdk_quartz_get_mouse_window     (void);
void       _gdk_quartz_update_mouse_window  (GdkWindow *window);
void       _gdk_quartz_update_cursor        (GdkWindow *window);

GdkImage  *_gdk_quartz_copy_to_image (GdkDrawable *drawable,
				      GdkImage    *image,
				      gint         src_x,
				      gint         src_y,
				      gint         dest_x,
				      gint         dest_y,
				      gint         width,
				      gint         height);

void _gdk_quartz_send_map_events (GdkWindow *window);

GdkEventType _gdk_quartz_key_event_type  (NSEvent   *event);
gboolean     _gdk_quartz_key_is_modifier (guint      keycode);

GdkEventMask _gdk_quartz_get_current_event_mask (void);

extern GdkWindow *_gdk_quartz_keyboard_grab_window;
extern GdkWindow *_gdk_quartz_pointer_grab_window;

#endif /* __GDK_PRIVATE_QUARTZ_H__ */
