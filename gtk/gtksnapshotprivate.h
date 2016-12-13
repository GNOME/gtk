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

typedef GskRenderNode * (* GtkSnapshotCollectFunc) (GskRenderNode **nodes,
                                                    guint           n_nodes,
                                                    const char     *name,
                                                    gpointer        user_data);

struct _GtkSnapshotState {
  GtkSnapshotState      *parent;

  char                  *name;
  GPtrArray             *nodes;

  cairo_region_t        *clip_region;
  double                 translate_x;
  double                 translate_y;

  GtkSnapshotCollectFunc collect_func;
  gpointer               collect_data;
};

struct _GtkSnapshot {
  GtkSnapshotState      *state;

  GskRenderer           *renderer;
};

void            gtk_snapshot_init               (GtkSnapshot             *state,
                                                 GskRenderer             *renderer,
                                                 const cairo_region_t    *clip,
                                                 const char              *name,
                                                 ...) G_GNUC_PRINTF (4, 5);
GskRenderNode * gtk_snapshot_finish             (GtkSnapshot             *state);

GskRenderer *   gtk_snapshot_get_renderer       (const GtkSnapshot       *snapshot);

G_END_DECLS

#endif /* __GTK_SNAPSHOT_PRIVATE_H__ */
