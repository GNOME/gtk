/* gskngltexturepoolprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _GSK_NGL_TEXTURE_POOL_PRIVATE_H__
#define _GSK_NGL_TEXTURE_POOL_PRIVATE_H__

#include "gskngltypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskNglTexturePool
{
  GQueue queue;
} GskNglTexturePool;

struct _GskNglTextureSlice
{
  cairo_rectangle_int_t rect;
  guint texture_id;
};

struct _GskNglTextureNineSlice
{
  cairo_rectangle_int_t rect;
  struct {
    float x;
    float y;
    float x2;
    float y2;
  } area;
};

struct _GskNglTexture
{
  /* Used to insert into queue */
  GList link;

  /* Identifier of the frame that created it */
  gint64 last_used_in_frame;

  /* Backpointer to texture (can be cleared asynchronously) */
  GdkTexture *user;

  /* Only used by nine-slice textures */
  GskNglTextureNineSlice *nine_slice;

  /* Only used by sliced textures */
  GskNglTextureSlice *slices;
  guint n_slices;

  /* The actual GL texture identifier in some shared context */
  guint texture_id;

  int width;
  int height;
  int min_filter;
  int mag_filter;

  /* Set when used by an atlas so we don't drop the texture */
  guint              permanent : 1;
};

void                          gsk_ngl_texture_pool_init      (GskNglTexturePool    *self);
void                          gsk_ngl_texture_pool_clear     (GskNglTexturePool    *self);
GskNglTexture                *gsk_ngl_texture_pool_get       (GskNglTexturePool    *self,
                                                              int                   width,
                                                              int                   height,
                                                              int                   min_filter,
                                                              int                   mag_filter);
void                          gsk_ngl_texture_pool_put       (GskNglTexturePool    *self,
                                                              GskNglTexture        *texture);
GskNglTexture                *gsk_ngl_texture_new            (guint                 texture_id,
                                                              int                   width,
                                                              int                   height,
                                                              int                   min_filter,
                                                              int                   mag_filter,
                                                              gint64                frame_id);
const GskNglTextureNineSlice *gsk_ngl_texture_get_nine_slice (GskNglTexture        *texture,
                                                              const GskRoundedRect *outline,
                                                              float                 extra_pixels_x,
                                                              float                 extra_pixels_y);
void                          gsk_ngl_texture_free           (GskNglTexture        *texture);

G_END_DECLS

#endif /* _GSK_NGL_TEXTURE_POOL_PRIVATE_H__ */
