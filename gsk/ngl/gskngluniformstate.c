/* gskngluniformstate.c
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

#include "config.h"

#include <gsk/gskroundedrectprivate.h>
#include <string.h>

#include "gskngluniformstateprivate.h"

static const guint8 uniform_sizes[] = {
  0,

  sizeof (Uniform1f),
  sizeof (Uniform2f),
  sizeof (Uniform3f),
  sizeof (Uniform4f),

  sizeof (Uniform1f),
  sizeof (Uniform2f),
  sizeof (Uniform3f),
  sizeof (Uniform4f),

  sizeof (Uniform1i),
  sizeof (Uniform2i),
  sizeof (Uniform3i),
  sizeof (Uniform4i),

  sizeof (Uniform1ui),

  sizeof (guint),

  sizeof (graphene_matrix_t),
  sizeof (GskRoundedRect),
  sizeof (GdkRGBA),

  0,
};

GskNglUniformState *
gsk_ngl_uniform_state_new (void)
{
  GskNglUniformState *state;

  state = g_atomic_rc_box_new0 (GskNglUniformState);
  state->programs = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  state->values_len = 4096;
  state->values_pos = 0;
  state->values_buf = g_malloc (4096);

  memset (state->apply_hash, 0, sizeof state->apply_hash);

  return g_steal_pointer (&state);
}

GskNglUniformState *
gsk_ngl_uniform_state_ref (GskNglUniformState *state)
{
  return g_atomic_rc_box_acquire (state);
}

static void
gsk_ngl_uniform_state_finalize (gpointer data)
{
  GskNglUniformState *state = data;

  g_clear_pointer (&state->programs, g_hash_table_unref);
  g_clear_pointer (&state->values_buf, g_free);
}

void
gsk_ngl_uniform_state_unref (GskNglUniformState *state)
{
  g_atomic_rc_box_release_full (state, gsk_ngl_uniform_state_finalize);
}

gpointer
gsk_ngl_uniform_state_init_value (GskNglUniformState        *state,
                                  GskNglUniformProgram      *program,
                                  GskNglUniformFormat        format,
                                  guint                      array_count,
                                  guint                      location,
                                  GskNglUniformInfoElement **infoptr)
{
  GskNglUniformInfoElement *info;
  guint offset;

  g_assert (state != NULL);
  g_assert (array_count < 32);
  g_assert ((int)format >= 0 && format < GSK_NGL_UNIFORM_FORMAT_LAST);
  g_assert (format > 0);
  g_assert (program != NULL);
  g_assert (program->sparse != NULL);
  g_assert (program->n_sparse <= program->n_uniforms);
  g_assert (location < GL_MAX_UNIFORM_LOCATIONS || location == (guint)-1);
  g_assert (location < program->n_uniforms);

  /* Handle unused uniforms gracefully */
  if G_UNLIKELY (location == (guint)-1)
    return NULL;

  info = &program->uniforms[location];

  if G_LIKELY (format == info->info.format)
    {
      if G_LIKELY (array_count <= info->info.array_count)
        {
          *infoptr = info;
          return GSK_NGL_UNIFORM_VALUE (state->values_buf, info->info.offset);
        }

      /* We found the uniform, but there is not enough space for the
       * amount that was requested. Instead, allocate new space and
       * set the value to "initial" so that the caller just writes
       * over the previous value.
       *
       * This can happen when using dynamic array lengths like the
       * "n_color_stops" in gradient shaders.
       */
      goto setup_info;
    }
  else if (info->info.format == 0)
    {
      goto setup_info;
    }
  else
    {
      g_critical ("Attempt to access uniform with different type of value "
                  "than it was initialized with. Program %u Location %u. "
                  "Was %d now %d (array length %d now %d).",
                  program->program_id, location, info->info.format, format,
                  info->info.array_count, array_count);
      *infoptr = NULL;
      return NULL;
    }

setup_info:

  gsk_ngl_uniform_state_realloc (state,
                                 uniform_sizes[format] * MAX (1, array_count),
                                 &offset);

  /* we have 21 bits for offset */
  g_assert (offset < (1 << GSK_NGL_UNIFORM_OFFSET_BITS));

  /* We could once again be setting up this info if the array size grew.
   * So make sure that we have space in our space array for the value.
   */
  g_assert (info->info.format != 0 || program->n_sparse < program->n_uniforms);
  if (info->info.format == 0)
    program->sparse[program->n_sparse++] = location;

  info->info.format = format;
  info->info.offset = offset;
  info->info.array_count = array_count;
  info->info.initial = TRUE;
  info->stamp = 0;

  *infoptr = info;

  return GSK_NGL_UNIFORM_VALUE (state->values_buf, info->info.offset);
}

void
gsk_ngl_uniform_state_end_frame (GskNglUniformState *state)
{
  GHashTableIter iter;
  GskNglUniformProgram *program;
  guint allocator = 0;

  g_return_if_fail (state != NULL);

  /* After a frame finishes, we want to remove all our copies of uniform
   * data that isn't needed any longer. Since we treat it as uninitialized
   * after this frame (to reset it on first use next frame) we can just
   * discard it but keep an allocation around to reuse.
   */

  g_hash_table_iter_init (&iter, state->programs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&program))
    {
      for (guint j = 0; j < program->n_sparse; j++)
        {
          guint location = program->sparse[j];
          GskNglUniformInfoElement *info = &program->uniforms[location];
          guint size;

          g_assert (info->info.format > 0);

          /* Calculate how much size is needed for the uniform, including arrays */
          size = uniform_sizes[info->info.format] * MAX (1, info->info.array_count);

          /* Adjust alignment for value */
          allocator += gsk_ngl_uniform_state_align (allocator, size);

          /* Offset is in slots of 4 bytes */
          info->info.offset = allocator / 4;
          info->info.initial = TRUE;
          info->stamp = 0;

          /* Now advance for this items data */
          allocator += size;
        }
    }

  state->values_pos = allocator;

  g_assert (allocator <= state->values_len);

  memset (state->apply_hash, 0, sizeof state->apply_hash);
}

gsize
gsk_ngl_uniform_format_size (GskNglUniformFormat format)
{
  g_assert (format > 0);
  g_assert (format < GSK_NGL_UNIFORM_FORMAT_LAST);

  return uniform_sizes[format];
}

GskNglUniformProgram *
gsk_ngl_uniform_state_get_program (GskNglUniformState *state,
                                   guint               program,
                                   guint               n_uniforms)
{
  GskNglUniformProgram *ret;

  g_return_val_if_fail (state != NULL, NULL);
  g_return_val_if_fail (program > 0, NULL);
  g_return_val_if_fail (program < G_MAXUINT, NULL);

  ret = g_hash_table_lookup (state->programs, GUINT_TO_POINTER (program));

  if (ret == NULL)
    {
      gsize uniform_size = n_uniforms * sizeof (GskNglUniformInfoElement);
      gsize sparse_size = n_uniforms * sizeof (guint);
      gsize size = sizeof (GskNglUniformProgram) + uniform_size + sparse_size;

      /* Must be multiple of 4 for space pointer to align */
      G_STATIC_ASSERT (sizeof (GskNglUniformInfoElement) == 8);

      ret = g_malloc0 (size);
      ret->program_id = program;
      ret->n_uniforms = n_uniforms;
      ret->n_sparse = 0;
      ret->sparse = (guint *)&ret->uniforms[n_uniforms];

      for (guint i = 0; i < n_uniforms; i++)
        ret->uniforms[i].info.initial = TRUE;

      g_hash_table_insert (state->programs, GUINT_TO_POINTER (program), ret);
    }

  return ret;
}
