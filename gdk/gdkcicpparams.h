/* gdkcicpparams.h
 *
 * Copyright 2024 Red Hat, Inc.
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

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS


#define GDK_TYPE_CICP_PARAMS (gdk_cicp_params_get_type ())
GDK_AVAILABLE_IN_4_16
GDK_DECLARE_INTERNAL_TYPE (GdkCicpParams, gdk_cicp_params, GDK, CICP_PARAMS, GObject)

GDK_AVAILABLE_IN_4_16
GdkCicpParams  *gdk_cicp_params_new                     (void);

GDK_AVAILABLE_IN_4_16
guint           gdk_cicp_params_get_color_primaries     (GdkCicpParams    *self);

GDK_AVAILABLE_IN_4_16
void            gdk_cicp_params_set_color_primaries     (GdkCicpParams    *self,
                                                         guint             color_primaries);

GDK_AVAILABLE_IN_4_16
guint           gdk_cicp_params_get_transfer_function   (GdkCicpParams    *self);

GDK_AVAILABLE_IN_4_16
void            gdk_cicp_params_set_transfer_function   (GdkCicpParams    *self,
                                                         guint             transfer_function);

GDK_AVAILABLE_IN_4_16
guint           gdk_cicp_params_get_matrix_coefficients (GdkCicpParams    *self);

GDK_AVAILABLE_IN_4_16
void            gdk_cicp_params_set_matrix_coefficients (GdkCicpParams    *self,
                                                         guint             matrix_coefficients);

/**
 * GdkCicpRange:
 * @GDK_CICP_RANGE_NARROW: The values use the range of 16-235 (for Y) and 16-240 for u and v.
 * @GDK_CICP_RANGE_FULL: The values use the full range.
 *
 * The values of this enumeration describe whether image data uses
 * the full range of 8-bit values.
 *
 * In digital broadcasting, it is common to reserve the lowest and
 * highest values. Typically the allowed values for the narrow range
 * are 16-235 for Y and 16-240 for u,v (when dealing with YUV data).
 *
 * Since: 4.16
 */
typedef enum
{
  GDK_CICP_RANGE_NARROW,
  GDK_CICP_RANGE_FULL,
} GdkCicpRange;

GDK_AVAILABLE_IN_4_16
GdkCicpRange    gdk_cicp_params_get_range               (GdkCicpParams    *self);

GDK_AVAILABLE_IN_4_16
void            gdk_cicp_params_set_range               (GdkCicpParams    *self,
                                                         GdkCicpRange      range);

GDK_AVAILABLE_IN_4_16
GdkColorState * gdk_cicp_params_build_color_state       (GdkCicpParams    *self,
                                                         GError          **error);


G_END_DECLS
