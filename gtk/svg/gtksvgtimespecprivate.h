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

#include <glib.h>
#include "gtksvgtypesprivate.h"
#include "gtksvggpaprivate.h"
#include "css/gtkcssparserprivate.h"
#include <stdint.h>

G_BEGIN_DECLS

typedef enum
{
  TIME_SPEC_TYPE_INDEFINITE,
  TIME_SPEC_TYPE_OFFSET,
  TIME_SPEC_TYPE_SYNC,
  TIME_SPEC_TYPE_STATES,
  TIME_SPEC_TYPE_EVENT,
} TimeSpecType;

typedef enum
{
  EVENT_TYPE_FOCUS,
  EVENT_TYPE_BLUR,
  EVENT_TYPE_MOUSE_ENTER,
  EVENT_TYPE_MOUSE_LEAVE,
  EVENT_TYPE_CLICK,
} EventType;

typedef enum
{
  TIME_SPEC_SIDE_BEGIN,
  TIME_SPEC_SIDE_END,
} TimeSpecSide;

struct _TimeSpec
{
  TimeSpecType type;
  int64_t offset;
  union {
    struct {
      char *ref;
      SvgAnimation *base;
      TimeSpecSide side;
    } sync;
    struct {
      uint64_t from;
      uint64_t to;
    } states;
    struct {
      char *ref;
      SvgElement *shape;
      EventType event;
    } event;
  };
  int64_t time;
  GPtrArray *animations;
};

struct _Timeline
{
  GPtrArray *times;
};

void time_spec_free (void *p);
void time_spec_clear (TimeSpec *p);
TimeSpec *time_spec_copy (const TimeSpec *p);
gboolean time_spec_equal (const void *p1,
                          const void *p2);

void time_spec_add_animation  (TimeSpec     *spec,
                               SvgAnimation *a);
void time_spec_drop_animation (TimeSpec     *spec,
                               SvgAnimation *a);

void time_spec_update_for_load_time (TimeSpec *spec,
                                     int64_t   load_time);
void time_spec_update_for_pause     (TimeSpec *spec,
                                     int64_t   duration);
void time_spec_update_for_state     (TimeSpec     *spec,
                                     unsigned int  previous_state,
                                     unsigned int  state,
                                     int64_t       state_start_time);
void time_spec_update_for_event     (TimeSpec   *spec,
                                     SvgElement *shape,
                                     EventType   event,
                                     int64_t     event_time);
void time_spec_update_for_base      (TimeSpec     *spec,
                                     SvgAnimation *base);

int64_t time_spec_get_state_change_delay (TimeSpec *spec);

gboolean time_spec_parse (GtkCssParser  *parser,
                          GtkSvg        *svg,
                          SvgElement    *default_event_target,
                          TimeSpec      *spec,
                          GError       **error);

int64_t find_first_time (GPtrArray *specs,
                         int64_t    after);


Timeline * timeline_new (void);
void       timeline_free (Timeline *timeline);
int64_t    timeline_get_state_change_delay (Timeline *timeline);
TimeSpec * timeline_get_time_spec (Timeline *timeline,
                                   TimeSpec *spec);
TimeSpec * timeline_get_start_of_time (Timeline *timeline);
TimeSpec * timeline_get_end_of_time (Timeline *timeline);
TimeSpec * timeline_get_fixed (Timeline *timeline,
                               int64_t   offset);
TimeSpec * timeline_get_sync (Timeline     *timeline,
                              const char   *ref,
                              SvgAnimation *base,
                              TimeSpecSide  side,
                              int64_t       offset);
TimeSpec * timeline_get_states (Timeline *timeline,
                                uint64_t  from,
                                uint64_t  to,
                                int64_t   offset);
void timeline_set_load_time (Timeline *timeline,
                             int64_t   load_time);
void timeline_update_for_pause (Timeline *timeline,
                                int64_t   duration);
void timeline_update_for_state (Timeline     *timeline,
                                unsigned int  previous_state,
                                unsigned int  state,
                                int64_t       state_start_time);

void timeline_update_for_event (Timeline   *timeline,
                                SvgElement *shape,
                                EventType   event,
                                int64_t     event_time);

void
time_spec_print (TimeSpec *spec,
                 GtkSvg   *svg,
                 GString  *s);

void
time_specs_print (GPtrArray *specs,
                  GtkSvg    *svg,
                  GString   *s);


G_END_DECLS
