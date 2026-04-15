/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#pragma once

#include <glib.h>
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include "gsk/gskpath.h"
#include "gsk/gskpathmeasure.h"

G_BEGIN_DECLS

typedef enum
{
  ANIMATION_TYPE_SET,
  ANIMATION_TYPE_ANIMATE,
  ANIMATION_TYPE_MOTION,
  ANIMATION_TYPE_TRANSFORM,
} AnimationType;

typedef enum
{
  ANIMATION_FILL_FREEZE,
  ANIMATION_FILL_REMOVE,
} AnimationFill;

typedef enum
{
  ANIMATION_RESTART_ALWAYS,
  ANIMATION_RESTART_WHEN_NOT_ACTIVE,
  ANIMATION_RESTART_NEVER,
} AnimationRestart;

typedef enum
{
  ANIMATION_ADDITIVE_REPLACE,
  ANIMATION_ADDITIVE_SUM,
} AnimationAdditive;

typedef enum
{
  ANIMATION_ACCUMULATE_NONE,
  ANIMATION_ACCUMULATE_SUM,
} AnimationAccumulate;

typedef enum
{
  CALC_MODE_DISCRETE,
  CALC_MODE_LINEAR,
  CALC_MODE_PACED,
  CALC_MODE_SPLINE,
} CalcMode;

typedef enum
{
  ROTATE_AUTO,
  ROTATE_AUTO_REVERSE,
  ROTATE_FIXED,
} AnimationRotate;

typedef enum
{
  ANIMATION_STATUS_INACTIVE,
  ANIMATION_STATUS_RUNNING,
  ANIMATION_STATUS_DONE,
} AnimationStatus;

typedef struct
{
  SvgValue *value;
  double time;
  double point;
  double params[4];
} Frame;

typedef struct
{
  int64_t begin;
  int64_t end;
} Activation;

struct _SvgAnimation
{
  AnimationType type;
  AnimationStatus status;
  char *id;
  char *href;
  size_t line; /* for resolving ties in order */

  /* shape, attr, and idx together identify the attribute
   * that this animation modifies. idx is only relevant
   * for gradients and filters, where it identifies the
   * index of the color stop or filter primitive. It is
   * shifted by one, so we can use the value use for the
   * shapes' own attributes without clashes.
   */
  SvgElement *shape;
  unsigned int attr;
  unsigned int idx;

  unsigned int has_simple_duration : 1;
  unsigned int has_repeat_count    : 1;
  unsigned int has_repeat_duration : 1;
  unsigned int has_begin           : 1;
  unsigned int has_end             : 1;

  GPtrArray *begin;
  GPtrArray *end;

  Activation current;
  Activation previous;

  int64_t simple_duration;
  double repeat_count;
  int64_t repeat_duration;
  ColorInterpolation color_interpolation;

  GtkSvgRunMode run_mode;
  int64_t next_invalidate;
  gboolean state_changed;


  AnimationFill fill;
  AnimationRestart restart;
  AnimationAdditive additive;
  AnimationAccumulate accumulate;

  CalcMode calc_mode;
  Frame *frames;
  unsigned int n_frames;

  struct {
    char *path_ref;
    SvgElement *path_shape;
    GskPath *path;
    GskPathMeasure *measure;
    AnimationRotate rotate;
    double angle;
  } motion;

  GPtrArray *deps;

  struct {
    unsigned int transition;
    unsigned int animation;
    unsigned int easing;
    double origin;
    double segment;
    double attach_pos;
  } gpa;
};

SvgAnimation *   svg_animation_new           (AnimationType  type);

void             svg_animation_free          (SvgAnimation  *animation);
void             svg_animation_drop_and_free (SvgAnimation  *animation);

gboolean         svg_animation_equal         (SvgAnimation  *animation1,
                                              SvgAnimation  *animation2);

gboolean         svg_animation_has_begin     (SvgAnimation  *animation,
                                              TimeSpec      *spec);
gboolean         svg_animation_has_end       (SvgAnimation  *animation,
                                              TimeSpec      *spec);

TimeSpec *       svg_animation_add_begin     (SvgAnimation  *animation,
                                              TimeSpec      *spec);
TimeSpec *       svg_animation_add_end       (SvgAnimation  *animation,
                                              TimeSpec      *spec);

int64_t          svg_animation_get_current_begin (SvgAnimation *animation);
int64_t          svg_animation_get_current_end   (SvgAnimation *animation);

void             svg_animation_update_for_pause
                                             (SvgAnimation  *animation,
                                              int64_t        duration);

GskPath *        svg_animation_motion_get_base_path
                                             (SvgAnimation          *animation,
                                              const graphene_rect_t *viewport);
GskPath *        svg_animation_motion_get_current_path
                                             (SvgAnimation          *animation,
                                              const graphene_rect_t *viewport);
GskPathMeasure * svg_animation_motion_get_current_measure
                                             (SvgAnimation          *animation,
                                              const graphene_rect_t *viewport);

void             svg_animation_add_dep       (SvgAnimation  *base,
                                              SvgAnimation  *anim);

CalcMode         svg_animation_type_default_calc_mode
                                             (AnimationType  type);

void             svg_animation_fill_from_values
                                             (SvgAnimation  *animation,
                                              double        *times,
                                              unsigned int   n_frames,
                                              SvgValue     **values,
                                              double        *params,
                                              double        *points);

void             svg_animation_motion_fill_from_path
                                             (SvgAnimation  *animation,
                                              GskPath       *path);

SvgAnimation *   svg_animation_clone (SvgAnimation *animation,
                                      SvgElement   *parent,
                                      Timeline     *timeline);

void            animation_update_for_spec (SvgAnimation *animation,
                                           TimeSpec     *spec);
void            animation_set_begin       (SvgAnimation *a,
                                          int64_t       current_time);

gboolean        animation_set_current_end (SvgAnimation *a,
                                           int64_t       time);

int64_t         determine_repeat_duration (SvgAnimation *a);
int64_t         determine_simple_duration (SvgAnimation *a);


G_END_DECLS
