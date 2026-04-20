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

#include "gtksvganimationprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvgtimespecprivate.h"
#include "gtksvgutilsprivate.h"

CalcMode
svg_animation_type_default_calc_mode (AnimationType type)
{
  switch ((unsigned int) type)
    {
    case ANIMATION_TYPE_SET: return CALC_MODE_DISCRETE;
    case ANIMATION_TYPE_MOTION: return CALC_MODE_PACED;
    default: return CALC_MODE_LINEAR;
    }
}

static void
svg_animation_init (SvgAnimation  *animation,
                    AnimationType  type)
{
  animation->status = ANIMATION_STATUS_INACTIVE;

  animation->type = type;
  animation->begin = NULL;
  animation->end = NULL;

  animation->simple_duration = INDEFINITE;
  animation->repeat_count = REPEAT_FOREVER;
  animation->repeat_duration = INDEFINITE;

  animation->fill = ANIMATION_FILL_REMOVE;
  animation->restart = ANIMATION_RESTART_ALWAYS;
  animation->calc_mode = svg_animation_type_default_calc_mode (type);
  animation->additive = ANIMATION_ADDITIVE_REPLACE;
  animation->accumulate = ANIMATION_ACCUMULATE_NONE;

  animation->color_interpolation = COLOR_INTERPOLATION_SRGB;

  animation->begin = g_ptr_array_new ();
  animation->end = g_ptr_array_new ();

  animation->current.begin = INDEFINITE;
  animation->current.end = INDEFINITE;
  animation->previous.begin = 0;
  animation->previous.end = 0;

  if (type == ANIMATION_TYPE_MOTION)
    {
      animation->attr = SVG_PROPERTY_TRANSFORM;
      animation->motion.rotate = ROTATE_FIXED;
      animation->motion.angle = 0;
    }
}

void
svg_animation_add_dep (SvgAnimation *base,
                       SvgAnimation *anim)
{
  if (!base->deps)
    base->deps = g_ptr_array_new ();

  g_ptr_array_add (base->deps, anim);
}

SvgAnimation *
svg_animation_new (AnimationType type)
{
  SvgAnimation *animation = g_new0 (SvgAnimation, 1);
  svg_animation_init (animation, type);
  return animation;
}

void
svg_animation_free (SvgAnimation *animation)
{
  g_free (animation->id);
  g_free (animation->href);

  g_clear_pointer (&animation->begin, g_ptr_array_unref);
  g_clear_pointer (&animation->end, g_ptr_array_unref);

  if (animation->type != ANIMATION_TYPE_MOTION)
    {
      for (unsigned int i = 0; i < animation->n_frames; i++)
        svg_value_unref (animation->frames[i].value);
    }
  g_free (animation->frames);

  g_free (animation->motion.path_ref);
  g_clear_pointer (&animation->motion.path, gsk_path_unref);
  g_clear_pointer (&animation->motion.measure, gsk_path_measure_unref);

  g_clear_pointer (&animation->deps, g_ptr_array_unref);

  g_free (animation);
}

void
svg_animation_drop_and_free (SvgAnimation *animation)
{
  for (unsigned int k = 0; k < 2; k++)
    {
      GPtrArray *specs = k == 0 ? animation->begin : animation->end;
      for (unsigned int i = 0; i < specs->len; i++)
        {
          TimeSpec *spec = g_ptr_array_index (specs, i);
          time_spec_drop_animation (spec, animation);
        }
    }
  svg_animation_free (animation);
}

TimeSpec *
svg_animation_add_begin (SvgAnimation *animation,
                         TimeSpec     *spec)
{
  g_ptr_array_add (animation->begin, spec);
  return spec;
}

TimeSpec *
svg_animation_add_end (SvgAnimation *animation,
                       TimeSpec     *spec)
{
  g_ptr_array_add (animation->end, spec);
  return spec;
}

gboolean
svg_animation_has_begin (SvgAnimation *animation,
                         TimeSpec     *spec)
{
  return g_ptr_array_find (animation->begin, spec, NULL);
}

gboolean
svg_animation_has_end (SvgAnimation *animation,
                       TimeSpec     *spec)
{
  return g_ptr_array_find (animation->end, spec, NULL);
}

void
svg_animation_fill_from_values (SvgAnimation  *animation,
                                double        *times,
                                unsigned int   n_frames,
                                SvgValue     **values,
                                double        *params,
                                double        *points)
{
  double linear[] = { 0, 0, 1, 1 };

  g_assert (n_frames > 1);

  animation->n_frames = n_frames;
  animation->frames = g_new0 (Frame, n_frames);

  for (unsigned int i = 0; i < n_frames; i++)
    {
      animation->frames[i].time = times[i];
      if (values)
        animation->frames[i].value = svg_value_ref (values[i]);
      else
        animation->frames[i].value = NULL;
      if (i + 1 < n_frames && params)
        memcpy (animation->frames[i].params, &params[4 * i], sizeof (double) * 4);
      else
        memcpy (animation->frames[i].params, linear, sizeof (double) * 4);
      if (points)
        animation->frames[i].point = points[i];
      else
        animation->frames[i].point = i / (double) (n_frames - 1);
    }
}

void
svg_animation_motion_fill_from_path (SvgAnimation *animation,
                                     GskPath      *path)
{
  GArray *frames;
  Frame frame = { .value = NULL, .params = { 0, 0, 1, 1 } };
  Frame *last;
  GskPathPoint point;
  GskPathMeasure *measure;
  double length;
  double distance, previous_distance;

  g_assert (animation->type == ANIMATION_TYPE_MOTION);

  if (!gsk_path_get_start_point (path, &point))
    return;

  measure = gsk_path_measure_new (path);

  frames = g_array_new (FALSE, FALSE, sizeof (Frame));

  frame.time = 0;
  frame.point = 0;
  g_array_append_val (frames, frame);

  length = gsk_path_measure_get_length (measure);

  previous_distance = 0;
  while (gsk_path_get_next (path, &point))
    {
      distance = gsk_path_point_get_distance (&point, measure);
      frame.point = distance / length;
      frame.time += (distance - previous_distance) / length;
      g_array_append_val (frames, frame);
      previous_distance = distance;
    }

  last = &g_array_index (frames, Frame, frames->len - 1);
  last->point = 1;
  last->time = 1;

  gsk_path_measure_unref (measure);

  animation->n_frames = frames->len;
  animation->frames = (Frame *) g_array_free (frames, FALSE);
}

static GskPath *
animation_motion_get_path (SvgAnimation          *animation,
                           const graphene_rect_t *viewport,
                           gboolean               current)
{
  g_assert (animation->type == ANIMATION_TYPE_MOTION);

  if (animation->motion.path_shape)
    return svg_element_get_path (animation->motion.path_shape, viewport, current);
  else if (animation->motion.path)
    return gsk_path_ref (animation->motion.path);
  else
    return NULL;
}

GskPath *
svg_animation_motion_get_base_path (SvgAnimation          *animation,
                                    const graphene_rect_t *viewport)
{
  return animation_motion_get_path (animation, viewport, FALSE);
}

GskPath *
svg_animation_motion_get_current_path (SvgAnimation          *animation,
                                       const graphene_rect_t *viewport)
{
  return animation_motion_get_path (animation, viewport, TRUE);
}

GskPathMeasure *
svg_animation_motion_get_current_measure (SvgAnimation          *animation,
                                          const graphene_rect_t *viewport)
{
  g_assert (animation->type == ANIMATION_TYPE_MOTION);

  if (animation->motion.path_shape)
    {
      return svg_element_get_current_measure (animation->motion.path_shape, viewport);
    }
  else if (animation->motion.path)
    {
      if (!animation->motion.measure)
        animation->motion.measure = gsk_path_measure_new (animation->motion.path);
      return gsk_path_measure_ref (animation->motion.measure);
    }
  else
    {
      return NULL;
    }
}

void
svg_animation_update_for_pause (SvgAnimation *animation,
                                int64_t       duration)
{
  if (animation->current.begin != INDEFINITE)
    animation->current.begin += duration;
  if (animation->current.end != INDEFINITE)
    animation->current.end += duration;
  if (animation->previous.begin != INDEFINITE)
    animation->previous.begin += duration;
  if (animation->previous.end != INDEFINITE)
    animation->previous.end += duration;
}

static gboolean
frame_equal (Frame *f1,
             Frame *f2)
{
  return svg_value_equal (f1->value, f2->value) &&
         f1->time == f2->time &&
         f1->point == f2->point &&
         memcmp (f1->params, f2->params, sizeof (double) * 4) == 0;
}

gboolean
svg_animation_equal (SvgAnimation *animation1,
                     SvgAnimation *animation2)
{
  if (animation1->type != animation2->type ||
      g_strcmp0 (animation1->id, animation2->id) != 0 ||
      animation1->attr != animation2->attr ||
      animation1->simple_duration != animation2->simple_duration ||
      animation1->repeat_count != animation2->repeat_count ||
      animation1->repeat_duration != animation2->repeat_duration ||
      animation1->fill != animation2->fill ||
      animation1->restart != animation2->restart ||
      animation1->additive != animation2->additive ||
      animation1->accumulate != animation2->accumulate ||
      animation1->calc_mode != animation2->calc_mode ||
      animation1->n_frames != animation2->n_frames)
    return FALSE;

  for (unsigned int i = 0; i < animation1->n_frames; i++)
    {
      if (!frame_equal (&animation1->frames[i], &animation2->frames[i]))
        return FALSE;
    }

  if (animation1->type == ANIMATION_TYPE_MOTION)
    {
      if (g_strcmp0 (animation1->motion.path_ref, animation2->motion.path_ref) != 0 ||
          !(animation1->motion.path == animation2->motion.path ||
            (animation1->motion.path && animation2->motion.path &&
             gsk_path_equal (animation1->motion.path, animation2->motion.path))) ||
          animation1->motion.rotate != animation2->motion.rotate ||
          animation1->motion.angle != animation2->motion.angle)
        return FALSE;
    }

  return TRUE;
}

SvgAnimation *
svg_animation_clone (SvgAnimation *a,
                     SvgElement   *parent,
                     Timeline     *timeline)
{
  SvgAnimation *clone = g_new0 (SvgAnimation, 1);

  clone->type = a->type;
  clone->status = ANIMATION_STATUS_INACTIVE;

  clone->id = NULL;
  clone->href = NULL;
  clone->line = a->line;

  clone->shape = parent;
  clone->attr = a->attr;
  clone->idx = a->idx;

  clone->has_simple_duration = a->has_simple_duration;
  clone->has_repeat_count = a->has_repeat_count;
  clone->has_repeat_duration = a->has_repeat_duration;
  clone->has_begin = a->has_begin;
  clone->has_end = a->has_end;

  clone->begin = g_ptr_array_new ();
  for (unsigned int i = 0; i < a->begin->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (a->begin, i);
      TimeSpec *p, *tmp;
      switch (spec->type)
        {
        case TIME_SPEC_TYPE_INDEFINITE:
        case TIME_SPEC_TYPE_OFFSET:
        case TIME_SPEC_TYPE_STATES:
          p = spec;
          break;
        case TIME_SPEC_TYPE_SYNC:
        case TIME_SPEC_TYPE_EVENT:
          tmp = time_spec_copy (spec);
          tmp->offset++; /* force a copy */
          p = timeline_get_time_spec (timeline, tmp);
          g_assert (p != spec);
          p->offset = spec->offset;
          time_spec_free (tmp);
          break;
        default:
          g_assert_not_reached ();
        }
      time_spec_add_animation (p, clone);
      svg_animation_add_begin (clone, p);
    }

  clone->end = g_ptr_array_new ();
  for (unsigned int i = 0; i < a->end->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (a->end, i);
      TimeSpec *p, *tmp;
      switch (spec->type)
        {
        case TIME_SPEC_TYPE_INDEFINITE:
        case TIME_SPEC_TYPE_OFFSET:
        case TIME_SPEC_TYPE_SYNC:
        case TIME_SPEC_TYPE_STATES:
          p = spec;
          break;
        case TIME_SPEC_TYPE_EVENT:
          tmp = time_spec_copy (spec);
          p = timeline_get_time_spec (timeline, tmp);
          time_spec_free (tmp);
          break;
        default:
          g_assert_not_reached ();
        }
      time_spec_add_animation (p, clone);
      svg_animation_add_end (clone, p);
    }

  clone->current.begin = INDEFINITE;
  clone->current.end = INDEFINITE;
  clone->previous.begin = 0;
  clone->previous.end = 0;

  clone->simple_duration = a->simple_duration;
  clone->repeat_count = a->repeat_count;
  clone->repeat_duration = a->repeat_duration;
  clone->color_interpolation = a->color_interpolation;

  clone->fill = a->fill;
  clone->restart = a->restart;
  clone->additive = a->additive;
  clone->accumulate = a->accumulate;

  clone->calc_mode = a->calc_mode;
  clone->frames = g_new (Frame, a->n_frames);
  memcpy (clone->frames, a->frames, sizeof (Frame) * a->n_frames);
  for (unsigned int i = 0; i < a->n_frames; i++)
    svg_value_ref (clone->frames[i].value);
  clone->n_frames = a->n_frames;

  if (a->type == ANIMATION_TYPE_MOTION)
    {
      clone->motion.path_ref = g_strdup (a->motion.path_ref);
      clone->motion.path_shape = a->motion.path_shape;
      if (a->motion.path)
        clone->motion.path = gsk_path_ref (a->motion.path);
      if (a->motion.measure)
        clone->motion.measure = gsk_path_measure_ref (a->motion.measure);
      clone->motion.rotate = a->motion.rotate;
      clone->motion.angle = a->motion.angle;
    }

  clone->deps = NULL;

  clone->gpa.transition = a->gpa.transition;
  clone->gpa.animation = a->gpa.animation;
  clone->gpa.easing = a->gpa.easing;
  clone->gpa.origin = a->gpa.origin;
  clone->gpa.segment = a->gpa.segment;
  clone->gpa.attach_pos = a->gpa.attach_pos;

  return clone;
}

static gboolean
animation_can_start (SvgAnimation *a)
{
  /* Handling restart */
  switch (a->status)
    {
    case ANIMATION_STATUS_INACTIVE:
      break;
    case ANIMATION_STATUS_RUNNING:
      if (a->restart != ANIMATION_RESTART_ALWAYS)
        return FALSE;
      break;
    case ANIMATION_STATUS_DONE:
      if (a->restart == ANIMATION_RESTART_NEVER)
        return FALSE;
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
time_specs_update_for_base (GPtrArray    *specs,
                            SvgAnimation *base)
{
  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);
      time_spec_update_for_base (spec, base);
    }
}

static gboolean
animation_set_current_end (SvgAnimation *a,
                           int64_t       time)
{
  /* FIXME take min, max into account */
  if (time < a->current.begin)
    time = a->current.begin;

  if (a->current.begin < INDEFINITE && a->repeat_duration < INDEFINITE)
    time = MIN (time, a->current.begin + a->repeat_duration);

  if (a->current.end == time)
    return FALSE;

  dbg_print ("times", "current end time of %s set to %s", a->id, format_time (time));
  a->current.end = time;
  return TRUE;
}

void
svg_animation_update_for_spec (SvgAnimation *animation,
                               TimeSpec     *spec)
{
  gboolean changed = FALSE;

  if (svg_animation_has_begin (animation, spec))
    {
      if (!animation_can_start (animation))
        {
          /* nothing to do */
        }
      else if (animation->status == ANIMATION_STATUS_RUNNING)
        {
          if (animation->current.begin < spec->time && spec->time < INDEFINITE)
            {
              dbg_print ("status", "Restarting %s at %s", animation->id, format_time (spec->time));
              animation->current.begin = spec->time;
              changed = TRUE;
            }
        }
      else
        {
          int64_t time;

          /* We need to allow starting in the past. But we don't allow
           * a post-dated start to fall into a previous activation.
           */

         time = find_first_time (animation->begin, animation->previous.end);

          if (animation->current.begin != time)
            {
              dbg_print ("times", "Current start time of %s now %s", animation->id, format_time (time));
              animation->current.begin = time;
              changed = TRUE;

              animation_set_current_end (animation, animation->current.end);
            }
        }
    }
  else if (svg_animation_has_end (animation, spec))
    {
      int64_t end = find_first_time (animation->end, animation->current.begin);

      changed = animation_set_current_end (animation, end);
    }

  if (changed && animation->deps)
    {
      dbg_print ("status", "Updating deps of %s", animation->id);
      for (unsigned int i = 0; i < animation->deps->len; i++)
        {
          SvgAnimation *dep = g_ptr_array_index (animation->deps, i);
          time_specs_update_for_base (dep->begin, animation);
          time_specs_update_for_base (dep->end, animation);
        }
    }
}

void
svg_animation_start (SvgAnimation *animation,
                     int64_t       current_time)
{
  gboolean changed = FALSE;

  if (!animation_can_start (animation))
    return;

  if (animation->status == ANIMATION_STATUS_RUNNING)
    {
      if (animation->current.begin < current_time)
        {
          dbg_print ("status", "Restarting %s at %s", animation->id, format_time (current_time));
          animation->current.begin = current_time;
          changed = TRUE;
        }
    }
  else
    {
      if (animation->current.begin != current_time)
        {
          dbg_print ("times", "current start time of %s now %s", animation->id, format_time (current_time));
          animation->current.begin = current_time;
          changed = TRUE;

          animation_set_current_end (animation, animation->current.end);
        }
    }

  if (!changed)
    return;

  if (animation->deps)
    {
      for (unsigned int i = 0; i < animation->deps->len; i++)
        {
          SvgAnimation *dep = g_ptr_array_index (animation->deps, i);
          time_specs_update_for_base (dep->begin, animation);
          time_specs_update_for_base (dep->end, animation);
        }
    }
}

int64_t
svg_animation_get_current_begin (SvgAnimation *animation)
{
  return animation->current.begin;
}

int64_t
svg_animation_get_current_end (SvgAnimation *animation)
{
  return animation->current.end;
}

static int64_t
compute_repeat_duration (SvgAnimation *a)
{
  if (a->repeat_duration < INDEFINITE)
    return a->repeat_duration;
  else if (a->simple_duration < INDEFINITE && a->repeat_count != REPEAT_FOREVER)
    return a->simple_duration * a->repeat_count;
  else if (a->current.end < INDEFINITE)
    return a->current.end - a->current.begin;
  else if (a->simple_duration < INDEFINITE)
    return a->simple_duration;

  return INDEFINITE;
}

static int64_t
compute_simple_duration (SvgAnimation *a)
{
  int64_t repeat_duration;

  if (a->simple_duration < INDEFINITE)
    return a->simple_duration;

  repeat_duration = compute_repeat_duration (a);

  if (repeat_duration < INDEFINITE && a->repeat_count != REPEAT_FOREVER)
    return (int64_t) (repeat_duration / a->repeat_count);

  return INDEFINITE;
}

int64_t
svg_animation_get_simple_duration (SvgAnimation *animation)
{
  return compute_simple_duration (animation);
}

/* {{{ Updates */

/* Determine what repetition the time falls into,
 * relative to the animations current start time.
 * Also what frame we are on, and how far into that
 * frame we are.
 *
 * Note: this assumes that the duration is finite
 * and the animation runs forever.
 */
gboolean
svg_animation_get_progress (SvgAnimation *a,
                            int64_t       time,
                            int          *out_rep,
                            unsigned int *out_frame,
                            double       *out_frame_t,
                            int64_t      *out_frame_start,
                            int64_t      *out_frame_end)
{
  int64_t start;
  int64_t simple_duration;
  double t;
  int rep;
  int64_t cycle_start, cycle_end;
  int64_t frame_start, frame_end;
  unsigned int i;

  start = a->current.begin;
  //g_assert (start < INDEFINITE);

  simple_duration = compute_simple_duration (a);
  if (simple_duration == INDEFINITE)
    simple_duration = compute_repeat_duration (a);

  if (simple_duration == INDEFINITE || simple_duration == 0)
    {
      *out_rep = 0;
      *out_frame = 0;
      *out_frame_t = 0;
      *out_frame_start = a->current.begin;
      *out_frame_end = a->current.end;
      return FALSE;
    }

  t = (time - start) / (double) simple_duration;
  rep = floor (t); /* number of completed repetitions */

  cycle_start = start + rep * simple_duration;
  cycle_end = cycle_start + simple_duration;

  frame_start = frame_end = cycle_start;
  for (i = 0; i + 1 < a->n_frames; i++)
    {
      frame_start = frame_end;
      frame_end = lerp (a->frames[i + 1].time, cycle_start, cycle_end);

      if (time < frame_end)
        break;
    }

  t = (time - frame_start) / (double) (frame_end - frame_start);

  *out_rep = rep;
  *out_frame = i;
  *out_frame_t = t;
  *out_frame_start = frame_start;
  *out_frame_end = frame_end;

  return TRUE;
}

void
svg_animation_update_run_mode (SvgAnimation *a,
                               int64_t       current_time)
{
  if (a->status == ANIMATION_STATUS_INACTIVE)
    {
      a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
      a->next_invalidate = a->current.begin;
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      int rep;
      unsigned int frame;
      double frame_t;
      int64_t frame_start, frame_end;

      if (a->type == ANIMATION_TYPE_SET)
        {
          a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
          a->next_invalidate = a->current.end;
          return;
        }

      svg_animation_get_progress (a, current_time,
                                  &rep, &frame, &frame_t, &frame_start, &frame_end);

      if (a->calc_mode == CALC_MODE_DISCRETE)
        {
          a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
          a->next_invalidate = frame_end;
        }
      else if (svg_property_discrete (a->attr))
        {
          a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
          if (frame_t < 0.5)
            a->next_invalidate = (frame_start + frame_end) / 2;
          else
            a->next_invalidate = frame_end;
        }
      else
        {
          a->run_mode = GTK_SVG_RUN_MODE_CONTINUOUS;
          a->next_invalidate = a->current.end;
        }
    }
  else if (a->current.begin < INDEFINITE && current_time <= a->current.begin)
    {
      a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
      a->next_invalidate = a->current.begin;
    }
  else
    {
      a->run_mode = GTK_SVG_RUN_MODE_STOPPED;
      a->next_invalidate = INDEFINITE;
    }
}

void
svg_animation_update_state (SvgAnimation *a,
                            int64_t       current_time)
{
  AnimationStatus status = a->status;

  if (a->status == ANIMATION_STATUS_INACTIVE ||
      a->status == ANIMATION_STATUS_DONE)
    {
      if (current_time < a->current.begin)
        ;  /* remain inactive */
      else if (current_time <= a->current.end)
        status = ANIMATION_STATUS_RUNNING;
      else
        status = ANIMATION_STATUS_DONE;
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      if (current_time >= a->current.end)
        status = ANIMATION_STATUS_DONE;
    }

  if (a->status != status)
    {
      if (a->status == ANIMATION_STATUS_RUNNING) /* Ending this activation */
        a->previous = a->current;

      a->status = status;

      if (a->status != ANIMATION_STATUS_RUNNING)
        {
          a->current.begin = find_first_time (a->begin, current_time);
          animation_set_current_end (a, find_first_time (a->end, a->current.begin));
        }

      svg_animation_update_run_mode (a, current_time);
      a->state_changed = TRUE;

#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "state"))
        {
          const char *name[] = { "INACTIVE", "RUNNING ", "DONE    " };
          GString *s = g_string_new ("");
          g_string_append_printf (s, "state of %s now %s [", a->id, name[status]);
          g_string_append_printf (s, "%s ", format_time (a->current.begin));
          g_string_append_printf (s, "%s] ", format_time (a->current.end));
          switch (a->run_mode)
            {
            case GTK_SVG_RUN_MODE_CONTINUOUS:
              g_string_append_printf (s, "--> %s\n", format_time (a->next_invalidate));
              break;
            case GTK_SVG_RUN_MODE_DISCRETE:
              g_string_append_printf (s, "> > %s\n", format_time (a->next_invalidate));
              break;
            case GTK_SVG_RUN_MODE_STOPPED:
              g_string_append (s, "\n");
              break;
            default:
              g_assert_not_reached ();
            }
          dbg_print ("state", "%s", s->str);
          g_string_free (s, TRUE);
        }
#endif
    }
  else
    svg_animation_update_run_mode (a, current_time);
}

/* }}} */
/* ((( Reference resolution */

void
svg_animation_resolve_shadow_references (SvgAnimation *animation,
                                         GHashTable   *map)
{
  SvgValue *value;
  SvgElement *clone;
  unsigned int first;

  if (animation->motion.path_shape)
    {
      clone = g_hash_table_lookup (map, animation->motion.path_shape);
      if (clone)
        animation->motion.path_shape = clone;
    }

  for (unsigned int k = 0; k < 2; k++)
    {
      GPtrArray *specs = k == 0 ? animation->begin : animation->end;
      for (unsigned int i = 0; i < specs->len; i++)
        {
          TimeSpec *spec = g_ptr_array_index (specs, i);
          if (spec->type == TIME_SPEC_TYPE_SYNC)
            {
              SvgAnimation *anim = g_hash_table_lookup (map, spec->sync.base);
              if (anim)
                spec->sync.base = anim;
              if (spec->sync.base)
                svg_animation_add_dep (spec->sync.base, animation);
            }
          else if (spec->type == TIME_SPEC_TYPE_EVENT)
            {
              clone = g_hash_table_lookup (map, spec->event.shape);
              if (clone)
                spec->event.shape = clone;
            }
        }
    }

  /* Skip the 'current' keyword in frames[0] if present */
  if (animation->frames &&
      animation->frames[0].value &&
      svg_value_is_current (animation->frames[0].value))
    first = 1;
  else
    first = 0;

  if (animation->attr == SVG_PROPERTY_CLIP_PATH)
    {
      for (unsigned int j = first; j < animation->n_frames; j++)
        {
          value = animation->frames[j].value;
          if (svg_clip_get_kind (value) == CLIP_URL)
            {
              clone = g_hash_table_lookup (map, svg_clip_get_shape (value));
              if (clone)
                {
                  animation->frames[j].value = svg_clip_new_url_take (g_strdup (svg_clip_get_id (value)));
                  svg_clip_set_shape (animation->frames[j].value, clone);
                  svg_value_unref (value);
                }
            }
        }
    }
  else if (animation->attr == SVG_PROPERTY_MASK)
    {
      for (unsigned int j = first; j < animation->n_frames; j++)
        {
          value = animation->frames[j].value;
          if (svg_mask_get_kind (value) == MASK_URL)
            {
              clone = g_hash_table_lookup (map, svg_mask_get_shape (value));
              if (clone)
                {
                  animation->frames[j].value = svg_mask_new_url_take (g_strdup (svg_mask_get_id (value)));
                  svg_mask_set_shape (animation->frames[j].value, clone);
                  svg_value_unref (value);
                }
            }
        }
    }
  else if (animation->attr == SVG_PROPERTY_HREF ||
           animation->attr == SVG_PROPERTY_FE_IMAGE_HREF ||
           animation->attr == SVG_PROPERTY_MARKER_START ||
           animation->attr == SVG_PROPERTY_MARKER_MID ||
           animation->attr == SVG_PROPERTY_MARKER_END)
    {
      for (unsigned int j = first; j < animation->n_frames; j++)
        {
          value = animation->frames[j].value;
          clone = g_hash_table_lookup (map, svg_href_get_shape (value));
          if (clone)
            {
               if (svg_href_get_kind (value) == HREF_URL)
                 animation->frames[j].value = svg_href_new_url_take (g_strdup (svg_href_get_ref (value)));
               else
                 animation->frames[j].value = svg_href_new_plain (svg_href_get_ref (value));
               svg_href_set_shape (animation->frames[j].value, clone);
               svg_value_unref (value);
            }
        }
    }
  else if (animation->attr == SVG_PROPERTY_FILL ||
           animation->attr == SVG_PROPERTY_STROKE)
    {
      for (unsigned int j = first; j < animation->n_frames; j++)
        {
          value = animation->frames[j].value;
          PaintKind kind = svg_paint_get_kind (value);
          if (paint_is_server (kind))
            {
              clone = g_hash_table_lookup (map, svg_paint_get_server_shape (value));
              if (clone)
                {
                  const char *ref = svg_paint_get_server_ref (value);
                  const GdkColor *fallback = svg_paint_get_server_fallback (value);

                  if (kind == PAINT_SERVER_WITH_FALLBACK)
                    animation->frames[j].value = svg_paint_new_server_with_fallback (ref, fallback);
                  else if (kind == PAINT_SERVER_WITH_CURRENT_COLOR)
                    animation->frames[j].value = svg_paint_new_server_with_current_color (ref);
                  else
                    animation->frames[j].value = svg_paint_new_server (ref);

                  svg_paint_set_server_shape (animation->frames[j].value, clone);
                  svg_value_unref (value);
                }
            }
        }
    }
  else if (animation->attr == SVG_PROPERTY_FILTER)
    {
      for (unsigned int j = first; j < animation->n_frames; j++)
        {
          value = animation->frames[j].value;

          animation->frames[j].value = svg_filter_functions_copy (value);
          for (unsigned int k = 0; k < svg_filter_functions_get_length (value); k++)
            {
              if (svg_filter_functions_get_kind (value, k) == FILTER_REF)
                {
                  clone = g_hash_table_lookup (map, svg_filter_functions_get_shape (value, k));
                  if (clone)
                    svg_filter_functions_set_shape (animation->frames[j].value, k, clone);
                }
            }

          svg_value_unref (value);
        }
    }
}

/* }}} */

/* vim:set foldmethod=marker: */
