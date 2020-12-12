/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "ottierenderprivate.h"

#include <glib/gi18n-lib.h>

void
ottie_render_init (OttieRender *self)
{
  memset (self, 0, sizeof (OttieRender));

  ottie_render_paths_init (&self->paths);
  ottie_render_nodes_init (&self->nodes);
}

void
ottie_render_clear_path (OttieRender *self)
{
  ottie_render_paths_set_size (&self->paths, 0);
  g_clear_pointer (&self->cached_path, gsk_path_unref);
}

void
ottie_render_clear_nodes (OttieRender *self)
{
  ottie_render_nodes_set_size (&self->nodes, 0);
}

void
ottie_render_clear (OttieRender *self)
{
  ottie_render_nodes_clear (&self->nodes);

  ottie_render_paths_clear (&self->paths);
  g_clear_pointer (&self->cached_path, gsk_path_unref);
}

void
ottie_render_merge (OttieRender *self,
                    OttieRender *source)
{
  /* prepend all the nodes from source */
  ottie_render_nodes_splice (&self->nodes,
                             0,
                             0, FALSE,
                             ottie_render_nodes_index (&source->nodes, 0),
                             ottie_render_nodes_get_size (&source->nodes));
  /* steal all the nodes from source because refcounting */
  ottie_render_nodes_splice (&source->nodes,
                             0,
                             ottie_render_nodes_get_size (&source->nodes), TRUE,
                             NULL, 0);

  /* append all the paths from source */
  ottie_render_paths_splice (&self->paths,
                             ottie_render_paths_get_size (&self->paths),
                             0, FALSE,
                             ottie_render_paths_index (&source->paths, 0),
                             ottie_render_paths_get_size (&source->paths));
  /* steal all the paths from source because refcounting */
  ottie_render_paths_splice (&source->paths,
                             0,
                             ottie_render_paths_get_size (&source->paths), TRUE,
                             NULL, 0);

  g_clear_pointer (&self->cached_path, gsk_path_unref);
  g_clear_pointer (&source->cached_path, gsk_path_unref);
}

void
ottie_render_add_path (OttieRender *self,
                       GskPath     *path)
{
  g_clear_pointer (&self->cached_path, gsk_path_unref);

  if (gsk_path_is_empty (path))
    {
      gsk_path_unref (path);
      return;
    }

  ottie_render_paths_append (&self->paths, &(OttieRenderPath) { path, NULL });
}

typedef struct 
{
  GskPathBuilder *builder;
  GskTransform *transform;
} TransformForeach;

static gboolean
ottie_render_path_transform_foreach (GskPathOperation        op,
                                     const graphene_point_t *pts,
                                     gsize                   n_pts,
                                     float                   weight,
                                     gpointer                data)
{
  TransformForeach *tf = data;
  graphene_point_t p[3];

  switch (op)
  {
    case GSK_PATH_MOVE:
      gsk_transform_transform_point (tf->transform, &pts[0], &p[0]);
      gsk_path_builder_move_to (tf->builder, p[0].x, p[0].y);
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (tf->builder);
      break;

    case GSK_PATH_LINE:
      gsk_transform_transform_point (tf->transform, &pts[1], &p[0]);
      gsk_path_builder_line_to (tf->builder, p[0].x, p[0].y);
      break;

    case GSK_PATH_CURVE:
      gsk_transform_transform_point (tf->transform, &pts[1], &p[0]);
      gsk_transform_transform_point (tf->transform, &pts[2], &p[1]);
      gsk_transform_transform_point (tf->transform, &pts[3], &p[2]);
      gsk_path_builder_curve_to (tf->builder, p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
      break;

    case GSK_PATH_CONIC:
      gsk_transform_transform_point (tf->transform, &pts[1], &p[0]);
      gsk_transform_transform_point (tf->transform, &pts[2], &p[1]);
      gsk_path_builder_conic_to (tf->builder, p[0].x, p[0].y, p[1].x, p[1].y, weight);
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

GskPath *
ottie_render_get_path (OttieRender *self)
{
  GskPathBuilder *builder;

  if (self->cached_path)
    return self->cached_path;

  builder = gsk_path_builder_new ();
  for (gsize i = 0; i < ottie_render_paths_get_size (&self->paths); i++)
    {
      OttieRenderPath *path = ottie_render_paths_get (&self->paths, i);

      switch (gsk_transform_get_category (path->transform))
        {
        case GSK_TRANSFORM_CATEGORY_IDENTITY:
          gsk_path_builder_add_path (builder, path->path);
          break;

        case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
        case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
        case GSK_TRANSFORM_CATEGORY_2D:
          {
            TransformForeach tf = { builder, path->transform };
            gsk_path_foreach (path->path, -1, ottie_render_path_transform_foreach, &tf);
          }
          break;

        case GSK_TRANSFORM_CATEGORY_3D:
        case GSK_TRANSFORM_CATEGORY_ANY:
        case GSK_TRANSFORM_CATEGORY_UNKNOWN:
          g_critical ("How did we get a 3D matrix?!");
          gsk_path_builder_add_path (builder, path->path);
          break;

        default:
          g_assert_not_reached();
          break;
        }
    }
  self->cached_path = gsk_path_builder_free_to_path (builder);

  return self->cached_path;
}

gsize
ottie_render_get_n_subpaths (OttieRender *self)
{
  return ottie_render_paths_get_size (&self->paths);
}

GskPath *
ottie_render_get_subpath (OttieRender *self,
                          gsize        i)
{
  OttieRenderPath *path = ottie_render_paths_get (&self->paths, i);

  return path->path;
}

void
ottie_render_replace_subpath (OttieRender *self,
                              gsize        i,
                              GskPath     *path)
{
  OttieRenderPath *rpath = ottie_render_paths_get (&self->paths, i);

  gsk_path_unref (rpath->path);
  rpath->path = path;

  g_clear_pointer (&self->cached_path, gsk_path_unref);
}

void
ottie_render_add_node (OttieRender   *self,
                       GskRenderNode *node)
{
  ottie_render_nodes_splice (&self->nodes, 0, 0, FALSE, &node, 1);
}

GskRenderNode *
ottie_render_get_node (OttieRender *self)
{
  if (ottie_render_nodes_get_size (&self->nodes) == 0)
    return NULL;

  if (ottie_render_nodes_get_size (&self->nodes) == 1)
    return gsk_render_node_ref (ottie_render_nodes_get (&self->nodes, 0));

  return gsk_container_node_new (ottie_render_nodes_index (&self->nodes, 0),
                                 ottie_render_nodes_get_size (&self->nodes));
}

void
ottie_render_transform (OttieRender  *self,
                        GskTransform *transform)
{
  GskRenderNode *node;

  if (gsk_transform_get_category (transform) == GSK_TRANSFORM_CATEGORY_IDENTITY)
    return;

  for (gsize i = 0; i < ottie_render_paths_get_size (&self->paths); i++)
    {
      OttieRenderPath *path = ottie_render_paths_get (&self->paths, i);

      path->transform = gsk_transform_transform (path->transform, transform);
    }

  node = ottie_render_get_node (self);
  if (node)
    {
      GskRenderNode *transform_node = gsk_transform_node_new (node, transform);

      ottie_render_clear_nodes (self);
      ottie_render_add_node (self, transform_node);

      gsk_render_node_unref (node);
    }
}
