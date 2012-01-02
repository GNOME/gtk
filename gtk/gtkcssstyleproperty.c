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
#include "gtkstylepropertiesprivate.h"

/* for resolvage */
#include <cairo-gobject.h>
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtkwin32themeprivate.h"


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
_gtk_css_style_property_assign (GtkStyleProperty   *property,
                                GtkStyleProperties *props,
                                GtkStateFlags       state,
                                const GValue       *value)
{
  _gtk_style_properties_set_property_by_property (props,
                                                  GTK_CSS_STYLE_PROPERTY (property),
                                                  state,
                                                  value);
}

static void
_gtk_style_property_default_value (GtkStyleProperty   *property,
                                   GtkStyleProperties *properties,
                                   GtkStateFlags       state,
                                   GValue             *value)
{
  g_value_copy (_gtk_css_style_property_get_initial_value (GTK_CSS_STYLE_PROPERTY (property)), value);
}

static gboolean
resolve_color (GtkStyleProperties *props,
	       GValue             *value)
{
  GdkRGBA color;

  /* Resolve symbolic color to GdkRGBA */
  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), props, &color))
    return FALSE;

  /* Store it back, this is where GdkRGBA caching happens */
  g_value_unset (value);
  g_value_init (value, GDK_TYPE_RGBA);
  g_value_set_boxed (value, &color);

  return TRUE;
}

static gboolean
resolve_color_rgb (GtkStyleProperties *props,
                   GValue             *value)
{
  GdkColor color = { 0 };
  GdkRGBA rgba;

  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), props, &rgba))
    return FALSE;

  color.red = rgba.red * 65535. + 0.5;
  color.green = rgba.green * 65535. + 0.5;
  color.blue = rgba.blue * 65535. + 0.5;

  g_value_unset (value);
  g_value_init (value, GDK_TYPE_COLOR);
  g_value_set_boxed (value, &color);

  return TRUE;
}

static gboolean
resolve_win32_theme_part (GtkStyleProperties *props,
			  GValue             *value,
			  GValue             *value_out,
			  GtkStylePropertyContext *context)
{
  GtkWin32ThemePart  *part;
  cairo_pattern_t *pattern;

  part = g_value_get_boxed (value);
  if (part == NULL)
    return FALSE;

  pattern = _gtk_win32_theme_part_render (part, context->width, context->height);

  g_value_take_boxed (value_out, pattern);

  return TRUE;
}


static gboolean
resolve_gradient (GtkStyleProperties *props,
                  GValue             *value)
{
  cairo_pattern_t *gradient;

  if (!gtk_gradient_resolve (g_value_get_boxed (value), props, &gradient))
    return FALSE;

  /* Store it back, this is where cairo_pattern_t caching happens */
  g_value_unset (value);
  g_value_init (value, CAIRO_GOBJECT_TYPE_PATTERN);
  g_value_take_boxed (value, gradient);

  return TRUE;
}

static gboolean
resolve_shadow (GtkStyleProperties *props,
                GValue *value)
{
  GtkShadow *resolved, *base;

  base = g_value_get_boxed (value);

  if (base == NULL)
    return TRUE;
  
  if (_gtk_shadow_get_resolved (base))
    return TRUE;

  resolved = _gtk_shadow_resolve (base, props);
  if (resolved == NULL)
    return FALSE;

  g_value_take_boxed (value, resolved);

  return TRUE;
}

static void
_gtk_style_property_resolve (GtkStyleProperty       *property,
                             GtkStyleProperties     *props,
                             GtkStateFlags           state,
			     GtkStylePropertyContext *context,
                             GValue                 *val,
			     GValue                 *val_out)
{
  if (G_VALUE_TYPE (val) == GTK_TYPE_CSS_SPECIAL_VALUE)
    {
      GtkCssSpecialValue special = g_value_get_enum (val);

      g_value_unset (val);
      switch (special)
        {
        case GTK_CSS_CURRENT_COLOR:
          g_assert (_gtk_style_property_get_value_type (property) == GDK_TYPE_RGBA);
          gtk_style_properties_get_property (props, "color", state, val);
          break;
        case GTK_CSS_INHERIT:
        case GTK_CSS_INITIAL:
        default:
          g_assert_not_reached ();
        }
    }
  else if (G_VALUE_TYPE (val) == GTK_TYPE_SYMBOLIC_COLOR)
    {
      if (_gtk_style_property_get_value_type (property) == GDK_TYPE_RGBA)
        {
          if (resolve_color (props, val))
            goto out;
        }
      else if (_gtk_style_property_get_value_type (property) == GDK_TYPE_COLOR)
        {
          if (resolve_color_rgb (props, val))
            goto out;
        }
      
      g_value_unset (val);
      g_value_init (val, _gtk_style_property_get_value_type (property));
      _gtk_style_property_default_value (property, props, state, val);
    }
  else if (G_VALUE_TYPE (val) == GDK_TYPE_RGBA)
    {
      if (g_value_get_boxed (val) == NULL)
        _gtk_style_property_default_value (property, props, state, val);
    }
  else if (G_VALUE_TYPE (val) == GTK_TYPE_GRADIENT)
    {
      g_return_if_fail (_gtk_style_property_get_value_type (property) == CAIRO_GOBJECT_TYPE_PATTERN);

      if (!resolve_gradient (props, val))
        {
          g_value_unset (val);
          g_value_init (val, CAIRO_GOBJECT_TYPE_PATTERN);
          _gtk_style_property_default_value (property, props, state, val);
        }
    }
  else if (G_VALUE_TYPE (val) == GTK_TYPE_SHADOW)
    {
      if (!resolve_shadow (props, val))
        _gtk_style_property_default_value (property, props, state, val);
    }
  else if (G_VALUE_TYPE (val) == GTK_TYPE_WIN32_THEME_PART)
    {
      if (resolve_win32_theme_part (props, val, val_out, context))
	return; /* Don't copy val, this sets val_out */
      _gtk_style_property_default_value (property, props, state, val);
    }

 out:
  g_value_copy (val, val_out);
}

static void
_gtk_css_style_property_query (GtkStyleProperty   *property,
                               GtkStyleProperties *props,
                               GtkStateFlags       state,
			       GtkStylePropertyContext *context,
                               GValue             *value)
{
  const GValue *val;
  
  val = _gtk_style_properties_peek_property (props, GTK_CSS_STYLE_PROPERTY (property), state);
  if (val)
    _gtk_style_property_resolve (property, props, state, context, (GValue *) val, value);
  else
    _gtk_style_property_default_value (property, props, state, value);
}

static gboolean
gtk_css_style_property_parse_value (GtkStyleProperty *property,
                                    GValue           *value,
                                    GtkCssParser     *parser,
                                    GFile            *base)
{
  gboolean success;

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
  else if (property->property_parse_func)
    {
      GError *error = NULL;
      char *value_str;
      
      value_str = _gtk_css_parser_read_value (parser);
      if (value_str == NULL)
        return FALSE;
      
      g_value_init (value, _gtk_style_property_get_value_type (property));
      success = (*property->property_parse_func) (value_str, value, &error);

      g_free (value_str);
      if (!success)
        g_value_unset (value);

      return success;
    }

  g_value_init (value, _gtk_style_property_get_value_type (property));
  if (property->parse_func)
    success = (* property->parse_func) (parser, base, value);
  else
    success = _gtk_css_style_parse_value (value, parser, base);

  if (!success)
    g_value_unset (value);

  return success;
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

  property_class->assign = _gtk_css_style_property_assign;
  property_class->query = _gtk_css_style_property_query;
  property_class->parse_value = gtk_css_style_property_parse_value;

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

