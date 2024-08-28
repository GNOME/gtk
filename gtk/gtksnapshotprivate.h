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
                                                                 const GdkColor         *color,
                                                                 float                   x,
                                                                 float                   y);

void                    gtk_snapshot_append_layout2             (GtkSnapshot            *snapshot,
                                                                 PangoLayout            *layout,
                                                                 const GdkColor         *color);

void                    gtk_snapshot_push_collect               (GtkSnapshot            *snapshot);
GskRenderNode *         gtk_snapshot_pop_collect                (GtkSnapshot            *snapshot);

void                    gtk_snapshot_push_subsurface            (GtkSnapshot            *snapshot,
                                                                 GdkSubsurface          *subsurface);

void                    gtk_snapshot_append_color2              (GtkSnapshot            *snapshot,
                                                                 const GdkColor         *color,
                                                                 const graphene_rect_t  *bounds);
void                    gtk_snapshot_append_border2             (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const float             border_width[4],
                                                                 const GdkColor          border_color[4]);

void                    gtk_snapshot_append_inset_shadow2       (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);

void                    gtk_snapshot_append_outset_shadow2      (GtkSnapshot            *snapshot,
                                                                 const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);

void                    gtk_snapshot_push_shadow2               (GtkSnapshot          *snapshot,
                                                                 const GskShadow2     *shadow,
                                                                 gsize                 n_shadows);

G_END_DECLS

