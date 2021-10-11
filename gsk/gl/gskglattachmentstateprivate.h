/* gskglattachmentstateprivate.h
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

#ifndef __GSK_GL_ATTACHMENT_STATE_PRIVATE_H__
#define __GSK_GL_ATTACHMENT_STATE_PRIVATE_H__

#include "gskgltypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskGLAttachmentState GskGLAttachmentState;
typedef struct _GskGLBindFramebuffer GskGLBindFramebuffer;
typedef struct _GskGLBindTexture     GskGLBindTexture;

struct _GskGLBindTexture
{
  guint changed : 1;
  guint initial : 1;
  GLenum target : 30;
  GLenum texture;
  guint id;
};

G_STATIC_ASSERT (sizeof (GskGLBindTexture) == 12);

struct _GskGLBindFramebuffer
{
  guint changed : 1;
  guint id : 31;
};

G_STATIC_ASSERT (sizeof (GskGLBindFramebuffer) == 4);

struct _GskGLAttachmentState
{
  GskGLBindFramebuffer fbo;
  /* Increase if shaders add more textures */
  GskGLBindTexture textures[4];
  guint n_changed;
};

GskGLAttachmentState *gsk_gl_attachment_state_new              (void);
GskGLAttachmentState *gsk_gl_attachment_state_ref              (GskGLAttachmentState *self);
void                  gsk_gl_attachment_state_unref            (GskGLAttachmentState *self);
void                  gsk_gl_attachment_state_bind_texture     (GskGLAttachmentState *self,
                                                                GLenum                 target,
                                                                GLenum                 texture,
                                                                guint                  id);
void                  gsk_gl_attachment_state_bind_framebuffer (GskGLAttachmentState *self,
                                                                guint                  id);

G_END_DECLS

#endif /* __GSK_GL_ATTACHMENT_STATE_PRIVATE_H__ */
