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

typedef enum {
  WHITESPACE     = (1 << 0),
  STRING_ELEMENT = (1 << 1),
  STRING_MARKER  = (1 << 2),
} JsonCharacterType;

static const guchar json_character_table[256] = {
  ['\t'] = WHITESPACE,
  ['\r'] = WHITESPACE,
  ['\n'] = WHITESPACE,
  [' ']  = WHITESPACE | STRING_ELEMENT,
  [' ']  = STRING_ELEMENT,
  ['!']  = STRING_ELEMENT,
  ['"']  = STRING_MARKER,
  ['#']  = STRING_ELEMENT,
  ['$']  = STRING_ELEMENT,
  ['%']  = STRING_ELEMENT,
  ['&']  = STRING_ELEMENT,
  ['\''] = STRING_ELEMENT,
  ['(']  = STRING_ELEMENT,
  [')']  = STRING_ELEMENT,
  ['*']  = STRING_ELEMENT,
  ['+']  = STRING_ELEMENT,
  [',']  = STRING_ELEMENT,
  ['-']  = STRING_ELEMENT,
  ['.']  = STRING_ELEMENT,
  ['/']  = STRING_ELEMENT,
  ['0']  = STRING_ELEMENT,
  ['1']  = STRING_ELEMENT,
  ['2']  = STRING_ELEMENT,
  ['3']  = STRING_ELEMENT,
  ['4']  = STRING_ELEMENT,
  ['5']  = STRING_ELEMENT,
  ['6']  = STRING_ELEMENT,
  ['7']  = STRING_ELEMENT,
  ['8']  = STRING_ELEMENT,
  ['9']  = STRING_ELEMENT,
  [':']  = STRING_ELEMENT,
  [';']  = STRING_ELEMENT,
  ['<']  = STRING_ELEMENT,
  ['=']  = STRING_ELEMENT,
  ['>']  = STRING_ELEMENT,
  ['?']  = STRING_ELEMENT,
  ['@']  = STRING_ELEMENT,
  ['A']  = STRING_ELEMENT,
  ['B']  = STRING_ELEMENT,
  ['C']  = STRING_ELEMENT,
  ['D']  = STRING_ELEMENT,
  ['E']  = STRING_ELEMENT,
  ['F']  = STRING_ELEMENT,
  ['G']  = STRING_ELEMENT,
  ['H']  = STRING_ELEMENT,
  ['I']  = STRING_ELEMENT,
  ['J']  = STRING_ELEMENT,
  ['K']  = STRING_ELEMENT,
  ['L']  = STRING_ELEMENT,
  ['M']  = STRING_ELEMENT,
  ['N']  = STRING_ELEMENT,
  ['O']  = STRING_ELEMENT,
  ['P']  = STRING_ELEMENT,
  ['Q']  = STRING_ELEMENT,
  ['R']  = STRING_ELEMENT,
  ['S']  = STRING_ELEMENT,
  ['T']  = STRING_ELEMENT,
  ['U']  = STRING_ELEMENT,
  ['V']  = STRING_ELEMENT,
  ['W']  = STRING_ELEMENT,
  ['X']  = STRING_ELEMENT,
  ['Y']  = STRING_ELEMENT,
  ['Z']  = STRING_ELEMENT,
  ['[']  = STRING_ELEMENT,
  ['\\'] = STRING_MARKER,
  [']']  = STRING_ELEMENT,
  ['^']  = STRING_ELEMENT,
  ['_']  = STRING_ELEMENT,
  ['`']  = STRING_ELEMENT,
  ['a']  = STRING_ELEMENT,
  ['b']  = STRING_ELEMENT,
  ['c']  = STRING_ELEMENT,
  ['d']  = STRING_ELEMENT,
  ['e']  = STRING_ELEMENT,
  ['f']  = STRING_ELEMENT,
  ['g']  = STRING_ELEMENT,
  ['h']  = STRING_ELEMENT,
  ['i']  = STRING_ELEMENT,
  ['j']  = STRING_ELEMENT,
  ['k']  = STRING_ELEMENT,
  ['l']  = STRING_ELEMENT,
  ['m']  = STRING_ELEMENT,
  ['n']  = STRING_ELEMENT,
  ['o']  = STRING_ELEMENT,
  ['p']  = STRING_ELEMENT,
  ['q']  = STRING_ELEMENT,
  ['r']  = STRING_ELEMENT,
  ['s']  = STRING_ELEMENT,
  ['t']  = STRING_ELEMENT,
  ['u']  = STRING_ELEMENT,
  ['v']  = STRING_ELEMENT,
  ['w']  = STRING_ELEMENT,
  ['x']  = STRING_ELEMENT,
  ['y']  = STRING_ELEMENT,
  ['z']  = STRING_ELEMENT,
  ['{']  = STRING_ELEMENT,
  ['|']  = STRING_ELEMENT,
  ['}']  = STRING_ELEMENT,
  ['~']  = STRING_ELEMENT,
  [127]  = STRING_ELEMENT,
};

static const guchar *
json_skip_characters (const guchar      *start,
                      const guchar      *end,
                      JsonCharacterType  type)
{
  const guchar *s;

  for (s = start; s < end; s++)
    {
      if (!(json_character_table[*s] & type))
        break;
    }
  return s;
}

static const guchar *
json_find_character (const guchar      *start,
                     JsonCharacterType  type)
{
  const guchar *s;

  for (s = start; ; s++)
    {
      if ((json_character_table[*s] & type))
        break;
    }
  return s;
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
  reader->data = json_skip_characters (reader->data, reader->end, WHITESPACE);
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

static gsize
gtk_json_unescape_char (const guchar *json_escape,
                        char          out_data[6],
                        gsize        *out_len)
{
  switch (json_escape[1])
    {
    case '"':
    case '\\':
    case '/':
      out_data[0] = json_escape[1];
      *out_len = 1;
      return 2;
    case 'b':
      out_data[0] = '\b';
      *out_len = 1;
      return 2;
    case 'f':
      out_data[0] = '\f';
      *out_len = 1;
      return 2;
    case 'n':
      out_data[0] = '\n';
      *out_len = 1;
      return 2;
    case 'r':
      out_data[0] = '\r';
      *out_len = 1;
      return 2;
    case 't':
      out_data[0] = '\t';
      *out_len = 1;
      return 2;
    case 'u':
      {
        gunichar unichar = (g_ascii_xdigit_value (json_escape[2]) << 12) |
                           (g_ascii_xdigit_value (json_escape[3]) <<  8) |
                           (g_ascii_xdigit_value (json_escape[4]) <<  4) |
                           (g_ascii_xdigit_value (json_escape[5]));
        gsize result = 6;

        /* resolve UTF-16 surrogates for Unicode characters not in the BMP,
         * as per ECMA 404, § 9, "String"
         */
        if (g_unichar_type (unichar) == G_UNICODE_SURROGATE)
          {
            unichar = decode_utf16_surrogate_pair (unichar,
                                                   (g_ascii_xdigit_value (json_escape[8])  << 12) |
                                                   (g_ascii_xdigit_value (json_escape[9])  <<  8) |
                                                   (g_ascii_xdigit_value (json_escape[10]) <<  4) |
                                                   (g_ascii_xdigit_value (json_escape[11])));
            result += 6;
          }
        *out_len = g_unichar_to_utf8 (unichar, out_data);
        return result;
        }
    default:
      g_assert_not_reached ();
      return 0;
    }
}
                   
/* The escaped string MUST be valid json, so it must begin
 * with " and end with " and must not contain any invalid
 * escape codes.
 * This function is meant to be fast
 */
static char *
gtk_json_unescape_string (const guchar *escaped)
{
  char buf[6];
  gsize buf_size;
  GString *string;
  const guchar *last, *s;

  string = NULL;

  g_assert (*escaped == '"');
  last = escaped + 1;
  for (s = json_find_character (last, STRING_MARKER);
       *s != '"';
       s = json_find_character (last, STRING_MARKER))
    {
      g_assert (*s == '\\');
      if (string == NULL)
        string = g_string_new (NULL);
      g_string_append_len (string, (const char *) last, s - last);
      last = s + gtk_json_unescape_char (s, buf, &buf_size);
      g_string_append_len (string, buf, buf_size);
    }

  if (string)
    {
      g_string_append_len (string, (const char *) last, s - last);
      return g_string_free (string, FALSE);
    }
  else
    {
      return g_strndup ((const char *) last, s - last);
    }
}

static gboolean
gtk_json_parser_parse_string (GtkJsonParser *self)
{
  if (!gtk_json_reader_try_char (&self->reader, '"'))
    {
      gtk_json_parser_syntax_error (self, "Not a string");
      return FALSE;
    }

  self->reader.data = json_skip_characters (self->reader.data, self->reader.end, STRING_ELEMENT);

  while (gtk_json_reader_remaining (&self->reader))
    {
      if (*self->reader.data < 0x20)
        {
          gtk_json_parser_syntax_error (self, "Disallowed control character in string literal");
          return FALSE;
        }
      else if (*self->reader.data > 127)
        {
          gunichar c = g_utf8_get_char_validated ((const char *) self->reader.data, gtk_json_reader_remaining (&self->reader));
          if (c == (gunichar) -2 || c == (gunichar) -1)
            {
              gtk_json_parser_syntax_error (self, "Invalid UTF-8");
              return FALSE;
            }
          self->reader.data = (const guchar *) g_utf8_next_char ((const char *) self->reader.data);
        }
      else if (*self->reader.data == '"')
        {
          self->reader.data++;
          return TRUE;
        }
      else if (*self->reader.data == '\\')
        {
          if (gtk_json_reader_remaining (&self->reader) < 2)
            goto end;
          self->reader.data++;
          switch (*self->reader.data)
            {
            case '"':
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
              break;

            case 'u':
              /* lots of work necessary to validate the unicode escapes here */
              if (gtk_json_reader_remaining (&self->reader) < 5 ||
                  !g_ascii_isxdigit (self->reader.data[1]) ||
                  !g_ascii_isxdigit (self->reader.data[2]) ||
                  !g_ascii_isxdigit (self->reader.data[3]) ||
                  !g_ascii_isxdigit (self->reader.data[4]))
                {
                  gtk_json_parser_syntax_error (self, "Invalid Unicode escape sequence");
                  return FALSE;
                }
              else
                {
                  gunichar unichar = (g_ascii_xdigit_value (self->reader.data[1]) << 12) |
                                     (g_ascii_xdigit_value (self->reader.data[2]) <<  8) |
                                     (g_ascii_xdigit_value (self->reader.data[3]) <<  4) |
                                     (g_ascii_xdigit_value (self->reader.data[4]));

                  self->reader.data += 4;
                  /* resolve UTF-16 surrogates for Unicode characters not in the BMP,
                   * as per ECMA 404, § 9, "String"
                   */
                  if (g_unichar_type (unichar) == G_UNICODE_SURROGATE)
                    {
                      if (gtk_json_reader_remaining (&self->reader) >= 7 &&
                          self->reader.data[1] == '\\' &&
                          self->reader.data[2] == 'u' &&
                          g_ascii_isxdigit (self->reader.data[3]) &&
                          g_ascii_isxdigit (self->reader.data[4]) &&
                          g_ascii_isxdigit (self->reader.data[5]) &&
                          g_ascii_isxdigit (self->reader.data[6]))
                        {
                          unichar = decode_utf16_surrogate_pair (unichar,
                                                                 (g_ascii_xdigit_value (self->reader.data[3]) << 12) |
                                                                 (g_ascii_xdigit_value (self->reader.data[4]) <<  8) |
                                                                 (g_ascii_xdigit_value (self->reader.data[5]) <<  4) |
                                                                 (g_ascii_xdigit_value (self->reader.data[6])));
                          self->reader.data += 6;
                        }
                      else
                        {
                          unichar = 0;
                        }

                      if (unichar == 0)
                        {
                          gtk_json_parser_syntax_error (self, "Invalid UTF-16 surrogate pair");
                          return FALSE;
                        }
                    }
                }
              break;
            default:
              gtk_json_parser_syntax_error (self, "Unknown escape sequence");
              return FALSE;
            }
          self->reader.data++;
        }

      self->reader.data = json_skip_characters (self->reader.data, self->reader.end, STRING_ELEMENT);
    }

end:
  gtk_json_parser_syntax_error (self, "Unterminated string literal");
  return FALSE;
}

static gboolean
gtk_json_parser_parse_number (GtkJsonParser *self)
{
  /* sign */
  gtk_json_reader_try_char (&self->reader, '-');

  /* integer part */
  if (!gtk_json_reader_try_char (&self->reader, '0'))
    {
      if (gtk_json_reader_is_eof (&self->reader) ||
          !g_ascii_isdigit (*self->reader.data))
        goto out;

      self->reader.data++;

      while (!gtk_json_reader_is_eof (&self->reader) && g_ascii_isdigit (*self->reader.data))
        self->reader.data++;
    }

  /* fractional part */
  if (gtk_json_reader_remaining (&self->reader) >= 2 && *self->reader.data == '.' && g_ascii_isdigit (self->reader.data[1]))
    {
      self->reader.data += 2;

      while (!gtk_json_reader_is_eof (&self->reader) && g_ascii_isdigit (*self->reader.data))
        self->reader.data++;
    }

  /* exponent */
  if (gtk_json_reader_remaining (&self->reader) >= 2 && (self->reader.data[0] == 'e' || self->reader.data[0] == 'E') &&
      (g_ascii_isdigit (self->reader.data[1]) ||
       (gtk_json_reader_remaining (&self->reader) >= 3 && (self->reader.data[1] == '+' || self->reader.data[1] == '-') && g_ascii_isdigit (self->reader.data[2]))))
    {
      self->reader.data += 2;

      while (!gtk_json_reader_is_eof (&self->reader) && g_ascii_isdigit (*self->reader.data))
        self->reader.data++;
    }
  return TRUE;

out:
  gtk_json_parser_syntax_error (self, "Not a valid number");
  return FALSE;
}

static gboolean
gtk_json_parser_parse_value (GtkJsonParser *self)
{
  if (gtk_json_reader_is_eof (&self->reader))
    {
      gtk_json_parser_syntax_error (self, "Unexpected end of file");
      return FALSE;
    }

  switch (*self->reader.data)
  {
    case '"':
      return gtk_json_parser_parse_string (self);
    
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
      return gtk_json_parser_parse_number (self);

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
    case '[':
      /* don't preparse objects */
      return TRUE;

    default:
      break;
  }

  gtk_json_parser_syntax_error (self, "Expected a value");
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
  gtk_json_parser_parse_value (self);

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
gtk_json_parser_skip_block (GtkJsonParser *self)
{
  if (self->reader.data != self->block->value)
    return TRUE;

  if (*self->reader.data == '{')
    {
      return gtk_json_parser_start_object (self) &&
             gtk_json_parser_end (self);
    }
  else if (*self->reader.data == '[')
    {
      return gtk_json_parser_start_array (self) &&
             gtk_json_parser_end (self);
    }
  else
    {
      g_assert_not_reached ();
      return FALSE;
    }
}

gboolean
gtk_json_parser_next (GtkJsonParser *self)
{
  if (self->error)
    return FALSE;

  if (self->block->value == NULL)
    return FALSE;

  if (!gtk_json_parser_skip_block (self))
    {
      g_assert (self->error);
      return FALSE;
    }

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

      if (!gtk_json_parser_parse_string (self))
        return FALSE;
      gtk_json_reader_skip_whitespace (&self->reader);
      if (!gtk_json_reader_try_char (&self->reader, ':'))
        {
          gtk_json_parser_syntax_error (self, "Missing ':' after member name");
          return FALSE;
        }

      gtk_json_reader_skip_whitespace (&self->reader);
      self->block->value = self->reader.data;
      if (!gtk_json_parser_parse_value (self))
        return FALSE;
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
      if (!gtk_json_parser_parse_value (self))
        return FALSE;
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
  if (self->error)
    return NULL;

  if (self->block->type != GTK_JSON_BLOCK_OBJECT)
    return NULL;

  if (self->block->member_name == NULL)
    return NULL;

  return gtk_json_unescape_string (self->block->member_name);
}

gboolean
gtk_json_parser_get_boolean (GtkJsonParser *self)
{
  if (self->error)
    return FALSE;

  if (self->block->value == NULL)
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
  if (self->error)
    return g_strdup ("");

  if (self->block->value == NULL)
    return g_strdup ("");

  if (*self->block->value != '"')
    {
      gtk_json_parser_value_error (self, "Expected a string");
      return g_strdup ("");
    }

  return gtk_json_unescape_string (self->block->value);
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

  if (!gtk_json_parser_parse_string (self))
    return FALSE;
  gtk_json_reader_skip_whitespace (&self->reader);
  if (!gtk_json_reader_try_char (&self->reader, ':'))
    {
      gtk_json_parser_syntax_error (self, "Missing ':' after member name");
      return FALSE;
    }

  gtk_json_reader_skip_whitespace (&self->reader);
  self->block->value = self->reader.data;
  if (!gtk_json_parser_parse_value (self))
    return FALSE;

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
  if (!gtk_json_parser_parse_value (self))
    return FALSE;

  return TRUE;
}

gboolean
gtk_json_parser_end (GtkJsonParser *self)
{
  char bracket;

  g_return_val_if_fail (self != NULL, FALSE);

  while (gtk_json_parser_next (self));

  if (self->error)
    return FALSE;

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

  if (!gtk_json_reader_try_char (&self->reader, bracket))
    {
      gtk_json_parser_syntax_error (self, "No terminating '%c'", bracket);
      return FALSE;
    }

  gtk_json_parser_pop_block (self);

  return TRUE;
}

