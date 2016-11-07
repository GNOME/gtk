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

#include "config.h"

#include "gtksnapshot.h"
#include "gtksnapshotprivate.h"

void
gtk_snapshot_init (GtkSnapshot             *state,
                   const GtkSnapshot       *parent,
                   const graphene_matrix_t *transform)
{
  state->parent = parent;
  state->renderer = parent->renderer;
  
  graphene_matrix_init_from_matrix (&state->transform, transform);
}

void
gtk_snapshot_init_root (GtkSnapshot *state,
                        GskRenderer *renderer)
{
  state->parent = NULL;
  state->renderer = renderer;

  graphene_matrix_init_identity (&state->transform);
}

void
gtk_snapshot_finish (GtkSnapshot *state)
{
  /* nothing to do so far */
}

GskRenderer *
gtk_snapshot_get_renderer (const GtkSnapshot *state)
{
  return state->renderer;
}

GskRenderNode *
gtk_snapshot_create_render_node (const GtkSnapshot *state,
                                 const char *name,
                                 ...)
{
  GskRenderNode *node;

  node = gsk_renderer_create_render_node (state->renderer);

  if (name)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (node, str);

      g_free (str);
    }

  return node;
}
