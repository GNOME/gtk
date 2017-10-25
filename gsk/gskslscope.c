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

#include "gskslscopeprivate.h"

#include "gskslfunctionprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslqualifierprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvariableprivate.h"

#include <string.h>

struct _GskSlScope
{
  int ref_count;

  GskSlScope *parent;
  GSList *children;
  GskSlType *return_type;
  
  GHashTable *variables;
  GHashTable *functions;
  GHashTable *types;

  guint can_break :1;
  guint can_continue :1;
};

static void
free_function_list (gpointer data)
{
  g_list_free_full (data, (GDestroyNotify) gsk_sl_function_unref);
}

GskSlScope *
gsk_sl_scope_new (GskSlScope *parent,
                  GskSlType  *return_type)
{
  if (parent)
    return gsk_sl_scope_new_full (parent, return_type, parent->can_break, parent->can_continue);
  else
    return gsk_sl_scope_new_full (parent, return_type, FALSE, FALSE);
}

GskSlScope *
gsk_sl_scope_new_full (GskSlScope *parent,
                       GskSlType  *return_type,
                       gboolean    can_break,
                       gboolean    can_continue)
{
  GskSlScope *scope;
  
  scope = g_slice_new0 (GskSlScope);
  scope->ref_count = 1;

  if (parent)
    {
      scope->parent = parent;
      parent->children = g_slist_prepend (parent->children, scope);
    }
  if (return_type)
    scope->return_type = gsk_sl_type_ref (return_type);
  scope->can_break = can_break;
  scope->can_continue = can_continue;
  scope->variables = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) gsk_sl_variable_unref);
  scope->functions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) free_function_list);
  scope->types = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) gsk_sl_type_unref);

  return scope;
}

GskSlScope *
gsk_sl_scope_ref (GskSlScope *scope)
{
  g_return_val_if_fail (scope != NULL, NULL);

  scope->ref_count += 1;

  return scope;
}

void
gsk_sl_scope_unref (GskSlScope *scope)
{
  GSList *l;

  if (scope == NULL)
    return;

  scope->ref_count -= 1;
  if (scope->ref_count > 0)
    return;

  g_hash_table_unref (scope->variables);
  g_hash_table_unref (scope->functions);
  g_hash_table_unref (scope->types);

  if (scope->parent)
    scope->parent->children = g_slist_remove (scope->parent->children, scope);
  for (l = scope->children; l; l = l->next)
    ((GskSlScope *) l->data)->parent = NULL;
  g_slist_free (scope->children);
  if (scope->return_type)
    gsk_sl_type_unref (scope->return_type);

  g_slice_free (GskSlScope, scope);
}

GskSlType *
gsk_sl_scope_get_return_type (const GskSlScope *scope)
{
  return scope->return_type;
}

gboolean
gsk_sl_scope_can_break (const GskSlScope *scope)
{
  return scope->can_break;
}

gboolean
gsk_sl_scope_can_continue (const GskSlScope *scope)
{
  return scope->can_continue;
}

gboolean
gsk_sl_scope_is_global (const GskSlScope *scope)
{
  return scope->parent == NULL;
}

void
gsk_sl_scope_add_variable (GskSlScope    *scope,
                           GskSlVariable *variable)
{
  g_hash_table_replace (scope->variables, (gpointer) gsk_sl_variable_get_name (variable), gsk_sl_variable_ref (variable));
}

static const char *
gsk_sl_scope_describe_variable (GskSlVariable *variable)
{
  switch (gsk_sl_qualifier_get_location (gsk_sl_variable_get_qualifier (variable)))
  {
    default:
      g_assert_not_reached ();
    case GSK_SL_QUALIFIER_GLOBAL:
      return "global variable";
    case GSK_SL_QUALIFIER_PARAMETER:
      return "function parameter";
    case GSK_SL_QUALIFIER_LOCAL:
      return "local variable";
    }
}

void
gsk_sl_scope_try_add_variable (GskSlScope        *scope,
                               GskSlPreprocessor *preproc,
                               GskSlVariable     *variable)
{
  GskSlVariable *shadow;

  /* only look in current scope for error */
  shadow = g_hash_table_lookup (scope->variables, gsk_sl_variable_get_name (variable));
  if (shadow)
    {
      gsk_sl_preprocessor_error (preproc, DECLARATION,
                                 "Redefinition of %s \"%s\".",
                                 gsk_sl_scope_describe_variable (shadow),
                                 gsk_sl_variable_get_name (variable));
      return;
    }

  if (scope->parent)
    {
      shadow = gsk_sl_scope_lookup_variable (scope->parent, gsk_sl_variable_get_name (variable));
      if (shadow)
        {
          gsk_sl_preprocessor_warn (preproc, SHADOW,
                                    "Name \"%s\" shadows %s of same name.",
                                    gsk_sl_variable_get_name (variable),
                                    gsk_sl_scope_describe_variable (shadow));
        }
    }

  gsk_sl_scope_add_variable (scope, variable);
}

GskSlVariable *
gsk_sl_scope_lookup_variable (GskSlScope *scope,
                              const char *name)
{
  GskSlVariable *result;

  for (;
       scope != NULL;
       scope = scope->parent)
    {
      result = g_hash_table_lookup (scope->variables, name);
      if (result)
        return result;
    }

  return NULL;
}

void
gsk_sl_scope_add_function (GskSlScope    *scope,
                           GskSlFunction *function)
{
  GList *functions;
  const char *name;

  name = gsk_sl_function_get_name (function);

  functions = g_hash_table_lookup (scope->functions, name);
  gsk_sl_function_ref (function);
  if (functions)
    functions = g_list_append (functions, function);
  else
    g_hash_table_insert (scope->functions, (gpointer) name, g_list_prepend (NULL, function));
}

void
gsk_sl_scope_match_function (GskSlScope           *scope,
                             GskSlFunctionMatcher *matcher,
                             const char           *name)
{
  GList *result = NULL, *lookup;

  for (;
       scope != NULL;
       scope = scope->parent)
    {
      lookup = g_hash_table_lookup (scope->functions, name);
      result = g_list_concat (result, g_list_copy (lookup));
    }

  gsk_sl_function_matcher_init (matcher, result);
}

void
gsk_sl_scope_add_type (GskSlScope *scope,
                       GskSlType  *type)
{
  g_hash_table_replace (scope->types, (gpointer) gsk_sl_type_get_name (type), gsk_sl_type_ref (type));
}

GskSlType *
gsk_sl_scope_lookup_type (GskSlScope *scope,
                          const char *name)
{
  GskSlType *result;

  for (;
       scope != NULL;
       scope = scope->parent)
    {
      result = g_hash_table_lookup (scope->types, name);
      if (result)
        return result;
    }

  return NULL;
}
