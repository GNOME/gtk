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

#include <gobject/gvaluecollector.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>
#include <math.h>

#include "gtkcssparserprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/* the actual parsers we have */
#include "gtkanimationdescription.h"
#include "gtkbindings.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcsseasevalueprivate.h"
#include "gtkcssimagegradientprivate.h"
#include "gtkcssimageprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtksymboliccolorprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

/*** REGISTRATION ***/

typedef enum {
  GTK_STYLE_PROPERTY_INHERIT = (1 << 0),
  GTK_STYLE_PROPERTY_ANIMATED = (1 << 1)
} GtkStylePropertyFlags;

static void
gtk_css_style_property_register (const char *                   name,
                                 guint                          expected_id,
                                 GType                          value_type,
                                 GtkStylePropertyFlags          flags,
                                 GtkCssStylePropertyParseFunc   parse_value,
                                 GtkCssStylePropertyPrintFunc   print_value,
                                 GtkCssStylePropertyComputeFunc compute_value,
                                 GtkCssStylePropertyQueryFunc   query_value,
                                 GtkCssStylePropertyAssignFunc  assign_value,
                                 GtkCssStylePropertyEqualFunc   equal_func,
                                 GtkCssValue *                  initial_value)
{
  GtkCssStyleProperty *node;

  g_assert (initial_value != NULL);
  g_assert (parse_value != NULL);
  g_assert (value_type == G_TYPE_NONE || query_value != NULL);
  g_assert (value_type == G_TYPE_NONE || assign_value != NULL);

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "value-type", value_type,
                       "animated", (flags & GTK_STYLE_PROPERTY_ANIMATED) ? TRUE : FALSE,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", name,
                       NULL);
  
  node->parse_value = parse_value;
  if (print_value)
    node->print_value = print_value;
  if (compute_value)
    node->compute_value = compute_value;
  node->query_value = query_value;
  node->assign_value = assign_value;
  if (equal_func)
    node->equal_func = equal_func;

  _gtk_css_value_unref (initial_value);

  g_assert (_gtk_css_style_property_get_id (node) == expected_id);
}

/*** IMPLEMENTATIONS ***/

static void
query_simple (GtkCssStyleProperty *property,
              const GtkCssValue   *css_value,
              GValue              *value)
{
  _gtk_css_value_init_gvalue (css_value, value);
}

static GtkCssValue *
assign_simple (GtkCssStyleProperty *property,
              const GValue        *value)
{
  return _gtk_css_value_new_from_gvalue (value);
}

static void
query_length_as_int (GtkCssStyleProperty *property,
                     const GtkCssValue   *css_value,
                     GValue              *value)
{
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, round (_gtk_css_number_value_get (css_value, 100)));
}

static GtkCssValue *
assign_length_from_int (GtkCssStyleProperty *property,
                        const GValue        *value)
{
  return _gtk_css_number_value_new (g_value_get_int (value), GTK_CSS_PX);
}

static void
query_length_as_double (GtkCssStyleProperty *property,
                        const GtkCssValue   *css_value,
                        GValue              *value)
{
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, _gtk_css_number_value_get (css_value, 100));
}

static GtkCssValue *
assign_length_from_double (GtkCssStyleProperty *property,
                           const GValue        *value)
{
  return _gtk_css_number_value_new (g_value_get_double (value), GTK_CSS_PX);
}

static GtkCssValue *
color_parse (GtkCssStyleProperty *property,
             GtkCssParser        *parser,
             GFile               *base)
{
  GtkSymbolicColor *symbolic;

  if (_gtk_css_parser_try (parser, "currentcolor", TRUE))
    {
      symbolic = gtk_symbolic_color_ref (_gtk_symbolic_color_get_current_color ());
    }
  else
    {
      symbolic = _gtk_css_parser_read_symbolic_color (parser);
      if (symbolic == NULL)
        return NULL;
    }

  return _gtk_css_value_new_take_symbolic_color (symbolic);
}

static GtkCssValue *
color_compute (GtkCssStyleProperty    *property,
               GtkStyleContext        *context,
               GtkCssValue            *specified)
{
  return _gtk_css_rgba_value_compute_from_symbolic (specified,
                                                    _gtk_css_style_property_get_initial_value (property),
                                                    context,
                                                    FALSE);
}

static GtkCssValue *
color_property_compute (GtkCssStyleProperty    *property,
                        GtkStyleContext        *context,
                        GtkCssValue            *specified)
{
  GtkCssValue *value;

  value = _gtk_css_rgba_value_compute_from_symbolic (specified,
                                                    _gtk_css_style_property_get_initial_value (property),
                                                    context,
                                                    TRUE);
  _gtk_css_rgba_value_get_rgba (value);
  return value;
}

static void
color_query (GtkCssStyleProperty *property,
             const GtkCssValue   *css_value,
             GValue              *value)
{
  g_value_init (value, GDK_TYPE_RGBA);
  g_value_set_boxed (value, _gtk_css_rgba_value_get_rgba (css_value));
}

static GtkCssValue *
color_assign (GtkCssStyleProperty *property,
              const GValue        *value)
{
  return _gtk_css_rgba_value_new_from_rgba (g_value_get_boxed (value));
}

static GtkCssValue *
font_family_parse_one (GtkCssParser *parser)
{
  char *name;

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
        return NULL;
    }

  return _gtk_css_string_value_new_take (name);
}

static GtkCssValue *
font_family_parse (GtkCssStyleProperty *property,
                   GtkCssParser        *parser,
                   GFile               *base)
{
  return _gtk_css_array_value_parse (parser, font_family_parse_one, FALSE);
}

static void
font_family_query (GtkCssStyleProperty *property,
                   const GtkCssValue   *css_value,
                   GValue              *value)
{
  GPtrArray *array;
  guint i;

  array = g_ptr_array_new ();

  for (i = 0; i < _gtk_css_array_value_get_n_values (css_value); i++)
    {
      g_ptr_array_add (array, g_strdup (_gtk_css_string_value_get (_gtk_css_array_value_get_nth (css_value, i))));
    }

  /* NULL-terminate */
  g_ptr_array_add (array, NULL);

  g_value_init (value, G_TYPE_STRV);
  g_value_set_boxed (value, g_ptr_array_free (array, FALSE));
}

static GtkCssValue *
font_family_assign (GtkCssStyleProperty *property,
                    const GValue        *value)
{
  const char **names = g_value_get_boxed (value);
  GtkCssValue *result;
  GPtrArray *array;

  array = g_ptr_array_new ();

  for (names = g_value_get_boxed (value); *names; names++)
    {
      g_ptr_array_add (array, _gtk_css_string_value_new (*names));
    }

  result = _gtk_css_array_value_new_from_array ((GtkCssValue **) array->pdata, array->len);
  g_ptr_array_free (array, TRUE);
  return result;
}

static GtkCssValue *
parse_pango_style (GtkCssStyleProperty *property,
                   GtkCssParser        *parser,
                   GFile               *base)
{
  GtkCssValue *value = _gtk_css_font_style_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static void
query_pango_style (GtkCssStyleProperty *property,
                    const GtkCssValue   *css_value,
                    GValue              *value)
{
  g_value_init (value, PANGO_TYPE_STYLE);
  g_value_set_enum (value, _gtk_css_font_style_value_get (css_value));
}

static GtkCssValue *
assign_pango_style (GtkCssStyleProperty *property,
                    const GValue        *value)
{
  return _gtk_css_font_style_value_new (g_value_get_enum (value));
}

static GtkCssValue *
parse_pango_weight (GtkCssStyleProperty *property,
                    GtkCssParser        *parser,
                    GFile               *base)
{
  GtkCssValue *value = _gtk_css_font_weight_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static void
query_pango_weight (GtkCssStyleProperty *property,
                    const GtkCssValue   *css_value,
                    GValue              *value)
{
  g_value_init (value, PANGO_TYPE_WEIGHT);
  g_value_set_enum (value, _gtk_css_font_weight_value_get (css_value));
}

static GtkCssValue *
assign_pango_weight (GtkCssStyleProperty *property,
                     const GValue        *value)
{
  return _gtk_css_font_weight_value_new (g_value_get_enum (value));
}

static GtkCssValue *
parse_pango_variant (GtkCssStyleProperty *property,
                     GtkCssParser        *parser,
                     GFile               *base)
{
  GtkCssValue *value = _gtk_css_font_variant_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static void
query_pango_variant (GtkCssStyleProperty *property,
                     const GtkCssValue   *css_value,
                     GValue              *value)
{
  g_value_init (value, PANGO_TYPE_VARIANT);
  g_value_set_enum (value, _gtk_css_font_variant_value_get (css_value));
}

static GtkCssValue *
assign_pango_variant (GtkCssStyleProperty *property,
                      const GValue        *value)
{
  return _gtk_css_font_variant_value_new (g_value_get_enum (value));
}

static GtkCssValue *
parse_border_style (GtkCssStyleProperty *property,
                    GtkCssParser        *parser,
                    GFile               *base)
{
  GtkCssValue *value = _gtk_css_border_style_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static void
query_border_style (GtkCssStyleProperty *property,
                    const GtkCssValue   *css_value,
                    GValue              *value)
{
  g_value_init (value, GTK_TYPE_BORDER_STYLE);
  g_value_set_enum (value, _gtk_css_border_style_value_get (css_value));
}

static GtkCssValue *
assign_border_style (GtkCssStyleProperty *property,
                     const GValue        *value)
{
  return _gtk_css_border_style_value_new (g_value_get_enum (value));
}

static GtkCssValue *
parse_css_area (GtkCssStyleProperty *property,
                GtkCssParser        *parser,
                GFile               *base)
{
  GtkCssValue *value = _gtk_css_area_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
bindings_value_parse (GtkCssStyleProperty *property,
                      GtkCssParser        *parser,
                      GFile               *base)
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

  return _gtk_css_value_new_take_binding_sets (array);
}

static void
bindings_value_print (GtkCssStyleProperty *property,
                      const GtkCssValue   *value,
                      GString             *string)
{
  GPtrArray *array;
  guint i;

  array = _gtk_css_value_get_boxed (value);

  for (i = 0; i < array->len; i++)
    {
      GtkBindingSet *binding_set = g_ptr_array_index (array, i);

      if (i > 0)
        g_string_append (string, ", ");
      g_string_append (string, binding_set->set_name);
    }
}

static GtkCssValue *
shadow_value_parse (GtkCssStyleProperty *property,
                    GtkCssParser        *parser,
                    GFile               *base)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_shadow_value_parse, TRUE);
}

static GtkCssValue *
shadow_value_compute (GtkCssStyleProperty *property,
                      GtkStyleContext     *context,
                      GtkCssValue         *specified)
{
  return _gtk_css_array_value_compute (specified, _gtk_css_shadow_value_compute, context);
}

static GtkCssValue *
border_corner_radius_value_parse (GtkCssStyleProperty *property,
                                  GtkCssParser        *parser,
                                  GFile               *base)
{
  GtkCssBorderCornerRadius corner;

  if (!_gtk_css_parser_read_number (parser,
                                    &corner.horizontal,
                                    GTK_CSS_POSITIVE_ONLY
                                    | GTK_CSS_PARSE_PERCENT
                                    | GTK_CSS_NUMBER_AS_PIXELS
                                    | GTK_CSS_PARSE_LENGTH))
    return FALSE;

  if (!_gtk_css_parser_has_number (parser))
    corner.vertical = corner.horizontal;
  else if (!_gtk_css_parser_read_number (parser,
                                         &corner.vertical,
                                         GTK_CSS_POSITIVE_ONLY
                                         | GTK_CSS_PARSE_PERCENT
                                         | GTK_CSS_NUMBER_AS_PIXELS
                                         | GTK_CSS_PARSE_LENGTH))
    return FALSE;

  return _gtk_css_value_new_from_border_corner_radius (&corner);
}

static void
border_corner_radius_value_print (GtkCssStyleProperty *property,
                                  const GtkCssValue   *value,
                                  GString             *string)
{
  const GtkCssBorderCornerRadius *corner;

  corner = _gtk_css_value_get_border_corner_radius (value);

  _gtk_css_number_print (&corner->horizontal, string);

  if (!_gtk_css_number_equal (&corner->horizontal, &corner->vertical))
    {
      g_string_append_c (string, ' ');
      _gtk_css_number_print (&corner->vertical, string);
    }
}

static GtkCssValue *
css_image_value_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser,
                       GFile               *base)
{
  GtkCssImage *image;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    image = NULL;
  else
    {
      image = _gtk_css_image_new_parse (parser, base);
      if (image == NULL)
        return FALSE;
    }

  return _gtk_css_image_value_new (image);
}

static GtkCssValue *
css_image_value_compute (GtkCssStyleProperty    *property,
                         GtkStyleContext        *context,
                         GtkCssValue            *specified)
{
  GtkCssImage *image, *computed;
  
  image = _gtk_css_image_value_get_image (specified);

  if (image == NULL)
    return _gtk_css_value_ref (specified);

  computed = _gtk_css_image_compute (image, context);

  if (computed == image)
    {
      g_object_unref (computed);
      return _gtk_css_value_ref (specified);
    }

  return _gtk_css_image_value_new (computed);
}

static void
css_image_value_query (GtkCssStyleProperty *property,
                       const GtkCssValue   *css_value,
                       GValue              *value)
{
  GtkCssImage *image = _gtk_css_image_value_get_image (css_value);
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;
  cairo_matrix_t matrix;
  
  g_value_init (value, CAIRO_GOBJECT_TYPE_PATTERN);

  if (GTK_IS_CSS_IMAGE_GRADIENT (image))
    g_value_set_boxed (value, GTK_CSS_IMAGE_GRADIENT (image)->pattern);
  else if (image != NULL)
    {
      double width, height;

      /* the 100, 100 is rather random */
      _gtk_css_image_get_concrete_size (image, 0, 0, 100, 100, &width, &height);
      surface = _gtk_css_image_get_surface (image, NULL, width, height);
      pattern = cairo_pattern_create_for_surface (surface);
      cairo_matrix_init_scale (&matrix, width, height);
      cairo_pattern_set_matrix (pattern, &matrix);
      cairo_surface_destroy (surface);
      g_value_take_boxed (value, pattern);
    }
}

static GtkCssValue *
css_image_value_assign (GtkCssStyleProperty *property,
                        const GValue        *value)
{
  g_warning ("FIXME: assigning images is not implemented");
  return _gtk_css_image_value_new (NULL);
}

static GtkCssValue *
font_size_parse (GtkCssStyleProperty *property,
                 GtkCssParser        *parser,
                 GFile               *base)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return NULL;
    }

  return _gtk_css_number_value_new (d, GTK_CSS_PX);
}

static GtkCssValue *
font_size_compute (GtkCssStyleProperty *property,
                   GtkStyleContext     *context,
                   GtkCssValue         *specified)
{
  return _gtk_css_number_value_compute (specified, context);
}

static GtkCssValue *
outline_parse (GtkCssStyleProperty *property,
               GtkCssParser        *parser,
               GFile               *base)
{
  int i;

  if (!_gtk_css_parser_try_int (parser, &i))
    {
      _gtk_css_parser_error (parser, "Expected an integer");
      return NULL;
    }

  return _gtk_css_value_new_from_int (i);
}

static GtkCssValue *
border_image_repeat_parse (GtkCssStyleProperty *property,
                           GtkCssParser        *parser,
                           GFile               *base)
{
  GValue value = G_VALUE_INIT;
  GtkCssValue *result;

  g_value_init (&value, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);
  if (!_gtk_css_style_parse_value (&value, parser, base))
    {
      g_value_unset (&value);
      return NULL;
    }

  result = _gtk_css_value_new_from_gvalue (&value);
  g_value_unset (&value);

  return result;
}

static GtkCssValue *
border_image_slice_parse (GtkCssStyleProperty *property,
                          GtkCssParser        *parser,
                          GFile               *base)
{
  GValue value = G_VALUE_INIT;
  GtkCssValue *result;

  g_value_init (&value, GTK_TYPE_BORDER);
  if (!_gtk_css_style_parse_value (&value, parser, base))
    {
      g_value_unset (&value);
      return NULL;
    }

  result = _gtk_css_value_new_from_gvalue (&value);
  g_value_unset (&value);

  return result;
}

static GtkCssValue *
border_image_width_parse (GtkCssStyleProperty *property,
                          GtkCssParser        *parser,
                          GFile               *base)
{
  GValue value = G_VALUE_INIT;
  GtkCssValue *result;

  g_value_init (&value, GTK_TYPE_BORDER);
  if (!_gtk_css_style_parse_value (&value, parser, base))
    {
      g_value_unset (&value);
      return NULL;
    }

  result = _gtk_css_value_new_from_gvalue (&value);
  g_value_unset (&value);

  return result;
}

static GtkCssValue *
transition_property_parse_one (GtkCssParser *parser)
{
  GtkCssValue *value;

  value = _gtk_css_ident_value_try_parse (parser);

  if (value == NULL)
    {
      _gtk_css_parser_error (parser, "Expected an identifier");
      return NULL;
    }

  return value;
}

static GtkCssValue *
transition_property_parse (GtkCssStyleProperty *property,
                           GtkCssParser        *parser,
                           GFile               *base)
{
  return _gtk_css_array_value_parse (parser, transition_property_parse_one, FALSE);
}

static GtkCssValue *
transition_time_parse_one (GtkCssParser *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_TIME);
}

static GtkCssValue *
transition_time_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser,
                       GFile               *base)
{
  return _gtk_css_array_value_parse (parser, transition_time_parse_one, FALSE);
}

static GtkCssValue *
transition_timing_function_parse (GtkCssStyleProperty *property,
                                  GtkCssParser        *parser,
                                  GFile               *base)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_ease_value_parse, FALSE);
}

static GtkCssValue *
engine_parse (GtkCssStyleProperty *property,
              GtkCssParser        *parser,
              GFile               *base)
{
  GtkThemingEngine *engine;
  char *str;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return _gtk_css_value_new_from_theming_engine (gtk_theming_engine_load (NULL));

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid theme name");
      return NULL;
    }

  engine = gtk_theming_engine_load (str);

  if (engine == NULL)
    {
      _gtk_css_parser_error (parser, "Theming engine '%s' not found", str);
      g_free (str);
      return NULL;
    }

  g_free (str);

  return _gtk_css_value_new_from_theming_engine (engine);
}

static GtkCssValue *
transition_parse (GtkCssStyleProperty *property,
                  GtkCssParser        *parser,
                  GFile               *base)
{
  GValue value = G_VALUE_INIT;
  GtkCssValue *result;

  g_value_init (&value, GTK_TYPE_ANIMATION_DESCRIPTION);
  if (!_gtk_css_style_parse_value (&value, parser, base))
    {
      g_value_unset (&value);
      return NULL;
    }

  result = _gtk_css_value_new_from_gvalue (&value);
  g_value_unset (&value);

  return result;
}

static GtkCssValue *
parse_margin (GtkCssStyleProperty *property,
              GtkCssParser        *parser,
              GFile               *base)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
compute_margin (GtkCssStyleProperty *property,
                GtkStyleContext     *context,
                GtkCssValue         *specified)
{
  return _gtk_css_number_value_compute (specified, context);
}

static GtkCssValue *
parse_padding (GtkCssStyleProperty *property,
               GtkCssParser        *parser,
               GFile               *base)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
compute_padding (GtkCssStyleProperty *property,
                 GtkStyleContext     *context,
                 GtkCssValue         *specified)
{
  return _gtk_css_number_value_compute (specified, context);
}

static GtkCssValue *
parse_border_width (GtkCssStyleProperty *property,
                    GtkCssParser        *parser,
                    GFile               *base)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
compute_border_width (GtkCssStyleProperty    *property,
                      GtkStyleContext        *context,
                      GtkCssValue            *specified)
{
  GtkBorderStyle border_style;
  
  /* The -1 is magic that is only true because we register the style
   * properties directly after the width properties.
   */
  border_style = _gtk_css_border_style_value_get (_gtk_style_context_peek_property (context, _gtk_css_style_property_get_id (property) - 1));

  if (border_style == GTK_BORDER_STYLE_NONE ||
      border_style == GTK_BORDER_STYLE_HIDDEN)
    return _gtk_css_number_value_new (0, GTK_CSS_PX);
  else
    return _gtk_css_number_value_compute (specified, context);
}

static GtkCssValue *
background_repeat_value_parse (GtkCssStyleProperty *property,
                               GtkCssParser        *parser,
                               GFile               *base)
{
  int repeat, vertical;

  if (!_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &repeat))
    {
      _gtk_css_parser_error (parser, "Not a valid value");
      return FALSE;
    }

  if (repeat <= GTK_CSS_BACKGROUND_REPEAT_MASK)
    {
      if (_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &vertical))
        {
          if (vertical >= GTK_CSS_BACKGROUND_REPEAT_MASK)
            {
              _gtk_css_parser_error (parser, "Not a valid 2nd value");
              return FALSE;
            }
          else
            repeat |= vertical << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
        }
      else
        repeat |= repeat << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
    }

  return _gtk_css_value_new_from_enum (GTK_TYPE_CSS_BACKGROUND_REPEAT, repeat);
}

static void
background_repeat_value_print (GtkCssStyleProperty *property,
                               const GtkCssValue   *value,
                               GString             *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  GtkCssBackgroundRepeat repeat;

  repeat = _gtk_css_value_get_enum (value);
  enum_class = g_type_class_ref (GTK_TYPE_CSS_BACKGROUND_REPEAT);
  enum_value = g_enum_get_value (enum_class, repeat);

  /* only triggers for 'repeat-x' and 'repeat-y' */
  if (enum_value)
    g_string_append (string, enum_value->value_nick);
  else
    {
      enum_value = g_enum_get_value (enum_class, GTK_CSS_BACKGROUND_HORIZONTAL (repeat));
      g_string_append (string, enum_value->value_nick);

      if (GTK_CSS_BACKGROUND_HORIZONTAL (repeat) != GTK_CSS_BACKGROUND_VERTICAL (repeat))
        {
          enum_value = g_enum_get_value (enum_class, GTK_CSS_BACKGROUND_VERTICAL (repeat));
          g_string_append (string, " ");
          g_string_append (string, enum_value->value_nick);
        }
    }

  g_type_class_unref (enum_class);
}

static GtkCssValue *
background_size_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser,
                       GFile               *base)
{
  GtkCssBackgroundSize size = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), FALSE, FALSE};

  if (_gtk_css_parser_try (parser, "cover", TRUE))
    size.cover = TRUE;
  else if (_gtk_css_parser_try (parser, "contain", TRUE))
    size.contain = TRUE;
  else
    {
      if (_gtk_css_parser_try (parser, "auto", TRUE))
        _gtk_css_number_init (&size.width, 0, GTK_CSS_PX);
      else if (!_gtk_css_parser_read_number (parser,
                                             &size.width,
                                             GTK_CSS_POSITIVE_ONLY
                                             | GTK_CSS_PARSE_PERCENT
                                             | GTK_CSS_PARSE_LENGTH))
        return NULL;

      if (_gtk_css_parser_try (parser, "auto", TRUE))
        _gtk_css_number_init (&size.height, 0, GTK_CSS_PX);
      else if (_gtk_css_parser_has_number (parser))
        {
          if (!_gtk_css_parser_read_number (parser,
                                            &size.height,
                                            GTK_CSS_POSITIVE_ONLY
                                            | GTK_CSS_PARSE_PERCENT
                                            | GTK_CSS_PARSE_LENGTH))
            return NULL;
        }
      else
        _gtk_css_number_init (&size.height, 0, GTK_CSS_PX);
    }

  return _gtk_css_value_new_from_background_size (&size);
}

static void
background_size_print (GtkCssStyleProperty *property,
                       const GtkCssValue   *value,
                       GString             *string)
{
  const GtkCssBackgroundSize *size = _gtk_css_value_get_background_size (value);

  if (size->cover)
    g_string_append (string, "cover");
  else if (size->contain)
    g_string_append (string, "contain");
  else
    {
      if (size->width.value == 0)
        g_string_append (string, "auto");
      else
        _gtk_css_number_print (&size->width, string);

      if (size->height.value != 0)
        {
          g_string_append (string, " ");
          _gtk_css_number_print (&size->height, string);
        }
    }
}

static GtkCssValue *
background_size_compute (GtkCssStyleProperty    *property,
                         GtkStyleContext        *context,
                         GtkCssValue            *specified)
{
  const GtkCssBackgroundSize *ssize = _gtk_css_value_get_background_size (specified);
  GtkCssBackgroundSize csize;
  gboolean changed;

  csize.cover = ssize->cover;
  csize.contain = ssize->contain;
  changed = _gtk_css_number_compute (&csize.width,
				     &ssize->width,
				     context);
  changed |= _gtk_css_number_compute (&csize.height,
				      &ssize->height,
				      context);
  if (changed)
    return _gtk_css_value_new_from_background_size (&csize);
  return _gtk_css_value_ref (specified);
}

static GtkCssValue *
background_position_parse (GtkCssStyleProperty *property,
			   GtkCssParser        *parser,
			   GFile               *base)
{
  static const struct {
    const char *name;
    guint       percentage;
    gboolean    horizontal;
    gboolean    vertical;
  } names[] = {
    { "left",     0, TRUE,  FALSE },
    { "right",  100, TRUE,  FALSE },
    { "center",  50, TRUE,  TRUE  },
    { "top",      0, FALSE, TRUE  },
    { "bottom", 100, FALSE, TRUE  },
    { NULL    ,   0, TRUE,  FALSE }, /* used for numbers */
    { NULL    ,  50, TRUE,  TRUE  }  /* used for no value */
  };
  GtkCssBackgroundPosition pos;
  GtkCssNumber *missing;
  guint first, second;

  for (first = 0; names[first].name != NULL; first++)
    {
      if (_gtk_css_parser_try (parser, names[first].name, TRUE))
        {
          if (names[first].horizontal)
            {
	      _gtk_css_number_init (&pos.x, names[first].percentage, GTK_CSS_PERCENT);
              missing = &pos.y;
            }
          else
            {
	      _gtk_css_number_init (&pos.y, names[first].percentage, GTK_CSS_PERCENT);
              missing = &pos.x;
            }
          break;
        }
    }
  if (names[first].name == NULL)
    {
      missing = &pos.y;
      if (!_gtk_css_parser_read_number (parser,
					&pos.x,
					GTK_CSS_PARSE_PERCENT
					| GTK_CSS_PARSE_LENGTH))
	return NULL;
    }

  for (second = 0; names[second].name != NULL; second++)
    {
      if (_gtk_css_parser_try (parser, names[second].name, TRUE))
        {
	  _gtk_css_number_init (missing, names[second].percentage, GTK_CSS_PERCENT);
          break;
        }
    }

  if (names[second].name == NULL)
    {
      if (_gtk_css_parser_has_number (parser))
        {
          if (missing != &pos.y)
            {
              _gtk_css_parser_error (parser, "Invalid combination of values");
              return NULL;
            }
          if (!_gtk_css_parser_read_number (parser,
                                            missing,
                                            GTK_CSS_PARSE_PERCENT
                                            | GTK_CSS_PARSE_LENGTH))
	    return NULL;
        }
      else
        {
          second++;
          _gtk_css_number_init (missing, 50, GTK_CSS_PERCENT);
        }
    }
  else
    {
      if ((names[first].horizontal && !names[second].vertical) ||
          (!names[first].horizontal && !names[second].horizontal))
        {
          _gtk_css_parser_error (parser, "Invalid combination of values");
          return NULL;
        }
    }

  return _gtk_css_value_new_from_background_position (&pos);
}

static void
background_position_print (GtkCssStyleProperty *property,
			   const GtkCssValue   *value,
			   GString             *string)
{
  const GtkCssBackgroundPosition *pos = _gtk_css_value_get_background_position (value);
  static const GtkCssNumber center = GTK_CSS_NUMBER_INIT (50, GTK_CSS_PERCENT);
  static const struct {
    const char *x_name;
    const char *y_name;
    GtkCssNumber number;
  } values[] = { 
    { "left",   "top",    GTK_CSS_NUMBER_INIT (0,   GTK_CSS_PERCENT) },
    { "right",  "bottom", GTK_CSS_NUMBER_INIT (100, GTK_CSS_PERCENT) }
  };
  guint i;

  if (_gtk_css_number_equal (&pos->x, &center))
    {
      if (_gtk_css_number_equal (&pos->y, &center))
        {
          g_string_append (string, "center");
          return;
        }
    }
  else
    {
      for (i = 0; i < G_N_ELEMENTS (values); i++)
        {
          if (_gtk_css_number_equal (&pos->x, &values[i].number))
            {
              g_string_append (string, values[i].x_name);
              break;
            }
        }
      if (i == G_N_ELEMENTS (values))
        _gtk_css_number_print (&pos->x, string);

      if (_gtk_css_number_equal (&pos->y, &center))
        return;

      g_string_append_c (string, ' ');
    }

  for (i = 0; i < G_N_ELEMENTS (values); i++)
    {
      if (_gtk_css_number_equal (&pos->y, &values[i].number))
        {
          g_string_append (string, values[i].y_name);
          break;
        }
    }
  if (i == G_N_ELEMENTS (values))
    {
      if (_gtk_css_number_equal (&pos->x, &center))
        g_string_append (string, "center ");
      _gtk_css_number_print (&pos->y, string);
    }
}

static GtkCssValue *
background_position_compute (GtkCssStyleProperty    *property,
			     GtkStyleContext        *context,
			     GtkCssValue            *specified)
{
  const GtkCssBackgroundPosition *spos = _gtk_css_value_get_background_position (specified);
  GtkCssBackgroundPosition cpos;
  gboolean changed;

  changed = _gtk_css_number_compute (&cpos.x,
				     &spos->x,
				     context);
  changed |= _gtk_css_number_compute (&cpos.y,
				      &spos->y,
				      context);
  if (changed)
    return _gtk_css_value_new_from_background_position (&cpos);
  return _gtk_css_value_ref (specified);
}

/*** REGISTRATION ***/

static GtkSymbolicColor *
gtk_symbolic_color_new_rgba (double red,
                             double green,
                             double blue,
                             double alpha)
{
  GdkRGBA rgba = { red, green, blue, alpha };

  return gtk_symbolic_color_new_literal (&rgba);
}

void
_gtk_css_style_property_init_properties (void)
{
  GtkCssBackgroundSize default_background_size = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), FALSE, FALSE };
  GtkCssBackgroundPosition default_background_position = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PERCENT), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PERCENT)};
  GtkCssBorderCornerRadius no_corner_radius = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX) };
  GtkBorder border_of_ones = { 1, 1, 1, 1 };
  GtkCssBorderImageRepeat border_image_repeat = { GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_STRETCH };

  /* Initialize "color" and "font-size" first,
   * so that when computing values later they are
   * done first. That way, 'currentColor' and font
   * sizes in em can be looked up properly */
  gtk_css_style_property_register        ("color",
                                          GTK_CSS_PROPERTY_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_property_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_new_rgba (1, 1, 1, 1)));
  gtk_css_style_property_register        ("font-size",
                                          GTK_CSS_PROPERTY_FONT_SIZE,
                                          G_TYPE_DOUBLE,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          font_size_parse,
                                          NULL,
                                          font_size_compute,
                                          query_length_as_double,
                                          assign_length_from_double,
                                          NULL,
                                          /* XXX: This should be 'normal' */
                                          _gtk_css_number_value_new (10.0, GTK_CSS_PX));

  /* properties that aren't referenced when computing values
   * start here */
  gtk_css_style_property_register        ("background-color",
                                          GTK_CSS_PROPERTY_BACKGROUND_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_new_rgba (0, 0, 0, 0)));

  gtk_css_style_property_register        ("font-family",
                                          GTK_CSS_PROPERTY_FONT_FAMILY,
                                          G_TYPE_STRV,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          font_family_parse,
                                          NULL,
                                          NULL,
                                          font_family_query,
                                          font_family_assign,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_string_value_new ("Sans")));
  gtk_css_style_property_register        ("font-style",
                                          GTK_CSS_PROPERTY_FONT_STYLE,
                                          PANGO_TYPE_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          parse_pango_style,
                                          NULL,
                                          NULL,
                                          query_pango_style,
                                          assign_pango_style,
                                          NULL,
                                          _gtk_css_font_style_value_new (PANGO_STYLE_NORMAL));
  gtk_css_style_property_register        ("font-variant",
                                          GTK_CSS_PROPERTY_FONT_VARIANT,
                                          PANGO_TYPE_VARIANT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          parse_pango_variant,
                                          NULL,
                                          NULL,
                                          query_pango_variant,
                                          assign_pango_variant,
                                          NULL,
                                          _gtk_css_font_variant_value_new (PANGO_VARIANT_NORMAL));
  gtk_css_style_property_register        ("font-weight",
                                          GTK_CSS_PROPERTY_FONT_WEIGHT,
                                          PANGO_TYPE_WEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          parse_pango_weight,
                                          NULL,
                                          NULL,
                                          query_pango_weight,
                                          assign_pango_weight,
                                          NULL,
                                          _gtk_css_font_weight_value_new (PANGO_WEIGHT_NORMAL));

  gtk_css_style_property_register        ("text-shadow",
                                          GTK_CSS_PROPERTY_TEXT_SHADOW,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          shadow_value_parse,
                                          NULL,
                                          shadow_value_compute,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (NULL));

  gtk_css_style_property_register        ("icon-shadow",
                                          GTK_CSS_PROPERTY_ICON_SHADOW,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          shadow_value_parse,
                                          NULL,
                                          shadow_value_compute,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (NULL));

  gtk_css_style_property_register        ("box-shadow",
                                          GTK_CSS_PROPERTY_BOX_SHADOW,
                                          G_TYPE_NONE,
                                          0,
                                          shadow_value_parse,
                                          NULL,
                                          shadow_value_compute,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (NULL));

  gtk_css_style_property_register        ("margin-top",
                                          GTK_CSS_PROPERTY_MARGIN_TOP,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-left",
                                          GTK_CSS_PROPERTY_MARGIN_LEFT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-bottom",
                                          GTK_CSS_PROPERTY_MARGIN_BOTTOM,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-right",
                                          GTK_CSS_PROPERTY_MARGIN_RIGHT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-top",
                                          GTK_CSS_PROPERTY_PADDING_TOP,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-left",
                                          GTK_CSS_PROPERTY_PADDING_LEFT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-bottom",
                                          GTK_CSS_PROPERTY_PADDING_BOTTOM,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-right",
                                          GTK_CSS_PROPERTY_PADDING_RIGHT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  /* IMPORTANT: compute_border_width() requires that the border-width
   * properties be immeditaly followed by the border-style properties
   */
  gtk_css_style_property_register        ("border-top-style",
                                          GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          parse_border_style,
                                          NULL,
                                          NULL,
                                          query_border_style,
                                          assign_border_style,
                                          NULL,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-top-width",
                                          GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-left-style",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          parse_border_style,
                                          NULL,
                                          NULL,
                                          query_border_style,
                                          assign_border_style,
                                          NULL,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-left-width",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-bottom-style",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          parse_border_style,
                                          NULL,
                                          NULL,
                                          query_border_style,
                                          assign_border_style,
                                          NULL,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-bottom-width",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-right-style",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          parse_border_style,
                                          NULL,
                                          NULL,
                                          query_border_style,
                                          assign_border_style,
                                          NULL,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-right-width",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));

  gtk_css_style_property_register        ("border-top-left-radius",
                                          GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_border_corner_radius (&no_corner_radius));
  gtk_css_style_property_register        ("border-top-right-radius",
                                          GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_border_corner_radius (&no_corner_radius));
  gtk_css_style_property_register        ("border-bottom-right-radius",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_border_corner_radius (&no_corner_radius));
  gtk_css_style_property_register        ("border-bottom-left-radius",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_border_corner_radius (&no_corner_radius));

  gtk_css_style_property_register        ("outline-style",
                                          GTK_CSS_PROPERTY_OUTLINE_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          parse_border_style,
                                          NULL,
                                          NULL,
                                          query_border_style,
                                          assign_border_style,
                                          NULL,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("outline-width",
                                          GTK_CSS_PROPERTY_OUTLINE_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          NULL,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("outline-offset",
                                          GTK_CSS_PROPERTY_OUTLINE_OFFSET,
                                          G_TYPE_INT,
                                          0,
                                          outline_parse,
                                          NULL,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_int (0));

  gtk_css_style_property_register        ("background-clip",
                                          GTK_CSS_PROPERTY_BACKGROUND_CLIP,
                                          G_TYPE_NONE,
                                          0,
                                          parse_css_area,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_area_value_new (GTK_CSS_AREA_BORDER_BOX));
  gtk_css_style_property_register        ("background-origin",
                                          GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
                                          G_TYPE_NONE,
                                          0,
                                          parse_css_area,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_area_value_new (GTK_CSS_AREA_PADDING_BOX));
  gtk_css_style_property_register        ("background-size",
                                          GTK_CSS_PROPERTY_BACKGROUND_SIZE,
                                          G_TYPE_NONE,
                                          0,
                                          background_size_parse,
                                          background_size_print,
                                          background_size_compute,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_value_new_from_background_size (&default_background_size));
  gtk_css_style_property_register        ("background-position",
                                          GTK_CSS_PROPERTY_BACKGROUND_POSITION,
                                          G_TYPE_NONE,
                                          0,
                                          background_position_parse,
                                          background_position_print,
                                          background_position_compute,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_value_new_from_background_position (&default_background_position));

  gtk_css_style_property_register        ("border-top-color",
                                          GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_ref (
                                              _gtk_symbolic_color_get_current_color ())));
  gtk_css_style_property_register        ("border-right-color",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_ref (
                                              _gtk_symbolic_color_get_current_color ())));
  gtk_css_style_property_register        ("border-bottom-color",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_ref (
                                              _gtk_symbolic_color_get_current_color ())));
  gtk_css_style_property_register        ("border-left-color",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_ref (
                                              _gtk_symbolic_color_get_current_color ())));
  gtk_css_style_property_register        ("outline-color",
                                          GTK_CSS_PROPERTY_OUTLINE_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          color_parse,
                                          NULL,
                                          color_compute,
                                          color_query,
                                          color_assign,
                                          NULL,
                                          _gtk_css_value_new_take_symbolic_color (
                                            gtk_symbolic_color_ref (
                                              _gtk_symbolic_color_get_current_color ())));

  gtk_css_style_property_register        ("background-repeat",
                                          GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
                                          GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                          0,
                                          background_repeat_value_parse,
                                          background_repeat_value_print,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_enum (GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                                                        GTK_CSS_BACKGROUND_REPEAT | 
                                                                        (GTK_CSS_BACKGROUND_REPEAT << GTK_CSS_BACKGROUND_REPEAT_SHIFT)));
  gtk_css_style_property_register        ("background-image",
                                          GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          css_image_value_parse,
                                          NULL,
                                          css_image_value_compute,
                                          css_image_value_query,
                                          css_image_value_assign,
                                          NULL,
                                          _gtk_css_image_value_new (NULL));

  gtk_css_style_property_register        ("border-image-source",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          css_image_value_parse,
                                          NULL,
                                          css_image_value_compute,
                                          css_image_value_query,
                                          css_image_value_assign,
                                          NULL,
                                          _gtk_css_image_value_new (NULL));
  gtk_css_style_property_register        ("border-image-repeat",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
                                          GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                          0,
                                          border_image_repeat_parse,
                                          NULL,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_border_image_repeat (&border_image_repeat));

  /* XXX: The initial value is wrong, it should be 100% */
  gtk_css_style_property_register        ("border-image-slice",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
                                          GTK_TYPE_BORDER,
                                          0,
                                          border_image_slice_parse,
                                          NULL,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_boxed (GTK_TYPE_BORDER, &border_of_ones));
  gtk_css_style_property_register        ("border-image-width",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH,
                                          GTK_TYPE_BORDER,
                                          0,
                                          border_image_width_parse,
                                          NULL,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_boxed (GTK_TYPE_BORDER, NULL));

  gtk_css_style_property_register        ("transition-property",
                                          GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
                                          G_TYPE_NONE,
                                          0,
                                          transition_property_parse,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_ident_value_new ("all")));
  gtk_css_style_property_register        ("transition-duration",
                                          GTK_CSS_PROPERTY_TRANSITION_DURATION,
                                          G_TYPE_NONE,
                                          0,
                                          transition_time_parse,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (0, GTK_CSS_S)));
  gtk_css_style_property_register        ("transition-timing-function",
                                          GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
                                          G_TYPE_NONE,
                                          0,
                                          transition_timing_function_parse,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (
                                            _gtk_css_ease_value_new_cubic_bezier (0.25, 0.1, 0.25, 1.0)));
  gtk_css_style_property_register        ("transition-delay",
                                          GTK_CSS_PROPERTY_TRANSITION_DELAY,
                                          G_TYPE_NONE,
                                          0,
                                          transition_time_parse,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (0, GTK_CSS_S)));

  gtk_css_style_property_register        ("engine",
                                          GTK_CSS_PROPERTY_ENGINE,
                                          GTK_TYPE_THEMING_ENGINE,
                                          0,
                                          engine_parse,
                                          NULL,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_theming_engine (gtk_theming_engine_load (NULL)));
  gtk_css_style_property_register        ("transition",
                                          GTK_CSS_PROPERTY_TRANSITION,
                                          GTK_TYPE_ANIMATION_DESCRIPTION,
                                          0,
                                          transition_parse,
                                          NULL,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_from_boxed (GTK_TYPE_ANIMATION_DESCRIPTION, NULL));

  /* Private property holding the binding sets */
  gtk_css_style_property_register        ("gtk-key-bindings",
                                          GTK_CSS_PROPERTY_GTK_KEY_BINDINGS,
                                          G_TYPE_PTR_ARRAY,
                                          0,
                                          bindings_value_parse,
                                          bindings_value_print,
                                          NULL,
                                          query_simple,
                                          assign_simple,
                                          NULL,
                                          _gtk_css_value_new_take_binding_sets (NULL));
}

