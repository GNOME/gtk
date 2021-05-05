/* fp16i.c
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

#include "config.h"

#include "fp16private.h"

#ifdef HAVE_F16C
#include <immintrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define CAST_M128I_P(a) (__m128i const *) a
#else
#define CAST_M128I_P(a) (__m128i_u const *) a
#endif

void
float_to_half4_f16c (const float f[4],
                     guint16     h[4])
{
  __m128 s = _mm_loadu_ps (f);
  __m128i i = _mm_cvtps_ph (s, 0);
  _mm_storel_epi64 ((__m128i*)h, i);
}

void
half_to_float4_f16c (const guint16 h[4],
                     float         f[4])
{
  __m128i i = _mm_loadl_epi64 (CAST_M128I_P (h));
  __m128 s = _mm_cvtph_ps (i);

  _mm_store_ps (f, s);
}

#endif  /* HAVE_F16C */
