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

#include "gskspvwriterprivate.h"

#include "gskslnodeprivate.h"
#include "gskslpointertypeprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"

struct _GskSpvWriter
{
  int ref_count;

  guint32 last_id;
  GArray *code[GSK_SPV_WRITER_N_SECTIONS];

  guint32 entry_point;
  GHashTable *types;
  GHashTable *pointer_types;
  GHashTable *values;
  GHashTable *variables;
};

GskSpvWriter *
gsk_spv_writer_new (void)
{
  GskSpvWriter *writer;
  guint i;
  
  writer = g_slice_new0 (GskSpvWriter);
  writer->ref_count = 1;

  for (i = 0; i < GSK_SPV_WRITER_N_SECTIONS; i++)
    {
      writer->code[i] = g_array_new (FALSE, FALSE, sizeof (guint32));
    }

  writer->types = g_hash_table_new_full (gsk_sl_type_hash, gsk_sl_type_equal,
                                         (GDestroyNotify) gsk_sl_type_unref, NULL);
  writer->pointer_types = g_hash_table_new_full (gsk_sl_pointer_type_hash, gsk_sl_pointer_type_equal,
                                                 (GDestroyNotify) gsk_sl_pointer_type_unref, NULL);
  writer->values = g_hash_table_new_full (gsk_sl_value_hash, gsk_sl_value_equal,
                                          (GDestroyNotify) gsk_sl_value_free, NULL);
  writer->variables = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             (GDestroyNotify) gsk_sl_variable_unref, NULL);
  /* the ID 1 is reserved for the GLSL instruction set (for now) */
  writer->last_id = 1;

  return writer;
}

GskSpvWriter *
gsk_spv_writer_ref (GskSpvWriter *writer)
{
  g_return_val_if_fail (writer != NULL, NULL);

  writer->ref_count += 1;

  return writer;
}

void
gsk_spv_writer_unref (GskSpvWriter *writer)
{
  guint i;

  if (writer == NULL)
    return;

  writer->ref_count -= 1;
  if (writer->ref_count > 0)
    return;

  for (i = 0; i < GSK_SPV_WRITER_N_SECTIONS; i++)
    {
      g_array_free (writer->code[i], TRUE);
    }

  g_hash_table_destroy (writer->pointer_types);
  g_hash_table_destroy (writer->types);
  g_hash_table_destroy (writer->values);
  g_hash_table_destroy (writer->variables);

  g_slice_free (GskSpvWriter, writer);
}

#define STRING(s, offset) ((guint32) ((s)[offset + 0] | ((s)[offset + 1] << 8) | ((s)[offset + 2] << 16) | ((s)[offset + 3] << 24)))
static void
gsk_spv_writer_write_header (GskSpvWriter *writer)
{
  guchar instruction_set[] = "\0\0\0\0GLSL.std.450\0\0\0\0";

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_HEADER,
                      2, GSK_SPV_OP_CAPABILITY,
                      (guint32[1]) { GSK_SPV_CAPABILITY_SHADER });
  *(guint32 *) instruction_set = 1;
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_HEADER,
                      1 + sizeof (instruction_set) / 4, GSK_SPV_OP_EXT_INST_IMPORT,
                      (guint32 *) instruction_set);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_HEADER,
                      3, GSK_SPV_OP_MEMORY_MODEL,
                      (guint32[2]) { GSK_SPV_ADDRESSING_LOGICAL,
                                     GSK_SPV_MEMORY_GLSL450 });
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_HEADER,
                      5, GSK_SPV_OP_ENTRY_POINT,
                      (guint32[4]) { GSK_SPV_EXECUTION_MODEL_FRAGMENT,
                                     writer->entry_point,
                                     STRING ("main", 0),
                                     0 });
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_HEADER,
                      3, GSK_SPV_OP_EXECUTION_MODE,
                      (guint32[4]) { writer->entry_point,
                                     GSK_SPV_EXECUTION_MODE_ORIGIN_UPPER_LEFT });
}

static void
gsk_spv_writer_clear_header (GskSpvWriter *writer)
{
  g_array_set_size (writer->code[GSK_SPV_WRITER_SECTION_HEADER], 0);
}

GBytes *
gsk_spv_writer_write (GskSpvWriter *writer)
{
  GArray *array;
  gsize size;
  guint i;

  gsk_spv_writer_write_header (writer);

  array = g_array_new (FALSE, FALSE, sizeof (guint32));

  g_array_append_val (array, (guint32) { GSK_SPV_MAGIC_NUMBER });
  g_array_append_val (array, (guint32) { (GSK_SPV_VERSION_MAJOR << 16) | (GSK_SPV_VERSION_MINOR << 8) });
  g_array_append_val (array, (guint32) { GSK_SPV_GENERATOR });
  g_array_append_val (array, (guint32) { writer->last_id + 1 });
  g_array_append_val (array, (guint32) { 0 });
  
  for (i = 0; i < GSK_SPV_WRITER_N_SECTIONS; i++)
    {
      g_array_append_vals (array, writer->code[i]->data, writer->code[i]->len);
    }

  gsk_spv_writer_clear_header (writer);

  size = array->len * sizeof (guint32);
  return g_bytes_new_take (g_array_free (array, FALSE), size);
}

guint32
gsk_spv_writer_get_id_for_type (GskSpvWriter *writer,
                                GskSlType    *type)
{
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->types, type));
  if (result != 0)
    return result;

  result = gsk_sl_type_write_spv (type, writer);
  g_hash_table_insert (writer->types, gsk_sl_type_ref (type), GUINT_TO_POINTER (result));
  return result;
}

guint32
gsk_spv_writer_get_id_for_pointer_type (GskSpvWriter       *writer,
                                        GskSlPointerType   *type)
{
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->pointer_types, type));
  if (result != 0)
    return result;

  result = gsk_sl_pointer_type_write_spv (type, writer);
  g_hash_table_insert (writer->pointer_types, gsk_sl_pointer_type_ref (type), GUINT_TO_POINTER (result));
  return result;
}

guint32
gsk_spv_writer_get_id_for_value (GskSpvWriter *writer,
                                 GskSlValue   *value)
{
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->values, value));
  if (result != 0)
    return result;

  result = gsk_sl_value_write_spv (value, writer);
  g_hash_table_insert (writer->values, gsk_sl_value_copy (value), GUINT_TO_POINTER (result));
  return result;
}

guint32
gsk_spv_writer_get_id_for_variable (GskSpvWriter  *writer,
                                    GskSlVariable *variable)
{
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->variables, variable));
  if (result != 0)
    return result;

  result = gsk_sl_variable_write_spv (variable, writer);
  g_hash_table_insert (writer->variables, gsk_sl_variable_ref (variable), GUINT_TO_POINTER (result));
  return result;
}

guint32
gsk_spv_writer_next_id (GskSpvWriter *writer)
{
  writer->last_id++;

  return writer->last_id;
}

void
gsk_spv_writer_set_entry_point (GskSpvWriter *writer,
                                guint32       entry_point)
{
  writer->entry_point = entry_point;
}

void
gsk_spv_writer_add (GskSpvWriter        *writer,
                    GskSpvWriterSection  section,
                    guint16              word_count,
                    guint16              opcode,
                    guint32             *words)
{
  guint32 word;

  word = word_count << 16 | opcode;
  g_array_append_val (writer->code[section], word);
  g_array_append_vals (writer->code[section], words, word_count - 1);
}

