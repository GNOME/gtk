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
#include <stdint.h>

typedef enum
{
  TRANSITION_TYPE_NONE,
  TRANSITION_TYPE_ANIMATE,
  TRANSITION_TYPE_MORPH,
  TRANSITION_TYPE_FADE,
} TransitionType;

typedef enum
{
  EASING_FUNCTION_LINEAR,
  EASING_FUNCTION_EASE_IN_OUT,
  EASING_FUNCTION_EASE_IN,
  EASING_FUNCTION_EASE_OUT,
  EASING_FUNCTION_EASE,
  EASING_FUNCTION_CUSTOM,
} EasingFunction;

typedef enum
{
  CALC_MODE_LINEAR,
  CALC_MODE_DISCRETE,
  CALC_MODE_SPLINE,
} CalcMode;

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
#define STATE_UNSET ((unsigned int) -1)


#define PATH_PAINTABLE_TYPE (path_paintable_get_type ())
G_DECLARE_FINAL_TYPE (PathPaintable, path_paintable, PATH, PAINTABLE, GObject)


PathPaintable * path_paintable_new                 (void);
PathPaintable * path_paintable_new_from_bytes      (GBytes         *bytes,
                                                    GError        **error);
PathPaintable * path_paintable_new_from_resource   (const char     *resource);

GBytes *        path_paintable_serialize           (PathPaintable  *self,
                                                    unsigned int    initial_state);

PathPaintable * path_paintable_copy                (PathPaintable   *self);

PathPaintable * path_paintable_combine             (PathPaintable   *one,
                                                    PathPaintable   *two);

gboolean        path_paintable_equal               (PathPaintable   *self,
                                                    PathPaintable   *other);

void            path_paintable_set_state           (PathPaintable   *self,
                                                    unsigned int     state);
unsigned int    path_paintable_get_state           (PathPaintable   *self);

unsigned int    path_paintable_get_max_state       (PathPaintable   *self);

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

size_t          path_paintable_get_n_paths         (PathPaintable   *self);

typedef enum
{
  SHAPE_CIRCLE,
  SHAPE_RECT,
  SHAPE_PATH,
} ShapeType;

size_t          path_paintable_add_path            (PathPaintable   *self,
                                                    GskPath         *path,
                                                    ShapeType        shape_type,
                                                    float            shape_params[6]);
void            path_paintable_delete_path         (PathPaintable   *self,
                                                    size_t           idx);
void            path_paintable_move_path           (PathPaintable   *self,
                                                    size_t           idx,
                                                    size_t           new_idx);
void            path_paintable_duplicate_path      (PathPaintable   *self,
                                                    gsize            idx);
void            path_paintable_set_path            (PathPaintable   *self,
                                                    size_t           idx,
                                                    GskPath         *path);
GskPath *       path_paintable_get_path            (PathPaintable   *self,
                                                    size_t           idx);

gboolean        path_paintable_set_path_id         (PathPaintable   *self,
                                                    size_t           idx,
                                                    const char      *id);
const char *    path_paintable_get_path_id         (PathPaintable   *self,
                                                    size_t           idx);

void            path_paintable_set_path_states     (PathPaintable   *self,
                                                    size_t           idx,
                                                    uint64_t         states);
uint64_t        path_paintable_get_path_states     (PathPaintable   *self,
                                                    size_t           idx);

void            path_paintable_set_path_animation  (PathPaintable     *self,
                                                    size_t             idx,
                                                    AnimationType      type,
                                                    AnimationDirection direction,
                                                    float              duration,
                                                    float              repeat,
                                                    EasingFunction     easing,
                                                    float              segment);
AnimationType
                path_paintable_get_path_animation_type
                                                   (PathPaintable     *self,
                                                    size_t             idx);
AnimationDirection
                path_paintable_get_path_animation_direction
                                                   (PathPaintable     *self,
                                                    size_t             idx);
float           path_paintable_get_path_animation_duration
                                                   (PathPaintable     *self,
                                                    size_t             idx);
float           path_paintable_get_path_animation_repeat
                                                   (PathPaintable     *self,
                                                    size_t             idx);
EasingFunction  path_paintable_get_path_animation_easing
                                                   (PathPaintable     *self,
                                                    size_t             idx);
float           path_paintable_get_path_animation_segment
                                                   (PathPaintable     *self,
                                                    size_t             idx);
typedef struct
{
  float time;
  float value;
  float params[4];
} KeyFrame;

void            path_paintable_set_path_animation_timing
                                                   (PathPaintable     *self,
                                                    size_t             idx,
                                                    EasingFunction     easing,
                                                    CalcMode           mode,
                                                    const KeyFrame    *frames,
                                                    unsigned int       n_frames);
CalcMode        path_paintable_get_path_animation_mode
                                                   (PathPaintable     *self,
                                                    size_t             idx);
unsigned int    path_paintable_get_path_animation_n_frames
                                                   (PathPaintable     *self,
                                                    size_t             idx);
const KeyFrame *path_paintable_get_path_animation_frames
                                                   (PathPaintable     *self,
                                                    size_t             idx);

void            path_paintable_set_path_transition (PathPaintable   *self,
                                                    size_t           idx,
                                                    TransitionType  transition,
                                                    float            duration,
                                                    float            delay,
                                                    EasingFunction   easing);
TransitionType  path_paintable_get_path_transition_type
                                                   (PathPaintable   *self,
                                                    size_t           idx);
float           path_paintable_get_path_transition_duration
                                                   (PathPaintable   *self,
                                                    size_t           idx);
float           path_paintable_get_path_transition_delay
                                                   (PathPaintable   *self,
                                                    size_t           idx);
EasingFunction  path_paintable_get_path_transition_easing
                                                   (PathPaintable   *self,
                                                    size_t           idx);

void            path_paintable_set_path_origin     (PathPaintable   *self,
                                                    size_t           idx,
                                                    float            origin);
float           path_paintable_get_path_origin     (PathPaintable   *self,
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

void            path_paintable_set_path_stroke_variation
                                                   (PathPaintable   *self,
                                                    size_t           idx,
                                                    float            min_width,
                                                    float            max_width);
void            path_paintable_get_path_stroke_variation
                                                   (PathPaintable   *self,
                                                    size_t           idx,
                                                    float           *min_width,
                                                    float           *max_width);

void            path_paintable_attach_path         (PathPaintable   *self,
                                                    size_t           idx,
                                                    size_t           to,
                                                    float            pos);
void            path_paintable_get_attach_path     (PathPaintable   *self,
                                                    size_t           idx,
                                                    size_t          *to,
                                                    float           *pos);

GtkCompatibility
                path_paintable_get_compatibility   (PathPaintable   *self);
