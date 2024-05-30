/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcsskeyframesprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcsscustompropertypoolprivate.h"
#include "gtkcssreferencevalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkstylepropertyprivate.h"

#include "gtkprivate.h"

#include <stdlib.h>
#include <string.h>

struct _GtkCssKeyframes {
  int ref_count;                 /* ref count */
  int n_keyframes;               /* number of keyframes (at least 2 for 0% and 100% */
  double *keyframe_progress;     /* ordered array of n_keyframes of [0..1] */
  int n_properties;              /* number of properties used by keyframes */
  guint *property_ids;           /* ordered array of n_properties property ids */
  GtkCssValue **values;          /* 2D array: n_keyframes * n_properties of (value or NULL) for all the keyframes */
  GtkCssVariableSet **variables; /* array of variable sets for each keyframe */
  int *variable_ids;             /* ordered array of variable ids */
  int n_variables;               /* number of variable used by keyframes */
};

#define KEYFRAMES_VALUE(keyframes, k, p) ((keyframes)->values[(k) * (keyframes)->n_properties + (p)])

GtkCssKeyframes *
_gtk_css_keyframes_ref (GtkCssKeyframes *keyframes)
{
  g_return_val_if_fail (keyframes != NULL, NULL);

  keyframes->ref_count++;

  return keyframes;
}

void
_gtk_css_keyframes_unref (GtkCssKeyframes *keyframes)
{
  guint k, p;

  g_return_if_fail (keyframes != NULL);

  keyframes->ref_count--;
  if (keyframes->ref_count > 0)
    return;

  g_free (keyframes->keyframe_progress);
  g_free (keyframes->property_ids);

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      for (p = 0; p < keyframes->n_properties; p++)
        {
          gtk_css_value_unref (KEYFRAMES_VALUE (keyframes, k, p));
          KEYFRAMES_VALUE (keyframes, k, p) = NULL;
        }

      if (keyframes->variables && keyframes->variables[k])
        gtk_css_variable_set_unref (keyframes->variables[k]);
    }
  g_free (keyframes->values);
  g_free (keyframes->variables);
  g_free (keyframes->variable_ids);

  g_free (keyframes);
}

static guint
gtk_css_keyframes_add_keyframe (GtkCssKeyframes *keyframes,
                                double           progress)
{
  guint k, p;

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      if (keyframes->keyframe_progress[k] == progress)
        {
          for (p = 0; p < keyframes->n_properties; p++)
            {
              if (KEYFRAMES_VALUE (keyframes, k, p) == NULL)
                continue;

              gtk_css_value_unref (KEYFRAMES_VALUE (keyframes, k, p));
              KEYFRAMES_VALUE (keyframes, k, p) = NULL;

              /* XXX: GC properties that are now unset
               * in all keyframes?
               */
            }
          return k;
        }
      else if (keyframes->keyframe_progress[k] > progress)
        break;
    }

  keyframes->n_keyframes++;
  keyframes->keyframe_progress = g_realloc (keyframes->keyframe_progress, sizeof (double) * keyframes->n_keyframes);
  memmove (keyframes->keyframe_progress + k + 1, keyframes->keyframe_progress + k, sizeof (double) * (keyframes->n_keyframes - k - 1));
  keyframes->keyframe_progress[k] = progress;

  if (keyframes->n_properties)
    {
      gsize size = sizeof (GtkCssValue *) * keyframes->n_properties;
      
      keyframes->values = g_realloc (keyframes->values, sizeof (GtkCssValue *) * keyframes->n_keyframes * keyframes->n_properties);
      memmove (&KEYFRAMES_VALUE (keyframes, k + 1, 0), &KEYFRAMES_VALUE (keyframes, k, 0), size * (keyframes->n_keyframes - k - 1));
      memset (&KEYFRAMES_VALUE (keyframes, k, 0), 0, size);
    }

  if (keyframes->variables)
    keyframes->variables = g_realloc (keyframes->variables, sizeof (GtkCssVariableSet *) * keyframes->n_keyframes);

  return k;
}

static guint
gtk_css_keyframes_lookup_property (GtkCssKeyframes *keyframes,
                                   guint            property_id)
{
  guint p;

  for (p = 0; p < keyframes->n_properties; p++)
    {
      if (keyframes->property_ids[p] == property_id)
        return p;
      else if (keyframes->property_ids[p] > property_id)
        break;
    }

  keyframes->n_properties++;
  keyframes->property_ids = g_realloc (keyframes->property_ids, sizeof (guint) * keyframes->n_properties);
  memmove (keyframes->property_ids + p + 1, keyframes->property_ids + p, sizeof (guint) * (keyframes->n_properties - p - 1));
  keyframes->property_ids[p] = property_id;

  if (keyframes->n_properties > 1)
    {
      guint old_n_properties = keyframes->n_properties - 1;
      int k;
      
      keyframes->values = g_realloc (keyframes->values, sizeof (GtkCssValue *) * keyframes->n_keyframes * keyframes->n_properties);

      if (p + 1 < keyframes->n_properties)
        {
          memmove (&KEYFRAMES_VALUE (keyframes, keyframes->n_keyframes - 1, p + 1),
                   &keyframes->values[(keyframes->n_keyframes - 1) * old_n_properties + p],
                   sizeof (GtkCssValue *) * (keyframes->n_properties - p - 1));
        }
      KEYFRAMES_VALUE (keyframes, keyframes->n_keyframes - 1, p) = NULL;

      for (k = keyframes->n_keyframes - 2; k >= 0; k--)
        {
          memmove (&KEYFRAMES_VALUE (keyframes, k, p + 1),
                   &keyframes->values[k * old_n_properties + p],
                   sizeof (GtkCssValue *) * old_n_properties);
          KEYFRAMES_VALUE (keyframes, k, p) = NULL;
        }
    }
  else
    {
      keyframes->values = g_new0 (GtkCssValue *, keyframes->n_keyframes);
    }

  return p;
}

static void
gtk_css_keyframes_register_variable (GtkCssKeyframes *keyframes,
                                     int              variable_id)
{
  guint p;

  for (p = 0; p < keyframes->n_variables; p++)
    {
      if (keyframes->variable_ids[p] == variable_id)
        return;
      else if (keyframes->variable_ids[p] > variable_id)
        break;
    }

  keyframes->n_variables++;
  keyframes->variable_ids = g_realloc (keyframes->variable_ids, sizeof (int) * keyframes->n_variables);
  memmove (keyframes->variable_ids + p + 1, keyframes->variable_ids + p, sizeof (int) * (keyframes->n_variables - p - 1));
  keyframes->variable_ids[p] = variable_id;
}

static GtkCssKeyframes *
gtk_css_keyframes_alloc (void)
{
  GtkCssKeyframes *keyframes;

  keyframes = g_new0 (GtkCssKeyframes, 1);
  keyframes->ref_count = 1;

  return keyframes;
}

static GtkCssKeyframes *
gtk_css_keyframes_new (void)
{
  GtkCssKeyframes *keyframes;

  keyframes = gtk_css_keyframes_alloc ();

  gtk_css_keyframes_add_keyframe (keyframes, 0);
  gtk_css_keyframes_add_keyframe (keyframes, 1);

  return keyframes;
}

static gboolean
keyframes_set_value (GtkCssKeyframes     *keyframes,
                     guint                k,
                     GtkCssStyleProperty *property,
                     GtkCssValue         *value)
{
  guint p;

  if (!_gtk_css_style_property_is_animated (property))
    return FALSE;

  p = gtk_css_keyframes_lookup_property (keyframes, _gtk_css_style_property_get_id (property));
  
  if (KEYFRAMES_VALUE (keyframes, k, p))
    gtk_css_value_unref (KEYFRAMES_VALUE (keyframes, k, p));

  KEYFRAMES_VALUE (keyframes, k, p) = gtk_css_value_ref (value);

  return TRUE;
}

static gboolean
gtk_css_keyframes_parse_declaration (GtkCssKeyframes *keyframes,
                                     guint            k,
                                     GtkCssParser    *parser)
{
  GtkStyleProperty *property;
  GtkCssValue *value;
  char *name;

  name = gtk_css_parser_consume_ident (parser);
  if (name == NULL)
    {
      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        gtk_css_parser_error_syntax (parser, "Expected a property name");
      return FALSE;
    }

  /* This is a custom property */
  if (name[0] == '-' && name[1] == '-')
    {
      GtkCssVariableValue *var_value;
      GtkCssCustomPropertyPool *pool;
      int id;

      if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (parser, "Expected a ':'");
          g_free (name);
          return FALSE;
        }

      var_value = gtk_css_parser_parse_value_into_token_stream (parser);
      if (var_value == NULL)
        {
          g_free (name);
          return FALSE;
        }

      if (!keyframes->variables)
        keyframes->variables = g_new0 (GtkCssVariableSet *, keyframes->n_keyframes);

      if (!keyframes->variables[k])
        keyframes->variables[k] = gtk_css_variable_set_new ();

      pool = gtk_css_custom_property_pool_get ();
      id = gtk_css_custom_property_pool_add (pool, name);
      gtk_css_keyframes_register_variable (keyframes, id);

      gtk_css_variable_value_taint (var_value);

      gtk_css_variable_set_add (keyframes->variables[k], id, var_value);

      gtk_css_custom_property_pool_unref (pool, id);

      g_free (name);
      return TRUE;
    }

  property = _gtk_style_property_lookup (name);
  if (property == NULL)
    {
      gtk_css_parser_error_value (parser, "No property named '%s'", name);
      g_free (name);
      return FALSE;
    }

  g_free (name);

  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected a ':'");
      return FALSE;
    }

  if (gtk_css_parser_has_references (parser))
    {
      GtkCssVariableValue *var_value;

      var_value = gtk_css_parser_parse_value_into_token_stream (parser);
      if (var_value == NULL)
        return FALSE;

      if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
        {
          GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
          guint i, n;
          GtkCssValue **values;

          n = _gtk_css_shorthand_property_get_n_subproperties (shorthand);

          values = g_new (GtkCssValue *, n);

          for (i = 0; i < n; i++)
            {
              GtkCssValue *child =
                _gtk_css_reference_value_new (property,
                                              var_value,
                                              gtk_css_parser_get_file (parser));
              _gtk_css_reference_value_set_subproperty (child, i);

              values[i] = _gtk_css_array_value_get_nth (child, i);
            }

          value = _gtk_css_array_value_new_from_array (values, n);
          g_free (values);
        }
      else
        {
          value = _gtk_css_reference_value_new (property,
                                                var_value,
                                                gtk_css_parser_get_file (parser));
        }

      gtk_css_variable_value_unref (var_value);
    }
  else
    {
      value = _gtk_style_property_parse_value (property, parser);
      if (value == NULL)
        return FALSE;
    }

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (parser, "Junk at end of value");
      gtk_css_value_unref (value);
      return FALSE;
    }

  if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
    {
      GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
      gboolean animatable = FALSE;
      guint i;

      for (i = 0; i < _gtk_css_shorthand_property_get_n_subproperties (shorthand); i++)
        {
          GtkCssStyleProperty *child = _gtk_css_shorthand_property_get_subproperty (shorthand, i);
          GtkCssValue *sub = _gtk_css_array_value_get_nth (value, i);
          
          animatable |= keyframes_set_value (keyframes, k, child, sub);
        }

      if (!animatable)
        gtk_css_parser_error_value (parser, "shorthand '%s' cannot be animated", _gtk_style_property_get_name (property));
    }
  else if (GTK_IS_CSS_STYLE_PROPERTY (property))
    {
      if (!keyframes_set_value (keyframes, k, GTK_CSS_STYLE_PROPERTY (property), value))
        gtk_css_parser_error_value (parser, "Cannot animate property '%s'", _gtk_style_property_get_name (property));
    }
  else
    {
      g_assert_not_reached ();
    }
      
  gtk_css_value_unref (value);

  return TRUE;
}

static gboolean
gtk_css_keyframes_parse_block (GtkCssKeyframes *keyframes,
                               guint            k,
                               GtkCssParser    *parser)
{
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_parser_error_syntax (parser, "Expected '{'");
      return FALSE;
    }

  gtk_css_parser_start_block (parser);

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_EOF);
      gtk_css_keyframes_parse_declaration (keyframes, k, parser);
      gtk_css_parser_end_block (parser);
    }

  if (keyframes->variables && keyframes->variables[k])
    gtk_css_variable_set_resolve_cycles (keyframes->variables[k]);

  gtk_css_parser_end_block (parser);

  return TRUE;
}

GtkCssKeyframes *
_gtk_css_keyframes_parse (GtkCssParser *parser)
{
  GtkCssKeyframes *keyframes;
  double progress;
  guint k;

  g_return_val_if_fail (parser != NULL, NULL);

  keyframes = gtk_css_keyframes_new ();

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      if (gtk_css_parser_try_ident (parser, "from"))
        progress = 0;
      else if (gtk_css_parser_try_ident (parser, "to"))
        progress = 1;
      else if (gtk_css_parser_consume_percentage (parser, &progress))
        {
          if (progress < 0 || progress > 100)
            {
              /* XXX: should we skip over the block here? */
              gtk_css_parser_error_value (parser, "percentages must be between 0%% and 100%%");
              _gtk_css_keyframes_unref (keyframes);
              return NULL;
            }
          progress /= 100;
        }
      else
        {
          _gtk_css_keyframes_unref (keyframes);
          return NULL;
        }

      k = gtk_css_keyframes_add_keyframe (keyframes, progress);

      if (!gtk_css_keyframes_parse_block (keyframes, k, parser))
        {
          _gtk_css_keyframes_unref (keyframes);
          return NULL;
        }
    }

  return keyframes;
}

static int
compare_property_by_name (gconstpointer a,
                          gconstpointer b,
                          gpointer      data)
{
  GtkCssKeyframes *keyframes = data;

  return strcmp (_gtk_style_property_get_name (GTK_STYLE_PROPERTY (
                    _gtk_css_style_property_lookup_by_id (keyframes->property_ids[*(const guint *) a]))),
                 _gtk_style_property_get_name (GTK_STYLE_PROPERTY (
                    _gtk_css_style_property_lookup_by_id (keyframes->property_ids[*(const guint *) b]))));
}

static int
compare_custom_property_ids (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GtkCssCustomPropertyPool *pool = user_data;
  int id1 = GPOINTER_TO_INT (*((const int *) a));
  int id2 = GPOINTER_TO_INT (*((const int *) b));
  const char *name1, *name2;

  name1 = gtk_css_custom_property_pool_get_name (pool, id1);
  name2 = gtk_css_custom_property_pool_get_name (pool, id2);

  return strcmp (name1, name2);
}

void
_gtk_css_keyframes_print (GtkCssKeyframes *keyframes,
                          GString         *string)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();
  guint k, p;
  guint *sorted, *sorted_variable_ids = NULL;

  g_return_if_fail (keyframes != NULL);
  g_return_if_fail (string != NULL);

  sorted = g_new (guint, keyframes->n_properties);
  for (p = 0; p < keyframes->n_properties; p++)
    sorted[p] = p;
  g_qsort_with_data (sorted, keyframes->n_properties, sizeof (guint), compare_property_by_name, keyframes);

  if (keyframes->variable_ids)
    {
      sorted_variable_ids = g_memdup2 (keyframes->variable_ids,
                                       sizeof (int) * keyframes->n_variables);

      g_qsort_with_data (sorted_variable_ids,
                         keyframes->n_variables,
                         sizeof (int),
                         compare_custom_property_ids,
                         pool);
    }

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      /* useful for 0% and 100% which might be empty */
      gboolean opened = FALSE;

      for (p = 0; p < keyframes->n_properties; p++)
        {
          if (KEYFRAMES_VALUE (keyframes, k, sorted[p]) == NULL)
            continue;

          if (!opened)
            {
              if (keyframes->keyframe_progress[k] == 0.0)
                g_string_append (string, "  from {\n");
              else if (keyframes->keyframe_progress[k] == 1.0)
                g_string_append (string, "  to {\n");
              else
                g_string_append_printf (string, "  %g%% {\n", keyframes->keyframe_progress[k] * 100);
              opened = TRUE;
            }
          
          g_string_append_printf (string, "    %s: ", _gtk_style_property_get_name (
                                                        GTK_STYLE_PROPERTY (
                                                          _gtk_css_style_property_lookup_by_id (
                                                            keyframes->property_ids[sorted[p]]))));
          gtk_css_value_print (KEYFRAMES_VALUE (keyframes, k, sorted[p]), string);
          g_string_append (string, ";\n");
        }

      if (keyframes->variables && keyframes->variables[k])
        {
          for (p = 0; p < keyframes->n_variables; p++)
            {
              int variable_id = sorted_variable_ids[p];
              GtkCssVariableValue *value =
                gtk_css_variable_set_lookup (keyframes->variables[k], variable_id, NULL);
              const char *name;

              if (value == NULL)
                continue;

              if (!opened)
                {
                  if (keyframes->keyframe_progress[k] == 0.0)
                    g_string_append (string, "  from {\n");
                  else if (keyframes->keyframe_progress[k] == 1.0)
                    g_string_append (string, "  to {\n");
                  else
                    g_string_append_printf (string, "  %g%% {\n", keyframes->keyframe_progress[k] * 100);
                  opened = TRUE;
                }

              name = gtk_css_custom_property_pool_get_name (pool, variable_id);

              g_string_append_printf (string, "    %s: ", name);
              gtk_css_variable_value_print (value, string);
              g_string_append (string, ";\n");
            }
        }

      if (opened)
        g_string_append (string, "  }\n");
    }

  g_free (sorted);
  g_free (sorted_variable_ids);
}

GtkCssKeyframes *
_gtk_css_keyframes_compute (GtkCssKeyframes  *keyframes,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *style,
                            GtkCssStyle      *parent_style)
{
  GtkCssComputeContext context = { NULL, };
  GtkCssKeyframes *resolved;
  guint k, p;

  g_return_val_if_fail (keyframes != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);
  g_return_val_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style), NULL);

  resolved = gtk_css_keyframes_alloc ();
  resolved->n_keyframes = keyframes->n_keyframes;
  resolved->keyframe_progress = g_memdup2 (keyframes->keyframe_progress, keyframes->n_keyframes * sizeof (double));
  resolved->n_properties = keyframes->n_properties;
  resolved->property_ids = g_memdup2 (keyframes->property_ids, keyframes->n_properties * sizeof (guint));
  resolved->values = g_new0 (GtkCssValue *, resolved->n_keyframes * resolved->n_properties);

  context.provider = provider;
  context.style = style;
  context.parent_style = parent_style;

  for (p = 0; p < resolved->n_properties; p++)
    {
      for (k = 0; k < resolved->n_keyframes; k++)
        {
          if (KEYFRAMES_VALUE (keyframes, k, p) == NULL)
            continue;

          context.variables = keyframes->variables ? keyframes->variables[k] : NULL;

          KEYFRAMES_VALUE (resolved, k, p) =  gtk_css_value_compute (KEYFRAMES_VALUE (keyframes, k, p),
                                                                     resolved->property_ids[p],
                                                                     &context);
        }
    }

  if (keyframes->variables)
    {
      resolved->variables = g_new0 (GtkCssVariableSet *, resolved->n_keyframes);

      for (k = 0; k < resolved->n_keyframes; k++)
        {
          if (keyframes->variables[k])
            resolved->variables[k] = gtk_css_variable_set_ref (keyframes->variables[k]);
        }
    }
  else
    resolved->variables = NULL;

  resolved->variable_ids = g_memdup2 (keyframes->variable_ids, keyframes->n_variables * sizeof (int));
  resolved->n_variables = keyframes->n_variables;

  return resolved;
}

guint
_gtk_css_keyframes_get_n_properties (GtkCssKeyframes *keyframes)
{
  g_return_val_if_fail (keyframes != NULL, 0);

  return keyframes->n_properties;
}

guint
_gtk_css_keyframes_get_property_id (GtkCssKeyframes *keyframes,
                                    guint            id)
{
  g_return_val_if_fail (keyframes != NULL, 0);
  g_return_val_if_fail (id < keyframes->n_properties, 0);

  return keyframes->property_ids[id];
}

GtkCssValue *
_gtk_css_keyframes_get_value (GtkCssKeyframes *keyframes,
                              guint            id,
                              double           progress,
                              GtkCssValue     *default_value)
{
  GtkCssValue *start_value, *end_value, *result;
  double start_progress, end_progress;
  guint k;

  g_return_val_if_fail (keyframes != NULL, 0);
  g_return_val_if_fail (id < keyframes->n_properties, 0);

  start_value = default_value;
  start_progress = 0.0;
  end_value = default_value;
  end_progress = 1.0;

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      if (KEYFRAMES_VALUE (keyframes, k, id) == NULL)
        continue;

      if (keyframes->keyframe_progress[k] == progress)
        {
          return gtk_css_value_ref (KEYFRAMES_VALUE (keyframes, k, id));
        }
      else if (keyframes->keyframe_progress[k] < progress)
        {
          start_value = KEYFRAMES_VALUE (keyframes, k, id);
          start_progress = keyframes->keyframe_progress[k];
        }
      else
        {
          end_value = KEYFRAMES_VALUE (keyframes, k, id);
          end_progress = keyframes->keyframe_progress[k];
          break;
        }
    }

  progress = (progress - start_progress) / (end_progress - start_progress);

  result = gtk_css_value_transition (start_value,
                                     end_value,
                                     keyframes->property_ids[id],
                                     progress);

  /* XXX: Dear spec, what's the correct thing to do here? */
  if (result == NULL)
    return gtk_css_value_ref (start_value);

  return result;
}

guint
_gtk_css_keyframes_get_n_variables (GtkCssKeyframes *keyframes)
{
  g_return_val_if_fail (keyframes != NULL, 0);

  return keyframes->n_variables;
}

int
_gtk_css_keyframes_get_variable_id (GtkCssKeyframes *keyframes,
                                    guint            id)
{
  g_return_val_if_fail (keyframes != NULL, 0);
  g_return_val_if_fail (id < keyframes->n_variables, 0);

  return keyframes->variable_ids[id];
}

GtkCssVariableValue *
_gtk_css_keyframes_get_variable (GtkCssKeyframes     *keyframes,
                                 guint                id,
                                 double               progress,
                                 GtkCssVariableValue *default_value)
{
  GtkCssVariableValue *start_value, *end_value, *result;
  double start_progress, end_progress;
  int variable_id;
  guint k;

  g_return_val_if_fail (keyframes != NULL, 0);
  g_return_val_if_fail (id < keyframes->n_variables, 0);

  start_value = default_value;
  start_progress = 0.0;
  end_value = default_value;
  end_progress = 1.0;

  variable_id = keyframes->variable_ids[id];

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      GtkCssVariableValue *value;

      if (keyframes->variables[k] == NULL)
        continue;

      value = gtk_css_variable_set_lookup (keyframes->variables[k], variable_id, NULL);

      if (value == NULL)
        continue;

      if (keyframes->keyframe_progress[k] == progress)
        {
          return gtk_css_variable_value_ref (value);
        }
      else if (keyframes->keyframe_progress[k] < progress)
        {
          start_value = value;
          start_progress = keyframes->keyframe_progress[k];
        }
      else
        {
          end_value = value;
          end_progress = keyframes->keyframe_progress[k];
          break;
        }
    }

  progress = (progress - start_progress) / (end_progress - start_progress);

  result = gtk_css_variable_value_transition (start_value,
                                              end_value,
                                              progress);

  /* XXX: Dear spec, what's the correct thing to do here? */
  if (result == NULL)
    return start_value ? gtk_css_variable_value_ref (start_value) : NULL;

  return result;
}
