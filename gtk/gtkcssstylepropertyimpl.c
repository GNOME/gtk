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
#include "gtkcssimageprivate.h"
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtksymboliccolorprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

/*** REGISTRATION ***/

static void
gtk_css_style_property_register (const char *                   name,
                                 GType                          specified_type,
                                 GType                          computed_type,
                                 GType                          value_type,
                                 GtkStylePropertyFlags          flags,
                                 GtkCssStylePropertyParseFunc   parse_value,
                                 GtkCssStylePropertyPrintFunc   print_value,
                                 GtkCssStylePropertyComputeFunc compute_value,
                                 ...)
{
  GtkCssStyleProperty *node;
  GValue initial_gvalue = G_VALUE_INIT;
  GtkCssValue *initial_value;
  char *error = NULL;
  va_list args;

  va_start (args, compute_value);
  G_VALUE_COLLECT_INIT (&initial_gvalue, specified_type,
                        args, 0, &error);
  if (error)
    {
      g_error ("property `%s' initial value is broken: %s", name, error);
      g_value_unset (&initial_gvalue);
      return;
    }

  va_end (args);

  initial_value = _gtk_css_value_new_take_gvalue (&initial_gvalue);

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "value-type", value_type,
                       "computed-type", computed_type,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", name,
                       NULL);
  
  if (parse_value)
    node->parse_value = parse_value;
  if (print_value)
    node->print_value = print_value;
  if (compute_value)
    node->compute_value = compute_value;

  _gtk_css_value_unref (initial_value);
}

/*** HELPERS ***/

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

static GtkCssValue *
color_compute (GtkCssStyleProperty    *property,
               GtkStyleContext        *context,
               GtkCssValue            *specified)
{
  GtkSymbolicColor *symbolic = _gtk_css_value_get_symbolic_color (specified);
  GtkCssValue *resolved;

  if (symbolic == _gtk_symbolic_color_get_current_color ())
    {
      /* The computed value of the ‘currentColor’ keyword is the computed
       * value of the ‘color’ property. If the ‘currentColor’ keyword is
       * set on the ‘color’ property itself, it is treated as ‘color: inherit’. 
       */
      if (g_str_equal (_gtk_style_property_get_name (GTK_STYLE_PROPERTY (property)), "color"))
        {
          GtkStyleContext *parent = gtk_style_context_get_parent (context);

          if (parent)
            return _gtk_css_value_ref (_gtk_style_context_peek_property (parent, "color"));
          else
            return _gtk_css_style_compute_value (context,
						 GDK_TYPE_RGBA,
						 _gtk_css_style_property_get_initial_value (property));
        }
      else
        {
          return _gtk_css_value_ref (_gtk_style_context_peek_property (context, "color"));
        }
    }
  else if ((resolved = _gtk_style_context_resolve_color_value (context,
							       symbolic)) != NULL)
    {
      return resolved;
    }
  else
    {
      return color_compute (property,
			    context,
			    _gtk_css_style_property_get_initial_value (property));
    }
}

static gboolean
font_family_parse (GtkCssStyleProperty *property,
                   GValue              *value,
                   GtkCssParser        *parser,
                   GFile               *base)
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
font_family_value_print (GtkCssStyleProperty *property,
                         const GValue        *value,
                         GString             *string)
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
bindings_value_parse (GtkCssStyleProperty *property,
                      GValue              *value,
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

  g_value_take_boxed (value, array);

  return TRUE;
}

static void
bindings_value_print (GtkCssStyleProperty *property,
                      const GValue        *value,
                      GString             *string)
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
border_corner_radius_value_parse (GtkCssStyleProperty *property,
                                  GValue              *value,
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

  g_value_set_boxed (value, &corner);
  return TRUE;
}

static void
border_corner_radius_value_print (GtkCssStyleProperty *property,
                                  const GValue        *value,
                                  GString             *string)
{
  GtkCssBorderCornerRadius *corner;

  corner = g_value_get_boxed (value);

  _gtk_css_number_print (&corner->horizontal, string);

  if (!_gtk_css_number_equal (&corner->horizontal, &corner->vertical))
    {
      g_string_append_c (string, ' ');
      _gtk_css_number_print (&corner->vertical, string);
    }
}

static gboolean 
css_image_value_parse (GtkCssStyleProperty *property,
                       GValue              *value,
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

  g_value_take_object (value, image);
  return TRUE;
}

static void
css_image_value_print (GtkCssStyleProperty *property,
                       const GValue        *value,
                       GString             *string)
{
  GtkCssImage *image = g_value_get_object (value);

  if (image)
    _gtk_css_image_print (image, string);
  else
    g_string_append (string, "none");
}

static GtkCssValue *
css_image_value_compute (GtkCssStyleProperty    *property,
                         GtkStyleContext        *context,
                         GtkCssValue            *specified)
{
  GtkCssImage *image, *computed;
  
  image = _gtk_css_value_get_image (specified);

  if (image == NULL)
    return _gtk_css_value_ref (specified);

  computed = _gtk_css_image_compute (image, context);

  if (computed == image)
    {
      g_object_unref (computed);
      return _gtk_css_value_ref (specified);
    }

  return _gtk_css_value_new_take_image (computed);
}

static gboolean 
parse_margin (GtkCssStyleProperty *property,
              GValue              *value,
              GtkCssParser        *parser,
              GFile               *base)
{
  GtkCssNumber number;

  if (!_gtk_css_parser_read_number (parser,
                                    &number, 
                                    GTK_CSS_NUMBER_AS_PIXELS
                                    | GTK_CSS_PARSE_LENGTH))
    return FALSE;

  g_value_set_boxed (value, &number);
  return TRUE;
}

static GtkCssValue *
compute_margin (GtkCssStyleProperty *property,
                GtkStyleContext     *context,
                GtkCssValue         *specified)
{
  GtkCssNumber number;
  
  if (_gtk_css_number_compute (&number,
			       _gtk_css_value_get_number (specified),
			       context))
    {
      return _gtk_css_value_new_from_number (&number);
    }
  return  _gtk_css_value_ref (specified);
}

static gboolean 
parse_padding (GtkCssStyleProperty *property,
               GValue              *value,
               GtkCssParser        *parser,
               GFile               *base)
{
  GtkCssNumber number;

  if (!_gtk_css_parser_read_number (parser,
                                    &number, 
                                    GTK_CSS_POSITIVE_ONLY
                                    | GTK_CSS_NUMBER_AS_PIXELS
                                    | GTK_CSS_PARSE_LENGTH))
    return FALSE;

  g_value_set_boxed (value, &number);
  return TRUE;
}

static GtkCssValue *
compute_padding (GtkCssStyleProperty *property,
                 GtkStyleContext     *context,
                 GtkCssValue         *specified)
{
  GtkCssNumber number;

  if (_gtk_css_number_compute (&number,
			       _gtk_css_value_get_number (specified),
			       context))
    return _gtk_css_value_new_from_number (&number);
  return _gtk_css_value_ref (specified);
}

static gboolean 
parse_border_width (GtkCssStyleProperty *property,
                    GValue              *value,
                    GtkCssParser        *parser,
                    GFile               *base)
{
  GtkCssNumber number;

  if (!_gtk_css_parser_read_number (parser,
                                    &number, 
                                    GTK_CSS_POSITIVE_ONLY
                                    | GTK_CSS_NUMBER_AS_PIXELS
                                    | GTK_CSS_PARSE_LENGTH))
    return FALSE;

  g_value_set_boxed (value, &number);
  return TRUE;
}

static GtkCssValue *
compute_border_width (GtkCssStyleProperty    *property,
                      GtkStyleContext        *context,
                      GtkCssValue            *specified)
{
  GtkCssStyleProperty *style;
  GtkBorderStyle border_style;
  GtkCssNumber number;
  int value = 0;
  
  /* The -1 is magic that is only true because we register the style
   * properties directly after the width properties.
   */
  style = _gtk_css_style_property_lookup_by_id (_gtk_css_style_property_get_id (property) - 1);
  
  border_style = _gtk_css_value_get_border_style (_gtk_style_context_peek_property (context, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (style))));

  if (border_style == GTK_BORDER_STYLE_NONE ||
      border_style == GTK_BORDER_STYLE_HIDDEN)
    {
      value = 0;
    }
  else
    {
      _gtk_css_number_compute (&number,
			       _gtk_css_value_get_number (specified),
			       context);
      value = round (number.value);
    }
  return _gtk_css_value_new_from_int (value);
}

static gboolean
background_repeat_value_parse (GtkCssStyleProperty *property,
                               GValue              *value,
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

  g_value_set_enum (value, repeat);
  return TRUE;
}

static void
background_repeat_value_print (GtkCssStyleProperty *property,
                               const GValue        *value,
                               GString             *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  GtkCssBackgroundRepeat repeat;

  repeat = g_value_get_enum (value);
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

static gboolean
background_size_parse (GtkCssStyleProperty *property,
                       GValue              *value,
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
        return FALSE;

      if (_gtk_css_parser_try (parser, "auto", TRUE))
        _gtk_css_number_init (&size.height, 0, GTK_CSS_PX);
      else if (_gtk_css_parser_has_number (parser))
        {
          if (!_gtk_css_parser_read_number (parser,
                                            &size.height,
                                            GTK_CSS_POSITIVE_ONLY
                                            | GTK_CSS_PARSE_PERCENT
                                            | GTK_CSS_PARSE_LENGTH))
            return FALSE;
        }
      else
        _gtk_css_number_init (&size.height, 0, GTK_CSS_PX);
    }

  g_value_set_boxed (value, &size);
  return TRUE;
}

static void
background_size_print (GtkCssStyleProperty *property,
                       const GValue        *value,
                       GString             *string)
{
  GtkCssBackgroundSize *size = g_value_get_boxed (value);

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
  GtkCssBackgroundSize *ssize = _gtk_css_value_get_background_size (specified);
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

static gboolean
background_position_parse (GtkCssStyleProperty *property,
			   GValue              *value,
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
	return FALSE;
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
              return FALSE;
            }
          if (!_gtk_css_parser_read_number (parser,
                                            missing,
                                            GTK_CSS_PARSE_PERCENT
                                            | GTK_CSS_PARSE_LENGTH))
	    return FALSE;
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
          return FALSE;
        }
    }

  g_value_set_boxed (value, &pos);
  return TRUE;
}

static void
background_position_print (GtkCssStyleProperty *property,
			   const GValue        *value,
			   GString             *string)
{
  GtkCssBackgroundPosition *pos = g_value_get_boxed (value);
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
  GtkCssBackgroundPosition *spos = _gtk_css_value_get_background_position (specified);
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
  char *default_font_family[] = { "Sans", NULL };
  GtkCssNumber number;
  GtkSymbolicColor *symbolic;
  GtkCssBackgroundSize default_background_size = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), FALSE, FALSE };
  GtkCssBackgroundPosition default_background_position = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PERCENT), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PERCENT)};
  GtkCssBorderCornerRadius no_corner_radius = { GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX), GTK_CSS_NUMBER_INIT (0, GTK_CSS_PX) };
  GtkBorder border_of_ones = { 1, 1, 1, 1 };
  GtkCssBorderImageRepeat border_image_repeat = { GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_STRETCH };

  /* Initialize "color" and "font-size" first,
   * so that when computing values later they are
   * done first. That way, 'currentColor' and font
   * sizes in em can be looked up properly */
  symbolic = gtk_symbolic_color_new_rgba (1, 1, 1, 1);
  gtk_css_style_property_register        ("color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          symbolic);
  gtk_symbolic_color_unref (symbolic);
  gtk_css_style_property_register        ("font-size",
                                          G_TYPE_DOUBLE,
                                          G_TYPE_DOUBLE,
                                          G_TYPE_DOUBLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          10.0);

  /* properties that aren't referenced when computing values
   * start here */
  symbolic = gtk_symbolic_color_new_rgba (0, 0, 0, 0);
  gtk_css_style_property_register        ("background-color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          symbolic);
  gtk_symbolic_color_unref (symbolic);

  gtk_css_style_property_register        ("font-family",
                                          G_TYPE_STRV,
                                          G_TYPE_STRV,
                                          G_TYPE_STRV,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          font_family_parse,
                                          font_family_value_print,
                                          NULL,
                                          default_font_family);
  gtk_css_style_property_register        ("font-style",
                                          PANGO_TYPE_STYLE,
                                          PANGO_TYPE_STYLE,
                                          PANGO_TYPE_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          PANGO_STYLE_NORMAL);
  gtk_css_style_property_register        ("font-variant",
                                          PANGO_TYPE_VARIANT,
                                          PANGO_TYPE_VARIANT,
                                          PANGO_TYPE_VARIANT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          PANGO_VARIANT_NORMAL);
  /* xxx: need to parse this properly, ie parse the numbers */
  gtk_css_style_property_register        ("font-weight",
                                          PANGO_TYPE_WEIGHT,
                                          PANGO_TYPE_WEIGHT,
                                          PANGO_TYPE_WEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          PANGO_WEIGHT_NORMAL);

  gtk_css_style_property_register        ("text-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_TYPE_SHADOW,
                                          GTK_TYPE_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_css_style_property_register        ("icon-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_TYPE_SHADOW,
                                          GTK_TYPE_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_css_style_property_register        ("box-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_TYPE_SHADOW,
                                          GTK_TYPE_SHADOW,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  _gtk_css_number_init (&number, 0, GTK_CSS_PX);
  gtk_css_style_property_register        ("margin-top",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          &number);
  gtk_css_style_property_register        ("margin-left",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          &number);
  gtk_css_style_property_register        ("margin-bottom",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          &number);
  gtk_css_style_property_register        ("margin-right",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_margin,
                                          NULL,
                                          compute_margin,
                                          &number);
  gtk_css_style_property_register        ("padding-top",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          &number);
  gtk_css_style_property_register        ("padding-left",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          &number);
  gtk_css_style_property_register        ("padding-bottom",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          &number);
  gtk_css_style_property_register        ("padding-right",
                                          GTK_TYPE_CSS_NUMBER,
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          0,
                                          parse_padding,
                                          NULL,
                                          compute_padding,
                                          &number);
  /* IMPORTANT: compute_border_width() requires that the border-width
   * properties be immeditaly followed by the border-style properties
   */
  gtk_css_style_property_register        ("border-top-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_css_style_property_register        ("border-top-width",
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          0,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          &number);
  gtk_css_style_property_register        ("border-left-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_css_style_property_register        ("border-left-width",
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          0,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          &number);
  gtk_css_style_property_register        ("border-bottom-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_css_style_property_register        ("border-bottom-width",
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          0,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          &number);
  gtk_css_style_property_register        ("border-right-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_css_style_property_register        ("border-right-width",
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          0,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          &number);

  gtk_css_style_property_register        ("border-top-left-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);
  gtk_css_style_property_register        ("border-top-right-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);
  gtk_css_style_property_register        ("border-bottom-right-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);
  gtk_css_style_property_register        ("border-bottom-left-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);

  gtk_css_style_property_register        ("outline-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_css_style_property_register        ("outline-width",
                                          GTK_TYPE_CSS_NUMBER,
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          0,
                                          parse_border_width,
                                          NULL,
                                          compute_border_width,
                                          &number);
  gtk_css_style_property_register        ("outline-offset",
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);

  gtk_css_style_property_register        ("background-clip",
                                          GTK_TYPE_CSS_AREA,
                                          GTK_TYPE_CSS_AREA,
                                          GTK_TYPE_CSS_AREA,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_CSS_AREA_BORDER_BOX);
  gtk_css_style_property_register        ("background-origin",
                                          GTK_TYPE_CSS_AREA,
                                          GTK_TYPE_CSS_AREA,
                                          GTK_TYPE_CSS_AREA,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_CSS_AREA_PADDING_BOX);
  gtk_css_style_property_register        ("background-size",
                                          GTK_TYPE_CSS_BACKGROUND_SIZE,
                                          GTK_TYPE_CSS_BACKGROUND_SIZE,
                                          G_TYPE_NONE,
                                          0,
                                          background_size_parse,
                                          background_size_print,
                                          background_size_compute,
                                          &default_background_size);
  gtk_css_style_property_register        ("background-position",
                                          GTK_TYPE_CSS_BACKGROUND_POSITION,
                                          GTK_TYPE_CSS_BACKGROUND_POSITION,
                                          G_TYPE_NONE,
                                          0,
                                          background_position_parse,
                                          background_position_print,
                                          background_position_compute,
                                          &default_background_position);

  gtk_css_style_property_register        ("border-top-color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          _gtk_symbolic_color_get_current_color ());
  gtk_css_style_property_register        ("border-right-color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          _gtk_symbolic_color_get_current_color ());
  gtk_css_style_property_register        ("border-bottom-color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          _gtk_symbolic_color_get_current_color ());
  gtk_css_style_property_register        ("border-left-color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          _gtk_symbolic_color_get_current_color ());
  gtk_css_style_property_register        ("outline-color",
                                          GTK_TYPE_SYMBOLIC_COLOR,
                                          GDK_TYPE_RGBA,
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          _gtk_symbolic_color_get_current_color ());

  gtk_css_style_property_register        ("background-repeat",
                                          GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                          GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                          GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                          0,
                                          background_repeat_value_parse,
                                          background_repeat_value_print,
                                          NULL,
                                          GTK_CSS_BACKGROUND_REPEAT | (GTK_CSS_BACKGROUND_REPEAT << GTK_CSS_BACKGROUND_REPEAT_SHIFT));
  gtk_css_style_property_register        ("background-image",
                                          GTK_TYPE_CSS_IMAGE,
                                          GTK_TYPE_CSS_IMAGE,
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          0,
                                          css_image_value_parse,
                                          css_image_value_print,
                                          css_image_value_compute,
                                          NULL);

  gtk_css_style_property_register        ("border-image-source",
                                          GTK_TYPE_CSS_IMAGE,
                                          GTK_TYPE_CSS_IMAGE,
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          0,
                                          css_image_value_parse,
                                          css_image_value_print,
                                          css_image_value_compute,
                                          NULL);
  gtk_css_style_property_register        ("border-image-repeat",
                                          GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                          GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                          GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &border_image_repeat);

  /* XXX: The initial value is wrong, it should be 100% */
  gtk_css_style_property_register        ("border-image-slice",
                                          GTK_TYPE_BORDER,
                                          GTK_TYPE_BORDER,
                                          GTK_TYPE_BORDER,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &border_of_ones);
  gtk_css_style_property_register        ("border-image-width",
                                          GTK_TYPE_BORDER,
                                          GTK_TYPE_BORDER,
                                          GTK_TYPE_BORDER,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  gtk_css_style_property_register        ("engine",
                                          GTK_TYPE_THEMING_ENGINE,
                                          GTK_TYPE_THEMING_ENGINE,
                                          GTK_TYPE_THEMING_ENGINE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          gtk_theming_engine_load (NULL));
  gtk_css_style_property_register        ("transition",
                                          GTK_TYPE_ANIMATION_DESCRIPTION,
                                          GTK_TYPE_ANIMATION_DESCRIPTION,
                                          GTK_TYPE_ANIMATION_DESCRIPTION,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  /* Private property holding the binding sets */
  gtk_css_style_property_register        ("gtk-key-bindings",
                                          G_TYPE_PTR_ARRAY,
                                          G_TYPE_PTR_ARRAY,
                                          G_TYPE_PTR_ARRAY,
                                          0,
                                          bindings_value_parse,
                                          bindings_value_print,
                                          NULL,
                                          NULL);
}

