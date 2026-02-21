/*
 * Copyright © 2025 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>
#include "gtk/gtksvgprivate.h"
#include <stdint.h>

typedef enum
{
  GTK_4_0,
  GTK_4_20,
  GTK_4_22,
} GtkCompatibility;

#define NO_STATES 0
#define ALL_STATES G_MAXUINT64
#define STATE_UNSET ((unsigned int) -1)


#define PATH_PAINTABLE_TYPE (path_paintable_get_type ())
G_DECLARE_FINAL_TYPE (PathPaintable, path_paintable, PATH, PAINTABLE, GObject)


PathPaintable * path_paintable_new                 (void);
PathPaintable * path_paintable_new_from_bytes      (GBytes         *bytes,
                                                    GError        **error);
PathPaintable * path_paintable_new_from_resource   (const char     *resource);

GBytes *        path_paintable_serialize           (PathPaintable  *self,
                                                    unsigned int    initial_state);

GBytes *        path_paintable_serialize_as_svg    (PathPaintable  *self);

PathPaintable * path_paintable_copy                (PathPaintable   *self);

gboolean        path_paintable_equal               (PathPaintable   *self,
                                                    PathPaintable   *other);

void            path_paintable_set_state           (PathPaintable   *self,
                                                    unsigned int     state);
unsigned int    path_paintable_get_state           (PathPaintable   *self);

unsigned int    path_paintable_get_n_states        (PathPaintable   *self);

void            path_paintable_set_weight          (PathPaintable   *self,
                                                    double           weight);
double          path_paintable_get_weight          (PathPaintable   *self);

void            path_paintable_set_size            (PathPaintable   *self,
                                                    double           width,
                                                    double           height);
double          path_paintable_get_width           (PathPaintable   *self);
double          path_paintable_get_height          (PathPaintable   *self);

void            path_paintable_set_keywords        (PathPaintable   *self,
                                                    const char      *keywords);
const char *    path_paintable_get_keywords        (PathPaintable   *self);
void            path_paintable_set_description     (PathPaintable   *self,
                                                    const char      *description);
const char *    path_paintable_get_description     (PathPaintable   *self);
void            path_paintable_set_author          (PathPaintable   *self,
                                                    const char      *author);
const char *    path_paintable_get_author          (PathPaintable   *self);
void            path_paintable_set_license         (PathPaintable   *self,
                                                    const char      *license);
const char *    path_paintable_get_license         (PathPaintable   *self);

size_t          path_paintable_get_n_paths         (PathPaintable   *self);

size_t          path_paintable_add_path            (PathPaintable   *self,
                                                    GskPath         *path);
size_t          path_paintable_add_shape           (PathPaintable   *self,
                                                    ShapeType        shape_type,
                                                    double          *params,
                                                    unsigned int     n_params);
GskPath *       path_paintable_get_path            (PathPaintable   *self,
                                                    size_t           idx);

GskPath *       path_paintable_get_path_by_id      (PathPaintable   *self,
                                                    const char      *id);

const char *    path_paintable_get_path_id         (PathPaintable   *self,
                                                    size_t           idx);

void            path_paintable_set_path_states     (PathPaintable   *self,
                                                    size_t           idx,
                                                    uint64_t         states);
uint64_t        path_paintable_get_path_states     (PathPaintable   *self,
                                                    size_t           idx);

double          path_paintable_get_path_origin     (PathPaintable   *self,
                                                    size_t           idx);

void            path_paintable_set_path_fill       (PathPaintable   *self,
                                                    size_t           idx,
                                                    gboolean         do_fill,
                                                    GskFillRule      rule,
                                                    unsigned int     symbolic,
                                                    const GdkRGBA   *color);
gboolean        path_paintable_get_path_fill       (PathPaintable   *self,
                                                    size_t           idx,
                                                    GskFillRule     *rule,
                                                    unsigned int    *symbolic,
                                                    GdkRGBA         *color);

void            path_paintable_set_path_stroke     (PathPaintable   *self,
                                                    size_t           idx,
                                                    gboolean         do_stroke,
                                                    GskStroke       *stroke,
                                                    unsigned int     symbolic,
                                                    const GdkRGBA   *color);
gboolean        path_paintable_get_path_stroke     (PathPaintable   *self,
                                                    size_t           idx,
                                                    GskStroke       *stroke,
                                                    unsigned int    *symbolic,
                                                    GdkRGBA         *color);

void            path_paintable_get_attach_path     (PathPaintable   *self,
                                                    size_t           idx,
                                                    size_t          *to,
                                                    double          *pos);

GtkCompatibility
                path_paintable_get_compatibility   (PathPaintable   *self);

Shape *         path_paintable_get_shape           (PathPaintable   *self,
                                                    size_t           idx);

Shape *         path_paintable_get_shape_by_id     (PathPaintable   *self,
                                                    const char      *id);

Shape *         path_paintable_get_content         (PathPaintable   *self);

const graphene_rect_t *
                path_paintable_get_viewport        (PathPaintable   *self);

void            path_paintable_changed             (PathPaintable   *self);
void            path_paintable_paths_changed       (PathPaintable   *self);

Shape *         shape_duplicate                    (Shape *shape);
gboolean        shape_is_graphical                 (Shape *shape);

GtkIconPaintable *
                path_paintable_get_icon_paintable  (PathPaintable *self);

void            path_paintable_set_playing         (PathPaintable *self,
                                                    gboolean       playing);
gboolean        path_paintable_get_playing         (PathPaintable *self);

void            path_paintable_set_frame_clock     (PathPaintable *self,
                                                    GdkFrameClock *clock);
