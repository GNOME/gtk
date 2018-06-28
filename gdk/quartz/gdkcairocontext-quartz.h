/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
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

#ifndef __GDK_QUARTZ_CAIRO_CONTEXT__
#define __GDK_QUARTZ_CAIRO_CONTEXT__

#include "gdkconfig.h"

#include "gdkcairocontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_CAIRO_CONTEXT		(gdk_quartz_cairo_context_get_type ())
#define GDK_QUARTZ_CAIRO_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_QUARTZ_CAIRO_CONTEXT, GdkQuartzCairoContext))
#define GDK_IS_QUARTZ_CAIRO_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_QUARTZ_CAIRO_CONTEXT))
#define GDK_QUARTZ_CAIRO_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_QUARTZ_CAIRO_CONTEXT, GdkQuartzCairoContextClass))
#define GDK_IS_QUARTZ_CAIRO_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_QUARTZ_CAIRO_CONTEXT))
#define GDK_QUARTZ_CAIRO_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_QUARTZ_CAIRO_CONTEXT, GdkQuartzCairoContextClass))

typedef struct _GdkQuartzCairoContext GdkQuartzCairoContext;
typedef struct _GdkQuartzCairoContextClass GdkQuartzCairoContextClass;

struct _GdkQuartzCairoContext
{
  GdkCairoContext parent_instance;
};

struct _GdkQuartzCairoContextClass
{
  GdkCairoContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_quartz_cairo_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_QUARTZ_CAIRO_CONTEXT__ */
