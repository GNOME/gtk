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

#ifndef __GDK_WIN32_DRAWING_CONTEXT_H__
#define __GDK_WIN32_DRAWING_CONTEXT_H__

#include "gdk/gdkdrawingcontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_DRAWING_CONTEXT                (gdk_win32_drawing_context_get_type ())
#define GDK_WIN32_DRAWING_CONTEXT(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WIN32_DRAWING_CONTEXT, GdkWin32DrawingContext))
#define GDK_IS_WIN32_DRAWING_CONTEXT(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WIN32_DRAWING_CONTEXT))
#define GDK_WIN32_DRAWING_CONTEXT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_DRAWING_CONTEXT, GdkWin32DrawingContextClass))
#define GDK_IS_WIN32_DRAWING_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_DRAWING_CONTEXT))
#define GDK_WIN32_DRAWING_CONTEXT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_DRAWING_CONTEXT, GdkWin32DrawingContextClass))

typedef struct _GdkWin32DrawingContext       GdkWin32DrawingContext;
typedef struct _GdkWin32DrawingContextClass  GdkWin32DrawingContextClass;

struct _GdkWin32DrawingContext
{
  GdkDrawingContext parent_instance;
};

struct _GdkWin32DrawingContextClass
{
  GdkDrawingContextClass parent_instance;
};

GType gdk_win32_drawing_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_WIN32_DRAWING_CONTEXT_H__ */
