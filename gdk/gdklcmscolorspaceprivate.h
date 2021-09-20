/* gdklcmscolorspace.h
 *
 * Copyright 2021 (c) Benjamin Otte
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

#ifndef __GDK_LCMS_COLOR_SPACE_PRIVATE_H__
#define __GDK_LCMS_COLOR_SPACE_PRIVATE_H__

#include "gdkcolorspaceprivate.h"

#include <lcms2.h>

G_BEGIN_DECLS

#define GDK_TYPE_LCMS_COLOR_SPACE (gdk_lcms_color_space_get_type ())

#define GDK_LCMS_COLOR_SPACE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_LCMS_COLOR_SPACE, GdkLcmsColorSpace))
#define GDK_IS_LCMS_COLOR_SPACE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_LCMS_COLOR_SPACE))
#define GDK_LCMS_COLOR_SPACE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_LCMS_COLOR_SPACE, GdkLcmsColorSpaceClass))
#define GDK_IS_LCMS_COLOR_SPACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_LCMS_COLOR_SPACE))
#define GDK_LCMS_COLOR_SPACE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_LCMS_COLOR_SPACE, GdkLcmsColorSpaceClass))

typedef struct _GdkLcmsColorSpace             GdkLcmsColorSpace;
typedef struct _GdkLcmsColorSpaceClass        GdkLcmsColorSpaceClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkLcmsColorSpace, g_object_unref)

GType                   gdk_lcms_color_space_get_type                   (void) G_GNUC_CONST;

GdkColorSpace *         gdk_lcms_color_space_new_from_lcms_profile      (cmsHPROFILE             lcms_profile);
cmsHPROFILE             gdk_lcms_color_space_get_lcms_profile           (GdkColorSpace          *color_space);

G_END_DECLS

#endif /* __GDK_LCMS_COLOR_SPACE_PRIVATE_H__ */
