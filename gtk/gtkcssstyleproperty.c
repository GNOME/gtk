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

#include "gtkcssstylepropertyprivate.h"

#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"

enum {
  PROP_0,
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
  const GValue *initial;

  switch (prop_id)
    {
    case PROP_INHERIT:
      property->inherit = g_value_get_boolean (value);
      break;
    case PROP_INITIAL:
      initial = g_value_get_boxed (value);
      g_assert (initial);
      g_value_init (&property->initial_value, G_VALUE_TYPE (initial));
      g_value_copy (initial, &property->initial_value);
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
    case PROP_ID:
      g_value_set_boolean (value, property->id);
      break;
    case PROP_INHERIT:
      g_value_set_boolean (value, property->inherit);
      break;
    case PROP_INITIAL:
      g_value_set_boxed (value, &property->initial_value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_css_style_property_class_init (GtkCssStylePropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
                                                       G_TYPE_VALUE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  klass->style_properties = g_ptr_array_new ();
}


static void
_gtk_css_style_property_init (GtkCssStyleProperty *style_property)
{
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
const GValue *
_gtk_css_style_property_get_initial_value (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), NULL);

  return &property->initial_value;
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
                                     const GValue           *value,
                                     GString                *string)
{
  g_return_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property));
  g_return_if_fail (value != NULL);
  g_return_if_fail (string != NULL);

  if (G_VALUE_HOLDS (value, GTK_TYPE_CSS_SPECIAL_VALUE))
    {
      GEnumClass *enum_class;
      GEnumValue *enum_value;

      enum_class = g_type_class_ref (GTK_TYPE_CSS_SPECIAL_VALUE);
      enum_value = g_enum_get_value (enum_class, g_value_get_enum (value));

      g_string_append (string, enum_value->value_nick);

      g_type_class_unref (enum_class);
    }
  else if (GTK_STYLE_PROPERTY (property)->print_func)
    (* GTK_STYLE_PROPERTY (property)->print_func) (value, string);
  else
    _gtk_css_style_print_value (value, string);
}

