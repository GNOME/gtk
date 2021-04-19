/* gskngldriver.c
 *
 * Copyright 2017 Timm Bäder <mail@baedert.org>
 * Copyright 2018 Matthias Clasen <mclasen@redhat.com>
 * Copyright 2018 Alexander Larsson <alexl@redhat.com>
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdktextureprivate.h>
#include <gsk/gskdebugprivate.h>
#include <gsk/gskglshaderprivate.h>
#include <gsk/gskrendererprivate.h>

#include "gsknglcommandqueueprivate.h"
#include "gsknglcompilerprivate.h"
#include "gskngldriverprivate.h"
#include "gsknglglyphlibraryprivate.h"
#include "gskngliconlibraryprivate.h"
#include "gsknglprogramprivate.h"
#include "gsknglshadowlibraryprivate.h"
#include "gskngltexturepoolprivate.h"

#define ATLAS_SIZE 512
#define MAX_OLD_RATIO 0.5

typedef struct _GskNglTextureState
{
  GdkGLContext *context;
  GLuint        texture_id;
} GskNglTextureState;

G_DEFINE_TYPE (GskNglDriver, gsk_ngl_driver, G_TYPE_OBJECT)

static guint
texture_key_hash (gconstpointer v)
{
  const GskTextureKey *k = (const GskTextureKey *)v;

  /* Optimize for 0..3 where 0 is the scaled out case. Usually
   * we'll be squarely on 1 or 2 for standard vs HiDPI. When rendering
   * to a texture scaled out like in node-editor, we might be < 1.
   */
  guint scale_x = floorf (k->scale_x);
  guint scale_y = floorf (k->scale_y);

  return GPOINTER_TO_SIZE (k->pointer) ^
    ((scale_x << 8) |
     (scale_y << 6) |
     (k->filter << 1) |
     k->pointer_is_child);
}

static gboolean
texture_key_equal (gconstpointer v1,
                   gconstpointer v2)
{
  const GskTextureKey *k1 = (const GskTextureKey *)v1;
  const GskTextureKey *k2 = (const GskTextureKey *)v2;

  return k1->pointer == k2->pointer &&
         k1->scale_x == k2->scale_x &&
         k1->scale_y == k2->scale_y &&
         k1->filter == k2->filter &&
         k1->pointer_is_child == k2->pointer_is_child &&
         (!k1->pointer_is_child || memcmp (&k1->parent_rect, &k2->parent_rect, sizeof k1->parent_rect) == 0);
}

static void
remove_texture_key_for_id (GskNglDriver *self,
                           guint         texture_id)
{
  GskTextureKey *key;

  g_assert (GSK_IS_NGL_DRIVER (self));
  g_assert (texture_id > 0);

  /* g_hash_table_remove() will cause @key to be freed */
  if (g_hash_table_steal_extended (self->texture_id_to_key,
                                   GUINT_TO_POINTER (texture_id),
                                   NULL,
                                   (gpointer *)&key))
    g_hash_table_remove (self->key_to_texture_id, key);
}

static void
gsk_ngl_texture_destroyed (gpointer data)
{
  ((GskNglTexture *)data)->user = NULL;
}

static guint
gsk_ngl_driver_collect_unused_textures (GskNglDriver *self,
                                        gint64        watermark)
{
  GHashTableIter iter;
  gpointer k, v;
  guint old_size;
  guint collected;

  g_assert (GSK_IS_NGL_DRIVER (self));

  old_size = g_hash_table_size (self->textures);

  g_hash_table_iter_init (&iter, self->textures);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      GskNglTexture *t = v;

      if (t->user || t->permanent)
        continue;

      if (t->last_used_in_frame <= watermark)
        {
          g_hash_table_iter_steal (&iter);

          g_assert (t->link.prev == NULL);
          g_assert (t->link.next == NULL);
          g_assert (t->link.data == t);

          /* Steal this texture and put it back into the pool */
          remove_texture_key_for_id (self, t->texture_id);
          gsk_ngl_texture_pool_put (&self->texture_pool, t);
        }
    }

  collected = old_size - g_hash_table_size (self->textures);

  return collected;
}

static void
gsk_ngl_texture_atlas_free (GskNglTextureAtlas *atlas)
{
  if (atlas->texture_id != 0)
    {
      glDeleteTextures (1, &atlas->texture_id);
      atlas->texture_id = 0;
    }

  g_clear_pointer (&atlas->nodes, g_free);
  g_slice_free (GskNglTextureAtlas, atlas);
}

GskNglTextureAtlas *
gsk_ngl_driver_create_atlas (GskNglDriver *self)
{
  GskNglTextureAtlas *atlas;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), NULL);

  atlas = g_slice_new0 (GskNglTextureAtlas);
  atlas->width = ATLAS_SIZE;
  atlas->height = ATLAS_SIZE;
  /* TODO: We might want to change the strategy about the amount of
   *       nodes here? stb_rect_pack.h says width is optimal. */
  atlas->nodes = g_malloc0_n (atlas->width, sizeof (struct stbrp_node));
  stbrp_init_target (&atlas->context, atlas->width, atlas->height, atlas->nodes, atlas->width);
  atlas->texture_id = gsk_ngl_command_queue_create_texture (self->command_queue,
                                                            atlas->width,
                                                            atlas->height,
                                                            GL_LINEAR,
                                                            GL_LINEAR);

  gdk_gl_context_label_object_printf (gdk_gl_context_get_current (),
                                      GL_TEXTURE, atlas->texture_id,
                                      "Texture atlas %d",
                                      atlas->texture_id);

  g_ptr_array_add (self->atlases, atlas);

  return atlas;
}

static void
remove_program (gpointer data)
{
  GskNglProgram *program = data;

  g_assert (!program || GSK_IS_NGL_PROGRAM (program));

  if (program != NULL)
    {
      gsk_ngl_program_delete (program);
      g_object_unref (program);
    }
}

static void
gsk_ngl_driver_shader_weak_cb (gpointer  data,
                               GObject  *where_object_was)
{
  GskNglDriver *self = data;

  g_assert (GSK_IS_NGL_DRIVER (self));

  if (self->shader_cache != NULL)
    g_hash_table_remove (self->shader_cache, where_object_was);
}

static void
gsk_ngl_driver_dispose (GObject *object)
{
  GskNglDriver *self = (GskNglDriver *)object;

  g_assert (GSK_IS_NGL_DRIVER (self));
  g_assert (self->in_frame == FALSE);

#define GSK_NGL_NO_UNIFORMS
#define GSK_NGL_ADD_UNIFORM(pos, KEY, name)
#define GSK_NGL_DEFINE_PROGRAM(name, resource, uniforms) \
  GSK_NGL_DELETE_PROGRAM(name);                          \
  GSK_NGL_DELETE_PROGRAM(name ## _no_clip);              \
  GSK_NGL_DELETE_PROGRAM(name ## _rect_clip);
#define GSK_NGL_DELETE_PROGRAM(name)                     \
  G_STMT_START {                                         \
    if (self->name)                                      \
      gsk_ngl_program_delete (self->name);               \
    g_clear_object (&self->name);                        \
  } G_STMT_END;
# include "gsknglprograms.defs"
#undef GSK_NGL_NO_UNIFORMS
#undef GSK_NGL_ADD_UNIFORM
#undef GSK_NGL_DEFINE_PROGRAM

  if (self->shader_cache != NULL)
    {
      GHashTableIter iter;
      gpointer k, v;

      g_hash_table_iter_init (&iter, self->shader_cache);
      while (g_hash_table_iter_next (&iter, &k, &v))
        {
          GskGLShader *shader = k;
          g_object_weak_unref (G_OBJECT (shader),
                               gsk_ngl_driver_shader_weak_cb,
                               self);
          g_hash_table_iter_remove (&iter);
        }

      g_clear_pointer (&self->shader_cache, g_hash_table_unref);
    }

  if (self->command_queue != NULL)
    {
      gsk_ngl_command_queue_make_current (self->command_queue);
      gsk_ngl_driver_collect_unused_textures (self, 0);
      g_clear_object (&self->command_queue);
    }

  if (self->autorelease_framebuffers->len > 0)
    {
      glDeleteFramebuffers (self->autorelease_framebuffers->len,
                            (GLuint *)(gpointer)self->autorelease_framebuffers->data);
      self->autorelease_framebuffers->len = 0;
    }

  gsk_ngl_texture_pool_clear (&self->texture_pool);

  g_assert (!self->textures || g_hash_table_size (self->textures) == 0);
  g_assert (!self->texture_id_to_key || g_hash_table_size (self->texture_id_to_key) == 0);
  g_assert (!self->key_to_texture_id|| g_hash_table_size (self->key_to_texture_id) == 0);

  g_clear_object (&self->glyphs);
  g_clear_object (&self->icons);
  g_clear_object (&self->shadows);

  g_clear_pointer (&self->atlases, g_ptr_array_unref);
  g_clear_pointer (&self->autorelease_framebuffers, g_array_unref);
  g_clear_pointer (&self->key_to_texture_id, g_hash_table_unref);
  g_clear_pointer (&self->textures, g_hash_table_unref);
  g_clear_pointer (&self->key_to_texture_id, g_hash_table_unref);
  g_clear_pointer (&self->texture_id_to_key, g_hash_table_unref);
  g_clear_pointer (&self->render_targets, g_ptr_array_unref);
  g_clear_pointer (&self->shader_cache, g_hash_table_unref);

  g_clear_object (&self->command_queue);
  g_clear_object (&self->shared_command_queue);

  G_OBJECT_CLASS (gsk_ngl_driver_parent_class)->dispose (object);
}

static void
gsk_ngl_driver_class_init (GskNglDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_ngl_driver_dispose;
}

static void
gsk_ngl_driver_init (GskNglDriver *self)
{
  self->autorelease_framebuffers = g_array_new (FALSE, FALSE, sizeof (guint));
  self->textures = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify)gsk_ngl_texture_free);
  self->texture_id_to_key = g_hash_table_new (NULL, NULL);
  self->key_to_texture_id = g_hash_table_new_full (texture_key_hash,
                                                   texture_key_equal,
                                                   g_free,
                                                   NULL);
  self->shader_cache = g_hash_table_new_full (NULL, NULL, NULL, remove_program);
  gsk_ngl_texture_pool_init (&self->texture_pool);
  self->render_targets = g_ptr_array_new ();
  self->atlases = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_ngl_texture_atlas_free);
}

static gboolean
gsk_ngl_driver_load_programs (GskNglDriver  *self,
                              GError       **error)
{
  GskNglCompiler *compiler;
  gboolean ret = FALSE;

  g_assert (GSK_IS_NGL_DRIVER (self));
  g_assert (GSK_IS_NGL_COMMAND_QUEUE (self->command_queue));

  compiler = gsk_ngl_compiler_new (self, self->debug);

  /* Setup preambles that are shared by all shaders */
  gsk_ngl_compiler_set_preamble_from_resource (compiler,
                                               GSK_NGL_COMPILER_ALL,
                                               "/org/gtk/libgsk/ngl/preamble.glsl");
  gsk_ngl_compiler_set_preamble_from_resource (compiler,
                                               GSK_NGL_COMPILER_VERTEX,
                                               "/org/gtk/libgsk/ngl/preamble.vs.glsl");
  gsk_ngl_compiler_set_preamble_from_resource (compiler,
                                               GSK_NGL_COMPILER_FRAGMENT,
                                               "/org/gtk/libgsk/ngl/preamble.fs.glsl");

  /* Setup attributes that are provided via VBO */
  gsk_ngl_compiler_bind_attribute (compiler, "aPosition", 0);
  gsk_ngl_compiler_bind_attribute (compiler, "aUv", 1);
  gsk_ngl_compiler_bind_attribute (compiler, "aColor", 2);
  gsk_ngl_compiler_bind_attribute (compiler, "aColor2", 3);

  /* Use XMacros to register all of our programs and their uniforms */
#define GSK_NGL_NO_UNIFORMS
#define GSK_NGL_ADD_UNIFORM(pos, KEY, name)                                                      \
  gsk_ngl_program_add_uniform (program, #name, UNIFORM_##KEY);
#define GSK_NGL_DEFINE_PROGRAM(name, resource, uniforms)                                         \
  gsk_ngl_compiler_set_source_from_resource (compiler, GSK_NGL_COMPILER_ALL, resource);          \
  GSK_NGL_COMPILE_PROGRAM(name ## _no_clip, uniforms, "#define NO_CLIP 1\n");                    \
  GSK_NGL_COMPILE_PROGRAM(name ## _rect_clip, uniforms, "#define RECT_CLIP 1\n");                \
  GSK_NGL_COMPILE_PROGRAM(name, uniforms, "");
#define GSK_NGL_COMPILE_PROGRAM(name, uniforms, clip)                                            \
  G_STMT_START {                                                                                 \
    GskNglProgram *program;                                                                      \
    gboolean have_alpha;                                                                         \
    gboolean have_source;                                                                        \
                                                                                                 \
    if (!(program = gsk_ngl_compiler_compile (compiler, #name, clip, error)))                    \
      goto failure;                                                                              \
                                                                                                 \
    have_alpha = gsk_ngl_program_add_uniform (program, "u_alpha", UNIFORM_SHARED_ALPHA);         \
    have_source = gsk_ngl_program_add_uniform (program, "u_source", UNIFORM_SHARED_SOURCE);      \
    gsk_ngl_program_add_uniform (program, "u_clip_rect", UNIFORM_SHARED_CLIP_RECT);              \
    gsk_ngl_program_add_uniform (program, "u_viewport", UNIFORM_SHARED_VIEWPORT);                \
    gsk_ngl_program_add_uniform (program, "u_projection", UNIFORM_SHARED_PROJECTION);            \
    gsk_ngl_program_add_uniform (program, "u_modelview", UNIFORM_SHARED_MODELVIEW);              \
                                                                                                 \
    uniforms                                                                                     \
                                                                                                 \
    gsk_ngl_program_uniforms_added (program, have_source);                                       \
    if (have_alpha)                                                                              \
      gsk_ngl_program_set_uniform1f (program, UNIFORM_SHARED_ALPHA, 0, 1.0f);                    \
                                                                                                 \
    *(GskNglProgram **)(((guint8 *)self) + G_STRUCT_OFFSET (GskNglDriver, name)) =               \
         g_steal_pointer (&program);                                                             \
  } G_STMT_END;
# include "gsknglprograms.defs"
#undef GSK_NGL_DEFINE_PROGRAM_CLIP
#undef GSK_NGL_DEFINE_PROGRAM
#undef GSK_NGL_ADD_UNIFORM

  ret = TRUE;

failure:
  g_clear_object (&compiler);

  return ret;
}

/**
 * gsk_ngl_driver_autorelease_framebuffer:
 * @self: a #GskNglDriver
 * @framebuffer_id: the id of the OpenGL framebuffer
 *
 * Marks @framebuffer_id to be deleted when the current frame has cmopleted.
 */
static void
gsk_ngl_driver_autorelease_framebuffer (GskNglDriver *self,
                                        guint         framebuffer_id)
{
  g_assert (GSK_IS_NGL_DRIVER (self));

  g_array_append_val (self->autorelease_framebuffers, framebuffer_id);
}

static GskNglDriver *
gsk_ngl_driver_new (GskNglCommandQueue  *command_queue,
                    gboolean             debug_shaders,
                    GError             **error)
{
  GskNglDriver *self;
  GdkGLContext *context;

  g_return_val_if_fail (GSK_IS_NGL_COMMAND_QUEUE (command_queue), NULL);

  context = gsk_ngl_command_queue_get_context (command_queue);

  gdk_gl_context_make_current (context);

  self = g_object_new (GSK_TYPE_NGL_DRIVER, NULL);
  self->command_queue = g_object_ref (command_queue);
  self->shared_command_queue = g_object_ref (command_queue);
  self->debug = !!debug_shaders;

  if (!gsk_ngl_driver_load_programs (self, error))
    {
      g_object_unref (self);
      return NULL;
    }

  self->glyphs = gsk_ngl_glyph_library_new (self);
  self->icons = gsk_ngl_icon_library_new (self);
  self->shadows = gsk_ngl_shadow_library_new (self);

  return g_steal_pointer (&self);
}

/**
 * gsk_ngl_driver_from_shared_context:
 * @context: a shared #GdkGLContext retrieved with gdk_gl_context_get_shared_context()
 * @debug_shaders: if debug information for shaders should be displayed
 * @error: location for error information
 *
 * Retrieves a driver for a shared context. Generally this is shared across all GL
 * contexts for a display so that fewer programs are necessary for driving output.
 *
 * Returns: (transfer full): a #GskNglDriver if successful; otherwise %NULL and
 *   @error is set.
 */
GskNglDriver *
gsk_ngl_driver_from_shared_context (GdkGLContext  *context,
                                    gboolean       debug_shaders,
                                    GError       **error)
{
  GskNglCommandQueue *command_queue = NULL;
  GskNglDriver *driver;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  if ((driver = g_object_get_data (G_OBJECT (context), "GSK_NGL_DRIVER")))
    return g_object_ref (driver);

  gdk_gl_context_make_current (context);

  /* Initially we create a command queue using the shared context. However,
   * as frames are processed this will be replaced with the command queue
   * for a given renderer. But since the programs are compiled into the
   * shared context, all other contexts sharing with it will have access
   * to those programs.
   */
  command_queue = gsk_ngl_command_queue_new (context, NULL);

  if (!(driver = gsk_ngl_driver_new (command_queue, debug_shaders, error)))
    goto failure;

  g_object_set_data_full (G_OBJECT (context),
                          "GSK_NGL_DRIVER",
                          g_object_ref (driver),
                          g_object_unref);

failure:
  g_clear_object (&command_queue);

  return g_steal_pointer (&driver);
}

static GPtrArray *
gsk_ngl_driver_compact_atlases (GskNglDriver *self)
{
  GPtrArray *removed = NULL;

  g_assert (GSK_IS_NGL_DRIVER (self));

  for (guint i = self->atlases->len; i > 0; i--)
    {
      GskNglTextureAtlas *atlas = g_ptr_array_index (self->atlases, i - 1);

      if (gsk_ngl_texture_atlas_get_unused_ratio (atlas) > MAX_OLD_RATIO)
        {
          GSK_NOTE (GLYPH_CACHE,
                    g_message ("Dropping atlas %d (%g.2%% old)", i,
                               100.0 * gsk_ngl_texture_atlas_get_unused_ratio (atlas)));
          if (removed == NULL)
            removed = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_ngl_texture_atlas_free);
          g_ptr_array_add (removed, g_ptr_array_steal_index (self->atlases, i - 1));
        }
    }

  GSK_NOTE (GLYPH_CACHE, {
    static guint timestamp;
    if (timestamp++ % 60 == 0)
      g_message ("%d atlases", self->atlases->len);
  });

  return removed;
}

/**
 * gsk_ngl_driver_begin_frame:
 * @self: a #GskNglDriver
 * @command_queue: A #GskNglCommandQueue from the renderer
 *
 * Begin a new frame.
 *
 * Texture atlases, pools, and other resources will be prepared to draw the
 * next frame. The command queue should be one that was created for the
 * target context to be drawn into (the context of the renderer's surface).
 */
void
gsk_ngl_driver_begin_frame (GskNglDriver       *self,
                            GskNglCommandQueue *command_queue)
{
  gint64 last_frame_id;
  GPtrArray *removed;

  g_return_if_fail (GSK_IS_NGL_DRIVER (self));
  g_return_if_fail (GSK_IS_NGL_COMMAND_QUEUE (command_queue));
  g_return_if_fail (self->in_frame == FALSE);

  last_frame_id = self->current_frame_id;

  self->in_frame = TRUE;
  self->current_frame_id++;

  g_set_object (&self->command_queue, command_queue);

  gsk_ngl_command_queue_begin_frame (self->command_queue);

  /* Compact atlases with too many freed pixels */
  removed = gsk_ngl_driver_compact_atlases (self);

  /* Mark unused pixel regions of the atlases */
  gsk_ngl_texture_library_begin_frame (GSK_NGL_TEXTURE_LIBRARY (self->icons),
                                       self->current_frame_id,
                                       removed);
  gsk_ngl_texture_library_begin_frame (GSK_NGL_TEXTURE_LIBRARY (self->glyphs),
                                       self->current_frame_id,
                                       removed);

  /* Cleanup old shadows */
  gsk_ngl_shadow_library_begin_frame (self->shadows);

  /* Remove all textures that are from a previous frame or are no
   * longer used by linked GdkTexture. We do this at the beginning
   * of the following frame instead of the end so that we reduce chances
   * we block on any resources while delivering our frames.
   */
  gsk_ngl_driver_collect_unused_textures (self, last_frame_id - 1);

  /* Now free atlas textures */
  g_clear_pointer (&removed, g_ptr_array_unref);
}

/**
 * gsk_ngl_driver_end_frame:
 * @self: a #GskNglDriver
 *
 * Clean up resources from drawing the current frame.
 *
 * Temporary resources used while drawing will be released.
 */
void
gsk_ngl_driver_end_frame (GskNglDriver *self)
{
  g_return_if_fail (GSK_IS_NGL_DRIVER (self));
  g_return_if_fail (self->in_frame == TRUE);

  gsk_ngl_command_queue_make_current (self->command_queue);
  gsk_ngl_command_queue_end_frame (self->command_queue);

  self->in_frame = FALSE;
}

/**
 * gsk_ngl_driver_after_frame:
 * @self: a #GskNglDriver
 *
 * This function does post-frame cleanup operations.
 *
 * To reduce the chances of blocking on the driver it is performed
 * after the frame has swapped buffers.
 */
void
gsk_ngl_driver_after_frame (GskNglDriver *self)
{
  g_return_if_fail (GSK_IS_NGL_DRIVER (self));
  g_return_if_fail (self->in_frame == FALSE);

  /* Release any render targets (possibly adding them to
   * self->autorelease_framebuffers) so we can release the FBOs immediately
   * afterwards.
   */
  while (self->render_targets->len > 0)
    {
      GskNglRenderTarget *render_target = g_ptr_array_index (self->render_targets, self->render_targets->len - 1);

      gsk_ngl_driver_autorelease_framebuffer (self, render_target->framebuffer_id);
      glDeleteTextures (1, &render_target->texture_id);
      g_slice_free (GskNglRenderTarget, render_target);

      self->render_targets->len--;
    }

  /* Now that we have collected render targets, release all the FBOs */
  if (self->autorelease_framebuffers->len > 0)
    {
      glDeleteFramebuffers (self->autorelease_framebuffers->len,
                            (GLuint *)(gpointer)self->autorelease_framebuffers->data);
      self->autorelease_framebuffers->len = 0;
    }

  /* Release any cached textures we used during the frame */
  gsk_ngl_texture_pool_clear (&self->texture_pool);

  /* Reset command queue to our shared queue incase we have operations
   * that need to be processed outside of a frame (such as callbacks
   * from external systems such as GDK).
   */
  g_set_object (&self->command_queue, self->shared_command_queue);
}

GdkGLContext *
gsk_ngl_driver_get_context (GskNglDriver *self)
{
  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), NULL);
  g_return_val_if_fail (GSK_IS_NGL_COMMAND_QUEUE (self->command_queue), NULL);

  return gsk_ngl_command_queue_get_context (self->command_queue);
}

/**
 * gsk_ngl_driver_cache_texture:
 * @self: a #GskNglDriver
 * @key: the key for the texture
 * @texture_id: the id of the texture to be cached
 *
 * Inserts @texture_id into the texture cache using @key.
 *
 * Textures can be looked up by @key after calling this function using
 * gsk_ngl_driver_lookup_texture().
 *
 * Textures that have not been used within a number of frames will be
 * purged from the texture cache automatically.
 */
void
gsk_ngl_driver_cache_texture (GskNglDriver        *self,
                              const GskTextureKey *key,
                              guint                texture_id)
{
  GskTextureKey *k;

  g_assert (GSK_IS_NGL_DRIVER (self));
  g_assert (key != NULL);
  g_assert (texture_id > 0);
  g_assert (g_hash_table_contains (self->textures, GUINT_TO_POINTER (texture_id)));

  k = g_memdup (key, sizeof *key);

  g_hash_table_insert (self->key_to_texture_id, k, GUINT_TO_POINTER (texture_id));
  g_hash_table_insert (self->texture_id_to_key, GUINT_TO_POINTER (texture_id), k);
}

/**
 * gsk_ngl_driver_load_texture:
 * @self: a #GdkTexture
 * @texture: a #GdkTexture
 * @min_filter: GL_NEAREST or GL_LINEAR
 * @mag_filter: GL_NEAREST or GL_LINEAR
 *
 * Loads a #GdkTexture by uploading the contents to the GPU when
 * necessary. If @texture is a #GdkGLTexture, it can be used without
 * uploading contents to the GPU.
 *
 * If the texture has already been uploaded and not yet released
 * from cache, this function returns that texture id without further
 * work.
 *
 * If the texture has not been used for a number of frames, it will
 * be removed from cache.
 *
 * There is no need to release the resulting texture identifier after
 * using it. It will be released automatically.
 *
 * Returns: a texture identifier
 */
guint
gsk_ngl_driver_load_texture (GskNglDriver *self,
                             GdkTexture   *texture,
                             int           min_filter,
                             int           mag_filter)
{
  GdkGLContext *context;
  GdkTexture *downloaded_texture = NULL;
  GdkTexture *source_texture;
  GskNglTexture *t;
  guint texture_id;
  int height;
  int width;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), 0);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);
  g_return_val_if_fail (GSK_IS_NGL_COMMAND_QUEUE (self->command_queue), 0);

  context = self->command_queue->context;

  if (GDK_IS_GL_TEXTURE (texture))
    {
      GdkGLContext *texture_context = gdk_gl_texture_get_context ((GdkGLTexture *)texture);
      GdkGLContext *shared_context = gdk_gl_context_get_shared_context (context);

      if (texture_context == context ||
          (shared_context != NULL &&
           shared_context == gdk_gl_context_get_shared_context (texture_context)))

        {
          /* A GL texture from the same GL context is a simple task... */
          return gdk_gl_texture_get_id ((GdkGLTexture *)texture);
        }
      else
        {
          cairo_surface_t *surface;

          /* In this case, we have to temporarily make the texture's
           * context the current one, download its data into our context
           * and then create a texture from it. */
          if (texture_context != NULL)
            gdk_gl_context_make_current (texture_context);

          surface = gdk_texture_download_surface (texture);
          downloaded_texture = gdk_texture_new_for_surface (surface);
          cairo_surface_destroy (surface);

          gdk_gl_context_make_current (context);

          source_texture = downloaded_texture;
        }
    }
  else
    {
      if ((t = gdk_texture_get_render_data (texture, self)))
        {
          if (t->min_filter == min_filter && t->mag_filter == mag_filter)
            return t->texture_id;
        }

      source_texture = texture;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  texture_id = gsk_ngl_command_queue_upload_texture (self->command_queue,
                                                     source_texture,
                                                     0,
                                                     0,
                                                     width,
                                                     height,
                                                     min_filter,
                                                     mag_filter);

  t = gsk_ngl_texture_new (texture_id,
                           width, height, min_filter, mag_filter,
                           self->current_frame_id);

  g_hash_table_insert (self->textures, GUINT_TO_POINTER (texture_id), t);

  if (gdk_texture_set_render_data (texture, self, t, gsk_ngl_texture_destroyed))
    t->user = texture;

  gdk_gl_context_label_object_printf (context, GL_TEXTURE, t->texture_id,
                                      "GdkTexture<%p> %d", texture, t->texture_id);

  g_clear_object (&downloaded_texture);

  return texture_id;
}

/**
 * gsk_ngl_driver_create_texture:
 * @self: a #GskNglDriver
 * @width: the width of the texture
 * @height: the height of the texture
 * @min_filter: GL_NEAREST or GL_LINEAR
 * @mag_filter: GL_NEAREST or GL_FILTER
 *
 * Creates a new texture immediately that can be used by the caller
 * to upload data, map to a framebuffer, or other uses which may
 * modify the texture immediately.
 *
 * Use gsk_ngl_driver_release_texture() to release this texture back into
 * the pool so it may be reused later in the pipeline.
 *
 * Returns: a #GskNglTexture which can be returned to the pool with
 *   gsk_ngl_driver_release_texture().
 */
GskNglTexture *
gsk_ngl_driver_create_texture (GskNglDriver *self,
                               float         width,
                               float         height,
                               int           min_filter,
                               int           mag_filter)
{
  GskNglTexture *texture;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), NULL);

  texture = gsk_ngl_texture_pool_get (&self->texture_pool,
                                      width, height,
                                      min_filter, mag_filter);
  g_hash_table_insert (self->textures,
                       GUINT_TO_POINTER (texture->texture_id),
                       texture);
  texture->last_used_in_frame = self->current_frame_id;
  return texture;
}

/**
 * gsk_ngl_driver_release_texture:
 * @self: a #GskNglDriver
 * @texture: a #GskNglTexture
 *
 * Releases @texture back into the pool so that it can be used later
 * in the command stream by future batches. This helps reduce VRAM
 * usage on the GPU.
 *
 * When the frame has completed, pooled textures will be released
 * to free additional VRAM back to the system.
 */
void
gsk_ngl_driver_release_texture (GskNglDriver *self,
                                 GskNglTexture  *texture)
{
  guint texture_id;

  g_assert (GSK_IS_NGL_DRIVER (self));
  g_assert (texture != NULL);

  texture_id = texture->texture_id;

  if (texture_id > 0)
    remove_texture_key_for_id (self, texture_id);

  g_hash_table_steal (self->textures, GUINT_TO_POINTER (texture_id));
  gsk_ngl_texture_pool_put (&self->texture_pool, texture);
}

/**
 * gsk_ngl_driver_create_render_target:
 * @self: a #GskNglDriver
 * @width: the width for the render target
 * @height: the height for the render target
 * @min_filter: the min filter to use for the texture
 * @mag_filter: the mag filter to use for the texture
 * @out_render_target: (out): a location for the render target
 *
 * Creates a new render target which contains a framebuffer and a texture
 * bound to that framebuffer of the size @width x @height and using the
 * appropriate filters.
 *
 * Use gsk_ngl_driver_release_render_target() when you are finished with
 * the render target to release it. You may steal the texture from the
 * render target when releasing it.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @out_fbo_id and
 *   @out_texture_id are undefined.
 */
gboolean
gsk_ngl_driver_create_render_target (GskNglDriver        *self,
                                     int                  width,
                                     int                  height,
                                     int                  min_filter,
                                     int                  mag_filter,
                                     GskNglRenderTarget **out_render_target)
{
  guint framebuffer_id;
  guint texture_id;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), FALSE);
  g_return_val_if_fail (GSK_IS_NGL_COMMAND_QUEUE (self->command_queue), FALSE);
  g_return_val_if_fail (out_render_target != NULL, FALSE);

#if 0
  if (self->render_targets->len > 0)
    {
      for (guint i = self->render_targets->len; i > 0; i--)
        {
          GskNglRenderTarget *render_target = g_ptr_array_index (self->render_targets, i-1);

          if (render_target->width == width &&
              render_target->height == height &&
              render_target->min_filter == min_filter &&
              render_target->mag_filter == mag_filter)
            {
              *out_render_target = g_ptr_array_steal_index_fast (self->render_targets, i-1);
              return TRUE;
            }
        }
    }
#endif

  if (gsk_ngl_command_queue_create_render_target (self->command_queue,
                                                  width, height,
                                                  min_filter, mag_filter,
                                                  &framebuffer_id, &texture_id))
    {
      GskNglRenderTarget *render_target;

      render_target = g_slice_new0 (GskNglRenderTarget);
      render_target->min_filter = min_filter;
      render_target->mag_filter = mag_filter;
      render_target->width = width;
      render_target->height = height;
      render_target->framebuffer_id = framebuffer_id;
      render_target->texture_id = texture_id;

      *out_render_target = render_target;

      return TRUE;
    }

  *out_render_target = NULL;

  return FALSE;
}

/**
 * gsk_ngl_driver_release_render_target:
 * @self: a #GskNglDriver
 * @render_target: a #GskNglRenderTarget created with
 *   gsk_ngl_driver_create_render_target().
 * @release_texture: if the texture should also be released
 *
 * Releases a render target that was previously created. An attempt may
 * be made to cache the render target so that future creations of render
 * targets are performed faster.
 *
 * If @release_texture is %FALSE, the backing texture id is returned and
 * the framebuffer is released. Otherwise, both the texture and framebuffer
 * are released or cached until the end of the frame.
 *
 * This may be called when building the render job as the texture or
 * framebuffer will not be removed immediately.
 *
 * Returns: a texture id if @release_texture is %FALSE, otherwise zero.
 */
guint
gsk_ngl_driver_release_render_target (GskNglDriver       *self,
                                      GskNglRenderTarget *render_target,
                                      gboolean            release_texture)
{
  guint texture_id;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), 0);
  g_return_val_if_fail (render_target != NULL, 0);

  if (release_texture)
    {
      texture_id = 0;
      g_ptr_array_add (self->render_targets, render_target);
    }
  else
    {
      GskNglTexture *texture;

      texture_id = render_target->texture_id;

      texture = gsk_ngl_texture_new (render_target->texture_id,
                                     render_target->width,
                                     render_target->height,
                                     render_target->min_filter,
                                     render_target->mag_filter,
                                     self->current_frame_id);
      g_hash_table_insert (self->textures,
                           GUINT_TO_POINTER (texture_id),
                           g_steal_pointer (&texture));

      gsk_ngl_driver_autorelease_framebuffer (self, render_target->framebuffer_id);
      g_slice_free (GskNglRenderTarget, render_target);

    }

  return texture_id;
}

/**
 * gsk_ngl_driver_lookup_shader:
 * @self: a #GskNglDriver
 * @shader: the shader to lookup or load
 * @error: a location for a #GError, or %NULL
 *
 * Attepts to load @shader from the shader cache.
 *
 * If it has not been loaded, then it will compile the shader on demand.
 *
 * Returns: (transfer none): a #GskGLShader if successful; otherwise
 *   %NULL and @error is set.
 */
GskNglProgram *
gsk_ngl_driver_lookup_shader (GskNglDriver  *self,
                              GskGLShader   *shader,
                              GError       **error)
{
  GskNglProgram *program;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (shader != NULL, NULL);

  program = g_hash_table_lookup (self->shader_cache, shader);

  if (program == NULL)
    {
      const GskGLUniform *uniforms;
      GskNglCompiler *compiler;
      GBytes *suffix;
      int n_required_textures;
      int n_uniforms;

      uniforms = gsk_gl_shader_get_uniforms (shader, &n_uniforms);
      if (n_uniforms > GSK_NGL_PROGRAM_MAX_CUSTOM_ARGS)
        {
          g_set_error (error,
                       GDK_GL_ERROR,
                       GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                       "Tried to use %d uniforms, while only %d is supported",
                       n_uniforms,
                       GSK_NGL_PROGRAM_MAX_CUSTOM_ARGS);
          return NULL;
        }

      n_required_textures = gsk_gl_shader_get_n_textures (shader);
      if (n_required_textures > GSK_NGL_PROGRAM_MAX_CUSTOM_TEXTURES)
        {
          g_set_error (error,
                       GDK_GL_ERROR,
                       GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                       "Tried to use %d textures, while only %d is supported",
                       n_required_textures,
                       GSK_NGL_PROGRAM_MAX_CUSTOM_TEXTURES);
          return NULL;
        }

      compiler = gsk_ngl_compiler_new (self, FALSE);
      suffix = gsk_gl_shader_get_source (shader);

      gsk_ngl_compiler_set_preamble_from_resource (compiler,
                                                   GSK_NGL_COMPILER_ALL,
                                                   "/org/gtk/libgsk/ngl/preamble.glsl");
      gsk_ngl_compiler_set_preamble_from_resource (compiler,
                                                   GSK_NGL_COMPILER_VERTEX,
                                                   "/org/gtk/libgsk/ngl/preamble.vs.glsl");
      gsk_ngl_compiler_set_preamble_from_resource (compiler,
                                                   GSK_NGL_COMPILER_FRAGMENT,
                                                   "/org/gtk/libgsk/ngl/preamble.fs.glsl");
      gsk_ngl_compiler_set_source_from_resource (compiler,
                                                 GSK_NGL_COMPILER_ALL,
                                                 "/org/gtk/libgsk/ngl/custom.glsl");
      gsk_ngl_compiler_set_suffix (compiler, GSK_NGL_COMPILER_FRAGMENT, suffix);

      /* Setup attributes that are provided via VBO */
      gsk_ngl_compiler_bind_attribute (compiler, "aPosition", 0);
      gsk_ngl_compiler_bind_attribute (compiler, "aUv", 1);
      gsk_ngl_compiler_bind_attribute (compiler, "aColor", 2);
      gsk_ngl_compiler_bind_attribute (compiler, "aColor2", 3);

      if ((program = gsk_ngl_compiler_compile (compiler, NULL, "", error)))
        {
          gboolean have_alpha;

          gsk_ngl_program_add_uniform (program, "u_source", UNIFORM_SHARED_SOURCE);
          gsk_ngl_program_add_uniform (program, "u_clip_rect", UNIFORM_SHARED_CLIP_RECT);
          gsk_ngl_program_add_uniform (program, "u_viewport", UNIFORM_SHARED_VIEWPORT);
          gsk_ngl_program_add_uniform (program, "u_projection", UNIFORM_SHARED_PROJECTION);
          gsk_ngl_program_add_uniform (program, "u_modelview", UNIFORM_SHARED_MODELVIEW);
          have_alpha = gsk_ngl_program_add_uniform (program, "u_alpha", UNIFORM_SHARED_ALPHA);

          gsk_ngl_program_add_uniform (program, "u_size", UNIFORM_CUSTOM_SIZE);
          gsk_ngl_program_add_uniform (program, "u_texture1", UNIFORM_CUSTOM_TEXTURE1);
          gsk_ngl_program_add_uniform (program, "u_texture2", UNIFORM_CUSTOM_TEXTURE2);
          gsk_ngl_program_add_uniform (program, "u_texture3", UNIFORM_CUSTOM_TEXTURE3);
          gsk_ngl_program_add_uniform (program, "u_texture4", UNIFORM_CUSTOM_TEXTURE4);

          /* Custom arguments (max is 8) */
          for (guint i = 0; i < n_uniforms; i++)
            gsk_ngl_program_add_uniform (program, uniforms[i].name, UNIFORM_CUSTOM_ARG0+i);

          gsk_ngl_program_uniforms_added (program, TRUE);

          if (have_alpha)
            gsk_ngl_program_set_uniform1f (program, UNIFORM_SHARED_ALPHA, 0, 1.0f);

          g_hash_table_insert (self->shader_cache, shader, program);
          g_object_weak_ref (G_OBJECT (shader),
                             gsk_ngl_driver_shader_weak_cb,
                             self);
        }

      g_object_unref (compiler);
    }

  return program;
}

#ifdef G_ENABLE_DEBUG
static void
write_atlas_to_png (GskNglTextureAtlas *atlas,
                    const char         *filename)
{
  int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, atlas->width);
  guchar *data = g_malloc (atlas->height * stride);
  cairo_surface_t *s;

  glBindTexture (GL_TEXTURE_2D, atlas->texture_id);
  glGetTexImage (GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
  s = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32, atlas->width, atlas->height, stride);
  cairo_surface_write_to_png (s, filename);

  cairo_surface_destroy (s);
  g_free (data);
}

void
gsk_ngl_driver_save_atlases_to_png (GskNglDriver *self,
                                    const char   *directory)
{
  g_return_if_fail (GSK_IS_NGL_DRIVER (self));

  if (directory == NULL)
    directory = ".";

  for (guint i = 0; i < self->atlases->len; i++)
    {
      GskNglTextureAtlas *atlas = g_ptr_array_index (self->atlases, i);
      char *filename = g_strdup_printf ("%s%sframe-%d-atlas-%d.png",
                                        directory,
                                        G_DIR_SEPARATOR_S,
                                        (int)self->current_frame_id,
                                        atlas->texture_id);
      write_atlas_to_png (atlas, filename);
      g_free (filename);
    }
}
#endif

GskNglCommandQueue *
gsk_ngl_driver_create_command_queue (GskNglDriver *self,
                                     GdkGLContext *context)
{
  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), NULL);
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return gsk_ngl_command_queue_new (context, self->shared_command_queue->uniforms);
}

void
gsk_ngl_driver_add_texture_slices (GskNglDriver        *self,
                                   GdkTexture          *texture,
                                   GskNglTextureSlice **out_slices,
                                   guint               *out_n_slices)
{
  int max_texture_size;
  GskNglTextureSlice *slices;
  GskNglTexture *t;
  guint n_slices;
  guint cols;
  guint rows;
  int tex_width;
  int tex_height;
  int x = 0, y = 0;

  g_assert (GSK_IS_NGL_DRIVER (self));
  g_assert (GDK_IS_TEXTURE (texture));
  g_assert (out_slices != NULL);
  g_assert (out_n_slices != NULL);

  /* XXX: Too much? */
  max_texture_size = self->command_queue->max_texture_size / 4;

  tex_width = texture->width;
  tex_height = texture->height;
  cols = (texture->width / max_texture_size) + 1;
  rows = (texture->height / max_texture_size) + 1;

  if ((t = gdk_texture_get_render_data (texture, self)))
    {
      *out_slices = t->slices;
      *out_n_slices = t->n_slices;
      return;
    }

  n_slices = cols * rows;
  slices = g_new0 (GskNglTextureSlice, n_slices);

  for (guint col = 0; col < cols; col ++)
    {
      int slice_width = MIN (max_texture_size, texture->width - x);

      for (guint row = 0; row < rows; row ++)
        {
          int slice_height = MIN (max_texture_size, texture->height - y);
          int slice_index = (col * rows) + row;
          guint texture_id;

          texture_id = gsk_ngl_command_queue_upload_texture (self->command_queue,
                                                             texture,
                                                             x, y,
                                                             slice_width, slice_height,
                                                             GL_NEAREST, GL_NEAREST);

          slices[slice_index].rect.x = x;
          slices[slice_index].rect.y = y;
          slices[slice_index].rect.width = slice_width;
          slices[slice_index].rect.height = slice_height;
          slices[slice_index].texture_id = texture_id;

          y += slice_height;
        }

      y = 0;
      x += slice_width;
    }

  /* Allocate one Texture for the entire thing. */
  t = gsk_ngl_texture_new (0,
                           tex_width, tex_height,
                           GL_NEAREST, GL_NEAREST,
                           self->current_frame_id);

  /* Use gsk_ngl_texture_free() as destroy notify here since we are
   * not inserting this GskNglTexture into self->textures!
   */
  gdk_texture_set_render_data (texture, self, t,
                               (GDestroyNotify)gsk_ngl_texture_free);

  t->slices = *out_slices = slices;
  t->n_slices = *out_n_slices = n_slices;
}

GskNglTexture *
gsk_ngl_driver_mark_texture_permanent (GskNglDriver *self,
                                       guint         texture_id)
{
  GskNglTexture *t;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), NULL);
  g_return_val_if_fail (texture_id > 0, NULL);

  if ((t = g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id))))
    t->permanent = TRUE;

  return t;
}

void
gsk_ngl_driver_release_texture_by_id (GskNglDriver *self,
                                      guint         texture_id)
{
  GskNglTexture *texture;

  g_return_if_fail (GSK_IS_NGL_DRIVER (self));
  g_return_if_fail (texture_id > 0);

  remove_texture_key_for_id (self, texture_id);

  if ((texture = g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id))))
    gsk_ngl_driver_release_texture (self, texture);
}


static void
create_texture_from_texture_destroy (gpointer data)
{
  GskNglTextureState *state = data;

  g_assert (state != NULL);
  g_assert (GDK_IS_GL_CONTEXT (state->context));

  gdk_gl_context_make_current (state->context);
  glDeleteTextures (1, &state->texture_id);
  g_clear_object (&state->context);
  g_slice_free (GskNglTextureState, state);
}

GdkTexture *
gsk_ngl_driver_create_gdk_texture (GskNglDriver *self,
                                   guint         texture_id)
{
  GskNglTextureState *state;
  GskNglTexture *texture;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (self), NULL);
  g_return_val_if_fail (self->command_queue != NULL, NULL);
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self->command_queue->context), NULL);
  g_return_val_if_fail (texture_id > 0, NULL);
  g_return_val_if_fail (!g_hash_table_contains (self->texture_id_to_key, GUINT_TO_POINTER (texture_id)), NULL);

  /* We must be tracking this texture_id already to use it */
  if (!(texture = g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id))))
    g_return_val_if_reached (NULL);

  state = g_slice_new0 (GskNglTextureState);
  state->texture_id = texture_id;
  state->context = g_object_ref (self->command_queue->context);

  g_hash_table_steal (self->textures, GUINT_TO_POINTER (texture_id));

  return gdk_gl_texture_new (self->command_queue->context,
                             texture_id,
                             texture->width,
                             texture->height,
                             create_texture_from_texture_destroy,
                             state);
}
