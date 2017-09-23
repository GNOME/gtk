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

#include "gskslnodeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlProgram {
  GObject parent_instance;

  GskSlScope *scope;
  GSList *declarations;
  GSList *functions;
};

G_DEFINE_TYPE (GskSlProgram, gsk_sl_program, G_TYPE_OBJECT)

static void
gsk_sl_program_dispose (GObject *object)
{
  GskSlProgram *program = GSK_SL_PROGRAM (object);

  g_slist_free (program->declarations);
  g_slist_free (program->functions);
  gsk_sl_scope_unref (program->scope);

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
  program->scope = gsk_sl_scope_new (NULL);
}

gboolean
gsk_sl_program_parse (GskSlProgram      *program,
                      GskSlPreprocessor *preproc)
{
  const GskSlToken *token;
  gboolean result = TRUE;

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      GskSlNode *node = gsk_sl_node_parse_function_definition (program->scope, preproc);

      if (node)
        {
          program->functions = g_slist_append (program->functions, node);
        }
      else
        {
          gsk_sl_preprocessor_consume (preproc, (GskSlNode *) program);
          result = FALSE;
        }
    }

  return result;
}

void
gsk_sl_program_print (GskSlProgram *program,
                      GString      *string)
{
  GSList *l;

  g_return_if_fail (GSK_IS_SL_PROGRAM (program));
  g_return_if_fail (string != NULL);

  for (l = program->declarations; l; l = l->next)
    gsk_sl_node_print (l->data, string);

  for (l = program->functions; l; l = l->next)
    {
      if (l != program->functions || program->declarations != NULL)
        g_string_append (string, "\n");
      gsk_sl_node_print (l->data, string);
    }
}

static void
gsk_sl_program_write_spv (GskSlProgram *program,
                          GskSpvWriter *writer)
{
  GSList *l;

  for (l = program->declarations; l; l = l->next)
    gsk_sl_node_write_spv (l->data, writer);

#if 0
  for (l = program->functions; l; l = l->next)
    {
      guint32 id = gsk_sl_node_write_spv (l->data, writer);

      if (g_str_equal (((GskSlNodeFunction *) l->data)->name, "main"))
        gsk_spv_writer_set_entry_point (writer, id);
    }
#endif
}

GBytes *
gsk_sl_program_to_spirv (GskSlProgram *program)
{
  GskSpvWriter *writer;
  GBytes *bytes;

  g_return_val_if_fail (GSK_IS_SL_PROGRAM (program), NULL);

  writer = gsk_spv_writer_new ();

  gsk_sl_program_write_spv (program, writer);
  bytes = gsk_spv_writer_write (writer);

  gsk_spv_writer_unref (writer);

  return bytes;
}
