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
void                    gtk_snapshot_append_text2               (GtkSnapshot            *snapshot,
                                                                 PangoFont              *font,
                                                                 PangoGlyphString       *glyphs,
                                                                 GdkColorState          *color_state,
                                                                 const float             color[4],
                                                                 float                   x,
                                                                 float                   y);

void                    gtk_snapshot_push_collect               (GtkSnapshot            *snapshot);
GskRenderNode *         gtk_snapshot_pop_collect                (GtkSnapshot            *snapshot);

void                    gtk_snapshot_push_subsurface            (GtkSnapshot            *snapshot,
                                                                 GdkSubsurface          *subsurface);

void                    gtk_snapshot_append_color2              (GtkSnapshot            *snapshot,
                                                                 GdkColorState          *color_state,
                                                                 const float             values[4],
                                                                 const graphene_rect_t  *bounds);
void                    gtk_snapshot_append_border2             (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const float             border_width[4],
                                                                 GdkColorState          *color_state[4],
                                                                 const float             border_color[4][4]);

void                    gtk_snapshot_append_inset_shadow2       (GtkSnapshot          *snapshot,
                                                                 const GskRoundedRect *outline,
                                                                 GdkColorState        *color_state,
                                                                 const float           color[4],
                                                                 float                 dx,
                                                                 float                 dy,
                                                                 float                 spread,
                                                                 float                 blur_radius);

void                    gtk_snapshot_append_outset_shadow2      (GtkSnapshot          *snapshot,
                                                                 const GskRoundedRect *outline,
                                                                 GdkColorState        *color_state,
                                                                 const float           color[4],
                                                                 float                 dx,
                                                                 float                 dy,
                                                                 float                 spread,
                                                                 float                 blur_radius);

void                    gtk_snapshot_push_shadow2               (GtkSnapshot          *snapshot,
                                                                 const GskShadow      *shadow,
                                                                 gsize                 n_shadows,
                                                                 GdkColorState       **color_states);

void                    gtk_snapshot_append_linear_gradient2    (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *start_point,
                                                                 const graphene_point_t  *end_point,
                                                                 const GskColorStop      *stops,
                                                                 gsize                    n_stops,
                                                                 GdkColorState          **color_states);
void                    gtk_snapshot_append_repeating_linear_gradient2
                                                                (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *start_point,
                                                                 const graphene_point_t  *end_point,
                                                                 const GskColorStop      *stops,
                                                                 gsize                    n_stops,
                                                                 GdkColorState          **color_states);
void                    gtk_snapshot_append_radial_gradient2    (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *center,
                                                                 float                    hradius,
                                                                 float                    vradius,
                                                                 float                    start,
                                                                 float                    end,
                                                                 const GskColorStop      *stops,
                                                                 gsize                    n_stops,
                                                                 GdkColorState          **color_states);
void                    gtk_snapshot_append_repeating_radial_gradient2
                                                                (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *center,
                                                                 float                    hradius,
                                                                 float                    vradius,
                                                                 float                    start,
                                                                 float                    end,
                                                                 const GskColorStop      *stops,
                                                                 gsize                    n_stops,
                                                                 GdkColorState          **color_states);
void                    gtk_snapshot_append_conic_gradient2     (GtkSnapshot             *snapshot,
                                                                 const graphene_rect_t   *bounds,
                                                                 const graphene_point_t  *center,
                                                                 float                    rotation,
                                                                 const GskColorStop      *stops,
                                                                 gsize                    n_stops,
                                                                 GdkColorState          **color_states);


G_END_DECLS

