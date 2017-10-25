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

#include "gskslnativevariableprivate.h"

#include "gskslenvironmentprivate.h"
#include "gskslqualifierprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvariableprivate.h"

typedef struct _NativeVariable NativeVariable;
struct _NativeVariable {
  const char *name;
  GskSlType *type;
  GskSpvBuiltIn builtin;
};

static void
gsk_sl_native_variable_add_block (GskSlScope           *scope,
                                  const char           *block_name,
                                  const char           *block_instance_name,
                                  GskSlStorage          storage,
                                  const NativeVariable *variables,
                                  gsize                 n_variables)
{
  GskSlQualifier qualifier;
  GskSlVariable *variable;
  GskSlTypeBuilder *builder;
  GskSlType *type;
  gsize i;

  gsk_sl_qualifier_init (&qualifier);
  qualifier.storage = storage;

  builder = gsk_sl_type_builder_new_block (block_name);
  for (i = 0; i < n_variables; i++)
    {
      gsk_sl_type_builder_add_builtin_member (builder,
                                              variables[i].type,
                                              variables[i].name,
                                              variables[i].builtin);
    }
  type = gsk_sl_type_builder_free (builder);

  variable = gsk_sl_variable_new (block_instance_name,
                                  type,
                                  &qualifier,
                                  NULL);
  if (block_instance_name)
    {
      gsk_sl_scope_add_variable (scope, variable);
    }
  else
    {
      for (i = 0; i < n_variables; i++)
        {
          GskSlVariable *sub;

          sub = gsk_sl_variable_new_block_member (variable, i);
          gsk_sl_scope_add_variable (scope, sub);
          gsk_sl_variable_unref (sub);
        }
    }
  gsk_sl_variable_unref (variable);
}

static void
gsk_sl_native_variable_add (GskSlScope      *scope,
                            const char      *name,
                            GskSlStorage     storage,
                            GskSlType       *type,
                            GskSpvBuiltIn    builtin)
{
  GskSlQualifier qualifier;
  GskSlVariable *variable;

  gsk_sl_qualifier_init (&qualifier);
  qualifier.storage = storage;

  variable = gsk_sl_variable_new_builtin (name,
                                          type,
                                          &qualifier,
                                          builtin);
  gsk_sl_scope_add_variable (scope, variable);
  gsk_sl_variable_unref (variable);
}

static void
gsk_sl_native_variable_add_simple (GskSlScope      *scope,
                                   const char      *name,
                                   GskSlScalarType  scalar,
                                   GskSpvBuiltIn    builtin)
{
  gsk_sl_native_variable_add (scope, name, GSK_SL_STORAGE_GLOBAL_IN, gsk_sl_type_get_scalar (scalar), builtin);
}

void
gsk_sl_native_variables_add (GskSlScope       *scope,
                             GskSlEnvironment *environment)
{
  if (gsk_sl_environment_get_stage (environment) == GSK_SL_SHADER_VERTEX)
    {
      gsk_sl_native_variable_add_simple (scope, "gl_VertexIndex", GSK_SL_INT, GSK_SPV_BUILT_IN_VERTEX_INDEX);
      gsk_sl_native_variable_add_simple (scope, "gl_InstanceIndex", GSK_SL_INT, GSK_SPV_BUILT_IN_INSTANCE_INDEX);
      if (gsk_sl_environment_get_version (environment) >= 150)
        {
          gsk_sl_native_variable_add_block (scope,
                                            "gl_PerVertex", NULL,
                                            GSK_SL_STORAGE_GLOBAL_OUT,
                                            (NativeVariable[2]) {
                                                { "gl_Position", gsk_sl_type_get_vector (GSK_SL_FLOAT, 4), GSK_SPV_BUILT_IN_POSITION },
                                                { "gl_PointSize", gsk_sl_type_get_scalar (GSK_SL_FLOAT), GSK_SPV_BUILT_IN_POINT_SIZE }
                                            }, 2);
        }
      else
        {
          gsk_sl_native_variable_add (scope, "gl_Position", GSK_SL_STORAGE_GLOBAL_OUT, gsk_sl_type_get_vector (GSK_SL_FLOAT, 4), GSK_SPV_BUILT_IN_POSITION);
          gsk_sl_native_variable_add (scope, "gl_PointSize", GSK_SL_STORAGE_GLOBAL_OUT, gsk_sl_type_get_scalar (GSK_SL_FLOAT), GSK_SPV_BUILT_IN_POINT_SIZE);
        }
    }
}

