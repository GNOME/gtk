/* gdkdrawable-quartz.h
 *
 * Copyright (C) 2005 Imendio AB
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

#ifndef __GDK_WINDOW_QUARTZ_H__
#define __GDK_WINDOW_QUARTZ_H__

#import <gdk/quartz/GdkQuartzView.h>
#import <gdk/quartz/GdkQuartzNSWindow.h>
#include "gdk/gdkwindowimpl.h"

G_BEGIN_DECLS

/* Window implementation for Quartz
 */

typedef struct _GdkWindowImplQuartz GdkWindowImplQuartz;
typedef struct _GdkWindowImplQuartzClass GdkWindowImplQuartzClass;

#define GDK_TYPE_WINDOW_IMPL_QUARTZ              (_gdk_window_impl_quartz_get_type ())
#define GDK_WINDOW_IMPL_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_QUARTZ, GdkWindowImplQuartz))
#define GDK_WINDOW_IMPL_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_QUARTZ, GdkWindowImplQuartzClass))
#define GDK_IS_WINDOW_IMPL_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_QUARTZ))
#define GDK_IS_WINDOW_IMPL_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_QUARTZ))
#define GDK_WINDOW_IMPL_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_QUARTZ, GdkWindowImplQuartzClass))

struct _GdkWindowImplQuartz
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;

  NSWindow *toplevel;
  NSTrackingRectTag tracking_rect;
  GdkQuartzView *view;

  GdkWindowTypeHint type_hint;

  gint in_paint_rect_count;

  GdkWindow *transient_for;

  /* Sorted by z-order */
  GList *sorted_children;

  cairo_region_t *needs_display_region;

  cairo_surface_t *cairo_surface;

  gint shadow_top;
};
 
struct _GdkWindowImplQuartzClass 
{
  GdkWindowImplClass parent_class;

  CGContextRef  (* get_context)     (GdkWindowImplQuartz *window,
                                     gboolean             antialias);
  void          (* release_context) (GdkWindowImplQuartz *window,
                                     CGContextRef         cg_context);
};

GType _gdk_window_impl_quartz_get_type (void);

CGContextRef gdk_quartz_window_get_context     (GdkWindowImplQuartz *window,
                                                gboolean             antialias);
void         gdk_quartz_window_release_context (GdkWindowImplQuartz *window,
                                                CGContextRef         context);

/* Root window implementation for Quartz
 */

typedef struct _GdkRootWindowImplQuartz GdkRootWindowImplQuartz;
typedef struct _GdkRootWindowImplQuartzClass GdkRootWindowImplQuartzClass;

#define GDK_TYPE_ROOT_WINDOW_IMPL_QUARTZ              (_gdk_root_window_impl_quartz_get_type ())
#define GDK_ROOT_WINDOW_IMPL_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ROOT_WINDOW_IMPL_QUARTZ, GdkRootWindowImplQuartz))
#define GDK_ROOT_WINDOW_IMPL_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_ROOT_WINDOW_IMPL_QUARTZ, GdkRootWindowImplQuartzClass))
#define GDK_IS_ROOT_WINDOW_IMPL_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ROOT_WINDOW_IMPL_QUARTZ))
#define GDK_IS_ROOT_WINDOW_IMPL_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_ROOT_WINDOW_IMPL_QUARTZ))
#define GDK_ROOT_WINDOW_IMPL_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_ROOT_WINDOW_IMPL_QUARTZ, GdkRootWindowImplQuartzClass))

struct _GdkRootWindowImplQuartz
{
  GdkWindowImplQuartz parent_instance;
};
 
struct _GdkRootWindowImplQuartzClass 
{
  GdkWindowImplQuartzClass parent_class;
};

GType _gdk_root_window_impl_quartz_get_type (void);


G_END_DECLS

#endif /* __GDK_WINDOW_QUARTZ_H__ */
