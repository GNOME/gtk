/*
 * Copyright (C) 2023 GNOME Foundation Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alice Mikhaylenko <alicem@gnome.org>
 */

#include "config.h"

#include "gtkcssreferencevalueprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcsscustompropertypoolprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssunsetvalueprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkstyleproviderprivate.h"

#define GDK_ARRAY_NAME gtk_css_refs
#define GDK_ARRAY_TYPE_NAME GtkCssRefs
#define GDK_ARRAY_ELEMENT_TYPE gpointer
#define GDK_ARAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define MAX_TOKEN_LENGTH 65536

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE

  GtkStyleProperty *property;
  GtkCssVariableValue *value;
  GFile *file;
  guint subproperty;
};

static void
gtk_css_value_reference_free (GtkCssValue *value)
{
  gtk_css_variable_value_unref (value->value);
  if (value->file)
    g_object_unref (value->file);
}

static gboolean
resolve_references_do (GtkCssVariableValue *value,
                       GtkCssVariableSet   *style_variables,
                       GtkCssVariableSet   *keyframes_variables,
                       gboolean             root,
                       GtkCssRefs          *refs,
                       gsize               *out_length,
                       gsize               *out_n_refs)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();
  gsize i;
  gsize length = value->length;
  gsize n_refs = 0;

  if (value->is_invalid)
    goto error;

  if (!root)
    {
      n_refs += 1;
      gtk_css_refs_append (refs, value);
    }

  for (i = 0; i < value->n_references; i++)
    {
      GtkCssVariableValueReference *ref = &value->references[i];
      int id = gtk_css_custom_property_pool_lookup (pool, ref->name);
      GtkCssVariableValue *var_value = NULL;
      gsize var_length, var_refs;
      GtkCssVariableSet *source = style_variables;

      if (keyframes_variables)
        var_value = gtk_css_variable_set_lookup (keyframes_variables, id, NULL);

      if (!var_value && style_variables)
        var_value = gtk_css_variable_set_lookup (style_variables, id, &source);

      if (!var_value || !resolve_references_do (var_value, source, keyframes_variables,
                                                FALSE, refs, &var_length, &var_refs))
        {
          var_value = ref->fallback;

          if (!var_value || !resolve_references_do (var_value, style_variables, keyframes_variables,
                                                    FALSE, refs, &var_length, &var_refs))
            goto error;
        }

      length += var_length - ref->length;
      n_refs += var_refs;

      if (length > MAX_TOKEN_LENGTH)
        goto error;
    }

  if (out_length)
    *out_length = length;

  if (out_n_refs)
    *out_n_refs = n_refs;

  return TRUE;

error:
  /* Remove the references we added as if nothing happened */
  gtk_css_refs_splice (refs, gtk_css_refs_get_size (refs) - n_refs, n_refs, TRUE, NULL, 0);

  if (out_length)
    *out_length = 0;

  if (out_n_refs)
    *out_n_refs = 0;

  return FALSE;
}

static void
resolve_references (GtkCssVariableValue *input,
                    GtkCssStyle         *style,
                    GtkCssVariableSet   *keyframes_variables,
                    GtkCssRefs          *refs)
  if (!resolve_references_do (input, style->variables, keyframes_variables, TRUE, refs, NULL, NULL))
    gtk_css_refs_clear (refs);
}

static void
parser_error (GtkCssParser         *parser,
              const GtkCssLocation *start,
              const GtkCssLocation *end,
              const GError         *error,
              gpointer              user_data)
{
  GtkStyleProvider *provider = user_data;
  GtkCssSection *section;

  section = gtk_css_section_new (gtk_css_parser_get_file (parser),
                                 start,
                                 end);

  gtk_style_provider_emit_error (provider, section, (GError *) error);

  gtk_css_section_unref (section);
}

static GtkCssValue *
gtk_css_value_reference_compute (GtkCssValue       *value,
                                 guint              property_id,
                                 GtkStyleProvider  *provider,
                                 GtkCssStyle       *style,
                                 GtkCssStyle       *parent_style,
                                 GtkCssVariableSet *variables)
{
  GtkCssValue *result = NULL, *computed;
  GtkCssRefs refs;

  gtk_css_refs_init (&refs);

  resolve_references (value->value, style, variables, &refs);

  if (gtk_css_refs_get_size (&refs) > 0)
    {
      GtkCssParser *value_parser =
        gtk_css_parser_new_for_token_stream (value->value,
                                             value->file,
                                             (GtkCssVariableValue **) refs.start,
                                             gtk_css_refs_get_size (&refs),
                                             parser_error, provider, NULL);

      result = _gtk_style_property_parse_value (value->property, value_parser);
      gtk_css_parser_unref (value_parser);
    }

  gtk_css_refs_clear (&refs);

  if (result == NULL)
    result = _gtk_css_unset_value_new ();

  if (GTK_IS_CSS_SHORTHAND_PROPERTY (value->property))
    {
      GtkCssValue *sub = gtk_css_value_ref (_gtk_css_array_value_get_nth (result, value->subproperty));
      gtk_css_value_unref (result);
      result = sub;
    }

  computed = _gtk_css_value_compute (result,
                                     property_id,
                                     provider,
                                     style,
                                     parent_style,
                                     variables);
  computed->is_computed = TRUE;

  gtk_css_value_unref (result);

  return computed;
}

static gboolean
gtk_css_value_reference_equal (const GtkCssValue *value1,
                               const GtkCssValue *value2)
{
  return FALSE;
}

static GtkCssValue *
gtk_css_value_reference_transition (GtkCssValue *start,
                                    GtkCssValue *end,
                                    guint        property_id,
                                    double       progress)
{
  return NULL;
}

static void
gtk_css_value_reference_print (const GtkCssValue *value,
                               GString           *string)
{
  gtk_css_variable_value_print (value->value, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_REFERENCE = {
  "GtkCssReferenceValue",
  gtk_css_value_reference_free,
  gtk_css_value_reference_compute,
  gtk_css_value_reference_equal,
  gtk_css_value_reference_transition,
  NULL,
  NULL,
  gtk_css_value_reference_print
};

GtkCssValue *
_gtk_css_reference_value_new (GtkStyleProperty    *property,
                              GtkCssVariableValue *value,
                              GFile               *file)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_REFERENCE);
  result->property = property;
  result->value = gtk_css_variable_value_ref (value);
  result->contains_variables = TRUE;

  if (file)
    result->file = g_object_ref (file);
  else
    result->file = NULL;

  return result;
}

void
_gtk_css_reference_value_set_subproperty (GtkCssValue *value,
                                          guint        property)
{
  g_assert (GTK_IS_CSS_SHORTHAND_PROPERTY (value->property));

  value->subproperty = property;
}

