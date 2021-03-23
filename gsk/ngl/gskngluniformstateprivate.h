/* gskngluniformstateprivate.h
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

#ifndef GSK_NGL_UNIFORM_STATE_PRIVATE_H
#define GSK_NGL_UNIFORM_STATE_PRIVATE_H

#include "gskngltypesprivate.h"

G_BEGIN_DECLS

typedef struct { float v0; } Uniform1f;
typedef struct { float v0; float v1; } Uniform2f;
typedef struct { float v0; float v1; float v2; } Uniform3f;
typedef struct { float v0; float v1; float v2; float v3; } Uniform4f;

typedef struct { int v0; } Uniform1i;
typedef struct { int v0; int v1; } Uniform2i;
typedef struct { int v0; int v1; int v2; } Uniform3i;
typedef struct { int v0; int v1; int v2; int v3; } Uniform4i;

typedef struct { guint v0; } Uniform1ui;

#define GSK_NGL_UNIFORM_ARRAY_BITS  5
#define GSK_NGL_UNIFORM_FORMAT_BITS 5
#define GSK_NGL_UNIFORM_OFFSET_BITS 21

typedef struct _GskNglUniformInfo
{
  guint initial     : 1;
  guint format      : GSK_NGL_UNIFORM_FORMAT_BITS;
  guint array_count : GSK_NGL_UNIFORM_ARRAY_BITS;
  guint offset      : GSK_NGL_UNIFORM_OFFSET_BITS;
} GskNglUniformInfo;

G_STATIC_ASSERT (sizeof (GskNglUniformInfo) == 4);

typedef struct _GskNglUniformMapping
{
  const char *name;
  GskNglUniformInfo info;
  guint stamp;
  int location;
} GskNglUniformMapping;

typedef struct _GskNglUniformProgram
{
  guint program_id;
  guint n_uniforms : 12;
  guint has_attachments : 1;
  guint n_mappings;
  GskNglUniformMapping mappings[32];
} GskNglUniformProgram;

typedef struct _GskNglUniformState
{
  GHashTable *programs;
  guint8 *values_buf;
  guint values_pos;
  guint values_len;
  GskNglUniformInfo apply_hash[512];
} GskNglUniformState;

typedef enum _GskNglUniformKind
{
  GSK_NGL_UNIFORM_FORMAT_1F = 1,
  GSK_NGL_UNIFORM_FORMAT_2F,
  GSK_NGL_UNIFORM_FORMAT_3F,
  GSK_NGL_UNIFORM_FORMAT_4F,

  GSK_NGL_UNIFORM_FORMAT_1FV,
  GSK_NGL_UNIFORM_FORMAT_2FV,
  GSK_NGL_UNIFORM_FORMAT_3FV,
  GSK_NGL_UNIFORM_FORMAT_4FV,

  GSK_NGL_UNIFORM_FORMAT_1I,
  GSK_NGL_UNIFORM_FORMAT_2I,
  GSK_NGL_UNIFORM_FORMAT_3I,
  GSK_NGL_UNIFORM_FORMAT_4I,

  GSK_NGL_UNIFORM_FORMAT_1UI,

  GSK_NGL_UNIFORM_FORMAT_TEXTURE,

  GSK_NGL_UNIFORM_FORMAT_MATRIX,
  GSK_NGL_UNIFORM_FORMAT_ROUNDED_RECT,
  GSK_NGL_UNIFORM_FORMAT_COLOR,

  GSK_NGL_UNIFORM_FORMAT_LAST
} GskNglUniformFormat;

G_STATIC_ASSERT (GSK_NGL_UNIFORM_FORMAT_LAST < (1 << GSK_NGL_UNIFORM_FORMAT_BITS));

GskNglUniformState   *gsk_ngl_uniform_state_new         (void);
GskNglUniformState   *gsk_ngl_uniform_state_ref         (GskNglUniformState          *state);
void                  gsk_ngl_uniform_state_unref       (GskNglUniformState          *state);
GskNglUniformProgram *gsk_ngl_uniform_state_get_program (GskNglUniformState          *state,
                                                         guint                        program,
                                                         const GskNglUniformMapping  *mappings,
                                                         guint                        n_mappings);
void                  gsk_ngl_uniform_state_end_frame   (GskNglUniformState          *state);
gsize                 gsk_ngl_uniform_format_size       (GskNglUniformFormat          format);
gpointer              gsk_ngl_uniform_state_init_value  (GskNglUniformState          *state,
                                                         GskNglUniformProgram        *program,
                                                         GskNglUniformFormat          format,
                                                         guint                        array_count,
                                                         guint                        key,
                                                         GskNglUniformMapping       **out_mapping);

#define GSK_NGL_UNIFORM_VALUE(base, offset) ((gpointer)((base) + ((offset) * 4)))
#define gsk_ngl_uniform_state_get_uniform_data(state,offset) GSK_NGL_UNIFORM_VALUE((state)->values_buf, offset)

static inline gpointer
gsk_ngl_uniform_state_get_value (GskNglUniformState    *state,
                                 GskNglUniformProgram  *program,
                                 GskNglUniformFormat    format,
                                 guint                  array_count,
                                 guint                  key,
                                 guint                  stamp,
                                 GskNglUniformMapping **out_mapping)
{
  GskNglUniformMapping *mapping;

  g_assert (key < G_N_ELEMENTS (program->mappings));
  g_assert (key < program->n_mappings);

  mapping = &program->mappings[key];

  /* Short-circuit if the program optimized the uniform out */
  if (mapping->location == -1)
    return NULL;

  /* If the stamp is the same, then we can ignore the request
   * and short-circuit as early as possible. This requires the
   * caller to increment their private stamp when they change
   * internal state.
   *
   * This is generally used for the shared uniforms like projection,
   * modelview, clip, etc to avoid so many comparisons which cost
   * considerable CPU.
   */
  if (stamp != 0 && stamp == mapping->stamp)
    return NULL;

  if G_LIKELY (format == mapping->info.format && array_count <= mapping->info.array_count)
    {
      *out_mapping = mapping;
      return GSK_NGL_UNIFORM_VALUE (state->values_buf, mapping->info.offset);
    }

  return gsk_ngl_uniform_state_init_value (state, program, format, array_count, key, out_mapping);
}

static inline guint
gsk_ngl_uniform_state_align (guint current_pos,
                             guint size)
{
  guint align = size > 8 ? 16 : (size > 4 ? 8 : 4);
  guint masked = current_pos & (align - 1);

  g_assert (size > 0);
  g_assert (align == 4 || align == 8 || align == 16);
  g_assert (masked < align);

  return align - masked;
}

static inline gpointer
gsk_ngl_uniform_state_realloc (GskNglUniformState *state,
                               guint               size,
                               guint              *offset)
{
  guint padding = gsk_ngl_uniform_state_align (state->values_pos, size);

  if G_UNLIKELY (state->values_len - padding - size < state->values_pos)
    {
      state->values_len *= 2;
      state->values_buf = g_realloc (state->values_buf, state->values_len);
    }

  /* offsets are in slots of 4 to use fewer bits */
  g_assert ((state->values_pos + padding) % 4 == 0);
  *offset = (state->values_pos + padding) / 4;
  state->values_pos += padding + size;

  return GSK_NGL_UNIFORM_VALUE (state->values_buf, *offset);
}

#define GSK_NGL_UNIFORM_STATE_REPLACE(info, u, type, count)                                \
  G_STMT_START {                                                                           \
    if ((info)->info.initial && count == (info)->info.array_count)                         \
      {                                                                                    \
        u = GSK_NGL_UNIFORM_VALUE (state->values_buf, (info)->info.offset);                \
      }                                                                                    \
    else                                                                                   \
      {                                                                                    \
        guint offset;                                                                      \
        u = gsk_ngl_uniform_state_realloc (state, sizeof(type) * MAX (1, count), &offset); \
        g_assert (offset < (1 << GSK_NGL_UNIFORM_OFFSET_BITS));                            \
        (info)->info.offset = offset;                                                      \
        /* We might have increased array length */                                         \
        (info)->info.array_count = count;                                                  \
      }                                                                                    \
  } G_STMT_END

static inline void
gsk_ngl_uniform_info_changed (GskNglUniformMapping *info,
                              guint                     stamp)
{
  info->stamp = stamp;
  info->info.initial = FALSE;
}

static inline void
gsk_ngl_uniform_state_set1f (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             float                 value0)
{
  Uniform1f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != 0);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_1F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform1f , 1);
          u->v0 = value0;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set2f (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             float                 value0,
                             float                 value1)
{
  Uniform2f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_2F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform2f, 1);
          u->v0 = value0;
          u->v1 = value1;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set3f (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             float                 value0,
                             float                 value1,
                             float                 value2)
{
  Uniform3f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_3F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform3f, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set4f (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             float                 value0,
                             float                 value1,
                             float                 value2,
                             float                 value3)
{
  Uniform4f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_4F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2 || u->v3 != value3)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform4f, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          u->v3 = value3;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set1ui (GskNglUniformState   *state,
                              GskNglUniformProgram *program,
                              guint                 key,
                              guint                 stamp,
                              guint                 value0)
{
  Uniform1ui *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_1UI, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform1ui, 1);
          u->v0 = value0;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set1i (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             int                   value0)
{
  Uniform1i *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_1I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform1i, 1);
          u->v0 = value0;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set2i (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             int                   value0,
                             int                   value1)
{
  Uniform2i *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_2I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform2i, 1);
          u->v0 = value0;
          u->v1 = value1;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set3i (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             int                   value0,
                             int                   value1,
                             int                   value2)
{
  Uniform3i *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_3I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform3i, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set4i (GskNglUniformState   *state,
                             GskNglUniformProgram *program,
                             guint                 key,
                             guint                 stamp,
                             int                   value0,
                             int                   value1,
                             int                   value2,
                             int                   value3)
{
  Uniform4i *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_4I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2 || u->v3 != value3)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform4i, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          u->v3 = value3;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set_rounded_rect (GskNglUniformState   *state,
                                        GskNglUniformProgram *program,
                                        guint                 key,
                                        guint                 stamp,
                                        const GskRoundedRect *rounded_rect)
{
  GskRoundedRect *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (rounded_rect != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_ROUNDED_RECT, 1, key, stamp, &info)))
    {
      if (info->info.initial || memcmp (u, rounded_rect, sizeof *u) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, GskRoundedRect, 1);
          memcpy (u, rounded_rect, sizeof *rounded_rect);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set_matrix (GskNglUniformState      *state,
                                  GskNglUniformProgram    *program,
                                  guint                    key,
                                  guint                    stamp,
                                  const graphene_matrix_t *matrix)
{
  graphene_matrix_t *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (matrix != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_MATRIX, 1, key, stamp, &info)))
    {
      if (info->info.initial || memcmp (u, matrix, sizeof *u) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, graphene_matrix_t, 1);
          memcpy (u, matrix, sizeof *matrix);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

/**
 * gsk_ngl_uniform_state_set_texture:
 * @state: a #GskNglUniformState
 * @program: the program id
 * @location: the location of the texture
 * @texture_slot: a texturing slot such as GL_TEXTURE0
 *
 * Sets the uniform expecting a texture to @texture_slot. This API
 * expects a texture slot such as GL_TEXTURE0 to reduce chances of
 * miss-use by the caller.
 *
 * The value stored to the uniform is in the form of 0 for GL_TEXTURE0,
 * 1 for GL_TEXTURE1, and so on.
 */
static inline void
gsk_ngl_uniform_state_set_texture (GskNglUniformState   *state,
                                   GskNglUniformProgram *program,
                                   guint                 key,
                                   guint                 stamp,
                                   guint                 texture_slot)
{
  GskNglUniformMapping *info;
  guint *u;

  g_assert (texture_slot >= GL_TEXTURE0);
  g_assert (texture_slot < GL_TEXTURE16);

  texture_slot -= GL_TEXTURE0;

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_TEXTURE, 1, key, stamp, &info)))
    {
      if (info->info.initial || *u != texture_slot)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, guint, 1);
          *u = texture_slot;
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

/**
 * gsk_ngl_uniform_state_set_color:
 * @state: a #GskNglUniformState
 * @program: a program id > 0
 * @location: the uniform location
 * @color: a color to set or %NULL for transparent
 *
 * Sets a uniform to the color described by @color. This is a convenience
 * function to allow callers to avoid having to translate colors to floats
 * in other portions of the renderer.
 */
static inline void
gsk_ngl_uniform_state_set_color (GskNglUniformState   *state,
                                 GskNglUniformProgram *program,
                                 guint                 key,
                                 guint                 stamp,
                                 const GdkRGBA        *color)
{
  static const GdkRGBA transparent = {0};
  GskNglUniformMapping *info;
  GdkRGBA *u;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_COLOR, 1, key, stamp, &info)))
    {
      if (color == NULL)
        color = &transparent;

      if (info->info.initial || memcmp (color, u, sizeof *u) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, GdkRGBA, 1);
          memcpy (u, color, sizeof *color);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set1fv (GskNglUniformState   *state,
                              GskNglUniformProgram *program,
                              guint                 key,
                              guint                 stamp,
                              guint                 count,
                              const float          *value)
{
  Uniform1f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_1FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform1f, count);
          memcpy (u, value, sizeof (Uniform1f) * count);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set2fv (GskNglUniformState   *state,
                              GskNglUniformProgram *program,
                              guint                 key,
                              guint                 stamp,
                              guint                 count,
                              const float          *value)
{
  Uniform2f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_2FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform2f, count);
          memcpy (u, value, sizeof (Uniform2f) * count);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set3fv (GskNglUniformState   *state,
                              GskNglUniformProgram *program,
                              guint                 key,
                              guint                 stamp,
                              guint                 count,
                              const float          *value)
{
  Uniform3f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_3FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform3f, count);
          memcpy (u, value, sizeof (Uniform3f) * count);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_ngl_uniform_state_set4fv (GskNglUniformState   *state,
                              GskNglUniformProgram *program,
                              guint                 key,
                              guint                 stamp,
                              guint                 count,
                              const float          *value)
{
  Uniform4f *u;
  GskNglUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_ngl_uniform_state_get_value (state, program, GSK_NGL_UNIFORM_FORMAT_4FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_NGL_UNIFORM_STATE_REPLACE (info, u, Uniform4f, count);
          memcpy (u, value, sizeof (Uniform4f) * count);
          gsk_ngl_uniform_info_changed (info, stamp);
        }
    }
}

static inline guint
gsk_ngl_uniform_state_fmix (guint program,
                            guint location)
{
  guint h = (program << 16) | location;

   h ^= h >> 16;
   h *= 0x85ebca6b;
   h ^= h >> 13;
   h *= 0xc2b2ae35;
   h ^= h >> 16;

   return h;
}

/*
 * gsk_ngl_uniform_state_apply:
 * @state: the uniform state
 * @program: the program id
 * @location: the location of the uniform
 * @offset: the offset of the data within the buffer
 * @info: the uniform info
 *
 * This function can be used to apply state that was previously recorded
 * by the #GskNglUniformState.
 *
 * It is specifically useful from the GskNglCommandQueue to execute uniform
 * changes but only when they have changed from the current value.
 */
static inline void
gsk_ngl_uniform_state_apply (GskNglUniformState *state,
                             guint               program,
                             guint               location,
                             GskNglUniformInfo   info)
{
  guint index = gsk_ngl_uniform_state_fmix (program, location) % G_N_ELEMENTS (state->apply_hash);
  gconstpointer dataptr = GSK_NGL_UNIFORM_VALUE (state->values_buf, info.offset);

  /* aligned, can treat as unsigned */
  if (*(guint *)&info == *(guint *)&state->apply_hash[index])
    return;

  state->apply_hash[index] = info;

  /* TODO: We could do additional comparisons here to make sure we are
   *       changing state.
   */

  switch (info.format)
    {
    case GSK_NGL_UNIFORM_FORMAT_1F:
      glUniform1fv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_2F:
      glUniform2fv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_3F:
      glUniform3fv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_4F:
      glUniform4fv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_1FV:
      glUniform1fv (location, info.array_count, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_2FV:
      glUniform2fv (location, info.array_count, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_3FV:
      glUniform3fv (location, info.array_count, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_4FV:
      glUniform4fv (location, info.array_count, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_1I:
    case GSK_NGL_UNIFORM_FORMAT_TEXTURE:
      glUniform1iv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_2I:
      glUniform2iv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_3I:
      glUniform3iv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_4I:
      glUniform4iv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_1UI:
      glUniform1uiv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_MATRIX: {
      float mat[16];
      graphene_matrix_to_float (dataptr, mat);
      glUniformMatrix4fv (location, 1, GL_FALSE, mat);
#if 0
      /* TODO: If Graphene can give us a peek here on platforms
       * where the format is float[16] (most/all x86_64?) then
       * We can avoid the SIMD operation to convert the format.
       */
      G_STATIC_ASSERT (sizeof (graphene_matrix_t) == 16*4);
      glUniformMatrix4fv (location, 1, GL_FALSE, dataptr);
#endif
    }
    break;

    case GSK_NGL_UNIFORM_FORMAT_COLOR:
      glUniform4fv (location, 1, dataptr);
    break;

    case GSK_NGL_UNIFORM_FORMAT_ROUNDED_RECT:
      glUniform4fv (location, 3, dataptr);
    break;

    default:
      g_assert_not_reached ();
    }
}

G_END_DECLS

#endif /* GSK_NGL_UNIFORM_STATE_PRIVATE_H */
