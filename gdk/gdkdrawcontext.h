/* GDK - The GIMP Drawing Kit
 *
 * gdkdrawcontext.h: base class for rendering system support
 *
 * Copyright © 2016  Benjamin Otte
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

#ifndef __GDK_DRAW_CONTEXT__
#define __GDK_DRAW_CONTEXT__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_DRAW_CONTEXT             (gdk_draw_context_get_type ())
#define GDK_DRAW_CONTEXT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_DRAW_CONTEXT, GdkDrawContext))
#define GDK_IS_DRAW_CONTEXT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_DRAW_CONTEXT))

GDK_AVAILABLE_IN_3_90
GType gdk_draw_context_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_90
GdkDisplay *            gdk_draw_context_get_display              (GdkDrawContext  *context);
GDK_AVAILABLE_IN_3_90
GdkWindow *             gdk_draw_context_get_window               (GdkDrawContext  *context);

G_END_DECLS

#endif /* __GDK_DRAW_CONTEXT__ */
