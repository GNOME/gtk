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
#include "gtk/svg/gtksvgprivate.h"
#include "gtk/svg/gtksvgutilsprivate.h"
#include <stdint.h>

typedef enum
{
  GTK_4_0,
  GTK_4_20,
  GTK_4_22,
} GtkCompatibility;

#define PATH_PAINTABLE_TYPE (path_paintable_get_type ())
G_DECLARE_FINAL_TYPE (PathPaintable, path_paintable, PATH, PAINTABLE, GObject)


PathPaintable * path_paintable_new                 (void);
PathPaintable * path_paintable_new_from_bytes      (GBytes         *bytes,
                                                    GError        **error);
PathPaintable * path_paintable_new_from_resource   (const char     *resource);
void            path_paintable_set_svg             (PathPaintable  *self,
                                                    GtkSvg         *svg);

PathPaintable * path_paintable_copy                (PathPaintable   *self);

void            path_paintable_set_state           (PathPaintable   *self,
                                                    unsigned int     state);
unsigned int    path_paintable_get_state           (PathPaintable   *self);

void            path_paintable_set_weight          (PathPaintable   *self,
                                                    double           weight);

GskPath *       path_paintable_get_path_by_id      (PathPaintable   *self,
                                                    const char      *id);

void            path_paintable_set_path_states_by_id
                                                   (PathPaintable   *self,
                                                    const char      *id,
                                                    uint64_t         states);

void            path_paintable_get_attach_path_for_shape
                                                   (PathPaintable  *self,
                                                    SvgElement          *shape,
                                                    SvgElement         **to,
                                                    double         *pos);

GtkCompatibility
                path_paintable_get_compatibility   (PathPaintable   *self);

SvgElement *         path_paintable_get_shape_by_id     (PathPaintable   *self,
                                                    const char      *id);

void            path_paintable_changed             (PathPaintable   *self);
void            path_paintable_paths_changed       (PathPaintable   *self);

void            shape_set_default_attrs            (SvgElement *shape);

GdkPaintable *  shape_get_path_image               (SvgElement  *shape,
                                                    GtkSvg *svg);

GtkIconPaintable *
                path_paintable_get_icon_paintable  (PathPaintable *self);

void            path_paintable_set_playing         (PathPaintable *self,
                                                    gboolean       playing);
gboolean        path_paintable_get_playing         (PathPaintable *self);

void            path_paintable_set_frame_clock     (PathPaintable *self,
                                                    GdkFrameClock *clock);

GtkSvg *        path_paintable_get_svg             (PathPaintable *self);

char *          path_paintable_find_unused_id      (PathPaintable *self,
                                                    const char    *prefix);

SvgValue *      ref_value (SvgElement *shape,
                           SvgProperty attr);
