/* gsknglbufferprivate.h
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

#ifndef __GSK_NGL_BUFFER_PRIVATE_H__
#define __GSK_NGL_BUFFER_PRIVATE_H__

#include "gskngltypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskNglBuffer
{
  guint8 *buffer;
  gsize   buffer_pos;
  gsize   buffer_len;
  guint   count;
  GLenum  target;
  gsize   element_size;
} GskNglBuffer;

void   gsk_ngl_buffer_init    (GskNglBuffer *self,
                               GLenum        target,
                               guint         element_size);
void   gsk_ngl_buffer_destroy (GskNglBuffer *buffer);
GLuint gsk_ngl_buffer_submit  (GskNglBuffer *buffer);

static inline gpointer
gsk_ngl_buffer_advance (GskNglBuffer *buffer,
                        guint         count)
{
  gpointer ret;
  gsize to_alloc = count * buffer->element_size;

  if G_UNLIKELY (buffer->buffer_pos + to_alloc > buffer->buffer_len)
    {
      while (buffer->buffer_pos + to_alloc > buffer->buffer_len)
        buffer->buffer_len *= 2;
      buffer->buffer = g_realloc (buffer->buffer, buffer->buffer_len);
    }

  ret = buffer->buffer + buffer->buffer_pos;

  buffer->buffer_pos += to_alloc;
  buffer->count += count;

  return ret;
}

static inline void
gsk_ngl_buffer_retract (GskNglBuffer *buffer,
                        guint         count)
{
  buffer->buffer_pos -= count * buffer->element_size;
  buffer->count -= count;
}

static inline guint
gsk_ngl_buffer_get_offset (GskNglBuffer *buffer)
{
  return buffer->count;
}

G_END_DECLS

#endif /* __GSK_NGL_BUFFER_PRIVATE_H__ */
