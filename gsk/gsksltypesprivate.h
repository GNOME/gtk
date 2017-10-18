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

typedef struct _GskSlDeclaration        GskSlDeclaration;
typedef struct _GskSlEnvironment        GskSlEnvironment;
typedef struct _GskSlExpression         GskSlExpression;
typedef struct _GskSlFunction           GskSlFunction;
typedef struct _GskSlFunctionMatcher    GskSlFunctionMatcher;
typedef struct _GskSlFunctionType       GskSlFunctionType;
typedef struct _GskSlImageType          GskSlImageType;
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

typedef struct _GskSpvAccessChain       GskSpvAccessChain;
typedef struct _GskSpvWriter            GskSpvWriter;
typedef void (* GskSpvWriterFunc)       (GskSpvWriter *, gpointer);

typedef enum {
  GSK_SL_VOID,
  GSK_SL_FLOAT,
  GSK_SL_DOUBLE,
  GSK_SL_INT,
  GSK_SL_UINT,
  GSK_SL_BOOL
} GskSlScalarType;

typedef enum {
  GSK_SL_SAMPLER_1D,
  GSK_SL_SAMPLER_1D_INT,
  GSK_SL_SAMPLER_1D_UINT,
  GSK_SL_SAMPLER_1D_SHADOW,
  GSK_SL_SAMPLER_2D,
  GSK_SL_SAMPLER_2D_INT,
  GSK_SL_SAMPLER_2D_UINT,
  GSK_SL_SAMPLER_2D_SHADOW,
  GSK_SL_SAMPLER_3D,
  GSK_SL_SAMPLER_3D_INT,
  GSK_SL_SAMPLER_3D_UINT,
  GSK_SL_SAMPLER_CUBE,
  GSK_SL_SAMPLER_CUBE_INT,
  GSK_SL_SAMPLER_CUBE_UINT,
  GSK_SL_SAMPLER_CUBE_SHADOW,
  GSK_SL_SAMPLER_2D_RECT,
  GSK_SL_SAMPLER_2D_RECT_INT,
  GSK_SL_SAMPLER_2D_RECT_UINT,
  GSK_SL_SAMPLER_2D_RECT_SHADOW,
  GSK_SL_SAMPLER_1D_ARRAY,
  GSK_SL_SAMPLER_1D_ARRAY_INT,
  GSK_SL_SAMPLER_1D_ARRAY_UINT,
  GSK_SL_SAMPLER_1D_ARRAY_SHADOW,
  GSK_SL_SAMPLER_2D_ARRAY,
  GSK_SL_SAMPLER_2D_ARRAY_INT,
  GSK_SL_SAMPLER_2D_ARRAY_UINT,
  GSK_SL_SAMPLER_2D_ARRAY_SHADOW,
  GSK_SL_SAMPLER_CUBE_ARRAY,
  GSK_SL_SAMPLER_CUBE_ARRAY_INT,
  GSK_SL_SAMPLER_CUBE_ARRAY_UINT,
  GSK_SL_SAMPLER_CUBE_ARRAY_SHADOW,
  GSK_SL_SAMPLER_BUFFER,
  GSK_SL_SAMPLER_BUFFER_INT,
  GSK_SL_SAMPLER_BUFFER_UINT,
  GSK_SL_SAMPLER_2DMS,
  GSK_SL_SAMPLER_2DMS_INT,
  GSK_SL_SAMPLER_2DMS_UINT,
  GSK_SL_SAMPLER_2DMS_ARRAY,
  GSK_SL_SAMPLER_2DMS_ARRAY_INT,
  GSK_SL_SAMPLER_2DMS_ARRAY_UINT,
  GSK_SL_N_SAMPLER_TYPES
} GskSlSamplerType;

typedef enum {
  GSK_SL_STORAGE_DEFAULT,

  GSK_SL_STORAGE_GLOBAL,
  GSK_SL_STORAGE_GLOBAL_CONST,
  GSK_SL_STORAGE_GLOBAL_IN,
  GSK_SL_STORAGE_GLOBAL_OUT,
  GSK_SL_STORAGE_GLOBAL_UNIFORM,

  GSK_SL_STORAGE_LOCAL,
  GSK_SL_STORAGE_LOCAL_CONST,

  GSK_SL_STORAGE_PARAMETER_IN,
  GSK_SL_STORAGE_PARAMETER_OUT,
  GSK_SL_STORAGE_PARAMETER_INOUT,
  GSK_SL_STORAGE_PARAMETER_CONST
} GskSlStorage;

#endif /* __GSK_SL_TYPES_H__ */
