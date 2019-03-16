/* gdkquartzwindow.h
 *
 * Copyright (C) 2005  Imendio AB
 * Copyright (C) 2010  Kristian Rietveld  <kris@gtk.org>
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

#ifndef __GDK_QUARTZ_WINDOW_H__
#define __GDK_QUARTZ_WINDOW_H__

#if !defined (__GDKQUARTZ_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkquartz.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_WINDOW              (gdk_quartz_window_get_type ())
#define GDK_QUARTZ_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_QUARTZ_WINDOW, GdkQuartzWindow))
#define GDK_QUARTZ_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_QUARTZ_WINDOW, GdkQuartzWindowClass))
#define GDK_IS_QUARTZ_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_QUARTZ_WINDOW))
#define GDK_IS_QUARTZ_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_QUARTZ_WINDOW))
#define GDK_QUARTZ_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_QUARTZ_WINDOW, GdkQuartzWindowClass))

#ifdef GDK_COMPILATION
typedef struct _GdkQuartzWindow GdkQuartzWindow;
#else
typedef GdkWindow GdkQuartzWindow;
#endif
typedef struct _GdkQuartzWindowClass GdkQuartzWindowClass;

GDK_AVAILABLE_IN_ALL
GType     gdk_quartz_window_get_type     (void);

G_END_DECLS

#endif /* __GDK_QUARTZ_WINDOW_H__ */
