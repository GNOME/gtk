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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkstylepropertyprivate.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssprovider.h"
#include "gtkcssparserprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"

/* the actual parsers we have */
#include "gtkanimationdescription.h"
#include "gtkbindings.h"
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

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

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, sizeof (buf), d);
  g_string_append (string, buf);
}

static void
string_append_string (GString    *str,
                      const char *string)
{
  gsize len;

  g_string_append_c (str, '"');

  do {
    len = strcspn (string, "\"\n\r\f");
    g_string_append (str, string);
    string += len;
    switch (*string)
      {
      case '\0':
        break;
      case '\n':
        g_string_append (str, "\\A ");
        break;
      case '\r':
        g_string_append (str, "\\D ");
        break;
      case '\f':
        g_string_append (str, "\\C ");
        break;
      case '\"':
        g_string_append (str, "\\\"");
        break;
      default:
        g_assert_not_reached ();
        break;
      }
  } while (*string);

  g_string_append_c (str, '"');
}

/*** IMPLEMENTATIONS ***/

static gboolean
font_family_parse (GtkCssParser *parser,
                   GFile        *base,
                   GValue       *value)
{
  GPtrArray *names;
  char *name;

  /* We don't special case generic families. Pango should do
   * that for us */

  names = g_ptr_array_new ();

  do {
    name = _gtk_css_parser_try_ident (parser, TRUE);
    if (name)
      {
        GString *string = g_string_new (name);
        g_free (name);
        while ((name = _gtk_css_parser_try_ident (parser, TRUE)))
          {
            g_string_append_c (string, ' ');
            g_string_append (string, name);
            g_free (name);
          }
        name = g_string_free (string, FALSE);
      }
    else 
      {
        name = _gtk_css_parser_read_string (parser);
        if (name == NULL)
          {
            g_ptr_array_free (names, TRUE);
            return FALSE;
          }
      }

    g_ptr_array_add (names, name);
  } while (_gtk_css_parser_try (parser, ",", TRUE));

  /* NULL-terminate array */
  g_ptr_array_add (names, NULL);
  g_value_set_boxed (value, g_ptr_array_free (names, FALSE));
  return TRUE;
}

static void
font_family_value_print (const GValue *value,
                         GString      *string)
{
  const char **names = g_value_get_boxed (value);

  if (names == NULL || *names == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_string (string, *names);
  names++;
  while (*names)
    {
      g_string_append (string, ", ");
      string_append_string (string, *names);
      names++;
    }
}

static gboolean 
bindings_value_parse (GtkCssParser *parser,
                      GFile        *base,
                      GValue       *value)
{
  GPtrArray *array;
  GtkBindingSet *binding_set;
  char *name;

  array = g_ptr_array_new ();

  do {
      name = _gtk_css_parser_try_ident (parser, TRUE);
      if (name == NULL)
        {
          _gtk_css_parser_error (parser, "Not a valid binding name");
          g_ptr_array_free (array, TRUE);
          return FALSE;
        }

      binding_set = gtk_binding_set_find (name);

      if (!binding_set)
        {
          _gtk_css_parser_error (parser, "No binding set named '%s'", name);
          g_free (name);
          continue;
        }

      g_ptr_array_add (array, binding_set);
      g_free (name);
    }
  while (_gtk_css_parser_try (parser, ",", TRUE));

  g_value_take_boxed (value, array);

  return TRUE;
}

static void
bindings_value_print (const GValue *value,
                      GString      *string)
{
  GPtrArray *array;
  guint i;

  array = g_value_get_boxed (value);

  for (i = 0; i < array->len; i++)
    {
      GtkBindingSet *binding_set = g_ptr_array_index (array, i);

      if (i > 0)
        g_string_append (string, ", ");
      g_string_append (string, binding_set->set_name);
    }
}

static gboolean 
border_corner_radius_value_parse (GtkCssParser *parser,
                                  GFile        *base,
                                  GValue       *value)
{
  GtkCssBorderCornerRadius corner;

  if (!_gtk_css_parser_try_double (parser, &corner.horizontal))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }
  else if (corner.horizontal < 0)
    goto negative;

  if (!_gtk_css_parser_try_double (parser, &corner.vertical))
    corner.vertical = corner.horizontal;
  else if (corner.vertical < 0)
    goto negative;

  g_value_set_boxed (value, &corner);
  return TRUE;

negative:
  _gtk_css_parser_error (parser, "Border radius values cannot be negative");
  return FALSE;
}

static void
border_corner_radius_value_print (const GValue *value,
                                  GString      *string)
{
  GtkCssBorderCornerRadius *corner;

  corner = g_value_get_boxed (value);

  if (corner == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_double (string, corner->horizontal);
  if (corner->horizontal != corner->vertical)
    {
      g_string_append_c (string, ' ');
      string_append_double (string, corner->vertical);
    }
}

/*** API ***/

gboolean
_gtk_style_property_parse_value (GtkStyleProperty *property,
                                 GValue           *value,
                                 GtkCssParser     *parser,
                                 GFile            *base)
{
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (parser != NULL, FALSE);

  if (property)
    {
      if (_gtk_css_parser_try (parser, "initial", TRUE))
        {
          /* the initial value can be explicitly specified with the
           * ‘initial’ keyword which all properties accept.
           */
          g_value_unset (value);
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
          g_value_unset (value);
          g_value_init (value, GTK_TYPE_CSS_SPECIAL_VALUE);
          g_value_set_enum (value, GTK_CSS_INHERIT);
          return TRUE;
        }
      else if (property->property_parse_func)
        {
          GError *error = NULL;
          char *value_str;
          gboolean success;
          
          value_str = _gtk_css_parser_read_value (parser);
          if (value_str == NULL)
            return FALSE;
          
          success = (*property->property_parse_func) (value_str, value, &error);

          g_free (value_str);

          return success;
        }

      if (property->parse_func)
        return (* property->parse_func) (parser, base, value);
    }

  return _gtk_css_style_parse_value (value, parser, base);
}

GParameter *
_gtk_style_property_unpack (GtkStyleProperty *property,
                            const GValue     *value,
                            guint            *n_params)
{
  g_return_val_if_fail (property != NULL, NULL);
  g_return_val_if_fail (property->unpack_func != NULL, NULL);
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (n_params != NULL, NULL);

  return property->unpack_func (value, n_params);
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
  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (value != NULL);

  klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  klass->assign (property, props, state, value);
}

/**
 * _gtk_style_property_query:
 * @property: the property
 * @props: The properties to query
 * @state: The state to query
 * @value: (out): an uninitialized #GValue to be filled with the
 *   contents of the lookup
 *
 * This function is called by gtk_style_properties_get() and in
 * turn gtk_style_context_get() and similar functions to get the
 * value to return to code using old APIs.
 **/
void
_gtk_style_property_query (GtkStyleProperty        *property,
                           GtkStyleProperties      *props,
                           GtkStateFlags            state,
			   GtkStylePropertyContext *context,
                           GValue                  *value)
{
  GtkStylePropertyClass *klass;

  g_return_if_fail (property != NULL);
  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (context != NULL);
  g_return_if_fail (value != NULL);

  klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  g_value_init (value, property->value_type);

  klass->query (property, props, state, context, value);
}

#define rgba_init(rgba, r, g, b, a) G_STMT_START{ \
  (rgba)->red = (r); \
  (rgba)->green = (g); \
  (rgba)->blue = (b); \
  (rgba)->alpha = (a); \
}G_STMT_END
static void
gtk_style_property_init_properties (void)
{
  static gboolean initialized = FALSE;
  GValue value = { 0, };
  char *default_font_family[] = { "Sans", NULL };
  GdkRGBA rgba;

  if (G_LIKELY (initialized))
    return;

  initialized = TRUE;

  g_value_init (&value, GDK_TYPE_RGBA);
  rgba_init (&rgba, 1, 1, 1, 1);
  g_value_set_boxed (&value, &rgba);
  _gtk_style_property_register           (g_param_spec_boxed ("color",
                                          "Foreground color",
                                          "Foreground color",
                                          GDK_TYPE_RGBA, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  rgba_init (&rgba, 0, 0, 0, 0);
  g_value_set_boxed (&value, &rgba);
  _gtk_style_property_register           (g_param_spec_boxed ("background-color",
                                          "Background color",
                                          "Background color",
                                          GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_STRV);
  g_value_set_boxed (&value, default_font_family);
  _gtk_style_property_register           (g_param_spec_boxed ("font-family",
                                                              "Font family",
                                                              "Font family",
                                                              G_TYPE_STRV, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          font_family_parse,
                                          font_family_value_print,
                                          &value);
  g_value_unset (&value);
  _gtk_style_property_register           (g_param_spec_enum ("font-style",
                                                             "Font style",
                                                             "Font style",
                                                             PANGO_TYPE_STYLE,
                                                             PANGO_STYLE_NORMAL, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_enum ("font-variant",
                                                             "Font variant",
                                                             "Font variant",
                                                             PANGO_TYPE_VARIANT,
                                                             PANGO_VARIANT_NORMAL, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  /* xxx: need to parse this properly, ie parse the numbers */
  _gtk_style_property_register           (g_param_spec_enum ("font-weight",
                                                             "Font weight",
                                                             "Font weight",
                                                             PANGO_TYPE_WEIGHT,
                                                             PANGO_WEIGHT_NORMAL, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, 10);
  _gtk_style_property_register           (g_param_spec_double ("font-size",
                                                               "Font size",
                                                               "Font size",
                                                               0, G_MAXDOUBLE, 0, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  g_value_unset (&value);

  _gtk_style_property_register           (g_param_spec_boxed ("text-shadow",
                                                              "Text shadow",
                                                              "Text shadow",
                                                              GTK_TYPE_SHADOW, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  _gtk_style_property_register           (g_param_spec_boxed ("icon-shadow",
                                                              "Icon shadow",
                                                              "Icon shadow",
                                                              GTK_TYPE_SHADOW, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("box-shadow",
                                                              "Box shadow",
                                                              "Box shadow",
                                                              GTK_TYPE_SHADOW, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-top",
                                                            "margin top",
                                                            "Margin at top",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-left",
                                                            "margin left",
                                                            "Margin at left",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-bottom",
                                                            "margin bottom",
                                                            "Margin at bottom",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-right",
                                                            "margin right",
                                                            "Margin at right",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-top",
                                                            "padding top",
                                                            "Padding at top",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-left",
                                                            "padding left",
                                                            "Padding at left",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-bottom",
                                                            "padding bottom",
                                                            "Padding at bottom",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-right",
                                                            "padding right",
                                                            "Padding at right",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-top-width",
                                                            "border top width",
                                                            "Border width at top",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-left-width",
                                                            "border left width",
                                                            "Border width at left",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-bottom-width",
                                                            "border bottom width",
                                                            "Border width at bottom",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-right-width",
                                                            "border right width",
                                                            "Border width at right",
                                                            0, G_MAXINT, 0, 0));

  _gtk_style_property_register           (g_param_spec_boxed ("border-top-left-radius",
                                                              "Border top left radius",
                                                              "Border radius of top left corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-top-right-radius",
                                                              "Border top right radius",
                                                              "Border radius of top right corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-bottom-right-radius",
                                                              "Border bottom right radius",
                                                              "Border radius of bottom right corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-bottom-left-radius",
                                                              "Border bottom left radius",
                                                              "Border radius of bottom left corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL);

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_enum ("border-style",
                                                             "Border style",
                                                             "Border style",
                                                             GTK_TYPE_BORDER_STYLE,
                                                             GTK_BORDER_STYLE_NONE, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_enum ("background-clip",
                                                             "Background clip",
                                                             "Background clip",
                                                             GTK_TYPE_CSS_AREA,
                                                             GTK_CSS_AREA_BORDER_BOX, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_enum ("background-origin",
                                                             "Background origin",
                                                             "Background origin",
                                                             GTK_TYPE_CSS_AREA,
                                                             GTK_CSS_AREA_PADDING_BOX, 0));
  g_value_init (&value, GTK_TYPE_CSS_SPECIAL_VALUE);
  g_value_set_enum (&value, GTK_CSS_CURRENT_COLOR);
  _gtk_style_property_register           (g_param_spec_boxed ("border-top-color",
                                                              "Border top color",
                                                              "Border top color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  _gtk_style_property_register           (g_param_spec_boxed ("border-right-color",
                                                              "Border right color",
                                                              "Border right color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  _gtk_style_property_register           (g_param_spec_boxed ("border-bottom-color",
                                                              "Border bottom color",
                                                              "Border bottom color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  _gtk_style_property_register           (g_param_spec_boxed ("border-left-color",
                                                              "Border left color",
                                                              "Border left color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  g_value_unset (&value);

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("background-image",
                                                              "Background Image",
                                                              "Background Image",
                                                              CAIRO_GOBJECT_TYPE_PATTERN, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("background-repeat",
                                                              "Background repeat",
                                                              "Background repeat",
                                                              GTK_TYPE_CSS_BACKGROUND_REPEAT, 0));

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image-source",
                                                              "Border image source",
                                                              "Border image source",
                                                              CAIRO_GOBJECT_TYPE_PATTERN, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image-repeat",
                                                              "Border image repeat",
                                                              "Border image repeat",
                                                              GTK_TYPE_CSS_BORDER_IMAGE_REPEAT, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image-slice",
                                                              "Border image slice",
                                                              "Border image slice",
                                                              GTK_TYPE_BORDER, 0));
  g_value_init (&value, GTK_TYPE_BORDER);
  _gtk_style_property_register           (g_param_spec_boxed ("border-image-width",
                                                              "Border image width",
                                                              "Border image width",
                                                              GTK_TYPE_BORDER, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &value);
  g_value_unset (&value);
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_object ("engine",
                                                               "Theming Engine",
                                                               "Theming Engine",
                                                               GTK_TYPE_THEMING_ENGINE, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("transition",
                                                              "Transition animation description",
                                                              "Transition animation description",
                                                              GTK_TYPE_ANIMATION_DESCRIPTION, 0));

  /* Private property holding the binding sets */
  _gtk_style_property_register           (g_param_spec_boxed ("gtk-key-bindings",
                                                              "Key bindings",
                                                              "Key bindings",
                                                              G_TYPE_PTR_ARRAY, 0),
                                          0,
                                          NULL,
                                          bindings_value_parse,
                                          bindings_value_print,
                                          NULL);

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

  gtk_style_property_init_properties ();

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


void
_gtk_style_property_register (GParamSpec               *pspec,
                              GtkStylePropertyFlags     flags,
                              GtkStylePropertyParser    property_parse_func,
                              GtkStyleParseFunc         parse_func,
                              GtkStylePrintFunc         print_func,
                              const GValue *            initial_value)
{
  GtkStyleProperty *node;
  GValue initial_fallback = { 0, };

  if (initial_value == NULL)
    {
      g_value_init (&initial_fallback, pspec->value_type);
      if (pspec->value_type == GTK_TYPE_THEMING_ENGINE)
        g_value_set_object (&initial_fallback, gtk_theming_engine_load (NULL));
      else if (pspec->value_type == PANGO_TYPE_FONT_DESCRIPTION)
        g_value_take_boxed (&initial_fallback, pango_font_description_from_string ("Sans 10"));
      else if (pspec->value_type == GDK_TYPE_RGBA)
        {
          GdkRGBA color;
          gdk_rgba_parse (&color, "pink");
          g_value_set_boxed (&initial_fallback, &color);
        }
      else if (pspec->value_type == GTK_TYPE_BORDER)
        {
          g_value_take_boxed (&initial_fallback, gtk_border_new ());
        }
      else
        g_param_value_set_default (pspec, &initial_fallback);

      initial_value = &initial_fallback;
    }

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", pspec->name,
                       "value-type", pspec->value_type,
                       NULL);
  g_assert (node->value_type == pspec->value_type);
  node->pspec = pspec;
  node->property_parse_func = property_parse_func;
  node->parse_func = parse_func;
  node->print_func = print_func;

  if (G_IS_VALUE (&initial_fallback))
    g_value_unset (&initial_fallback);
}
