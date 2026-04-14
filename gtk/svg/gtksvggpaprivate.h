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

#include "css/gtkcssparserprivate.h"
#include "gtksvg.h"
#include "gtksvgenumsprivate.h"
#include "gtksvgtypesprivate.h"
#include <stdint.h>

G_BEGIN_DECLS

gboolean        state_match      (uint64_t     states,
                                  unsigned int state);

gboolean        valid_state_name (const char *name);

gboolean        find_named_state (GtkSvg       *svg,
                                  const char   *name,
                                  unsigned int *state);

gboolean        parse_states_css (GtkCssParser *parser,
                                  GtkSvg       *svg,
                                  uint64_t     *states);

gboolean        parse_states     (GtkSvg       *svg,
                                  const char   *text,
                                  uint64_t     *states);

void print_states (GString  *s,
                   GtkSvg   *svg,
                   uint64_t  states);

void
shape_apply_state (GtkSvg       *self,
                   SvgElement   *shape,
                   unsigned int  state);

void
apply_state (GtkSvg   *self,
             uint64_t  state);

void
create_states (SvgElement   *shape,
               Timeline     *timeline,
               uint64_t      states,
               int64_t       delay,
               unsigned int  initial);

void
create_path_length (SvgElement *shape,
                    Timeline   *timeline);

void
create_transitions (SvgElement    *shape,
                    Timeline      *timeline,
                    GHashTable    *shapes,
                    uint64_t       states,
                    GpaTransition  type,
                    int64_t        duration,
                    int64_t        delay,
                    GpaEasing      easing,
                    double         origin);

void
create_animations (SvgElement   *shape,
                   Timeline     *timeline,
                   uint64_t      states,
                   unsigned int  initial,
                   double        repeat,
                   int64_t       duration,
                   GpaAnimation  direction,
                   GpaEasing     easing,
                   double        segment,
                   double        origin);

void
create_attachment (SvgElement *shape,
                   Timeline   *timeline,
                   uint64_t    states,
                   const char *attach_to,
                   double      attach_pos,
                   double      origin);

void
create_attachment_connection (SvgAnimation *a,
                              SvgElement   *sh,
                              Timeline     *timeline);


G_END_DECLS
