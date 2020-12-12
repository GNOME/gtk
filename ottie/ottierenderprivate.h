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

#ifndef __OTTIE_RENDER_PRIVATE_H__
#define __OTTIE_RENDER_PRIVATE_H__

#include <gsk/gsk.h>

G_BEGIN_DECLS

typedef struct _OttieRender OttieRender;
typedef struct _OttieRenderPath OttieRenderPath;

struct _OttieRenderPath
{
  GskPath *path;
  GskTransform *transform;
};

static inline void
ottie_render_path_clear (OttieRenderPath *path)
{
  gsk_path_unref (path->path);
  gsk_transform_unref (path->transform);
}

#define GDK_ARRAY_ELEMENT_TYPE OttieRenderPath
#define GDK_ARRAY_TYPE_NAME OttieRenderPaths
#define GDK_ARRAY_NAME ottie_render_paths
#define GDK_ARRAY_FREE_FUNC ottie_render_path_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 8
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE GskRenderNode *
#define GDK_ARRAY_TYPE_NAME OttieRenderNodes
#define GDK_ARRAY_NAME ottie_render_nodes
#define GDK_ARRAY_FREE_FUNC gsk_render_node_unref
#define GDK_ARRAY_PREALLOC 8
#include "gdk/gdkarrayimpl.c"

struct _OttieRender
{
  OttieRenderPaths paths;
  GskPath *cached_path;
  OttieRenderNodes nodes;
};

void                    ottie_render_init                       (OttieRender            *self);
void                    ottie_render_clear                      (OttieRender            *self);

void                    ottie_render_merge                      (OttieRender            *self,
                                                                 OttieRender            *source);

void                    ottie_render_add_path                   (OttieRender            *self,
                                                                 GskPath                *path);
GskPath *               ottie_render_get_path                   (OttieRender            *self);
void                    ottie_render_clear_path                 (OttieRender            *self);
gsize                   ottie_render_get_n_subpaths             (OttieRender            *self);
GskPath *               ottie_render_get_subpath                (OttieRender            *self,
                                                                 gsize                   i);
void                    ottie_render_replace_subpath            (OttieRender            *self,
                                                                 gsize                   i,
                                                                 GskPath                *path);

void                    ottie_render_add_node                   (OttieRender            *self,
                                                                 GskRenderNode          *node);
GskRenderNode *         ottie_render_get_node                   (OttieRender            *self);
void                    ottie_render_clear_nodes                (OttieRender            *self);

void                    ottie_render_transform                  (OttieRender            *self,
                                                                 GskTransform           *transform);
G_END_DECLS

#endif /* __OTTIE_RENDER_PRIVATE_H__ */
