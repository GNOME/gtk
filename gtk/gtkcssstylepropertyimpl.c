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

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/* the actual parsers we have */
#include "gtkbindings.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssbgsizevalueprivate.h"
#include "gtkcssbordervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcsseasevalueprivate.h"
#include "gtkcssenginevalueprivate.h"
#include "gtkcssiconthemevalueprivate.h"
#include "gtkcssimageprivate.h"
#include "gtkcssimagebuiltinprivate.h"
#include "gtkcssimagegradientprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

#include "deprecated/gtkthemingengine.h"

/*** REGISTRATION ***/

typedef enum {
  GTK_STYLE_PROPERTY_INHERIT = (1 << 0),
  GTK_STYLE_PROPERTY_ANIMATED = (1 << 1),
} GtkStylePropertyFlags;

static void
gtk_css_style_property_register (const char *                   name,
                                 guint                          expected_id,
                                 GType                          value_type,
                                 GtkStylePropertyFlags          flags,
                                 GtkCssAffects                  affects,
                                 GtkCssStylePropertyParseFunc   parse_value,
                                 GtkCssStylePropertyQueryFunc   query_value,
                                 GtkCssStylePropertyAssignFunc  assign_value,
                                 GtkCssValue *                  initial_value)
{
  GtkCssStyleProperty *node;

  g_assert (initial_value != NULL);
  g_assert (parse_value != NULL);
  g_assert (value_type == G_TYPE_NONE || query_value != NULL);
  g_assert (value_type == G_TYPE_NONE || assign_value != NULL);

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "value-type", value_type,
                       "affects", affects,
                       "animated", (flags & GTK_STYLE_PROPERTY_ANIMATED) ? TRUE : FALSE,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", name,
                       NULL);
  
  node->parse_value = parse_value;
  node->query_value = query_value;
  node->assign_value = assign_value;

  _gtk_css_value_unref (initial_value);

  g_assert (_gtk_css_style_property_get_id (node) == expected_id);
}

/*** IMPLEMENTATIONS ***/

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

static void
query_border (GtkCssStyleProperty *property,
              const GtkCssValue   *css_value,
              GValue              *value)
{
  GtkBorder border;

  g_value_init (value, GTK_TYPE_BORDER);
  
  border.top = round (_gtk_css_number_value_get (_gtk_css_border_value_get_top (css_value), 100));
  border.right = round (_gtk_css_number_value_get (_gtk_css_border_value_get_right (css_value), 100));
  border.bottom = round (_gtk_css_number_value_get (_gtk_css_border_value_get_bottom (css_value), 100));
  border.left = round (_gtk_css_number_value_get (_gtk_css_border_value_get_left (css_value), 100));

  g_value_set_boxed (value, &border);
}

static GtkCssValue *
assign_border (GtkCssStyleProperty *property,
               const GValue        *value)
{
  const GtkBorder *border = g_value_get_boxed (value);

  if (border == NULL)
    return _gtk_css_initial_value_new ();
  else
    return _gtk_css_border_value_new (_gtk_css_number_value_new (border->top, GTK_CSS_PX),
                                      _gtk_css_number_value_new (border->right, GTK_CSS_PX),
                                      _gtk_css_number_value_new (border->bottom, GTK_CSS_PX),
                                      _gtk_css_number_value_new (border->left, GTK_CSS_PX));
}

static GtkCssValue *
color_parse (GtkCssStyleProperty *property,
             GtkCssParser        *parser)
{
  return _gtk_css_color_value_parse (parser);
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
                   GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, font_family_parse_one);
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
                   GtkCssParser        *parser)
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
                    GtkCssParser        *parser)
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
                     GtkCssParser        *parser)
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
parse_pango_stretch (GtkCssStyleProperty *property,
                     GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_stretch_value_try_parse (parser);

  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static void
query_pango_stretch (GtkCssStyleProperty *property,
                     const GtkCssValue   *css_value,
                     GValue              *value)
{
  g_value_init (value, PANGO_TYPE_STRETCH);
  g_value_set_enum (value, _gtk_css_font_stretch_value_get (css_value));
}

static GtkCssValue *
assign_pango_stretch (GtkCssStyleProperty *property,
                      const GValue        *value)
{
  return _gtk_css_font_stretch_value_new (g_value_get_enum (value));
}

static GtkCssValue *
parse_border_style (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
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
parse_css_area_one (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_area_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
parse_css_area (GtkCssStyleProperty *property,
                GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_css_area_one);
}

static GtkCssValue *
parse_one_css_direction (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_direction_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
parse_css_direction (GtkCssStyleProperty *property,
                     GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_one_css_direction);
}

static GtkCssValue *
opacity_parse (GtkCssStyleProperty *property,
	       GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
}


static GtkCssValue *
parse_one_css_play_state (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_play_state_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
parse_css_play_state (GtkCssStyleProperty *property,
                      GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_one_css_play_state);
}

static GtkCssValue *
parse_one_css_fill_mode (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_fill_mode_value_try_parse (parser);
  
  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
parse_css_fill_mode (GtkCssStyleProperty *property,
                     GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_one_css_fill_mode);
}

static GtkCssValue *
image_effect_parse (GtkCssStyleProperty *property,
		    GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_image_effect_value_try_parse (parser);

  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
icon_style_parse (GtkCssStyleProperty *property,
		  GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_icon_style_value_try_parse (parser);

  if (value == NULL)
    _gtk_css_parser_error (parser, "unknown value for property");

  return value;
}

static GtkCssValue *
bindings_value_parse_one (GtkCssParser *parser)
{
  char *name;

  name = _gtk_css_parser_try_ident (parser, TRUE);
  if (name == NULL)
    {
      _gtk_css_parser_error (parser, "Not a valid binding name");
      return NULL;
    }

  if (g_ascii_strcasecmp (name, "none") == 0)
    {
      name = NULL;
    }
  else if (!gtk_binding_set_find (name))
    {
      _gtk_css_parser_error (parser, "No binding set named '%s'", name);
      g_free (name);
      return NULL;
    }

  return _gtk_css_string_value_new_take (name);
}

static GtkCssValue *
bindings_value_parse (GtkCssStyleProperty *property,
                      GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, bindings_value_parse_one);
}

static void
bindings_value_query (GtkCssStyleProperty *property,
                      const GtkCssValue   *css_value,
                      GValue              *value)
{
  GPtrArray *array;
  guint i;

  g_value_init (value, G_TYPE_PTR_ARRAY);

  if (_gtk_css_array_value_get_n_values (css_value) == 0)
    return;

  array = NULL;

  for (i = 0; i < _gtk_css_array_value_get_n_values (css_value); i++)
    {
      const char *name;
      GtkBindingSet *binding_set;
      
      name = _gtk_css_string_value_get (_gtk_css_array_value_get_nth (css_value, i));
      if (name == NULL)
        continue;

      binding_set = gtk_binding_set_find (name);
      if (binding_set == NULL)
        continue;
      
      if (array == NULL)
        array = g_ptr_array_new ();
      g_ptr_array_add (array, binding_set);
    }

  g_value_take_boxed (value, array);
}

static GtkCssValue *
bindings_value_assign (GtkCssStyleProperty *property,
                       const GValue        *value)
{
  GPtrArray *binding_sets = g_value_get_boxed (value);
  GtkCssValue **values, *result;
  guint i;

  if (binding_sets == NULL || binding_sets->len == 0)
    return _gtk_css_array_value_new (_gtk_css_string_value_new (NULL));

  values = g_new (GtkCssValue *, binding_sets->len);

  for (i = 0; i < binding_sets->len; i++)
    {
      GtkBindingSet *binding_set = g_ptr_array_index (binding_sets, i);
      values[i] = _gtk_css_string_value_new (binding_set->set_name);
    }

  result = _gtk_css_array_value_new_from_array (values, binding_sets->len);
  g_free (values);
  return result;
}

static GtkCssValue *
box_shadow_value_parse (GtkCssStyleProperty *property,
                        GtkCssParser        *parser)
{
  return _gtk_css_shadows_value_parse (parser, TRUE);
}

static GtkCssValue *
shadow_value_parse (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  return _gtk_css_shadows_value_parse (parser, FALSE);
}

static GtkCssValue *
transform_value_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_transform_value_parse (parser);
}

static GtkCssValue *
border_corner_radius_value_parse (GtkCssStyleProperty *property,
                                  GtkCssParser        *parser)
{
  return _gtk_css_corner_value_parse (parser);
}

static GtkCssValue *
css_image_value_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  GtkCssImage *image;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    image = NULL;
  else
    {
      image = _gtk_css_image_new_parse (parser);
      if (image == NULL)
        return NULL;
    }

  return _gtk_css_image_value_new (image);
}

static GtkCssValue *
css_image_value_parse_with_builtin (GtkCssStyleProperty *property,
                                    GtkCssParser        *parser)
{
  if (_gtk_css_parser_try (parser, "builtin", TRUE))
    return _gtk_css_image_value_new (gtk_css_image_builtin_new ());

  return css_image_value_parse (property, parser);
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
background_image_value_parse_one (GtkCssParser *parser)
{
  return css_image_value_parse (NULL, parser);
}

static GtkCssValue *
background_image_value_parse (GtkCssStyleProperty *property,
                              GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, background_image_value_parse_one);
}

static void
background_image_value_query (GtkCssStyleProperty *property,
                              const GtkCssValue   *css_value,
                              GValue              *value)
{
  css_image_value_query (property, _gtk_css_array_value_get_nth (css_value, 0), value);
}

static GtkCssValue *
background_image_value_assign (GtkCssStyleProperty *property,
                               const GValue        *value)
{
  return _gtk_css_array_value_new (css_image_value_assign (property, value));
}

static GtkCssValue *
font_size_parse (GtkCssStyleProperty *property,
                 GtkCssParser        *parser)
{
  GtkCssValue *value;

  value = _gtk_css_font_size_value_try_parse (parser);
  if (value)
    return value;

  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_PARSE_LENGTH
                                      | GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_NUMBER_AS_PIXELS);
}

static GtkCssValue *
outline_parse (GtkCssStyleProperty *property,
               GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
border_image_repeat_parse (GtkCssStyleProperty *property,
                           GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_border_repeat_value_try_parse (parser);

  if (value == NULL)
    {
      _gtk_css_parser_error (parser, "Not a valid value");
      return NULL;
    }

  return value;
}

static GtkCssValue *
border_image_slice_parse (GtkCssStyleProperty *property,
                          GtkCssParser        *parser)
{
  return _gtk_css_border_value_parse (parser,
                                      GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_PARSE_NUMBER
                                      | GTK_CSS_POSITIVE_ONLY,
                                      FALSE,
                                      TRUE);
}

static GtkCssValue *
border_image_width_parse (GtkCssStyleProperty *property,
                          GtkCssParser        *parser)
{
  return _gtk_css_border_value_parse (parser,
                                      GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_PARSE_LENGTH
                                      | GTK_CSS_PARSE_NUMBER
                                      | GTK_CSS_POSITIVE_ONLY,
                                      TRUE,
                                      FALSE);
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
                           GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, transition_property_parse_one);
}

static GtkCssValue *
transition_time_parse_one (GtkCssParser *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_TIME);
}

static GtkCssValue *
transition_time_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, transition_time_parse_one);
}

static GtkCssValue *
transition_timing_function_parse (GtkCssStyleProperty *property,
                                  GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_ease_value_parse);
}

static GtkCssValue *
iteration_count_parse_one (GtkCssParser *parser)
{
  if (_gtk_css_parser_try (parser, "infinite", TRUE))
    return _gtk_css_number_value_new (HUGE_VAL, GTK_CSS_NUMBER);

  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER | GTK_CSS_POSITIVE_ONLY);
}

static GtkCssValue *
iteration_count_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, iteration_count_parse_one);
}

static GtkCssValue *
engine_parse (GtkCssStyleProperty *property,
              GtkCssParser        *parser)
{
  return _gtk_css_engine_value_parse (parser);
}

static void
engine_query (GtkCssStyleProperty *property,
              const GtkCssValue   *css_value,
              GValue              *value)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_value_init (value, GTK_TYPE_THEMING_ENGINE);
  g_value_set_object (value, _gtk_css_engine_value_get_engine (css_value));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static GtkCssValue *
engine_assign (GtkCssStyleProperty *property,
               const GValue        *value)
{
  return _gtk_css_engine_value_new (g_value_get_object (value));
}

static GtkCssValue *
parse_margin (GtkCssStyleProperty *property,
              GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
parse_padding (GtkCssStyleProperty *property,
               GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
parse_border_width (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_NUMBER_AS_PIXELS
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
background_repeat_value_parse_one (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_background_repeat_value_try_parse (parser);

  if (value == NULL)
    {
      _gtk_css_parser_error (parser, "Not a valid value");
      return NULL;
    }

  return value;
}

static GtkCssValue *
background_repeat_value_parse (GtkCssStyleProperty *property,
                               GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, background_repeat_value_parse_one);
}

static GtkCssValue *
background_size_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_bg_size_value_parse);
}

static GtkCssValue *
background_position_parse (GtkCssStyleProperty *property,
			   GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_position_value_parse);
}

static GtkCssValue *
icon_theme_value_parse (GtkCssStyleProperty *property,
		        GtkCssParser        *parser)
{
  _gtk_css_parser_error (parser, "Only 'inherit', 'initial' or 'unset' are allowed");

  return NULL;
}

/*** REGISTRATION ***/

void
_gtk_css_style_property_init_properties (void)
{
  /* Initialize "color" and "font-size" first,
   * so that when computing values later they are
   * done first. That way, 'currentColor' and font
   * sizes in em can be looked up properly */
  gtk_css_style_property_register        ("color",
                                          GTK_CSS_PROPERTY_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_FOREGROUND | GTK_CSS_AFFECTS_TEXT | GTK_CSS_AFFECTS_ICON,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_rgba (1, 1, 1, 1));
  gtk_css_style_property_register        ("font-size",
                                          GTK_CSS_PROPERTY_FONT_SIZE,
                                          G_TYPE_DOUBLE,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_FONT | GTK_CSS_AFFECTS_TEXT | GTK_CSS_AFFECTS_SIZE,
                                          font_size_parse,
                                          query_length_as_double,
                                          assign_length_from_double,
                                          _gtk_css_font_size_value_new (GTK_CSS_FONT_SIZE_MEDIUM));

  /* properties that aren't referenced when computing values
   * start here */
  gtk_css_style_property_register        ("-gtk-icon-theme",
                                          GTK_CSS_PROPERTY_ICON_THEME,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_ICON,
                                          icon_theme_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_icon_theme_value_new());
  gtk_css_style_property_register        ("background-color",
                                          GTK_CSS_PROPERTY_BACKGROUND_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_rgba (0, 0, 0, 0));

  gtk_css_style_property_register        ("font-family",
                                          GTK_CSS_PROPERTY_FONT_FAMILY,
                                          G_TYPE_STRV,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_FONT | GTK_CSS_AFFECTS_TEXT,
                                          font_family_parse,
                                          font_family_query,
                                          font_family_assign,
                                          _gtk_css_array_value_new (_gtk_css_string_value_new ("Sans")));
  gtk_css_style_property_register        ("font-style",
                                          GTK_CSS_PROPERTY_FONT_STYLE,
                                          PANGO_TYPE_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_FONT | GTK_CSS_AFFECTS_TEXT,
                                          parse_pango_style,
                                          query_pango_style,
                                          assign_pango_style,
                                          _gtk_css_font_style_value_new (PANGO_STYLE_NORMAL));
  gtk_css_style_property_register        ("font-variant",
                                          GTK_CSS_PROPERTY_FONT_VARIANT,
                                          PANGO_TYPE_VARIANT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_FONT | GTK_CSS_AFFECTS_TEXT,
                                          parse_pango_variant,
                                          query_pango_variant,
                                          assign_pango_variant,
                                          _gtk_css_font_variant_value_new (PANGO_VARIANT_NORMAL));
  gtk_css_style_property_register        ("font-weight",
                                          GTK_CSS_PROPERTY_FONT_WEIGHT,
                                          PANGO_TYPE_WEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_FONT | GTK_CSS_AFFECTS_TEXT,
                                          parse_pango_weight,
                                          query_pango_weight,
                                          assign_pango_weight,
                                          _gtk_css_font_weight_value_new (PANGO_WEIGHT_NORMAL));
  gtk_css_style_property_register        ("font-stretch",
                                          GTK_CSS_PROPERTY_FONT_STRETCH,
                                          PANGO_TYPE_STRETCH,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_FONT | GTK_CSS_AFFECTS_TEXT,
                                          parse_pango_stretch,
                                          query_pango_stretch,
                                          assign_pango_stretch,
                                          _gtk_css_font_stretch_value_new (PANGO_STRETCH_NORMAL));

  gtk_css_style_property_register        ("text-shadow",
                                          GTK_CSS_PROPERTY_TEXT_SHADOW,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT | GTK_CSS_AFFECTS_CLIP,
                                          shadow_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_shadows_value_new_none ());

  gtk_css_style_property_register        ("box-shadow",
                                          GTK_CSS_PROPERTY_BOX_SHADOW,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_CLIP,
                                          box_shadow_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_shadows_value_new_none ());

  gtk_css_style_property_register        ("margin-top",
                                          GTK_CSS_PROPERTY_MARGIN_TOP,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-left",
                                          GTK_CSS_PROPERTY_MARGIN_LEFT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-bottom",
                                          GTK_CSS_PROPERTY_MARGIN_BOTTOM,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-right",
                                          GTK_CSS_PROPERTY_MARGIN_RIGHT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-top",
                                          GTK_CSS_PROPERTY_PADDING_TOP,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-left",
                                          GTK_CSS_PROPERTY_PADDING_LEFT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-bottom",
                                          GTK_CSS_PROPERTY_PADDING_BOTTOM,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-right",
                                          GTK_CSS_PROPERTY_PADDING_RIGHT,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  /* IMPORTANT: the border-width properties must come after border-style properties,
   * they depend on them for their value computation.
   */
  gtk_css_style_property_register        ("border-top-style",
                                          GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          query_border_style,
                                          assign_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-top-width",
                                          GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-left-style",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          query_border_style,
                                          assign_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-left-width",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-bottom-style",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          query_border_style,
                                          assign_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-bottom-width",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-right-style",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          query_border_style,
                                          assign_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-right-width",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));

  gtk_css_style_property_register        ("border-top-left-radius",
                                          GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("border-top-right-radius",
                                          GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("border-bottom-right-radius",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("border-bottom-left-radius",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));

  gtk_css_style_property_register        ("outline-style",
                                          GTK_CSS_PROPERTY_OUTLINE_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          parse_border_style,
                                          query_border_style,
                                          assign_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("outline-width",
                                          GTK_CSS_PROPERTY_OUTLINE_WIDTH,
                                          G_TYPE_INT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE | GTK_CSS_AFFECTS_CLIP,
                                          parse_border_width,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("outline-offset",
                                          GTK_CSS_PROPERTY_OUTLINE_OFFSET,
                                          G_TYPE_INT,
                                          0,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          outline_parse,
                                          query_length_as_int,
                                          assign_length_from_int,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));

  gtk_css_style_property_register        ("outline-top-left-radius",
                                          GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("outline-top-right-radius",
                                          GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("outline-bottom-right-radius",
                                          GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("outline-bottom-left-radius",
                                          GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          border_corner_radius_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));

  gtk_css_style_property_register        ("background-clip",
                                          GTK_CSS_PROPERTY_BACKGROUND_CLIP,
                                          G_TYPE_NONE,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          parse_css_area,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_area_value_new (GTK_CSS_AREA_BORDER_BOX)));
  gtk_css_style_property_register        ("background-origin",
                                          GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
                                          G_TYPE_NONE,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          parse_css_area,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_area_value_new (GTK_CSS_AREA_PADDING_BOX)));
  gtk_css_style_property_register        ("background-size",
                                          GTK_CSS_PROPERTY_BACKGROUND_SIZE,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_size_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_bg_size_value_new (NULL, NULL)));
  gtk_css_style_property_register        ("background-position",
                                          GTK_CSS_PROPERTY_BACKGROUND_POSITION,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_position_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_position_value_new (_gtk_css_number_value_new (0, GTK_CSS_PERCENT),
                                                                                                 _gtk_css_number_value_new (0, GTK_CSS_PERCENT))));

  gtk_css_style_property_register        ("border-top-color",
                                          GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("border-right-color",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("border-bottom-color",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("border-left-color",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("outline-color",
                                          GTK_CSS_PROPERTY_OUTLINE_COLOR,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          color_parse,
                                          color_query,
                                          color_assign,
                                          _gtk_css_color_value_new_current_color ());

  gtk_css_style_property_register        ("background-repeat",
                                          GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
                                          G_TYPE_NONE,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_repeat_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_background_repeat_value_new (GTK_CSS_REPEAT_STYLE_REPEAT,
                                                                                                          GTK_CSS_REPEAT_STYLE_REPEAT)));
  gtk_css_style_property_register        ("background-image",
                                          GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_image_value_parse,
                                          background_image_value_query,
                                          background_image_value_assign,
                                          _gtk_css_array_value_new (_gtk_css_image_value_new (NULL)));

  gtk_css_style_property_register        ("border-image-source",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          css_image_value_parse,
                                          css_image_value_query,
                                          css_image_value_assign,
                                          _gtk_css_image_value_new (NULL));
  gtk_css_style_property_register        ("border-image-repeat",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
                                          G_TYPE_NONE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          border_image_repeat_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_border_repeat_value_new (GTK_CSS_REPEAT_STYLE_STRETCH,
                                                                            GTK_CSS_REPEAT_STYLE_STRETCH));

  gtk_css_style_property_register        ("border-image-slice",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
                                          GTK_TYPE_BORDER,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          border_image_slice_parse,
                                          query_border,
                                          assign_border,
                                          _gtk_css_border_value_new (_gtk_css_number_value_new (100, GTK_CSS_PERCENT),
                                                                     _gtk_css_number_value_new (100, GTK_CSS_PERCENT),
                                                                     _gtk_css_number_value_new (100, GTK_CSS_PERCENT),
                                                                     _gtk_css_number_value_new (100, GTK_CSS_PERCENT)));
  gtk_css_style_property_register        ("border-image-width",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH,
                                          GTK_TYPE_BORDER,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          border_image_width_parse,
                                          query_border,
                                          assign_border,
                                          _gtk_css_border_value_new (_gtk_css_number_value_new (1, GTK_CSS_NUMBER),
                                                                     _gtk_css_number_value_new (1, GTK_CSS_NUMBER),
                                                                     _gtk_css_number_value_new (1, GTK_CSS_NUMBER),
                                                                     _gtk_css_number_value_new (1, GTK_CSS_NUMBER)));

  gtk_css_style_property_register        ("-gtk-icon-source",
                                          GTK_CSS_PROPERTY_ICON_SOURCE,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_ICON,
                                          css_image_value_parse_with_builtin,
                                          NULL,
                                          NULL,
                                          _gtk_css_image_value_new (gtk_css_image_builtin_new ()));
  gtk_css_style_property_register        ("icon-shadow",
                                          GTK_CSS_PROPERTY_ICON_SHADOW,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_ICON | GTK_CSS_AFFECTS_CLIP,
                                          shadow_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_shadows_value_new_none ());
  gtk_css_style_property_register        ("-gtk-icon-style",
                                          GTK_CSS_PROPERTY_ICON_STYLE,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_ICON,
                                          icon_style_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_icon_style_value_new (GTK_CSS_ICON_STYLE_REQUESTED));
  gtk_css_style_property_register        ("-gtk-icon-transform",
                                          GTK_CSS_PROPERTY_ICON_TRANSFORM,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_ICON | GTK_CSS_AFFECTS_CLIP,
                                          transform_value_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_transform_value_new_none ());

  gtk_css_style_property_register        ("transition-property",
                                          GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_property_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_ident_value_new ("all")));
  gtk_css_style_property_register        ("transition-duration",
                                          GTK_CSS_PROPERTY_TRANSITION_DURATION,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_time_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (0, GTK_CSS_S)));
  gtk_css_style_property_register        ("transition-timing-function",
                                          GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_timing_function_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (
                                            _gtk_css_ease_value_new_cubic_bezier (0.25, 0.1, 0.25, 1.0)));
  gtk_css_style_property_register        ("transition-delay",
                                          GTK_CSS_PROPERTY_TRANSITION_DELAY,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_time_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (0, GTK_CSS_S)));

  gtk_css_style_property_register        ("animation-name",
                                          GTK_CSS_PROPERTY_ANIMATION_NAME,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_property_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_ident_value_new ("none")));
  gtk_css_style_property_register        ("animation-duration",
                                          GTK_CSS_PROPERTY_ANIMATION_DURATION,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_time_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (0, GTK_CSS_S)));
  gtk_css_style_property_register        ("animation-timing-function",
                                          GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_timing_function_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (
                                            _gtk_css_ease_value_new_cubic_bezier (0.25, 0.1, 0.25, 1.0)));
  gtk_css_style_property_register        ("animation-iteration-count",
                                          GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          iteration_count_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (1, GTK_CSS_NUMBER)));
  gtk_css_style_property_register        ("animation-direction",
                                          GTK_CSS_PROPERTY_ANIMATION_DIRECTION,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          parse_css_direction,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_direction_value_new (GTK_CSS_DIRECTION_NORMAL)));
  gtk_css_style_property_register        ("animation-play-state",
                                          GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          parse_css_play_state,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_play_state_value_new (GTK_CSS_PLAY_STATE_RUNNING)));
  gtk_css_style_property_register        ("animation-delay",
                                          GTK_CSS_PROPERTY_ANIMATION_DELAY,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          transition_time_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_number_value_new (0, GTK_CSS_S)));
  gtk_css_style_property_register        ("animation-fill-mode",
                                          GTK_CSS_PROPERTY_ANIMATION_FILL_MODE,
                                          G_TYPE_NONE,
                                          0,
                                          0,
                                          parse_css_fill_mode,
                                          NULL,
                                          NULL,
                                          _gtk_css_array_value_new (_gtk_css_fill_mode_value_new (GTK_CSS_FILL_NONE)));

  gtk_css_style_property_register        ("opacity",
                                          GTK_CSS_PROPERTY_OPACITY,
                                          G_TYPE_NONE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          0,
                                          opacity_parse,
                                          NULL,
                                          NULL,
                                          _gtk_css_number_value_new (1, GTK_CSS_NUMBER));
  gtk_css_style_property_register        ("-gtk-image-effect",
					  GTK_CSS_PROPERTY_GTK_IMAGE_EFFECT,
					  G_TYPE_NONE,
					  GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_ICON,
					  image_effect_parse,
					  NULL,
					  NULL,
					  _gtk_css_image_effect_value_new (GTK_CSS_IMAGE_EFFECT_NONE));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_css_style_property_register        ("engine",
                                          GTK_CSS_PROPERTY_ENGINE,
                                          GTK_TYPE_THEMING_ENGINE,
                                          0,
                                          0,
                                          engine_parse,
                                          engine_query,
                                          engine_assign,
                                          _gtk_css_engine_value_new (gtk_theming_engine_load (NULL)));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* Private property holding the binding sets */
  gtk_css_style_property_register        ("gtk-key-bindings",
                                          GTK_CSS_PROPERTY_GTK_KEY_BINDINGS,
                                          G_TYPE_PTR_ARRAY,
                                          0,
                                          0,
                                          bindings_value_parse,
                                          bindings_value_query,
                                          bindings_value_assign,
                                          _gtk_css_array_value_new (_gtk_css_string_value_new (NULL)));
}

