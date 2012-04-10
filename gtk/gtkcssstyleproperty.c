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

#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"

#include <math.h>
#include <cairo-gobject.h>
#include "gtkcssimagegradientprivate.h"
#include "gtkcssimageprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

enum {
  PROP_0,
  PROP_ID,
  PROP_SPECIFIED_TYPE,
  PROP_COMPUTED_TYPE,
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
    case PROP_INHERIT:
      property->inherit = g_value_get_boolean (value);
      break;
    case PROP_INITIAL:
      property->initial_value = g_value_dup_boxed (value);
      g_assert (property->initial_value != NULL);
      break;
    case PROP_COMPUTED_TYPE:
      property->computed_type = g_value_get_gtype (value);
      g_assert (property->computed_type != G_TYPE_NONE);
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
    case PROP_SPECIFIED_TYPE:
      g_value_set_gtype (value, G_VALUE_TYPE (&property->initial_value));
      break;
    case PROP_COMPUTED_TYPE:
      g_value_set_gtype (value, property->computed_type);
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
  GtkCssValue *css_value = _gtk_css_value_new_from_gvalue (value);
  _gtk_style_properties_set_property_by_property (props,
                                                  GTK_CSS_STYLE_PROPERTY (property),
                                                  state,
                                                  css_value);
  _gtk_css_value_unref (css_value);
}

static GtkCssValue *
_gtk_css_style_property_query (GtkStyleProperty   *property,
                               GtkStyleQueryFunc   query_func,
                               gpointer            query_data)
{
  GtkCssValue *css_value;
  
  css_value = (* query_func) (GTK_CSS_STYLE_PROPERTY (property)->id, query_data);
  if (css_value)
    {
      /* Somebody make this a vfunc */
      if (_gtk_css_value_holds (css_value, GTK_TYPE_CSS_IMAGE))
        {
          GtkCssImage *image = _gtk_css_value_get_image (css_value);
          cairo_pattern_t *pattern;
          cairo_surface_t *surface;
          cairo_matrix_t matrix;
          
          if (image == NULL)
	    return _gtk_css_value_new_from_pattern (NULL);
          else if (GTK_IS_CSS_IMAGE_GRADIENT (image))
	    return _gtk_css_value_new_from_pattern (GTK_CSS_IMAGE_GRADIENT (image)->pattern);
          else
            {
              double width, height;

              /* the 100, 100 is rather random */
              _gtk_css_image_get_concrete_size (image, 0, 0, 100, 100, &width, &height);
              surface = _gtk_css_image_get_surface (image, NULL, width, height);
              pattern = cairo_pattern_create_for_surface (surface);
              cairo_matrix_init_scale (&matrix, width, height);
              cairo_pattern_set_matrix (pattern, &matrix);
              cairo_surface_destroy (surface);
	      return _gtk_css_value_new_take_pattern (pattern);
            }
        }
      else if (_gtk_css_value_holds (css_value, GTK_TYPE_CSS_NUMBER))
        {
	  int v = round (_gtk_css_number_get (_gtk_css_value_get_number (css_value), 100));
	  return _gtk_css_value_new_from_int (v);
        }
      else
	return _gtk_css_value_ref (css_value);
    }
  else
    return _gtk_css_value_ref (_gtk_css_style_property_get_initial_value (GTK_CSS_STYLE_PROPERTY (property)));
}

static gboolean
gtk_css_style_property_parse_value (GtkStyleProperty *property,
                                    GValue           *value,
                                    GtkCssParser     *parser,
                                    GFile            *base)
{
  GtkCssStyleProperty *style_property = GTK_CSS_STYLE_PROPERTY (property);

  if (_gtk_css_parser_try (parser, "initial", TRUE))
    {
      /* the initial value can be explicitly specified with the
       * ‘initial’ keyword which all properties accept.
       */
      g_value_init (value, GTK_TYPE_CSS_SPECIAL_VALUE);
      g_value_set_enum (value, GTK_CSS_INITIAL);
      return TRUE;
    }
  else if (_gtk_css_parser_try (parser, "inherit", TRUE))
    {
      /* All properties accept the ‘inherit’ value which
       * explicitly specifies that the value will be determined
       * by inheritance. The ‘inherit’ value can be used to
       * strengthen inherited values in the cascade, and it can
       * also be used on properties that are not normally inherited.
       */
      g_value_init (value, GTK_TYPE_CSS_SPECIAL_VALUE);
      g_value_set_enum (value, GTK_CSS_INHERIT);
      return TRUE;
    }

  g_value_init (value, _gtk_css_style_property_get_specified_type (style_property));
  if (!(* style_property->parse_value) (style_property, value, parser, base))
    {
      g_value_unset (value);
      return FALSE;
    }

  return TRUE;
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
                                   PROP_ID,
                                   g_param_spec_uint ("id",
                                                      P_("ID"),
                                                      P_("The numeric id for quick access"),
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE));
  g_object_class_install_property (object_class,
                                   PROP_SPECIFIED_TYPE,
                                   g_param_spec_gtype ("specified-type",
                                                       P_("Specified type"),
                                                       P_("The type of values after parsing"),
                                                       G_TYPE_NONE,
                                                       G_PARAM_READABLE));
  g_object_class_install_property (object_class,
                                   PROP_COMPUTED_TYPE,
                                   g_param_spec_gtype ("computed-type",
                                                       P_("Computed type"),
                                                       P_("The type of values after style lookup"),
                                                       G_TYPE_NONE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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

static gboolean
gtk_css_style_property_real_parse_value (GtkCssStyleProperty *property,
                                         GValue              *value,
                                         GtkCssParser        *parser,
                                         GFile               *base)
{
  return _gtk_css_style_parse_value (value, parser, base);
}

static void
gtk_css_style_property_real_print_value (GtkCssStyleProperty *property,
                                         const GValue        *value,
                                         GString             *string)
{
  _gtk_css_style_print_value (value, string);
}

static GtkCssValue *
gtk_css_style_property_real_compute_value (GtkCssStyleProperty *property,
                                           GtkStyleContext     *context,
                                           GtkCssValue         *specified)
{
  return _gtk_css_style_compute_value (context, _gtk_css_style_property_get_computed_type (property), specified);
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
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), 0);

  return property->inherit;
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
 * _gtk_css_style_property_get_computed_type:
 * @property: the property to query
 *
 * Gets the #GType used for values for this property after a CSS lookup has
 * happened. _gtk_css_style_property_compute_value() will convert values to
 * this type.
 *
 * Returns: the #GType used for computed values.
 **/
GType
_gtk_css_style_property_get_computed_type (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), G_TYPE_NONE);

  return property->computed_type;
}

/**
 * _gtk_css_style_property_get_specified_type:
 * @property: the property to query
 *
 * Gets the #GType used for values for this property after CSS parsing if
 * the value is not a special keyword. _gtk_css_style_property_compute_value()
 * will convert values of this type to the computed type.
 *
 * The initial value returned by _gtk_css_style_property_get_initial_value()
 * will be of this type.
 *
 * Returns: the #GType used for specified values.
 **/
GType
_gtk_css_style_property_get_specified_type (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), G_TYPE_NONE);

  return _gtk_css_value_get_content_type (property->initial_value);
}

gboolean
_gtk_css_style_property_is_specified_type (GtkCssStyleProperty *property,
                                           GType                type)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  /* If it's our specified type, of course it's valid */
  if (type == _gtk_css_value_get_content_type (property->initial_value))
    return TRUE;

  /* The special values 'inherit' and 'initial' are always valid */
  if (type == GTK_TYPE_CSS_SPECIAL_VALUE)
    return TRUE;

  /* XXX: Someone needs to fix that legacy */
  if ((_gtk_css_value_holds (property->initial_value, GDK_TYPE_RGBA) ||
       _gtk_css_value_holds (property->initial_value, GDK_TYPE_COLOR)) &&
      type == GTK_TYPE_SYMBOLIC_COLOR)
    return TRUE;
  if (_gtk_css_value_holds (property->initial_value, CAIRO_GOBJECT_TYPE_PATTERN) &&
      type == GTK_TYPE_GRADIENT)
    return TRUE;

  return FALSE;
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
                                     GtkCssValue            *css_value,
                                     GString                *string)
{
  g_return_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property));
  g_return_if_fail (css_value != NULL);
  g_return_if_fail (string != NULL);

  if (_gtk_css_value_is_special (css_value))
    {
      GEnumClass *enum_class;
      GEnumValue *enum_value;

      enum_class = g_type_class_ref (GTK_TYPE_CSS_SPECIAL_VALUE);
      enum_value = g_enum_get_value (enum_class, _gtk_css_value_get_special_kind (css_value));

      g_string_append (string, enum_value->value_nick);

      g_type_class_unref (enum_class);
    }
  else
    {
      GValue value = G_VALUE_INIT;
      _gtk_css_value_init_gvalue (css_value, &value);
      property->print_value (property, &value, string);
      g_value_unset (&value);
    }
}

