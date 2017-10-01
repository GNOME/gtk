/* GSK - The GTK Scene Kit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#ifndef __GSK_SL_TYPES_H__
#define __GSK_SL_TYPES_H__

#include <gsk/gsktypes.h>

typedef struct _GskSlExpression         GskSlExpression;
typedef struct _GskSlFunction           GskSlFunction;
typedef struct _GskSlFunctionMatcher    GskSlFunctionMatcher;
typedef struct _GskSlNativeFunction     GskSlNativeFunction;
typedef struct _GskSlPreprocessor       GskSlPreprocessor;
typedef struct _GskSlPointerType        GskSlPointerType;
typedef struct _GskSlPrinter            GskSlPrinter;
typedef struct _GskSlQualifier          GskSlQualifier;
typedef struct _GskSlScope              GskSlScope;
typedef struct _GskSlStatement          GskSlStatement;
typedef struct _GskSlToken              GskSlToken;
typedef struct _GskSlType               GskSlType;
typedef struct _GskSlValue              GskSlValue;
typedef struct _GskSlVariable           GskSlVariable;

typedef struct _GskSpvWriter            GskSpvWriter;

typedef enum {
  GSK_SL_VOID,
  GSK_SL_FLOAT,
  GSK_SL_DOUBLE,
  GSK_SL_INT,
  GSK_SL_UINT,
  GSK_SL_BOOL
} GskSlScalarType;

typedef enum {
  GSK_SL_BUILTIN_VOID,
  GSK_SL_BUILTIN_FLOAT,
  GSK_SL_BUILTIN_DOUBLE,
  GSK_SL_BUILTIN_INT,
  GSK_SL_BUILTIN_UINT,
  GSK_SL_BUILTIN_BOOL,
  GSK_SL_BUILTIN_BVEC2,
  GSK_SL_BUILTIN_BVEC3,
  GSK_SL_BUILTIN_BVEC4,
  GSK_SL_BUILTIN_IVEC2,
  GSK_SL_BUILTIN_IVEC3,
  GSK_SL_BUILTIN_IVEC4,
  GSK_SL_BUILTIN_UVEC2,
  GSK_SL_BUILTIN_UVEC3,
  GSK_SL_BUILTIN_UVEC4,
  GSK_SL_BUILTIN_VEC2,
  GSK_SL_BUILTIN_VEC3,
  GSK_SL_BUILTIN_VEC4,
  GSK_SL_BUILTIN_DVEC2,
  GSK_SL_BUILTIN_DVEC3,
  GSK_SL_BUILTIN_DVEC4,
  GSK_SL_BUILTIN_MAT2,
  GSK_SL_BUILTIN_MAT2X3,
  GSK_SL_BUILTIN_MAT2X4,
  GSK_SL_BUILTIN_MAT3X2,
  GSK_SL_BUILTIN_MAT3,
  GSK_SL_BUILTIN_MAT3X4,
  GSK_SL_BUILTIN_MAT4X2,
  GSK_SL_BUILTIN_MAT4X3,
  GSK_SL_BUILTIN_MAT4,
  GSK_SL_BUILTIN_DMAT2,
  GSK_SL_BUILTIN_DMAT2X3,
  GSK_SL_BUILTIN_DMAT2X4,
  GSK_SL_BUILTIN_DMAT3X2,
  GSK_SL_BUILTIN_DMAT3,
  GSK_SL_BUILTIN_DMAT3X4,
  GSK_SL_BUILTIN_DMAT4X2,
  GSK_SL_BUILTIN_DMAT4X3,
  GSK_SL_BUILTIN_DMAT4
} GskSlBuiltinType;

#endif /* __GSK_SL_TYPES_H__ */
