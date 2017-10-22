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

static void
gsk_sl_native_variable_add (GskSlScope      *scope,
                            const char      *name,
                            GskSlScalarType  scalar,
                            GskSpvBuiltIn    builtin)
{
  GskSlQualifier qualifier;
  GskSlVariable *variable;

  gsk_sl_qualifier_init (&qualifier);
  qualifier.storage = GSK_SL_STORAGE_GLOBAL_IN;

  variable = gsk_sl_variable_new_builtin (name,
                                          gsk_sl_type_get_scalar (scalar),
                                          &qualifier,
                                          builtin);
  gsk_sl_scope_add_variable (scope, variable);
  gsk_sl_variable_unref (variable);
}

void
gsk_sl_native_variables_add (GskSlScope       *scope,
                             GskSlEnvironment *environment)
{
  if (gsk_sl_environment_get_stage (environment) == GSK_SL_SHADER_VERTEX)
    {
      gsk_sl_native_variable_add (scope, "gl_VertexIndex", GSK_SL_INT, GSK_SPV_BUILT_IN_VERTEX_INDEX);
      gsk_sl_native_variable_add (scope, "gl_InstanceIndex", GSK_SL_INT, GSK_SPV_BUILT_IN_INSTANCE_INDEX);
    }
}

