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
#include "gskslfunctiontypeprivate.h"
#include "gskslqualifierprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"

typedef struct _GskSpvCodeBlock GskSpvCodeBlock;
typedef struct _GskSpvPointerType GskSpvPointerType;

struct _GskSpvPointerType
{
  GskSlType *type;
  GskSpvStorageClass storage;
};

struct _GskSpvCodeBlock
{
  GArray *code[GSK_SPV_WRITER_N_BLOCK_SECTIONS];

  guint32 label_id;
  guint32 continue_id;
  guint32 break_id;
};

struct _GskSpvWriter
{
  int ref_count;

  guint32 last_id;
  guint extended_instructions_id;
  GArray *code[GSK_SPV_WRITER_N_GLOBAL_SECTIONS];
  GSList *blocks;
  GSList *pending_blocks;

  GHashTable *types;
  GHashTable *pointer_types;
  GHashTable *values;
  GHashTable *variables;
  GHashTable *functions;
  GHashTable *function_types;
};

static GskSpvCodeBlock *
gsk_spv_code_block_new (void)
{
  GskSpvCodeBlock *block;
  guint i;

  block = g_slice_new0 (GskSpvCodeBlock);
  for (i = 0; i < GSK_SPV_WRITER_N_BLOCK_SECTIONS; i++)
    {
      block->code[i] = g_array_new (FALSE, TRUE, sizeof (guint32));
    }

  return block;
}

static void
gsk_spv_code_block_free (GskSpvCodeBlock *block)
{
  guint i;

  for (i = 0; i < GSK_SPV_WRITER_N_BLOCK_SECTIONS; i++)
    g_array_free (block->code[i], TRUE);

  g_slice_free (GskSpvCodeBlock, block);
}

static gboolean
pointer_type_equal (gconstpointer a,
                    gconstpointer b)
{
  const GskSpvPointerType *typea = a;
  const GskSpvPointerType *typeb = b;

  if (!gsk_sl_type_equal (typea->type, typeb->type))
    return FALSE;

  return typea->storage == typeb->storage;
}

static guint
pointer_type_hash (gconstpointer t)
{
  const GskSpvPointerType *type = t;

  return gsk_sl_type_hash (type->type)
       ^ type->storage;
}

static void
pointer_type_free (gpointer data)
{
  GskSpvPointerType *pointer_type = data;

  gsk_sl_type_unref (pointer_type->type);

  g_free (pointer_type);
}

GskSpvWriter *
gsk_spv_writer_new (void)
{
  GskSpvWriter *writer;
  guint i;
  
  writer = g_slice_new0 (GskSpvWriter);
  writer->ref_count = 1;

  for (i = 0; i < GSK_SPV_WRITER_N_GLOBAL_SECTIONS; i++)
    {
      writer->code[i] = g_array_new (FALSE, TRUE, sizeof (guint32));
    }

  writer->types = g_hash_table_new_full (gsk_sl_type_hash, gsk_sl_type_equal,
                                         (GDestroyNotify) gsk_sl_type_unref, NULL);
  writer->pointer_types = g_hash_table_new_full (pointer_type_hash, pointer_type_equal,
                                                 pointer_type_free, NULL);
  writer->values = g_hash_table_new_full (gsk_sl_value_hash, gsk_sl_value_equal,
                                          (GDestroyNotify) gsk_sl_value_free, NULL);
  writer->variables = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             (GDestroyNotify) gsk_sl_variable_unref, NULL);
  writer->functions = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             (GDestroyNotify) gsk_sl_function_unref, NULL);
  writer->function_types = g_hash_table_new_full (gsk_sl_function_type_hash, gsk_sl_function_type_equal,
                                                  (GDestroyNotify) gsk_sl_function_type_unref, NULL);

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

  for (i = 0; i < GSK_SPV_WRITER_N_GLOBAL_SECTIONS; i++)
    {
      g_array_free (writer->code[i], TRUE);
    }

  g_hash_table_destroy (writer->pointer_types);
  g_hash_table_destroy (writer->types);
  g_hash_table_destroy (writer->values);
  g_hash_table_destroy (writer->variables);
  g_hash_table_destroy (writer->functions);
  g_hash_table_destroy (writer->function_types);

  g_slice_free (GskSpvWriter, writer);
}

static guint32
gsk_spv_writer_write_function (GskSpvWriter     *writer,
                               GskSlFunction    *function,
                               GskSpvWriterFunc  initializer,
                               gpointer          initializer_data)
{
  GskSpvCodeBlock *block;
  guint32 result;

  g_assert (g_hash_table_lookup (writer->functions, function) == NULL);

  block = gsk_spv_code_block_new ();
  writer->blocks = g_slist_prepend (writer->blocks, block);
  result = gsk_sl_function_write_spv (function, writer, initializer, initializer_data);
  g_hash_table_insert (writer->functions, gsk_sl_function_ref (function), GUINT_TO_POINTER (result));
  g_assert (writer->blocks->data == block);
  writer->blocks = g_slist_remove (writer->blocks, block);
  writer->pending_blocks = g_slist_prepend (writer->pending_blocks, block);

  return result;
}

static gint
compare_guint32 (gconstpointer a,
                 gconstpointer b)
{
  guint32 ua = *(const guint32 *) a;
  guint32 ub = *(const guint32 *) b;

  return ua - ub;
}

static guint32 *
gsk_spv_writer_collect_entry_point_interfaces (GskSpvWriter *writer,
                                               gsize        *n_interfaces)
{
  GHashTableIter iter;
  GArray *interfaces;
  gpointer variable, id;

  interfaces = g_array_new (FALSE, FALSE, sizeof (guint32));

  g_hash_table_iter_init (&iter, writer->variables);
  while (g_hash_table_iter_next (&iter, &variable, &id))
    {
      switch (gsk_sl_variable_get_qualifier (variable)->storage)
      {
        case GSK_SL_STORAGE_DEFAULT:
        default:
          g_assert_not_reached();
        case GSK_SL_STORAGE_GLOBAL:
        case GSK_SL_STORAGE_GLOBAL_CONST:
        case GSK_SL_STORAGE_LOCAL:
        case GSK_SL_STORAGE_LOCAL_CONST:
        case GSK_SL_STORAGE_PARAMETER_IN:
        case GSK_SL_STORAGE_PARAMETER_OUT:
        case GSK_SL_STORAGE_PARAMETER_INOUT:
        case GSK_SL_STORAGE_PARAMETER_CONST:
        case GSK_SL_STORAGE_GLOBAL_UNIFORM:
          continue;

        case GSK_SL_STORAGE_GLOBAL_IN:
        case GSK_SL_STORAGE_GLOBAL_OUT:
          break;
      }
      
      g_array_append_val (interfaces, (guint32) { GPOINTER_TO_UINT (id) });
    }

  /* Try to be like glslang */
  g_array_sort (interfaces, compare_guint32);

  *n_interfaces = interfaces->len;
  return (guint32 *) g_array_free (interfaces, FALSE);
}

static void
gsk_spv_writer_do_write (GskSpvWriter     *writer,
                         GskSlFunction    *entry_point,
                         GskSpvWriterFunc  initializer,
                         gpointer          initializer_data)
{
  guint32 *interfaces;
  gsize n_interfaces;
  guint32 entry_point_id;

  gsk_spv_writer_capability (writer, GSK_SPV_CAPABILITY_SHADER);
  writer->extended_instructions_id = gsk_spv_writer_ext_inst_import (writer,
                                                                     "GLSL.std.450");
  gsk_spv_writer_source (writer,
                         GSK_SPV_SOURCE_LANGUAGE_GLSL,
                         440,
                         0,
                         NULL);
  gsk_spv_writer_source_extension (writer, "GL_GOOGLE_cpp_style_line_directive");
  gsk_spv_writer_source_extension (writer, "GL_GOOGLE_include_directive");
  gsk_spv_writer_memory_model (writer,
                               GSK_SPV_ADDRESSING_MODEL_LOGICAL,
                               GSK_SPV_MEMORY_MODEL_GLSL450);

  entry_point_id = gsk_spv_writer_write_function (writer, entry_point, initializer, initializer_data);

  interfaces = gsk_spv_writer_collect_entry_point_interfaces (writer,
                                                              &n_interfaces);
  gsk_spv_writer_entry_point (writer,
                              GSK_SPV_EXECUTION_MODEL_FRAGMENT,
                              entry_point_id,
                              "main",
                              interfaces,
                              n_interfaces);
  g_free (interfaces);

  gsk_spv_writer_execution_mode (writer,
                                 entry_point_id,
                                 GSK_SPV_EXECUTION_MODE_ORIGIN_UPPER_LEFT);
}

static void
gsk_spv_writer_clear (GskSpvWriter *writer)
{
  guint i;

  g_slist_free_full (writer->pending_blocks, (GDestroyNotify) gsk_spv_code_block_free);
  writer->pending_blocks = NULL;
  writer->extended_instructions_id = 0;

  for (i = 0; i < GSK_SPV_WRITER_N_GLOBAL_SECTIONS; i++)
    {
      g_array_set_size (writer->code[i], 0);
    }

  g_hash_table_remove_all (writer->pointer_types);
  g_hash_table_remove_all (writer->types);
  g_hash_table_remove_all (writer->values);
  g_hash_table_remove_all (writer->variables);
  g_hash_table_remove_all (writer->functions);
  g_hash_table_remove_all (writer->function_types);
}

GBytes *
gsk_spv_writer_write (GskSpvWriter     *writer,
                      GskSlFunction    *entry_point,
                      GskSpvWriterFunc  initializer,
                      gpointer          initializer_data)
{
  GArray *array;
  gsize size;
  GSList *l;
  guint i;

  gsk_spv_writer_do_write (writer, entry_point, initializer, initializer_data);

  array = g_array_new (FALSE, FALSE, sizeof (guint32));

  g_array_append_val (array, (guint32) { GSK_SPV_MAGIC_NUMBER });
  g_array_append_val (array, (guint32) { (GSK_SPV_VERSION_MAJOR << 16) | (GSK_SPV_VERSION_MINOR << 8) });
  g_array_append_val (array, (guint32) { GSK_SPV_GENERATOR });
  g_array_append_val (array, (guint32) { writer->last_id + 1 });
  g_array_append_val (array, (guint32) { 0 });
  
  for (i = 0; i < GSK_SPV_WRITER_N_GLOBAL_SECTIONS; i++)
    {
      g_array_append_vals (array, writer->code[i]->data, writer->code[i]->len);
    }

  for (l = writer->pending_blocks; l; l = l->next)
    {
      GskSpvCodeBlock *block = l->data;

      for (i = 0; i < GSK_SPV_WRITER_N_BLOCK_SECTIONS; i++)
        {
          g_array_append_vals (array, block->code[i]->data, block->code[i]->len);
        }
    }

  gsk_spv_writer_clear (writer);

  size = array->len * sizeof (guint32);
  return g_bytes_new_take (g_array_free (array, FALSE), size);
}

guint32
gsk_spv_writer_get_id_for_extended_instructions (GskSpvWriter *writer)
{
  return writer->extended_instructions_id;
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
                                        GskSlType          *type,
                                        GskSpvStorageClass  storage)
{
  GskSpvPointerType pointer_type = { type, storage };
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->pointer_types, &pointer_type));
  if (result != 0)
    return result;

  result = gsk_spv_writer_type_pointer (writer, 
                                        storage,
                                        gsk_spv_writer_get_id_for_type (writer, type));
  gsk_sl_type_ref (type);
  g_hash_table_insert (writer->pointer_types, g_memdup (&pointer_type, sizeof (GskSpvPointerType)), GUINT_TO_POINTER (result));
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
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->functions, function));
  if (result != 0)
    return result;
  
  return gsk_spv_writer_write_function (writer, function, NULL, NULL);
}

guint32
gsk_spv_writer_get_id_for_function_type (GskSpvWriter      *writer,
                                         GskSlFunctionType *function_type)
{
  guint32 result;

  result = GPOINTER_TO_UINT (g_hash_table_lookup (writer->function_types, function_type));
  if (result != 0)
    return result;
  
  result = gsk_sl_function_type_write_spv (function_type, writer);
  g_hash_table_insert (writer->function_types, gsk_sl_function_type_ref (function_type), GUINT_TO_POINTER (result));
  return result;
}

guint32
gsk_spv_writer_make_id (GskSpvWriter *writer)
{
  writer->last_id++;

  return writer->last_id;
}

GArray *
gsk_spv_writer_get_bytes (GskSpvWriter        *writer,
                          GskSpvWriterSection  section)
{
  if (section < GSK_SPV_WRITER_SECTION_BLOCK_FIRST)
    return writer->code[section];
  
  return ((GskSpvCodeBlock *) writer->blocks->data)->code[section - GSK_SPV_WRITER_SECTION_BLOCK_FIRST];
}

void
gsk_spv_writer_start_code_block (GskSpvWriter *writer,
                                 guint32       label_id,
                                 guint32       continue_id,
                                 guint32       break_id)
{
  GskSpvCodeBlock *block;

  block = writer->blocks->data;

  block->label_id = label_id;
  if (continue_id != 0)
    block->continue_id = continue_id;
  if (break_id != 0)
    block->break_id = break_id;
}

guint32
gsk_spv_writer_get_label_id (GskSpvWriter *writer)
{
  GskSpvCodeBlock *block = writer->blocks->data;

  return block->label_id;
}

guint32
gsk_spv_writer_get_continue_id (GskSpvWriter *writer)
{
  GskSpvCodeBlock *block = writer->blocks->data;

  return block->continue_id;
}

guint32
gsk_spv_writer_get_break_id (GskSpvWriter *writer)
{
  GskSpvCodeBlock *block = writer->blocks->data;

  return block->break_id;
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

struct _GskSpvAccessChain
{
  GskSpvWriter *writer;
  GskSlVariable *variable;
  GskSlType *type;
  GArray *chain;
  guint32 swizzle[4];
  guint swizzle_length;
};

GskSpvAccessChain *
gsk_spv_access_chain_new (GskSpvWriter  *writer,
                          GskSlVariable *variable)
{
  GskSpvAccessChain *chain;

  chain = g_slice_new0 (GskSpvAccessChain);

  chain->writer = gsk_spv_writer_ref (writer);
  chain->variable = gsk_sl_variable_ref (variable);
  chain->type = gsk_sl_type_ref (gsk_sl_variable_get_type (variable));

  return chain;
}

void
gsk_spv_access_chain_free (GskSpvAccessChain *chain)
{
  if (chain->chain)
    g_array_free (chain->chain, TRUE);
  gsk_sl_type_unref (chain->type);
  gsk_sl_variable_unref (chain->variable);
  gsk_spv_writer_unref (chain->writer);

  g_slice_free (GskSpvAccessChain, chain);
}

void
gsk_spv_access_chain_add_index (GskSpvAccessChain *chain,
                                GskSlType         *type,
                                guint32            index_id)
{
  if (chain->chain == NULL)
    chain->chain = g_array_new (FALSE, FALSE, sizeof (guint32));
  gsk_sl_type_unref (chain->type);
  chain->type = gsk_sl_type_ref (type);

  g_array_append_val (chain->chain, index_id);
}

void
gsk_spv_access_chain_swizzle (GskSpvAccessChain *chain,
                              const guint       *indexes,
                              guint              length)
{
  guint tmp[4];
  guint i;

  if (length == 1)
    {
      GskSlValue *value;
      guint32 new_index;

      if (chain->swizzle_length != 0)
        new_index = chain->swizzle[indexes[0]];
      else
        new_index = indexes[0];
      chain->swizzle_length = 0;

      value = gsk_sl_value_new_for_data (gsk_sl_type_get_scalar (GSK_SL_UINT),
                                         &new_index,
                                         NULL, NULL);
      gsk_spv_access_chain_add_index (chain,
                                      gsk_sl_type_get_index_type (chain->type),
                                      gsk_spv_writer_get_id_for_value (chain->writer, value));
      gsk_sl_value_free (value);
      return;
    }

  if (chain->swizzle_length != 0)
    {
      g_assert (length <= chain->swizzle_length);

      for (i = 0; i < length; i++)
        {
          tmp[i] = chain->swizzle[indexes[i]];
        }
      indexes = tmp;
    }

  /* Mean trick to do optimization: We only assign a swizzle_length
   * If something is actually swizzled. If we're doing an identity
   * swizzle, ignore it.
   */
  if (length < gsk_sl_type_get_n_components (chain->type))
    chain->swizzle_length = length;
  else
    chain->swizzle_length = 0;

  for (i = 0; i < length; i++)
    {
      chain->swizzle[i] = indexes[i];
      if (indexes[i] != i)
        chain->swizzle_length = length;
    }
}

static guint32
gsk_spv_access_get_variable (GskSpvAccessChain *chain)
{
  guint32 variable_id;
    
  variable_id = gsk_spv_writer_get_id_for_variable (chain->writer,
                                                    chain->variable);

  if (chain->chain)
    variable_id = gsk_spv_writer_access_chain (chain->writer,
                                               chain->type,
                                               gsk_sl_qualifier_get_storage_class (gsk_sl_variable_get_qualifier (chain->variable)),
                                               variable_id,
                                               (guint32 *) chain->chain->data,
                                               chain->chain->len);

  return variable_id;
}

static GskSlType *
gsk_spv_access_chain_get_swizzle_type (GskSpvAccessChain *chain)
{
  g_assert (chain->swizzle_length != 0);

  if (chain->swizzle_length == 1)
    return gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (chain->type));
  else
    return gsk_sl_type_get_vector (gsk_sl_type_get_scalar_type (chain->type), chain->swizzle_length);
}

guint32
gsk_spv_access_chain_load (GskSpvAccessChain *chain)
{
  guint result_id;
  
  result_id = gsk_spv_writer_load (chain->writer,
                                   chain->type,
                                   gsk_spv_access_get_variable (chain),
                                   0);

  if (chain->swizzle_length)
    result_id = gsk_spv_writer_vector_shuffle (chain->writer,
                                               gsk_spv_access_chain_get_swizzle_type (chain),
                                               result_id,
                                               result_id,
                                               chain->swizzle,
                                               chain->swizzle_length);

  return result_id;
}

void
gsk_spv_access_chain_store (GskSpvAccessChain *chain,
                            guint32            value)
{
  guint32 chain_id;
  
  chain_id = gsk_spv_access_get_variable (chain);

  if (chain->swizzle_length)
    {
      guint32 indexes[4] = { 0, };
      guint32 merge;
      guint i, n;
      
      merge = gsk_spv_writer_load (chain->writer,
                                   chain->type,
                                   chain_id,
                                   0);

      n = gsk_sl_type_get_n_components (chain->type);
      for (i = 0; i < n; i++)
        {
          if (i < chain->swizzle_length)
            indexes[chain->swizzle[i]] = n + i;
          if (indexes[i] == 0)
            indexes[i] = i;
        }

      value = gsk_spv_writer_vector_shuffle (chain->writer,
                                             chain->type,
                                             merge,
                                             value,
                                             indexes,
                                             n);
    }

  gsk_spv_writer_store (chain->writer,
                        chain_id,
                        value,
                        0);
}

