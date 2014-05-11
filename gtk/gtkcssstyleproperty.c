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

#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssunsetvalueprivate.h"
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
  PROP_AFFECTS_SIZE,
  PROP_AFFECTS_FONT,
  PROP_ID,
  PROP_INHERIT,
  PROP_INITIAL
};

G_DEFINE_TYPE (GtkCssStyleProperty, _gtk_css_style_property, GTK_TYPE_STYLE_PROPERTY)

static GtkBitmask *_properties_affecting_size = NULL;
static GtkBitmask *_properties_affecting_font = NULL;

static GtkCssStylePropertyClass *gtk_css_style_property_class = NULL;

static void
gtk_css_style_property_constructed (GObject *object)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);
  GtkCssStylePropertyClass *klass = GTK_CSS_STYLE_PROPERTY_GET_CLASS (property);

  property->id = klass->style_properties->len;
  g_ptr_array_add (klass->style_properties, property);

  if (property->affects_size)
    _properties_affecting_size = _gtk_bitmask_set (_properties_affecting_size, property->id, TRUE);

  if (property->affects_font)
    _properties_affecting_font = _gtk_bitmask_set (_properties_affecting_font, property->id, TRUE);

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
    case PROP_AFFECTS_SIZE:
      property->affects_size = g_value_get_boolean (value);
      break;
    case PROP_AFFECTS_FONT:
      property->affects_font = g_value_get_boolean (value);
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
    case PROP_AFFECTS_SIZE:
      g_value_set_boolean (value, property->affects_size);
      break;
    case PROP_AFFECTS_FONT:
      g_value_set_boolean (value, property->affects_font);
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

static gboolean
_gtk_css_style_property_query_special_case (GtkCssStyleProperty *property,
                                            GValue              *value,
                                            GtkStyleQueryFunc    query_func,
                                            gpointer             query_data)
{
  GtkBorderStyle border_style;

  switch (property->id)
    {
      case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
        border_style = _gtk_css_border_style_value_get (query_func (GTK_CSS_PROPERTY_BORDER_TOP_STYLE, query_data));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          {
            g_value_init (value, G_TYPE_INT);
            return TRUE;
          }
        break;
      case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
        border_style = _gtk_css_border_style_value_get (query_func (GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE, query_data));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          {
            g_value_init (value, G_TYPE_INT);
            return TRUE;
          }
        break;
      case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
        border_style = _gtk_css_border_style_value_get (query_func (GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE, query_data));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          {
            g_value_init (value, G_TYPE_INT);
            return TRUE;
          }
        break;
      case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
        border_style = _gtk_css_border_style_value_get (query_func (GTK_CSS_PROPERTY_BORDER_LEFT_STYLE, query_data));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          {
            g_value_init (value, G_TYPE_INT);
            return TRUE;
          }
        break;
      case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
        border_style = _gtk_css_border_style_value_get (query_func (GTK_CSS_PROPERTY_OUTLINE_STYLE, query_data));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          {
            g_value_init (value, G_TYPE_INT);
            return TRUE;
          }
        break;
      default:
        break;
    }

  return FALSE;
}

static void
_gtk_css_style_property_query (GtkStyleProperty   *property,
                               GValue             *value,
                               GtkStyleQueryFunc   query_func,
                               gpointer            query_data)
{
  GtkCssStyleProperty *style_property = GTK_CSS_STYLE_PROPERTY (property);
  GtkCssValue *css_value;
  
  /* I don't like this special case being here in this generic code path, but no idea where else to put it. */
  if (_gtk_css_style_property_query_special_case (style_property, value, query_func, query_data))
    return;

  css_value = (* query_func) (GTK_CSS_STYLE_PROPERTY (property)->id, query_data);
  if (css_value == NULL)
    css_value =_gtk_css_style_property_get_initial_value (style_property);

  style_property->query_value (style_property, css_value, value);
}

static GtkCssValue *
gtk_css_style_property_parse_value (GtkStyleProperty *property,
                                    GtkCssParser     *parser)
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
  else if (_gtk_css_parser_try (parser, "unset", TRUE))
    {
      /* If the cascaded value of a property is the unset keyword,
       * then if it is an inherited property, this is treated as
       * inherit, and if it is not, this is treated as initial.
       */
      return _gtk_css_unset_value_new ();
    }

  return (* style_property->parse_value) (style_property, parser);
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
                                   PROP_AFFECTS_SIZE,
                                   g_param_spec_boolean ("affects-size",
                                                         P_("Affects size"),
                                                         P_("Set if the value affects the sizing of elements"),
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_AFFECTS_FONT,
                                   g_param_spec_boolean ("affects-font",
                                                         P_("Affects font"),
                                                         P_("Set if the value affects the font"),
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

  _properties_affecting_size = _gtk_bitmask_new ();
  _properties_affecting_font = _gtk_bitmask_new ();

  gtk_css_style_property_class = klass;
}

static GtkCssValue *
gtk_css_style_property_real_parse_value (GtkCssStyleProperty *property,
                                         GtkCssParser        *parser)
{
  g_assert_not_reached ();
  return NULL;
}

static void
_gtk_css_style_property_init (GtkCssStyleProperty *property)
{
  property->parse_value = gtk_css_style_property_real_parse_value;
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
  if (G_UNLIKELY (gtk_css_style_property_class == NULL))
    {
      _gtk_style_property_init_properties ();
      g_assert (gtk_css_style_property_class);
    }

  return gtk_css_style_property_class->style_properties->len;
}

/**
 * _gtk_css_style_property_lookup_by_id:
 * @id: the id of the property
 *
 * Gets the style property with the given id. All style properties (but not
 * shorthand properties) are indexable by id so that it’s easy to use arrays
 * when doing style lookups.
 *
 * Returns: (transfer none): The style property with the given id
 **/
GtkCssStyleProperty *
_gtk_css_style_property_lookup_by_id (guint id)
{

  if (G_UNLIKELY (gtk_css_style_property_class == NULL))
    {
      _gtk_style_property_init_properties ();
      g_assert (gtk_css_style_property_class);
    }

  g_return_val_if_fail (id < gtk_css_style_property_class->style_properties->len, NULL);

  return g_ptr_array_index (gtk_css_style_property_class->style_properties, id);
}

/**
 * _gtk_css_style_property_is_inherit:
 * @property: the property
 *
 * Queries if the given @property is inherited. See the
 * [CSS Documentation](http://www.w3.org/TR/css3-cascade/#inheritance)
 * for an explanation of this concept.
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
 * Queries if the given @property can be is animated. See the
 * [CSS Documentation](http://www.w3.org/TR/css3-transitions/#animatable-css)
 * for animatable properties.
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
 * _gtk_css_style_property_affects_size:
 * @property: the property
 *
 * Queries if the given @property affects the size of elements. This is
 * used for optimizations inside GTK, where a gtk_widget_queue_resize()
 * can be avoided if the property does not affect size.
 *
 * Returns: %TRUE if the property affects sizing of elements.
 **/
gboolean
_gtk_css_style_property_affects_size (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  return property->affects_size;
}

/**
 * _gtk_css_style_property_affects_font:
 * @property: the property
 *
 * Queries if the given @property affects the default font. This is
 * used for optimizations inside GTK, where clearing pango
 * layouts can be avoided if the font doesn’t change.
 *
 * Returns: %TRUE if the property affects the font.
 **/
gboolean
_gtk_css_style_property_affects_font (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  return property->affects_font;
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
 * Queries the initial value of the given @property. See the
 * [CSS Documentation](http://www.w3.org/TR/css3-cascade/#intial)
 * for an explanation of this concept.
 *
 * Returns: a reference to the initial value. The value will never change.
 **/
GtkCssValue *
_gtk_css_style_property_get_initial_value (GtkCssStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), NULL);

  return property->initial_value;
}

gboolean
_gtk_css_style_property_changes_affect_size (const GtkBitmask *changes)
{
  return _gtk_bitmask_intersects (changes, _properties_affecting_size);
}

gboolean
_gtk_css_style_property_changes_affect_font (const GtkBitmask *changes)
{
  return _gtk_bitmask_intersects (changes, _properties_affecting_font);
}
