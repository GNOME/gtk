/* gsknglprogramprivate.h
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

#ifndef __GSK_NGL_PROGRAM_PRIVATE_H__
#define __GSK_NGL_PROGRAM_PRIVATE_H__

#include "gskngltypesprivate.h"

#include "gsknglattachmentstateprivate.h"
#include "gsknglcommandqueueprivate.h"
#include "gskngldriverprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_PROGRAM (gsk_ngl_program_get_type())
#define GSK_NGL_PROGRAM_MAX_CUSTOM_TEXTURES 4
#define GSK_NGL_PROGRAM_MAX_CUSTOM_ARGS 8

G_DECLARE_FINAL_TYPE (GskNglProgram, gsk_ngl_program, GSK, NGL_PROGRAM, GObject)

struct _GskNglProgram
{
  GObject parent_instance;

  int id;
  char *name;
  GskNglDriver *driver;

  /* Cached pointer to avoid lots of pointer chasing/lookups */
  GskNglUniformState *uniforms;
  GskNglUniformProgram *program_info;

  /* Static array for key->location transforms */
  GskNglUniformMapping mappings[32];
  guint n_mappings;
};

GskNglProgram *gsk_ngl_program_new            (GskNglDriver  *driver,
                                               const char    *name,
                                               int            program_id);
gboolean       gsk_ngl_program_add_uniform    (GskNglProgram *self,
                                               const char    *name,
                                               guint          key);
void           gsk_ngl_program_uniforms_added (GskNglProgram *self,
                                               gboolean       has_attachments);
void           gsk_ngl_program_delete         (GskNglProgram *self);

static inline void
gsk_ngl_program_set_uniform1fv (GskNglProgram *self,
                                guint          key,
                                guint          stamp,
                                guint          count,
                                const float   *values)
{
  gsk_ngl_uniform_state_set1fv (self->uniforms, self->program_info,
                                key,
                                stamp,
                                count,
                                values);
}

static inline void
gsk_ngl_program_set_uniform2fv (GskNglProgram *self,
                                guint          key,
                                guint          stamp,
                                guint          count,
                                const float   *values)
{
  gsk_ngl_uniform_state_set2fv (self->uniforms, self->program_info,
                                key,
                                stamp,
                                count,
                                values);
}

static inline void
gsk_ngl_program_set_uniform4fv (GskNglProgram *self,
                                guint          key,
                                guint          stamp,
                                guint          count,
                                const float   *values)
{
  gsk_ngl_uniform_state_set4fv (self->uniforms, self->program_info,
                                key,
                                stamp,
                                count,
                                values);
}

static inline void
gsk_ngl_program_set_uniform_rounded_rect (GskNglProgram        *self,
                                          guint                 key,
                                          guint                 stamp,
                                          const GskRoundedRect *rounded_rect)
{
  gsk_ngl_uniform_state_set_rounded_rect (self->uniforms, self->program_info,
                                          key,
                                          stamp,
                                          rounded_rect);
}

static inline void
gsk_ngl_program_set_uniform1i (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               int            value0)
{
  gsk_ngl_uniform_state_set1i (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0);
}

static inline void
gsk_ngl_program_set_uniform2i (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               int            value0,
                               int            value1)
{
  gsk_ngl_uniform_state_set2i (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0, value1);
}

static inline void
gsk_ngl_program_set_uniform3i (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               int            value0,
                               int            value1,
                               int            value2)
{
  gsk_ngl_uniform_state_set3i (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0, value1, value2);
}

static inline void
gsk_ngl_program_set_uniform4i (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               int            value0,
                               int            value1,
                               int            value2,
                               int            value3)
{
  gsk_ngl_uniform_state_set4i (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0, value1, value2, value3);
}

static inline void
gsk_ngl_program_set_uniform1f (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               float          value0)
{
  gsk_ngl_uniform_state_set1f (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0);
}

static inline void
gsk_ngl_program_set_uniform2f (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               float          value0,
                               float          value1)
{
  gsk_ngl_uniform_state_set2f (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0, value1);
}

static inline void
gsk_ngl_program_set_uniform3f (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               float          value0,
                               float          value1,
                               float          value2)
{
  gsk_ngl_uniform_state_set3f (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0, value1, value2);
}

static inline void
gsk_ngl_program_set_uniform4f (GskNglProgram *self,
                               guint          key,
                               guint          stamp,
                               float          value0,
                               float          value1,
                               float          value2,
                               float          value3)
{
  gsk_ngl_uniform_state_set4f (self->uniforms,
                               self->program_info,
                               key,
                               stamp,
                               value0, value1, value2, value3);
}

static inline void
gsk_ngl_program_set_uniform_color (GskNglProgram *self,
                                   guint          key,
                                   guint          stamp,
                                   const GdkRGBA *color)
{
  gsk_ngl_uniform_state_set_color (self->uniforms,
                                   self->program_info,
                                   key,
                                   stamp,
                                   color);
}

static inline void
gsk_ngl_program_set_uniform_texture (GskNglProgram *self,
                                     guint          key,
                                     guint          stamp,
                                     GLenum         texture_target,
                                     GLenum         texture_slot,
                                     guint          texture_id)
{
  gsk_ngl_attachment_state_bind_texture (self->driver->command_queue->attachments,
                                         texture_target,
                                         texture_slot,
                                         texture_id);
  gsk_ngl_uniform_state_set_texture (self->uniforms,
                                     self->program_info,
                                     key,
                                     stamp,
                                     texture_slot);
}

static inline void
gsk_ngl_program_set_uniform_matrix (GskNglProgram           *self,
                                    guint                    key,
                                    guint                    stamp,
                                    const graphene_matrix_t *matrix)
{
  gsk_ngl_uniform_state_set_matrix (self->uniforms,
                                    self->program_info,
                                    key,
                                    stamp,
                                    matrix);
}

G_END_DECLS

#endif /* __GSK_NGL_PROGRAM_PRIVATE_H__ */
