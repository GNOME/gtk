/* GDK - The GIMP Drawing Kit
 *
 * gdkcairocontext.h:  specific Cairo wrappers
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

#ifndef __GDK_CAIRO_CONTEXT__
#define __GDK_CAIRO_CONTEXT__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

#include <cairo.h>

G_BEGIN_DECLS

#define GDK_CAIRO_ERROR                    (gdk_cairo_error_quark ())

#define GDK_TYPE_CAIRO_CONTEXT (gdk_cairo_context_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GdkCairoContext, gdk_cairo_context, GDK, CAIRO_CONTEXT)

GDK_AVAILABLE_IN_ALL
cairo_t *               gdk_cairo_context_cairo_create                  (GdkCairoContext        *self);

G_END_DECLS

#endif /* __GDK_CAIRO_CONTEXT__ */
