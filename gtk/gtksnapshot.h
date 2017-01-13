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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_SNAPSHOT_H__
#define __GTK_SNAPSHOT_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gsk/gsk.h>

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push                       (GtkSnapshot            *snapshot,
                                                         gboolean                keep_coordinates,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_transform             (GtkSnapshot            *snapshot,
                                                         const graphene_matrix_t*transform,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_opacity               (GtkSnapshot            *snapshot,
                                                         double                  opacity,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_color_matrix          (GtkSnapshot            *snapshot,
                                                         const graphene_matrix_t*color_matrix,
                                                         const graphene_vec4_t  *color_offset,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (4, 5);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_repeat                (GtkSnapshot            *snapshot,
                                                         const graphene_rect_t  *bounds,
                                                         const graphene_rect_t  *child_bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (4, 5);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_clip                  (GtkSnapshot            *snapshot,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_rounded_clip          (GtkSnapshot            *snapshot,
                                                         const GskRoundedRect   *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_shadow                (GtkSnapshot            *snapshot,
                                                         const GskShadow        *shadow,
                                                         gsize                   n_shadows,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (4, 5);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_blend                 (GtkSnapshot            *snapshot,
                                                         GskBlendMode            blend_mode,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_cross_fade            (GtkSnapshot            *snapshot,
                                                         double                  progress,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_pop                        (GtkSnapshot            *snapshot);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_offset                     (GtkSnapshot            *snapshot,
                                                         int                     x,
                                                         int                     y);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_get_offset                 (GtkSnapshot            *snapshot,
                                                         int                    *x,
                                                         int                    *y);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_append_node                (GtkSnapshot            *snapshot,
                                                         GskRenderNode          *node);
GDK_AVAILABLE_IN_3_90
cairo_t *       gtk_snapshot_append_cairo               (GtkSnapshot            *snapshot,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF(3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_append_texture             (GtkSnapshot            *snapshot,
                                                         GskTexture             *texture,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (4, 5);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_append_color               (GtkSnapshot            *snapshot,
                                                         const GdkRGBA          *color,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF (4, 5);

GDK_AVAILABLE_IN_3_90
gboolean        gtk_snapshot_clips_rect                 (GtkSnapshot            *snapshot,
                                                         const cairo_rectangle_int_t  *bounds);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_background          (GtkSnapshot            *snapshot,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_frame               (GtkSnapshot            *snapshot,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_focus               (GtkSnapshot            *snapshot,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_layout              (GtkSnapshot            *snapshot,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         PangoLayout            *layout);
GDK_AVAILABLE_IN_3_90 /* in gtkstylecontext.c */
void            gtk_snapshot_render_insertion_cursor    (GtkSnapshot            *snapshot,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         PangoLayout            *layout,
                                                         int                     index,
                                                         PangoDirection          direction);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_icon                (GtkSnapshot            *snapshot,
                                                         GtkStyleContext        *context,
                                                         GdkPixbuf              *pixbuf,
                                                         gdouble                 x,
                                                         gdouble                 y);

G_END_DECLS

#endif /* __GTK_SNAPSHOT_H__ */
