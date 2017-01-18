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

#include "gtkcssstylefuncsprivate.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssprovider.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkcsswin32sizevalueprivate.h"


/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

static GHashTable *parse_funcs = NULL;
static GHashTable *print_funcs = NULL;

typedef gboolean         (* GtkStyleParseFunc)             (GtkCssParser            *parser,
                                                            GValue                  *value);
typedef void             (* GtkStylePrintFunc)             (const GValue            *value,
                                                            GString                 *string);

static void
register_conversion_function (GType               type,
                              GtkStyleParseFunc   parse,
                              GtkStylePrintFunc   print)
{
  if (parse)
    g_hash_table_insert (parse_funcs, GSIZE_TO_POINTER (type), parse);
  if (print)
    g_hash_table_insert (print_funcs, GSIZE_TO_POINTER (type), print);
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
  _gtk_css_print_string (str, string);
}

/*** IMPLEMENTATIONS ***/

static gboolean 
enum_parse (GtkCssParser *parser,
	    GType         type,
	    int          *res)
{
  char *str;

  if (_gtk_css_parser_try_enum (parser, type, res))
    return TRUE;

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected an identifier");
      return FALSE;
    }

  _gtk_css_parser_error (parser,
			 "Unknown value '%s' for enum type '%s'",
			 str, g_type_name (type));
  g_free (str);

  return FALSE;
}

static void
enum_print (int         value,
	    GType       type,
	    GString    *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_nick);

  g_type_class_unref (enum_class);
}

static gboolean 
font_description_value_parse (GtkCssParser *parser,
                              GValue       *value)
{
  PangoFontDescription *font_desc;
  guint mask;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  font_desc = pango_font_description_from_string (str);
  mask = pango_font_description_get_set_fields (font_desc);
  /* These values are not really correct,
   * but the fields must be set, so we set them to something */
  if ((mask & PANGO_FONT_MASK_FAMILY) == 0)
    pango_font_description_set_family_static (font_desc, "Sans");
  if ((mask & PANGO_FONT_MASK_SIZE) == 0)
    pango_font_description_set_size (font_desc, 10 * PANGO_SCALE);
  g_free (str);
  g_value_take_boxed (value, font_desc);
  return TRUE;
}

static void
font_description_value_print (const GValue *value,
                              GString      *string)
{
  const PangoFontDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = pango_font_description_to_string (desc);
      g_string_append (string, s);
      g_free (s);
    }
}

static gboolean 
boolean_value_parse (GtkCssParser *parser,
                     GValue       *value)
{
  if (_gtk_css_parser_try (parser, "true", TRUE) ||
      _gtk_css_parser_try (parser, "1", TRUE))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (_gtk_css_parser_try (parser, "false", TRUE) ||
           _gtk_css_parser_try (parser, "0", TRUE))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else
    {
      _gtk_css_parser_error (parser, "Expected a boolean value");
      return FALSE;
    }
}

static void
boolean_value_print (const GValue *value,
                     GString      *string)
{
  if (g_value_get_boolean (value))
    g_string_append (string, "true");
  else
    g_string_append (string, "false");
}

static gboolean 
int_value_parse (GtkCssParser *parser,
                 GValue       *value)
{
  gint i;

  if (_gtk_css_parser_has_prefix (parser, "-gtk"))
    {
      GtkCssValue *cssvalue = gtk_css_win32_size_value_parse (parser, GTK_CSS_PARSE_NUMBER);

      if (cssvalue)
        {
          g_value_set_int (value, _gtk_css_number_value_get (cssvalue, 100));
          _gtk_css_value_unref (cssvalue);
          return TRUE;
        }

      return FALSE;
    }

  if (!_gtk_css_parser_try_int (parser, &i))
    {
      _gtk_css_parser_error (parser, "Expected a valid integer value");
      return FALSE;
    }

  g_value_set_int (value, i);
  return TRUE;
}

static void
int_value_print (const GValue *value,
                 GString      *string)
{
  g_string_append_printf (string, "%d", g_value_get_int (value));
}

static gboolean 
uint_value_parse (GtkCssParser *parser,
                  GValue       *value)
{
  guint u;

  if (!_gtk_css_parser_try_uint (parser, &u))
    {
      _gtk_css_parser_error (parser, "Expected a valid unsigned value");
      return FALSE;
    }

  g_value_set_uint (value, u);
  return TRUE;
}

static void
uint_value_print (const GValue *value,
                  GString      *string)
{
  g_string_append_printf (string, "%u", g_value_get_uint (value));
}

static gboolean 
double_value_parse (GtkCssParser *parser,
                    GValue       *value)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  g_value_set_double (value, d);
  return TRUE;
}

static void
double_value_print (const GValue *value,
                    GString      *string)
{
  string_append_double (string, g_value_get_double (value));
}

static gboolean 
float_value_parse (GtkCssParser *parser,
                   GValue       *value)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  g_value_set_float (value, d);
  return TRUE;
}

static void
float_value_print (const GValue *value,
                   GString      *string)
{
  string_append_double (string, g_value_get_float (value));
}

static gboolean 
string_value_parse (GtkCssParser *parser,
                    GValue       *value)
{
  char *str = _gtk_css_parser_read_string (parser);

  if (str == NULL)
    return FALSE;

  g_value_take_string (value, str);
  return TRUE;
}

static void
string_value_print (const GValue *value,
                    GString      *str)
{
  string_append_string (str, g_value_get_string (value));
}

static gboolean 
border_value_parse (GtkCssParser *parser,
                    GValue       *value)
{
  GtkBorder border = { 0, };
  guint i;
  int numbers[4];

  for (i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      if (_gtk_css_parser_has_prefix (parser, "-gtk"))
        {
          GtkCssValue *cssvalue = gtk_css_win32_size_value_parse (parser, GTK_CSS_PARSE_NUMBER);

          if (cssvalue)
            {
              numbers[i] = _gtk_css_number_value_get (cssvalue, 100);
              _gtk_css_value_unref (cssvalue);
              return TRUE;
            }

          return FALSE;
        }
      else
        {
          if (!_gtk_css_parser_try_length (parser, &numbers[i]))
            break;
        }
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected valid border");
      return FALSE;
    }

  border.top = numbers[0];
  if (i > 1)
    border.right = numbers[1];
  else
    border.right = border.top;
  if (i > 2)
    border.bottom = numbers[2];
  else
    border.bottom = border.top;
  if (i > 3)
    border.left = numbers[3];
  else
    border.left = border.right;

  g_value_set_boxed (value, &border);
  return TRUE;
}

static void
border_value_print (const GValue *value, GString *string)
{
  const GtkBorder *border = g_value_get_boxed (value);

  if (border == NULL)
    g_string_append (string, "none");
  else if (border->left != border->right)
    g_string_append_printf (string, "%d %d %d %d", border->top, border->right, border->bottom, border->left);
  else if (border->top != border->bottom)
    g_string_append_printf (string, "%d %d %d", border->top, border->right, border->bottom);
  else if (border->top != border->left)
    g_string_append_printf (string, "%d %d", border->top, border->right);
  else
    g_string_append_printf (string, "%d", border->top);
}

static gboolean 
pattern_value_parse (GtkCssParser *parser,
                     GValue       *value)
{
  if (_gtk_css_parser_try (parser, "none", TRUE))
    {
      /* nothing to do here */
    }
  else
    {
      GError *error = NULL;
      gchar *path;
      GdkPixbuf *pixbuf;
      GFile *file;
      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_matrix_t matrix;

      file = _gtk_css_parser_read_url (parser);
      if (file == NULL)
        return FALSE;

      path = g_file_get_path (file);
      g_object_unref (file);

      pixbuf = gdk_pixbuf_new_from_file (path, &error);
      g_free (path);
      if (pixbuf == NULL)
        {
          _gtk_css_parser_take_error (parser, error);
          return FALSE;
        }

      surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);
      pattern = cairo_pattern_create_for_surface (surface);
      cairo_surface_destroy (surface);

      cairo_matrix_init_scale (&matrix,
                               gdk_pixbuf_get_width (pixbuf),
                               gdk_pixbuf_get_height (pixbuf));
      cairo_pattern_set_matrix (pattern, &matrix);

      g_object_unref (pixbuf);

      g_value_take_boxed (value, pattern);
    }
  
  return TRUE;
}

static cairo_status_t
surface_write (void                *closure,
               const unsigned char *data,
               unsigned int         length)
{
  g_byte_array_append (closure, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
surface_print (cairo_surface_t *surface,
               GString *        string)
{
#if CAIRO_HAS_PNG_FUNCTIONS
  GByteArray *array;
  char *base64;
  
  array = g_byte_array_new ();
  cairo_surface_write_to_png_stream (surface, surface_write, array);
  base64 = g_base64_encode (array->data, array->len);
  g_byte_array_free (array, TRUE);

  g_string_append (string, "url(\"data:image/png;base64,");
  g_string_append (string, base64);
  g_string_append (string, "\")");

  g_free (base64);
#else
  g_string_append (string, "none /* you need cairo png functions enabled to make this work */");
#endif
}

static void
pattern_value_print (const GValue *value,
                     GString      *string)
{
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;

  pattern = g_value_get_boxed (value);

  if (pattern == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  switch (cairo_pattern_get_type (pattern))
    {
    case CAIRO_PATTERN_TYPE_SURFACE:
      if (cairo_pattern_get_surface (pattern, &surface) != CAIRO_STATUS_SUCCESS)
        {
          g_assert_not_reached ();
        }
      surface_print (surface, string);
      break;
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
      g_string_append (string, "none /* FIXME: add support for printing gradients */");
      break;
    case CAIRO_PATTERN_TYPE_SOLID:
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean 
enum_value_parse (GtkCssParser *parser,
                  GValue       *value)
{
  int v;

  if (enum_parse (parser, G_VALUE_TYPE (value), &v))
    {
      g_value_set_enum (value, v);
      return TRUE;
    }

  return FALSE;
}

static void
enum_value_print (const GValue *value,
                  GString      *string)
{
  enum_print (g_value_get_enum (value), G_VALUE_TYPE (value), string);
}

static gboolean 
flags_value_parse (GtkCssParser *parser,
                   GValue       *value)
{
  GFlagsClass *flags_class;
  GFlagsValue *flag_value;
  guint flags = 0;
  char *str;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));

  do {
    str = _gtk_css_parser_try_ident (parser, TRUE);
    if (str == NULL)
      {
        _gtk_css_parser_error (parser, "Expected an identifier");
        g_type_class_unref (flags_class);
        return FALSE;
      }

      flag_value = g_flags_get_value_by_nick (flags_class, str);
      if (!flag_value)
        {
          _gtk_css_parser_error (parser,
                                 "Unknown flag value '%s' for type '%s'",
                                 str, g_type_name (G_VALUE_TYPE (value)));
          /* XXX Do we want to return FALSE here? We can get
           * forward-compatibility for new values this way
           */
          g_free (str);
          g_type_class_unref (flags_class);
          return FALSE;
        }

      g_free (str);
    }
  while (_gtk_css_parser_try (parser, ",", FALSE));

  g_type_class_unref (flags_class);

  g_value_set_enum (value, flags);

  return TRUE;
}

static void
flags_value_print (const GValue *value,
                   GString      *string)
{
  GFlagsClass *flags_class;
  guint i, flags;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));
  flags = g_value_get_flags (value);

  for (i = 0; i < flags_class->n_values; i++)
    {
      GFlagsValue *flags_value = &flags_class->values[i];

      if (flags & flags_value->value)
        {
          if (string->len != 0)
            g_string_append (string, ", ");

          g_string_append (string, flags_value->value_nick);
        }
    }

  g_type_class_unref (flags_class);
}

/*** API ***/

static void
gtk_css_style_funcs_init (void)
{
  if (G_LIKELY (parse_funcs != NULL))
    return;

  parse_funcs = g_hash_table_new (NULL, NULL);
  print_funcs = g_hash_table_new (NULL, NULL);

  register_conversion_function (PANGO_TYPE_FONT_DESCRIPTION,
                                font_description_value_parse,
                                font_description_value_print);
  register_conversion_function (G_TYPE_BOOLEAN,
                                boolean_value_parse,
                                boolean_value_print);
  register_conversion_function (G_TYPE_INT,
                                int_value_parse,
                                int_value_print);
  register_conversion_function (G_TYPE_UINT,
                                uint_value_parse,
                                uint_value_print);
  register_conversion_function (G_TYPE_DOUBLE,
                                double_value_parse,
                                double_value_print);
  register_conversion_function (G_TYPE_FLOAT,
                                float_value_parse,
                                float_value_print);
  register_conversion_function (G_TYPE_STRING,
                                string_value_parse,
                                string_value_print);
  register_conversion_function (GTK_TYPE_BORDER,
                                border_value_parse,
                                border_value_print);
  register_conversion_function (CAIRO_GOBJECT_TYPE_PATTERN,
                                pattern_value_parse,
                                pattern_value_print);
  register_conversion_function (G_TYPE_ENUM,
                                enum_value_parse,
                                enum_value_print);
  register_conversion_function (G_TYPE_FLAGS,
                                flags_value_parse,
                                flags_value_print);
}

/**
 * _gtk_css_style_parse_value:
 * @value: the value to parse into. Must be a valid initialized #GValue
 * @parser: the parser to parse from
 *
 * This is the generic parsing function used for CSS values. If the
 * function fails to parse a value, it will emit an error on @parser,
 * return %FALSE and not touch @value.
 *
 * Returns: %TRUE if parsing succeeded.
 **/
gboolean
_gtk_css_style_funcs_parse_value (GValue       *value,
                                  GtkCssParser *parser)
{
  GtkStyleParseFunc func;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (parser != NULL, FALSE);

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (parse_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (parse_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      _gtk_css_parser_error (parser,
                             "Cannot convert to type '%s'",
                             g_type_name (G_VALUE_TYPE (value)));
      return FALSE;
    }

  return (*func) (parser, value);
}

/**
 * _gtk_css_style_print_value:
 * @value: an initialized GValue returned from _gtk_css_style_parse()
 * @string: the string to print into
 *
 * Prints @value into @string as a CSS value. If @value is not a
 * valid value, a random string will be printed instead.
 **/
void
_gtk_css_style_funcs_print_value (const GValue *value,
                                  GString      *string)
{
  GtkStylePrintFunc func;

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (print_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (print_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      char *s = g_strdup_value_contents (value);
      g_string_append (string, s);
      g_free (s);
      return;
    }
  
  func (value, string);
}

