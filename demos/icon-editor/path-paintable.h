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

typedef enum
{
  TRANSITION_TYPE_NONE,
  TRANSITION_TYPE_ANIMATE,
  TRANSITION_TYPE_BLUR,
  TRANSITION_TYPE_FADE,
} TransitionType;

typedef enum
{
  EASING_FUNCTION_LINEAR,
  EASING_FUNCTION_EASE_IN_OUT,
  EASING_FUNCTION_EASE_IN,
  EASING_FUNCTION_EASE_OUT,
  EASING_FUNCTION_EASE,
} EasingFunction;

typedef enum
{
  ANIMATION_TYPE_NONE,
  ANIMATION_TYPE_AUTOMATIC,
} AnimationType;

typedef enum
{
  ANIMATION_DIRECTION_NORMAL,
  ANIMATION_DIRECTION_ALTERNATE,
  ANIMATION_DIRECTION_REVERSE,
  ANIMATION_DIRECTION_REVERSE_ALTERNATE,
  ANIMATION_DIRECTION_IN_OUT,
  ANIMATION_DIRECTION_IN_OUT_ALTERNATE,
  ANIMATION_DIRECTION_IN_OUT_REVERSE,
  ANIMATION_DIRECTION_SEGMENT,
  ANIMATION_DIRECTION_SEGMENT_ALTERNATE,
} AnimationDirection;

typedef enum
{
  GTK_4_0,
  GTK_4_20,
  GTK_4_22,
} GtkCompatibility;

#define NO_STATES 0
#define ALL_STATES 0xffffffff
#define STATE_UNSET ((guint) -1)


#define PATH_PAINTABLE_TYPE (path_paintable_get_type ())
G_DECLARE_FINAL_TYPE (PathPaintable, path_paintable, PATH, PAINTABLE, GObject)


PathPaintable * path_paintable_new            (void);
PathPaintable * path_paintable_new_from_bytes (GBytes         *bytes,
                                               GError        **error);
PathPaintable * path_paintable_new_from_resource (const char  *path);

GBytes *        path_paintable_serialize      (PathPaintable  *self);

gboolean        path_paintable_equal          (PathPaintable  *self,
                                               PathPaintable  *other);

void            path_paintable_set_state      (PathPaintable  *self,
                                               guint           state);
guint           path_paintable_get_state      (PathPaintable  *self);

guint           path_paintable_get_max_state  (PathPaintable  *self);

void            path_paintable_set_weight          (PathPaintable   *self,
                                                    float            weight);
float           path_paintable_get_weight          (PathPaintable   *self);

void            path_paintable_set_size            (PathPaintable   *self,
                                                    double           width,
                                                    double           height);
double          path_paintable_get_width           (PathPaintable   *self);
double          path_paintable_get_height          (PathPaintable   *self);

void            path_paintable_set_keywords        (PathPaintable   *self,
                                                    GStrv            keywords);
GStrv           path_paintable_get_keywords        (PathPaintable   *self);

gsize           path_paintable_get_n_paths         (PathPaintable   *self);

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

void            path_paintable_set_path_states     (PathPaintable   *self,
                                                    gsize            idx,
                                                    guint64          states);
guint64         path_paintable_get_path_states     (PathPaintable   *self,
                                                    gsize            idx);

void            path_paintable_set_path_animation  (PathPaintable     *self,
                                                    gsize              idx,
                                                    AnimationType      type,
                                                    AnimationDirection direction,
                                                    float              duration,
                                                    EasingFunction     easing);
AnimationType
                path_paintable_get_path_animation_type
                                                   (PathPaintable     *self,
                                                    gsize              idx);
AnimationDirection
                path_paintable_get_path_animation_direction
                                                   (PathPaintable     *self,
                                                    gsize              idx);
float           path_paintable_get_path_animation_duration
                                                   (PathPaintable     *self,
                                                    gsize              idx);
EasingFunction  path_paintable_get_path_animation_easing
                                                   (PathPaintable     *self,
                                                    gsize              idx);

void            path_paintable_set_path_transition (PathPaintable   *self,
                                                    gsize            idx,
                                                    TransitionType  transition,
                                                    float            duration,
                                                    EasingFunction   easing);
TransitionType
                path_paintable_get_path_transition_type
                                                   (PathPaintable   *self,
                                                    gsize            idx);
float           path_paintable_get_path_transition_duration
                                                   (PathPaintable   *self,
                                                    gsize            idx);
EasingFunction  path_paintable_get_path_transition_easing
                                                   (PathPaintable   *self,
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

void            path_paintable_set_path_stroke_variation (PathPaintable   *self,
                                                          gsize            idx,
                                                          float            min_stroke_width,
                                                          float            max_stroke_width);
void            path_paintable_get_path_stroke_variation (PathPaintable   *self,
                                                          gsize            idx,
                                                          float           *min_stroke_width,
                                                          float           *max_stroke_width);

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

GtkCompatibility path_paintable_get_compatibility (PathPaintable *self);

char *   states_to_string (guint64     states);
gboolean states_parse     (const char *text,
                           guint64     default_value,
                           guint64    *states);

gboolean origin_parse     (const char *text,
                           float      *origin);

gboolean parse_symbolic_svg (PathPaintable  *self,
                             GBytes         *bytes,
                             GError        **error);
