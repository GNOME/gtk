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

#include "gskslprogramprivate.h"

#include "gsksldeclarationprivate.h"
#include "gskslenvironmentprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlProgram {
  GObject parent_instance;

  GskSlScope *scope;
  GSList *declarations;
};

G_DEFINE_TYPE (GskSlProgram, gsk_sl_program, G_TYPE_OBJECT)

static void
gsk_sl_program_dispose (GObject *object)
{
  GskSlProgram *program = GSK_SL_PROGRAM (object);

  g_slist_free_full (program->declarations, (GDestroyNotify) gsk_sl_declaration_unref);
  g_clear_pointer (&program->scope, gsk_sl_scope_unref);

  G_OBJECT_CLASS (gsk_sl_program_parent_class)->dispose (object);
}

static void
gsk_sl_program_class_init (GskSlProgramClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_sl_program_dispose;
}

static void
gsk_sl_program_init (GskSlProgram *program)
{
}

void
gsk_sl_program_parse (GskSlProgram      *program,
                      GskSlPreprocessor *preproc)
{
  GskSlDeclaration *declaration;
  const GskSlToken *token;

  program->scope = gsk_sl_environment_create_scope (gsk_sl_preprocessor_get_environment (preproc));

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      declaration = gsk_sl_declaration_parse (program->scope, preproc);
      if (declaration)
        program->declarations = g_slist_prepend (program->declarations, declaration);
    }

  program->declarations = g_slist_reverse (program->declarations);
}

void
gsk_sl_program_print (GskSlProgram *program,
                      GString      *string)
{
  GskSlPrinter *printer;
  GskSlFunction *function;
  gboolean need_newline = FALSE;
  GSList *l;
  char *str;

  g_return_if_fail (GSK_IS_SL_PROGRAM (program));
  g_return_if_fail (string != NULL);

  printer = gsk_sl_printer_new ();

  for (l = program->declarations; l; l = l->next)
    {
      function = gsk_sl_declaration_get_function (l->data);

      if ((function || need_newline) && l != program->declarations)
        gsk_sl_printer_newline (printer);

      gsk_sl_declaration_print (l->data, printer);

      need_newline = function != NULL;
    }

  str = gsk_sl_printer_write_to_string (printer);
  g_string_append (string, str);
  g_free (str);

  gsk_sl_printer_unref (printer);
}

static void
gsk_sl_program_write_spv_initializer (GskSpvWriter *writer,
                                      gpointer      data)
{
  GskSlProgram *program = data;
  GSList *l;

  for (l = program->declarations; l; l = l->next)
    gsk_sl_declaration_write_initializer_spv (l->data, writer);
}

static GskSlFunction *
gsk_sl_program_get_entry_point (GskSlProgram *program)
{
  GSList *l;

  for (l = program->declarations; l; l = l->next)
    {
      GskSlFunction *function = gsk_sl_declaration_get_function (l->data);

      if (function && g_str_equal (gsk_sl_function_get_name (function), "main"))
        return function;
    }

  return NULL;
}

GBytes *
gsk_sl_program_to_spirv (GskSlProgram *program)
{
  GskSpvWriter *writer;
  GBytes *bytes;

  g_return_val_if_fail (GSK_IS_SL_PROGRAM (program), NULL);

  writer = gsk_spv_writer_new ();

  bytes = gsk_spv_writer_write (writer,
                                gsk_sl_program_get_entry_point (program),
                                gsk_sl_program_write_spv_initializer,
                                program);

  gsk_spv_writer_unref (writer);

  return bytes;
}
