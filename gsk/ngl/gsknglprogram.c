/* gsknglprogram.c
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

#include "gsknglcommandqueueprivate.h"
#include "gsknglprogramprivate.h"
#include "gskngluniformstateprivate.h"

G_DEFINE_TYPE (GskNglProgram, gsk_ngl_program, G_TYPE_OBJECT)

GskNglProgram *
gsk_ngl_program_new (GskNglDriver *driver,
                     const char   *name,
                     int           program_id)
{
  GskNglProgram *self;

  g_return_val_if_fail (GSK_IS_NGL_DRIVER (driver), NULL);
  g_return_val_if_fail (program_id >= -1, NULL);

  self = g_object_new (GSK_TYPE_GL_PROGRAM, NULL);
  self->id = program_id;
  self->name = g_strdup (name);
  self->driver = g_object_ref (driver);
  self->n_uniforms = 0;

  return self;
}

static void
gsk_ngl_program_finalize (GObject *object)
{
  GskNglProgram *self = (GskNglProgram *)object;

  if (self->id >= 0)
    g_warning ("Leaking GLSL program %d (%s)",
               self->id,
               self->name ? self->name : "");

  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->driver);

  G_OBJECT_CLASS (gsk_ngl_program_parent_class)->finalize (object);
}

static void
gsk_ngl_program_class_init (GskNglProgramClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_ngl_program_finalize;
}

static void
gsk_ngl_program_init (GskNglProgram *self)
{
  self->id = -1;

  for (guint i = 0; i < G_N_ELEMENTS (self->uniform_locations); i++)
    self->uniform_locations[i] = -1;
}

/**
 * gsk_ngl_program_add_uniform:
 * @self: a #GskNglProgram
 * @name: the name of the uniform such as "u_source"
 * @key: the identifier to use for the uniform
 *
 * This method will create a mapping between @key and the location
 * of the uniform on the GPU. This simplifies calling code to not
 * need to know where the uniform location is and only register it
 * when creating the program.
 *
 * You might use this with an enum of all your uniforms for the
 * program and then register each of them like:
 *
 * ```
 * gsk_ngl_program_add_uniform (program, "u_source", UNIFORM_SOURCE);
 * ```
 *
 * That allows you to set values for the program with something
 * like the following:
 *
 * ```
 * gsk_ngl_program_set_uniform1i (program, UNIFORM_SOURCE, 1);
 * ```
 *
 * Returns: %TRUE if the uniform was found; otherwise %FALSE
 */
gboolean
gsk_ngl_program_add_uniform (GskNglProgram *self,
                             const char    *name,
                             guint          key)
{
  GLint location;

  g_return_val_if_fail (GSK_IS_NGL_PROGRAM (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (key < 1024, FALSE);

  if (-1 == (location = glGetUniformLocation (self->id, name)))
    return FALSE;

  self->uniform_locations[key] = location;

  if (location >= self->n_uniforms)
    self->n_uniforms = location + 1;

#if 0
  g_print ("program [%d] %s uniform %s at location %d.\n",
           self->id, self->name, name, location);
#endif

  return TRUE;
}

/**
 * gsk_ngl_program_delete:
 * @self: a #GskNglProgram
 *
 * Deletes the GLSL program.
 *
 * You must call gsk_ngl_program_use() before and
 * gsk_ngl_program_unuse() after this function.
 */
void
gsk_ngl_program_delete (GskNglProgram *self)
{
  g_return_if_fail (GSK_IS_NGL_PROGRAM (self));
  g_return_if_fail (self->driver->command_queue != NULL);

  gsk_ngl_command_queue_delete_program (self->driver->command_queue, self->id);
  self->id = -1;
}

/**
 * gsk_ngl_program_uniforms_added:
 * @self: a #GskNglProgram
 * @has_attachments: if any uniform is for a bind/texture attachment
 *
 * This function should be called after all of the uniforms ahve
 * been added with gsk_ngl_program_add_uniform().
 *
 * This function will setup the uniform state so that the program
 * has fast access to the data buffers without as many lookups at
 * runtime for comparison data.
 */
void
gsk_ngl_program_uniforms_added (GskNglProgram *self,
                                gboolean       has_attachments)
{
  g_return_if_fail (GSK_IS_NGL_PROGRAM (self));
  g_return_if_fail (self->uniforms == NULL);

  self->uniforms = self->driver->command_queue->uniforms;
  self->program_info = gsk_ngl_uniform_state_get_program (self->uniforms, self->id, self->n_uniforms);
  self->program_info->has_attachments = has_attachments;
}
