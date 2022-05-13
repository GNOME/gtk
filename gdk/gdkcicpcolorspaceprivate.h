/* gdkcicpcolorspaceprivate.h
 *
 * Copyright 2022 (c) Red Hat, Inc
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

#ifndef __GDK_CICP_COLOR_SPACE_PRIVATE_H__
#define __GDK_CICP_COLOR_SPACE_PRIVATE_H__

#include "gdkcolorspaceprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_CICP_COLOR_SPACE (gdk_cicp_color_space_get_type ())

#define GDK_CICP_COLOR_SPACE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CICP_COLOR_SPACE, GdkCicpColorSpace))
#define GDK_IS_CICP_COLOR_SPACE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CICP_COLOR_SPACE))
#define GDK_CICP_COLOR_SPACE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CICP_COLOR_SPACE, GdkCicpColorSpaceClass))
#define GDK_IS_CICP_COLOR_SPACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CICP_COLOR_SPACE))
#define GDK_CICP_COLOR_SPACE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CICP_COLOR_SPACE, GdkCicpColorSpaceClass))

typedef struct _GdkCicpColorSpace             GdkCicpColorSpace;
typedef struct _GdkCicpColorSpaceClass        GdkCicpColorSpaceClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkCicpColorSpace, g_object_unref)

GType                   gdk_cicp_color_space_get_type                   (void) G_GNUC_CONST;

GdkColorSpace *         gdk_cicp_color_space_get_lcms_color_space       (GdkColorSpace *self);

G_END_DECLS

#endif /* __GDK_CICP_COLOR_SPACE_PRIVATE_H__ */
