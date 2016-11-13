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

#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkstylecontextprivate.h"

#include "gsk/gskrendernodeprivate.h"

void
gtk_snapshot_init (GtkSnapshot *state,
                   GskRenderer *renderer)
{
  state->node = NULL;
  state->root = NULL;
  state->renderer = renderer;

  graphene_matrix_init_identity (&state->transform);
}

GskRenderNode *
gtk_snapshot_finish (GtkSnapshot *state)
{
  if (state->node != NULL)
    {
      g_warning ("Too many gtk_snapshot_push() calls.");
    }

  return state->root;
}

void
gtk_snapshot_push_node (GtkSnapshot   *state,
                        GskRenderNode *node)
{
  gtk_snapshot_append_node (state, node);

  state->node = node;
  graphene_matrix_init_identity (&state->transform);
}

void
gtk_snapshot_push (GtkSnapshot           *state,
                   const graphene_rect_t *bounds,
                   const char            *name,
                   ...)
{
  GskRenderNode *node;

  node = gsk_renderer_create_render_node (state->renderer);
  gsk_render_node_set_bounds (node, bounds);

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

  gtk_snapshot_push_node (state, node);
  gsk_render_node_unref (node);
}

void
gtk_snapshot_pop (GtkSnapshot *state)
{
  if (state->node == NULL)
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return;
    }

  gsk_render_node_get_transform (state->node, &state->transform);

  state->node = gsk_render_node_get_parent (state->node);
}

GskRenderer *
gtk_snapshot_get_renderer (const GtkSnapshot *state)
{
  return state->renderer;
}

#if 0
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
#endif

void
gtk_snapshot_set_transform (GtkSnapshot             *state,
                            const graphene_matrix_t *transform)
{
  graphene_matrix_init_from_matrix (&state->transform, transform);
}

void
gtk_snapshot_transform (GtkSnapshot             *state,
                        const graphene_matrix_t *transform)
{
  graphene_matrix_t result;

  graphene_matrix_multiply (&state->transform, transform, &result);
  graphene_matrix_init_from_matrix (&state->transform, &result);
}

void
gtk_snapshot_translate_2d (GtkSnapshot *state,
                           int          x,
                           int          y)
{
  graphene_matrix_t transform;
  graphene_point3d_t point;

  graphene_point3d_init (&point, x, y, 0);
  graphene_matrix_init_translate (&transform, &point);
  gtk_snapshot_transform (state, &transform);
}

void
gtk_snapshot_append_node (GtkSnapshot   *state,
                          GskRenderNode *node)
{
  g_return_if_fail (state != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  gsk_render_node_set_transform (node, &state->transform);
  if (state->node)
    gsk_render_node_append_child (state->node, node);
  else if (state->root == NULL)
    state->root = gsk_render_node_ref (node);
  else
    {
      g_warning ("Tried appending a node to an already finished snapshot.");
    }
}

cairo_t *
gtk_snapshot_append_cairo_node (GtkSnapshot           *state,
                                const graphene_rect_t *bounds,
                                const char            *name,
                                ...)
{
  GskRenderNode *node;

  g_return_val_if_fail (state != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  node = gsk_renderer_create_render_node (state->renderer);
  gsk_render_node_set_bounds (node, bounds);

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

  gtk_snapshot_append_node (state, node);
  gsk_render_node_unref (node);

  return gsk_render_node_get_draw_context (node, state->renderer);
}

cairo_t *
gtk_snapshot_push_cairo_node (GtkSnapshot            *state,
                              const graphene_rect_t  *bounds,
                              const char             *name,
                              ...)
{
  GskRenderNode *node;

  g_return_val_if_fail (state != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  node = gsk_renderer_create_render_node (state->renderer);
  gsk_render_node_set_bounds (node, bounds);

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

  gtk_snapshot_push_node (state, node);
  gsk_render_node_unref (node);

  return gsk_render_node_get_draw_context (node, state->renderer);
}

void
gtk_snapshot_render_background (GtkSnapshot     *state,
                                GtkStyleContext *context,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height)
{
  g_return_if_fail (state != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_translate_2d (state, x, y);
  gtk_css_style_snapshot_background (gtk_style_context_lookup_style (context),
                                     state,
                                     width, height,
                                     gtk_style_context_get_junction_sides (context));
  gtk_snapshot_translate_2d (state, -x, -y);
}

void
gtk_snapshot_render_frame (GtkSnapshot     *state,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  g_return_if_fail (state != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_translate_2d (state, x, y);
  gtk_css_style_snapshot_border (gtk_style_context_lookup_style (context),
                                 state,
                                 width, height,
                                 gtk_style_context_get_junction_sides (context));
  gtk_snapshot_translate_2d (state, -x, -y);
}
