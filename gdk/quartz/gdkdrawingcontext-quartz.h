/* GDK - The GIMP Drawing Kit
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

#ifndef __GDK_QUARTZ_DRAWING_CONTEXT_H__
#define __GDK_QUARTZ_DRAWING_CONTEXT_H__

#include "gdk/gdkdrawingcontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_DRAWING_CONTEXT                (gdk_quartz_drawing_context_get_type ())
#define GDK_QUARTZ_DRAWING_CONTEXT(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_QUARTZ_DRAWING_CONTEXT, GdkQuartzDrawingContext))
#define GDK_IS_QUARTZ_DRAWING_CONTEXT(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_QUARTZ_DRAWING_CONTEXT))
#define GDK_QUARTZ_DRAWING_CONTEXT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_QUARTZ_DRAWING_CONTEXT, GdkQuartzDrawingContextClass))
#define GDK_IS_QUARTZ_DRAWING_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_QUARTZ_DRAWING_CONTEXT))
#define GDK_QUARTZ_DRAWING_CONTEXT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_QUARTZ_DRAWING_CONTEXT, GdkQuartzDrawingContextClass))

typedef struct _GdkQuartzDrawingContext       GdkQuartzDrawingContext;
typedef struct _GdkQuartzDrawingContextClass  GdkQuartzDrawingContextClass;

struct _GdkQuartzDrawingContext
{
  GdkDrawingContext parent_instance;
};

struct _GdkQuartzDrawingContextClass
{
  GdkDrawingContextClass parent_instance;
};

GType gdk_quartz_drawing_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_QUARTZ_DRAWING_CONTEXT_H__ */
