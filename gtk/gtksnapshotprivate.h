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
typedef struct _GtkSnapshotStateArray GtkSnapshotStateArray;

/* This is a stripped-down copy of GArray tailored specifically
 * for GtkSnapshotState and guaranteed to be aligned to 16 byte
 * bounaries.
 */
struct _GtkSnapshotStateArray
{
  GtkSnapshotState *data;
  guint len;
};

typedef GskRenderNode * (* GtkSnapshotCollectFunc) (GtkSnapshot      *snapshot,
                                                    GtkSnapshotState *state,
                                                    GskRenderNode   **nodes,
                                                    guint             n_nodes);

struct _GtkSnapshotState {
  guint                  start_node_index;
  guint                  n_nodes;

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
      double            radius;
    } blur;
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
    struct {
      char *message;
    } debug;
  } data;
} GRAPHENE_ALIGN16;

G_STATIC_ASSERT (sizeof (GtkSnapshotState) % 16 == 0);

/* This is a nasty little hack. We typedef GtkSnapshot to the fake object GdkSnapshot
 * so that we don't need to typecast between them.
 * After all, the GdkSnapshot only exist so poor language bindings don't trip. Hardcore
 * C code can just blatantly ignore such layering violations with a typedef.
 */
struct _GdkSnapshot {
  GObject                parent_instance; /* it's really GdkSnapshot, but don't tell anyone! */

  GtkSnapshotStateArray *state_stack;
  GPtrArray             *nodes;
};

struct _GtkSnapshotClass {
  GObjectClass           parent_class; /* it's really GdkSnapshotClass, but don't tell anyone! */
};

void                    gtk_snapshot_append_node_internal       (GtkSnapshot            *snapshot,
                                                                 GskRenderNode          *node);

G_END_DECLS

#endif /* __GTK_SNAPSHOT_PRIVATE_H__ */
