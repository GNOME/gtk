/*
 * Copyright © 2011 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssshorthandpropertyprivate.h"

#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"

enum {
  PROP_0,
  PROP_SUBPROPERTIES,
};

G_DEFINE_TYPE (GtkCssShorthandProperty, _gtk_css_shorthand_property, GTK_TYPE_STYLE_PROPERTY)

static void
gtk_css_shorthand_property_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GtkCssShorthandProperty *property = GTK_CSS_SHORTHAND_PROPERTY (object);
  const char **subproperties;
  guint i;

  switch (prop_id)
    {
    case PROP_SUBPROPERTIES:
      subproperties = g_value_get_boxed (value);
      g_assert (subproperties);
      for (i = 0; subproperties[i] != NULL; i++)
        {
          GtkStyleProperty *subproperty = _gtk_style_property_lookup (subproperties[i]);
          g_assert (GTK_IS_CSS_STYLE_PROPERTY (subproperty));
          g_ptr_array_add (property->subproperties, subproperty);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_css_shorthand_property_assign (GtkStyleProperty   *property,
                                    GtkStyleProperties *props,
                                    GtkStateFlags       state,
                                    const GValue       *value)
{
  GParameter *parameters;
  guint i, n_parameters;

  parameters = property->unpack_func (value, &n_parameters);

  for (i = 0; i < n_parameters; i++)
    {
      _gtk_style_property_assign (_gtk_style_property_lookup (parameters[i].name),
                                  props,
                                  state,
                                  &parameters[i].value);
      g_value_unset (&parameters[i].value);
    }
  g_free (parameters);
}

static void
_gtk_css_shorthand_property_query (GtkStyleProperty   *property,
                                   GtkStyleProperties *props,
                                   GtkStateFlags       state,
			           GtkStylePropertyContext *context,
                                   GValue             *value)
{
  property->pack_func (value, props, state, context);
}

static gboolean
gtk_css_shorthand_property_parse_value (GtkStyleProperty *property,
                                        GValue           *value,
                                        GtkCssParser     *parser,
                                        GFile            *base)
{
  GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
  GValueArray *array;
  guint i;

  array = g_value_array_new (shorthand->subproperties->len);
  for (i = 0; i < shorthand->subproperties->len; i++)
    g_value_array_append (array, NULL);

  if (_gtk_css_parser_try (parser, "initial", TRUE))
    {
      /* the initial value can be explicitly specified with the
       * ‘initial’ keyword which all properties accept.
       */
      for (i = 0; i < shorthand->subproperties->len; i++)
        {
          GValue *val = g_value_array_get_nth (array, i);
          g_value_init (val, GTK_TYPE_CSS_SPECIAL_VALUE);
          g_value_set_enum (val, GTK_CSS_INITIAL);
        }
    }
  else if (_gtk_css_parser_try (parser, "inherit", TRUE))
    {
      /* All properties accept the ‘inherit’ value which
       * explicitly specifies that the value will be determined
       * by inheritance. The ‘inherit’ value can be used to
       * strengthen inherited values in the cascade, and it can
       * also be used on properties that are not normally inherited.
       */
      for (i = 0; i < shorthand->subproperties->len; i++)
        {
          GValue *val = g_value_array_get_nth (array, i);
          g_value_init (val, GTK_TYPE_CSS_SPECIAL_VALUE);
          g_value_set_enum (val, GTK_CSS_INHERIT);
        }
    }
  else if (!shorthand->parse (shorthand, array->values, parser, base))
    {
      g_value_array_free (array);
      return FALSE;
    }

  g_value_unset (value);
  g_value_init (value, G_TYPE_VALUE_ARRAY);
  g_value_set_boxed (value, array);
  return TRUE;
}

static void
_gtk_css_shorthand_property_class_init (GtkCssShorthandPropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStylePropertyClass *property_class = GTK_STYLE_PROPERTY_CLASS (klass);

  object_class->set_property = gtk_css_shorthand_property_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SUBPROPERTIES,
                                   g_param_spec_boxed ("subproperties",
                                                       P_("Subproperties"),
                                                       P_("The list of subproperties"),
                                                       G_TYPE_STRV,
                                                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  property_class->assign = _gtk_css_shorthand_property_assign;
  property_class->query = _gtk_css_shorthand_property_query;
  property_class->parse_value = gtk_css_shorthand_property_parse_value;
}

/* XXX: This function is compat only, don't read it */
static gboolean
gtk_css_shorthand_property_parse (GtkCssShorthandProperty *shorthand,
                                  GValue                  *values,
                                  GtkCssParser            *parser,
                                  GFile                   *base)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (shorthand);
  GParameter *parameters;
  guint i, j, n_parameters;

  GValue val = G_VALUE_INIT;

  g_value_init (&val, _gtk_style_property_get_value_type (property));
  if (property->parse_func)
    {
      if (!(* property->parse_func) (parser, base, &val))
        {
          g_value_unset (&val);
          return FALSE;
        }
    }
  else if (!_gtk_css_style_parse_value (&val, parser, base))
    {
      g_value_unset (&val);
      return FALSE;
    }

  parameters = property->unpack_func (&val, &n_parameters);
  g_value_unset (&val);

  for (i = 0; i < shorthand->subproperties->len; i++)
    {
      for (j = 0; j < n_parameters; j++)
        {
          if (GTK_STYLE_PROPERTY (_gtk_css_shorthand_property_get_subproperty (shorthand, i))
              == _gtk_style_property_lookup (parameters[j].name))
            {
              g_value_init (&values[i], G_VALUE_TYPE (&parameters[j].value));
              g_value_copy (&parameters[j].value, &values[i]);
              g_value_unset (&parameters[j].value);
              break;
            }
        }
      g_assert (j < n_parameters);
    }
  
  g_free (parameters);

  return TRUE;
}

static void
_gtk_css_shorthand_property_init (GtkCssShorthandProperty *shorthand)
{
  shorthand->subproperties = g_ptr_array_new_with_free_func (g_object_unref);

  shorthand->parse = gtk_css_shorthand_property_parse;
}

GtkCssStyleProperty *
_gtk_css_shorthand_property_get_subproperty (GtkCssShorthandProperty *shorthand,
                                             guint                    property)
{
  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_PROPERTY (shorthand), NULL);
  g_return_val_if_fail (property < shorthand->subproperties->len, NULL);

  return g_ptr_array_index (shorthand->subproperties, property);
}

guint
_gtk_css_shorthand_property_get_n_subproperties (GtkCssShorthandProperty *shorthand)
{
  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_PROPERTY (shorthand), 0);
  
  return shorthand->subproperties->len;
}

