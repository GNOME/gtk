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

#include "config.h"

#include "gtksvgtimespecprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvganimationprivate.h"
#include "gtksvggpaprivate.h"
#include "gtksvgstringutilsprivate.h"

/* {{{ Time specs */

/* How timing works:
 *
 * The timeline contains all TimeSpecs.
 *
 * spec->time is the resolved time for the spec (may be indefinite,
 * and may change in some cases).
 *
 * Time specs and animations are connected in a dependency tree
 * (via time specs of type 'sync').
 *
 * There are three ways in which time specs change. First, when the
 * timeline gets a start time, all 'offset' time specs get their
 * resolved time, and will never change afterwards. Second, when
 * the state changes, 'state' time specs will update their time.
 * Lastly, when a time spec changes, its dependent time specs are
 * updated, so the effects ripple through the timeline.
 *
 * Each animation has a list of begin and end time specs, as well as
 * an activation interval. The activation interval may have indefinite
 * start or end. When the animation is not playing, the activation
 * interval represents the next known activation (if any). Both
 * the start and the end time of that activation may change when
 * the animations begin or end time specs change.
 *
 * While the animation is playing, the activation interval represents
 * the current activation. In this case, the only way the start
 * time may change is when the animation is restarted. The end
 * time may still change due to time spec changes.
 */

void
time_spec_clear (TimeSpec *t)
{
  if (t->type == TIME_SPEC_TYPE_SYNC)
    g_free (t->sync.ref);
  else if (t->type == TIME_SPEC_TYPE_EVENT)
    g_free (t->event.ref);

  g_clear_pointer (&t->animations, g_ptr_array_unref);
}

void
time_spec_free (void *p)
{
  TimeSpec *t = p;
  time_spec_clear (t);
  g_free (t);
}

TimeSpec *
time_spec_copy (const TimeSpec *orig)
{
  TimeSpec *t = g_new0 (TimeSpec, 1);

  t->type = orig->type;
  t->offset = orig->offset;
  t->time = orig->time;

  switch (t->type)
    {
    case TIME_SPEC_TYPE_INDEFINITE:
      break;
    case TIME_SPEC_TYPE_OFFSET:
      break;
    case TIME_SPEC_TYPE_SYNC:
      t->sync.ref = g_strdup (orig->sync.ref);
      t->sync.base = orig->sync.base;
      t->sync.side = orig->sync.side;
      break;
    case TIME_SPEC_TYPE_STATES:
      t->states.from = orig->states.from;
      t->states.to = orig->states.to;
      break;
    case TIME_SPEC_TYPE_EVENT:
      t->event.ref = g_strdup (orig->event.ref);
      t->event.shape = orig->event.shape;
      t->event.event = orig->event.event;
      break;
    default:
      g_assert_not_reached ();
    }

  return t;
}

gboolean
time_spec_equal (const void *p1,
                 const void *p2)
{
  const TimeSpec *t1 = p1;
  const TimeSpec *t2 = p2;

  if (t1->type != t2->type)
    return FALSE;

  switch (t1->type)
    {
    case TIME_SPEC_TYPE_INDEFINITE:
      return TRUE;

    case TIME_SPEC_TYPE_OFFSET:
      return t1->offset == t2->offset;

    case TIME_SPEC_TYPE_SYNC:
      return t1->sync.base == t2->sync.base &&
             g_strcmp0 (t1->sync.ref, t2->sync.ref) == 0 &&
             t1->sync.side == t2->sync.side &&
             t1->offset == t2->offset;

    case TIME_SPEC_TYPE_STATES:
      return t1->states.from == t2->states.from &&
             t1->states.to == t2->states.to &&
             t1->offset == t2->offset;

    case TIME_SPEC_TYPE_EVENT:
      return ((t1->event.shape != NULL && t1->event.shape == t2->event.shape) ||
              (t1->event.ref != NULL && g_strcmp0 (t1->event.ref, t2->event.ref) == 0)) &&
             t1->event.event == t2->event.event &&
             t1->offset == t2->offset;

    default:
      g_assert_not_reached ();
    }
}

typedef struct
{
  GtkSvg *svg;
  uint64_t states[2];
  gboolean set[2];
} ParseStatesArg;

static unsigned int
parse_states_arg (GtkCssParser *parser,
                  unsigned int  n,
                  gpointer      data)
{
  ParseStatesArg *arg = data;
  uint64_t states;

  if (!parse_states_css (parser, arg->svg, &states))
    return 0;

  arg->states[n] = states;
  arg->set[n] = TRUE;
  return 1;
}

gboolean
time_spec_parse (GtkCssParser  *parser,
                 GtkSvg        *svg,
                 SvgElement    *default_event_target,
                 TimeSpec      *spec,
                 GError       **error)
{
  const char *event_types[] = { "focus", "blur", "mouseenter", "mouseleave", "click" };
  memset (spec, 0, sizeof (TimeSpec));

  gtk_css_parser_skip_whitespace (parser);
  if (gtk_css_parser_try_ident (parser, "indefinite"))
    {
      spec->type = TIME_SPEC_TYPE_INDEFINITE;
    }
  else if (parser_try_duration (parser, &spec->offset))
    {
      spec->type = TIME_SPEC_TYPE_OFFSET;
    }
  else if (gtk_css_parser_has_function (parser, "StateChange"))
    {
      ParseStatesArg arg = {
        .svg = svg,
        .states = { NO_STATES, NO_STATES },
        .set = { FALSE, FALSE },
      };

      if (gtk_css_parser_consume_function (parser, 2, 2, parse_states_arg, &arg))
        {
          spec->type = TIME_SPEC_TYPE_STATES;
          spec->states.from = arg.states[0];
          spec->states.to = arg.states[1];

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
      else
        return FALSE;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      char *id = gtk_css_parser_consume_ident (parser);

      if (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON) &&
          strcmp (id, "gpa") == 0)
        {
          g_free (id);

          if (gtk_css_parser_has_function (parser, "states"))
            {
              ParseStatesArg arg = {
                .svg = svg,
                .states = { NO_STATES, NO_STATES },
                .set = { FALSE, FALSE },
              };

              if (gtk_css_parser_consume_function (parser, 1, 2, parse_states_arg, &arg))
                {
                  spec->type = TIME_SPEC_TYPE_STATES;

                  if (!arg.set[1])
                    {
                      TimeSpecSide side;

                      if (!gtk_css_parser_try_delim (parser, '.'))
                        return FALSE;
                      if (gtk_css_parser_try_ident (parser, "begin"))
                        side = TIME_SPEC_SIDE_BEGIN;
                      else if (gtk_css_parser_try_ident (parser, "end"))
                        side = TIME_SPEC_SIDE_END;
                      else
                        return FALSE;

                      if (side == TIME_SPEC_SIDE_BEGIN)
                        {
                          spec->states.from = ALL_STATES & ~arg.states[0];
                          spec->states.to = arg.states[0];
                        }
                      else
                        {
                          spec->states.from = arg.states[0];
                          spec->states.to = ALL_STATES & ~arg.states[0];
                        }
                    }
                  else
                    {
                      spec->states.from = arg.states[0];
                      spec->states.to = arg.states[1];
                    }

                  gtk_css_parser_skip_whitespace (parser);
                  parser_try_duration (parser, &spec->offset);
                }

              if (*error == NULL)
                g_set_error (error,
                             GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_SYNTAX,
                             "gpa:states() is deprecated. Use StateChange()");
            }
          else
            return FALSE;
        }
      else if (gtk_css_parser_try_delim (parser, '.'))
        {
          unsigned int value;

          if (parser_try_enum (parser, (const char *[]) { "begin", "end" }, 2, &value))
            {
              spec->type = TIME_SPEC_TYPE_SYNC;
              spec->sync.ref = id;
              spec->sync.side = value;
              spec->sync.base = NULL;
            }
          else if (parser_try_enum (parser, event_types, G_N_ELEMENTS (event_types), &value))
            {
              spec->type = TIME_SPEC_TYPE_EVENT;
              spec->event.ref = id;
              spec->event.event = value;
              spec->event.shape = NULL;
            }
          else
            {
              g_free (id);
              return FALSE;
            }

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
      else
        {
          unsigned int value;

          if (parse_enum (id, event_types, G_N_ELEMENTS (event_types), &value))
            {
              g_free (id);
              spec->type = TIME_SPEC_TYPE_EVENT;
              spec->event.ref = NULL;
              spec->event.event = value;
              spec->event.shape = default_event_target;
            }
          else
            {
              g_free (id);
              return FALSE;
            }

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
    }
  else
    return FALSE;

  return TRUE;
}

void
time_spec_add_animation (TimeSpec     *spec,
                         SvgAnimation *a)
{
  if (!spec->animations)
    spec->animations = g_ptr_array_new ();
  g_ptr_array_add (spec->animations, a);
}

void
time_spec_drop_animation (TimeSpec     *spec,
                          SvgAnimation *a)
{
  g_ptr_array_remove (spec->animations, a);
}

static void
time_spec_set_time (TimeSpec *spec,
                    int64_t   time)
{
  if (spec->time == time)
    return;

  spec->time = time;

  if (spec->animations)
    {
      for (unsigned int i = 0; i < spec->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (spec->animations, i);
          animation_update_for_spec (a, spec);
        }
    }
}

void
time_spec_update_for_load_time (TimeSpec *spec,
                                int64_t   load_time)
{
  if (spec->type == TIME_SPEC_TYPE_OFFSET && spec->time == INDEFINITE)
    time_spec_set_time (spec, load_time + spec->offset);
}

void
time_spec_update_for_pause (TimeSpec *spec,
                            int64_t   duration)
{
  if (spec->time != INDEFINITE)
    time_spec_set_time (spec, spec->time + duration);
}

void
time_spec_update_for_state (TimeSpec     *spec,
                            unsigned int  previous_state,
                            unsigned int  state,
                            int64_t       state_start_time)
{
  if (spec->type == TIME_SPEC_TYPE_STATES && previous_state != state)
    {
      if (state_match (spec->states.from & ~spec->states.to, previous_state) &&
          state_match (spec->states.to & ~spec->states.from, state))
        time_spec_set_time (spec, state_start_time + spec->offset);
    }
}

void
time_spec_update_for_event (TimeSpec   *spec,
                            SvgElement *shape,
                            EventType   event,
                            int64_t     event_time)
{
  if (spec->type == TIME_SPEC_TYPE_EVENT)
    {
      if (spec->event.shape == shape && spec->event.event == event)
        time_spec_set_time (spec, event_time + spec->offset);
    }
}

void
time_spec_update_for_base (TimeSpec     *spec,
                           SvgAnimation *base)
{
  if (spec->type == TIME_SPEC_TYPE_SYNC && spec->sync.base == base)
    {
      if (spec->sync.side == TIME_SPEC_SIDE_BEGIN)
        time_spec_set_time (spec, svg_animation_get_current_begin (base) + spec->offset);
      else
        time_spec_set_time (spec, svg_animation_get_current_end (base) + spec->offset);
    }
}

int64_t
time_spec_get_state_change_delay (TimeSpec *spec)
{
  if (spec->type == TIME_SPEC_TYPE_STATES && spec->offset < 0)
    return - spec->offset;

  return 0;
}

int64_t
find_first_time (GPtrArray *specs,
                 int64_t    after)
{
  int64_t first = INDEFINITE;
  int64_t slop = G_TIME_SPAN_MILLISECOND;

  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);

      if (after <= spec->time + slop && spec->time < first)
        first = spec->time;
    }

  return first;
}

void
time_spec_print (TimeSpec *spec,
                 GtkSvg   *svg,
                 GString  *s)
{
  gboolean only_nonzero = FALSE;
  const char *sides[] = { "begin", "end" };
  const char *events[] = { "focus", "blur", "mouseenter", "mouseleave", "click" };

  switch (spec->type)
    {
    case TIME_SPEC_TYPE_INDEFINITE:
      g_string_append (s, "indefinite");
      return;
    case TIME_SPEC_TYPE_OFFSET:
      break;
    case TIME_SPEC_TYPE_SYNC:
      g_string_append_printf (s, "%s.%s", spec->sync.ref, sides[spec->sync.side]);
      only_nonzero = TRUE;
      break;
    case TIME_SPEC_TYPE_STATES:
      g_string_append (s, "StateChange(");
      print_states (s, svg, spec->states.from);
      g_string_append (s, ", ");
      print_states (s, svg, spec->states.to);
      g_string_append (s, ")");
      only_nonzero = TRUE;
      break;
    case TIME_SPEC_TYPE_EVENT:
      if (spec->event.ref)
        g_string_append_printf (s, "%s.", spec->event.ref);
      g_string_append_printf (s, "%s", events[spec->event.event]);
      only_nonzero = TRUE;
      break;
    default:
      g_assert_not_reached ();
    }

  if (!only_nonzero || spec->offset != 0)
    {
      string_append_double (s, only_nonzero ? " " : "", spec->offset / (double) G_TIME_SPAN_MILLISECOND);
      g_string_append (s, "ms");
    }
}

void
time_specs_print (GPtrArray *specs,
                  GtkSvg    *svg,
                  GString   *s)
{
  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);
      if (i > 0)
        g_string_append (s, "; ");
      time_spec_print (spec, svg, s);
    }
}

/* }}} */
/* {{{ Timeline */

Timeline *
timeline_new (void)
{
  Timeline *timeline = g_new (Timeline, 1);
  timeline->times = g_ptr_array_new_with_free_func (time_spec_free);
  return timeline;
}

void
timeline_free (Timeline *timeline)
{
  g_ptr_array_unref (timeline->times);
  g_free (timeline);
}

int64_t
timeline_get_state_change_delay (Timeline *timeline)
{
  int64_t delay = 0;

  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      delay = MAX (delay, time_spec_get_state_change_delay (spec));
    }

  return delay;
}

TimeSpec *
timeline_get_time_spec (Timeline *timeline,
                        TimeSpec *spec)
{
  unsigned int idx;

  if (g_ptr_array_find_with_equal_func (timeline->times, spec, time_spec_equal, &idx))
    return g_ptr_array_index (timeline->times, idx);

  spec = time_spec_copy (spec);
  spec->time = INDEFINITE;
  spec->animations = NULL;

  g_ptr_array_add (timeline->times, spec);
  return g_ptr_array_index (timeline->times, timeline->times->len - 1);
}

TimeSpec *
timeline_get_start_of_time (Timeline *timeline)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_OFFSET, .offset = 0, };
  return timeline_get_time_spec (timeline, &spec);
}

TimeSpec *
timeline_get_end_of_time (Timeline *timeline)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_INDEFINITE, };
  return timeline_get_time_spec (timeline, &spec);
}

TimeSpec *
timeline_get_fixed (Timeline *timeline,
                    int64_t   offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_OFFSET, .offset = offset, };
  return timeline_get_time_spec (timeline, &spec);
}

TimeSpec *
timeline_get_sync (Timeline     *timeline,
                   const char   *ref,
                   SvgAnimation *base,
                   TimeSpecSide  side,
                   int64_t       offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_SYNC,
                    .sync = { .ref = (char *) ref, .base = base, .side = side },
                    .offset = offset };
  return timeline_get_time_spec (timeline, &spec);
}

TimeSpec *
timeline_get_states (Timeline *timeline,
                     uint64_t  from,
                     uint64_t  to,
                     int64_t   offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_STATES,
                    .states = { .from = from, .to = to },
                    .offset = offset };
  return timeline_get_time_spec (timeline, &spec);
}

void
timeline_set_load_time (Timeline *timeline,
                        int64_t   load_time)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_load_time (spec, load_time);
    }
}

void
timeline_update_for_pause (Timeline *timeline,
                           int64_t   duration)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_pause (spec, duration);
    }
}

void
timeline_update_for_state (Timeline     *timeline,
                           unsigned int  previous_state,
                           unsigned int  state,
                           int64_t       state_start_time)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_state (spec, previous_state, state, state_start_time);
    }
}

void
timeline_update_for_event (Timeline   *timeline,
                           SvgElement *shape,
                           EventType   event,
                           int64_t     event_time)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_event (spec, shape, event, event_time);
    }
}

/* }}} */

/* vim:set foldmethod=marker: */
