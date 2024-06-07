/* gdklcmscolorstate.h
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

#pragma once

#include "gdkcolorstateprivate.h"

#include <lcms2.h>

G_BEGIN_DECLS

#define GDK_TYPE_LCMS_COLOR_STATE (gdk_lcms_color_state_get_type ())

GDK_DECLARE_INTERNAL_TYPE (GdkLcmsColorState, gdk_lcms_color_state, GDK, LCMS_COLOR_STATE, GdkColorState)

GdkColorState *         gdk_lcms_color_state_new_from_lcms_profile      (cmsHPROFILE             lcms_profile);
cmsHPROFILE             gdk_lcms_color_state_get_lcms_profile           (GdkColorState          *color_state);
cmsHTRANSFORM *         gdk_lcms_color_state_lookup_transform           (GdkColorState          *source,
                                                                         guint                   source_type,
                                                                         GdkColorState          *dest,
                                                                         guint                   dest_type);

G_END_DECLS
