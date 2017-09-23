/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gskslcompiler.h"

#include "gskslpreprocessorprivate.h"
#include "gskslprogramprivate.h"

struct _GskSlCompiler {
  GObject parent_instance;
};

G_DEFINE_TYPE (GskSlCompiler, gsk_sl_compiler, G_TYPE_OBJECT)

static void
gsk_sl_compiler_dispose (GObject *object)
{
  //GskSlCompiler *compiler = GSK_SL_COMPILER (object);

  G_OBJECT_CLASS (gsk_sl_compiler_parent_class)->dispose (object);
}

static void
gsk_sl_compiler_class_init (GskSlCompilerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_sl_compiler_dispose;
}

static void
gsk_sl_compiler_init (GskSlCompiler *compiler)
{
}

GskSlCompiler *
gsk_sl_compiler_new (void)
{
  return g_object_new (GSK_TYPE_SL_COMPILER, NULL);
}

GskSlProgram *
gsk_sl_compiler_compile (GskSlCompiler *compiler,
                         GBytes        *source)
{
  GskSlPreprocessor *preproc;
  GskSlProgram *program;

  program = g_object_new (GSK_TYPE_SL_PROGRAM, NULL);

  preproc = gsk_sl_preprocessor_new (compiler, source);

  if (!gsk_sl_program_parse (program, preproc))
    {
      g_object_unref (program);
      program = NULL;
    }

  gsk_sl_preprocessor_unref (preproc);

  return program;
}

