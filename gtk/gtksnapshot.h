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

typedef struct _GtkSnapshot GtkSnapshot;


GDK_AVAILABLE_IN_3_90
GskRenderer *   gtk_snapshot_get_renderer               (const GtkSnapshot      *state);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push                       (GtkSnapshot            *state,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF(3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_push_node                 (GtkSnapshot            *state,
                                                         GskRenderNode          *node);
GDK_AVAILABLE_IN_3_90
cairo_t *       gtk_snapshot_push_cairo_node            (GtkSnapshot            *state,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF(3, 4);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_pop                        (GtkSnapshot            *state);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_set_transform              (GtkSnapshot            *state,
                                                         const graphene_matrix_t *transform);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_transform                  (GtkSnapshot            *state,
                                                         const graphene_matrix_t *matrix);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_translate_2d               (GtkSnapshot            *state,
                                                         int                     x,
                                                         int                     y);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_append_node                (GtkSnapshot            *state,
                                                         GskRenderNode          *node);
GDK_AVAILABLE_IN_3_90
cairo_t *       gtk_snapshot_append_cairo_node          (GtkSnapshot            *state,
                                                         const graphene_rect_t  *bounds,
                                                         const char             *name,
                                                         ...) G_GNUC_PRINTF(3, 4);

GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_background          (GtkSnapshot            *state,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);
GDK_AVAILABLE_IN_3_90
void            gtk_snapshot_render_frame               (GtkSnapshot            *state,
                                                         GtkStyleContext        *context,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);

G_END_DECLS

#endif /* __GTK_SNAPSHOT_H__ */
