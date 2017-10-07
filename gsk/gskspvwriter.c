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

#include "gskslfunctionprivate.h"
#include "gskslpointertypeprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"

struct _GskSpvCodeBlock
{
  GArray *code;

  guint32 label;
};

struct _GskSpvWriter
{
  int ref_count;

  guint32 last_id;
  GArray *code[GSK_SPV_WRITER_N_SECTIONS];
  GSList *blocks;
  GSList *pending_blocks;

  GskSlFunction *entry_point;

  GHashTable *types;
  GHashTable *pointer_types;
  GHashTable *values;
  GHashTable *variables;
  GHashTable *functions;
};

static void
gsk_spv_code_block_free (GskSpvCodeBlock *block)
{
  g_array_free (block->code, TRUE);

  g_slice_free (GskSpvCodeBlock, block);
}

GskSpvWriter *
gsk_spv_writer_new (void)
{
  GskSpvWriter *writer;
  guint i;
  
  writer = g_slice_new0 (GskSpvWriter);
  writer->ref_count = 1;

  for (i = 0; i < GSK_SPV_WRITER_N_SECTIONS; i++)
    {
      writer->code[i] = g_array_new (FALSE, TRUE, sizeof (guint32));
    }

  writer->types = g_hash_table_new_full (gsk_sl_type_hash, gsk_sl_type_equal,
                                         (GDestroyNotify) gsk_sl_type_unref, NULL);
  writer->pointer_types = g_hash_table_new_full (gsk_sl_pointer_type_hash, gsk_sl_pointer_type_equal,
                                                 (GDestroyNotify) gsk_sl_pointer_type_unref, NULL);
  writer->values = g_hash_table_new_full (gsk_sl_value_hash, gsk_sl_value_equal,
                                          (GDestroyNotify) gsk_sl_value_free, NULL);
  writer->variables = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             (GDestroyNotify) gsk_sl_variable_unref, NULL);
  writer->functions = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             (GDestroyNotify) gsk_sl_function_unref, NULL);
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

  g_slist_free_full (writer->pending_blocks, (GDestroyNotify) gsk_spv_code_block_free);

  for (i = 0; i < GSK_SPV_WRITER_N_SECTIONS; i++)
    {
      g_array_free (writer->code[i], TRUE);
    }

  g_hash_table_destroy (writer->pointer_types);
  g_hash_table_destroy (writer->types);
  g_hash_table_destroy (writer->values);
  g_hash_table_destroy (writer->variables);
  g_hash_table_destroy (writer->functions);

  g_slice_free (GskSpvWriter, writer);
}

#define STRING(s, offset) ((guint32) ((s)[offset + 0] | ((s)[offset + 1] << 8) | ((s)[offset + 2] << 16) | ((s)[offset + 3] << 24)))
static void
gsk_spv_writer_write_header (GskSpvWriter *writer)
{
  guint32 entry_point;

  gsk_spv_writer_capability (writer, GSK_SPV_CAPABILITY_SHADER);
  gsk_spv_writer_ext_inst_import (writer,
                                  "GLSL.std.450");
  gsk_spv_writer_memory_model (writer,
                               GSK_SPV_ADDRESSING_MODEL_LOGICAL,
                               GSK_SPV_MEMORY_MODEL_GLSL450);
  entry_point = gsk_spv_writer_get_id_for_function (writer, writer->entry_point);
  gsk_spv_writer_entry_point (writer,
                              GSK_SPV_EXECUTION_MODEL_FRAGMENT,
                              entry_point,
                              "main",
                              NULL,
                              0);
  gsk_spv_writer_execution_mode (writer,
                                 entry_point,
                                 GSK_SPV_EXECUTION_MODE_ORIGIN_UPPER_LEFT);
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
  GSList *l;
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

  for (l = writer->pending_blocks; l; l = l->next)
    {
      GskSpvCodeBlock *block = l->data;

      g_array_append_vals (array, block->code->data, block->code->len);
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
gsk_spv_writer_get_id_for_zero (GskSpvWriter    *writer,
                                GskSlScalarType  scalar)
{
  GskSlValue *value;
  guint32 result_id;

  value = gsk_sl_value_new (gsk_sl_type_get_scalar (scalar));
  result_id = gsk_spv_writer_get_id_for_value (writer, value);
  gsk_sl_value_free (value);

  return result_id;
}

guint32
gsk_spv_writer_get_id_for_one (GskSpvWriter    *writer,
                               GskSlScalarType  scalar)
{
  GskSlValue *value;
  guint32 result_id;

  value = gsk_sl_value_new (gsk_sl_type_get_scalar (scalar));
  switch (scalar)
    {
    case GSK_SL_INT:
      *(gint32 *) gsk_sl_value_get_data (value) = 1;
      break;

    case GSK_SL_UINT:
      *(guint32 *) gsk_sl_value_get_data (value) = 1;
      break;

    case GSK_SL_FLOAT:
      *(float *) gsk_sl_value_get_data (value) = 1;
      break;

    case GSK_SL_DOUBLE:
      *(double *) gsk_sl_value_get_data (value) = 1;
      break;

    case GSK_SL_BOOL:
      *(guint32 *) gsk_sl_value_get_data (value) = TRUE;
      break;

    case GSK_SL_VOID:
      break;

    default:
      g_assert_not_reached ();
      break;
    }
  result_id = gsk_spv_writer_get_id_for_value (writer, value);
  gsk_sl_value_free (value);

  return result_id;
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
gsk_spv_writer_get_id_for_function (GskSpvWriter  *writer,
                                    GskSlFunction *function)
{
  GskSpvCodeBlock *block;
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->functions, function));
  if (result != 0)
    return result;

  gsk_spv_writer_push_new_code_block (writer);
  result = gsk_sl_function_write_spv (function, writer);
  g_hash_table_insert (writer->functions, gsk_sl_function_ref (function), GUINT_TO_POINTER (result));
  block = gsk_spv_writer_pop_code_block (writer);
  writer->pending_blocks = g_slist_prepend (writer->pending_blocks, block);

  return result;
}

guint32
gsk_spv_writer_make_id (GskSpvWriter *writer)
{
  writer->last_id++;

  return writer->last_id;
}

void
gsk_spv_writer_set_entry_point (GskSpvWriter  *writer,
                                GskSlFunction *function)
{
  writer->entry_point = gsk_sl_function_ref (function);
}

GArray *
gsk_spv_writer_get_bytes (GskSpvWriter        *writer,
                          GskSpvWriterSection  section)
{
  if (section == GSK_SPV_WRITER_SECTION_CODE && writer->blocks)
    return ((GskSpvCodeBlock *) writer->blocks->data)->code;
  return writer->code[section];
}

guint32
gsk_spv_writer_push_new_code_block (GskSpvWriter *writer)
{
  GskSpvCodeBlock *block;

  block = g_slice_new0 (GskSpvCodeBlock);
  block->code = g_array_new (FALSE, TRUE, sizeof (guint32));

  gsk_spv_writer_push_code_block (writer, block);

  block->label = gsk_spv_writer_label (writer);

  return block->label;
}

void
gsk_spv_writer_push_code_block (GskSpvWriter    *writer,
                                GskSpvCodeBlock *block)
{
  writer->blocks = g_slist_prepend (writer->blocks, block);
}

GskSpvCodeBlock *
gsk_spv_writer_pop_code_block (GskSpvWriter *writer)
{
  GskSpvCodeBlock *result;

  result = writer->blocks->data;
  g_assert (result);

  writer->blocks = g_slist_remove (writer->blocks, result);

  return result;
}

void
gsk_spv_writer_commit_code_block (GskSpvWriter *writer)
{
  GskSpvCodeBlock *block;
  GArray *commit_target;

  block = gsk_spv_writer_pop_code_block (writer);

  commit_target = gsk_spv_writer_get_bytes (writer, GSK_SPV_WRITER_SECTION_CODE);
  g_array_append_vals (commit_target, block->code->data, block->code->len);

  gsk_spv_code_block_free (block);
}

guint32
gsk_spv_code_block_get_label (GskSpvCodeBlock *block)
{
  return block->label;
}

static void
copy_4_bytes (gpointer dest, gpointer src)
{
  memcpy (dest, src, 4);
}

static void
copy_8_bytes (gpointer dest, gpointer src)
{
  memcpy (dest, src, 8);
}

guint32
gsk_spv_writer_convert (GskSpvWriter *writer,
                        guint32       id,
                        GskSlType    *type,
                        GskSlType    *new_type)
{
  GskSlScalarType scalar = gsk_sl_type_get_scalar_type (type);
  GskSlScalarType new_scalar = gsk_sl_type_get_scalar_type (new_type);

  if (scalar == new_scalar)
    return id;

  if (gsk_sl_type_is_scalar (type) ||
      gsk_sl_type_is_vector (type))
    {
      GskSlValue *value;
      guint32 true_id, false_id, zero_id;

      switch (new_scalar)
        {
        case GSK_SL_VOID:
        default:
          g_assert_not_reached ();
          return id;

        case GSK_SL_INT:
        case GSK_SL_UINT:
          switch (scalar)
            {
            case GSK_SL_INT:
            case GSK_SL_UINT:
              return gsk_spv_writer_bitcast (writer, new_type, id);

            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              if (new_scalar == GSK_SL_UINT)
                return gsk_spv_writer_convert_f_to_u (writer, new_type, id);
              else
                return gsk_spv_writer_convert_f_to_s (writer, new_type, id);

            case GSK_SL_BOOL:
              value = gsk_sl_value_new (new_type);
              false_id = gsk_spv_writer_get_id_for_value (writer, value);
              gsk_sl_value_componentwise (value, copy_4_bytes, &(gint32) { 1 });
              true_id = gsk_spv_writer_get_id_for_value (writer, value);
              gsk_sl_value_free (value);
              return gsk_spv_writer_select (writer, new_type, id, true_id, false_id);

            case GSK_SL_VOID:
            default:
              g_assert_not_reached ();
              return id;
            }
          g_assert_not_reached ();

        case GSK_SL_FLOAT:
        case GSK_SL_DOUBLE:
          switch (scalar)
            {
            case GSK_SL_INT:
              return gsk_spv_writer_convert_s_to_f (writer, new_type, id);

            case GSK_SL_UINT:
              return gsk_spv_writer_convert_u_to_f (writer, new_type, id);

            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              return gsk_spv_writer_f_convert (writer, new_type, id);

            case GSK_SL_BOOL:
              value = gsk_sl_value_new (new_type);
              false_id = gsk_spv_writer_get_id_for_value (writer, value);
              if (scalar == GSK_SL_DOUBLE)
                gsk_sl_value_componentwise (value, copy_8_bytes, &(double) { 1 });
              else
                gsk_sl_value_componentwise (value, copy_4_bytes, &(float) { 1 });
              true_id = gsk_spv_writer_get_id_for_value (writer, value);
              gsk_sl_value_free (value);
              return gsk_spv_writer_select (writer,
                                            new_type,
                                            id,
                                            true_id,
                                            false_id);

            case GSK_SL_VOID:
            default:
              g_assert_not_reached ();
              return id;
            }
          g_assert_not_reached ();

        case GSK_SL_BOOL:
          switch (scalar)
            {
            case GSK_SL_INT:
            case GSK_SL_UINT:
              value = gsk_sl_value_new (new_type);
              zero_id = gsk_spv_writer_get_id_for_value (writer, value);
              gsk_sl_value_free (value);
              return gsk_spv_writer_i_not_equal (writer, new_type, id, zero_id);

            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              value = gsk_sl_value_new (new_type);
              zero_id = gsk_spv_writer_get_id_for_value (writer, value);
              gsk_sl_value_free (value);
              return gsk_spv_writer_f_ord_not_equal (writer, new_type, id, zero_id);

            case GSK_SL_BOOL:
            case GSK_SL_VOID:
            default:
              g_assert_not_reached ();
              return id;
            }
          g_assert_not_reached ();

        }
    }
  else if (gsk_sl_type_is_matrix (type))
    {
      GskSlType *row_type, *new_row_type;
      guint i, n = gsk_sl_type_get_length (type);
      guint32 ids[n];

      row_type = gsk_sl_type_get_index_type (type);
      new_row_type = gsk_sl_type_get_index_type (new_type);
      for (i = 0; i < n; i++)
        {
          ids[i] = gsk_spv_writer_composite_extract (writer, row_type, id, (guint32[1]) { i }, 1);
          ids[i] = gsk_spv_writer_convert (writer, ids[i], row_type, new_row_type);
        }

      return gsk_spv_writer_composite_construct (writer, new_type, ids, n);
    }
  else
    {
      g_return_val_if_reached (id);
    }
}
