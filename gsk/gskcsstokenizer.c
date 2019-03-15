/* GSK - The GIMP Toolkit
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gskcsstokenizerprivate.h"

/* for error enum */
#include "gtk/gtkcssprovider.h"

#include <math.h>
#include <string.h>

struct _GskCssTokenizer
{
  gint                   ref_count;
  GBytes                *bytes;

  const gchar           *data;
  const gchar           *end;

  GskCssLocation         position;
};

static void
gsk_css_location_init (GskCssLocation *location)
{
  memset (location, 0, sizeof (GskCssLocation));
}

static void
gsk_css_location_advance (GskCssLocation *location,
                          gsize           bytes,
                          gsize           chars)
{
  location->bytes += bytes;
  location->chars += chars;
  location->line_bytes += bytes;
  location->line_chars += chars;
}

static void
gsk_css_location_advance_newline (GskCssLocation *location,
                                  gboolean        is_windows)
{
  gsk_css_location_advance (location, is_windows ? 2 : 1, is_windows ? 2 : 1);

  location->lines++;
  location->line_bytes = 0;
  location->line_chars = 0;
}

void
gsk_css_token_clear (GskCssToken *token)
{
  switch (token->type)
    {
    case GSK_CSS_TOKEN_STRING:
    case GSK_CSS_TOKEN_IDENT:
    case GSK_CSS_TOKEN_FUNCTION:
    case GSK_CSS_TOKEN_AT_KEYWORD:
    case GSK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GSK_CSS_TOKEN_HASH_ID:
    case GSK_CSS_TOKEN_URL:
      g_free (token->string.string);
      break;

    case GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_DIMENSION:
      g_free (token->dimension.dimension);
      break;

    default:
      g_assert_not_reached ();
    case GSK_CSS_TOKEN_EOF:
    case GSK_CSS_TOKEN_WHITESPACE:
    case GSK_CSS_TOKEN_OPEN_PARENS:
    case GSK_CSS_TOKEN_CLOSE_PARENS:
    case GSK_CSS_TOKEN_OPEN_SQUARE:
    case GSK_CSS_TOKEN_CLOSE_SQUARE:
    case GSK_CSS_TOKEN_OPEN_CURLY:
    case GSK_CSS_TOKEN_CLOSE_CURLY:
    case GSK_CSS_TOKEN_COMMA:
    case GSK_CSS_TOKEN_COLON:
    case GSK_CSS_TOKEN_SEMICOLON:
    case GSK_CSS_TOKEN_CDC:
    case GSK_CSS_TOKEN_CDO:
    case GSK_CSS_TOKEN_DELIM:
    case GSK_CSS_TOKEN_SIGNED_INTEGER:
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GSK_CSS_TOKEN_SIGNED_NUMBER:
    case GSK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GSK_CSS_TOKEN_PERCENTAGE:
    case GSK_CSS_TOKEN_INCLUDE_MATCH:
    case GSK_CSS_TOKEN_DASH_MATCH:
    case GSK_CSS_TOKEN_PREFIX_MATCH:
    case GSK_CSS_TOKEN_SUFFIX_MATCH:
    case GSK_CSS_TOKEN_SUBSTRING_MATCH:
    case GSK_CSS_TOKEN_COLUMN:
    case GSK_CSS_TOKEN_BAD_STRING:
    case GSK_CSS_TOKEN_BAD_URL:
    case GSK_CSS_TOKEN_COMMENT:
      break;
    }

  token->type = GSK_CSS_TOKEN_EOF;
}

static void
gsk_css_token_initv (GskCssToken     *token,
                     GskCssTokenType  type,
                     va_list          args)
{
  token->type = type;

  switch (type)
    {
    case GSK_CSS_TOKEN_STRING:
    case GSK_CSS_TOKEN_IDENT:
    case GSK_CSS_TOKEN_FUNCTION:
    case GSK_CSS_TOKEN_AT_KEYWORD:
    case GSK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GSK_CSS_TOKEN_HASH_ID:
    case GSK_CSS_TOKEN_URL:
      token->string.string = va_arg (args, char *);
      break;

    case GSK_CSS_TOKEN_DELIM:
      token->delim.delim = va_arg (args, gunichar);
      break;

    case GSK_CSS_TOKEN_SIGNED_INTEGER:
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GSK_CSS_TOKEN_SIGNED_NUMBER:
    case GSK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GSK_CSS_TOKEN_PERCENTAGE:
      token->number.number = va_arg (args, double);
      break;

    case GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_DIMENSION:
      token->dimension.value = va_arg (args, double);
      token->dimension.dimension = va_arg (args, char *);
      break;

    default:
      g_assert_not_reached ();
    case GSK_CSS_TOKEN_EOF:
    case GSK_CSS_TOKEN_WHITESPACE:
    case GSK_CSS_TOKEN_OPEN_PARENS:
    case GSK_CSS_TOKEN_CLOSE_PARENS:
    case GSK_CSS_TOKEN_OPEN_SQUARE:
    case GSK_CSS_TOKEN_CLOSE_SQUARE:
    case GSK_CSS_TOKEN_OPEN_CURLY:
    case GSK_CSS_TOKEN_CLOSE_CURLY:
    case GSK_CSS_TOKEN_COMMA:
    case GSK_CSS_TOKEN_COLON:
    case GSK_CSS_TOKEN_SEMICOLON:
    case GSK_CSS_TOKEN_CDC:
    case GSK_CSS_TOKEN_CDO:
    case GSK_CSS_TOKEN_INCLUDE_MATCH:
    case GSK_CSS_TOKEN_DASH_MATCH:
    case GSK_CSS_TOKEN_PREFIX_MATCH:
    case GSK_CSS_TOKEN_SUFFIX_MATCH:
    case GSK_CSS_TOKEN_SUBSTRING_MATCH:
    case GSK_CSS_TOKEN_COLUMN:
    case GSK_CSS_TOKEN_BAD_STRING:
    case GSK_CSS_TOKEN_BAD_URL:
    case GSK_CSS_TOKEN_COMMENT:
      break;
    }
}

static void
append_ident (GString    *string,
              const char *ident)
{
  /* XXX */
  g_string_append (string, ident);
}

static void
append_string (GString    *string,
               const char *s)
{
  g_string_append_c (string, '"');
  /* XXX */
  g_string_append (string, s);
  g_string_append_c (string, '"');
}

/*
 * gsk_css_token_is_finite:
 * @token: a #GskCssToken
 *
 * A token is considered finite when it would stay the same no matter
 * what bytes follow it in the data stream.
 *
 * An obvious example for this is the ';' token.
 *
 * Returns: %TRUE if the token is considered finite.
 **/
gboolean
gsk_css_token_is_finite (const GskCssToken *token)
{
  switch (token->type)
    {
    case GSK_CSS_TOKEN_EOF:
    case GSK_CSS_TOKEN_STRING:
    case GSK_CSS_TOKEN_FUNCTION:
    case GSK_CSS_TOKEN_URL:
    case GSK_CSS_TOKEN_PERCENTAGE:
    case GSK_CSS_TOKEN_OPEN_PARENS:
    case GSK_CSS_TOKEN_CLOSE_PARENS:
    case GSK_CSS_TOKEN_OPEN_SQUARE:
    case GSK_CSS_TOKEN_CLOSE_SQUARE:
    case GSK_CSS_TOKEN_OPEN_CURLY:
    case GSK_CSS_TOKEN_CLOSE_CURLY:
    case GSK_CSS_TOKEN_COMMA:
    case GSK_CSS_TOKEN_COLON:
    case GSK_CSS_TOKEN_SEMICOLON:
    case GSK_CSS_TOKEN_CDC:
    case GSK_CSS_TOKEN_CDO:
    case GSK_CSS_TOKEN_INCLUDE_MATCH:
    case GSK_CSS_TOKEN_DASH_MATCH:
    case GSK_CSS_TOKEN_PREFIX_MATCH:
    case GSK_CSS_TOKEN_SUFFIX_MATCH:
    case GSK_CSS_TOKEN_SUBSTRING_MATCH:
    case GSK_CSS_TOKEN_COLUMN:
    case GSK_CSS_TOKEN_COMMENT:
      return TRUE;

    default:
      g_assert_not_reached ();
    case GSK_CSS_TOKEN_WHITESPACE:
    case GSK_CSS_TOKEN_IDENT:
    case GSK_CSS_TOKEN_AT_KEYWORD:
    case GSK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GSK_CSS_TOKEN_HASH_ID:
    case GSK_CSS_TOKEN_DELIM:
    case GSK_CSS_TOKEN_SIGNED_INTEGER:
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GSK_CSS_TOKEN_SIGNED_NUMBER:
    case GSK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GSK_CSS_TOKEN_BAD_STRING:
    case GSK_CSS_TOKEN_BAD_URL:
    case GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_DIMENSION:
      return FALSE;
    }
}

gboolean
gsk_css_token_is_ident (const GskCssToken *token,
                        const char        *ident)
{
  return gsk_css_token_is (token, GSK_CSS_TOKEN_IDENT)
      && (g_ascii_strcasecmp (token->string.string, ident) == 0);
}

gboolean
gsk_css_token_is_function (const GskCssToken *token,
                           const char        *ident)
{
  return gsk_css_token_is (token, GSK_CSS_TOKEN_FUNCTION)
      && (g_ascii_strcasecmp (token->string.string, ident) == 0);
}

gboolean
gsk_css_token_is_delim (const GskCssToken *token,
                        gunichar           delim)
{
  return gsk_css_token_is (token, GSK_CSS_TOKEN_DELIM)
      && token->delim.delim == delim;
}

void
gsk_css_token_print (const GskCssToken *token,
                     GString           *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (token->type)
    {
    case GSK_CSS_TOKEN_STRING:
      append_string (string, token->string.string);
      break;

    case GSK_CSS_TOKEN_IDENT:
      append_ident (string, token->string.string);
      break;

    case GSK_CSS_TOKEN_URL:
      g_string_append (string, "url(");
      append_ident (string, token->string.string);
      g_string_append (string, ")");
      break;

    case GSK_CSS_TOKEN_FUNCTION:
      append_ident (string, token->string.string);
      g_string_append_c (string, '(');
      break;

    case GSK_CSS_TOKEN_AT_KEYWORD:
      g_string_append_c (string, '@');
      append_ident (string, token->string.string);
      break;

    case GSK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GSK_CSS_TOKEN_HASH_ID:
      g_string_append_c (string, '#');
      append_ident (string, token->string.string);
      break;

    case GSK_CSS_TOKEN_DELIM:
      g_string_append_unichar (string, token->delim.delim);
      break;

    case GSK_CSS_TOKEN_SIGNED_INTEGER:
    case GSK_CSS_TOKEN_SIGNED_NUMBER:
      if (token->number.number >= 0)
        g_string_append_c (string, '+');
      /* fall through */
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GSK_CSS_TOKEN_SIGNLESS_NUMBER:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->number.number);
      g_string_append (string, buf);
      break;

    case GSK_CSS_TOKEN_PERCENTAGE:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->number.number);
      g_string_append (string, buf);
      g_string_append_c (string, '%');
      break;

    case GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
      if (token->dimension.value >= 0)
        g_string_append_c (string, '+');
      /* fall through */
    case GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GSK_CSS_TOKEN_DIMENSION:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->dimension.value);
      g_string_append (string, buf);
      append_ident (string, token->dimension.dimension);
      break;

    case GSK_CSS_TOKEN_EOF:
      break;

    case GSK_CSS_TOKEN_WHITESPACE:
      g_string_append (string, " ");
      break;

    case GSK_CSS_TOKEN_OPEN_PARENS:
      g_string_append (string, "(");
      break;

    case GSK_CSS_TOKEN_CLOSE_PARENS:
      g_string_append (string, ")");
      break;

    case GSK_CSS_TOKEN_OPEN_SQUARE:
      g_string_append (string, "[");
      break;

    case GSK_CSS_TOKEN_CLOSE_SQUARE:
      g_string_append (string, "]");
      break;

    case GSK_CSS_TOKEN_OPEN_CURLY:
      g_string_append (string, "{");
      break;

    case GSK_CSS_TOKEN_CLOSE_CURLY:
      g_string_append (string, "}");
      break;

    case GSK_CSS_TOKEN_COMMA:
      g_string_append (string, ",");
      break;

    case GSK_CSS_TOKEN_COLON:
      g_string_append (string, ":");
      break;

    case GSK_CSS_TOKEN_SEMICOLON:
      g_string_append (string, ";");
      break;

    case GSK_CSS_TOKEN_CDO:
      g_string_append (string, "<!--");
      break;

    case GSK_CSS_TOKEN_CDC:
      g_string_append (string, "-->");
      break;

    case GSK_CSS_TOKEN_INCLUDE_MATCH:
      g_string_append (string, "~=");
      break;

    case GSK_CSS_TOKEN_DASH_MATCH:
      g_string_append (string, "|=");
      break;

    case GSK_CSS_TOKEN_PREFIX_MATCH:
      g_string_append (string, "^=");
      break;

    case GSK_CSS_TOKEN_SUFFIX_MATCH:
      g_string_append (string, "$=");
      break;

    case GSK_CSS_TOKEN_SUBSTRING_MATCH:
      g_string_append (string, "*=");
      break;

    case GSK_CSS_TOKEN_COLUMN:
      g_string_append (string, "||");
      break;

    case GSK_CSS_TOKEN_BAD_STRING:
      g_string_append (string, "\"\n");
      break;

    case GSK_CSS_TOKEN_BAD_URL:
      g_string_append (string, "url(bad url)");
      break;

    case GSK_CSS_TOKEN_COMMENT:
      g_string_append (string, "/* comment */");
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

char *
gsk_css_token_to_string (const GskCssToken *token)
{
  GString *string;

  string = g_string_new (NULL);
  gsk_css_token_print (token, string);
  return g_string_free (string, FALSE);
}

static void
gsk_css_token_init (GskCssToken     *token,
                    GskCssTokenType  type,
                    ...)
{
  va_list args;

  va_start (args, type);
  gsk_css_token_initv (token, type, args);
  va_end (args);
}

GskCssTokenizer *
gsk_css_tokenizer_new (GBytes *bytes)
{
  GskCssTokenizer *tokenizer;

  tokenizer = g_slice_new0 (GskCssTokenizer);
  tokenizer->ref_count = 1;
  tokenizer->bytes = g_bytes_ref (bytes);

  tokenizer->data = g_bytes_get_data (bytes, NULL);
  tokenizer->end = tokenizer->data + g_bytes_get_size (bytes);

  gsk_css_location_init (&tokenizer->position);

  return tokenizer;
}

GskCssTokenizer *
gsk_css_tokenizer_ref (GskCssTokenizer *tokenizer)
{
  tokenizer->ref_count++;
  
  return tokenizer;
}

void
gsk_css_tokenizer_unref (GskCssTokenizer *tokenizer)
{
  tokenizer->ref_count--;
  if (tokenizer->ref_count > 0)
    return;

  g_bytes_unref (tokenizer->bytes);
  g_slice_free (GskCssTokenizer, tokenizer);
}

const GskCssLocation *
gsk_css_tokenizer_get_location (GskCssTokenizer *tokenizer)
{
  return &tokenizer->position;
}

static void
gsk_css_tokenizer_parse_error (GError     **error,
                               const char  *format,
                               ...) G_GNUC_PRINTF(2, 3);
static void
gsk_css_tokenizer_parse_error (GError     **error,
                               const char  *format,
                               ...)
{
  va_list args;

  va_start (args, format);
  if (error)
    {
      *error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                                   GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                   format, args);
    }
  else
    {
      char *s = g_strdup_vprintf (format, args);
      g_print ("error: %s\n", s);
      g_free (s);
    }
  va_end (args);
}

static gboolean
is_newline (char c)
{
  return c == '\n'
      || c == '\r'
      || c == '\f';
}

static gboolean
is_whitespace (char c)
{
  return is_newline (c)
      || c == '\t'
      || c == ' ';
}

static gboolean
is_multibyte (char c)
{
  return c & 0x80;
}

static gboolean
is_name_start (char c)
{
   return is_multibyte (c)
       || g_ascii_isalpha (c)
       || c == '_';
}

static gboolean
is_name (char c)
{
  return is_name_start (c)
      || g_ascii_isdigit (c)
      || c == '-';
}

static gboolean
is_valid_escape (char c1, char c2)
{
  return c1 == '\\'
      && !is_newline (c2);
}

static gboolean
is_non_printable (char c)
{
  return (c >= 0 && c <= 0x08)
      || c == 0x0B
      || c == 0x0E
      || c == 0x1F
      || c == 0x7F;
}

static inline gsize
gsk_css_tokenizer_remaining (GskCssTokenizer *tokenizer)
{
  return tokenizer->end - tokenizer->data;
}

static gboolean
gsk_css_tokenizer_has_valid_escape (GskCssTokenizer *tokenizer)
{
  switch (gsk_css_tokenizer_remaining (tokenizer))
    {
      case 0:
        return FALSE;
      case 1:
        return *tokenizer->data == '\\';
      default:
        return is_valid_escape (tokenizer->data[0], tokenizer->data[1]);
    }
}

static gboolean
gsk_css_tokenizer_has_identifier (GskCssTokenizer *tokenizer)
{
  const char *data = tokenizer->data;

  if (data == tokenizer->end)
    return FALSE;

  if (*data == '-')
    {
      data++;
      if (data == tokenizer->end)
        return FALSE;
      if (*data == '-')
        return TRUE;
    }

  if (is_name_start (*data))
    return TRUE;

  if (*data == '\\')
    {
      data++;
      if (data == tokenizer->end)
        return TRUE; /* really? */
      if (is_newline (*data))
        return FALSE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
gsk_css_tokenizer_has_number (GskCssTokenizer *tokenizer)
{
  const char *data = tokenizer->data;

  if (data == tokenizer->end)
    return FALSE;

  if (*data == '-' || *data == '+')
    {
      data++;
      if (data == tokenizer->end)
        return FALSE;
    }

  if (*data == '.')
    {
      data++;
      if (data == tokenizer->end)
        return FALSE;
    }

  return g_ascii_isdigit (*data);
}

static void
gsk_css_tokenizer_consume_newline (GskCssTokenizer *tokenizer)
{
  gsize n;

  if (gsk_css_tokenizer_remaining (tokenizer) > 1 &&
      tokenizer->data[0] == '\r' && tokenizer->data[1] == '\n')
    n = 2;
  else
    n = 1;
  
  tokenizer->data += n;
  gsk_css_location_advance_newline (&tokenizer->position, n == 2 ? TRUE : FALSE);
}

static inline void
gsk_css_tokenizer_consume (GskCssTokenizer *tokenizer,
                           gsize            n_bytes,
                           gsize            n_characters)
{
  /* NB: must not contain newlines! */
  tokenizer->data += n_bytes;

  gsk_css_location_advance (&tokenizer->position, n_bytes, n_characters);
}

static inline void
gsk_css_tokenizer_consume_ascii (GskCssTokenizer *tokenizer)
{
  /* NB: must not contain newlines! */
  gsk_css_tokenizer_consume (tokenizer, 1, 1);
}

static inline void
gsk_css_tokenizer_consume_whitespace (GskCssTokenizer *tokenizer)
{
  if (is_newline (*tokenizer->data))
    gsk_css_tokenizer_consume_newline (tokenizer);
  else
    gsk_css_tokenizer_consume_ascii (tokenizer);
}

static inline void
gsk_css_tokenizer_consume_char (GskCssTokenizer *tokenizer,
                                GString         *string)
{
  if (is_newline (*tokenizer->data))
    gsk_css_tokenizer_consume_newline (tokenizer);
  else
    {
      gsize char_size = g_utf8_next_char (tokenizer->data) - tokenizer->data;

      if (string)
        g_string_append_len (string, tokenizer->data, char_size);
      gsk_css_tokenizer_consume (tokenizer, char_size, 1);
    }
}

static void
gsk_css_tokenizer_read_whitespace (GskCssTokenizer *tokenizer,
                                   GskCssToken     *token)
{
  do {
    gsk_css_tokenizer_consume_whitespace (tokenizer);
  } while (tokenizer->data != tokenizer->end &&
           is_whitespace (*tokenizer->data));

  gsk_css_token_init (token, GSK_CSS_TOKEN_WHITESPACE);
}

static gunichar 
gsk_css_tokenizer_read_escape (GskCssTokenizer *tokenizer)
{
  gunichar value = 0;
  guint i;

  gsk_css_tokenizer_consume (tokenizer, 1, 1);

  for (i = 0; i < 6 && tokenizer->data < tokenizer->end && g_ascii_isxdigit (*tokenizer->data); i++)
    {
      value = value * 16 + g_ascii_xdigit_value (*tokenizer->data);
      gsk_css_tokenizer_consume (tokenizer, 1, 1);
    }

  if (i == 0)
    return 0xFFFD;

  return value;
}

static char *
gsk_css_tokenizer_read_name (GskCssTokenizer *tokenizer)
{
  GString *string = g_string_new (NULL);

  do {
      if (*tokenizer->data == '\\')
        {
          if (gsk_css_tokenizer_has_valid_escape (tokenizer))
            {
              gunichar value = gsk_css_tokenizer_read_escape (tokenizer);

              if (value > 0 ||
                  (value >= 0xD800 && value <= 0xDFFF) ||
                  value <= 0x10FFFF)
                g_string_append_unichar (string, value);
              else
                g_string_append_unichar (string, 0xFFFD);
            }
          else
            {
              gsk_css_tokenizer_consume_ascii (tokenizer);

              if (tokenizer->data == tokenizer->end)
                {
                  g_string_append_unichar (string, 0xFFFD);
                  break;
                }
              
              gsk_css_tokenizer_consume_char (tokenizer, string);
            }
        }
      else if (is_name (*tokenizer->data))
        {
          gsk_css_tokenizer_consume_char (tokenizer, string);
        }
      else
        {
          break;
        }
    }
  while (tokenizer->data != tokenizer->end);

  return g_string_free (string, FALSE);
}

static void
gsk_css_tokenizer_read_bad_url (GskCssTokenizer  *tokenizer,
                                GskCssToken      *token)
{
  while (tokenizer->data < tokenizer->end && *tokenizer->data != ')')
    {
      if (gsk_css_tokenizer_has_valid_escape (tokenizer))
        gsk_css_tokenizer_read_escape (tokenizer);
      else
        gsk_css_tokenizer_consume_char (tokenizer, NULL);
    }
  
  if (tokenizer->data < tokenizer->end)
    gsk_css_tokenizer_consume_ascii (tokenizer);

  gsk_css_token_init (token, GSK_CSS_TOKEN_BAD_URL);
}

static gboolean
gsk_css_tokenizer_read_url (GskCssTokenizer  *tokenizer,
                            GskCssToken      *token,
                            GError          **error)
{
  GString *url = g_string_new (NULL);

  while (tokenizer->data < tokenizer->end && is_whitespace (*tokenizer->data))
    gsk_css_tokenizer_consume_whitespace (tokenizer);

  while (tokenizer->data < tokenizer->end)
    {
      if (*tokenizer->data == ')')
        {
          gsk_css_tokenizer_consume_ascii (tokenizer);
          break;
        }
      else if (is_whitespace (*tokenizer->data))
        {
          do
            gsk_css_tokenizer_consume_whitespace (tokenizer);
          while (tokenizer->data < tokenizer->end && is_whitespace (*tokenizer->data));
          
          if (*tokenizer->data == ')')
            {
              gsk_css_tokenizer_consume_ascii (tokenizer);
              break;
            }
          else if (tokenizer->data >= tokenizer->end)
            {
              break;
            }
          else
            {
              gsk_css_tokenizer_read_bad_url (tokenizer, token);
              gsk_css_tokenizer_parse_error (error, "Whitespace only allowed at start and end of url");
              return FALSE;
            }
        }
      else if (is_non_printable (*tokenizer->data))
        {
          gsk_css_tokenizer_read_bad_url (tokenizer, token);
          g_string_free (url, TRUE);
          gsk_css_tokenizer_parse_error (error, "Nonprintable character 0x%02X in url", *tokenizer->data);
          return FALSE;
        }
      else if (*tokenizer->data == '"' ||
               *tokenizer->data == '\'' ||
               *tokenizer->data == '(')
        {
          gsk_css_tokenizer_read_bad_url (tokenizer, token);
          gsk_css_tokenizer_parse_error (error, "Invalid character %c in url", *tokenizer->data);
          g_string_free (url, TRUE);
          return FALSE;
        }
      else if (gsk_css_tokenizer_has_valid_escape (tokenizer))
        {
          g_string_append_unichar (url, gsk_css_tokenizer_read_escape (tokenizer));
        }
      else if (*tokenizer->data == '\\')
        {
          gsk_css_tokenizer_read_bad_url (tokenizer, token);
          gsk_css_tokenizer_parse_error (error, "Newline may not follow '\' escape character");
          g_string_free (url, TRUE);
          return FALSE;
        }
      else
        {
          gsk_css_tokenizer_consume_char (tokenizer, url);
        }
    }

  gsk_css_token_init (token, GSK_CSS_TOKEN_URL, g_string_free (url, FALSE));

  return TRUE;
}

static gboolean
gsk_css_tokenizer_read_ident_like (GskCssTokenizer  *tokenizer,
                                   GskCssToken      *token,
                                   GError          **error)
{
  char *name = gsk_css_tokenizer_read_name (tokenizer);

  if (*tokenizer->data == '(')
    {
      gsk_css_tokenizer_consume_ascii (tokenizer);
      if (g_ascii_strcasecmp (name, "url") == 0)
        {
          const char *data = tokenizer->data;

          while (is_whitespace (*data))
            data++;

          if (*data != '"' && *data != '\'')
            {
              return gsk_css_tokenizer_read_url (tokenizer, token, error);
            }
        }
      
      gsk_css_token_init (token, GSK_CSS_TOKEN_FUNCTION, name);
      return TRUE;
    }
  else
    {
      gsk_css_token_init (token, GSK_CSS_TOKEN_IDENT, name);
      return TRUE;
    }
}

static void
gsk_css_tokenizer_read_numeric (GskCssTokenizer *tokenizer,
                                GskCssToken     *token)
{
  int sign = 1, exponent_sign = 1;
  gint64 integer, fractional = 0, fractional_length = 1, exponent = 0;
  gboolean is_int = TRUE, has_sign = FALSE;
  const char *data = tokenizer->data;

  if (*data == '-')
    {
      has_sign = TRUE;
      sign = -1;
      data++;
    }
  else if (*data == '+')
    {
      has_sign = TRUE;
      data++;
    }

  for (integer = 0; data < tokenizer->end && g_ascii_isdigit (*data); data++)
    {
      /* check for overflow here? */
      integer = 10 * integer + g_ascii_digit_value (*data);
    }

  if (data + 1 < tokenizer->end && *data == '.' && g_ascii_isdigit (data[1]))
    {
      is_int = FALSE;
      data++;

      fractional = g_ascii_digit_value (*data);
      fractional_length = 10;
      data++;

      while (data < tokenizer->end && g_ascii_isdigit (*data))
        {
          if (fractional_length < G_MAXINT64 / 10)
            {
              fractional = 10 * fractional + g_ascii_digit_value (*data);
              fractional_length *= 10;
            }
          data++;
        }
    }

  if (data + 1 < tokenizer->end && (*data == 'e' || *data == 'E') && 
      (g_ascii_isdigit (data[1]) || 
       (data + 2 < tokenizer->end && (data[1] == '+' || data[2] == '-') && g_ascii_isdigit (data[2]))))
    {
      is_int = FALSE;
      data++;
      exponent = g_ascii_digit_value (*data);

      if (*data == '-')
        {
          exponent_sign = -1;
          data++;
        }
      else if (*data == '+')
        {
          data++;
        }

      while (data < tokenizer->end && g_ascii_isdigit (*data))
        {
          exponent = 10 * exponent + g_ascii_digit_value (*data);
          data++;
        }
    }

  gsk_css_tokenizer_consume (tokenizer, data - tokenizer->data, data - tokenizer->data);

  if (gsk_css_tokenizer_has_identifier (tokenizer))
    {
      gsk_css_token_init (token,
                          is_int ? (has_sign ? GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION : GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)
                                 : GSK_CSS_TOKEN_DIMENSION,
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent),
                          gsk_css_tokenizer_read_name (tokenizer));
    }
  else if (gsk_css_tokenizer_remaining (tokenizer) > 0 && *tokenizer->data == '%')
    {
      gsk_css_token_init (token,
                          GSK_CSS_TOKEN_PERCENTAGE,
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent));
      gsk_css_tokenizer_consume_ascii (tokenizer);
    }
  else
    {
      gsk_css_token_init (token,
                          is_int ? (has_sign ? GSK_CSS_TOKEN_SIGNED_INTEGER : GSK_CSS_TOKEN_SIGNLESS_INTEGER)
                                 : (has_sign ? GSK_CSS_TOKEN_SIGNED_NUMBER : GSK_CSS_TOKEN_SIGNLESS_NUMBER),
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent));
    }
}

static void
gsk_css_tokenizer_read_delim (GskCssTokenizer *tokenizer,
                              GskCssToken     *token)
{
  gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, g_utf8_get_char (tokenizer->data));
  gsk_css_tokenizer_consume_char (tokenizer, NULL);
}

static gboolean
gsk_css_tokenizer_read_dash (GskCssTokenizer  *tokenizer,
                             GskCssToken      *token,
                             GError          **error)
{
  if (gsk_css_tokenizer_remaining (tokenizer) == 1)
    {
      gsk_css_tokenizer_read_delim (tokenizer, token);
      return TRUE;
    }
  else if (gsk_css_tokenizer_has_number (tokenizer))
    {
      gsk_css_tokenizer_read_numeric (tokenizer, token);
      return TRUE;
    }
  else if (gsk_css_tokenizer_remaining (tokenizer) >= 3 &&
           tokenizer->data[1] == '-' &&
           tokenizer->data[2] == '>')
    {
      gsk_css_token_init (token, GSK_CSS_TOKEN_CDC);
      gsk_css_tokenizer_consume (tokenizer, 3, 3);
      return TRUE;
    }
  else if (gsk_css_tokenizer_has_identifier (tokenizer))
    {
      return gsk_css_tokenizer_read_ident_like (tokenizer, token, error);
    }
  else
    {
      gsk_css_tokenizer_read_delim (tokenizer, token);
      return TRUE;
    }
}

static gboolean
gsk_css_tokenizer_read_string (GskCssTokenizer  *tokenizer,
                               GskCssToken      *token,
                               GError          **error)
{
  GString *string = g_string_new (NULL);
  char end = *tokenizer->data;

  gsk_css_tokenizer_consume_ascii (tokenizer);

  while (tokenizer->data < tokenizer->end)
    {
      if (*tokenizer->data == end)
        {
          gsk_css_tokenizer_consume_ascii (tokenizer);
          break;
        }
      else if (*tokenizer->data == '\\')
        {
          if (gsk_css_tokenizer_remaining (tokenizer) == 1)
            {
              gsk_css_tokenizer_consume_ascii (tokenizer);
              break;
            }
          else if (is_newline (tokenizer->data[1]))
            {
              gsk_css_tokenizer_consume_ascii (tokenizer);
              gsk_css_tokenizer_consume_newline (tokenizer);
            }
          else
            {
              g_string_append_unichar (string, gsk_css_tokenizer_read_escape (tokenizer));
            }
        }
      else if (is_newline (*tokenizer->data))
        {
          g_string_free (string, TRUE);
          gsk_css_token_init (token, GSK_CSS_TOKEN_BAD_STRING);
          gsk_css_tokenizer_parse_error (error, "Newlines inside strings must be escaped");
          return FALSE;
        }
      else
        {
          gsk_css_tokenizer_consume_char (tokenizer, string);
        }
    }
  
  gsk_css_token_init (token, GSK_CSS_TOKEN_STRING, g_string_free (string, FALSE));

  return TRUE;
}

static gboolean
gsk_css_tokenizer_read_comment (GskCssTokenizer  *tokenizer,
                                GskCssToken      *token,
                                GError          **error)
{
  gsk_css_tokenizer_consume (tokenizer, 2, 2);

  while (tokenizer->data < tokenizer->end)
    {
      if (gsk_css_tokenizer_remaining (tokenizer) > 1 &&
          tokenizer->data[0] == '*' && tokenizer->data[1] == '/')
        {
          gsk_css_tokenizer_consume (tokenizer, 2, 2);
          gsk_css_token_init (token, GSK_CSS_TOKEN_COMMENT);
          return TRUE;
        }
      gsk_css_tokenizer_consume_char (tokenizer, NULL);
    }

  gsk_css_token_init (token, GSK_CSS_TOKEN_COMMENT);
  gsk_css_tokenizer_parse_error (error, "Comment not terminated at end of document.");
  return FALSE;
}

static void
gsk_css_tokenizer_read_match (GskCssTokenizer *tokenizer,
                              GskCssToken     *token,
                              GskCssTokenType  type)
{
  if (gsk_css_tokenizer_remaining (tokenizer) > 1 && tokenizer->data[1] == '=')
    {
      gsk_css_token_init (token, type);
      gsk_css_tokenizer_consume (tokenizer, 2, 2);
    }
  else
    {
      gsk_css_tokenizer_read_delim (tokenizer, token);
    }
}

gboolean
gsk_css_tokenizer_read_token (GskCssTokenizer  *tokenizer,
                              GskCssToken      *token,
                              GError          **error)
{
  if (tokenizer->data == tokenizer->end)
    {
      gsk_css_token_init (token, GSK_CSS_TOKEN_EOF);
      return TRUE;
    }

  if (tokenizer->data[0] == '/' && gsk_css_tokenizer_remaining (tokenizer) > 1 &&
      tokenizer->data[1] == '*')
    return gsk_css_tokenizer_read_comment (tokenizer, token, error);

  switch (*tokenizer->data)
    {
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case ' ':
      gsk_css_tokenizer_read_whitespace (tokenizer, token);
      return TRUE;

    case '"':
      return gsk_css_tokenizer_read_string (tokenizer, token, error);

    case '#':
      gsk_css_tokenizer_consume_ascii (tokenizer);
      if (is_name (*tokenizer->data) || gsk_css_tokenizer_has_valid_escape (tokenizer))
        {
          GskCssTokenType type;

          if (gsk_css_tokenizer_has_identifier (tokenizer))
            type = GSK_CSS_TOKEN_HASH_ID;
          else
            type = GSK_CSS_TOKEN_HASH_UNRESTRICTED;

          gsk_css_token_init (token,
                              type,
                              gsk_css_tokenizer_read_name (tokenizer));
        }
      else
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, '#');
        }
      return TRUE;

    case '$':
      gsk_css_tokenizer_read_match (tokenizer, token, GSK_CSS_TOKEN_SUFFIX_MATCH);
      return TRUE;

    case '\'':
      return gsk_css_tokenizer_read_string (tokenizer, token, error);

    case '(':
      gsk_css_token_init (token, GSK_CSS_TOKEN_OPEN_PARENS);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case ')':
      gsk_css_token_init (token, GSK_CSS_TOKEN_CLOSE_PARENS);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '*':
      gsk_css_tokenizer_read_match (tokenizer, token, GSK_CSS_TOKEN_SUBSTRING_MATCH);
      return TRUE;

    case '+':
      if (gsk_css_tokenizer_has_number (tokenizer))
        gsk_css_tokenizer_read_numeric (tokenizer, token);
      else
        gsk_css_tokenizer_read_delim (tokenizer, token);
      return TRUE;

    case ',':
      gsk_css_token_init (token, GSK_CSS_TOKEN_COMMA);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '-':
      return gsk_css_tokenizer_read_dash (tokenizer, token, error);

    case '.':
      if (gsk_css_tokenizer_has_number (tokenizer))
        gsk_css_tokenizer_read_numeric (tokenizer, token);
      else
        gsk_css_tokenizer_read_delim (tokenizer, token);
      return TRUE;

    case ':':
      gsk_css_token_init (token, GSK_CSS_TOKEN_COLON);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case ';':
      gsk_css_token_init (token, GSK_CSS_TOKEN_SEMICOLON);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '<':
      if (gsk_css_tokenizer_remaining (tokenizer) >= 4 &&
          tokenizer->data[1] == '!' &&
          tokenizer->data[2] == '-' &&
          tokenizer->data[3] == '-')
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_CDO);
          gsk_css_tokenizer_consume (tokenizer, 3, 3);
        }
      else
        {
          gsk_css_tokenizer_read_delim (tokenizer, token);
        }
      return TRUE;

    case '@':
      gsk_css_tokenizer_consume_ascii (tokenizer);
      if (gsk_css_tokenizer_has_identifier (tokenizer))
        {
          gsk_css_token_init (token,
                              GSK_CSS_TOKEN_AT_KEYWORD,
                              gsk_css_tokenizer_read_name (tokenizer));
        }
      else
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, '@');
        }
      return TRUE;

    case '[':
      gsk_css_token_init (token, GSK_CSS_TOKEN_OPEN_SQUARE);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '\\':
      if (gsk_css_tokenizer_has_valid_escape (tokenizer))
        {
          return gsk_css_tokenizer_read_ident_like (tokenizer, token, error);
        }
      else
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, '\\');
          gsk_css_tokenizer_parse_error (error, "Newline may not follow '\' escape character");
          return FALSE;
        }

    case ']':
      gsk_css_token_init (token, GSK_CSS_TOKEN_CLOSE_SQUARE);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '^':
      gsk_css_tokenizer_read_match (tokenizer, token, GSK_CSS_TOKEN_PREFIX_MATCH);
      return TRUE;

    case '{':
      gsk_css_token_init (token, GSK_CSS_TOKEN_OPEN_CURLY);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '}':
      gsk_css_token_init (token, GSK_CSS_TOKEN_CLOSE_CURLY);
      gsk_css_tokenizer_consume_ascii (tokenizer);
      return TRUE;

    case '|':
      if (gsk_css_tokenizer_remaining (tokenizer) > 1 && tokenizer->data[1] == '|')
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_COLUMN);
          gsk_css_tokenizer_consume (tokenizer, 2, 2);
        }
      else
        {
          gsk_css_tokenizer_read_match (tokenizer, token, GSK_CSS_TOKEN_DASH_MATCH);
        }
      return TRUE;

    case '~':
      gsk_css_tokenizer_read_match (tokenizer, token, GSK_CSS_TOKEN_INCLUDE_MATCH);
      return TRUE;

    default:
      if (g_ascii_isdigit (*tokenizer->data))
        {
          gsk_css_tokenizer_read_numeric (tokenizer, token);
          return TRUE;
        }
      else if (is_name_start (*tokenizer->data))
        {
          return gsk_css_tokenizer_read_ident_like (tokenizer, token, error);
        }
      else
        {
          gsk_css_tokenizer_read_delim (tokenizer, token);
          return TRUE;
        }
    }
}

