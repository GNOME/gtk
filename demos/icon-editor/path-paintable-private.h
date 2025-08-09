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

#include "path-paintable.h"

typedef enum
{
  STATE_TRANSITION_NONE,
  STATE_TRANSITION_ANIMATE,
  STATE_TRANSITION_BLUR,
} StateTransition;

typedef enum
{
  EASING_FUNCTION_LINEAR,
  EASING_FUNCTION_EASE_IN_OUT,
  EASING_FUNCTION_EASE_IN,
  EASING_FUNCTION_EASE_OUT,
  EASING_FUNCTION_EASE,
} EasingFunction;

#define ALL_STATES 0xffffffff

PathPaintable * path_paintable_new                 (void);

void            path_paintable_set_size            (PathPaintable   *self,
                                                    double           width,
                                                    double           height);
double          path_paintable_get_width           (PathPaintable   *self);
double          path_paintable_get_height          (PathPaintable   *self);

void            path_paintable_set_duration        (PathPaintable   *self,
                                                    float            duration);
float           path_paintable_get_duration        (PathPaintable   *self);

void            path_paintable_set_delay           (PathPaintable   *self,
                                                    float            delay);
float           path_paintable_get_delay           (PathPaintable   *self);

void            path_paintable_set_easing          (PathPaintable   *self,
                                                    EasingFunction   func);
EasingFunction  path_paintable_get_easing          (PathPaintable   *self);

guint           path_paintable_get_state           (PathPaintable   *self);

gsize           path_paintable_add_path            (PathPaintable   *self,
                                                    GskPath         *path);
void            path_paintable_delete_path         (PathPaintable   *self,
                                                    gsize            idx);
void            path_paintable_move_path           (PathPaintable   *self,
                                                    gsize            idx,
                                                    gsize            new_idx);
void            path_paintable_set_path            (PathPaintable   *self,
                                                    gsize            idx,
                                                    GskPath         *path);
GskPath *       path_paintable_get_path            (PathPaintable   *self,
                                                    gsize            idx);

gsize           path_paintable_get_n_paths         (PathPaintable   *self);

void            path_paintable_set_path_states     (PathPaintable   *self,
                                                    gsize            idx,
                                                    guint64          states);
guint64         path_paintable_get_path_states     (PathPaintable   *self,
                                                    gsize            idx);

void            path_paintable_set_path_transition (PathPaintable   *self,
                                                    gsize            idx,
                                                    StateTransition  transition);
StateTransition
                path_paintable_get_path_transition (PathPaintable   *self,
                                                    gsize            idx);

void            path_paintable_set_path_origin     (PathPaintable   *self,
                                                    gsize            idx,
                                                    float            origin);
float           path_paintable_get_path_origin     (PathPaintable   *self,
                                                    gsize            idx);

void            path_paintable_set_path_fill       (PathPaintable   *self,
                                                    gsize            idx,
                                                    gboolean         do_fill,
                                                    GskFillRule      rule,
                                                    guint            symbolic,
                                                    const GdkRGBA   *color);
gboolean        path_paintable_get_path_fill       (PathPaintable   *self,
                                                    gsize            idx,
                                                    GskFillRule     *rule,
                                                    guint           *symbolic,
                                                    GdkRGBA         *color);

void            path_paintable_set_path_stroke     (PathPaintable   *self,
                                                    gsize            idx,
                                                    gboolean         do_stroke,
                                                    GskStroke       *stroke,
                                                    guint            symbolic,
                                                    const GdkRGBA   *color);
gboolean        path_paintable_get_path_stroke     (PathPaintable   *self,
                                                    gsize            idx,
                                                    GskStroke       *stroke,
                                                    guint           *symbolic,
                                                    GdkRGBA         *color);

void            path_paintable_attach_path         (PathPaintable   *self,
                                                    gsize            idx,
                                                    gsize            to,
                                                    float            pos);
void            path_paintable_get_attach_path     (PathPaintable   *self,
                                                    gsize            idx,
                                                    gsize           *to,
                                                    float           *pos);

PathPaintable * path_paintable_copy                (PathPaintable   *self);

PathPaintable * path_paintable_combine             (PathPaintable   *one,
                                                    PathPaintable   *two);

GBytes *        path_paintable_serialize_state     (PathPaintable   *self,
                                                    guint            state);


char *   states_to_string (guint64     states);
gboolean states_parse     (const char *text,
                           guint64    *states);

char *   origin_to_string (float       origin);
gboolean origin_parse     (const char *text,
                           float      *origin);

