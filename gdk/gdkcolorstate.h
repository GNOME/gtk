/* gdkcolorstate.h
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

#define GDK_TYPE_COLOR_STATE (gdk_color_state_get_type ())

GDK_AVAILABLE_IN_4_16
GType           gdk_color_state_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_16
GdkColorState * gdk_color_state_ref                     (GdkColorState  *self);

GDK_AVAILABLE_IN_4_16
void            gdk_color_state_unref                   (GdkColorState  *self);

GDK_AVAILABLE_IN_4_16
GdkColorState * gdk_color_state_get_srgb                (void);

GDK_AVAILABLE_IN_4_16
GdkColorState * gdk_color_state_get_srgb_linear         (void);

GDK_AVAILABLE_IN_4_16
GdkColorState * gdk_color_state_get_rec2100_pq          (void);

GDK_AVAILABLE_IN_4_16
GdkColorState * gdk_color_state_get_rec2100_linear      (void);

GDK_AVAILABLE_IN_4_16
gboolean        gdk_color_state_equal                   (GdkColorState  *self,
                                                         GdkColorState  *other);

GDK_AVAILABLE_IN_4_16
GdkCicpParams  *gdk_color_state_create_cicp_params      (GdkColorState  *self);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkColorState, gdk_color_state_unref);

G_END_DECLS
