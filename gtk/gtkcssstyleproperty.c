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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssstylepropertyprivate.h"

#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

enum {
  PROP_0,
  PROP_ANIMATED,
  PROP_ID,
  PROP_INHERIT,
  PROP_INITIAL
};

G_DEFINE_TYPE (GtkCssStyleProperty, _gtk_css_style_property, GTK_TYPE_STYLE_PROPERTY)

static void
gtk_css_style_property_constructed (GObject *object)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);
  GtkCssStylePropertyClass *klass = GTK_CSS_STYLE_PROPERTY_GET_CLASS (property);

  property->id = klass->style_properties->len;
  g_ptr_array_add (klass->style_properties, property);

  G_OBJECT_CLASS (_gtk_css_style_property_parent_class)->constructed (object);
}

static void
gtk_css_style_property_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_ANIMATED:
      property->animated = g_value_get_boolean (value);
      break;
    case PROP_INHERIT:
      property->inherit = g_value_get_boolean (value);
      break;
    case PROP_INITIAL:
      property->initial_value = g_value_dup_boxed (value);
      g_assert (property->initial_value != NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_style_property_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_ANIMATED:
      g_value_set_boolean (value, property->animated);
      break;
    case PROP_ID:
      g_value_set_boolean (value, property->id);
      break;
    case PROP_INHERIT:
      g_value_set_boolean (value, property->inherit);
      break;
    case PROP_INITIAL:
      g_value_set_boxed (value, property->initial_value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_css_style_property_assign (GtkStyleProperty   *property,
                                GtkStyleProperties *props,
                                GtkStateFlags       state,
                                const GValue       *value)
{
  GtkCssStyleProperty *style;
  GtkCssValue *css_value;
  
  style = GTK_CSS_STYLE_PROPERTY (property);
  css_value = style->assign_value (style, value);

  _gtk_style_properties_set_property_by_property (props,
                                                  style,
                                                  state,
                                                  css_value);
  _gtk_css_value_unref (css_value);
}

static void
_gtk_css_style_property_query (GtkStyleProperty   *property,
                               GValue             *value,
                               GtkStyleQueryFunc   query_func,
                               gpointer            query_data)
{
  GtkCssStyleProperty *style_property = GTK_CSS_STYLE_PROPERTY (property);
  GtkCssValue *css_value;
  
  css_value = (* query_func) (GTK_CSS_STYLE_PROPERTY (property)->id, query_data);
  if (css_value == NULL)
    css_value =_gtk_css_style_property_get_initial_value (style_property);

  style_property->query_value (style_property, css_value, value);
}

static GtkCssValue *
gtk_css_style_property_parse_value (GtkStyleProperty *property,
                                    GtkCssParser     *parser,
                                    GFile            *base)
{
  GtkCssStyleProperty *style_property = GTK_CSS_STYLE_PROPERTY (property);

  if (_gtk_css_parser_try (parser, "initial", TRUE))
    {
      /* the initial value can be explicitly specified with the
       * ‘initial’ keyword which all properties accept.
       */
      return _gtk_css_initial_value_new ();
    }
  else if (_gtk_css_parser_try (parser, "inherit", TRUE))
    {
      /* All properties accept the ‘inherit’ value which
       * explicitly specifies that the value will be determined
       * by inheritance. The ‘inherit’ value can be used to
       * strengthen inherited values in the cascade, and it can
       * also be used on properties that are not normally inherited.
       */
      return _gtk_css_inherit_value_new ();
    }

  return (* style_property->parse_value) (style_property, parser, base);
}

static void
_gtk_css_style_property_class_init (GtkCssStylePropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStylePropertyClass *property_class = GTK_STYLE_PROPERTY_CLASS (klass);

  object_class->constructed = gtk_css_style_property_constructed;
  object_class->set_property = gtk_css_style_property_set_property;
  object_class->get_property = gtk_css_style_property_get_property;

  g_object_class_install_property (object_class,
                                   PROP_ANIMATED,
                                   g_param_spec_boolean ("animated",
                                                         P_("Animated"),
                                                         P_("Set if the value can be animated"),
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_uint ("id",
                                                      P_("ID"),
                                                      P_("The numeric id for quick access"),
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE));
  g_object_class_install_property (object_class,
                                   PROP_INHERIT,
                                   g_param_spec_boolean ("inherit",
                                                         P_("Inherit"),
                                                         P_("Set if the value is inherited by default"),
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_INITIAL,
                                   g_param_spec_boxed ("initial-value",
                                                       P_("Initial value"),
                                                       P_("The initial specified value used for this property"),
                                                       GTK_TYPE_CSS_VALUE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  property_class->assign = _gtk_css_style_property_assign;
  property_class->query = _gtk_css_style_property_query;
  property_class->parse_value = gtk_css_style_property_parse_value;

  klass->style_properties = g_ptr_array_new ();
}

static GtkCssValue *
gtk_css_style_property_real_parse_value (GtkCssStyleProperty *property,
                                         GtkCssParser        *parser,
                                         GFile               *base)
{
  g_assert_not_reached ();
  return NULL;
}

static void
gtk_css_style_property_real_print_value (GtkCssStyleProperty *property,
                                         const GtkCssValue   *value,
                                         GString             *string)
{
  _gtk_css_value_print (value, string);
}

static GtkCssValue *
gtk_css_style_property_real_compute_value (GtkCssStyleProperty *property,
                                           GtkStyleContext     *context,
                                           GtkCssValue         *specified)
{
  return _gtk_css_value_ref (specified);
}

static void
_gtk_css_style_property_init (GtkCssStyleProperty *property)
{
  property->parse_value = gtk_css_style_property_real_parse_value;
  property->print_value = gtk_css_style_property_real_print_value;
  property->compute_value = gtk_css_style_property_real_compute_value;
}

/**
 * _gtk_css_style_property_get_n_properties:
 *
 * Gets the number of style properties. This number can increase when new
 * theme engines are loaded. Shorthand properties are not included here.
 *
 * Returns: The number of style properties.
 **/
guint
_gtk_css_style_property_get_n_properties (void)
{
  GtkCssStylePropertyClass *klass;

  klass = g_type_class_peek (GTK_TYPE_CSS_STYLE_PROPERTY);
  if (G_UNLIKELY (klass == NULL))
    {
      _gtk_style_property_init_properties ();
      klass = g_type_class_peek (GTK_TYPE_CSS_STYLE_PROPERTY);
      g_assert (klass);
    }

  return klass->style_properties->len;
}

/**
 * _gtk_css_style_property_lookup_by_id:
 * @id: the id of the property
 *
 * Gets the style property with the given id. All style properties (but not
 * shorthand properties) are indexable by id so that it's easy to use arrays
 * when doing style lookups.
 *
 * Returns: (transfer none): The style property with the given id
 **/
GtkCssStyleProperty *
_gtk_css_style_property_lookup_by_id (guint id)
{
  GtkCssStylePropertyClass *klass;

  klass = g_type_class_peek (GTK_TYPE_CSS_STYLE_PROPERTY);
  if (G_UNLIKELY (klass == NULL))
    {
      _gtk_style_property_init_properties ();
      klass = g_type_class_peek (GTK_TYPE_CSS_STYLE_PROPERTY);
      g_assert (klass);
    }
  g_return_val_if_fail (id < klass->style_properties->len, NULL);

  return g_ptr_array_index (klass->style_properties, id);
}

/**
 * _gtk_css_style_property_is_inherit:
 * @property: the property
 *
 * Queries if the given @property is inherited. See
 * <ulink url="http://www.w3.org/TR/css3-cascade/#inheritance>
 * the CSS documentation</ulink> for an explanation of this concept.
 *
 * Returns: %TRUE if the property is inherited by default.
 **/
gboolean
_gtk_css_style_property_is_inherit (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  return property->inherit;
}

/**
 * _gtk_css_style_property_is_animated:
 * @property: the property
 *
 * Queries if the given @property can be is animated. See
 * <ulink url="http://www.w3.org/TR/css3-transitions/#animatable-css>
 * the CSS documentation</ulink> for animatable properties.
 *
 * Returns: %TRUE if the property can be animated.
 **/
gboolean
_gtk_css_style_property_is_animated (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  return property->animated;
}

/**
 * _gtk_css_style_property_get_id:
 * @property: the property
 *
 * Gets the id for the given property. IDs are used to allow using arrays
 * for style lookups.
 *
 * Returns: The id of the property
 **/
guint
_gtk_css_style_property_get_id (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), 0);

  return property->id;
}

/**
 * _gtk_css_style_property_get_initial_value:
 * @property: the property
 *
 * Queries the initial value of the given @property. See
 * <ulink url="http://www.w3.org/TR/css3-cascade/#intial>
 * the CSS documentation</ulink> for an explanation of this concept.
 *
 * Returns: a reference to the initial value. The value will never change.
 **/
GtkCssValue *
_gtk_css_style_property_get_initial_value (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), NULL);

  return property->initial_value;
}

/**
 * _gtk_css_style_property_compute_value:
 * @property: the property
 * @computed: (out): an uninitialized value to be filled with the result
 * @context: the context to use for resolving
 * @specified: the value to compute from
 *
 * Converts the @specified value into the @computed value using the
 * information in @context. This step is explained in detail in
 * <ulink url="http://www.w3.org/TR/css3-cascade/#computed>
 * the CSS documentation</ulink>.
 **/
GtkCssValue *
_gtk_css_style_property_compute_value (GtkCssStyleProperty *property,
                                       GtkStyleContext     *context,
                                       GtkCssValue         *specified)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), NULL);
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return property->compute_value (property, context, specified);
}

/**
 * _gtk_css_style_property_print_value:
 * @property: the property
 * @value: the value to print
 * @string: the string to print to
 *
 * Prints @value to the given @string in CSS format. The @value must be a
 * valid specified value as parsed using the parse functions or as assigned
 * via _gtk_style_property_assign().
 **/
void
_gtk_css_style_property_print_value (GtkCssStyleProperty    *property,
                                     GtkCssValue            *value,
                                     GString                *string)
{
  g_return_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property));
  g_return_if_fail (value != NULL);
  g_return_if_fail (string != NULL);

  if (_gtk_css_value_is_inherit (value) ||
      _gtk_css_value_is_initial (value))
    _gtk_css_value_print (value, string);
  else
    property->print_value (property, value, string);
}
