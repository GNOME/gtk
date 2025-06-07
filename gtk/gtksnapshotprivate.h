/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtksnapshot.h"

#include "gdk/gdksubsurfaceprivate.h"
#include "gsk/gskrendernodeprivate.h"

G_BEGIN_DECLS

void                    gtk_snapshot_append_text                (GtkSnapshot            *snapshot,
                                                                 PangoFont              *font,
                                                                 PangoGlyphString       *glyphs,
                                                                 const GdkRGBA          *color,
                                                                 float                   x,
                                                                 float                   y);
void                    gtk_snapshot_add_text                   (GtkSnapshot            *snapshot,
                                                                 PangoFont              *font,
                                                                 PangoGlyphString       *glyphs,
                                                                 const GdkColor         *color,
                                                                 float                   x,
                                                                 float                   y);

void                    gtk_snapshot_add_layout                 (GtkSnapshot            *snapshot,
                                                                 PangoLayout            *layout,
                                                                 const GdkColor         *color);

void                    gtk_snapshot_push_collect               (GtkSnapshot            *snapshot);
GskRenderNode *         gtk_snapshot_pop_collect                (GtkSnapshot            *snapshot);

void                    gtk_snapshot_push_subsurface            (GtkSnapshot            *snapshot,
                                                                 GdkSubsurface          *subsurface);

void                    gtk_snapshot_add_color                  (GtkSnapshot            *snapshot,
                                                                 const GdkColor         *color,
                                                                 const graphene_rect_t  *bounds);
void                    gtk_snapshot_add_border                 (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const float             border_width[4],
                                                                 const GdkColor          border_color[4]);

void                    gtk_snapshot_add_inset_shadow           (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);

void                    gtk_snapshot_add_outset_shadow          (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);

void                    gtk_snapshot_push_shadows               (GtkSnapshot          *snapshot,
                                                                 const GskShadowEntry *shadow,
                                                                 gsize                 n_shadows);

void                    gtk_snapshot_add_linear_gradient        (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *start_point,
                                                                 const graphene_point_t  *end_point,
                                                                 GdkColorState           *interpolation,
                                                                 GskHueInterpolation      hue_interpolation,
                                                                 const GskGradientStop   *stops,
                                                                 gsize                    n_stops);
void                    gtk_snapshot_add_repeating_linear_gradient
                                                                (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *start_point,
                                                                 const graphene_point_t  *end_point,
                                                                 GdkColorState           *interpolation,
                                                                 GskHueInterpolation      hue_interpolation,
                                                                 const GskGradientStop   *stops,
                                                                 gsize                    n_stops);
void                    gtk_snapshot_add_radial_gradient        (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *center,
                                                                 float                    hradius,
                                                                 float                    vradius,
                                                                 float                    start,
                                                                 float                    end,
                                                                 GdkColorState           *interpolation,
                                                                 GskHueInterpolation      hue_interpolation,
                                                                 const GskGradientStop   *stops,
                                                                 gsize                    n_stops);
void                    gtk_snapshot_add_repeating_radial_gradient
                                                                (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *center,
                                                                 float                    hradius,
                                                                 float                    vradius,
                                                                 float                    start,
                                                                 float                    end,
                                                                 GdkColorState           *interpolation,
                                                                 GskHueInterpolation      hue_interpolation,
                                                                 const GskGradientStop   *stops,
                                                                 gsize                    n_stops);
void                    gtk_snapshot_add_conic_gradient         (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *center,
                                                                 float                    rotation,
                                                                 GdkColorState           *interpolation,
                                                                 GskHueInterpolation      hue_interpolation,
                                                                 const GskGradientStop   *stops,
                                                                 gsize                    n_stops);
void                    gtk_snapshot_append_node_scaled         (GtkSnapshot             *snapshot,
                                                                 GskRenderNode           *node,
                                                                 graphene_rect_t         *from,
                                                                 graphene_rect_t         *to);


G_END_DECLS
