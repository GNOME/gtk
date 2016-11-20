/* GDK - The GIMP Drawing Kit
 * Copyright 2016  Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#define GDK_TYPE_MIR_DRAWING_CONTEXT                (gdk_mir_drawing_context_get_type ())
#define GDK_MIR_DRAWING_CONTEXT(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_MIR_DRAWING_CONTEXT, GdkMirDrawingContext))
#define GDK_IS_MIR_DRAWING_CONTEXT(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_MIR_DRAWING_CONTEXT))
#define GDK_MIR_DRAWING_CONTEXT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_DRAWING_CONTEXT, GdkMirDrawingContextClass))
#define GDK_IS_MIR_DRAWING_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_DRAWING_CONTEXT))
#define GDK_MIR_DRAWING_CONTEXT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_DRAWING_CONTEXT, GdkMirDrawingContextClass))

typedef struct _GdkMirDrawingContext       GdkMirDrawingContext;
typedef struct _GdkMirDrawingContextClass  GdkMirDrawingContextClass;

struct _GdkMirDrawingContext
{
  GdkDrawingContext parent_instance;
};

struct _GdkMirDrawingContextClass
{
  GdkDrawingContextClass parent_instance;
};

GType gdk_mir_drawing_context_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkMirDrawingContext, gdk_mir_drawing_context, GDK_TYPE_DRAWING_CONTEXT)

static void
gdk_mir_drawing_context_class_init (GdkMirDrawingContextClass *klass)
{
}

static void
gdk_mir_drawing_context_init (GdkMirDrawingContext *self)
{
}

GdkDrawingContext *
gdk_mir_drawing_context_new (GdkWindow            *window,
                             const cairo_region_t *region)
{
  return g_object_new (GDK_TYPE_MIR_DRAWING_CONTEXT,
                       "window", window,
                       "clip", region,
                       NULL);
}

