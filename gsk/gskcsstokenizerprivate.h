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

#ifndef __GSK_CSS_TOKENIZER_PRIVATE_H__
#define __GSK_CSS_TOKENIZER_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  /* no content */
  GSK_CSS_TOKEN_EOF,
  GSK_CSS_TOKEN_WHITESPACE,
  GSK_CSS_TOKEN_OPEN_PARENS,
  GSK_CSS_TOKEN_CLOSE_PARENS,
  GSK_CSS_TOKEN_OPEN_SQUARE,
  GSK_CSS_TOKEN_CLOSE_SQUARE,
  GSK_CSS_TOKEN_OPEN_CURLY,
  GSK_CSS_TOKEN_CLOSE_CURLY,
  GSK_CSS_TOKEN_COMMA,
  GSK_CSS_TOKEN_COLON,
  GSK_CSS_TOKEN_SEMICOLON,
  GSK_CSS_TOKEN_CDO,
  GSK_CSS_TOKEN_CDC,
  GSK_CSS_TOKEN_INCLUDE_MATCH,
  GSK_CSS_TOKEN_DASH_MATCH,
  GSK_CSS_TOKEN_PREFIX_MATCH,
  GSK_CSS_TOKEN_SUFFIX_MATCH,
  GSK_CSS_TOKEN_SUBSTRING_MATCH,
  GSK_CSS_TOKEN_COLUMN,
  GSK_CSS_TOKEN_BAD_STRING,
  GSK_CSS_TOKEN_BAD_URL,
  GSK_CSS_TOKEN_COMMENT,
  /* delim */
  GSK_CSS_TOKEN_DELIM,
  /* string */
  GSK_CSS_TOKEN_STRING,
  GSK_CSS_TOKEN_IDENT,
  GSK_CSS_TOKEN_FUNCTION,
  GSK_CSS_TOKEN_AT_KEYWORD,
  GSK_CSS_TOKEN_HASH_UNRESTRICTED,
  GSK_CSS_TOKEN_HASH_ID,
  GSK_CSS_TOKEN_URL,
  /* number */
  GSK_CSS_TOKEN_SIGNED_INTEGER,
  GSK_CSS_TOKEN_SIGNLESS_INTEGER,
  GSK_CSS_TOKEN_SIGNED_NUMBER,
  GSK_CSS_TOKEN_SIGNLESS_NUMBER,
  GSK_CSS_TOKEN_PERCENTAGE,
  /* dimension */
  GSK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION,
  GSK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION,
  GSK_CSS_TOKEN_DIMENSION
} GskCssTokenType;

typedef union _GskCssToken GskCssToken;
typedef struct _GskCssTokenizer GskCssTokenizer;
typedef struct _GskCssLocation GskCssLocation;

typedef struct _GskCssStringToken GskCssStringToken;
typedef struct _GskCssDelimToken GskCssDelimToken;
typedef struct _GskCssNumberToken GskCssNumberToken;
typedef struct _GskCssDimensionToken GskCssDimensionToken;

typedef void (* GskCssTokenizerErrorFunc) (GskCssTokenizer   *parser,
                                           const GskCssToken *token,
                                           const GError      *error,
                                           gpointer           user_data);

struct _GskCssStringToken {
  GskCssTokenType  type;
  char            *string;
};

struct _GskCssDelimToken {
  GskCssTokenType  type;
  gunichar         delim;
};

struct _GskCssNumberToken {
  GskCssTokenType  type;
  double           number;
};

struct _GskCssDimensionToken {
  GskCssTokenType  type;
  double           value;
  char            *dimension;
};

union _GskCssToken {
  GskCssTokenType type;
  GskCssStringToken string;
  GskCssDelimToken delim;
  GskCssNumberToken number;
  GskCssDimensionToken dimension;
};

struct _GskCssLocation
{
  gsize                  bytes;
  gsize                  chars;
  gsize                  lines;
  gsize                  line_bytes;
  gsize                  line_chars;
};

void                    gsk_css_token_clear                     (GskCssToken            *token);

gboolean                gsk_css_token_is_finite                 (const GskCssToken      *token);
gboolean                gsk_css_token_is_preserved              (const GskCssToken      *token,
                                                                 GskCssTokenType        *out_closing);
#define gsk_css_token_is(token, _type) ((token)->type == (_type))
gboolean                gsk_css_token_is_ident                  (const GskCssToken      *token,
                                                                 const char             *ident);
gboolean                gsk_css_token_is_function               (const GskCssToken      *token,
                                                                 const char             *ident);
gboolean                gsk_css_token_is_delim                  (const GskCssToken      *token,
                                                                 gunichar                delim);

void                    gsk_css_token_print                     (const GskCssToken      *token,
                                                                 GString                *string);
char *                  gsk_css_token_to_string                 (const GskCssToken      *token);

GskCssTokenizer *       gsk_css_tokenizer_new                   (GBytes                 *bytes);

GskCssTokenizer *       gsk_css_tokenizer_ref                   (GskCssTokenizer        *tokenizer);
void                    gsk_css_tokenizer_unref                 (GskCssTokenizer        *tokenizer);

const GskCssLocation *  gsk_css_tokenizer_get_location          (GskCssTokenizer        *tokenizer);

gboolean                gsk_css_tokenizer_read_token            (GskCssTokenizer        *tokenizer,
                                                                 GskCssToken            *token,
                                                                 GError                **error);

G_END_DECLS

#endif /* __GSK_CSS_TOKENIZER_PRIVATE_H__ */
