/* ninesliceprivate.h
 *
 * Copyright 2021 Red Hat, Inc.
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
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __FP16_PRIVATE_H__
#define __FP16_PRIVATE_H__

#include <glib.h>
#include <graphene.h>

#ifdef GRAPHENE_USE_SSE
#include <immintrin.h>
#endif

G_BEGIN_DECLS

#define FP16_ZERO ((guint16)0)
#define FP16_ONE ((guint16)15360)
#define FP16_MINUS_ONE ((guint16)48128)

#ifdef GRAPHENE_USE_SSE

static inline void
float_to_half4 (const float f[4],
                guint16     h[4])
{
  __m128 s = _mm_loadu_ps (f);
  __m128i i = _mm_cvtps_ph (s, 0);
  _mm_storel_epi64 ((__m128i*)h, i);
}

static inline void
half_to_float4 (const guint16 h[4],
                float         f[4])
{
  __m128i i = _mm_loadl_epi64 ((__m128i_u const *)h);
  __m128 s = _mm_cvtph_ps (i);
  _mm_store_ps (f, s);
}

#else  /* GRAPHENE_USE_SSE */

static inline guint
as_uint (const float x)
{
  return *(guint*)&x;
}

static inline float
as_float (const guint x)
{
  return *(float*)&x;
}

// IEEE-754 16-bit floating-point format (without infinity): 1-5-10

static inline float
half_to_float (const guint16 x)
{
  const guint e = (x&0x7C00)>>10; // exponent
  const guint m = (x&0x03FF)<<13; // mantissa
  const guint v = as_uint((float)m)>>23;
  return as_float((x&0x8000)<<16 | (e!=0)*((e+112)<<23|m) | ((e==0)&(m!=0))*((v-37)<<23|((m<<(150-v))&0x007FE000)));
}

static inline guint16
float_to_half (const float x)
{
  const guint b = as_uint(x)+0x00001000; // round-to-nearest-even
  const guint e = (b&0x7F800000)>>23; // exponent
  const guint m = b&0x007FFFFF; // mantissa
  return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
}

static inline void
float_to_half4 (const float f[4],
                guint16     h[4])
{
  h[0] = float_to_half (f[0]);
  h[1] = float_to_half (f[1]);
  h[2] = float_to_half (f[2]);
  h[3] = float_to_half (f[3]);
}

static inline void
half_to_float4 (const guint16 h[4],
                float         f[4])
{
  f[0] = half_to_float (h[0]);
  f[1] = half_to_float (h[1]);
  f[2] = half_to_float (h[2]);
  f[3] = half_to_float (h[3]);
}

#endif  /* GRAPHENE_USE_SSE */

G_END_DECLS

#endif
