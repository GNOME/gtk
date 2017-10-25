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

#include "gskslvariableprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslprinterprivate.h"
#include "gskslqualifierprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

typedef struct _GskSlVariableClass GskSlVariableClass;

struct _GskSlVariable {
  const GskSlVariableClass *class;

  guint ref_count;

  char *name;
  GskSlType *type;
  GskSlQualifier qualifier;
};

struct _GskSlVariableClass
{
  gsize                 size;

  void                  (* free)                                (GskSlVariable          *variable);

  GskSlValue *          (* get_initial_value)                   (const GskSlVariable    *variable);
  GskSpvAccessChain *   (* get_access_chain)                    (GskSlVariable          *variable,
                                                                 GskSpvWriter           *writer);
  guint32               (* write_spv)                           (const GskSlVariable    *variable,
                                                                 GskSpvWriter           *writer);
  guint32               (* load_spv)                            (GskSlVariable          *variable,
                                                                 GskSpvWriter           *writer);
};

static gpointer
gsk_sl_variable_alloc (const GskSlVariableClass *klass,
                       const char               *name,
                       const GskSlQualifier     *qualifier,
                       GskSlType                *type)
{
  GskSlVariable *variable;

  variable = g_slice_alloc0 (klass->size);

  variable->class = klass;
  variable->ref_count = 1;

  variable->name = g_strdup (name);
  variable->qualifier = *qualifier;
  variable->type = gsk_sl_type_ref (type);

  return variable;
}

static void
gsk_sl_variable_free (GskSlVariable *variable)
{
  gsk_sl_type_unref (variable->type);
  g_free (variable->name);

  g_slice_free1 (variable->class->size, variable);
}

static GskSlValue *
gsk_sl_variable_default_get_initial_value (const GskSlVariable *variable)
{
  return NULL;
}

static GskSpvAccessChain *
gsk_spv_access_chain_new (GskSlVariable *variable,
                          GskSpvWriter  *writer);

static GskSpvAccessChain *
gsk_sl_variable_default_get_access_chain (GskSlVariable *variable,
                                          GskSpvWriter  *writer)
{
  return gsk_spv_access_chain_new (variable, writer);
}

static GskSpvAccessChain *
gsk_sl_variable_default_get_no_access_chain (GskSlVariable *variable,
                                             GskSpvWriter  *writer)
{
  return NULL;
}

static guint32
gsk_sl_variable_default_load_spv (GskSlVariable *variable,
                                  GskSpvWriter  *writer)
{
  return gsk_spv_writer_load (writer,
                              variable->type,
                              gsk_spv_writer_get_id_for_variable (writer, variable),
                              0);
}

/* STANDARD */

typedef struct _GskSlVariableStandard GskSlVariableStandard;

struct _GskSlVariableStandard {
  GskSlVariable parent;

  GskSlValue *initial_value;
};

static void
gsk_sl_variable_standard_free (GskSlVariable *variable)
{
  const GskSlVariableStandard *standard = (const GskSlVariableStandard *) variable;

  if (standard->initial_value)
    gsk_sl_value_free (standard->initial_value);

  gsk_sl_variable_free (variable);
}

static GskSlValue *
gsk_sl_variable_standard_get_initial_value (const GskSlVariable *variable)
{
  const GskSlVariableStandard *standard = (const GskSlVariableStandard *) variable;

  return standard->initial_value;
}

static guint32
gsk_sl_variable_standard_write_spv (const GskSlVariable *variable,
                                    GskSpvWriter        *writer)
{
  const GskSlVariableStandard *standard = (const GskSlVariableStandard *) variable;
  guint32 result_id;
  guint32 value_id;
  GskSlQualifierLocation location;
  GskSpvStorageClass storage_class;

  location = gsk_sl_qualifier_get_location (&variable->qualifier);

  if (standard->initial_value)
    value_id = gsk_spv_writer_get_id_for_value (writer, standard->initial_value);
  else
    value_id = 0;

  storage_class = gsk_sl_qualifier_get_storage_class (&variable->qualifier, variable->type);
  result_id = gsk_spv_writer_variable (writer,
                                       location == GSK_SL_QUALIFIER_GLOBAL ? GSK_SPV_WRITER_SECTION_DEFINE : GSK_SPV_WRITER_SECTION_DECLARE,
                                       variable->type,
                                       storage_class,
                                       storage_class,
                                       value_id);

  if (variable->name)
    gsk_spv_writer_name (writer, result_id, variable->name);

  gsk_sl_qualifier_write_spv_decorations (&variable->qualifier, writer, result_id);

  return result_id;
}

static const GskSlVariableClass GSK_SL_VARIABLE_STANDARD = {
  sizeof (GskSlVariableStandard),
  gsk_sl_variable_standard_free,
  gsk_sl_variable_standard_get_initial_value,
  gsk_sl_variable_default_get_access_chain,
  gsk_sl_variable_standard_write_spv,
  gsk_sl_variable_default_load_spv
};

/* BUILTIN */

typedef struct _GskSlVariableBuiltin GskSlVariableBuiltin;

struct _GskSlVariableBuiltin {
  GskSlVariable parent;

  GskSpvBuiltIn builtin;
};

static guint32
gsk_sl_variable_builtin_write_spv (const GskSlVariable *variable,
                                   GskSpvWriter        *writer)
{
  const GskSlVariableBuiltin *builtin = (const GskSlVariableBuiltin *) variable;
  guint32 result_id;
  GskSpvStorageClass storage_class;

  storage_class = gsk_sl_qualifier_get_storage_class (&variable->qualifier, variable->type);
  result_id = gsk_spv_writer_variable (writer,
                                       GSK_SPV_WRITER_SECTION_DEFINE,
                                       variable->type,
                                       storage_class,
                                       storage_class,
                                       0);

  if (variable->name)
    gsk_spv_writer_name (writer, result_id, variable->name);

  gsk_sl_qualifier_write_spv_decorations (&variable->qualifier, writer, result_id);

  gsk_spv_writer_decorate (writer, result_id, GSK_SPV_DECORATION_BUILT_IN, (guint32[1]) { builtin->builtin }, 1);

  return result_id;
}

static const GskSlVariableClass GSK_SL_VARIABLE_BUILTIN = {
  sizeof (GskSlVariableBuiltin),
  gsk_sl_variable_free,
  gsk_sl_variable_default_get_initial_value,
  gsk_sl_variable_default_get_access_chain,
  gsk_sl_variable_builtin_write_spv,
  gsk_sl_variable_default_load_spv
};

/* CONSTANT */

typedef struct _GskSlVariableConstant GskSlVariableConstant;

struct _GskSlVariableConstant {
  GskSlVariable parent;

  GskSlValue *value;
};

static void
gsk_sl_variable_constant_free (GskSlVariable *variable)
{
  const GskSlVariableConstant *constant = (const GskSlVariableConstant *) variable;

  gsk_sl_value_free (constant->value);

  gsk_sl_variable_free (variable);
}

static GskSlValue *
gsk_sl_variable_constant_get_initial_value (const GskSlVariable *variable)
{
  const GskSlVariableConstant *constant = (const GskSlVariableConstant *) variable;

  return constant->value;
}

static guint32
gsk_sl_variable_constant_write_spv (const GskSlVariable *variable,
                                    GskSpvWriter        *writer)
{
  return 0;
}

static guint32
gsk_sl_variable_constant_load_spv (GskSlVariable *variable,
                                   GskSpvWriter  *writer)
{
  const GskSlVariableConstant *constant = (const GskSlVariableConstant *) variable;

  return gsk_spv_writer_get_id_for_value (writer, constant->value);
}

static const GskSlVariableClass GSK_SL_VARIABLE_CONSTANT = {
  sizeof (GskSlVariableConstant),
  gsk_sl_variable_constant_free,
  gsk_sl_variable_constant_get_initial_value,
  gsk_sl_variable_default_get_no_access_chain,
  gsk_sl_variable_constant_write_spv,
  gsk_sl_variable_constant_load_spv
};

/* PARAMETER */

static guint32
gsk_sl_variable_parameter_write_spv (const GskSlVariable *variable,
                                     GskSpvWriter        *writer)
{
  guint32 type_id, result_id;
  
  type_id = gsk_spv_writer_get_id_for_pointer_type (writer,
                                                    variable->type,
                                                    GSK_SPV_STORAGE_CLASS_FUNCTION);
  result_id = gsk_spv_writer_function_parameter (writer, type_id);

  gsk_spv_writer_name (writer, result_id, variable->name);

  return result_id;
}

static const GskSlVariableClass GSK_SL_VARIABLE_PARAMETER = {
  sizeof (GskSlVariable),
  gsk_sl_variable_free,
  gsk_sl_variable_default_get_initial_value,
  gsk_sl_variable_default_get_access_chain,
  gsk_sl_variable_parameter_write_spv,
  gsk_sl_variable_default_load_spv
};

/* CONST_PARAMETER */

static guint32
gsk_sl_variable_const_parameter_write_spv (const GskSlVariable *variable,
                                           GskSpvWriter        *writer)
{
  guint32 result_id, type_id;
  
  type_id = gsk_spv_writer_get_id_for_type (writer, variable->type);
  result_id = gsk_spv_writer_function_parameter (writer, type_id);

  gsk_spv_writer_name (writer, result_id, variable->name);

  return result_id;
}

static guint32
gsk_sl_variable_const_parameter_load_spv (GskSlVariable *variable,
                                          GskSpvWriter  *writer)
{
  return gsk_spv_writer_get_id_for_variable (writer, variable);
}

static const GskSlVariableClass GSK_SL_VARIABLE_CONST_PARAMETER = {
  sizeof (GskSlVariable),
  gsk_sl_variable_free,
  gsk_sl_variable_default_get_initial_value,
  gsk_sl_variable_default_get_no_access_chain,
  gsk_sl_variable_const_parameter_write_spv,
  gsk_sl_variable_const_parameter_load_spv
};

/* MEMBER */

typedef struct _GskSlVariableMember GskSlVariableMember;

struct _GskSlVariableMember {
  GskSlVariable parent;

  GskSlVariable *block;
  gsize member_id;
};

static void
gsk_sl_variable_member_free (GskSlVariable *variable)
{
  const GskSlVariableMember *member = (const GskSlVariableMember *) variable;

  gsk_sl_variable_unref (member->block);

  gsk_sl_variable_free (variable);
}

static GskSpvAccessChain *
gsk_sl_variable_member_get_access_chain (GskSlVariable *variable,
                                         GskSpvWriter  *writer)
{
  const GskSlVariableMember *member = (const GskSlVariableMember *) variable;
  GskSpvAccessChain *chain;
  GskSlValue *value;

  chain = gsk_sl_variable_get_access_chain (member->block, writer);
  value = gsk_sl_value_new_for_data (gsk_sl_type_get_scalar (GSK_SL_INT), &(gint32) { member->member_id }, NULL, NULL);
  gsk_spv_access_chain_add_index (chain,
                                  variable->type,
                                  gsk_spv_writer_get_id_for_value (writer, value));
  gsk_sl_value_free (value);

  return chain;
}

static guint32
gsk_sl_variable_member_write_spv (const GskSlVariable *variable,
                                  GskSpvWriter        *writer)
{
  const GskSlVariableMember *member = (const GskSlVariableMember *) variable;

  return gsk_spv_writer_get_id_for_variable (writer, member->block);
}

static const GskSlVariableClass GSK_SL_VARIABLE_MEMBER = {
  sizeof (GskSlVariableMember),
  gsk_sl_variable_member_free,
  gsk_sl_variable_default_get_initial_value,
  gsk_sl_variable_member_get_access_chain,
  gsk_sl_variable_member_write_spv,
  gsk_sl_variable_default_load_spv
};

/* API */

GskSlVariable *
gsk_sl_variable_new (const char           *name,
                     GskSlType            *type,
                     const GskSlQualifier *qualifier,
                     GskSlValue           *initial_value)
{
  g_return_val_if_fail (initial_value == NULL || gsk_sl_type_equal (type, gsk_sl_value_get_type (initial_value)), NULL);

  switch (qualifier->storage)
  {
    case GSK_SL_STORAGE_GLOBAL_CONST:
      g_assert (initial_value != NULL);
    case GSK_SL_STORAGE_LOCAL_CONST:
      if (initial_value)
        {
          GskSlVariableConstant *constant = gsk_sl_variable_alloc (&GSK_SL_VARIABLE_CONSTANT, name, qualifier, type);
          constant->value = initial_value;
          return &constant->parent;
        }
      /* else fall through */
    case GSK_SL_STORAGE_GLOBAL:
    case GSK_SL_STORAGE_GLOBAL_IN:
    case GSK_SL_STORAGE_GLOBAL_OUT:
    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
    case GSK_SL_STORAGE_LOCAL:
        {
          GskSlVariableStandard *standard = gsk_sl_variable_alloc (&GSK_SL_VARIABLE_STANDARD, name, qualifier, type);
          standard->initial_value = initial_value;
          return &standard->parent;
        }
    case GSK_SL_STORAGE_PARAMETER_IN:
    case GSK_SL_STORAGE_PARAMETER_OUT:
    case GSK_SL_STORAGE_PARAMETER_INOUT:
      g_assert (initial_value == NULL);
      return gsk_sl_variable_alloc (&GSK_SL_VARIABLE_PARAMETER, name, qualifier, type);

    case GSK_SL_STORAGE_PARAMETER_CONST:
      g_assert (initial_value == NULL);
      return gsk_sl_variable_alloc (&GSK_SL_VARIABLE_CONST_PARAMETER, name, qualifier, type);

    case GSK_SL_STORAGE_DEFAULT:
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

GskSlVariable *
gsk_sl_variable_new_block_member (GskSlVariable *block,
                                  guint          member_id)
{
  GskSlVariableMember *member;

  g_return_val_if_fail (block != NULL, NULL);
  g_return_val_if_fail (gsk_sl_type_is_block (block->type), NULL);
  g_return_val_if_fail (member_id < gsk_sl_type_get_n_members (block->type), NULL);

  member = gsk_sl_variable_alloc (&GSK_SL_VARIABLE_MEMBER,
                                  gsk_sl_type_get_member_name (block->type, member_id),
                                  &block->qualifier,
                                  gsk_sl_type_get_member_type (block->type, member_id));
  member->block = gsk_sl_variable_ref (block);
  member->member_id = member_id;

  return &member->parent;
}

GskSlVariable *
gsk_sl_variable_new_builtin (const char           *name,
                             GskSlType            *type,
                             const GskSlQualifier *qualifier,
                             GskSpvBuiltIn         builtin)
{
  GskSlVariableBuiltin *builtin_var;

  builtin_var = gsk_sl_variable_alloc (&GSK_SL_VARIABLE_BUILTIN, name, qualifier, type);

  builtin_var->builtin = builtin;

  return &builtin_var->parent;
}

GskSlVariable *
gsk_sl_variable_ref (GskSlVariable *variable)
{
  g_return_val_if_fail (variable != NULL, NULL);

  variable->ref_count += 1;

  return variable;
}

void
gsk_sl_variable_unref (GskSlVariable *variable)
{
  if (variable == NULL)
    return;

  variable->ref_count -= 1;
  if (variable->ref_count > 0)
    return;

  variable->class->free (variable);
}

void
gsk_sl_variable_print (const GskSlVariable *variable,
                       GskSlPrinter        *printer)
{
  if (gsk_sl_qualifier_print (&variable->qualifier, printer))
    gsk_sl_printer_append (printer, " ");
  gsk_sl_printer_append (printer, gsk_sl_type_get_name (variable->type));
  if (gsk_sl_type_is_block (variable->type))
    {
      guint i, n;

      gsk_sl_printer_append (printer, " {");
      gsk_sl_printer_push_indentation (printer);
      n = gsk_sl_type_get_n_members (variable->type);
      for (i = 0; i < n; i++)
        {
          gsk_sl_printer_newline (printer);
          gsk_sl_printer_append (printer, gsk_sl_type_get_name (gsk_sl_type_get_member_type (variable->type, i)));
          gsk_sl_printer_append (printer, " ");
          gsk_sl_printer_append (printer, gsk_sl_type_get_member_name (variable->type, i));
          gsk_sl_printer_append (printer, ";");
        }
      gsk_sl_printer_pop_indentation (printer);
      gsk_sl_printer_newline (printer);
      gsk_sl_printer_append (printer, "}");
    }
  if (variable->name)
    {
      gsk_sl_printer_append (printer, " ");
      gsk_sl_printer_append (printer, variable->name);
    }
}

GskSlType *
gsk_sl_variable_get_type (const GskSlVariable *variable)
{
  return variable->type;
}

const GskSlQualifier *
gsk_sl_variable_get_qualifier (const GskSlVariable *variable)
{
  return &variable->qualifier;
}

const char *
gsk_sl_variable_get_name (const GskSlVariable *variable)
{
  return variable->name;
}

const GskSlValue *
gsk_sl_variable_get_initial_value (const GskSlVariable *variable)
{
  return variable->class->get_initial_value (variable);
}

gboolean
gsk_sl_variable_is_constant (const GskSlVariable *variable)
{
  return gsk_sl_qualifier_is_constant (&variable->qualifier);
}

GskSpvAccessChain *
gsk_sl_variable_get_access_chain (GskSlVariable *variable,
                                  GskSpvWriter  *writer)
{
  return variable->class->get_access_chain (variable, writer);
}

guint32
gsk_sl_variable_write_spv (const GskSlVariable *variable,
                           GskSpvWriter        *writer)
{
  return variable->class->write_spv (variable, writer);
}

guint32
gsk_sl_variable_load_spv (GskSlVariable *variable,
                          GskSpvWriter  *writer)
{
  return variable->class->load_spv (variable, writer);
}

/* ACCESS CHAIN */

struct _GskSpvAccessChain
{
  GskSpvWriter *writer;
  GskSlVariable *variable;
  GskSlType *type;
  GArray *chain;
  GSList *pending_indexes;
  guint32 swizzle[4];
  guint swizzle_length;
};

static GskSpvAccessChain *
gsk_spv_access_chain_new (GskSlVariable *variable,
                          GskSpvWriter  *writer)
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
  g_slist_free_full (chain->pending_indexes, (GDestroyNotify) gsk_sl_expression_unref);
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
  g_assert (!gsk_spv_access_chain_has_swizzle (chain));

  if (chain->chain == NULL)
    chain->chain = g_array_new (FALSE, FALSE, sizeof (guint32));
  gsk_sl_type_unref (chain->type);
  chain->type = gsk_sl_type_ref (type);

  g_array_append_val (chain->chain, index_id);
}

void
gsk_spv_access_chain_add_dynamic_index (GskSpvAccessChain *chain,
                                        GskSlType         *type,
                                        GskSlExpression   *expr)
{
  gsk_spv_access_chain_add_index (chain, type, 0);

  chain->pending_indexes = g_slist_prepend (chain->pending_indexes, gsk_sl_expression_ref (expr));
}

gboolean
gsk_spv_access_chain_has_swizzle (GskSpvAccessChain *chain)
{
  return chain->swizzle_length > 0;
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

static void
gsk_spv_access_resolve_pending (GskSpvAccessChain *chain)
{
  GSList *l;
  guint i;

  if (chain->pending_indexes == NULL)
    return;
  
  i = chain->chain->len;
  l = chain->pending_indexes;
  while (i-- > 0)
    {
      if (g_array_index (chain->chain, guint32, i) != 0)
        continue;

      g_array_index (chain->chain, guint32, i) = gsk_sl_expression_write_spv (l->data, chain->writer, NULL);
      l = l->next;
    }

  g_slist_free_full (chain->pending_indexes, (GDestroyNotify) gsk_sl_expression_unref);
  chain->pending_indexes = NULL;
}

static guint32
gsk_spv_access_get_variable (GskSpvAccessChain *chain)
{
  guint32 variable_id;
    
  gsk_spv_access_resolve_pending (chain);

  variable_id = gsk_spv_writer_get_id_for_variable (chain->writer,
                                                    chain->variable);

  if (chain->chain)
    variable_id = gsk_spv_writer_access_chain (chain->writer,
                                               chain->type,
                                               gsk_sl_qualifier_get_storage_class (gsk_sl_variable_get_qualifier (chain->variable),
                                                                                   gsk_sl_variable_get_type (chain->variable)),
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

