/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_SNAPSHOT_PRIVATE_H__
#define __GTK_SNAPSHOT_PRIVATE_H__

#include "gtksnapshot.h"

G_BEGIN_DECLS

typedef struct _GtkSnapshotState GtkSnapshotState;

typedef GskRenderNode * (* GtkSnapshotCollectFunc) (GtkSnapshotState *state,
                                                    GskRenderNode   **nodes,
                                                    guint             n_nodes,
                                                    const char       *name);

struct _GtkSnapshotState {
  GtkSnapshotState      *parent;
  GtkSnapshotState      *cached_state; /* A cleared state object we can (re)use */

  char                  *name;
  GPtrArray             *nodes;

  cairo_region_t        *clip_region;
  int                    translate_x;
  int                    translate_y;

  GtkSnapshotCollectFunc collect_func;
  union {
    struct {
      graphene_matrix_t transform;
    } transform;
    struct {
      double            opacity;
    } opacity;
    struct {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;
    } color_matrix;
    struct {
      graphene_rect_t bounds;
      graphene_rect_t child_bounds;
    } repeat;
    struct {
      graphene_rect_t bounds;
    } clip;
    struct {
      GskRoundedRect bounds;
    } rounded_clip;
    struct {
      gsize n_shadows;
      GskShadow *shadows;
      GskShadow a_shadow; /* Used if n_shadows == 1 */
    } shadow;
    struct {
      GskBlendMode blend_mode;
      GskRenderNode *bottom_node;
    } blend;
    struct {
      double progress;
      GskRenderNode *start_node;
    } cross_fade;
  } data;
};

struct _GtkSnapshot {
  GtkSnapshotState      *state;
  gboolean               record_names;
  GskRenderer           *renderer;
};

void            gtk_snapshot_init               (GtkSnapshot             *state,
                                                 GskRenderer             *renderer,
                                                 gboolean                 record_names,
                                                 const cairo_region_t    *clip,
                                                 const char              *name,
                                                 ...) G_GNUC_PRINTF (5, 6);
GskRenderNode * gtk_snapshot_finish             (GtkSnapshot             *state);

GskRenderer *   gtk_snapshot_get_renderer       (const GtkSnapshot       *snapshot);

G_END_DECLS

#endif /* __GTK_SNAPSHOT_PRIVATE_H__ */
