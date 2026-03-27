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

G_BEGIN_DECLS

static inline void
color_apply_color_matrix (const GdkColor          *color,
                          GdkColorState           *color_state,
                          const graphene_matrix_t *matrix,
                          const graphene_vec4_t   *offset,
                          GdkColor                *new_color)
{
  GdkColor c;
  graphene_vec4_t p;
  float v[4];

  gdk_color_convert (&c, color_state, color);
  graphene_vec4_init (&p, c.r, c.g, c.b, c.a);
  graphene_matrix_transform_vec4 (matrix, &p, &p);
  graphene_vec4_add (&p, offset, &p);
  graphene_vec4_to_float (&p, v);
  gdk_color_init (new_color, color_state, v);
  gdk_color_finish (&c);
}

G_END_DECLS
