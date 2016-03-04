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
#include <gtk/gtkcssprovider.h>

#include <math.h>
#include <string.h>

typedef struct _GskCssTokenReader GskCssTokenReader;

struct _GskCssTokenReader {
  const char *           data;
  const char *           end;

  GskCssLocation         position;
};

struct _GskCssTokenizer
{
  gint                   ref_count;
  GBytes                *bytes;
  GskCssTokenizerErrorFunc error_func;
  gpointer               user_data;
  GDestroyNotify         user_destroy;

  GskCssTokenReader      reader;
};

static void
gsk_css_location_init (GskCssLocation *location)
{
  memset (location, 0, sizeof (GskCssLocation));
}

static void
gsk_css_location_init_copy (GskCssLocation       *location,
                            const GskCssLocation *source)
{
  *location = *source;
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

static void
gsk_css_token_reader_init (GskCssTokenReader *reader,
                           GBytes            *bytes)
{
  reader->data = g_bytes_get_data (bytes, NULL);
  reader->end = reader->data + g_bytes_get_size (bytes);

  gsk_css_location_init (&reader->position);
}

static void
gsk_css_token_reader_init_copy (GskCssTokenReader       *reader,
                                const GskCssTokenReader *source)
{
  *reader = *source;
}

GskCssTokenizer *
gsk_css_tokenizer_new (GBytes                   *bytes,
                       GskCssTokenizerErrorFunc  func,
                       gpointer                  user_data,
                       GDestroyNotify            user_destroy)
{
  GskCssTokenizer *tokenizer;

  tokenizer = g_slice_new0 (GskCssTokenizer);
  tokenizer->ref_count = 1;
  tokenizer->bytes = g_bytes_ref (bytes);
  tokenizer->error_func = func;
  tokenizer->user_data = user_data;
  tokenizer->user_destroy = user_destroy;

  gsk_css_token_reader_init (&tokenizer->reader, bytes);

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

  if (tokenizer->user_destroy)
    tokenizer->user_destroy (tokenizer->user_data);

  g_bytes_unref (tokenizer->bytes);
  g_slice_free (GskCssTokenizer, tokenizer);
}

const GskCssLocation *
gsk_css_tokenizer_get_location (GskCssTokenizer *tokenizer)
{
  return &tokenizer->reader.position;
}

static void
set_parse_error (GError     **error,
                 const char  *format,
                 ...) G_GNUC_PRINTF(2, 3);
static void
set_parse_error (GError     **error,
                 const char  *format,
                 ...)
{
  va_list args;

  if (error == NULL)
    return;
      
  g_assert (*error == NULL);

  va_start (args, format); 
  *error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_SYNTAX,
                               format,
                               args);
  va_end (args);
}

static void
gsk_css_tokenizer_emit_error (GskCssTokenizer      *tokenizer,
                              const GskCssLocation *location,
                              const GskCssToken    *token,
                              const GError         *error)
{
  if (tokenizer->error_func)
    tokenizer->error_func (tokenizer, location, token, error, tokenizer->user_data);
  else
    g_warning ("Unhandled CSS error: %zu:%zu: %s", location->lines + 1, location->line_chars + 1, error->message);
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
gsk_css_token_reader_remaining (const GskCssTokenReader *reader)
{
  return reader->end - reader->data;
}

static gboolean
gsk_css_token_reader_has_valid_escape (const GskCssTokenReader *reader)
{
  switch (gsk_css_token_reader_remaining (reader))
    {
      case 0:
        return FALSE;
      case 1:
        return *reader->data == '\\';
      default:
        return is_valid_escape (reader->data[0], reader->data[1]);
    }
}

static gboolean
gsk_css_token_reader_has_identifier (const GskCssTokenReader *reader)
{
  const char *data = reader->data;

  if (data == reader->end)
    return FALSE;

  if (*data == '-')
    {
      data++;
      if (data == reader->end)
        return FALSE;
      if (*data == '-')
        return TRUE;
    }

  if (is_name_start (*data))
    return TRUE;

  if (*data == '\\')
    {
      data++;
      if (data == reader->end)
        return TRUE; /* really? */
      if (is_newline (*data))
        return FALSE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
gsk_css_token_reader_has_number (const GskCssTokenReader *reader)
{
  const char *data = reader->data;

  if (data == reader->end)
    return FALSE;

  if (*data == '-' || *data == '+')
    {
      data++;
      if (data == reader->end)
        return FALSE;
    }

  if (*data == '.')
    {
      data++;
      if (data == reader->end)
        return FALSE;
    }

  return g_ascii_isdigit (*data);
}

static void
gsk_css_token_reader_consume_newline (GskCssTokenReader *reader)
{
  gsize n;

  if (gsk_css_token_reader_remaining (reader) > 1 &&
      reader->data[0] == '\r' && reader->data[1] == '\n')
    n = 2;
  else
    n = 1;
  
  reader->data += n;
  gsk_css_location_advance_newline (&reader->position, n == 2 ? TRUE : FALSE);
}

static inline void
gsk_css_token_reader_consume (GskCssTokenReader *reader,
                              gsize              n_bytes,
                              gsize              n_characters)
{
  /* NB: must not contain newlines! */
  reader->data += n_bytes;

  gsk_css_location_advance (&reader->position, n_bytes, n_characters);
}

static inline void
gsk_css_token_reader_consume_ascii (GskCssTokenReader *reader)
{
  /* NB: must not contain newlines! */
  gsk_css_token_reader_consume (reader, 1, 1);
}

static inline void
gsk_css_token_reader_consume_whitespace (GskCssTokenReader *reader)
{
  if (is_newline (*reader->data))
    gsk_css_token_reader_consume_newline (reader);
  else
    gsk_css_token_reader_consume_ascii (reader);
}

static inline void
gsk_css_token_reader_consume_char (GskCssTokenReader *reader,
                                   GString           *string)
{
  if (is_newline (*reader->data))
    gsk_css_token_reader_consume_newline (reader);
  else
    {
      gsize char_size = g_utf8_next_char (reader->data) - reader->data;

      if (string)
        g_string_append_len (string, reader->data, char_size);
      gsk_css_token_reader_consume (reader, char_size, 1);
    }
}

static void
gsk_css_token_reader_read_whitespace (GskCssTokenReader *reader,
                                      GskCssToken       *token)
{
  do {
    gsk_css_token_reader_consume_whitespace (reader);
  } while (reader->data != reader->end &&
           is_whitespace (*reader->data));

  gsk_css_token_init (token, GSK_CSS_TOKEN_WHITESPACE);
}

static gunichar 
gsk_css_token_reader_read_escape (GskCssTokenReader *reader)
{
  gunichar value = 0;
  guint i;

  gsk_css_token_reader_consume (reader, 1, 1);

  for (i = 0; i < 6 && reader->data < reader->end && g_ascii_isxdigit (*reader->data); i++)
    {
      value = value * 16 + g_ascii_xdigit_value (*reader->data);
      gsk_css_token_reader_consume (reader, 1, 1);
    }

  if (i == 0)
    return 0xFFFD;

  if (reader->data < reader->end && is_whitespace (*reader->data))
    gsk_css_token_reader_consume_whitespace (reader);

  return value;
}

static char *
gsk_css_token_reader_read_name (GskCssTokenReader *reader)
{
  GString *string = g_string_new (NULL);

  do {
      if (*reader->data == '\\')
        {
          if (gsk_css_token_reader_has_valid_escape (reader))
            {
              gunichar value = gsk_css_token_reader_read_escape (reader);

              if (value > 0 ||
                  (value >= 0xD800 && value <= 0xDFFF) ||
                  value <= 0x10FFFF)
                g_string_append_unichar (string, value);
              else
                g_string_append_unichar (string, 0xFFFD);
            }
          else
            {
              gsk_css_token_reader_consume_ascii (reader);

              if (reader->data == reader->end)
                {
                  g_string_append_unichar (string, 0xFFFD);
                  break;
                }
              
              gsk_css_token_reader_consume_char (reader, string);
            }
        }
      else if (is_name (*reader->data))
        {
          gsk_css_token_reader_consume_char (reader, string);
        }
      else
        {
          break;
        }
    }
  while (reader->data != reader->end);

  return g_string_free (string, FALSE);
}

static void
gsk_css_token_reader_read_bad_url (GskCssTokenReader *reader,
                                   GskCssToken       *token)
{
  while (reader->data < reader->end && *reader->data != ')')
    {
      if (gsk_css_token_reader_has_valid_escape (reader))
        gsk_css_token_reader_read_escape (reader);
      else
        gsk_css_token_reader_consume_char (reader, NULL);
    }
  
  if (reader->data < reader->end)
    gsk_css_token_reader_consume_ascii (reader);

  gsk_css_token_init (token, GSK_CSS_TOKEN_BAD_URL);
}

static void
gsk_css_token_reader_read_url (GskCssTokenReader  *reader,
                               GskCssToken        *token,
                               GError            **error)
{
  GString *url = g_string_new (NULL);

  while (reader->data < reader->end && is_whitespace (*reader->data))
    gsk_css_token_reader_consume_whitespace (reader);

  while (reader->data < reader->end)
    {
      if (*reader->data == ')')
        {
          gsk_css_token_reader_consume_ascii (reader);
          break;
        }
      else if (is_whitespace (*reader->data))
        {
          do
            gsk_css_token_reader_consume_whitespace (reader);
          while (reader->data < reader->end && is_whitespace (*reader->data));
          
          if (*reader->data == ')')
            {
              gsk_css_token_reader_consume_ascii (reader);
              break;
            }
          else if (reader->data >= reader->end)
            {
              break;
            }
          else
            {
              gsk_css_token_reader_read_bad_url (reader, token);
              set_parse_error (error, "Whitespace only allowed at start and end of url");
              return;
            }
        }
      else if (is_non_printable (*reader->data))
        {
          gsk_css_token_reader_read_bad_url (reader, token);
          g_string_free (url, TRUE);
          set_parse_error (error, "Nonprintable character 0x%02X in url", *reader->data);
          return;
        }
      else if (*reader->data == '"' ||
               *reader->data == '\'' ||
               *reader->data == '(')
        {
          gsk_css_token_reader_read_bad_url (reader, token);
          set_parse_error (error, "Invalid character %c in url", *reader->data);
          g_string_free (url, TRUE);
          return;
        }
      else if (gsk_css_token_reader_has_valid_escape (reader))
        {
          g_string_append_unichar (url, gsk_css_token_reader_read_escape (reader));
        }
      else if (*reader->data == '\\')
        {
          gsk_css_token_reader_read_bad_url (reader, token);
          set_parse_error (error, "Newline may not follow '\' escape character");
          g_string_free (url, TRUE);
          return;
        }
      else
        {
          gsk_css_token_reader_consume_char (reader, url);
        }
    }

  gsk_css_token_init (token, GSK_CSS_TOKEN_URL, g_string_free (url, FALSE));
}

static void
gsk_css_token_reader_read_ident_like (GskCssTokenReader  *reader,
                                      GskCssToken        *token,
                                      GError            **error)
{
  char *name = gsk_css_token_reader_read_name (reader);

  if (reader->data < reader->end &&
      *reader->data == '(')
    {
      gsk_css_token_reader_consume_ascii (reader);
      if (g_ascii_strcasecmp (name, "url") == 0)
        {
          const char *data = reader->data;

          while (is_whitespace (*data))
            data++;

          if (*data != '"' && *data != '\'')
            {
              gsk_css_token_reader_read_url (reader, token, error);
              return;
            }
        }
      
      gsk_css_token_init (token, GSK_CSS_TOKEN_FUNCTION, name);
    }
  else
    {
      gsk_css_token_init (token, GSK_CSS_TOKEN_IDENT, name);
    }
}

static void
gsk_css_token_reader_read_numeric (GskCssTokenReader *reader,
                                   GskCssToken     *token)
{
  int sign = 1, exponent_sign = 1;
  gint64 integer, fractional = 0, fractional_length = 1, exponent = 0;
  gboolean is_int = TRUE, has_sign = FALSE;
  const char *data = reader->data;

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

  for (integer = 0; data < reader->end && g_ascii_isdigit (*data); data++)
    {
      /* check for overflow here? */
      integer = 10 * integer + g_ascii_digit_value (*data);
    }

  if (data + 1 < reader->end && *data == '.' && g_ascii_isdigit (data[1]))
    {
      is_int = FALSE;
      data++;

      fractional = g_ascii_digit_value (*data);
      fractional_length = 10;
      data++;

      while (data < reader->end && g_ascii_isdigit (*data))
        {
          if (fractional_length < G_MAXINT64 / 10)
            {
              fractional = 10 * fractional + g_ascii_digit_value (*data);
              fractional_length *= 10;
            }
          data++;
        }
    }

  if (data + 1 < reader->end && (*data == 'e' || *data == 'E') && 
      (g_ascii_isdigit (data[1]) || 
       (data + 2 < reader->end && (data[1] == '+' || data[2] == '-') && g_ascii_isdigit (data[2]))))
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

      while (data < reader->end && g_ascii_isdigit (*data))
        {
          exponent = 10 * exponent + g_ascii_digit_value (*data);
          data++;
        }
    }

  gsk_css_token_reader_consume (reader, data - reader->data, data - reader->data);

  if (gsk_css_token_reader_has_identifier (reader))
    {
      gsk_css_token_init (token,
                          is_int ? (has_sign ? GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION : GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)
                                 : GSK_CSS_TOKEN_DIMENSION,
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent),
                          gsk_css_token_reader_read_name (reader));
    }
  else if (gsk_css_token_reader_remaining (reader) > 0 && *reader->data == '%')
    {
      gsk_css_token_init (token,
                          GSK_CSS_TOKEN_PERCENTAGE,
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent));
      gsk_css_token_reader_consume_ascii (reader);
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
gsk_css_token_reader_read_delim (GskCssTokenReader *reader,
                                 GskCssToken       *token)
{
  gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, g_utf8_get_char (reader->data));
  gsk_css_token_reader_consume_char (reader, NULL);
}

static void
gsk_css_token_reader_read_dash (GskCssTokenReader  *reader,
                                GskCssToken        *token,
                                GError            **error)
{
  if (gsk_css_token_reader_remaining (reader) == 1)
    {
      gsk_css_token_reader_read_delim (reader, token);
    }
  else if (gsk_css_token_reader_has_number (reader))
    {
      gsk_css_token_reader_read_numeric (reader, token);
    }
  else if (gsk_css_token_reader_remaining (reader) >= 3 &&
           reader->data[1] == '-' &&
           reader->data[2] == '>')
    {
      gsk_css_token_init (token, GSK_CSS_TOKEN_CDC);
      gsk_css_token_reader_consume (reader, 3, 3);
    }
  else if (gsk_css_token_reader_has_identifier (reader))
    {
      gsk_css_token_reader_read_ident_like (reader, token, error);
    }
  else
    {
      gsk_css_token_reader_read_delim (reader, token);
    }
}

static void
gsk_css_token_reader_read_string (GskCssTokenReader  *reader,
                                  GskCssToken        *token,
                                  GError            **error)
{
  GString *string = g_string_new (NULL);
  char end = *reader->data;

  gsk_css_token_reader_consume_ascii (reader);

  while (reader->data < reader->end)
    {
      if (*reader->data == end)
        {
          gsk_css_token_reader_consume_ascii (reader);
          break;
        }
      else if (*reader->data == '\\')
        {
          if (gsk_css_token_reader_remaining (reader) == 1)
            {
              gsk_css_token_reader_consume_ascii (reader);
              break;
            }
          else if (is_newline (reader->data[1]))
            {
              gsk_css_token_reader_consume_ascii (reader);
              gsk_css_token_reader_consume_newline (reader);
            }
          else
            {
              g_string_append_unichar (string, gsk_css_token_reader_read_escape (reader));
            }
        }
      else if (is_newline (*reader->data))
        {
          g_string_free (string, TRUE);
          gsk_css_token_init (token, GSK_CSS_TOKEN_BAD_STRING);
          set_parse_error (error, "Newlines inside strings must be escaped");
          return;
        }
      else
        {
          gsk_css_token_reader_consume_char (reader, string);
        }
    }
  
  gsk_css_token_init (token, GSK_CSS_TOKEN_STRING, g_string_free (string, FALSE));
}

static void
gsk_css_token_reader_read_comment (GskCssTokenReader  *reader,
                                   GskCssToken        *token,
                                   GError            **error)
{
  gsk_css_token_reader_consume (reader, 2, 2);

  while (reader->data < reader->end)
    {
      if (gsk_css_token_reader_remaining (reader) > 1 &&
          reader->data[0] == '*' && reader->data[1] == '/')
        {
          gsk_css_token_reader_consume (reader, 2, 2);
          gsk_css_token_init (token, GSK_CSS_TOKEN_COMMENT);
          return;
        }
      gsk_css_token_reader_consume_char (reader, NULL);
    }

  gsk_css_token_init (token, GSK_CSS_TOKEN_COMMENT);
  set_parse_error (error, "Comment not terminated at end of document.");
}

static void
gsk_css_token_reader_read_match (GskCssTokenReader *reader,
                                 GskCssToken     *token,
                                 GskCssTokenType  type)
{
  if (gsk_css_token_reader_remaining (reader) > 1 && reader->data[1] == '=')
    {
      gsk_css_token_init (token, type);
      gsk_css_token_reader_consume (reader, 2, 2);
    }
  else
    {
      gsk_css_token_reader_read_delim (reader, token);
    }
}

void
gsk_css_tokenizer_read_token (GskCssTokenizer  *tokenizer,
                              GskCssToken      *token)
{
  GskCssTokenReader reader;
  GError *error = NULL;

  if (tokenizer->reader.data == tokenizer->reader.end)
    {
      gsk_css_token_init (token, GSK_CSS_TOKEN_EOF);
      return;
    }

  gsk_css_token_reader_init_copy (&reader, &tokenizer->reader);

  if (reader.data[0] == '/' && gsk_css_token_reader_remaining (&reader) > 1 &&
      reader.data[1] == '*')
    {
      gsk_css_token_reader_read_comment (&reader, token, &error);
      goto out;
    }

  switch (*reader.data)
    {
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case ' ':
      gsk_css_token_reader_read_whitespace (&reader, token);
      break;

    case '"':
      gsk_css_token_reader_read_string (&reader, token, &error);
      break;

    case '#':
      gsk_css_token_reader_consume_ascii (&reader);
      if (is_name (*reader.data) || gsk_css_token_reader_has_valid_escape (&reader))
        {
          GskCssTokenType type;

          if (gsk_css_token_reader_has_identifier (&reader))
            type = GSK_CSS_TOKEN_HASH_ID;
          else
            type = GSK_CSS_TOKEN_HASH_UNRESTRICTED;

          gsk_css_token_init (token,
                              type,
                              gsk_css_token_reader_read_name (&reader));
        }
      else
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, '#');
        }
      break;

    case '$':
      gsk_css_token_reader_read_match (&reader, token, GSK_CSS_TOKEN_SUFFIX_MATCH);
      break;

    case '\'':
      gsk_css_token_reader_read_string (&reader, token, &error);
      break;

    case '(':
      gsk_css_token_init (token, GSK_CSS_TOKEN_OPEN_PARENS);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case ')':
      gsk_css_token_init (token, GSK_CSS_TOKEN_CLOSE_PARENS);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '*':
      gsk_css_token_reader_read_match (&reader, token, GSK_CSS_TOKEN_SUBSTRING_MATCH);
      break;

    case '+':
      if (gsk_css_token_reader_has_number (&reader))
        gsk_css_token_reader_read_numeric (&reader, token);
      else
        gsk_css_token_reader_read_delim (&reader, token);
      break;

    case ',':
      gsk_css_token_init (token, GSK_CSS_TOKEN_COMMA);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '-':
      gsk_css_token_reader_read_dash (&reader, token, &error);
      break;

    case '.':
      if (gsk_css_token_reader_has_number (&reader))
        gsk_css_token_reader_read_numeric (&reader, token);
      else
        gsk_css_token_reader_read_delim (&reader, token);
      break;

    case ':':
      gsk_css_token_init (token, GSK_CSS_TOKEN_COLON);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case ';':
      gsk_css_token_init (token, GSK_CSS_TOKEN_SEMICOLON);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '<':
      if (gsk_css_token_reader_remaining (&reader) >= 4 &&
          reader.data[1] == '!' &&
          reader.data[2] == '-' &&
          reader.data[3] == '-')
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_CDO);
          gsk_css_token_reader_consume (&reader, 3, 3);
        }
      else
        {
          gsk_css_token_reader_read_delim (&reader, token);
        }
      break;

    case '@':
      gsk_css_token_reader_consume_ascii (&reader);
      if (gsk_css_token_reader_has_identifier (&reader))
        {
          gsk_css_token_init (token,
                              GSK_CSS_TOKEN_AT_KEYWORD,
                              gsk_css_token_reader_read_name (&reader));
        }
      else
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, '@');
        }
      break;

    case '[':
      gsk_css_token_init (token, GSK_CSS_TOKEN_OPEN_SQUARE);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '\\':
      if (gsk_css_token_reader_has_valid_escape (&reader))
        {
          gsk_css_token_reader_read_ident_like (&reader, token, &error);
        }
      else
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_DELIM, '\\');
          set_parse_error (&error, "Newline may not follow '\' escape character");
        }
      break;

    case ']':
      gsk_css_token_init (token, GSK_CSS_TOKEN_CLOSE_SQUARE);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '^':
      gsk_css_token_reader_read_match (&reader, token, GSK_CSS_TOKEN_PREFIX_MATCH);
      break;

    case '{':
      gsk_css_token_init (token, GSK_CSS_TOKEN_OPEN_CURLY);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '}':
      gsk_css_token_init (token, GSK_CSS_TOKEN_CLOSE_CURLY);
      gsk_css_token_reader_consume_ascii (&reader);
      break;

    case '|':
      if (gsk_css_token_reader_remaining (&reader) > 1 && reader.data[1] == '|')
        {
          gsk_css_token_init (token, GSK_CSS_TOKEN_COLUMN);
          gsk_css_token_reader_consume (&reader, 2, 2);
        }
      else
        {
          gsk_css_token_reader_read_match (&reader, token, GSK_CSS_TOKEN_DASH_MATCH);
        }
      break;

    case '~':
      gsk_css_token_reader_read_match (&reader, token, GSK_CSS_TOKEN_INCLUDE_MATCH);
      break;

    default:
      if (g_ascii_isdigit (*reader.data))
        {
          gsk_css_token_reader_read_numeric (&reader, token);
        }
      else if (is_name_start (*reader.data))
        {
          gsk_css_token_reader_read_ident_like (&reader, token, &error);
        }
      else
        gsk_css_token_reader_read_delim (&reader, token);
      break;
    }

out:
  if (error != NULL)
    {
      GskCssLocation error_location;

      gsk_css_location_init_copy (&error_location, &reader.position);
      gsk_css_token_reader_init_copy (&tokenizer->reader, &reader);
      gsk_css_tokenizer_emit_error (tokenizer, &error_location, token, error);
      g_error_free (error);
    }
  else
    {
      gsk_css_token_reader_init_copy (&tokenizer->reader, &reader);
    }
}

