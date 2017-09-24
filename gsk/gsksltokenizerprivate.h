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

#ifndef __GSK_SL_TOKENIZER_PRIVATE_H__
#define __GSK_SL_TOKENIZER_PRIVATE_H__

#include <gsktypes.h>

G_BEGIN_DECLS

typedef enum {
  GSK_SL_TOKEN_EOF = 0,
  GSK_SL_TOKEN_ERROR,
  GSK_SL_TOKEN_NEWLINE,
  GSK_SL_TOKEN_WHITESPACE,
  GSK_SL_TOKEN_COMMENT,
  GSK_SL_TOKEN_SINGLE_LINE_COMMENT,
  /* real tokens */
  GSK_SL_TOKEN_CONST,
  GSK_SL_TOKEN_BOOL,
  GSK_SL_TOKEN_FLOAT,
  GSK_SL_TOKEN_DOUBLE,
  GSK_SL_TOKEN_INT,
  GSK_SL_TOKEN_UINT,
  GSK_SL_TOKEN_BREAK,
  GSK_SL_TOKEN_CONTINUE,
  GSK_SL_TOKEN_DO,
  GSK_SL_TOKEN_ELSE,
  GSK_SL_TOKEN_FOR,
  GSK_SL_TOKEN_IF,
  GSK_SL_TOKEN_DISCARD,
  GSK_SL_TOKEN_RETURN,
  GSK_SL_TOKEN_SWITCH,
  GSK_SL_TOKEN_CASE,
  GSK_SL_TOKEN_DEFAULT,
  GSK_SL_TOKEN_SUBROUTINE,
  GSK_SL_TOKEN_BVEC2,
  GSK_SL_TOKEN_BVEC3,
  GSK_SL_TOKEN_BVEC4,
  GSK_SL_TOKEN_IVEC2,
  GSK_SL_TOKEN_IVEC3,
  GSK_SL_TOKEN_IVEC4,
  GSK_SL_TOKEN_UVEC2,
  GSK_SL_TOKEN_UVEC3,
  GSK_SL_TOKEN_UVEC4,
  GSK_SL_TOKEN_VEC2,
  GSK_SL_TOKEN_VEC3,
  GSK_SL_TOKEN_VEC4,
  GSK_SL_TOKEN_MAT2,
  GSK_SL_TOKEN_MAT3,
  GSK_SL_TOKEN_MAT4,
  GSK_SL_TOKEN_CENTROID,
  GSK_SL_TOKEN_IN,
  GSK_SL_TOKEN_OUT,
  GSK_SL_TOKEN_INOUT,
  GSK_SL_TOKEN_UNIFORM,
  GSK_SL_TOKEN_PATCH,
  GSK_SL_TOKEN_SAMPLE,
  GSK_SL_TOKEN_BUFFER,
  GSK_SL_TOKEN_SHARED,
  GSK_SL_TOKEN_COHERENT,
  GSK_SL_TOKEN_VOLATILE,
  GSK_SL_TOKEN_RESTRICT,
  GSK_SL_TOKEN_READONLY,
  GSK_SL_TOKEN_WRITEONLY,
  GSK_SL_TOKEN_DVEC2,
  GSK_SL_TOKEN_DVEC3,
  GSK_SL_TOKEN_DVEC4,
  GSK_SL_TOKEN_DMAT2,
  GSK_SL_TOKEN_DMAT3,
  GSK_SL_TOKEN_DMAT4,
  GSK_SL_TOKEN_NOPERSPECTIVE,
  GSK_SL_TOKEN_FLAT,
  GSK_SL_TOKEN_SMOOTH,
  GSK_SL_TOKEN_LAYOUT,
  GSK_SL_TOKEN_MAT2X2,
  GSK_SL_TOKEN_MAT2X3,
  GSK_SL_TOKEN_MAT2X4,
  GSK_SL_TOKEN_MAT3X2,
  GSK_SL_TOKEN_MAT3X3,
  GSK_SL_TOKEN_MAT3X4,
  GSK_SL_TOKEN_MAT4X2,
  GSK_SL_TOKEN_MAT4X3,
  GSK_SL_TOKEN_MAT4X4,
  GSK_SL_TOKEN_DMAT2X2,
  GSK_SL_TOKEN_DMAT2X3,
  GSK_SL_TOKEN_DMAT2X4,
  GSK_SL_TOKEN_DMAT3X2,
  GSK_SL_TOKEN_DMAT3X3,
  GSK_SL_TOKEN_DMAT3X4,
  GSK_SL_TOKEN_DMAT4X2,
  GSK_SL_TOKEN_DMAT4X3,
  GSK_SL_TOKEN_DMAT4X4,
  GSK_SL_TOKEN_ATOMIC_UINT,
  GSK_SL_TOKEN_SAMPLER1D,
  GSK_SL_TOKEN_SAMPLER2D,
  GSK_SL_TOKEN_SAMPLER3D,
  GSK_SL_TOKEN_SAMPLERCUBE,
  GSK_SL_TOKEN_SAMPLER1DSHADOW,
  GSK_SL_TOKEN_SAMPLER2DSHADOW,
  GSK_SL_TOKEN_SAMPLERCUBESHADOW,
  GSK_SL_TOKEN_SAMPLER1DARRAY,
  GSK_SL_TOKEN_SAMPLER2DARRAY,
  GSK_SL_TOKEN_SAMPLER1DARRAYSHADOW,
  GSK_SL_TOKEN_SAMPLER2DARRAYSHADOW,
  GSK_SL_TOKEN_ISAMPLER1D,
  GSK_SL_TOKEN_ISAMPLER2D,
  GSK_SL_TOKEN_ISAMPLER3D,
  GSK_SL_TOKEN_ISAMPLERCUBE,
  GSK_SL_TOKEN_ISAMPLER1DARRAY,
  GSK_SL_TOKEN_ISAMPLER2DARRAY,
  GSK_SL_TOKEN_USAMPLER1D,
  GSK_SL_TOKEN_USAMPLER2D,
  GSK_SL_TOKEN_USAMPLER3D,
  GSK_SL_TOKEN_USAMPLERCUBE,
  GSK_SL_TOKEN_USAMPLER1DARRAY,
  GSK_SL_TOKEN_USAMPLER2DARRAY,
  GSK_SL_TOKEN_SAMPLER2DRECT,
  GSK_SL_TOKEN_SAMPLER2DRECTSHADOW,
  GSK_SL_TOKEN_ISAMPLER2DRECT,
  GSK_SL_TOKEN_USAMPLER2DRECT,
  GSK_SL_TOKEN_SAMPLERBUFFER,
  GSK_SL_TOKEN_ISAMPLERBUFFER,
  GSK_SL_TOKEN_USAMPLERBUFFER,
  GSK_SL_TOKEN_SAMPLERCUBEARRAY,
  GSK_SL_TOKEN_SAMPLERCUBEARRAYSHADOW,
  GSK_SL_TOKEN_ISAMPLERCUBEARRAY,
  GSK_SL_TOKEN_USAMPLERCUBEARRAY,
  GSK_SL_TOKEN_SAMPLER2DMS,
  GSK_SL_TOKEN_ISAMPLER2DMS,
  GSK_SL_TOKEN_USAMPLER2DMS,
  GSK_SL_TOKEN_SAMPLER2DMSARRAY,
  GSK_SL_TOKEN_ISAMPLER2DMSARRAY,
  GSK_SL_TOKEN_USAMPLER2DMSARRAY,
  GSK_SL_TOKEN_IMAGE1D,
  GSK_SL_TOKEN_IIMAGE1D,
  GSK_SL_TOKEN_UIMAGE1D,
  GSK_SL_TOKEN_IMAGE2D,
  GSK_SL_TOKEN_IIMAGE2D,
  GSK_SL_TOKEN_UIMAGE2D,
  GSK_SL_TOKEN_IMAGE3D,
  GSK_SL_TOKEN_IIMAGE3D,
  GSK_SL_TOKEN_UIMAGE3D,
  GSK_SL_TOKEN_IMAGE2DRECT,
  GSK_SL_TOKEN_IIMAGE2DRECT,
  GSK_SL_TOKEN_UIMAGE2DRECT,
  GSK_SL_TOKEN_IMAGECUBE,
  GSK_SL_TOKEN_IIMAGECUBE,
  GSK_SL_TOKEN_UIMAGECUBE,
  GSK_SL_TOKEN_IMAGEBUFFER,
  GSK_SL_TOKEN_IIMAGEBUFFER,
  GSK_SL_TOKEN_UIMAGEBUFFER,
  GSK_SL_TOKEN_IMAGE1DARRAY,
  GSK_SL_TOKEN_IIMAGE1DARRAY,
  GSK_SL_TOKEN_UIMAGE1DARRAY,
  GSK_SL_TOKEN_IMAGE2DARRAY,
  GSK_SL_TOKEN_IIMAGE2DARRAY,
  GSK_SL_TOKEN_UIMAGE2DARRAY,
  GSK_SL_TOKEN_IMAGECUBEARRAY,
  GSK_SL_TOKEN_IIMAGECUBEARRAY,
  GSK_SL_TOKEN_UIMAGECUBEARRAY,
  GSK_SL_TOKEN_IMAGE2DMS,
  GSK_SL_TOKEN_IIMAGE2DMS,
  GSK_SL_TOKEN_UIMAGE2DMS,
  GSK_SL_TOKEN_IMAGE2DMSARRAY,
  GSK_SL_TOKEN_IIMAGE2DMSARRAY,
  GSK_SL_TOKEN_UIMAGE2DMSARRAY,
  GSK_SL_TOKEN_STRUCT,
  GSK_SL_TOKEN_VOID,
  GSK_SL_TOKEN_WHILE,
  GSK_SL_TOKEN_IDENTIFIER,
  GSK_SL_TOKEN_FLOATCONSTANT,
  GSK_SL_TOKEN_DOUBLECONSTANT,
  GSK_SL_TOKEN_INTCONSTANT,
  GSK_SL_TOKEN_UINTCONSTANT,
  GSK_SL_TOKEN_BOOLCONSTANT,
  GSK_SL_TOKEN_LEFT_OP,
  GSK_SL_TOKEN_RIGHT_OP,
  GSK_SL_TOKEN_INC_OP,
  GSK_SL_TOKEN_DEC_OP,
  GSK_SL_TOKEN_LE_OP,
  GSK_SL_TOKEN_GE_OP,
  GSK_SL_TOKEN_EQ_OP,
  GSK_SL_TOKEN_NE_OP,
  GSK_SL_TOKEN_AND_OP,
  GSK_SL_TOKEN_OR_OP,
  GSK_SL_TOKEN_XOR_OP,
  GSK_SL_TOKEN_MUL_ASSIGN,
  GSK_SL_TOKEN_DIV_ASSIGN,
  GSK_SL_TOKEN_ADD_ASSIGN,
  GSK_SL_TOKEN_MOD_ASSIGN,
  GSK_SL_TOKEN_LEFT_ASSIGN,
  GSK_SL_TOKEN_RIGHT_ASSIGN,
  GSK_SL_TOKEN_AND_ASSIGN,
  GSK_SL_TOKEN_XOR_ASSIGN,
  GSK_SL_TOKEN_OR_ASSIGN,
  GSK_SL_TOKEN_SUB_ASSIGN,
  GSK_SL_TOKEN_LEFT_PAREN,
  GSK_SL_TOKEN_RIGHT_PAREN,
  GSK_SL_TOKEN_LEFT_BRACKET,
  GSK_SL_TOKEN_RIGHT_BRACKET,
  GSK_SL_TOKEN_LEFT_BRACE,
  GSK_SL_TOKEN_RIGHT_BRACE,
  GSK_SL_TOKEN_DOT,
  GSK_SL_TOKEN_COMMA,
  GSK_SL_TOKEN_COLON,
  GSK_SL_TOKEN_EQUAL,
  GSK_SL_TOKEN_SEMICOLON,
  GSK_SL_TOKEN_BANG,
  GSK_SL_TOKEN_DASH,
  GSK_SL_TOKEN_TILDE,
  GSK_SL_TOKEN_PLUS,
  GSK_SL_TOKEN_STAR,
  GSK_SL_TOKEN_SLASH,
  GSK_SL_TOKEN_PERCENT,
  GSK_SL_TOKEN_LEFT_ANGLE,
  GSK_SL_TOKEN_RIGHT_ANGLE,
  GSK_SL_TOKEN_VERTICAL_BAR,
  GSK_SL_TOKEN_CARET,
  GSK_SL_TOKEN_AMPERSAND,
  GSK_SL_TOKEN_QUESTION,
  GSK_SL_TOKEN_HASH,
  GSK_SL_TOKEN_INVARIANT,
  GSK_SL_TOKEN_PRECISE,
  GSK_SL_TOKEN_HIGH_PRECISION,
  GSK_SL_TOKEN_MEDIUM_PRECISION,
  GSK_SL_TOKEN_LOW_PRECISION,
  GSK_SL_TOKEN_PRECISION
} GskSlTokenType;

typedef struct _GskSlToken GskSlToken;
typedef struct _GskSlTokenizer GskSlTokenizer;

typedef void (* GskSlTokenizerErrorFunc) (GskSlTokenizer        *parser,
                                          gboolean               fatal,
                                          const GskCodeLocation *location,
                                          const GskSlToken      *token,
                                          const GError          *error,
                                          gpointer               user_data);

struct _GskSlToken {
  GskSlTokenType type;
  union {
    gint32       i32;
    guint32      u32;
    float        f;
    double       d;
    gboolean     b;
    char        *str;
  };
};

void                    gsk_sl_token_clear                      (GskSlToken            *token);
void                    gsk_sl_token_copy                       (GskSlToken            *dest,
                                                                 const GskSlToken      *src);

gboolean                gsk_sl_string_is_valid_identifier       (const char            *ident);
void                    gsk_sl_token_init_from_identifier       (GskSlToken            *token,
                                                                 const char            *ident);

gboolean                gsk_sl_token_is_skipped                 (const GskSlToken      *token);
#define gsk_sl_token_is(token, _type) ((token)->type == (_type))
gboolean                gsk_sl_token_is_ident                   (const GskSlToken      *token,
                                                                 const char            *ident);
gboolean                gsk_sl_token_is_function                (const GskSlToken      *token,
                                                                 const char            *ident);

void                    gsk_sl_token_print                      (const GskSlToken      *token,
                                                                 GString               *string);
char *                  gsk_sl_token_to_string                  (const GskSlToken      *token);

GskSlTokenizer *        gsk_sl_tokenizer_new                    (GBytes                 *bytes,
                                                                 GskSlTokenizerErrorFunc func,
                                                                 gpointer                user_data,
                                                                 GDestroyNotify          user_destroy);

GskSlTokenizer *        gsk_sl_tokenizer_ref                    (GskSlTokenizer        *tokenizer);
void                    gsk_sl_tokenizer_unref                  (GskSlTokenizer        *tokenizer);

const GskCodeLocation * gsk_sl_tokenizer_get_location           (GskSlTokenizer        *tokenizer);

void                    gsk_sl_tokenizer_read_token             (GskSlTokenizer        *tokenizer,
                                                                 GskSlToken            *token);

G_END_DECLS

#endif /* __GSK_SL_TOKENIZER_PRIVATE_H__ */
