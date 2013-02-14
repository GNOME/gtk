/* gdkquartzscreen.h
 *
 * Copyright (C) 2009, 2010  Kristian Rietveld  <kris@gtk.org>
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

#ifndef __GDK_QUARTZ_SCREEN_H__
#define __GDK_QUARTZ_SCREEN_H__

#if !defined(__GDKQUARTZ_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkquartz.h> can be included directly."
#endif

G_BEGIN_DECLS

#include <gdk/gdk.h>

#define GDK_TYPE_QUARTZ_SCREEN              (gdk_quartz_screen_get_type ())
#define GDK_QUARTZ_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_QUARTZ_SCREEN, GdkQuartzScreen))
#define GDK_QUARTZ_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_QUARTZ_SCREEN, GdkQuartzScreenClass))
#define GDK_IS_QUARTZ_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_QUARTZ_SCREEN))
#define GDK_IS_QUARTZ_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_QUARTZ_SCREEN))
#define GDK_QUARTZ_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_QUARTZ_SCREEN, GdkQuartzScreenClass))

#ifdef GDK_COMPILATION
typedef struct _GdkQuartzScreen GdkQuartzScreen;
#else
typedef GdkScreen GdkQuartzScreen;
#endif
typedef struct _GdkQuartzScreenClass GdkQuartzScreenClass;


GType      gdk_quartz_screen_get_type (void);

G_END_DECLS

#endif /* _GDK_QUARTZ_SCREEN_H_ */
