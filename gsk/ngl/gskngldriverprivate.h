/* gskngldriverprivate.h
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

#ifndef __GSK_NGL_DRIVER_PRIVATE_H__
#define __GSK_NGL_DRIVER_PRIVATE_H__

#include <gdk/gdkgltextureprivate.h>

#include "gskngltypesprivate.h"
#include "gskngltexturepoolprivate.h"

G_BEGIN_DECLS

enum {
  UNIFORM_SHARED_ALPHA,
  UNIFORM_SHARED_SOURCE,
  UNIFORM_SHARED_CLIP_RECT,
  UNIFORM_SHARED_VIEWPORT,
  UNIFORM_SHARED_PROJECTION,
  UNIFORM_SHARED_MODELVIEW,

  UNIFORM_SHARED_LAST
};

enum {
  UNIFORM_CUSTOM_SIZE = UNIFORM_SHARED_LAST,
  UNIFORM_CUSTOM_TEXTURE1,
  UNIFORM_CUSTOM_TEXTURE2,
  UNIFORM_CUSTOM_TEXTURE3,
  UNIFORM_CUSTOM_TEXTURE4,
  UNIFORM_CUSTOM_ARG0,
  UNIFORM_CUSTOM_ARG1,
  UNIFORM_CUSTOM_ARG2,
  UNIFORM_CUSTOM_ARG3,
  UNIFORM_CUSTOM_ARG4,
  UNIFORM_CUSTOM_ARG5,
  UNIFORM_CUSTOM_ARG6,
  UNIFORM_CUSTOM_ARG7,

  UNIFORM_CUSTOM_LAST
};

typedef struct {
  gconstpointer   pointer;
  float           scale_x;
  float           scale_y;
  int             filter;
  int             pointer_is_child;
  graphene_rect_t parent_rect; /* Valid when pointer_is_child */
} GskTextureKey;

#define GSK_NGL_NO_UNIFORMS CONCAT_EXPANDED(UNIFORM_INVALID_,__COUNTER__)
#define CONCAT_EXPANDED(a,b) CONCAT_EXPANDED2(a,b)
#define CONCAT_EXPANDED2(a,b) a##b
#define GSK_NGL_ADD_UNIFORM(pos, KEY, name) UNIFORM_##KEY = UNIFORM_SHARED_LAST + pos,
#define GSK_NGL_DEFINE_PROGRAM(name, resource, uniforms) enum { uniforms };
# include "gsknglprograms.defs"
#undef GSK_NGL_DEFINE_PROGRAM
#undef GSK_NGL_ADD_UNIFORM
#undef GSK_NGL_NO_UNIFORMS
#undef CONCAT_EXPANDED
#undef CONCAT_EXPANDED2

#define GSK_TYPE_NGL_DRIVER (gsk_ngl_driver_get_type())

G_DECLARE_FINAL_TYPE (GskNglDriver, gsk_ngl_driver, GSK, NGL_DRIVER, GObject)

struct _GskNglRenderTarget
{
  guint framebuffer_id;
  guint texture_id;
  int min_filter;
  int mag_filter;
  int width;
  int height;
};

struct _GskNglDriver
{
  GObject parent_instance;

  GskNglCommandQueue *shared_command_queue;
  GskNglCommandQueue *command_queue;

  GskNglTexturePool texture_pool;

  GskNglGlyphLibrary *glyphs;
  GskNglIconLibrary *icons;
  GskNglShadowLibrary *shadows;

  GHashTable *textures;
  GHashTable *key_to_texture_id;
  GHashTable *texture_id_to_key;

  GPtrArray *atlases;

  GHashTable *shader_cache;

  GArray *autorelease_framebuffers;
  GPtrArray *render_targets;

#define GSK_NGL_NO_UNIFORMS
#define GSK_NGL_ADD_UNIFORM(pos, KEY, name)
#define GSK_NGL_DEFINE_PROGRAM(name, resource, uniforms) \
  GskNglProgram *name ## _no_clip; \
  GskNglProgram *name ## _rect_clip; \
  GskNglProgram *name;
# include "gsknglprograms.defs"
#undef GSK_NGL_NO_UNIFORMS
#undef GSK_NGL_ADD_UNIFORM
#undef GSK_NGL_DEFINE_PROGRAM

  gint64 current_frame_id;

  /* Used to reduce number of comparisons */
  guint stamps[UNIFORM_SHARED_LAST];

  guint debug : 1;
  guint in_frame : 1;
};

GskNglDriver       *gsk_ngl_driver_from_shared_context    (GdkGLContext         *context,
                                                           gboolean              debug_shaders,
                                                           GError              **error);
GskNglCommandQueue *gsk_ngl_driver_create_command_queue   (GskNglDriver         *self,
                                                           GdkGLContext         *context);
GdkGLContext       *gsk_ngl_driver_get_context            (GskNglDriver         *self);
gboolean            gsk_ngl_driver_create_render_target   (GskNglDriver         *self,
                                                           int                   width,
                                                           int                   height,
                                                           int                   min_filter,
                                                           int                   mag_filter,
                                                           GskNglRenderTarget  **render_target);
guint               gsk_ngl_driver_release_render_target  (GskNglDriver         *self,
                                                           GskNglRenderTarget   *render_target,
                                                           gboolean              release_texture);
void                gsk_ngl_driver_begin_frame            (GskNglDriver         *self,
                                                           GskNglCommandQueue   *command_queue);
void                gsk_ngl_driver_end_frame              (GskNglDriver         *self);
void                gsk_ngl_driver_after_frame            (GskNglDriver         *self);
GdkTexture         *gsk_ngl_driver_create_gdk_texture     (GskNglDriver         *self,
                                                           guint                 texture_id);
void                gsk_ngl_driver_cache_texture          (GskNglDriver         *self,
                                                           const GskTextureKey  *key,
                                                           guint                 texture_id);
guint               gsk_ngl_driver_load_texture           (GskNglDriver         *self,
                                                           GdkTexture           *texture,
                                                           int                   min_filter,
                                                           int                   mag_filter);
GskNglTexture      *gsk_ngl_driver_create_texture         (GskNglDriver         *self,
                                                           float                 width,
                                                           float                 height,
                                                           int                   min_filter,
                                                           int                   mag_filter);
void                gsk_ngl_driver_release_texture        (GskNglDriver         *self,
                                                           GskNglTexture        *texture);
void                gsk_ngl_driver_release_texture_by_id  (GskNglDriver         *self,
                                                           guint                 texture_id);
GskNglTexture      *gsk_ngl_driver_mark_texture_permanent (GskNglDriver         *self,
                                                           guint                 texture_id);
void                gsk_ngl_driver_add_texture_slices     (GskNglDriver         *self,
                                                           GdkTexture           *texture,
                                                           GskNglTextureSlice  **out_slices,
                                                           guint                *out_n_slices);
GskNglProgram      *gsk_ngl_driver_lookup_shader          (GskNglDriver         *self,
                                                           GskGLShader          *shader,
                                                           GError              **error);
GskNglTextureAtlas *gsk_ngl_driver_create_atlas           (GskNglDriver         *self);

#ifdef G_ENABLE_DEBUG
void                gsk_ngl_driver_save_atlases_to_png    (GskNglDriver         *self,
                                                           const char           *directory);
#endif

static inline GskNglTexture *
gsk_ngl_driver_get_texture_by_id (GskNglDriver *self,
                                  guint         texture_id)
{
  return g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id));
}

/**
 * gsk_ngl_driver_lookup_texture:
 * @self: a #GskNglDriver
 * @key: the key for the texture
 *
 * Looks up a texture in the texture cache by @key.
 *
 * If the texture could not be found, then zero is returned.
 *
 * Returns: a positive integer if the texture was found; otherwise 0.
 */
static inline guint
gsk_ngl_driver_lookup_texture (GskNglDriver        *self,
                               const GskTextureKey *key)
{
  gpointer id;

  if (g_hash_table_lookup_extended (self->key_to_texture_id, key, NULL, &id))
    {
      GskNglTexture *texture = g_hash_table_lookup (self->textures, id);

      if (texture != NULL)
        texture->last_used_in_frame = self->current_frame_id;

      return GPOINTER_TO_UINT (id);
    }

  return 0;
}

static inline void
gsk_ngl_driver_slice_texture (GskNglDriver        *self,
                              GdkTexture          *texture,
                              GskNglTextureSlice **out_slices,
                              guint               *out_n_slices)
{
  GskNglTexture *t;

  if ((t = gdk_texture_get_render_data (texture, self)))
    {
      *out_slices = t->slices;
      *out_n_slices = t->n_slices;
      return;
    }

  gsk_ngl_driver_add_texture_slices (self, texture, out_slices, out_n_slices);
}

G_END_DECLS

#endif /* __GSK_NGL_DRIVER_PRIVATE_H__ */
