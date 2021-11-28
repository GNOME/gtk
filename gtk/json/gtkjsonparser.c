/*
 * Copyright © 2021 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#include "config.h"

#include "gtkjsonparserprivate.h"

typedef struct _GtkJsonBlock GtkJsonBlock;
typedef struct _GtkJsonReader GtkJsonReader;

struct _GtkJsonReader
{
  const guchar *data;
  const guchar *end;
};

typedef enum {
  GTK_JSON_BLOCK_TOPLEVEL,
  GTK_JSON_BLOCK_OBJECT,
  GTK_JSON_BLOCK_ARRAY,
} GtkJsonBlockType;

struct _GtkJsonBlock
{
  GtkJsonBlockType type;
  const guchar *value; /* start of current value to be consumed by external code */
  const guchar *member_name; /* name of current value, only used for object types */
  gsize index; /* index of the current element */
};

struct _GtkJsonParser
{
  GBytes *bytes;
  GtkJsonReader reader; /* current read head, pointing as far as we've read */

  GError *error; /* if an error has happened, it's stored here. Errors aren't recoverable. */

  GtkJsonBlock *block; /* current block */
  GtkJsonBlock *blocks; /* blocks array */
  GtkJsonBlock *blocks_end; /* blocks array */
  GtkJsonBlock blocks_preallocated[128]; /* preallocated */
};

static void
gtk_json_set_syntax_error (GError     **error,
                           const char  *format,
                           ...) G_GNUC_PRINTF(2, 3);
static void
gtk_json_set_syntax_error (GError     **error,
                           const char  *format,
                           ...)
{
  va_list args;

  if (error == NULL)
    return;

  va_start (args, format);
  *error = g_error_new_valist (G_FILE_ERROR,
                               G_FILE_ERROR_FAILED,
                               format, args);
  va_end (args);
}

static void
gtk_json_parser_value_error (GtkJsonParser *self,
                             const char  *format,
                             ...) G_GNUC_PRINTF(2, 3);
static void
gtk_json_parser_value_error (GtkJsonParser *self,
                             const char  *format,
                             ...)
{
  va_list args;

  if (self->error)
    return;

  va_start (args, format);
  self->error = g_error_new_valist (G_FILE_ERROR,
                                    G_FILE_ERROR_FAILED,
                                    format, args);
  va_end (args);
}

static void
gtk_json_parser_syntax_error (GtkJsonParser *self,
                              const char  *format,
                              ...) G_GNUC_PRINTF(2, 3);
static void
gtk_json_parser_syntax_error (GtkJsonParser *self,
                              const char  *format,
                              ...)
{
  va_list args;

  if (self->error)
    return;

  va_start (args, format);
  self->error = g_error_new_valist (G_FILE_ERROR,
                                    G_FILE_ERROR_FAILED,
                                    format, args);
  va_end (args);
}

static void
gtk_json_reader_init (GtkJsonReader *reader,
                      const guchar  *data,
                      gsize          size)
{
  reader->data = data;
  reader->end = data + size;
}

static gboolean
gtk_json_reader_is_eof (GtkJsonReader *reader)
{
  return reader->data >= reader->end || *reader->data == '\0';
}

static gsize
gtk_json_reader_remaining (GtkJsonReader *reader)
{
  g_return_val_if_fail (reader->data <= reader->end, 0);

  return reader->end - reader->data;
}

static void
gtk_json_reader_skip_whitespace (GtkJsonReader *reader)
{
  while (reader->data < reader->end)
    {
      switch (*reader->data)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
          reader->data++;
          break;
        default:
          return;
        }
    }
}

static gboolean
gtk_json_reader_has_char (GtkJsonReader *reader,
                          char           c)
{
  return gtk_json_reader_remaining (reader) && *reader->data == c;
}

static gboolean
gtk_json_reader_try_char (GtkJsonReader *reader,
                          char           c)
{
  if (!gtk_json_reader_has_char (reader, c))
    return FALSE;

  reader->data++;
  return TRUE;
}

static gboolean
gtk_json_reader_try_identifier_len (GtkJsonReader *reader,
                                    const char    *ident,
                                    gsize          len)
{
  if (gtk_json_reader_remaining (reader) < len)
    return FALSE;

  if (memcmp (reader->data, ident, len) != 0)
    return FALSE;

  reader->data += len;
  return TRUE;
}

#define gtk_json_reader_try_identifier(reader, ident) gtk_json_reader_try_identifier_len(reader, ident, strlen(ident))

/*
 * decode_utf16_surrogate_pair:
 * @first: the first UTF-16 code point
 * @second: the second UTF-16 code point
 *
 * Decodes a surrogate pair of UTF-16 code points into the equivalent
 * Unicode code point.
 *
 * If the code points are not valid, 0 is returned.
 *
 * Returns: the Unicode code point equivalent to the surrogate pair
 */
static inline gunichar
decode_utf16_surrogate_pair (gunichar first,
                             gunichar second)
{
  if (0xd800 > first || first > 0xdbff ||
      0xdc00 > second || second > 0xdfff)
    return 0;

  return 0x10000
       | (first & 0x3ff) << 10
       | (second & 0x3ff);
}

static gboolean
gtk_json_reader_parse_string (GtkJsonReader  *reader,
                              char          **out_string,
                              GError        **error)
{
  GString *string = NULL;
  const guchar *last;

  if (!gtk_json_reader_try_char (reader, '"'))
    {
      gtk_json_set_syntax_error (error, "Not a string");
      return FALSE;
    }

  last = reader->data;

  while (gtk_json_reader_remaining (reader))
    {
      switch (*reader->data)
      {
        case '\0':
          goto end;

        case '"':
          if (!g_utf8_validate ((const char *) last, reader->data - last, (const char **) &reader->data))
            {
              if (string)
                g_string_free (string, TRUE);
              gtk_json_set_syntax_error (error, "Invalid UTF-8");
              return FALSE;
            }
          if (out_string)
            {
              if (string)
                {
                  g_string_append_len (string, (const char *) last, reader->data - last);
                  *out_string = g_string_free (string, FALSE);
                }
              else
                *out_string = g_strndup ((const char *) last, reader->data - last);
            }
          reader->data++;
          return TRUE;

        case '\\':
          if (!g_utf8_validate ((const char *) last, reader->data - last, (const char **) &reader->data))
            {
              if (string)
                g_string_free (string, TRUE);
              gtk_json_set_syntax_error (error, "Invalid UTF-8");
              return FALSE;
            }
          if (gtk_json_reader_remaining (reader) < 2)
            goto end;
          if (out_string)
            {
              if (!string)
                string = g_string_new (NULL);
              g_string_append_len (string, (const char *) last, reader->data - last);
            }
          reader->data++;
          switch (*reader->data)
            {
            case '"':
            case '\\':
            case '/':
              if (string)
                g_string_append_c (string, *reader->data);
              break;
            case 'b':
              if (string)
                g_string_append_c (string, '\b');
              break;
            case 'f':
              if (string)
                g_string_append_c (string, '\f');
              break;
            case 'n':
              if (string)
                g_string_append_c (string, '\n');
              break;
            case 'r':
              if (string)
                g_string_append_c (string, '\r');
              break;
            case 't':
              if (string)
                g_string_append_c (string, '\t');
              break;
            case 'u':
              if (gtk_json_reader_remaining (reader) < 5 ||
                  !g_ascii_isxdigit (reader->data[1]) ||
                  !g_ascii_isxdigit (reader->data[2]) ||
                  !g_ascii_isxdigit (reader->data[3]) ||
                  !g_ascii_isxdigit (reader->data[4]))
                {
                  if (string)
                    g_string_free (string, TRUE);
                  gtk_json_set_syntax_error (error, "Invalid Unicode escape sequence");
                  return FALSE;
                }
              else
                {
                  gunichar unichar = (g_ascii_xdigit_value (reader->data[1]) << 12) |
                                     (g_ascii_xdigit_value (reader->data[2]) <<  8) |
                                     (g_ascii_xdigit_value (reader->data[3]) <<  4) |
                                     (g_ascii_xdigit_value (reader->data[4]));

                  reader->data += 4;
                  /* resolve UTF-16 surrogates for Unicode characters not in the BMP,
                   * as per ECMA 404, § 9, "String"
                   */
                  if (g_unichar_type (unichar) == G_UNICODE_SURROGATE &&
                      gtk_json_reader_remaining (reader) >= 7 &&
                      reader->data[1] == '\\' &&
                      reader->data[2] == 'u' &&
                      g_ascii_isxdigit (reader->data[3]) &&
                      g_ascii_isxdigit (reader->data[4]) &&
                      g_ascii_isxdigit (reader->data[5]) &&
                      g_ascii_isxdigit (reader->data[6]))
                    {
                      unichar = decode_utf16_surrogate_pair (unichar,
                                                             (g_ascii_xdigit_value (reader->data[3]) << 12) |
                                                             (g_ascii_xdigit_value (reader->data[4]) <<  8) |
                                                             (g_ascii_xdigit_value (reader->data[5]) <<  4) |
                                                             (g_ascii_xdigit_value (reader->data[6])));
                      reader->data += 6;

                      if (unichar == 0)
                        {
                          if (string)
                            g_string_free (string, TRUE);
                          gtk_json_set_syntax_error (error, "Invalid UTF-16 surrogate pair");
                          return FALSE;
                        }
                    }
                  if (string)
                    g_string_append_unichar (string, unichar);
                }
              break;
            default:
              if (string)
                g_string_free (string, TRUE);
              gtk_json_set_syntax_error (error, "Unknown escape sequence");
              return FALSE;
            }
          last = reader->data + 1;
          break;

        default:
          if (*reader->data < 0x20)
            {
              if (string)
                g_string_free (string, TRUE);
              gtk_json_set_syntax_error (error, "Disallowed control character in string literal");
              return FALSE;
            }
          break;
      }
      reader->data++;
    }

end:
  if (string)
    g_string_free (string, TRUE);
  gtk_json_set_syntax_error (error, "Unterminated string literal");
  return FALSE;
}

static gboolean
gtk_json_reader_parse_number (GtkJsonReader  *reader,
                              GError        **error)
{
  /* sign */
  gtk_json_reader_try_char (reader, '-');

  /* integer part */
  if (!gtk_json_reader_try_char (reader, '0'))
    {
      if (reader->data >= reader->end ||
          !g_ascii_isdigit (*reader->data))
        goto out;

      reader->data++;

      while (reader->data < reader->end && g_ascii_isdigit (*reader->data))
        reader->data++;
    }

  /* fractional part */
  if (gtk_json_reader_remaining (reader) >= 2 && *reader->data == '.' && g_ascii_isdigit (reader->data[1]))
    {
      reader->data += 2;

      while (reader->data < reader->end && g_ascii_isdigit (*reader->data))
        reader->data++;
    }

  /* exponent */
  if (gtk_json_reader_remaining (reader) >= 2 && (reader->data[0] == 'e' || reader->data[0] == 'E') &&
      (g_ascii_isdigit (reader->data[1]) ||
       (gtk_json_reader_remaining (reader) >= 3 && (reader->data[1] == '+' || reader->data[1] == '-') && g_ascii_isdigit (reader->data[2]))))
    {
      reader->data += 2;

      while (reader->data < reader->end && g_ascii_isdigit (*reader->data))
        reader->data++;
    }
  return TRUE;

out:
  gtk_json_set_syntax_error (error, "Not a valid number");
  return FALSE;
}

static void
gtk_json_parser_push_block (GtkJsonParser    *self,
                            GtkJsonBlockType  type)
{
  self->block++;
  if (self->block == self->blocks_end)
    {
      gsize old_size = self->blocks_end - self->blocks;
      gsize new_size = old_size + 128;

      if (self->blocks == self->blocks_preallocated)
        {
          self->blocks = g_new (GtkJsonBlock, new_size);
          memcpy (self->blocks, self->blocks_preallocated, sizeof (GtkJsonBlock) * G_N_ELEMENTS (self->blocks_preallocated));
        }
      else
        {
          self->blocks = g_renew (GtkJsonBlock, self->blocks, new_size);
        }
      self->blocks_end = self->blocks + new_size;
      self->block = self->blocks + old_size;
    }

  self->block->type = type;
  self->block->member_name = 0;
  self->block->value = 0;
  self->block->index = 0;
}
                     
static void
gtk_json_parser_pop_block (GtkJsonParser *self)
{
  g_assert (self->block > self->blocks);
  self->block--;
}

GtkJsonParser *
gtk_json_parser_new_for_string (const char *string,
                                gssize      size)
{
  GtkJsonParser *self;
  GBytes *bytes;

  bytes = g_bytes_new (string, size >= 0 ? size : strlen (string));

  self = gtk_json_parser_new_for_bytes (bytes);

  g_bytes_unref (bytes);

  return self;
}

GtkJsonParser *
gtk_json_parser_new_for_bytes (GBytes *bytes)
{
  GtkJsonParser *self;
  const guchar *data;
  gsize size;

  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_slice_new0 (GtkJsonParser);

  self->bytes = g_bytes_ref (bytes);
  data = g_bytes_get_data (bytes, &size);
  gtk_json_reader_init (&self->reader, data, size);

  self->blocks = self->blocks_preallocated;
  self->blocks_end = self->blocks + G_N_ELEMENTS (self->blocks_preallocated);
  self->block = self->blocks;
  self->block->type = GTK_JSON_BLOCK_TOPLEVEL;

  gtk_json_reader_skip_whitespace (&self->reader);
  self->block->value = self->reader.data;
  if (!gtk_json_parser_get_node (self))
    {
      gtk_json_parser_syntax_error (self, "Empty document");
    }

  return self;
}

void
gtk_json_parser_free (GtkJsonParser *self)
{
  if (self == NULL)
    return;

  g_bytes_unref (self->bytes);

  if (self->blocks != self->blocks_preallocated)
    g_free (self->blocks);

  if (self->error)
    g_error_free (self->error);

  g_slice_free (GtkJsonParser, self);
}

static gboolean
gtk_json_parser_has_skipped_value (GtkJsonParser *self)
{
  return self->reader.data != self->block->value;
}

static gboolean
gtk_json_parser_skip_value (GtkJsonParser *self)
{
  /* read the value already */
  if (gtk_json_parser_has_skipped_value (self))
    return TRUE;

  if (gtk_json_reader_is_eof (&self->reader))
    {
      gtk_json_parser_syntax_error (self, "Unexpected end of file");
      return FALSE;
    }

  switch (*self->reader.data)
  {
    case '"':
      return gtk_json_reader_parse_string (&self->reader, NULL, &self->error);
    
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return gtk_json_reader_parse_number (&self->reader, &self->error);

    case 'f':
      if (gtk_json_reader_try_identifier (&self->reader, "false"))
        return TRUE;
      break;

    case 'n':
      if (gtk_json_reader_try_identifier (&self->reader, "null"))
        return TRUE;
      break;

    case 't':
      if (gtk_json_reader_try_identifier (&self->reader, "true"))
        return TRUE;
      break;

    case '{':
      return gtk_json_parser_start_object (self) &&
             gtk_json_parser_end (self);

    case '[':
      return gtk_json_parser_start_array (self) &&
             gtk_json_parser_end (self);

    default:
      break;
  }

  gtk_json_parser_syntax_error (self, "Unexpected character in document");
  return FALSE;
}

gboolean
gtk_json_parser_next (GtkJsonParser *self)
{
  if (self->error)
    return FALSE;

  if (!gtk_json_parser_skip_value (self))
    return FALSE;

  switch (self->block->type)
    {
    case GTK_JSON_BLOCK_TOPLEVEL:
      gtk_json_reader_skip_whitespace (&self->reader);
      if (gtk_json_reader_is_eof (&self->reader))
        {
          self->block->value = NULL;
          if (gtk_json_parser_remaining (self))
            {
              gtk_json_parser_syntax_error (self, "Unexpected nul byte in document");
            }
        }
      else
        {
          gtk_json_parser_syntax_error (self, "Data at end of document");
        }
      return FALSE;

    case GTK_JSON_BLOCK_OBJECT:
      gtk_json_reader_skip_whitespace (&self->reader);
      if (gtk_json_reader_is_eof (&self->reader))
        {
          gtk_json_parser_syntax_error (self, "Unexpected end of document");
          self->block->member_name = NULL;
          self->block->value = NULL;
        }
      if (gtk_json_reader_has_char (&self->reader, '}'))
        {
          self->block->member_name = NULL;
          self->block->value = NULL;
          return FALSE;
        }
      if (!gtk_json_reader_try_char (&self->reader, ','))
        {
          gtk_json_parser_syntax_error (self, "Expected a ',' to separate object members");
          return FALSE;
        }
      gtk_json_reader_skip_whitespace (&self->reader);
      self->block->member_name = self->reader.data;

      if (!gtk_json_reader_parse_string (&self->reader, NULL, &self->error))
        return FALSE;
      gtk_json_reader_skip_whitespace (&self->reader);
      if (!gtk_json_reader_try_char (&self->reader, ':'))
        {
          gtk_json_parser_syntax_error (self, "Missing ':' after member name");
          return FALSE;
        }

      gtk_json_reader_skip_whitespace (&self->reader);
      self->block->value = self->reader.data;
      if (!gtk_json_parser_get_node (self))
        {
          gtk_json_parser_syntax_error (self, "No value after ','");
          return FALSE;
        }
      break;

    case GTK_JSON_BLOCK_ARRAY:
      gtk_json_reader_skip_whitespace (&self->reader);
      if (gtk_json_reader_is_eof (&self->reader))
        {
          gtk_json_parser_syntax_error (self, "Unexpected end of document");
          self->block->member_name = NULL;
          self->block->value = NULL;
        }
      if (gtk_json_reader_has_char (&self->reader, ']'))
        {
          self->block->value = NULL;
          return FALSE;
        }

      if (!gtk_json_reader_try_char (&self->reader, ','))
        {
          gtk_json_parser_syntax_error (self, "Expected a ',' to separate array members");
          return FALSE;
        }

      gtk_json_reader_skip_whitespace (&self->reader);
      self->block->value = self->reader.data;
      if (!gtk_json_parser_get_node (self))
        {
          gtk_json_parser_syntax_error (self, "No value after ','");
          return FALSE;
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

GtkJsonNode
gtk_json_parser_get_node (GtkJsonParser *self)
{
  if (self->error)
    return GTK_JSON_NONE;

  if (self->block->value == NULL)
    return GTK_JSON_NONE;

  switch (*self->block->value)
    {
    case '"':
      return GTK_JSON_STRING;
    
    case '{':
      return GTK_JSON_OBJECT;

    case '[':
      return GTK_JSON_ARRAY;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return GTK_JSON_NUMBER;

    case 't':
    case 'f':
      return GTK_JSON_BOOLEAN;

    case 'n':
      return GTK_JSON_NULL;

    default:
      return GTK_JSON_NONE;
    }
}

const GError *
gtk_json_parser_get_error (GtkJsonParser *self)
{
  return self->error;
}

char *
gtk_json_parser_get_member_name (GtkJsonParser *self)
{
  GtkJsonReader reader;
  char *result;

  if (self->error)
    return NULL;

  if (self->block->type != GTK_JSON_BLOCK_OBJECT)
    return NULL;

  if (self->block->member_name == NULL)
    return NULL;

  gtk_json_reader_init (&reader,
                        self->block->member_name,
                        self->reader.end - self->block->member_name);
  if (!gtk_json_reader_parse_string (&reader, &result, NULL))
    return NULL;

  return result;
}

gboolean
gtk_json_parser_get_boolean (GtkJsonParser *self)
{
  if (self->error)
    return FALSE;

  if (self->block->value == NULL)
    return FALSE;

  if (!gtk_json_parser_skip_value (self))
    return FALSE;

  if (*self->block->value == 't')
    return TRUE;
  else if (*self->block->value == 'f')
    return FALSE;

  gtk_json_parser_value_error (self, "Expected a boolean value");
  return FALSE;
}

double
gtk_json_parser_get_number (GtkJsonParser *self)
{
  double result;

  if (self->error)
    return 0;

  if (self->block->value == NULL)
    return 0;

  if (!gtk_json_parser_skip_value (self))
    return FALSE;

  if (!strchr ("-0123456789", *self->block->value))
    {
      gtk_json_parser_value_error (self, "Expected a number");
      return 0;
    }

  errno = 0;
  if (gtk_json_reader_remaining (&self->reader) == 0)
    {
      /* need terminated string here */
      char *s = g_strndup ((const char *) self->block->value, self->reader.data - self->block->value);
      result = g_ascii_strtod (s, NULL);
      g_free (s);
    }
  else
    {
      result = g_ascii_strtod ((const char *) self->block->value, NULL);
    }

  if (errno)
    {
      if (errno == ERANGE)
        gtk_json_parser_value_error (self, "Number out of range");
      else
        gtk_json_parser_value_error (self, "%s", g_strerror (errno));

      return 0;
    }

  return result;
}

#if 0
int                     gtk_json_parser_get_int                 (GtkJsonParser          *self);
guint                   gtk_json_parser_get_uint                (GtkJsonParser          *self);
#endif

char *
gtk_json_parser_get_string (GtkJsonParser *self)
{
  char *result;

  if (self->error)
    return g_strdup ("");

  if (self->block->value == NULL)
    return g_strdup ("");

  if (gtk_json_parser_has_skipped_value (self))
    {
      GtkJsonReader reader;
      gtk_json_reader_init (&reader, self->block->value, self->reader.end - self->block->value);
      if (!gtk_json_reader_parse_string (&reader, &result, NULL))
        return g_strdup ("");
    }
  else
    {
      if (!gtk_json_reader_parse_string (&self->reader, &result, &self->error))
        return g_strdup ("");
    }

  return result;
}

gboolean
gtk_json_parser_start_object (GtkJsonParser *self)
{
  if (self->error)
    return FALSE;

  if (!gtk_json_reader_try_char (&self->reader, '{'))
    {
      gtk_json_parser_value_error (self, "Expected an object");
      return FALSE;
    }

  gtk_json_parser_push_block (self, GTK_JSON_BLOCK_OBJECT);

  gtk_json_reader_skip_whitespace (&self->reader);
  if (gtk_json_reader_is_eof (&self->reader))
    {
      gtk_json_parser_syntax_error (self, "Unexpected end of document");
      return FALSE;
    }
  if (gtk_json_reader_has_char (&self->reader, '}'))
    return TRUE;
  self->block->member_name = self->reader.data;

  if (!gtk_json_reader_parse_string (&self->reader, NULL, &self->error))
    return FALSE;
  gtk_json_reader_skip_whitespace (&self->reader);
  if (!gtk_json_reader_try_char (&self->reader, ':'))
    {
      gtk_json_parser_syntax_error (self, "Missing ':' after member name");
      return FALSE;
    }

  gtk_json_reader_skip_whitespace (&self->reader);
  self->block->value = self->reader.data;

  return TRUE;
}

gboolean
gtk_json_parser_start_array (GtkJsonParser *self)
{
  if (self->error)
    return FALSE;

  if (!gtk_json_reader_try_char (&self->reader, '['))
    {
      gtk_json_parser_value_error (self, "Expected an array");
      return FALSE;
    }

  gtk_json_parser_push_block (self, GTK_JSON_BLOCK_ARRAY);
  gtk_json_reader_skip_whitespace (&self->reader);
  if (gtk_json_reader_is_eof (&self->reader))
    {
      gtk_json_parser_syntax_error (self, "Unexpected end of document");
      return FALSE;
    }
  if (gtk_json_reader_has_char (&self->reader, ']'))
    {
      self->block->value = NULL;
      return TRUE;
    }
  self->block->value = self->reader.data;

  return TRUE;
}

gboolean
gtk_json_parser_end (GtkJsonParser *self)
{
  char bracket;

  if (self->error)
    return FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  switch (self->block->type)
    {
    case GTK_JSON_BLOCK_OBJECT:
      bracket = '}';
      break;
    case GTK_JSON_BLOCK_ARRAY:
      bracket = ']';
      break;
    case GTK_JSON_BLOCK_TOPLEVEL:
    default:
      g_return_val_if_reached (FALSE);
    }

  while (!gtk_json_reader_try_char (&self->reader, bracket))
    {
      if (!gtk_json_parser_next (self))
        return FALSE;
    }

  gtk_json_parser_pop_block (self);

  return TRUE;
}

