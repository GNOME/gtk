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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcsstokenizerprivate.h"

/* for error enum */
#include "gtkcssprovider.h"

#include <math.h>
#include <string.h>

typedef struct _GtkCssTokenReader GtkCssTokenReader;

struct _GtkCssTokenReader {
  const char *           data;
  const char *           end;

  GtkCssLocation         position;
};

struct _GtkCssTokenizer
{
  gint                   ref_count;
  GBytes                *bytes;
  GtkCssTokenizerErrorFunc error_func;
  gpointer               user_data;
  GDestroyNotify         user_destroy;

  GtkCssTokenReader      reader;
};

static void
gtk_css_location_init (GtkCssLocation *location)
{
  memset (location, 0, sizeof (GtkCssLocation));
}

static void
gtk_css_location_advance (GtkCssLocation *location,
                          gsize           bytes,
                          gsize           chars)
{
  location->bytes += bytes;
  location->chars += chars;
  location->line_bytes += bytes;
  location->line_chars += chars;
}

static void
gtk_css_location_advance_newline (GtkCssLocation *location,
                                  gboolean        is_windows)
{
  gtk_css_location_advance (location, is_windows ? 2 : 1, is_windows ? 2 : 1);

  location->lines++;
  location->line_bytes = 0;
  location->line_chars = 0;
}

void
gtk_css_token_clear (GtkCssToken *token)
{
  switch (token->type)
    {
    case GTK_CSS_TOKEN_STRING:
    case GTK_CSS_TOKEN_IDENT:
    case GTK_CSS_TOKEN_FUNCTION:
    case GTK_CSS_TOKEN_AT_KEYWORD:
    case GTK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GTK_CSS_TOKEN_HASH_ID:
    case GTK_CSS_TOKEN_URL:
      g_free (token->string.string);
      break;

    case GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_DIMENSION:
      g_free (token->dimension.dimension);
      break;

    default:
      g_assert_not_reached ();
    case GTK_CSS_TOKEN_EOF:
    case GTK_CSS_TOKEN_WHITESPACE:
    case GTK_CSS_TOKEN_OPEN_PARENS:
    case GTK_CSS_TOKEN_CLOSE_PARENS:
    case GTK_CSS_TOKEN_OPEN_SQUARE:
    case GTK_CSS_TOKEN_CLOSE_SQUARE:
    case GTK_CSS_TOKEN_OPEN_CURLY:
    case GTK_CSS_TOKEN_CLOSE_CURLY:
    case GTK_CSS_TOKEN_COMMA:
    case GTK_CSS_TOKEN_COLON:
    case GTK_CSS_TOKEN_SEMICOLON:
    case GTK_CSS_TOKEN_CDC:
    case GTK_CSS_TOKEN_CDO:
    case GTK_CSS_TOKEN_DELIM:
    case GTK_CSS_TOKEN_SIGNED_INTEGER:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GTK_CSS_TOKEN_SIGNED_NUMBER:
    case GTK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GTK_CSS_TOKEN_PERCENTAGE:
    case GTK_CSS_TOKEN_INCLUDE_MATCH:
    case GTK_CSS_TOKEN_DASH_MATCH:
    case GTK_CSS_TOKEN_PREFIX_MATCH:
    case GTK_CSS_TOKEN_SUFFIX_MATCH:
    case GTK_CSS_TOKEN_SUBSTRING_MATCH:
    case GTK_CSS_TOKEN_COLUMN:
    case GTK_CSS_TOKEN_BAD_STRING:
    case GTK_CSS_TOKEN_BAD_URL:
    case GTK_CSS_TOKEN_COMMENT:
      break;
    }

  token->type = GTK_CSS_TOKEN_EOF;
}

static void
gtk_css_token_initv (GtkCssToken     *token,
                     GtkCssTokenType  type,
                     va_list          args)
{
  token->type = type;

  switch (type)
    {
    case GTK_CSS_TOKEN_STRING:
    case GTK_CSS_TOKEN_IDENT:
    case GTK_CSS_TOKEN_FUNCTION:
    case GTK_CSS_TOKEN_AT_KEYWORD:
    case GTK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GTK_CSS_TOKEN_HASH_ID:
    case GTK_CSS_TOKEN_URL:
      token->string.string = va_arg (args, char *);
      break;

    case GTK_CSS_TOKEN_DELIM:
      token->delim.delim = va_arg (args, gunichar);
      break;

    case GTK_CSS_TOKEN_SIGNED_INTEGER:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GTK_CSS_TOKEN_SIGNED_NUMBER:
    case GTK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GTK_CSS_TOKEN_PERCENTAGE:
      token->number.number = va_arg (args, double);
      break;

    case GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_DIMENSION:
      token->dimension.value = va_arg (args, double);
      token->dimension.dimension = va_arg (args, char *);
      break;

    default:
      g_assert_not_reached ();
    case GTK_CSS_TOKEN_EOF:
    case GTK_CSS_TOKEN_WHITESPACE:
    case GTK_CSS_TOKEN_OPEN_PARENS:
    case GTK_CSS_TOKEN_CLOSE_PARENS:
    case GTK_CSS_TOKEN_OPEN_SQUARE:
    case GTK_CSS_TOKEN_CLOSE_SQUARE:
    case GTK_CSS_TOKEN_OPEN_CURLY:
    case GTK_CSS_TOKEN_CLOSE_CURLY:
    case GTK_CSS_TOKEN_COMMA:
    case GTK_CSS_TOKEN_COLON:
    case GTK_CSS_TOKEN_SEMICOLON:
    case GTK_CSS_TOKEN_CDC:
    case GTK_CSS_TOKEN_CDO:
    case GTK_CSS_TOKEN_INCLUDE_MATCH:
    case GTK_CSS_TOKEN_DASH_MATCH:
    case GTK_CSS_TOKEN_PREFIX_MATCH:
    case GTK_CSS_TOKEN_SUFFIX_MATCH:
    case GTK_CSS_TOKEN_SUBSTRING_MATCH:
    case GTK_CSS_TOKEN_COLUMN:
    case GTK_CSS_TOKEN_BAD_STRING:
    case GTK_CSS_TOKEN_BAD_URL:
    case GTK_CSS_TOKEN_COMMENT:
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
 * gtk_css_token_is_finite:
 * @token: a #GtkCssToken
 *
 * A token is considered finite when it would stay the same no matter
 * what bytes follow it in the data stream.
 *
 * An obvious example for this is the ';' token.
 *
 * Returns: %TRUE if the token is considered finite.
 **/
gboolean
gtk_css_token_is_finite (const GtkCssToken *token)
{
  switch (token->type)
    {
    case GTK_CSS_TOKEN_EOF:
    case GTK_CSS_TOKEN_STRING:
    case GTK_CSS_TOKEN_FUNCTION:
    case GTK_CSS_TOKEN_URL:
    case GTK_CSS_TOKEN_PERCENTAGE:
    case GTK_CSS_TOKEN_OPEN_PARENS:
    case GTK_CSS_TOKEN_CLOSE_PARENS:
    case GTK_CSS_TOKEN_OPEN_SQUARE:
    case GTK_CSS_TOKEN_CLOSE_SQUARE:
    case GTK_CSS_TOKEN_OPEN_CURLY:
    case GTK_CSS_TOKEN_CLOSE_CURLY:
    case GTK_CSS_TOKEN_COMMA:
    case GTK_CSS_TOKEN_COLON:
    case GTK_CSS_TOKEN_SEMICOLON:
    case GTK_CSS_TOKEN_CDC:
    case GTK_CSS_TOKEN_CDO:
    case GTK_CSS_TOKEN_INCLUDE_MATCH:
    case GTK_CSS_TOKEN_DASH_MATCH:
    case GTK_CSS_TOKEN_PREFIX_MATCH:
    case GTK_CSS_TOKEN_SUFFIX_MATCH:
    case GTK_CSS_TOKEN_SUBSTRING_MATCH:
    case GTK_CSS_TOKEN_COLUMN:
    case GTK_CSS_TOKEN_COMMENT:
      return TRUE;

    default:
      g_assert_not_reached ();
    case GTK_CSS_TOKEN_WHITESPACE:
    case GTK_CSS_TOKEN_IDENT:
    case GTK_CSS_TOKEN_AT_KEYWORD:
    case GTK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GTK_CSS_TOKEN_HASH_ID:
    case GTK_CSS_TOKEN_DELIM:
    case GTK_CSS_TOKEN_SIGNED_INTEGER:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GTK_CSS_TOKEN_SIGNED_NUMBER:
    case GTK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GTK_CSS_TOKEN_BAD_STRING:
    case GTK_CSS_TOKEN_BAD_URL:
    case GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_DIMENSION:
      return FALSE;
    }
}

gboolean
gtk_css_token_is_ident (const GtkCssToken *token,
                        const char        *ident)
{
  return gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT)
      && (g_ascii_strcasecmp (token->string.string, ident) == 0);
}

gboolean
gtk_css_token_is_function (const GtkCssToken *token,
                           const char        *ident)
{
  return gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION)
      && (g_ascii_strcasecmp (token->string.string, ident) == 0);
}

gboolean
gtk_css_token_is_delim (const GtkCssToken *token,
                        gunichar           delim)
{
  return gtk_css_token_is (token, GTK_CSS_TOKEN_DELIM)
      && token->delim.delim == delim;
}

void
gtk_css_token_print (const GtkCssToken *token,
                     GString           *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (token->type)
    {
    case GTK_CSS_TOKEN_STRING:
      append_string (string, token->string.string);
      break;

    case GTK_CSS_TOKEN_IDENT:
      append_ident (string, token->string.string);
      break;

    case GTK_CSS_TOKEN_URL:
      g_string_append (string, "url(");
      append_ident (string, token->string.string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_TOKEN_FUNCTION:
      append_ident (string, token->string.string);
      g_string_append_c (string, '(');
      break;

    case GTK_CSS_TOKEN_AT_KEYWORD:
      g_string_append_c (string, '@');
      append_ident (string, token->string.string);
      break;

    case GTK_CSS_TOKEN_HASH_UNRESTRICTED:
    case GTK_CSS_TOKEN_HASH_ID:
      g_string_append_c (string, '#');
      append_ident (string, token->string.string);
      break;

    case GTK_CSS_TOKEN_DELIM:
      g_string_append_unichar (string, token->delim.delim);
      break;

    case GTK_CSS_TOKEN_SIGNED_INTEGER:
    case GTK_CSS_TOKEN_SIGNED_NUMBER:
      if (token->number.number >= 0)
        g_string_append_c (string, '+');
      /* fall through */
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GTK_CSS_TOKEN_SIGNLESS_NUMBER:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->number.number);
      g_string_append (string, buf);
      break;

    case GTK_CSS_TOKEN_PERCENTAGE:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->number.number);
      g_string_append (string, buf);
      g_string_append_c (string, '%');
      break;

    case GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
      if (token->dimension.value >= 0)
        g_string_append_c (string, '+');
      /* fall through */
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_DIMENSION:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->dimension.value);
      g_string_append (string, buf);
      append_ident (string, token->dimension.dimension);
      break;

    case GTK_CSS_TOKEN_EOF:
      break;

    case GTK_CSS_TOKEN_WHITESPACE:
      g_string_append (string, " ");
      break;

    case GTK_CSS_TOKEN_OPEN_PARENS:
      g_string_append (string, "(");
      break;

    case GTK_CSS_TOKEN_CLOSE_PARENS:
      g_string_append (string, ")");
      break;

    case GTK_CSS_TOKEN_OPEN_SQUARE:
      g_string_append (string, "[");
      break;

    case GTK_CSS_TOKEN_CLOSE_SQUARE:
      g_string_append (string, "]");
      break;

    case GTK_CSS_TOKEN_OPEN_CURLY:
      g_string_append (string, "{");
      break;

    case GTK_CSS_TOKEN_CLOSE_CURLY:
      g_string_append (string, "}");
      break;

    case GTK_CSS_TOKEN_COMMA:
      g_string_append (string, ",");
      break;

    case GTK_CSS_TOKEN_COLON:
      g_string_append (string, ":");
      break;

    case GTK_CSS_TOKEN_SEMICOLON:
      g_string_append (string, ";");
      break;

    case GTK_CSS_TOKEN_CDO:
      g_string_append (string, "<!--");
      break;

    case GTK_CSS_TOKEN_CDC:
      g_string_append (string, "-->");
      break;

    case GTK_CSS_TOKEN_INCLUDE_MATCH:
      g_string_append (string, "~=");
      break;

    case GTK_CSS_TOKEN_DASH_MATCH:
      g_string_append (string, "|=");
      break;

    case GTK_CSS_TOKEN_PREFIX_MATCH:
      g_string_append (string, "^=");
      break;

    case GTK_CSS_TOKEN_SUFFIX_MATCH:
      g_string_append (string, "$=");
      break;

    case GTK_CSS_TOKEN_SUBSTRING_MATCH:
      g_string_append (string, "*=");
      break;

    case GTK_CSS_TOKEN_COLUMN:
      g_string_append (string, "||");
      break;

    case GTK_CSS_TOKEN_BAD_STRING:
      g_string_append (string, "\"\n");
      break;

    case GTK_CSS_TOKEN_BAD_URL:
      g_string_append (string, "url(bad url)");
      break;

    case GTK_CSS_TOKEN_COMMENT:
      g_string_append (string, "/* comment */");
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

char *
gtk_css_token_to_string (const GtkCssToken *token)
{
  GString *string;

  string = g_string_new (NULL);
  gtk_css_token_print (token, string);
  return g_string_free (string, FALSE);
}

static void
gtk_css_token_init (GtkCssToken     *token,
                    GtkCssTokenType  type,
                    ...)
{
  va_list args;

  va_start (args, type);
  gtk_css_token_initv (token, type, args);
  va_end (args);
}

static void
gtk_css_token_reader_init (GtkCssTokenReader *reader,
                           GBytes            *bytes)
{
  reader->data = g_bytes_get_data (bytes, NULL);
  reader->end = reader->data + g_bytes_get_size (bytes);

  gtk_css_location_init (&reader->position);
}

static void
gtk_css_token_reader_init_copy (GtkCssTokenReader       *reader,
                                const GtkCssTokenReader *source)
{
  *reader = *source;
}

GtkCssTokenizer *
gtk_css_tokenizer_new (GBytes                   *bytes,
                       GtkCssTokenizerErrorFunc  func,
                       gpointer                  user_data,
                       GDestroyNotify            user_destroy)
{
  GtkCssTokenizer *tokenizer;

  tokenizer = g_slice_new0 (GtkCssTokenizer);
  tokenizer->ref_count = 1;
  tokenizer->bytes = g_bytes_ref (bytes);
  tokenizer->error_func = func;
  tokenizer->user_data = user_data;
  tokenizer->user_destroy = user_destroy;

  gtk_css_token_reader_init (&tokenizer->reader, bytes);

  return tokenizer;
}

GtkCssTokenizer *
gtk_css_tokenizer_ref (GtkCssTokenizer *tokenizer)
{
  tokenizer->ref_count++;
  
  return tokenizer;
}

void
gtk_css_tokenizer_unref (GtkCssTokenizer *tokenizer)
{
  tokenizer->ref_count--;
  if (tokenizer->ref_count > 0)
    return;

  if (tokenizer->user_destroy)
    tokenizer->user_destroy (tokenizer->user_data);

  g_bytes_unref (tokenizer->bytes);
  g_slice_free (GtkCssTokenizer, tokenizer);
}

const GtkCssLocation *
gtk_css_tokenizer_get_location (GtkCssTokenizer *tokenizer)
{
  return &tokenizer->reader.position;
}

static void
gtk_css_tokenizer_parse_error (GtkCssTokenizer   *tokenizer,
                               const GtkCssToken *token,
                               const char        *format,
                               ...) G_GNUC_PRINTF(3, 4);
static void
gtk_css_tokenizer_parse_error (GtkCssTokenizer   *tokenizer,
                               const GtkCssToken *token,
                               const char        *format,
                               ...)
{
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_SYNTAX,
                              format, args);
  va_end (args);

  if (tokenizer->error_func)
    tokenizer->error_func (tokenizer, token, error, tokenizer->user_data);
  else
    g_print ("error: %s\n", error->message);

  g_error_free (error);
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
gtk_css_token_reader_remaining (const GtkCssTokenReader *reader)
{
  return reader->end - reader->data;
}

static gboolean
gtk_css_token_reader_has_valid_escape (const GtkCssTokenReader *reader)
{
  switch (gtk_css_token_reader_remaining (reader))
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
gtk_css_token_reader_has_identifier (const GtkCssTokenReader *reader)
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
gtk_css_token_reader_has_number (const GtkCssTokenReader *reader)
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
gtk_css_token_reader_consume_newline (GtkCssTokenReader *reader)
{
  gsize n;

  if (gtk_css_token_reader_remaining (reader) > 1 &&
      reader->data[0] == '\r' && reader->data[1] == '\n')
    n = 2;
  else
    n = 1;
  
  reader->data += n;
  gtk_css_location_advance_newline (&reader->position, n == 2 ? TRUE : FALSE);
}

static inline void
gtk_css_token_reader_consume (GtkCssTokenReader *reader,
                              gsize              n_bytes,
                              gsize              n_characters)
{
  /* NB: must not contain newlines! */
  reader->data += n_bytes;

  gtk_css_location_advance (&reader->position, n_bytes, n_characters);
}

static inline void
gtk_css_token_reader_consume_ascii (GtkCssTokenReader *reader)
{
  /* NB: must not contain newlines! */
  gtk_css_token_reader_consume (reader, 1, 1);
}

static inline void
gtk_css_token_reader_consume_whitespace (GtkCssTokenReader *reader)
{
  if (is_newline (*reader->data))
    gtk_css_token_reader_consume_newline (reader);
  else
    gtk_css_token_reader_consume_ascii (reader);
}

static inline void
gtk_css_token_reader_consume_char (GtkCssTokenReader *reader,
                                   GString           *string)
{
  if (is_newline (*reader->data))
    gtk_css_token_reader_consume_newline (reader);
  else
    {
      gsize char_size = g_utf8_next_char (reader->data) - reader->data;

      if (string)
        g_string_append_len (string, reader->data, char_size);
      gtk_css_token_reader_consume (reader, char_size, 1);
    }
}

static void
gtk_css_token_reader_read_whitespace (GtkCssTokenReader *reader,
                                      GtkCssToken       *token)
{
  do {
    gtk_css_token_reader_consume_whitespace (reader);
  } while (reader->data != reader->end &&
           is_whitespace (*reader->data));

  gtk_css_token_init (token, GTK_CSS_TOKEN_WHITESPACE);
}

static gunichar 
gtk_css_token_reader_read_escape (GtkCssTokenReader *reader)
{
  gunichar value = 0;
  guint i;

  gtk_css_token_reader_consume (reader, 1, 1);

  for (i = 0; i < 6 && reader->data < reader->end && g_ascii_isxdigit (*reader->data); i++)
    {
      value = value * 16 + g_ascii_xdigit_value (*reader->data);
      gtk_css_token_reader_consume (reader, 1, 1);
    }

  if (i == 0)
    return 0xFFFD;

  if (reader->data < reader->end && is_whitespace (*reader->data))
    gtk_css_token_reader_consume_whitespace (reader);

  return value;
}

static char *
gtk_css_token_reader_read_name (GtkCssTokenReader *reader)
{
  GString *string = g_string_new (NULL);

  do {
      if (*reader->data == '\\')
        {
          if (gtk_css_token_reader_has_valid_escape (reader))
            {
              gunichar value = gtk_css_token_reader_read_escape (reader);

              if (value > 0 ||
                  (value >= 0xD800 && value <= 0xDFFF) ||
                  value <= 0x10FFFF)
                g_string_append_unichar (string, value);
              else
                g_string_append_unichar (string, 0xFFFD);
            }
          else
            {
              gtk_css_token_reader_consume_ascii (reader);

              if (reader->data == reader->end)
                {
                  g_string_append_unichar (string, 0xFFFD);
                  break;
                }
              
              gtk_css_token_reader_consume_char (reader, string);
            }
        }
      else if (is_name (*reader->data))
        {
          gtk_css_token_reader_consume_char (reader, string);
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
gtk_css_token_reader_read_bad_url (GtkCssTokenReader *reader,
                                   GtkCssToken       *token)
{
  while (reader->data < reader->end && *reader->data != ')')
    {
      if (gtk_css_token_reader_has_valid_escape (reader))
        gtk_css_token_reader_read_escape (reader);
      else
        gtk_css_token_reader_consume_char (reader, NULL);
    }
  
  if (reader->data < reader->end)
    gtk_css_token_reader_consume_ascii (reader);

  gtk_css_token_init (token, GTK_CSS_TOKEN_BAD_URL);
}

static void
gtk_css_token_reader_read_url (GtkCssTokenizer   *tokenizer,
                               GtkCssTokenReader *reader,
                               GtkCssToken       *token)
{
  GString *url = g_string_new (NULL);

  while (reader->data < reader->end && is_whitespace (*reader->data))
    gtk_css_token_reader_consume_whitespace (reader);

  while (reader->data < reader->end)
    {
      if (*reader->data == ')')
        {
          gtk_css_token_reader_consume_ascii (reader);
          break;
        }
      else if (is_whitespace (*reader->data))
        {
          do
            gtk_css_token_reader_consume_whitespace (reader);
          while (reader->data < reader->end && is_whitespace (*reader->data));
          
          if (*reader->data == ')')
            {
              gtk_css_token_reader_consume_ascii (reader);
              break;
            }
          else if (reader->data >= reader->end)
            {
              break;
            }
          else
            {
              gtk_css_token_reader_read_bad_url (reader, token);
              gtk_css_tokenizer_parse_error (tokenizer, token, "Whitespace only allowed at start and end of url");
              return;
            }
        }
      else if (is_non_printable (*reader->data))
        {
          gtk_css_token_reader_read_bad_url (reader, token);
          g_string_free (url, TRUE);
          gtk_css_tokenizer_parse_error (tokenizer, token, "Nonprintable character 0x%02X in url", *reader->data);
          return;
        }
      else if (*reader->data == '"' ||
               *reader->data == '\'' ||
               *reader->data == '(')
        {
          gtk_css_token_reader_read_bad_url (reader, token);
          gtk_css_tokenizer_parse_error (tokenizer, token, "Invalid character %c in url", *reader->data);
          g_string_free (url, TRUE);
          return;
        }
      else if (gtk_css_token_reader_has_valid_escape (reader))
        {
          g_string_append_unichar (url, gtk_css_token_reader_read_escape (reader));
        }
      else if (*reader->data == '\\')
        {
          gtk_css_token_reader_read_bad_url (reader, token);
          gtk_css_tokenizer_parse_error (tokenizer, token, "Newline may not follow '\' escape character");
          g_string_free (url, TRUE);
          return;
        }
      else
        {
          gtk_css_token_reader_consume_char (reader, url);
        }
    }

  gtk_css_token_init (token, GTK_CSS_TOKEN_URL, g_string_free (url, FALSE));
}

static void
gtk_css_token_reader_read_ident_like (GtkCssTokenizer   *tokenizer,
                                      GtkCssTokenReader *reader,
                                      GtkCssToken       *token)
{
  char *name = gtk_css_token_reader_read_name (reader);

  if (reader->data < reader->end &&
      *reader->data == '(')
    {
      gtk_css_token_reader_consume_ascii (reader);
      if (g_ascii_strcasecmp (name, "url") == 0)
        {
          const char *data = reader->data;

          while (is_whitespace (*data))
            data++;

          if (*data != '"' && *data != '\'')
            {
              gtk_css_token_reader_read_url (tokenizer, reader, token);
              return;
            }
        }
      
      gtk_css_token_init (token, GTK_CSS_TOKEN_FUNCTION, name);
    }
  else
    {
      gtk_css_token_init (token, GTK_CSS_TOKEN_IDENT, name);
    }
}

static void
gtk_css_token_reader_read_numeric (GtkCssTokenReader *reader,
                                   GtkCssToken     *token)
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

  gtk_css_token_reader_consume (reader, data - reader->data, data - reader->data);

  if (gtk_css_token_reader_has_identifier (reader))
    {
      gtk_css_token_init (token,
                          is_int ? (has_sign ? GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION : GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)
                                 : GTK_CSS_TOKEN_DIMENSION,
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent),
                          gtk_css_token_reader_read_name (reader));
    }
  else if (gtk_css_token_reader_remaining (reader) > 0 && *reader->data == '%')
    {
      gtk_css_token_init (token,
                          GTK_CSS_TOKEN_PERCENTAGE,
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent));
      gtk_css_token_reader_consume_ascii (reader);
    }
  else
    {
      gtk_css_token_init (token,
                          is_int ? (has_sign ? GTK_CSS_TOKEN_SIGNED_INTEGER : GTK_CSS_TOKEN_SIGNLESS_INTEGER)
                                 : (has_sign ? GTK_CSS_TOKEN_SIGNED_NUMBER : GTK_CSS_TOKEN_SIGNLESS_NUMBER),
                          sign * (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent));
    }
}

static void
gtk_css_token_reader_read_delim (GtkCssTokenReader *reader,
                                 GtkCssToken       *token)
{
  gtk_css_token_init (token, GTK_CSS_TOKEN_DELIM, g_utf8_get_char (reader->data));
  gtk_css_token_reader_consume_char (reader, NULL);
}

static void
gtk_css_token_reader_read_dash (GtkCssTokenizer   *tokenizer,
                                GtkCssTokenReader *reader,
                                GtkCssToken       *token)
{
  if (gtk_css_token_reader_remaining (reader) == 1)
    {
      gtk_css_token_reader_read_delim (reader, token);
    }
  else if (gtk_css_token_reader_has_number (reader))
    {
      gtk_css_token_reader_read_numeric (reader, token);
    }
  else if (gtk_css_token_reader_remaining (reader) >= 3 &&
           reader->data[1] == '-' &&
           reader->data[2] == '>')
    {
      gtk_css_token_init (token, GTK_CSS_TOKEN_CDC);
      gtk_css_token_reader_consume (reader, 3, 3);
    }
  else if (gtk_css_token_reader_has_identifier (reader))
    {
      gtk_css_token_reader_read_ident_like (tokenizer, reader, token);
    }
  else
    {
      gtk_css_token_reader_read_delim (reader, token);
    }
}

static void
gtk_css_token_reader_read_string (GtkCssTokenizer   *tokenizer,
                                  GtkCssTokenReader *reader,
                                  GtkCssToken       *token)
{
  GString *string = g_string_new (NULL);
  char end = *reader->data;

  gtk_css_token_reader_consume_ascii (reader);

  while (reader->data < reader->end)
    {
      if (*reader->data == end)
        {
          gtk_css_token_reader_consume_ascii (reader);
          break;
        }
      else if (*reader->data == '\\')
        {
          if (gtk_css_token_reader_remaining (reader) == 1)
            {
              gtk_css_token_reader_consume_ascii (reader);
              break;
            }
          else if (is_newline (reader->data[1]))
            {
              gtk_css_token_reader_consume_ascii (reader);
              gtk_css_token_reader_consume_newline (reader);
            }
          else
            {
              g_string_append_unichar (string, gtk_css_token_reader_read_escape (reader));
            }
        }
      else if (is_newline (*reader->data))
        {
          g_string_free (string, TRUE);
          gtk_css_token_init (token, GTK_CSS_TOKEN_BAD_STRING);
          gtk_css_tokenizer_parse_error (tokenizer, token, "Newlines inside strings must be escaped");
          return;
        }
      else
        {
          gtk_css_token_reader_consume_char (reader, string);
        }
    }
  
  gtk_css_token_init (token, GTK_CSS_TOKEN_STRING, g_string_free (string, FALSE));
}

static void
gtk_css_token_reader_read_comment (GtkCssTokenizer   *tokenizer,
                                   GtkCssTokenReader *reader,
                                   GtkCssToken       *token)
{
  gtk_css_token_reader_consume (reader, 2, 2);

  while (reader->data < reader->end)
    {
      if (gtk_css_token_reader_remaining (reader) > 1 &&
          reader->data[0] == '*' && reader->data[1] == '/')
        {
          gtk_css_token_reader_consume (reader, 2, 2);
          gtk_css_token_init (token, GTK_CSS_TOKEN_COMMENT);
          return;
        }
      gtk_css_token_reader_consume_char (reader, NULL);
    }

  gtk_css_token_init (token, GTK_CSS_TOKEN_COMMENT);
  gtk_css_tokenizer_parse_error (tokenizer, token, "Comment not terminated at end of document.");
}

static void
gtk_css_token_reader_read_match (GtkCssTokenReader *reader,
                                 GtkCssToken     *token,
                                 GtkCssTokenType  type)
{
  if (gtk_css_token_reader_remaining (reader) > 1 && reader->data[1] == '=')
    {
      gtk_css_token_init (token, type);
      gtk_css_token_reader_consume (reader, 2, 2);
    }
  else
    {
      gtk_css_token_reader_read_delim (reader, token);
    }
}

void
gtk_css_tokenizer_read_token (GtkCssTokenizer  *tokenizer,
                              GtkCssToken      *token)
{
  GtkCssTokenReader reader;

  if (tokenizer->reader.data == tokenizer->reader.end)
    {
      gtk_css_token_init (token, GTK_CSS_TOKEN_EOF);
      return;
    }

  gtk_css_token_reader_init_copy (&reader, &tokenizer->reader);

  if (reader.data[0] == '/' && gtk_css_token_reader_remaining (&reader) > 1 &&
      reader.data[1] == '*')
    {
      gtk_css_token_reader_read_comment (tokenizer, &reader, token);
      goto out;
    }

  switch (*reader.data)
    {
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case ' ':
      gtk_css_token_reader_read_whitespace (&reader, token);
      break;

    case '"':
      gtk_css_token_reader_read_string (tokenizer, &reader, token);
      break;

    case '#':
      gtk_css_token_reader_consume_ascii (&reader);
      if (is_name (*reader.data) || gtk_css_token_reader_has_valid_escape (&reader))
        {
          GtkCssTokenType type;

          if (gtk_css_token_reader_has_identifier (&reader))
            type = GTK_CSS_TOKEN_HASH_ID;
          else
            type = GTK_CSS_TOKEN_HASH_UNRESTRICTED;

          gtk_css_token_init (token,
                              type,
                              gtk_css_token_reader_read_name (&reader));
        }
      else
        {
          gtk_css_token_init (token, GTK_CSS_TOKEN_DELIM, '#');
        }
      break;

    case '$':
      gtk_css_token_reader_read_match (&reader, token, GTK_CSS_TOKEN_SUFFIX_MATCH);
      break;

    case '\'':
      gtk_css_token_reader_read_string (tokenizer, &reader, token);
      break;

    case '(':
      gtk_css_token_init (token, GTK_CSS_TOKEN_OPEN_PARENS);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case ')':
      gtk_css_token_init (token, GTK_CSS_TOKEN_CLOSE_PARENS);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '*':
      gtk_css_token_reader_read_match (&reader, token, GTK_CSS_TOKEN_SUBSTRING_MATCH);
      break;

    case '+':
      if (gtk_css_token_reader_has_number (&reader))
        gtk_css_token_reader_read_numeric (&reader, token);
      else
        gtk_css_token_reader_read_delim (&reader, token);
      break;

    case ',':
      gtk_css_token_init (token, GTK_CSS_TOKEN_COMMA);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '-':
      gtk_css_token_reader_read_dash (tokenizer, &reader, token);
      break;

    case '.':
      if (gtk_css_token_reader_has_number (&reader))
        gtk_css_token_reader_read_numeric (&reader, token);
      else
        gtk_css_token_reader_read_delim (&reader, token);
      break;

    case ':':
      gtk_css_token_init (token, GTK_CSS_TOKEN_COLON);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case ';':
      gtk_css_token_init (token, GTK_CSS_TOKEN_SEMICOLON);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '<':
      if (gtk_css_token_reader_remaining (&reader) >= 4 &&
          reader.data[1] == '!' &&
          reader.data[2] == '-' &&
          reader.data[3] == '-')
        {
          gtk_css_token_init (token, GTK_CSS_TOKEN_CDO);
          gtk_css_token_reader_consume (&reader, 3, 3);
        }
      else
        {
          gtk_css_token_reader_read_delim (&reader, token);
        }
      break;

    case '@':
      gtk_css_token_reader_consume_ascii (&reader);
      if (gtk_css_token_reader_has_identifier (&reader))
        {
          gtk_css_token_init (token,
                              GTK_CSS_TOKEN_AT_KEYWORD,
                              gtk_css_token_reader_read_name (&reader));
        }
      else
        {
          gtk_css_token_init (token, GTK_CSS_TOKEN_DELIM, '@');
        }
      break;

    case '[':
      gtk_css_token_init (token, GTK_CSS_TOKEN_OPEN_SQUARE);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '\\':
      if (gtk_css_token_reader_has_valid_escape (&reader))
        {
          gtk_css_token_reader_read_ident_like (tokenizer, &reader, token);
        }
      else
        {
          gtk_css_token_init (token, GTK_CSS_TOKEN_DELIM, '\\');
          gtk_css_tokenizer_parse_error (tokenizer, token, "Newline may not follow '\' escape character");
        }
      break;

    case ']':
      gtk_css_token_init (token, GTK_CSS_TOKEN_CLOSE_SQUARE);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '^':
      gtk_css_token_reader_read_match (&reader, token, GTK_CSS_TOKEN_PREFIX_MATCH);
      break;

    case '{':
      gtk_css_token_init (token, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '}':
      gtk_css_token_init (token, GTK_CSS_TOKEN_CLOSE_CURLY);
      gtk_css_token_reader_consume_ascii (&reader);
      break;

    case '|':
      if (gtk_css_token_reader_remaining (&reader) > 1 && reader.data[1] == '|')
        {
          gtk_css_token_init (token, GTK_CSS_TOKEN_COLUMN);
          gtk_css_token_reader_consume (&reader, 2, 2);
        }
      else
        {
          gtk_css_token_reader_read_match (&reader, token, GTK_CSS_TOKEN_DASH_MATCH);
        }
      break;

    case '~':
      gtk_css_token_reader_read_match (&reader, token, GTK_CSS_TOKEN_INCLUDE_MATCH);
      break;

    default:
      if (g_ascii_isdigit (*reader.data))
        {
          gtk_css_token_reader_read_numeric (&reader, token);
        }
      else if (is_name_start (*reader.data))
        {
          gtk_css_token_reader_read_ident_like (tokenizer, &reader, token);
        }
      else
        gtk_css_token_reader_read_delim (&reader, token);
      break;
    }

out:
  gtk_css_token_reader_init_copy (&tokenizer->reader, &reader);
}

