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

#include "gtksvg.h"

G_BEGIN_DECLS

typedef struct _Shape Shape;
typedef struct _Timeline Timeline;

typedef enum
{
  ALIGN_MIN = 1 << 0,
  ALIGN_MID = 1 << 1,
  ALIGN_MAX = 1 << 2,
} Align;

typedef enum
{
  MEET,
  SLICE,
} MeetOrSlice;

typedef enum
{
  GTK_SVG_RUN_MODE_STOPPED,
  GTK_SVG_RUN_MODE_DISCRETE,
  GTK_SVG_RUN_MODE_CONTINUOUS,
} GtkSvgRunMode;

struct _GtkSvg
{
  GObject parent_instance;
  Shape *content;

  double width, height;
  graphene_rect_t view_box;
  graphene_rect_t bounds;

  Align align;
  MeetOrSlice meet_or_slice;

  double weight;
  unsigned int state;
  unsigned int max_state;
  int64_t state_change_delay;

  int64_t load_time;
  int64_t current_time;

  gboolean playing;
  GtkSvgRunMode run_mode;
  GdkFrameClock *clock;
  unsigned long clock_update_id;
  unsigned int periodic_update_id;

  int64_t next_update;
  unsigned int pending_invalidate;
  gboolean advance_after_snapshot;

  unsigned int gpa_version;

  Timeline *timeline;
};

void           gtk_svg_set_load_time   (GtkSvg                *self,
                                        int64_t                load_time);

void           gtk_svg_set_playing     (GtkSvg                *self,
                                        gboolean               playing);

void           gtk_svg_advance         (GtkSvg                *self,
                                        int64_t                current_time);

GtkSvgRunMode  gtk_svg_get_run_mode    (GtkSvg *self);

int64_t        gtk_svg_get_next_update (GtkSvg *self);

typedef enum
{
  GTK_SVG_SERIALIZE_DEFAULT           = 0,
  GTK_SVG_SERIALIZE_AT_CURRENT_TIME   = 1 << 0,
  GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION = 1 << 1,
  GTK_SVG_SERIALIZE_INCLUDE_STATE     = 1 << 2,
} GtkSvgSerializeFlags;

GBytes *       gtk_svg_serialize_full  (GtkSvg                *self,
                                        const GdkRGBA         *colors,
                                        size_t                 n_colors,
                                        GtkSvgSerializeFlags   flags);

G_END_DECLS
