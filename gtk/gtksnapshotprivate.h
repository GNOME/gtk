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

struct _GtkSnapshot {
  const GtkSnapshot     *parent;
  
  GskRenderer           *renderer;

  graphene_matrix_t      transform;
};

void            gtk_snapshot_init               (GtkSnapshot             *state,
                                                 const GtkSnapshot       *parent,
                                                 const graphene_matrix_t *transform);
void            gtk_snapshot_init_root          (GtkSnapshot             *state,
                                                 GskRenderer             *renderer);

void            gtk_snapshot_finish             (GtkSnapshot             *state);

G_END_DECLS

#endif /* __GTK_SNAPSHOT_PRIVATE_H__ */
