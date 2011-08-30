/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssparserprivate.h"

#include <errno.h>
#include <string.h>

/* just for the errors, yay! */
#include "gtkcssprovider.h"

#define NEWLINE_CHARS "\r\n"
#define WHITESPACE_CHARS "\f \t"
#define NMSTART "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define NMCHAR NMSTART "01234567890-_"
#define URLCHAR NMCHAR "!#$%&*~"

#define GTK_IS_CSS_PARSER(parser) ((parser) != NULL)

struct _GtkCssParser
{
  const char *data;
  GtkCssParserErrorFunc error_func;
  gpointer              user_data;

  const char *line_start;
  guint line;
};

GtkCssParser *
_gtk_css_parser_new (const char            *data,
                     GtkCssParserErrorFunc  error_func,
                     gpointer               user_data)
{
  GtkCssParser *parser;

  g_return_val_if_fail (data != NULL, NULL);

  parser = g_slice_new0 (GtkCssParser);

  parser->data = data;
  parser->error_func = error_func;
  parser->user_data = user_data;

  parser->line_start = data;
  parser->line = 0;

  return parser;
}

void
_gtk_css_parser_free (GtkCssParser *parser)
{
  g_return_if_fail (GTK_IS_CSS_PARSER (parser));

  g_slice_free (GtkCssParser, parser);
}

gboolean
_gtk_css_parser_is_eof (GtkCssParser *parser)
{
  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), TRUE);

  return *parser->data == 0;
}

gboolean
_gtk_css_parser_begins_with (GtkCssParser *parser,
                             char          c)
{
  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), TRUE);

  return *parser->data == c;
}

guint
_gtk_css_parser_get_line (GtkCssParser *parser)
{
  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), 1);

  return parser->line;
}

guint
_gtk_css_parser_get_position (GtkCssParser *parser)
{
  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), 1);

  return parser->data - parser->line_start;
}

void
_gtk_css_parser_take_error (GtkCssParser *parser,
                            GError       *error)
{
  parser->error_func (parser, error, parser->user_data);

  g_error_free (error);
}

void
_gtk_css_parser_error (GtkCssParser *parser,
                       const char   *format,
                       ...)
{
  GError *error;

  va_list args;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_SYNTAX,
                              format, args);
  va_end (args);

  _gtk_css_parser_take_error (parser, error);
}

static gboolean
gtk_css_parser_new_line (GtkCssParser *parser)
{
  gboolean result = FALSE;

  if (*parser->data == '\r')
    {
      result = TRUE;
      parser->data++;
    }
  if (*parser->data == '\n')
    {
      result = TRUE;
      parser->data++;
    }

  if (result)
    {
      parser->line++;
      parser->line_start = parser->data;
    }

  return result;
}

static gboolean
gtk_css_parser_skip_comment (GtkCssParser *parser)
{
  if (parser->data[0] != '/' ||
      parser->data[1] != '*')
    return FALSE;

  parser->data += 2;

  while (*parser->data)
    {
      gsize len = strcspn (parser->data, NEWLINE_CHARS "/");

      parser->data += len;
  
      if (gtk_css_parser_new_line (parser))
        continue;

      parser->data++;

      if (parser->data[-2] == '*')
        return TRUE;
      if (parser->data[0] == '*')
        _gtk_css_parser_error (parser, "'/*' in comment block");
    }

  /* FIXME: position */
  _gtk_css_parser_error (parser, "Unterminated comment");
  return TRUE;
}

void
_gtk_css_parser_skip_whitespace (GtkCssParser *parser)
{
  size_t len;

  while (*parser->data)
    {
      if (gtk_css_parser_new_line (parser))
        continue;

      len = strspn (parser->data, WHITESPACE_CHARS);
      if (len)
        {
          parser->data += len;
          continue;
        }
      
      if (!gtk_css_parser_skip_comment (parser))
        break;
    }
}

gboolean
_gtk_css_parser_try (GtkCssParser *parser,
                     const char   *string,
                     gboolean      skip_whitespace)
{
  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  if (g_ascii_strncasecmp (parser->data, string, strlen (string)) != 0)
    return FALSE;

  parser->data += strlen (string);

  if (skip_whitespace)
    _gtk_css_parser_skip_whitespace (parser);
  return TRUE;
}

static guint
get_xdigit (char c)
{
  if (c >= 'a')
    return c - 'a' + 10;
  else if (c >= 'A')
    return c - 'A' + 10;
  else
    return c - '0';
}

static void
_gtk_css_parser_unescape (GtkCssParser *parser,
                          GString      *str)
{
  guint i;
  gunichar result = 0;

  g_assert (*parser->data == '\\');

  parser->data++;

  for (i = 0; i < 6; i++)
    {
      if (!g_ascii_isxdigit (parser->data[i]))
        break;

      result = (result << 4) + get_xdigit (parser->data[i]);
    }

  if (i != 0)
    {
      g_string_append_unichar (str, result);
      parser->data += i;

      /* NB: gtk_css_parser_new_line() forward data pointer itself */
      if (!gtk_css_parser_new_line (parser) &&
          *parser->data &&
          strchr (WHITESPACE_CHARS, *parser->data))
        parser->data++;
      return;
    }

  if (gtk_css_parser_new_line (parser))
    return;

  g_string_append_c (str, *parser->data);
  parser->data++;

  return;
}

static gboolean
_gtk_css_parser_read_char (GtkCssParser *parser,
                           GString *     str,
                           const char *  allowed)
{
  if (*parser->data == 0)
    return FALSE;

  if (strchr (allowed, *parser->data))
    {
      g_string_append_c (str, *parser->data);
      parser->data++;
      return TRUE;
    }
  if (*parser->data >= 127)
    {
      gsize len = g_utf8_skip[(guint) *(guchar *) parser->data];

      g_string_append_len (str, parser->data, len);
      parser->data += len;
      return TRUE;
    }
  if (*parser->data == '\\')
    {
      _gtk_css_parser_unescape (parser, str);
      return TRUE;
    }

  return FALSE;
}

char *
_gtk_css_parser_try_name (GtkCssParser *parser,
                          gboolean      skip_whitespace)
{
  GString *name;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), NULL);

  name = g_string_new (NULL);

  while (_gtk_css_parser_read_char (parser, name, NMCHAR))
    ;

  if (skip_whitespace)
    _gtk_css_parser_skip_whitespace (parser);

  return g_string_free (name, FALSE);
}

char *
_gtk_css_parser_try_ident (GtkCssParser *parser,
                           gboolean      skip_whitespace)
{
  const char *start;
  GString *ident;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), NULL);

  start = parser->data;
  
  ident = g_string_new (NULL);

  if (*parser->data == '-')
    {
      g_string_append_c (ident, '-');
      parser->data++;
    }

  if (!_gtk_css_parser_read_char (parser, ident, NMSTART))
    {
      parser->data = start;
      g_string_free (ident, TRUE);
      return NULL;
    }

  while (_gtk_css_parser_read_char (parser, ident, NMCHAR))
    ;

  if (skip_whitespace)
    _gtk_css_parser_skip_whitespace (parser);

  return g_string_free (ident, FALSE);
}

gboolean
_gtk_css_parser_is_string (GtkCssParser *parser)
{
  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), FALSE);

  return *parser->data == '"' || *parser->data == '\'';
}

char *
_gtk_css_parser_read_string (GtkCssParser *parser)
{
  GString *str;
  char quote;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), NULL);

  quote = *parser->data;
  
  if (quote != '"' && quote != '\'')
    {
      _gtk_css_parser_error (parser, "Expected a string.");
      return NULL;
    }
  
  parser->data++;
  str = g_string_new (NULL);

  while (TRUE)
    {
      gsize len = strcspn (parser->data, "\\'\"\n\r\f");

      g_string_append_len (str, parser->data, len);

      parser->data += len;

      switch (*parser->data)
        {
        case '\\':
          _gtk_css_parser_unescape (parser, str);
          break;
        case '"':
        case '\'':
          if (*parser->data == quote)
            {
              parser->data++;
              _gtk_css_parser_skip_whitespace (parser);
              return g_string_free (str, FALSE);
            }
          
          g_string_append_c (str, *parser->data);
          parser->data++;
          break;
        case '\0':
          /* FIXME: position */
          _gtk_css_parser_error (parser, "Missing end quote in string.");
          g_string_free (str, TRUE);
          return NULL;
        default:
          _gtk_css_parser_error (parser, 
                                 "Invalid character in string. Must be escaped.");
          g_string_free (str, TRUE);
          return NULL;
        }
    }

  g_assert_not_reached ();
  return NULL;
}

char *
_gtk_css_parser_read_uri (GtkCssParser *parser)
{
  char *result;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), NULL);

  if (!_gtk_css_parser_try (parser, "url(", TRUE))
    {
      _gtk_css_parser_error (parser, "expected 'url('");
      return NULL;
    }

  _gtk_css_parser_skip_whitespace (parser);

  if (_gtk_css_parser_is_string (parser))
    {
      result = _gtk_css_parser_read_string (parser);
    }
  else
    {
      GString *str = g_string_new (NULL);

      while (_gtk_css_parser_read_char (parser, str, URLCHAR))
        ;
      result = g_string_free (str, FALSE);
      if (result == NULL)
        _gtk_css_parser_error (parser, "not a url");
    }
  
  if (result == NULL)
    return NULL;

  _gtk_css_parser_skip_whitespace (parser);

  if (*parser->data != ')')
    {
      _gtk_css_parser_error (parser, "missing ')' for url");
      g_free (result);
      return NULL;
    }

  parser->data++;

  _gtk_css_parser_skip_whitespace (parser);

  return result;
}

gboolean
_gtk_css_parser_try_int (GtkCssParser *parser,
                         int          *value)
{
  gint64 result;
  char *end;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  /* strtoll parses a plus, but we are not allowed to */
  if (*parser->data == '+')
    return FALSE;

  errno = 0;
  result = g_ascii_strtoll (parser->data, &end, 10);
  if (errno)
    return FALSE;
  if (result > G_MAXINT || result < G_MININT)
    return FALSE;
  if (parser->data == end)
    return FALSE;

  parser->data = end;
  *value = result;

  _gtk_css_parser_skip_whitespace (parser);

  return TRUE;
}

gboolean
_gtk_css_parser_try_uint (GtkCssParser *parser,
                          guint        *value)
{
  guint64 result;
  char *end;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  errno = 0;
  result = g_ascii_strtoull (parser->data, &end, 10);
  if (errno)
    return FALSE;
  if (result > G_MAXUINT)
    return FALSE;
  if (parser->data == end)
    return FALSE;

  parser->data = end;
  *value = result;

  _gtk_css_parser_skip_whitespace (parser);

  return TRUE;
}

gboolean
_gtk_css_parser_try_double (GtkCssParser *parser,
                            gdouble      *value)
{
  gdouble result;
  char *end;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  errno = 0;
  result = g_ascii_strtod (parser->data, &end);
  if (errno)
    return FALSE;
  if (parser->data == end)
    return FALSE;

  parser->data = end;
  *value = result;

  _gtk_css_parser_skip_whitespace (parser);

  return TRUE;
}

typedef enum {
  COLOR_RGBA,
  COLOR_RGB,
  COLOR_LIGHTER,
  COLOR_DARKER,
  COLOR_SHADE,
  COLOR_ALPHA,
  COLOR_MIX
} ColorType;

static GtkSymbolicColor *
gtk_css_parser_read_symbolic_color_function (GtkCssParser *parser,
                                             ColorType     color)
{
  GtkSymbolicColor *symbolic;
  GtkSymbolicColor *child1, *child2;
  double value;

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing opening bracket in color definition");
      return NULL;
    }

  if (color == COLOR_RGB || color == COLOR_RGBA)
    {
      GdkRGBA rgba;
      double tmp;
      guint i;

      for (i = 0; i < 3; i++)
        {
          if (i > 0 && !_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &tmp))
            {
              _gtk_css_parser_error (parser, "Invalid number for color value");
              return NULL;
            }
          if (_gtk_css_parser_try (parser, "%", TRUE))
            tmp /= 100.0;
          else
            tmp /= 255.0;
          if (i == 0)
            rgba.red = tmp;
          else if (i == 1)
            rgba.green = tmp;
          else if (i == 2)
            rgba.blue = tmp;
          else
            g_assert_not_reached ();
        }

      if (color == COLOR_RGBA)
        {
          if (i > 0 && !_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &rgba.alpha))
            {
              _gtk_css_parser_error (parser, "Invalid number for alpha value");
              return NULL;
            }
        }
      else
        rgba.alpha = 1.0;
      
      symbolic = gtk_symbolic_color_new_literal (&rgba);
    }
  else
    {
      child1 = _gtk_css_parser_read_symbolic_color (parser);
      if (child1 == NULL)
        return NULL;

      if (color == COLOR_MIX)
        {
          if (!_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              gtk_symbolic_color_unref (child1);
              return NULL;
            }

          child2 = _gtk_css_parser_read_symbolic_color (parser);
          if (child2 == NULL)
            {
              gtk_symbolic_color_unref (child1);
              return NULL;
            }
        }
      else
        child2 = NULL;

      if (color == COLOR_LIGHTER)
        value = 1.3;
      else if (color == COLOR_DARKER)
        value = 0.7;
      else
        {
          if (!_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              gtk_symbolic_color_unref (child1);
              if (child2)
                gtk_symbolic_color_unref (child2);
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &value))
            {
              _gtk_css_parser_error (parser, "Expected number in color definition");
              gtk_symbolic_color_unref (child1);
              if (child2)
                gtk_symbolic_color_unref (child2);
              return NULL;
            }
        }
      
      switch (color)
        {
        case COLOR_LIGHTER:
        case COLOR_DARKER:
        case COLOR_SHADE:
          symbolic = gtk_symbolic_color_new_shade (child1, value);
          break;
        case COLOR_ALPHA:
          symbolic = gtk_symbolic_color_new_alpha (child1, value);
          break;
        case COLOR_MIX:
          symbolic = gtk_symbolic_color_new_mix (child1, child2, value);
          break;
        default:
          g_assert_not_reached ();
          symbolic = NULL;
        }

      gtk_symbolic_color_unref (child1);
      if (child2)
        gtk_symbolic_color_unref (child2);
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected ')' in color definition");
      gtk_symbolic_color_unref (symbolic);
      return NULL;
    }

  return symbolic;
}

static GtkSymbolicColor *
gtk_css_parser_try_hash_color (GtkCssParser *parser)
{
  if (parser->data[0] == '#' &&
      g_ascii_isxdigit (parser->data[1]) &&
      g_ascii_isxdigit (parser->data[2]) &&
      g_ascii_isxdigit (parser->data[3]))
    {
      GdkRGBA rgba;
      
      if (g_ascii_isxdigit (parser->data[4]) &&
          g_ascii_isxdigit (parser->data[5]) &&
          g_ascii_isxdigit (parser->data[6]))
        {
          rgba.red   = ((get_xdigit (parser->data[1]) << 4) + get_xdigit (parser->data[2])) / 255.0;
          rgba.green = ((get_xdigit (parser->data[3]) << 4) + get_xdigit (parser->data[4])) / 255.0;
          rgba.blue  = ((get_xdigit (parser->data[5]) << 4) + get_xdigit (parser->data[6])) / 255.0;
          rgba.alpha = 1.0;
          parser->data += 7;
        }
      else
        {
          rgba.red   = get_xdigit (parser->data[1]) / 15.0;
          rgba.green = get_xdigit (parser->data[2]) / 15.0;
          rgba.blue  = get_xdigit (parser->data[3]) / 15.0;
          rgba.alpha = 1.0;
          parser->data += 4;
        }

      _gtk_css_parser_skip_whitespace (parser);

      return gtk_symbolic_color_new_literal (&rgba);
    }

  return NULL;
}

GtkSymbolicColor *
_gtk_css_parser_read_symbolic_color (GtkCssParser *parser)
{
  GtkSymbolicColor *symbolic;
  guint color;
  const char *names[] = {"rgba", "rgb",  "lighter", "darker", "shade", "alpha", "mix" };
  char *name;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), NULL);

  if (_gtk_css_parser_try (parser, "@", FALSE))
    {
      name = _gtk_css_parser_try_name (parser, TRUE);

      if (name)
        {
          symbolic = gtk_symbolic_color_new_name (name);
        }
      else
        {
          _gtk_css_parser_error (parser, "'%s' is not a valid symbolic color name", name);
          symbolic = NULL;
        }

      g_free (name);
      return symbolic;
    }

  for (color = 0; color < G_N_ELEMENTS (names); color++)
    {
      if (_gtk_css_parser_try (parser, names[color], TRUE))
        break;
    }

  if (color < G_N_ELEMENTS (names))
    return gtk_css_parser_read_symbolic_color_function (parser, color);

  symbolic = gtk_css_parser_try_hash_color (parser);
  if (symbolic)
    return symbolic;

  name = _gtk_css_parser_try_name (parser, TRUE);
  if (name)
    {
      GdkRGBA rgba;

      if (gdk_rgba_parse (&rgba, name))
        {
          symbolic = gtk_symbolic_color_new_literal (&rgba);
        }
      else
        {
          _gtk_css_parser_error (parser, "'%s' is not a valid color name", name);
          symbolic = NULL;
        }
      g_free (name);
      return symbolic;
    }

  _gtk_css_parser_error (parser, "Not a color definition");
  return NULL;
}

void
_gtk_css_parser_resync_internal (GtkCssParser *parser,
                                 gboolean      sync_at_semicolon,
                                 gboolean      read_sync_token,
                                 char          terminator)
{
  gsize len;

  do {
    len = strcspn (parser->data, "\\\"'/()[]{};" NEWLINE_CHARS);
    parser->data += len;

    if (gtk_css_parser_new_line (parser))
      continue;

    if (_gtk_css_parser_is_string (parser))
      {
        /* Hrm, this emits errors, and i suspect it shouldn't... */
        char *free_me = _gtk_css_parser_read_string (parser);
        g_free (free_me);
        continue;
      }

    if (gtk_css_parser_skip_comment (parser))
      continue;

    switch (*parser->data)
      {
      case '\\':
        {
          GString *ignore = g_string_new (NULL);
          _gtk_css_parser_unescape (parser, ignore);
          g_string_free (ignore, TRUE);
        }
        break;
      case ';':
        if (sync_at_semicolon && !read_sync_token)
          return;
        parser->data++;
        if (sync_at_semicolon)
          {
            _gtk_css_parser_skip_whitespace (parser);
            return;
          }
        break;
      case '(':
        parser->data++;
        _gtk_css_parser_resync (parser, FALSE, ')');
        if (*parser->data)
          parser->data++;
        break;
      case '[':
        parser->data++;
        _gtk_css_parser_resync (parser, FALSE, ']');
        if (*parser->data)
          parser->data++;
        break;
      case '{':
        parser->data++;
        _gtk_css_parser_resync (parser, FALSE, '}');
        if (*parser->data)
          parser->data++;
        if (sync_at_semicolon || !terminator)
          {
            _gtk_css_parser_skip_whitespace (parser);
            return;
          }
        break;
      case '}':
      case ')':
      case ']':
        if (terminator == *parser->data)
          {
            _gtk_css_parser_skip_whitespace (parser);
            return;
          }
        parser->data++;
        continue;
      case '\0':
        break;
      case '/':
      default:
        parser->data++;
        break;
      }
  } while (*parser->data);
}

char *
_gtk_css_parser_read_value (GtkCssParser *parser)
{
  const char *start;
  char *result;

  g_return_val_if_fail (GTK_IS_CSS_PARSER (parser), NULL);

  start = parser->data;

  /* This needs to be done better */
  _gtk_css_parser_resync_internal (parser, TRUE, FALSE, '}');

  result = g_strndup (start, parser->data - start);
  if (result)
    {
      g_strchomp (result);
      if (result[0] == 0)
        {
          g_free (result);
          result = NULL;
        }
    }

  if (result == NULL)
    _gtk_css_parser_error (parser, "Expected a property value");

  return result;
}

void
_gtk_css_parser_resync (GtkCssParser *parser,
                        gboolean      sync_at_semicolon,
                        char          terminator)
{
  g_return_if_fail (GTK_IS_CSS_PARSER (parser));

  _gtk_css_parser_resync_internal (parser, sync_at_semicolon, TRUE, terminator);
}
