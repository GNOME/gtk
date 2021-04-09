/* gsknglcommandqueueprivate.h
 *
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

#ifndef __GSK_NGL_COMMAND_QUEUE_PRIVATE_H__
#define __GSK_NGL_COMMAND_QUEUE_PRIVATE_H__

#include <gsk/gskprofilerprivate.h>

#include "gskngltypesprivate.h"
#include "gsknglbufferprivate.h"
#include "gsknglattachmentstateprivate.h"
#include "gskngluniformstateprivate.h"

#include "inlinearray.h"

#include "../gl/gskglprofilerprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_COMMAND_QUEUE (gsk_ngl_command_queue_get_type())

G_DECLARE_FINAL_TYPE (GskNglCommandQueue, gsk_ngl_command_queue, GSK, NGL_COMMAND_QUEUE, GObject)

typedef enum _GskNglCommandKind
{
  /* The batch will perform a glClear() */
  GSK_NGL_COMMAND_KIND_CLEAR,

  /* The batch will perform a glDrawArrays() */
  GSK_NGL_COMMAND_KIND_DRAW,
} GskNglCommandKind;

typedef struct _GskNglCommandBind
{
  /* @texture is the value passed to glActiveTexture(), the "slot" the
   * texture will be placed into. We always use GL_TEXTURE_2D so we don't
   * waste any bits here to indicate that.
   */
  guint texture : 5;

  /* The identifier for the texture created with glGenTextures(). */
  guint id : 27;
} GskNglCommandBind;

G_STATIC_ASSERT (sizeof (GskNglCommandBind) == 4);

typedef struct _GskNglCommandBatchAny
{
  /* A GskNglCommandKind indicating what the batch will do */
  guint kind : 8;

  /* The program's identifier to use for determining if we can merge two
   * batches together into a single set of draw operations. We put this
   * here instead of the GskNglCommandDraw so that we can use the extra
   * bits here without making the structure larger.
   */
  guint program : 24;

  /* The index of the next batch following this one. This is used
   * as a sort of integer-based linked list to simplify out-of-order
   * batching without moving memory around. -1 indicates last batch.
   */
  gint16 next_batch_index;

  /* Same but for reverse direction as we sort in reverse to get the
   * batches ordered by framebuffer.
   */
  gint16 prev_batch_index;

  /* The viewport size of the batch. We check this as we process
   * batches to determine if we need to resize the viewport.
   */
  struct {
    guint16 width;
    guint16 height;
  } viewport;
} GskNglCommandBatchAny;

G_STATIC_ASSERT (sizeof (GskNglCommandBatchAny) == 12);

typedef struct _GskNglCommandDraw
{
  GskNglCommandBatchAny head;

  /* There doesn't seem to be a limit on the framebuffer identifier that
   * can be returned, so we have to use a whole unsigned for the framebuffer
   * we are drawing to. When processing batches, we check to see if this
   * changes and adjust the render target accordingly. Some sorting is
   * performed to reduce the amount we change framebuffers.
   */
  guint framebuffer;

  /* The number of uniforms to change. This must be less than or equal to
   * GL_MAX_UNIFORM_LOCATIONS but only guaranteed up to 1024 by any OpenGL
   * implementation to be conformant.
   */
  guint uniform_count : 11;

  /* The number of textures to bind, which is only guaranteed up to 16
   * by the OpenGL specification to be conformant.
   */
  guint bind_count : 5;

  /* GL_MAX_ELEMENTS_VERTICES specifies 33000 for this which requires 16-bit
   * to address all possible counts <= GL_MAX_ELEMENTS_VERTICES.
   */
  guint vbo_count : 16;

  /* The offset within the VBO containing @vbo_count vertices to send with
   * glDrawArrays().
   */
  guint vbo_offset;

  /* The offset within the array of uniform changes to be made containing
   * @uniform_count #GskNglCommandUniform elements to apply.
   */
  guint uniform_offset;

  /* The offset within the array of bind changes to be made containing
   * @bind_count #GskNglCommandBind elements to apply.
   */
  guint bind_offset;
} GskNglCommandDraw;

G_STATIC_ASSERT (sizeof (GskNglCommandDraw) == 32);

typedef struct _GskNglCommandClear
{
  GskNglCommandBatchAny  any;
  guint                 bits;
  guint                 framebuffer;
} GskNglCommandClear;

G_STATIC_ASSERT (sizeof (GskNglCommandClear) == 20);

typedef struct _GskNglCommandUniform
{
  GskNglUniformInfo info;
  guint             location;
} GskNglCommandUniform;

G_STATIC_ASSERT (sizeof (GskNglCommandUniform) == 8);

typedef union _GskNglCommandBatch
{
  GskNglCommandBatchAny any;
  GskNglCommandDraw     draw;
  GskNglCommandClear    clear;
} GskNglCommandBatch;

G_STATIC_ASSERT (sizeof (GskNglCommandBatch) == 32);

DEFINE_INLINE_ARRAY (GskNglCommandBatches, gsk_ngl_command_batches, GskNglCommandBatch)
DEFINE_INLINE_ARRAY (GskNglCommandBinds, gsk_ngl_command_binds, GskNglCommandBind)
DEFINE_INLINE_ARRAY (GskNglCommandUniforms, gsk_ngl_command_uniforms, GskNglCommandUniform)

struct _GskNglCommandQueue
{
  GObject parent_instance;

  /* The GdkGLContext we make current before executing GL commands. */
  GdkGLContext *context;

  /* Array of GskNglCommandBatch which is a fixed size structure that will
   * point into offsets of other arrays so that all similar data is stored
   * together. The idea here is that we reduce the need for pointers so that
   * using g_realloc()'d arrays is fine.
   */
  GskNglCommandBatches batches;

  /* Contains array of vertices and some wrapper code to help upload them
   * to the GL driver. We can also tweak this to use double buffered arrays
   * if we find that to be faster on some hardware and/or drivers.
   */
  GskNglBuffer vertices;

  /* The GskNglAttachmentState contains information about our FBO and texture
   * attachments as we process incoming operations. We snapshot them into
   * various batches so that we can compare differences between merge
   * candidates.
   */
  GskNglAttachmentState *attachments;

  /* The uniform state across all programs. We snapshot this into batches so
   * that we can compare uniform state between batches to give us more
   * chances at merging draw commands.
   */
  GskNglUniformState *uniforms;

  /* Current program if we are in a draw so that we can send commands
   * to the uniform state as needed.
   */
  GskNglUniformProgram *program_info;

  /* The profiler instance to deliver timing/etc data */
  GskProfiler *profiler;
  GskGLProfiler *gl_profiler;

  /* Array of GskNglCommandBind which denote what textures need to be attached
   * to which slot. GskNglCommandDraw.bind_offset and bind_count reference this
   * array to determine what to attach.
   */
  GskNglCommandBinds batch_binds;

  /* Array of GskNglCommandUniform denoting which uniforms must be updated
   * before the glDrawArrays() may be called. These are referenced from the
   * GskNglCommandDraw.uniform_offset and uniform_count fields.
   */
  GskNglCommandUniforms batch_uniforms;

  /* Discovered max texture size when loading the command queue so that we
   * can either scale down or slice textures to fit within this size. Assumed
   * to be both height and width.
   */
  int max_texture_size;

  /* The index of the last batch in @batches, which may not be the element
   * at the end of the array, as batches can be reordered. This is used to
   * update the "next" index when adding a new batch.
   */
  gint16 tail_batch_index;
  gint16 head_batch_index;

  /* Max framebuffer we used, so we can sort items faster */
  guint fbo_max;

  /* Various GSK and GDK metric counter ids */
  struct {
    GQuark n_frames;
    GQuark cpu_time;
    GQuark gpu_time;
    guint n_binds;
    guint n_fbos;
    guint n_uniforms;
    guint n_uploads;
    guint n_programs;
    guint queue_depth;
  } metrics;

  /* Counter for uploads on the frame */
  guint n_uploads;

  /* If we're inside a begin/end_frame pair */
  guint in_frame : 1;

  /* If we're inside of a begin_draw()/end_draw() pair. */
  guint in_draw : 1;

  /* If we've warned about truncating batches */
  guint have_truncated : 1;
};

GskNglCommandQueue *gsk_ngl_command_queue_new                  (GdkGLContext          *context,
                                                                GskNglUniformState    *uniforms);
void                gsk_ngl_command_queue_set_profiler         (GskNglCommandQueue    *self,
                                                                GskProfiler           *profiler);
GdkGLContext       *gsk_ngl_command_queue_get_context          (GskNglCommandQueue    *self);
void                gsk_ngl_command_queue_make_current         (GskNglCommandQueue    *self);
void                gsk_ngl_command_queue_begin_frame          (GskNglCommandQueue    *self);
void                gsk_ngl_command_queue_end_frame            (GskNglCommandQueue    *self);
void                gsk_ngl_command_queue_execute              (GskNglCommandQueue    *self,
                                                                guint                  surface_height,
                                                                guint                  scale_factor,
                                                                const cairo_region_t  *scissor);
int                 gsk_ngl_command_queue_upload_texture       (GskNglCommandQueue    *self,
                                                                GdkTexture            *texture,
                                                                guint                  x_offset,
                                                                guint                  y_offset,
                                                                guint                  width,
                                                                guint                  height,
                                                                int                    min_filter,
                                                                int                    mag_filter);
int                 gsk_ngl_command_queue_create_texture       (GskNglCommandQueue    *self,
                                                                int                    width,
                                                                int                    height,
                                                                int                    min_filter,
                                                                int                    mag_filter);
guint               gsk_ngl_command_queue_create_framebuffer   (GskNglCommandQueue    *self);
gboolean            gsk_ngl_command_queue_create_render_target (GskNglCommandQueue    *self,
                                                                int                    width,
                                                                int                    height,
                                                                int                    min_filter,
                                                                int                    mag_filter,
                                                                guint                 *out_fbo_id,
                                                                guint                 *out_texture_id);
void                gsk_ngl_command_queue_delete_program       (GskNglCommandQueue    *self,
                                                                guint                  program_id);
void                gsk_ngl_command_queue_clear                (GskNglCommandQueue    *self,
                                                                guint                  clear_bits,
                                                                const graphene_rect_t *viewport);
void                gsk_ngl_command_queue_begin_draw           (GskNglCommandQueue    *self,
                                                                GskNglUniformProgram  *program_info,
                                                                guint                  width,
                                                                guint                  height);
void                gsk_ngl_command_queue_end_draw             (GskNglCommandQueue    *self);
void                gsk_ngl_command_queue_split_draw           (GskNglCommandQueue    *self);

static inline GskNglCommandBatch *
gsk_ngl_command_queue_get_batch (GskNglCommandQueue *self)
{
  return gsk_ngl_command_batches_tail (&self->batches);
}

static inline GskNglDrawVertex *
gsk_ngl_command_queue_add_vertices (GskNglCommandQueue *self)
{
  gsk_ngl_command_queue_get_batch (self)->draw.vbo_count += GSK_NGL_N_VERTICES;
  return gsk_ngl_buffer_advance (&self->vertices, GSK_NGL_N_VERTICES);
}

static inline GskNglDrawVertex *
gsk_ngl_command_queue_add_n_vertices (GskNglCommandQueue *self,
                                      guint               count)
{
  /* This is a batch form of gsk_ngl_command_queue_add_vertices(). Note that
   * it does *not* add the count to .draw.vbo_count as the caller is responsible
   * for that.
   */
  return gsk_ngl_buffer_advance (&self->vertices, GSK_NGL_N_VERTICES * count);
}

static inline void
gsk_ngl_command_queue_retract_n_vertices (GskNglCommandQueue *self,
                                          guint               count)
{
  /* Like gsk_ngl_command_queue_add_n_vertices(), this does not tweak
   * the draw vbo_count.
   */
  gsk_ngl_buffer_retract (&self->vertices, GSK_NGL_N_VERTICES * count);
}

static inline guint
gsk_ngl_command_queue_bind_framebuffer (GskNglCommandQueue *self,
                                        guint               framebuffer)
{
  guint ret = self->attachments->fbo.id;
  gsk_ngl_attachment_state_bind_framebuffer (self->attachments, framebuffer);
  return ret;
}

G_END_DECLS

#endif /* __GSK_NGL_COMMAND_QUEUE_PRIVATE_H__ */
