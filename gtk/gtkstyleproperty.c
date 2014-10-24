/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtkstylepropertyprivate.h"

#include "gtkcssprovider.h"
#include "gtkcssparserprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"

enum {
  PROP_0,
  PROP_NAME,
  PROP_VALUE_TYPE
};

G_DEFINE_ABSTRACT_TYPE (GtkStyleProperty, _gtk_style_property, G_TYPE_OBJECT)

static void
gtk_style_property_finalize (GObject *object)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (object);

  g_warning ("finalizing %s `%s', how could this happen?", G_OBJECT_TYPE_NAME (object), property->name);

  G_OBJECT_CLASS (_gtk_style_property_parent_class)->finalize (object);
}

static void
gtk_style_property_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (object);
  GtkStylePropertyClass *klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  switch (prop_id)
    {
    case PROP_NAME:
      property->name = g_value_dup_string (value);
      g_assert (property->name);
      g_assert (g_hash_table_lookup (klass->properties, property->name) == NULL);
      g_hash_table_insert (klass->properties, property->name, property);
      break;
    case PROP_VALUE_TYPE:
      property->value_type = g_value_get_gtype (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_property_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, property->name);
      break;
    case PROP_VALUE_TYPE:
      g_value_set_gtype (value, property->value_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_style_property_class_init (GtkStylePropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_property_finalize;
  object_class->set_property = gtk_style_property_set_property;
  object_class->get_property = gtk_style_property_get_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("Property name"),
                                                        P_("The name of the property"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_VALUE_TYPE,
                                   g_param_spec_gtype ("value-type",
                                                       P_("Value type"),
                                                       P_("The value type returned by GtkStyleContext"),
                                                       G_TYPE_NONE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  klass->properties = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
_gtk_style_property_init (GtkStyleProperty *property)
{
  property->value_type = G_TYPE_NONE;
}

/**
 * _gtk_style_property_parse_value:
 * @property: the property
 * @parser: the parser to parse from
 *
 * Tries to parse the given @property from the given @parser into
 * @value. The type that @value will be assigned is dependant on
 * the parser and no assumptions must be made about it. If the
 * parsing fails, %FALSE will be returned and @value will be
 * left uninitialized.
 *
 * Only if @property is a #GtkCssShorthandProperty, the @value will
 * always be a #GtkCssValue whose values can be queried with
 * _gtk_css_array_value_get_nth().
 *
 * Returns: %NULL on failure or the parsed #GtkCssValue
 **/
GtkCssValue *
_gtk_style_property_parse_value (GtkStyleProperty *property,
                                 GtkCssParser     *parser)
{
  GtkStylePropertyClass *klass;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTY (property), NULL);
  g_return_val_if_fail (parser != NULL, NULL);

  klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  return klass->parse_value (property, parser);
}

/**
 * _gtk_style_property_assign:
 * @property: the property
 * @props: The properties to assign to
 * @state: The state to assign
 * @value: (out): the #GValue with the value to be
 *     assigned
 *
 * This function is called by gtk_style_properties_set() and in
 * turn gtk_style_context_set() and similar functions to set the
 * value from code using old APIs.
 **/
void
_gtk_style_property_assign (GtkStyleProperty   *property,
                            GtkStyleProperties *props,
                            GtkStateFlags       state,
                            const GValue       *value)
{
  GtkStylePropertyClass *klass;

  g_return_if_fail (GTK_IS_STYLE_PROPERTY (property));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
G_GNUC_END_IGNORE_DEPRECATIONS;
  g_return_if_fail (value != NULL);

  klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  klass->assign (property, props, state, value);
}

/**
 * _gtk_style_property_query:
 * @property: the property
 * @value: (out): an uninitialized #GValue to be filled with the
 *   contents of the lookup
 * @query_func: The function to use to query properties
 * @query_data: The data to pass to @query_func
 *
 * This function is called by gtk_style_properties_get() and in
 * turn gtk_style_context_get() and similar functions to get the
 * value to return to code using old APIs.
 **/
void
_gtk_style_property_query (GtkStyleProperty  *property,
                           GValue            *value,
                           GtkStyleQueryFunc  query_func,
                           gpointer           query_data)
{
  GtkStylePropertyClass *klass;

  g_return_if_fail (value != NULL);
  g_return_if_fail (GTK_IS_STYLE_PROPERTY (property));
  g_return_if_fail (query_func != NULL);

  klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  return klass->query (property, value, query_func, query_data);
}

void
_gtk_style_property_init_properties (void)
{
  static gboolean initialized = FALSE;

  if (G_LIKELY (initialized))
    return;

  initialized = TRUE;

  _gtk_css_style_property_init_properties ();
  /* initialize shorthands last, they depend on the real properties existing */
  _gtk_css_shorthand_property_init_properties ();
}

/**
 * _gtk_style_property_lookup:
 * @name: name of the property to lookup
 *
 * Looks up the CSS property with the given @name. If no such
 * property exists, %NULL is returned.
 *
 * Returns: (transfer none): The property or %NULL if no
 *     property with the given name exists.
 **/
GtkStyleProperty *
_gtk_style_property_lookup (const char *name)
{
  GtkStylePropertyClass *klass;

  g_return_val_if_fail (name != NULL, NULL);

  _gtk_style_property_init_properties ();

  klass = g_type_class_peek (GTK_TYPE_STYLE_PROPERTY);

  return g_hash_table_lookup (klass->properties, name);
}

/**
 * _gtk_style_property_get_name:
 * @property: the property to query
 *
 * Gets the name of the given property.
 *
 * Returns: the name of the property
 **/
const char *
_gtk_style_property_get_name (GtkStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_STYLE_PROPERTY (property), NULL);

  return property->name;
}

/**
 * _gtk_style_property_get_value_type:
 * @property: the property to query
 *
 * Gets the value type of the @property, if the property is usable
 * in public API via _gtk_style_property_assign() and
 * _gtk_style_property_query(). If the @property is not usable in that
 * way, %G_TYPE_NONE is returned.
 *
 * Returns: the value type in use or %G_TYPE_NONE if none.
 **/
GType
_gtk_style_property_get_value_type (GtkStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_STYLE_PROPERTY (property), G_TYPE_NONE);

  return property->value_type;
}
