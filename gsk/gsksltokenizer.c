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

#include "gsksltokenizerprivate.h"

#include "gskslcompiler.h"
#include "gskcodesourceprivate.h"

#include <math.h>
#include <string.h>

typedef struct _GskSlTokenReader GskSlTokenReader;

struct _GskSlTokenReader {
  const char *           data;
  const char *           end;

  GskCodeLocation        position;
};

struct _GskSlTokenizer
{
  gint                   ref_count;
  GskCodeSource         *source;
  GBytes                *bytes;
  GskSlTokenizerErrorFunc error_func;
  gpointer               user_data;
  GDestroyNotify         user_destroy;

  GskSlTokenReader       reader;
};

static void
gsk_code_location_init (GskCodeLocation *location,
                        GskCodeSource   *source)
{
  *location = (GskCodeLocation) { source, };
}

static void
gsk_code_location_init_copy (GskCodeLocation       *location,
                             const GskCodeLocation *source)
{
  *location = *source;
}

static void
gsk_code_location_advance (GskCodeLocation *location,
                           gsize            bytes,
                           gsize            chars)
{
  location->bytes += bytes;
  location->chars += chars;
  location->line_bytes += bytes;
  location->line_chars += chars;
}

static void
gsk_code_location_advance_newline (GskCodeLocation *location,
                                   gsize            n_chars)
{
  gsk_code_location_advance (location, n_chars, n_chars);

  location->lines++;
  location->line_bytes = 0;
  location->line_chars = 0;
}

void
gsk_sl_token_clear (GskSlToken *token)
{
  switch (token->type)
    {
    case GSK_SL_TOKEN_IDENTIFIER:
    case GSK_SL_TOKEN_STRING:
      g_free (token->str);
      break;

    case GSK_SL_TOKEN_EOF:
    case GSK_SL_TOKEN_ERROR:
    case GSK_SL_TOKEN_NEWLINE:
    case GSK_SL_TOKEN_WHITESPACE:
    case GSK_SL_TOKEN_COMMENT:
    case GSK_SL_TOKEN_SINGLE_LINE_COMMENT:
  /* real tokens */
    case GSK_SL_TOKEN_CONST:
    case GSK_SL_TOKEN_BOOL:
    case GSK_SL_TOKEN_FLOAT:
    case GSK_SL_TOKEN_DOUBLE:
    case GSK_SL_TOKEN_INT:
    case GSK_SL_TOKEN_UINT:
    case GSK_SL_TOKEN_BREAK:
    case GSK_SL_TOKEN_CONTINUE:
    case GSK_SL_TOKEN_DO:
    case GSK_SL_TOKEN_ELSE:
    case GSK_SL_TOKEN_FOR:
    case GSK_SL_TOKEN_IF:
    case GSK_SL_TOKEN_DISCARD:
    case GSK_SL_TOKEN_RETURN:
    case GSK_SL_TOKEN_SWITCH:
    case GSK_SL_TOKEN_CASE:
    case GSK_SL_TOKEN_DEFAULT:
    case GSK_SL_TOKEN_SUBROUTINE:
    case GSK_SL_TOKEN_BVEC2:
    case GSK_SL_TOKEN_BVEC3:
    case GSK_SL_TOKEN_BVEC4:
    case GSK_SL_TOKEN_IVEC2:
    case GSK_SL_TOKEN_IVEC3:
    case GSK_SL_TOKEN_IVEC4:
    case GSK_SL_TOKEN_UVEC2:
    case GSK_SL_TOKEN_UVEC3:
    case GSK_SL_TOKEN_UVEC4:
    case GSK_SL_TOKEN_VEC2:
    case GSK_SL_TOKEN_VEC3:
    case GSK_SL_TOKEN_VEC4:
    case GSK_SL_TOKEN_MAT2:
    case GSK_SL_TOKEN_MAT3:
    case GSK_SL_TOKEN_MAT4:
    case GSK_SL_TOKEN_CENTROID:
    case GSK_SL_TOKEN_IN:
    case GSK_SL_TOKEN_OUT:
    case GSK_SL_TOKEN_INOUT:
    case GSK_SL_TOKEN_UNIFORM:
    case GSK_SL_TOKEN_PATCH:
    case GSK_SL_TOKEN_SAMPLE:
    case GSK_SL_TOKEN_BUFFER:
    case GSK_SL_TOKEN_SHARED:
    case GSK_SL_TOKEN_COHERENT:
    case GSK_SL_TOKEN_VOLATILE:
    case GSK_SL_TOKEN_RESTRICT:
    case GSK_SL_TOKEN_READONLY:
    case GSK_SL_TOKEN_WRITEONLY:
    case GSK_SL_TOKEN_DVEC2:
    case GSK_SL_TOKEN_DVEC3:
    case GSK_SL_TOKEN_DVEC4:
    case GSK_SL_TOKEN_DMAT2:
    case GSK_SL_TOKEN_DMAT3:
    case GSK_SL_TOKEN_DMAT4:
    case GSK_SL_TOKEN_NOPERSPECTIVE:
    case GSK_SL_TOKEN_FLAT:
    case GSK_SL_TOKEN_SMOOTH:
    case GSK_SL_TOKEN_LAYOUT:
    case GSK_SL_TOKEN_MAT2X2:
    case GSK_SL_TOKEN_MAT2X3:
    case GSK_SL_TOKEN_MAT2X4:
    case GSK_SL_TOKEN_MAT3X2:
    case GSK_SL_TOKEN_MAT3X3:
    case GSK_SL_TOKEN_MAT3X4:
    case GSK_SL_TOKEN_MAT4X2:
    case GSK_SL_TOKEN_MAT4X3:
    case GSK_SL_TOKEN_MAT4X4:
    case GSK_SL_TOKEN_DMAT2X2:
    case GSK_SL_TOKEN_DMAT2X3:
    case GSK_SL_TOKEN_DMAT2X4:
    case GSK_SL_TOKEN_DMAT3X2:
    case GSK_SL_TOKEN_DMAT3X3:
    case GSK_SL_TOKEN_DMAT3X4:
    case GSK_SL_TOKEN_DMAT4X2:
    case GSK_SL_TOKEN_DMAT4X3:
    case GSK_SL_TOKEN_DMAT4X4:
    case GSK_SL_TOKEN_ATOMIC_UINT:
    case GSK_SL_TOKEN_SAMPLER1D:
    case GSK_SL_TOKEN_SAMPLER2D:
    case GSK_SL_TOKEN_SAMPLER3D:
    case GSK_SL_TOKEN_SAMPLERCUBE:
    case GSK_SL_TOKEN_SAMPLER1DSHADOW:
    case GSK_SL_TOKEN_SAMPLER2DSHADOW:
    case GSK_SL_TOKEN_SAMPLERCUBESHADOW:
    case GSK_SL_TOKEN_SAMPLER1DARRAY:
    case GSK_SL_TOKEN_SAMPLER2DARRAY:
    case GSK_SL_TOKEN_SAMPLER1DARRAYSHADOW:
    case GSK_SL_TOKEN_SAMPLER2DARRAYSHADOW:
    case GSK_SL_TOKEN_ISAMPLER1D:
    case GSK_SL_TOKEN_ISAMPLER2D:
    case GSK_SL_TOKEN_ISAMPLER3D:
    case GSK_SL_TOKEN_ISAMPLERCUBE:
    case GSK_SL_TOKEN_ISAMPLER1DARRAY:
    case GSK_SL_TOKEN_ISAMPLER2DARRAY:
    case GSK_SL_TOKEN_USAMPLER1D:
    case GSK_SL_TOKEN_USAMPLER2D:
    case GSK_SL_TOKEN_USAMPLER3D:
    case GSK_SL_TOKEN_USAMPLERCUBE:
    case GSK_SL_TOKEN_USAMPLER1DARRAY:
    case GSK_SL_TOKEN_USAMPLER2DARRAY:
    case GSK_SL_TOKEN_SAMPLER2DRECT:
    case GSK_SL_TOKEN_SAMPLER2DRECTSHADOW:
    case GSK_SL_TOKEN_ISAMPLER2DRECT:
    case GSK_SL_TOKEN_USAMPLER2DRECT:
    case GSK_SL_TOKEN_SAMPLERBUFFER:
    case GSK_SL_TOKEN_ISAMPLERBUFFER:
    case GSK_SL_TOKEN_USAMPLERBUFFER:
    case GSK_SL_TOKEN_SAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_SAMPLERCUBEARRAYSHADOW:
    case GSK_SL_TOKEN_ISAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_USAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_SAMPLER2DMS:
    case GSK_SL_TOKEN_ISAMPLER2DMS:
    case GSK_SL_TOKEN_USAMPLER2DMS:
    case GSK_SL_TOKEN_SAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_ISAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_USAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_IMAGE1D:
    case GSK_SL_TOKEN_IIMAGE1D:
    case GSK_SL_TOKEN_UIMAGE1D:
    case GSK_SL_TOKEN_IMAGE2D:
    case GSK_SL_TOKEN_IIMAGE2D:
    case GSK_SL_TOKEN_UIMAGE2D:
    case GSK_SL_TOKEN_IMAGE3D:
    case GSK_SL_TOKEN_IIMAGE3D:
    case GSK_SL_TOKEN_UIMAGE3D:
    case GSK_SL_TOKEN_IMAGE2DRECT:
    case GSK_SL_TOKEN_IIMAGE2DRECT:
    case GSK_SL_TOKEN_UIMAGE2DRECT:
    case GSK_SL_TOKEN_IMAGECUBE:
    case GSK_SL_TOKEN_IIMAGECUBE:
    case GSK_SL_TOKEN_UIMAGECUBE:
    case GSK_SL_TOKEN_IMAGEBUFFER:
    case GSK_SL_TOKEN_IIMAGEBUFFER:
    case GSK_SL_TOKEN_UIMAGEBUFFER:
    case GSK_SL_TOKEN_IMAGE1DARRAY:
    case GSK_SL_TOKEN_IIMAGE1DARRAY:
    case GSK_SL_TOKEN_UIMAGE1DARRAY:
    case GSK_SL_TOKEN_IMAGE2DARRAY:
    case GSK_SL_TOKEN_IIMAGE2DARRAY:
    case GSK_SL_TOKEN_UIMAGE2DARRAY:
    case GSK_SL_TOKEN_IMAGECUBEARRAY:
    case GSK_SL_TOKEN_IIMAGECUBEARRAY:
    case GSK_SL_TOKEN_UIMAGECUBEARRAY:
    case GSK_SL_TOKEN_IMAGE2DMS:
    case GSK_SL_TOKEN_IIMAGE2DMS:
    case GSK_SL_TOKEN_UIMAGE2DMS:
    case GSK_SL_TOKEN_IMAGE2DMSARRAY:
    case GSK_SL_TOKEN_IIMAGE2DMSARRAY:
    case GSK_SL_TOKEN_UIMAGE2DMSARRAY:
    case GSK_SL_TOKEN_STRUCT:
    case GSK_SL_TOKEN_VOID:
    case GSK_SL_TOKEN_WHILE:
    case GSK_SL_TOKEN_FLOATCONSTANT:
    case GSK_SL_TOKEN_DOUBLECONSTANT:
    case GSK_SL_TOKEN_INTCONSTANT:
    case GSK_SL_TOKEN_UINTCONSTANT:
    case GSK_SL_TOKEN_BOOLCONSTANT:
    case GSK_SL_TOKEN_LEFT_OP:
    case GSK_SL_TOKEN_RIGHT_OP:
    case GSK_SL_TOKEN_INC_OP:
    case GSK_SL_TOKEN_DEC_OP:
    case GSK_SL_TOKEN_LE_OP:
    case GSK_SL_TOKEN_GE_OP:
    case GSK_SL_TOKEN_EQ_OP:
    case GSK_SL_TOKEN_NE_OP:
    case GSK_SL_TOKEN_AND_OP:
    case GSK_SL_TOKEN_OR_OP:
    case GSK_SL_TOKEN_XOR_OP:
    case GSK_SL_TOKEN_MUL_ASSIGN:
    case GSK_SL_TOKEN_DIV_ASSIGN:
    case GSK_SL_TOKEN_ADD_ASSIGN:
    case GSK_SL_TOKEN_MOD_ASSIGN:
    case GSK_SL_TOKEN_LEFT_ASSIGN:
    case GSK_SL_TOKEN_RIGHT_ASSIGN:
    case GSK_SL_TOKEN_AND_ASSIGN:
    case GSK_SL_TOKEN_XOR_ASSIGN:
    case GSK_SL_TOKEN_OR_ASSIGN:
    case GSK_SL_TOKEN_SUB_ASSIGN:
    case GSK_SL_TOKEN_LEFT_PAREN:
    case GSK_SL_TOKEN_RIGHT_PAREN:
    case GSK_SL_TOKEN_LEFT_BRACKET:
    case GSK_SL_TOKEN_RIGHT_BRACKET:
    case GSK_SL_TOKEN_LEFT_BRACE:
    case GSK_SL_TOKEN_RIGHT_BRACE:
    case GSK_SL_TOKEN_DOT:
    case GSK_SL_TOKEN_COMMA:
    case GSK_SL_TOKEN_COLON:
    case GSK_SL_TOKEN_EQUAL:
    case GSK_SL_TOKEN_SEMICOLON:
    case GSK_SL_TOKEN_BANG:
    case GSK_SL_TOKEN_DASH:
    case GSK_SL_TOKEN_TILDE:
    case GSK_SL_TOKEN_PLUS:
    case GSK_SL_TOKEN_STAR:
    case GSK_SL_TOKEN_SLASH:
    case GSK_SL_TOKEN_PERCENT:
    case GSK_SL_TOKEN_LEFT_ANGLE:
    case GSK_SL_TOKEN_RIGHT_ANGLE:
    case GSK_SL_TOKEN_VERTICAL_BAR:
    case GSK_SL_TOKEN_CARET:
    case GSK_SL_TOKEN_AMPERSAND:
    case GSK_SL_TOKEN_QUESTION:
    case GSK_SL_TOKEN_HASH:
    case GSK_SL_TOKEN_INVARIANT:
    case GSK_SL_TOKEN_PRECISE:
    case GSK_SL_TOKEN_HIGH_PRECISION:
    case GSK_SL_TOKEN_MEDIUM_PRECISION:
    case GSK_SL_TOKEN_LOW_PRECISION:
    case GSK_SL_TOKEN_PRECISION:
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  token->type = GSK_SL_TOKEN_EOF;
}

void
gsk_sl_token_copy (GskSlToken       *dest,
                   const GskSlToken *src)
{
  dest->type = src->type;

  switch (src->type)
    {
    case GSK_SL_TOKEN_IDENTIFIER:
    case GSK_SL_TOKEN_STRING:
      dest->str = g_strdup (src->str);
      break;

    case GSK_SL_TOKEN_BOOLCONSTANT:
      dest->b = src->b;
      break;

    case GSK_SL_TOKEN_FLOATCONSTANT:
      dest->f = src->f;
      break;

    case GSK_SL_TOKEN_DOUBLECONSTANT:
      dest->d = src->d;
      break;

    case GSK_SL_TOKEN_INTCONSTANT:
      dest->i32 = src->i32;
      break;

    case GSK_SL_TOKEN_UINTCONSTANT:
      dest->u32 = src->u32;
      break;

    case GSK_SL_TOKEN_EOF:
    case GSK_SL_TOKEN_ERROR:
    case GSK_SL_TOKEN_NEWLINE:
    case GSK_SL_TOKEN_WHITESPACE:
    case GSK_SL_TOKEN_COMMENT:
    case GSK_SL_TOKEN_SINGLE_LINE_COMMENT:
    case GSK_SL_TOKEN_CONST:
    case GSK_SL_TOKEN_BREAK:
    case GSK_SL_TOKEN_CONTINUE:
    case GSK_SL_TOKEN_DO:
    case GSK_SL_TOKEN_ELSE:
    case GSK_SL_TOKEN_FOR:
    case GSK_SL_TOKEN_IF:
    case GSK_SL_TOKEN_DISCARD:
    case GSK_SL_TOKEN_RETURN:
    case GSK_SL_TOKEN_SWITCH:
    case GSK_SL_TOKEN_CASE:
    case GSK_SL_TOKEN_DEFAULT:
    case GSK_SL_TOKEN_SUBROUTINE:
    case GSK_SL_TOKEN_BVEC2:
    case GSK_SL_TOKEN_BVEC3:
    case GSK_SL_TOKEN_BVEC4:
    case GSK_SL_TOKEN_IVEC2:
    case GSK_SL_TOKEN_IVEC3:
    case GSK_SL_TOKEN_IVEC4:
    case GSK_SL_TOKEN_UVEC2:
    case GSK_SL_TOKEN_UVEC3:
    case GSK_SL_TOKEN_UVEC4:
    case GSK_SL_TOKEN_VEC2:
    case GSK_SL_TOKEN_VEC3:
    case GSK_SL_TOKEN_VEC4:
    case GSK_SL_TOKEN_MAT2:
    case GSK_SL_TOKEN_MAT3:
    case GSK_SL_TOKEN_MAT4:
    case GSK_SL_TOKEN_CENTROID:
    case GSK_SL_TOKEN_IN:
    case GSK_SL_TOKEN_OUT:
    case GSK_SL_TOKEN_INOUT:
    case GSK_SL_TOKEN_UNIFORM:
    case GSK_SL_TOKEN_PATCH:
    case GSK_SL_TOKEN_SAMPLE:
    case GSK_SL_TOKEN_BUFFER:
    case GSK_SL_TOKEN_SHARED:
    case GSK_SL_TOKEN_COHERENT:
    case GSK_SL_TOKEN_VOLATILE:
    case GSK_SL_TOKEN_RESTRICT:
    case GSK_SL_TOKEN_READONLY:
    case GSK_SL_TOKEN_WRITEONLY:
    case GSK_SL_TOKEN_DVEC2:
    case GSK_SL_TOKEN_DVEC3:
    case GSK_SL_TOKEN_DVEC4:
    case GSK_SL_TOKEN_DMAT2:
    case GSK_SL_TOKEN_DMAT3:
    case GSK_SL_TOKEN_DMAT4:
    case GSK_SL_TOKEN_NOPERSPECTIVE:
    case GSK_SL_TOKEN_FLAT:
    case GSK_SL_TOKEN_SMOOTH:
    case GSK_SL_TOKEN_LAYOUT:
    case GSK_SL_TOKEN_MAT2X2:
    case GSK_SL_TOKEN_MAT2X3:
    case GSK_SL_TOKEN_MAT2X4:
    case GSK_SL_TOKEN_MAT3X2:
    case GSK_SL_TOKEN_MAT3X3:
    case GSK_SL_TOKEN_MAT3X4:
    case GSK_SL_TOKEN_MAT4X2:
    case GSK_SL_TOKEN_MAT4X3:
    case GSK_SL_TOKEN_MAT4X4:
    case GSK_SL_TOKEN_DMAT2X2:
    case GSK_SL_TOKEN_DMAT2X3:
    case GSK_SL_TOKEN_DMAT2X4:
    case GSK_SL_TOKEN_DMAT3X2:
    case GSK_SL_TOKEN_DMAT3X3:
    case GSK_SL_TOKEN_DMAT3X4:
    case GSK_SL_TOKEN_DMAT4X2:
    case GSK_SL_TOKEN_DMAT4X3:
    case GSK_SL_TOKEN_DMAT4X4:
    case GSK_SL_TOKEN_ATOMIC_UINT:
    case GSK_SL_TOKEN_SAMPLER1D:
    case GSK_SL_TOKEN_SAMPLER2D:
    case GSK_SL_TOKEN_SAMPLER3D:
    case GSK_SL_TOKEN_SAMPLERCUBE:
    case GSK_SL_TOKEN_SAMPLER1DSHADOW:
    case GSK_SL_TOKEN_SAMPLER2DSHADOW:
    case GSK_SL_TOKEN_SAMPLERCUBESHADOW:
    case GSK_SL_TOKEN_SAMPLER1DARRAY:
    case GSK_SL_TOKEN_SAMPLER2DARRAY:
    case GSK_SL_TOKEN_SAMPLER1DARRAYSHADOW:
    case GSK_SL_TOKEN_SAMPLER2DARRAYSHADOW:
    case GSK_SL_TOKEN_ISAMPLER1D:
    case GSK_SL_TOKEN_ISAMPLER2D:
    case GSK_SL_TOKEN_ISAMPLER3D:
    case GSK_SL_TOKEN_ISAMPLERCUBE:
    case GSK_SL_TOKEN_ISAMPLER1DARRAY:
    case GSK_SL_TOKEN_ISAMPLER2DARRAY:
    case GSK_SL_TOKEN_USAMPLER1D:
    case GSK_SL_TOKEN_USAMPLER2D:
    case GSK_SL_TOKEN_USAMPLER3D:
    case GSK_SL_TOKEN_USAMPLERCUBE:
    case GSK_SL_TOKEN_USAMPLER1DARRAY:
    case GSK_SL_TOKEN_USAMPLER2DARRAY:
    case GSK_SL_TOKEN_SAMPLER2DRECT:
    case GSK_SL_TOKEN_SAMPLER2DRECTSHADOW:
    case GSK_SL_TOKEN_ISAMPLER2DRECT:
    case GSK_SL_TOKEN_USAMPLER2DRECT:
    case GSK_SL_TOKEN_SAMPLERBUFFER:
    case GSK_SL_TOKEN_ISAMPLERBUFFER:
    case GSK_SL_TOKEN_USAMPLERBUFFER:
    case GSK_SL_TOKEN_SAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_SAMPLERCUBEARRAYSHADOW:
    case GSK_SL_TOKEN_ISAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_USAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_SAMPLER2DMS:
    case GSK_SL_TOKEN_ISAMPLER2DMS:
    case GSK_SL_TOKEN_USAMPLER2DMS:
    case GSK_SL_TOKEN_SAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_ISAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_USAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_IMAGE1D:
    case GSK_SL_TOKEN_IIMAGE1D:
    case GSK_SL_TOKEN_UIMAGE1D:
    case GSK_SL_TOKEN_IMAGE2D:
    case GSK_SL_TOKEN_IIMAGE2D:
    case GSK_SL_TOKEN_UIMAGE2D:
    case GSK_SL_TOKEN_IMAGE3D:
    case GSK_SL_TOKEN_IIMAGE3D:
    case GSK_SL_TOKEN_UIMAGE3D:
    case GSK_SL_TOKEN_IMAGE2DRECT:
    case GSK_SL_TOKEN_IIMAGE2DRECT:
    case GSK_SL_TOKEN_UIMAGE2DRECT:
    case GSK_SL_TOKEN_IMAGECUBE:
    case GSK_SL_TOKEN_IIMAGECUBE:
    case GSK_SL_TOKEN_UIMAGECUBE:
    case GSK_SL_TOKEN_IMAGEBUFFER:
    case GSK_SL_TOKEN_IIMAGEBUFFER:
    case GSK_SL_TOKEN_UIMAGEBUFFER:
    case GSK_SL_TOKEN_IMAGE1DARRAY:
    case GSK_SL_TOKEN_IIMAGE1DARRAY:
    case GSK_SL_TOKEN_UIMAGE1DARRAY:
    case GSK_SL_TOKEN_IMAGE2DARRAY:
    case GSK_SL_TOKEN_IIMAGE2DARRAY:
    case GSK_SL_TOKEN_UIMAGE2DARRAY:
    case GSK_SL_TOKEN_IMAGECUBEARRAY:
    case GSK_SL_TOKEN_IIMAGECUBEARRAY:
    case GSK_SL_TOKEN_UIMAGECUBEARRAY:
    case GSK_SL_TOKEN_IMAGE2DMS:
    case GSK_SL_TOKEN_IIMAGE2DMS:
    case GSK_SL_TOKEN_UIMAGE2DMS:
    case GSK_SL_TOKEN_IMAGE2DMSARRAY:
    case GSK_SL_TOKEN_IIMAGE2DMSARRAY:
    case GSK_SL_TOKEN_UIMAGE2DMSARRAY:
    case GSK_SL_TOKEN_STRUCT:
    case GSK_SL_TOKEN_VOID:
    case GSK_SL_TOKEN_WHILE:
    case GSK_SL_TOKEN_FLOAT:
    case GSK_SL_TOKEN_DOUBLE:
    case GSK_SL_TOKEN_INT:
    case GSK_SL_TOKEN_UINT:
    case GSK_SL_TOKEN_BOOL:
    case GSK_SL_TOKEN_LEFT_OP:
    case GSK_SL_TOKEN_RIGHT_OP:
    case GSK_SL_TOKEN_INC_OP:
    case GSK_SL_TOKEN_DEC_OP:
    case GSK_SL_TOKEN_LE_OP:
    case GSK_SL_TOKEN_GE_OP:
    case GSK_SL_TOKEN_EQ_OP:
    case GSK_SL_TOKEN_NE_OP:
    case GSK_SL_TOKEN_AND_OP:
    case GSK_SL_TOKEN_OR_OP:
    case GSK_SL_TOKEN_XOR_OP:
    case GSK_SL_TOKEN_MUL_ASSIGN:
    case GSK_SL_TOKEN_DIV_ASSIGN:
    case GSK_SL_TOKEN_ADD_ASSIGN:
    case GSK_SL_TOKEN_MOD_ASSIGN:
    case GSK_SL_TOKEN_LEFT_ASSIGN:
    case GSK_SL_TOKEN_RIGHT_ASSIGN:
    case GSK_SL_TOKEN_AND_ASSIGN:
    case GSK_SL_TOKEN_XOR_ASSIGN:
    case GSK_SL_TOKEN_OR_ASSIGN:
    case GSK_SL_TOKEN_SUB_ASSIGN:
    case GSK_SL_TOKEN_LEFT_PAREN:
    case GSK_SL_TOKEN_RIGHT_PAREN:
    case GSK_SL_TOKEN_LEFT_BRACKET:
    case GSK_SL_TOKEN_RIGHT_BRACKET:
    case GSK_SL_TOKEN_LEFT_BRACE:
    case GSK_SL_TOKEN_RIGHT_BRACE:
    case GSK_SL_TOKEN_DOT:
    case GSK_SL_TOKEN_COMMA:
    case GSK_SL_TOKEN_COLON:
    case GSK_SL_TOKEN_EQUAL:
    case GSK_SL_TOKEN_SEMICOLON:
    case GSK_SL_TOKEN_BANG:
    case GSK_SL_TOKEN_DASH:
    case GSK_SL_TOKEN_TILDE:
    case GSK_SL_TOKEN_PLUS:
    case GSK_SL_TOKEN_STAR:
    case GSK_SL_TOKEN_SLASH:
    case GSK_SL_TOKEN_PERCENT:
    case GSK_SL_TOKEN_LEFT_ANGLE:
    case GSK_SL_TOKEN_RIGHT_ANGLE:
    case GSK_SL_TOKEN_VERTICAL_BAR:
    case GSK_SL_TOKEN_CARET:
    case GSK_SL_TOKEN_AMPERSAND:
    case GSK_SL_TOKEN_QUESTION:
    case GSK_SL_TOKEN_HASH:
    case GSK_SL_TOKEN_INVARIANT:
    case GSK_SL_TOKEN_PRECISE:
    case GSK_SL_TOKEN_HIGH_PRECISION:
    case GSK_SL_TOKEN_MEDIUM_PRECISION:
    case GSK_SL_TOKEN_LOW_PRECISION:
    case GSK_SL_TOKEN_PRECISION:
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static const char *keywords[] = {
  [GSK_SL_TOKEN_CONST] = "const",
  [GSK_SL_TOKEN_BOOL] = "bool",
  [GSK_SL_TOKEN_FLOAT] = "float",
  [GSK_SL_TOKEN_DOUBLE] = "double",
  [GSK_SL_TOKEN_INT] = "int",
  [GSK_SL_TOKEN_UINT] = "uint",
  [GSK_SL_TOKEN_BREAK] = "break",
  [GSK_SL_TOKEN_CONTINUE] = "continue",
  [GSK_SL_TOKEN_DO] = "do",
  [GSK_SL_TOKEN_ELSE] = "else",
  [GSK_SL_TOKEN_FOR] = "for",
  [GSK_SL_TOKEN_IF] = "if",
  [GSK_SL_TOKEN_DISCARD] = "discard",
  [GSK_SL_TOKEN_RETURN] = "return",
  [GSK_SL_TOKEN_SWITCH] = "switch",
  [GSK_SL_TOKEN_CASE] = "case",
  [GSK_SL_TOKEN_DEFAULT] = "default",
  [GSK_SL_TOKEN_SUBROUTINE] = "subroutine",
  [GSK_SL_TOKEN_BVEC2] = "bvec2",
  [GSK_SL_TOKEN_BVEC3] = "bvec3",
  [GSK_SL_TOKEN_BVEC4] = "bvec4",
  [GSK_SL_TOKEN_IVEC2] = "ivec2",
  [GSK_SL_TOKEN_IVEC3] = "ivec3",
  [GSK_SL_TOKEN_IVEC4] = "ivec4",
  [GSK_SL_TOKEN_UVEC2] = "uvec2",
  [GSK_SL_TOKEN_UVEC3] = "uvec3",
  [GSK_SL_TOKEN_UVEC4] = "uvec4",
  [GSK_SL_TOKEN_VEC2] = "vec2",
  [GSK_SL_TOKEN_VEC3] = "vec3",
  [GSK_SL_TOKEN_VEC4] = "vec4",
  [GSK_SL_TOKEN_MAT2] = "mat2",
  [GSK_SL_TOKEN_MAT3] = "mat3",
  [GSK_SL_TOKEN_MAT4] = "mat4",
  [GSK_SL_TOKEN_CENTROID] = "centroid",
  [GSK_SL_TOKEN_IN] = "in",
  [GSK_SL_TOKEN_OUT] = "out",
  [GSK_SL_TOKEN_INOUT] = "inout",
  [GSK_SL_TOKEN_UNIFORM] = "uniform",
  [GSK_SL_TOKEN_PATCH] = "patch",
  [GSK_SL_TOKEN_SAMPLE] = "sample",
  [GSK_SL_TOKEN_BUFFER] = "buffer",
  [GSK_SL_TOKEN_SHARED] = "shared",
  [GSK_SL_TOKEN_COHERENT] = "coherent",
  [GSK_SL_TOKEN_VOLATILE] = "volatile",
  [GSK_SL_TOKEN_RESTRICT] = "restrict",
  [GSK_SL_TOKEN_READONLY] = "readonly",
  [GSK_SL_TOKEN_WRITEONLY] = "writeonly",
  [GSK_SL_TOKEN_DVEC2] = "dvec2",
  [GSK_SL_TOKEN_DVEC3] = "dvec3",
  [GSK_SL_TOKEN_DVEC4] = "dvec4",
  [GSK_SL_TOKEN_DMAT2] = "dmat2",
  [GSK_SL_TOKEN_DMAT3] = "dmat3",
  [GSK_SL_TOKEN_DMAT4] = "dmat4",
  [GSK_SL_TOKEN_NOPERSPECTIVE] = "noperspective",
  [GSK_SL_TOKEN_FLAT] = "flat",
  [GSK_SL_TOKEN_SMOOTH] = "smooth",
  [GSK_SL_TOKEN_LAYOUT] = "layout",
  [GSK_SL_TOKEN_MAT2X2] = "mat2x2",
  [GSK_SL_TOKEN_MAT2X3] = "mat2x3",
  [GSK_SL_TOKEN_MAT2X4] = "mat2x4",
  [GSK_SL_TOKEN_MAT3X2] = "mat3x2",
  [GSK_SL_TOKEN_MAT3X3] = "mat3x3",
  [GSK_SL_TOKEN_MAT3X4] = "mat3x4",
  [GSK_SL_TOKEN_MAT4X2] = "mat4x2",
  [GSK_SL_TOKEN_MAT4X3] = "mat4x3",
  [GSK_SL_TOKEN_MAT4X4] = "mat4x4",
  [GSK_SL_TOKEN_DMAT2X2] = "dmat2x2",
  [GSK_SL_TOKEN_DMAT2X3] = "dmat2x3",
  [GSK_SL_TOKEN_DMAT2X4] = "dmat2x4",
  [GSK_SL_TOKEN_DMAT3X2] = "dmat3x2",
  [GSK_SL_TOKEN_DMAT3X3] = "dmat3x3",
  [GSK_SL_TOKEN_DMAT3X4] = "dmat3x4",
  [GSK_SL_TOKEN_DMAT4X2] = "dmat4x2",
  [GSK_SL_TOKEN_DMAT4X3] = "dmat4x3",
  [GSK_SL_TOKEN_DMAT4X4] = "dmat4x4",
  [GSK_SL_TOKEN_ATOMIC_UINT] = "atomic_uint",
  [GSK_SL_TOKEN_SAMPLER1D] = "sampler1D",
  [GSK_SL_TOKEN_SAMPLER2D] = "sampler2D",
  [GSK_SL_TOKEN_SAMPLER3D] = "sampler3D",
  [GSK_SL_TOKEN_SAMPLERCUBE] = "samplerCube",
  [GSK_SL_TOKEN_SAMPLER1DSHADOW] = "sampler1DShadow",
  [GSK_SL_TOKEN_SAMPLER2DSHADOW] = "sampler2DShadow",
  [GSK_SL_TOKEN_SAMPLERCUBESHADOW] = "samplerCubeShadow",
  [GSK_SL_TOKEN_SAMPLER1DARRAY] = "sampler1DArray",
  [GSK_SL_TOKEN_SAMPLER2DARRAY] = "sampler2DArray",
  [GSK_SL_TOKEN_SAMPLER1DARRAYSHADOW] = "sampler1DArrayShadow",
  [GSK_SL_TOKEN_SAMPLER2DARRAYSHADOW] = "sampler2DArrayShadow",
  [GSK_SL_TOKEN_ISAMPLER1D] = "isampler1D",
  [GSK_SL_TOKEN_ISAMPLER2D] = "isampler2D",
  [GSK_SL_TOKEN_ISAMPLER3D] = "isampler3D",
  [GSK_SL_TOKEN_ISAMPLERCUBE] = "isamplerCube",
  [GSK_SL_TOKEN_ISAMPLER1DARRAY] = "isampler1DArray",
  [GSK_SL_TOKEN_ISAMPLER2DARRAY] = "isampler2DArray",
  [GSK_SL_TOKEN_USAMPLER1D] = "usampler1D",
  [GSK_SL_TOKEN_USAMPLER2D] = "usampler2D",
  [GSK_SL_TOKEN_USAMPLER3D] = "usampler3D",
  [GSK_SL_TOKEN_USAMPLERCUBE] = "usamplerCube",
  [GSK_SL_TOKEN_USAMPLER1DARRAY] = "usampler1DArray",
  [GSK_SL_TOKEN_USAMPLER2DARRAY] = "usampler2DArray",
  [GSK_SL_TOKEN_SAMPLER2DRECT] = "sampler2DRect",
  [GSK_SL_TOKEN_SAMPLER2DRECTSHADOW] = "sampler2DRectShadow",
  [GSK_SL_TOKEN_ISAMPLER2DRECT] = "isampler2DRect",
  [GSK_SL_TOKEN_USAMPLER2DRECT] = "usampler2DRect",
  [GSK_SL_TOKEN_SAMPLERBUFFER] = "samplerBuffer",
  [GSK_SL_TOKEN_ISAMPLERBUFFER] = "isamplerBuffer",
  [GSK_SL_TOKEN_USAMPLERBUFFER] = "usamplerBuffer",
  [GSK_SL_TOKEN_SAMPLERCUBEARRAY] = "samplerCubeArray",
  [GSK_SL_TOKEN_SAMPLERCUBEARRAYSHADOW] = "samplerCubeArrayShadow",
  [GSK_SL_TOKEN_ISAMPLERCUBEARRAY] = "isamplerCubeArray",
  [GSK_SL_TOKEN_USAMPLERCUBEARRAY] = "usamplerCubeArray",
  [GSK_SL_TOKEN_SAMPLER2DMS] = "sampler2DMS",
  [GSK_SL_TOKEN_ISAMPLER2DMS] = "isampler2DMS",
  [GSK_SL_TOKEN_USAMPLER2DMS] = "usampler2DMS",
  [GSK_SL_TOKEN_SAMPLER2DMSARRAY] = "sampler2DMSArray",
  [GSK_SL_TOKEN_ISAMPLER2DMSARRAY] = "isampler2DMSArray",
  [GSK_SL_TOKEN_USAMPLER2DMSARRAY] = "usampler2DMSArray",
  [GSK_SL_TOKEN_IMAGE1D] = "image1D",
  [GSK_SL_TOKEN_IIMAGE1D] = "iimage1D",
  [GSK_SL_TOKEN_UIMAGE1D] = "uimage1D",
  [GSK_SL_TOKEN_IMAGE2D] = "image2D",
  [GSK_SL_TOKEN_IIMAGE2D] = "iimage2D",
  [GSK_SL_TOKEN_UIMAGE2D] = "uimage2D",
  [GSK_SL_TOKEN_IMAGE3D] = "image3D",
  [GSK_SL_TOKEN_IIMAGE3D] = "iimage3D",
  [GSK_SL_TOKEN_UIMAGE3D] = "uimage3D",
  [GSK_SL_TOKEN_IMAGE2DRECT] = "image2DRect",
  [GSK_SL_TOKEN_IIMAGE2DRECT] = "iimage2DRect",
  [GSK_SL_TOKEN_UIMAGE2DRECT] = "uimage2DRect",
  [GSK_SL_TOKEN_IMAGECUBE] = "imageCube",
  [GSK_SL_TOKEN_IIMAGECUBE] = "iimageCube",
  [GSK_SL_TOKEN_UIMAGECUBE] = "uimageCube",
  [GSK_SL_TOKEN_IMAGEBUFFER] = "imageBuffer",
  [GSK_SL_TOKEN_IIMAGEBUFFER] = "iimageBuffer",
  [GSK_SL_TOKEN_UIMAGEBUFFER] = "uimageBuffer",
  [GSK_SL_TOKEN_IMAGE1DARRAY] = "image1DArray",
  [GSK_SL_TOKEN_IIMAGE1DARRAY] = "iimage1DArray",
  [GSK_SL_TOKEN_UIMAGE1DARRAY] = "uimage1DArray",
  [GSK_SL_TOKEN_IMAGE2DARRAY] = "image2DArray",
  [GSK_SL_TOKEN_IIMAGE2DARRAY] = "iimage2DArray",
  [GSK_SL_TOKEN_UIMAGE2DARRAY] = "uimage2DArray",
  [GSK_SL_TOKEN_IMAGECUBEARRAY] = "imageCubeArray",
  [GSK_SL_TOKEN_IIMAGECUBEARRAY] = "iimageCubeArray",
  [GSK_SL_TOKEN_UIMAGECUBEARRAY] = "uimageCubeArray",
  [GSK_SL_TOKEN_IMAGE2DMS] = "image2DMS",
  [GSK_SL_TOKEN_IIMAGE2DMS] = "iimage2DMS",
  [GSK_SL_TOKEN_UIMAGE2DMS] = "uimage2DMS",
  [GSK_SL_TOKEN_IMAGE2DMSARRAY] = "image2DMSArray",
  [GSK_SL_TOKEN_IIMAGE2DMSARRAY] = "iimage2DMSArray",
  [GSK_SL_TOKEN_UIMAGE2DMSARRAY] = "uimage2DMSArray",
  [GSK_SL_TOKEN_STRUCT] = "struct",
  [GSK_SL_TOKEN_VOID] = "void",
  [GSK_SL_TOKEN_WHILE] = "while",
  [GSK_SL_TOKEN_INVARIANT] = "invariant",
  [GSK_SL_TOKEN_PRECISE] = "precise",
  [GSK_SL_TOKEN_HIGH_PRECISION] = "highp",
  [GSK_SL_TOKEN_MEDIUM_PRECISION] = "mediump",
  [GSK_SL_TOKEN_LOW_PRECISION] = "lowp",
  [GSK_SL_TOKEN_PRECISION] = "precision"
};

void
gsk_sl_token_print (const GskSlToken *token,
                    GString          *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (token->type)
    {
    case GSK_SL_TOKEN_EOF:
    case GSK_SL_TOKEN_ERROR:
    case GSK_SL_TOKEN_COMMENT:
    case GSK_SL_TOKEN_SINGLE_LINE_COMMENT:
      break;

    case GSK_SL_TOKEN_NEWLINE:
    case GSK_SL_TOKEN_WHITESPACE:
      g_string_append (string, " ");
      break;

    case GSK_SL_TOKEN_FLOAT:
    case GSK_SL_TOKEN_DOUBLE:
    case GSK_SL_TOKEN_CONST:
    case GSK_SL_TOKEN_BOOL:
    case GSK_SL_TOKEN_INT:
    case GSK_SL_TOKEN_UINT:
    case GSK_SL_TOKEN_BREAK:
    case GSK_SL_TOKEN_CONTINUE:
    case GSK_SL_TOKEN_DO:
    case GSK_SL_TOKEN_ELSE:
    case GSK_SL_TOKEN_FOR:
    case GSK_SL_TOKEN_IF:
    case GSK_SL_TOKEN_DISCARD:
    case GSK_SL_TOKEN_RETURN:
    case GSK_SL_TOKEN_SWITCH:
    case GSK_SL_TOKEN_CASE:
    case GSK_SL_TOKEN_DEFAULT:
    case GSK_SL_TOKEN_SUBROUTINE:
    case GSK_SL_TOKEN_BVEC2:
    case GSK_SL_TOKEN_BVEC3:
    case GSK_SL_TOKEN_BVEC4:
    case GSK_SL_TOKEN_IVEC2:
    case GSK_SL_TOKEN_IVEC3:
    case GSK_SL_TOKEN_IVEC4:
    case GSK_SL_TOKEN_UVEC2:
    case GSK_SL_TOKEN_UVEC3:
    case GSK_SL_TOKEN_UVEC4:
    case GSK_SL_TOKEN_VEC2:
    case GSK_SL_TOKEN_VEC3:
    case GSK_SL_TOKEN_VEC4:
    case GSK_SL_TOKEN_MAT2:
    case GSK_SL_TOKEN_MAT3:
    case GSK_SL_TOKEN_MAT4:
    case GSK_SL_TOKEN_CENTROID:
    case GSK_SL_TOKEN_IN:
    case GSK_SL_TOKEN_OUT:
    case GSK_SL_TOKEN_INOUT:
    case GSK_SL_TOKEN_UNIFORM:
    case GSK_SL_TOKEN_PATCH:
    case GSK_SL_TOKEN_SAMPLE:
    case GSK_SL_TOKEN_BUFFER:
    case GSK_SL_TOKEN_SHARED:
    case GSK_SL_TOKEN_COHERENT:
    case GSK_SL_TOKEN_VOLATILE:
    case GSK_SL_TOKEN_RESTRICT:
    case GSK_SL_TOKEN_READONLY:
    case GSK_SL_TOKEN_WRITEONLY:
    case GSK_SL_TOKEN_DVEC2:
    case GSK_SL_TOKEN_DVEC3:
    case GSK_SL_TOKEN_DVEC4:
    case GSK_SL_TOKEN_DMAT2:
    case GSK_SL_TOKEN_DMAT3:
    case GSK_SL_TOKEN_DMAT4:
    case GSK_SL_TOKEN_NOPERSPECTIVE:
    case GSK_SL_TOKEN_FLAT:
    case GSK_SL_TOKEN_SMOOTH:
    case GSK_SL_TOKEN_LAYOUT:
    case GSK_SL_TOKEN_MAT2X2:
    case GSK_SL_TOKEN_MAT2X3:
    case GSK_SL_TOKEN_MAT2X4:
    case GSK_SL_TOKEN_MAT3X2:
    case GSK_SL_TOKEN_MAT3X3:
    case GSK_SL_TOKEN_MAT3X4:
    case GSK_SL_TOKEN_MAT4X2:
    case GSK_SL_TOKEN_MAT4X3:
    case GSK_SL_TOKEN_MAT4X4:
    case GSK_SL_TOKEN_DMAT2X2:
    case GSK_SL_TOKEN_DMAT2X3:
    case GSK_SL_TOKEN_DMAT2X4:
    case GSK_SL_TOKEN_DMAT3X2:
    case GSK_SL_TOKEN_DMAT3X3:
    case GSK_SL_TOKEN_DMAT3X4:
    case GSK_SL_TOKEN_DMAT4X2:
    case GSK_SL_TOKEN_DMAT4X3:
    case GSK_SL_TOKEN_DMAT4X4:
    case GSK_SL_TOKEN_ATOMIC_UINT:
    case GSK_SL_TOKEN_SAMPLER1D:
    case GSK_SL_TOKEN_SAMPLER2D:
    case GSK_SL_TOKEN_SAMPLER3D:
    case GSK_SL_TOKEN_SAMPLERCUBE:
    case GSK_SL_TOKEN_SAMPLER1DSHADOW:
    case GSK_SL_TOKEN_SAMPLER2DSHADOW:
    case GSK_SL_TOKEN_SAMPLERCUBESHADOW:
    case GSK_SL_TOKEN_SAMPLER1DARRAY:
    case GSK_SL_TOKEN_SAMPLER2DARRAY:
    case GSK_SL_TOKEN_SAMPLER1DARRAYSHADOW:
    case GSK_SL_TOKEN_SAMPLER2DARRAYSHADOW:
    case GSK_SL_TOKEN_ISAMPLER1D:
    case GSK_SL_TOKEN_ISAMPLER2D:
    case GSK_SL_TOKEN_ISAMPLER3D:
    case GSK_SL_TOKEN_ISAMPLERCUBE:
    case GSK_SL_TOKEN_ISAMPLER1DARRAY:
    case GSK_SL_TOKEN_ISAMPLER2DARRAY:
    case GSK_SL_TOKEN_USAMPLER1D:
    case GSK_SL_TOKEN_USAMPLER2D:
    case GSK_SL_TOKEN_USAMPLER3D:
    case GSK_SL_TOKEN_USAMPLERCUBE:
    case GSK_SL_TOKEN_USAMPLER1DARRAY:
    case GSK_SL_TOKEN_USAMPLER2DARRAY:
    case GSK_SL_TOKEN_SAMPLER2DRECT:
    case GSK_SL_TOKEN_SAMPLER2DRECTSHADOW:
    case GSK_SL_TOKEN_ISAMPLER2DRECT:
    case GSK_SL_TOKEN_USAMPLER2DRECT:
    case GSK_SL_TOKEN_SAMPLERBUFFER:
    case GSK_SL_TOKEN_ISAMPLERBUFFER:
    case GSK_SL_TOKEN_USAMPLERBUFFER:
    case GSK_SL_TOKEN_SAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_SAMPLERCUBEARRAYSHADOW:
    case GSK_SL_TOKEN_ISAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_USAMPLERCUBEARRAY:
    case GSK_SL_TOKEN_SAMPLER2DMS:
    case GSK_SL_TOKEN_ISAMPLER2DMS:
    case GSK_SL_TOKEN_USAMPLER2DMS:
    case GSK_SL_TOKEN_SAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_ISAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_USAMPLER2DMSARRAY:
    case GSK_SL_TOKEN_IMAGE1D:
    case GSK_SL_TOKEN_IIMAGE1D:
    case GSK_SL_TOKEN_UIMAGE1D:
    case GSK_SL_TOKEN_IMAGE2D:
    case GSK_SL_TOKEN_IIMAGE2D:
    case GSK_SL_TOKEN_UIMAGE2D:
    case GSK_SL_TOKEN_IMAGE3D:
    case GSK_SL_TOKEN_IIMAGE3D:
    case GSK_SL_TOKEN_UIMAGE3D:
    case GSK_SL_TOKEN_IMAGE2DRECT:
    case GSK_SL_TOKEN_IIMAGE2DRECT:
    case GSK_SL_TOKEN_UIMAGE2DRECT:
    case GSK_SL_TOKEN_IMAGECUBE:
    case GSK_SL_TOKEN_IIMAGECUBE:
    case GSK_SL_TOKEN_UIMAGECUBE:
    case GSK_SL_TOKEN_IMAGEBUFFER:
    case GSK_SL_TOKEN_IIMAGEBUFFER:
    case GSK_SL_TOKEN_UIMAGEBUFFER:
    case GSK_SL_TOKEN_IMAGE1DARRAY:
    case GSK_SL_TOKEN_IIMAGE1DARRAY:
    case GSK_SL_TOKEN_UIMAGE1DARRAY:
    case GSK_SL_TOKEN_IMAGE2DARRAY:
    case GSK_SL_TOKEN_IIMAGE2DARRAY:
    case GSK_SL_TOKEN_UIMAGE2DARRAY:
    case GSK_SL_TOKEN_IMAGECUBEARRAY:
    case GSK_SL_TOKEN_IIMAGECUBEARRAY:
    case GSK_SL_TOKEN_UIMAGECUBEARRAY:
    case GSK_SL_TOKEN_IMAGE2DMS:
    case GSK_SL_TOKEN_IIMAGE2DMS:
    case GSK_SL_TOKEN_UIMAGE2DMS:
    case GSK_SL_TOKEN_IMAGE2DMSARRAY:
    case GSK_SL_TOKEN_IIMAGE2DMSARRAY:
    case GSK_SL_TOKEN_UIMAGE2DMSARRAY:
    case GSK_SL_TOKEN_STRUCT:
    case GSK_SL_TOKEN_VOID:
    case GSK_SL_TOKEN_WHILE:
    case GSK_SL_TOKEN_INVARIANT:
    case GSK_SL_TOKEN_PRECISE:
    case GSK_SL_TOKEN_HIGH_PRECISION:
    case GSK_SL_TOKEN_MEDIUM_PRECISION:
    case GSK_SL_TOKEN_LOW_PRECISION:
    case GSK_SL_TOKEN_PRECISION:
      g_assert (keywords[token->type] != NULL);
      g_string_append (string, keywords[token->type]);
      break;

    case GSK_SL_TOKEN_IDENTIFIER:
      g_string_append (string, token->str);
      break;

    case GSK_SL_TOKEN_STRING:
      g_string_append_c (string, '"');
      g_string_append (string, token->str);
      g_string_append_c (string, '"');
      break;

    case GSK_SL_TOKEN_FLOATCONSTANT:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->f);
      g_string_append (string, buf);
      if (strchr (buf, '.') == NULL)
        g_string_append (string, ".0");
      g_string_append (string, "f");
      break;

    case GSK_SL_TOKEN_DOUBLECONSTANT:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, token->d);
      g_string_append (string, buf);
      if (strchr (buf, '.') == NULL)
        g_string_append (string, ".0");
      break;

    case GSK_SL_TOKEN_INTCONSTANT:
      g_string_append_printf (string, "%" G_GINT32_FORMAT, token->i32);
      break;

    case GSK_SL_TOKEN_UINTCONSTANT:
      g_string_append_printf (string, "%" G_GUINT32_FORMAT"u", token->u32);
      break;

    case GSK_SL_TOKEN_BOOLCONSTANT:
      g_string_append (string, token->b ? "true" : "false");
      break;

    case GSK_SL_TOKEN_LEFT_OP:
      g_string_append (string, "<<");
      break;

    case GSK_SL_TOKEN_RIGHT_OP:
      g_string_append (string, ">>");
      break;

    case GSK_SL_TOKEN_INC_OP:
      g_string_append (string, "++");
      break;

    case GSK_SL_TOKEN_DEC_OP:
      g_string_append (string, "--");
      break;

    case GSK_SL_TOKEN_LE_OP:
      g_string_append (string, "<=");
      break;

    case GSK_SL_TOKEN_GE_OP:
      g_string_append (string, ">=");
      break;

    case GSK_SL_TOKEN_EQ_OP:
      g_string_append (string, "==");
      break;

    case GSK_SL_TOKEN_NE_OP:
      g_string_append (string, "!=");
      break;

    case GSK_SL_TOKEN_AND_OP:
      g_string_append (string, "&&");
      break;

    case GSK_SL_TOKEN_OR_OP:
      g_string_append (string, "||");
      break;

    case GSK_SL_TOKEN_XOR_OP:
      g_string_append (string, "^^");
      break;

    case GSK_SL_TOKEN_MUL_ASSIGN:
      g_string_append (string, "*=");
      break;

    case GSK_SL_TOKEN_DIV_ASSIGN:
      g_string_append (string, "/=");
      break;

    case GSK_SL_TOKEN_ADD_ASSIGN:
      g_string_append (string, "+=");
      break;

    case GSK_SL_TOKEN_MOD_ASSIGN:
      g_string_append (string, "%=");
      break;

    case GSK_SL_TOKEN_LEFT_ASSIGN:
      g_string_append (string, "<<=");
      break;

    case GSK_SL_TOKEN_RIGHT_ASSIGN:
      g_string_append (string, ">>=");
      break;

    case GSK_SL_TOKEN_AND_ASSIGN:
      g_string_append (string, "&=");
      break;

    case GSK_SL_TOKEN_XOR_ASSIGN:
      g_string_append (string, "^=");
      break;

    case GSK_SL_TOKEN_OR_ASSIGN:
      g_string_append (string, "|=");
      break;

    case GSK_SL_TOKEN_SUB_ASSIGN:
      g_string_append (string, "-=");
      break;

    case GSK_SL_TOKEN_LEFT_PAREN:
      g_string_append_c (string, '(');
      break;

    case GSK_SL_TOKEN_RIGHT_PAREN:
      g_string_append_c (string, ')');
      break;

    case GSK_SL_TOKEN_LEFT_BRACKET:
      g_string_append_c (string, '[');
      break;

    case GSK_SL_TOKEN_RIGHT_BRACKET:
      g_string_append_c (string, ']');
      break;

    case GSK_SL_TOKEN_LEFT_BRACE:
      g_string_append_c (string, '{');
      break;

    case GSK_SL_TOKEN_RIGHT_BRACE:
      g_string_append_c (string, '}');
      break;

    case GSK_SL_TOKEN_DOT:
      g_string_append_c (string, '.');
      break;

    case GSK_SL_TOKEN_COMMA:
      g_string_append_c (string, ',');
      break;

    case GSK_SL_TOKEN_COLON:
      g_string_append_c (string, ':');
      break;

    case GSK_SL_TOKEN_EQUAL:
      g_string_append_c (string, '=');
      break;

    case GSK_SL_TOKEN_SEMICOLON:
      g_string_append_c (string, ';');
      break;

    case GSK_SL_TOKEN_BANG:
      g_string_append_c (string, '!');
      break;

    case GSK_SL_TOKEN_DASH:
      g_string_append_c (string, '-');
      break;

    case GSK_SL_TOKEN_TILDE:
      g_string_append_c (string, '~');
      break;

    case GSK_SL_TOKEN_PLUS:
      g_string_append_c (string, '+');
      break;

    case GSK_SL_TOKEN_STAR:
      g_string_append_c (string, '*');
      break;

    case GSK_SL_TOKEN_SLASH:
      g_string_append_c (string, '/');
      break;

    case GSK_SL_TOKEN_PERCENT:
      g_string_append_c (string, '%');
      break;

    case GSK_SL_TOKEN_LEFT_ANGLE:
      g_string_append_c (string, '<');
      break;

    case GSK_SL_TOKEN_RIGHT_ANGLE:
      g_string_append_c (string, '>');
      break;

    case GSK_SL_TOKEN_VERTICAL_BAR:
      g_string_append_c (string, '|');
      break;

    case GSK_SL_TOKEN_CARET:
      g_string_append_c (string, '^');
      break;

    case GSK_SL_TOKEN_AMPERSAND:
      g_string_append_c (string, '&');
      break;

    case GSK_SL_TOKEN_QUESTION:
      g_string_append_c (string, '?');
      break;

    case GSK_SL_TOKEN_HASH:
      g_string_append_c (string, '#');
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

char *
gsk_sl_token_to_string (const GskSlToken *token)
{
  GString *string;

  string = g_string_new (NULL);
  gsk_sl_token_print (token, string);
  return g_string_free (string, FALSE);
}

static void
gsk_sl_token_init (GskSlToken     *token,
                   GskSlTokenType  type)
{
  token->type = type;
}

static void
gsk_sl_token_init_float (GskSlToken     *token,
                         GskSlTokenType  type,
                         double          d)
{
  gsk_sl_token_init (token, type);

  if (type == GSK_SL_TOKEN_FLOATCONSTANT)
    {
      token->f = d;
    }
  else if (type == GSK_SL_TOKEN_DOUBLECONSTANT)
    {
      token->d = d;
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
gsk_sl_token_init_number (GskSlToken     *token,
                          GskSlTokenType  type,
                          guint64         number)
{
  gsk_sl_token_init (token, type);

  if (type == GSK_SL_TOKEN_INTCONSTANT)
    {
      token->i32 = number;
    }
  else if (type == GSK_SL_TOKEN_UINTCONSTANT)
    {
      token->u32 = number;
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
gsk_sl_token_reader_init (GskSlTokenReader *reader,
                          GskCodeSource    *source,
                          GBytes           *bytes)
{
  reader->data = g_bytes_get_data (bytes, NULL);
  reader->end = reader->data + g_bytes_get_size (bytes);

  gsk_code_location_init (&reader->position, source);
}

static void
gsk_sl_token_reader_init_copy (GskSlTokenReader       *reader,
                               const GskSlTokenReader *source)
{
  *reader = *source;
}

static inline gsize
gsk_sl_token_reader_remaining (const GskSlTokenReader *reader)
{
  return reader->end - reader->data;
}

static inline gboolean
is_newline (char c)
{
  return c == '\n'
      || c == '\r';
}

static gboolean
is_identifier_start (char c)
{
   return g_ascii_isalpha (c)
       || c == '_';
}

static gboolean
is_identifier (char c)
{
  return is_identifier_start (c)
      || g_ascii_isdigit (c);
}

static inline gboolean
is_whitespace (char c)
{
  return c == ' '
      || c == '\t'
      || c == '\f'
      || is_newline (c);
}

static inline gsize
gsk_sl_token_reader_forward (GskSlTokenReader *reader,
                             gsize             n)
{
  gsize i, len;

  i = 0;
  len = reader->end - reader->data;

  for (; n; n--)
    {
      /* Skip '\' + newline */
      if (len - i > 1 &&
          reader->data[i] == '\\' && 
          is_newline (reader->data[i + 1]))
      {
        i += 2;
        if (len - i > 0 &&
            is_newline (reader->data[i]) &&
            reader->data[i] != reader->data[i - 1])
          i++;
      }

      if (i >= len)
        return i;

      i++;
    }

  return i;
}

static inline char
gsk_sl_token_reader_get (GskSlTokenReader *reader,
                         gsize             n)
{
  gsize offset;
  
  offset = gsk_sl_token_reader_forward (reader, n);
  
  if (offset >= reader->end - reader->data)
    return 0;

  return reader->data[offset];
}

static inline void
gsk_sl_token_reader_consume (GskSlTokenReader *reader,
                             gsize             n)
{
  gsize i, offset;
  
  offset = gsk_sl_token_reader_forward (reader, n);

  for (i = 0; i < offset; i++)
    {
      if (!is_newline (reader->data[i]))
        {
          gsk_code_location_advance (&reader->position, 1, 1);
        }
      else
        {
          if (reader->end - reader->data > i + 2 &&
              is_newline (reader->data[i + 1]) && 
              reader->data[i] != reader->data[i + 1])
            gsk_code_location_advance_newline (&reader->position, 2);
          else
            gsk_code_location_advance_newline (&reader->position, 1);
        }
    }
  
  reader->data += offset;
}

static void
gsk_sl_tokenizer_emit_error (GskSlTokenizer        *tokenizer,
                             const GskCodeLocation *location,
                             const GskSlToken      *token,
                             const GError          *error)
{
  if (tokenizer->error_func)
    tokenizer->error_func (tokenizer, TRUE, location, token, error, tokenizer->user_data);
  else
    g_warning ("Unhandled GLSL error: %zu:%zu: %s", location->lines + 1, location->line_chars + 1, error->message);
}

GskSlTokenizer *
gsk_sl_tokenizer_new (GskCodeSource           *source,
                      GskSlTokenizerErrorFunc  func,
                      gpointer                 user_data,
                      GDestroyNotify           user_destroy)
{
  GskSlTokenizer *tokenizer;
  GError *error = NULL;

  tokenizer = g_slice_new0 (GskSlTokenizer);
  tokenizer->ref_count = 1;
  tokenizer->source = g_object_ref (source);
  tokenizer->error_func = func;
  tokenizer->user_data = user_data;
  tokenizer->user_destroy = user_destroy;

  tokenizer->bytes = gsk_code_source_load (source, &error);
  if (tokenizer->bytes == NULL)
    {
      GskCodeLocation location;
      GskSlToken token;

      gsk_code_location_init (&location, tokenizer->source);
      gsk_sl_token_init (&token, GSK_SL_TOKEN_EOF);
      gsk_sl_tokenizer_emit_error (tokenizer, &location, &token, error);
      g_error_free (error);
      tokenizer->bytes = g_bytes_new (NULL, 0);
    }

  gsk_sl_token_reader_init (&tokenizer->reader, source, tokenizer->bytes);

  return tokenizer;
}

GskSlTokenizer *
gsk_sl_tokenizer_ref (GskSlTokenizer *tokenizer)
{
  tokenizer->ref_count++;
  
  return tokenizer;
}

void
gsk_sl_tokenizer_unref (GskSlTokenizer *tokenizer)
{
  tokenizer->ref_count--;
  if (tokenizer->ref_count > 0)
    return;

  if (tokenizer->user_destroy)
    tokenizer->user_destroy (tokenizer->user_data);

  g_bytes_unref (tokenizer->bytes);
  g_object_unref (tokenizer->source);

  g_slice_free (GskSlTokenizer, tokenizer);
}

const GskCodeLocation *
gsk_sl_tokenizer_get_location (GskSlTokenizer *tokenizer)
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
  *error = g_error_new_valist (GSK_SL_COMPILER_ERROR,
                               GSK_SL_COMPILER_ERROR_TOKENIZER,
                               format,
                               args);
  va_end (args);
}

static void
gsk_sl_token_reader_read_multi_line_comment (GskSlTokenReader  *reader,
                                             GskSlToken        *token,
                                             GError           **error)
{
  gsk_sl_token_reader_consume (reader, 2);

  while (reader->data < reader->end)
    {
      if (gsk_sl_token_reader_get (reader, 0) == '*' &&
          gsk_sl_token_reader_get (reader, 1) == '/')
        {
          gsk_sl_token_reader_consume (reader, 2);
          gsk_sl_token_init (token, GSK_SL_TOKEN_COMMENT);
          return;
        }
      gsk_sl_token_reader_consume (reader, 1);
    }

  gsk_sl_token_init (token, GSK_SL_TOKEN_COMMENT);
  set_parse_error (error, "Unterminated comment at end of document.");
}

static void
gsk_sl_token_reader_read_single_line_comment (GskSlTokenReader  *reader,
                                              GskSlToken        *token,
                                              GError           **error)
{
  char c;

  gsk_sl_token_reader_consume (reader, 2);

  for (c = gsk_sl_token_reader_get (reader, 0);
       c != 0 && !is_newline (c);
       c = gsk_sl_token_reader_get (reader, 0))
    {
      gsk_sl_token_reader_consume (reader, 1);
    }

  gsk_sl_token_init (token, GSK_SL_TOKEN_SINGLE_LINE_COMMENT);
}

static void
gsk_sl_token_reader_read_whitespace (GskSlTokenReader  *reader,
                                     GskSlToken        *token)
{
  gboolean has_newline = FALSE;
  char c;

  for (c = gsk_sl_token_reader_get (reader, 0);
       is_whitespace (c);
       c = gsk_sl_token_reader_get (reader, 0))
    {
      has_newline |= is_newline (c);
      gsk_sl_token_reader_consume (reader, 1);
    }

  gsk_sl_token_init (token, has_newline ? GSK_SL_TOKEN_NEWLINE : GSK_SL_TOKEN_WHITESPACE);
}

static gboolean
gsk_sl_token_reader_read_float_number (GskSlTokenReader  *reader,
                                       GskSlToken        *token,
                                       GError           **error)
{
  int exponent_sign = 0;
  guint64 integer = 0;
  gint64 fractional = 0, fractional_length = 1, exponent = 0;
  gboolean is_int = TRUE, overflow = FALSE;
  char c;
  guint i;

  for (i = 0; ; i++)
    {
      c = gsk_sl_token_reader_get (reader, i);

      if (!g_ascii_isdigit (c))
        break;
      if (integer > G_MAXUINT64 / 10)
        overflow = TRUE;

      integer = 10 * integer + g_ascii_digit_value (c);
    }

  if (c == '.')
    {
      is_int = FALSE;

      for (i = i + 1; ; i++)
        {
          c = gsk_sl_token_reader_get (reader, i);

          if (!g_ascii_isdigit (c))
            break;

          if (fractional_length < G_MAXINT64 / 10)
            {
              fractional = 10 * fractional + g_ascii_digit_value (c);
              fractional_length *= 10;
            }
        }
    }

  if (c == 'e' || c == 'E')
    {
      is_int = FALSE;

      c = gsk_sl_token_reader_get (reader, i + 1);

      if (c == '-')
        {
          exponent_sign = -1;
          c = gsk_sl_token_reader_get (reader, i + 2);
        }
      else if (c == '+')
        {
          exponent_sign = 1;
          c = gsk_sl_token_reader_get (reader, i + 2);
        }

      if (g_ascii_isdigit (c))
        {
          if (exponent_sign == 0)
            {
              i++;
              exponent_sign = 1;
            }
          else
            {
              i += 2;
            }

          for (i = 0; ; i++)
            {
              c = gsk_sl_token_reader_get (reader, i);

              if (!g_ascii_isdigit (c))
                break;

              exponent = 10 * exponent + g_ascii_digit_value (c);
            }
        }
      else
        {
          c = gsk_sl_token_reader_get (reader, i);
        }
    }

  gsk_sl_token_reader_consume (reader, i);

  if (is_int)
    {
      if (integer > G_MAXUINT32)
        overflow = TRUE;

      if (c == 'U' || c == 'u')
        {
          gsk_sl_token_reader_consume (reader, 1);
          gsk_sl_token_init_number (token, GSK_SL_TOKEN_UINTCONSTANT, integer);
        }
      else
        {
          gsk_sl_token_init_number (token, GSK_SL_TOKEN_INTCONSTANT, integer);
        }

      if (overflow)
        {
          set_parse_error (error, "Overflow in integer constant");
          return FALSE;
        }

      return TRUE;
    }
  else
    {
      double d = (integer + ((double) fractional / fractional_length)) * pow (10, exponent_sign * exponent);

      if (c == 'f' || c == 'F')
        {
          gsk_sl_token_reader_consume (reader, 1);
          gsk_sl_token_init_float (token, GSK_SL_TOKEN_FLOATCONSTANT, d);
        }
      else if ((c == 'l' && gsk_sl_token_reader_get (reader, 1) == 'f') ||
               (c == 'L' && gsk_sl_token_reader_get (reader, 1) == 'F'))
        {
          gsk_sl_token_reader_consume (reader, 2);
          gsk_sl_token_init_float (token, GSK_SL_TOKEN_DOUBLECONSTANT, d);
        }
      else
        {
          gsk_sl_token_init_float (token, GSK_SL_TOKEN_FLOATCONSTANT, d);
        }

      if (overflow)
        {
          set_parse_error (error, "Overflow in floating point constant");
          return FALSE;
        }

      return TRUE;
    }
}

static gboolean
gsk_sl_token_reader_read_hex_number (GskSlTokenReader  *reader,
                                     GskSlToken        *token,
                                     GError           **error)
{
  char c;
  guint64 result = 0;
  gboolean overflow = FALSE;

  for (c = gsk_sl_token_reader_get (reader, 0);
       g_ascii_isxdigit (c);
       c = gsk_sl_token_reader_get (reader, 0))
    {
      if (result > G_MAXUINT32 / 16)
        overflow = TRUE;

      result = result * 16 + g_ascii_xdigit_value (c);
      gsk_sl_token_reader_consume (reader, 1);
    }

  if (c == 'U' || c == 'u')
    {
      gsk_sl_token_reader_consume (reader, 1);
      gsk_sl_token_init_number (token, GSK_SL_TOKEN_UINTCONSTANT, result);
    }
  else
    {
      gsk_sl_token_init_number (token, GSK_SL_TOKEN_INTCONSTANT, result);
    }

  if (overflow)
    {
      set_parse_error (error, "Overflow in integer constant");
      return FALSE;
    }

  return TRUE;
}

static gboolean
gsk_sl_token_reader_read_octal_number (GskSlTokenReader  *reader,
                                       GskSlToken        *token,
                                       GError           **error)
{
  char c;
  guint i;
  guint64 result = 0;
  gboolean overflow = FALSE;

  for (i = 0; ; i++)
    {
      c = gsk_sl_token_reader_get (reader, i);
      if (c < '0' || c > '7')
        break;

      if (result > G_MAXUINT32 / 8)
        overflow = TRUE;

      result = result * 8 + g_ascii_digit_value (c);
    }

  if (c == 'U' || c == 'u')
    {
      gsk_sl_token_reader_consume (reader, i + 1);
      gsk_sl_token_init_number (token, GSK_SL_TOKEN_UINTCONSTANT, result);
    }
  else if (c == '.' || c == 'e' || c == 'f' || c == 'E' || c == 'F')
    {
      return gsk_sl_token_reader_read_float_number (reader, token, error);
    }
  else
    {
      gsk_sl_token_reader_consume (reader, i);
      gsk_sl_token_init_number (token, GSK_SL_TOKEN_INTCONSTANT, result);
    }

  if (overflow)
    {
      set_parse_error (error, "Overflow in octal constant");
      return FALSE;
    }

  return TRUE;
}

static gboolean
gsk_sl_token_reader_read_number (GskSlTokenReader  *reader,
                                 GskSlToken        *token,
                                 GError           **error)
{
  if (gsk_sl_token_reader_get (reader, 0) == '0')
    {
      char c = gsk_sl_token_reader_get (reader, 1);
      if (c == 'x' || c == 'X')
        {
          if (!g_ascii_isxdigit (gsk_sl_token_reader_get (reader, 3)))
            {
              gsk_sl_token_init_number (token, GSK_SL_TOKEN_INTCONSTANT, 0);
              gsk_sl_token_reader_consume (reader, 1);
              return TRUE;
            }
          gsk_sl_token_reader_consume (reader, 2);
          return gsk_sl_token_reader_read_hex_number (reader, token, error);
        }
      else
        {
          return gsk_sl_token_reader_read_octal_number (reader, token, error);
        }
    }

  return gsk_sl_token_reader_read_float_number (reader, token, error);
}

static void
gsk_sl_token_reader_read_identifier (GskSlTokenReader  *reader,
                                     GskSlToken        *token)
{
  GString *string;
  char c;

  string = g_string_new ("");

  for (c = gsk_sl_token_reader_get (reader, 0);
       is_identifier (c);
       c = gsk_sl_token_reader_get (reader, 0))
    {
      g_string_append_c (string, c);
      gsk_sl_token_reader_consume (reader, 1);
    }

  gsk_sl_token_init (token, GSK_SL_TOKEN_IDENTIFIER);
  token->str = g_string_free (string, FALSE);
}

static void
gsk_sl_token_reader_read_string (GskSlTokenReader  *reader,
                                 GskSlToken        *token,
                                 GError           **error)
{
  GString *string;
  char c;

  g_assert (gsk_sl_token_reader_get (reader, 0) == '"');
  gsk_sl_token_reader_consume (reader, 1);

  string = g_string_new ("");

  for (c = gsk_sl_token_reader_get (reader, 0);
       c != '"' && c != 0;
       c = gsk_sl_token_reader_get (reader, 0))
    {
      g_string_append_c (string, c);
      gsk_sl_token_reader_consume (reader, 1);
    }

  if (c == 0)
    set_parse_error (error, "Unterminated string literal.");
  else
    gsk_sl_token_reader_consume (reader, 1);

  gsk_sl_token_init (token, GSK_SL_TOKEN_STRING);
  token->str = g_string_free (string, FALSE);
}

gboolean
gsk_sl_string_is_valid_identifier (const char *ident)
{
  guint i;

  if (!is_identifier_start (ident[0]))
    return FALSE;

  for (i = 1; ident[i]; i++)
    {
      if (!is_identifier (ident[i]))
        return FALSE;
    }

  return TRUE;
}

void
gsk_sl_token_init_from_identifier (GskSlToken *token,
                                   const char *ident)
{
  guint i;
  if (g_str_equal (ident, "true"))
    {
      gsk_sl_token_init (token, GSK_SL_TOKEN_BOOLCONSTANT);
      token->b = TRUE;
      return;
    }
  else if (g_str_equal (ident, "false"))
    {
      gsk_sl_token_init (token, GSK_SL_TOKEN_BOOLCONSTANT);
      token->b = FALSE;
      return;
    }

  for (i = 0; i < G_N_ELEMENTS (keywords); i++)
    {
      if (keywords[i] == NULL)
        continue;

      if (g_str_equal (ident, keywords[i]))
        {
          gsk_sl_token_init (token, i);
          return;
        }
    }

  gsk_sl_token_init (token, GSK_SL_TOKEN_IDENTIFIER);
  token->str = g_strdup (ident);
}

gboolean
gsk_sl_token_is_skipped (const GskSlToken *token)
{
  return gsk_sl_token_is (token, GSK_SL_TOKEN_ERROR)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_NEWLINE)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_WHITESPACE)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_COMMENT)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_SINGLE_LINE_COMMENT);
}

void
gsk_sl_tokenizer_read_token (GskSlTokenizer *tokenizer,
                             GskSlToken     *token)
{
  GskSlTokenReader reader;
  GError *error = NULL;
  char c;

  gsk_sl_token_reader_init_copy (&reader, &tokenizer->reader);
  c = gsk_sl_token_reader_get (&reader, 0);

  switch (c)
    {
    case 0:
      gsk_sl_token_init (token, GSK_SL_TOKEN_EOF);
      break;

    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case ' ':
      gsk_sl_token_reader_read_whitespace (&reader, token);
      break;

    case '/':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '/':
          gsk_sl_token_reader_read_single_line_comment (&reader, token, &error);
          break;
        case '*':
          gsk_sl_token_reader_read_multi_line_comment (&reader, token, &error);
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_DIV_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_SLASH);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '"':
      gsk_sl_token_reader_read_string (&reader, token, &error);
      break;

    case '<':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '<':
          if (gsk_sl_token_reader_get (&reader, 2) == '=')
            {
              gsk_sl_token_init (token, GSK_SL_TOKEN_LEFT_ASSIGN);
              gsk_sl_token_reader_consume (&reader, 3);
            }
          else
            {
              gsk_sl_token_init (token, GSK_SL_TOKEN_LEFT_OP);
              gsk_sl_token_reader_consume (&reader, 2);
            }
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_LE_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_LEFT_ANGLE);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;


    case '>':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '>':
          if (gsk_sl_token_reader_get (&reader, 2) == '=')
            {
              gsk_sl_token_init (token, GSK_SL_TOKEN_RIGHT_ASSIGN);
              gsk_sl_token_reader_consume (&reader, 3);
            }
          else
            {
              gsk_sl_token_init (token, GSK_SL_TOKEN_RIGHT_OP);
              gsk_sl_token_reader_consume (&reader, 2);
            }
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_GE_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_RIGHT_ANGLE);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '+':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '+':
          gsk_sl_token_init (token, GSK_SL_TOKEN_INC_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_ADD_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_PLUS);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '-':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '-':
          gsk_sl_token_init (token, GSK_SL_TOKEN_DEC_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_SUB_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_DASH);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '=':
      if (gsk_sl_token_reader_get (&reader, 1) == '=')
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_EQ_OP);
          gsk_sl_token_reader_consume (&reader, 2);
        }
      else
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_EQUAL);
          gsk_sl_token_reader_consume (&reader, 1);
        }
      break;

    case '!':
      if (gsk_sl_token_reader_get (&reader, 1) == '=')
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_NE_OP);
          gsk_sl_token_reader_consume (&reader, 2);
        }
      else
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_BANG);
          gsk_sl_token_reader_consume (&reader, 1);
        }
      break;

    case '&':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '&':
          gsk_sl_token_init (token, GSK_SL_TOKEN_AND_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_AND_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_AMPERSAND);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '|':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '|':
          gsk_sl_token_init (token, GSK_SL_TOKEN_OR_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_OR_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_VERTICAL_BAR);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '^':
      switch (gsk_sl_token_reader_get (&reader, 1))
        {
        case '^':
          gsk_sl_token_init (token, GSK_SL_TOKEN_XOR_OP);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        case '=':
          gsk_sl_token_init (token, GSK_SL_TOKEN_XOR_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
          break;
        default:
          gsk_sl_token_init (token, GSK_SL_TOKEN_CARET);
          gsk_sl_token_reader_consume (&reader, 1);
          break;
        }
      break;

    case '*':
      if (gsk_sl_token_reader_get (&reader, 1) == '=')
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_MUL_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
        }
      else
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_STAR);
          gsk_sl_token_reader_consume (&reader, 1);
        }
      break;

    case '%':
      if (gsk_sl_token_reader_get (&reader, 1) == '=')
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_MOD_ASSIGN);
          gsk_sl_token_reader_consume (&reader, 2);
        }
      else
        {
          gsk_sl_token_init (token, GSK_SL_TOKEN_PERCENT);
          gsk_sl_token_reader_consume (&reader, 1);
        }
      break;

    case '(':
      gsk_sl_token_init (token, GSK_SL_TOKEN_LEFT_PAREN);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case ')':
      gsk_sl_token_init (token, GSK_SL_TOKEN_RIGHT_PAREN);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '[':
      gsk_sl_token_init (token, GSK_SL_TOKEN_LEFT_BRACKET);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case ']':
      gsk_sl_token_init (token, GSK_SL_TOKEN_RIGHT_BRACKET);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '{':
      gsk_sl_token_init (token, GSK_SL_TOKEN_LEFT_BRACE);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '}':
      gsk_sl_token_init (token, GSK_SL_TOKEN_RIGHT_BRACE);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '.':
      gsk_sl_token_init (token, GSK_SL_TOKEN_DOT);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case ':':
      gsk_sl_token_init (token, GSK_SL_TOKEN_COLON);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case ';':
      gsk_sl_token_init (token, GSK_SL_TOKEN_SEMICOLON);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '~':
      gsk_sl_token_init (token, GSK_SL_TOKEN_TILDE);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '?':
      gsk_sl_token_init (token, GSK_SL_TOKEN_QUESTION);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case '#':
      gsk_sl_token_init (token, GSK_SL_TOKEN_HASH);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    case ',':
      gsk_sl_token_init (token, GSK_SL_TOKEN_COMMA);
      gsk_sl_token_reader_consume (&reader, 1);
      break;

    default:
      if (g_ascii_isdigit (c))
        {
          gsk_sl_token_reader_read_number (&reader, token, &error);
        }
      else if (is_identifier_start (c))
        {
          gsk_sl_token_reader_read_identifier (&reader, token);
        }
      else
        {
          set_parse_error (&error, "Unknown character 0x%X", gsk_sl_token_reader_get (&reader, 0));
          gsk_sl_token_init (token, GSK_SL_TOKEN_ERROR);
          gsk_sl_token_reader_consume (&reader, 1);
        }
      break;
    }

  if (error != NULL)
    {
      GskCodeLocation error_location;

      gsk_code_location_init_copy (&error_location, &reader.position);
      gsk_sl_token_reader_init_copy (&tokenizer->reader, &reader);
      gsk_sl_tokenizer_emit_error (tokenizer, &error_location, token, error);
      g_error_free (error);
    }
  else
    {
      gsk_sl_token_reader_init_copy (&tokenizer->reader, &reader);
    }
}

