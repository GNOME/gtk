/* fp16private.h
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define FP16_ZERO ((guint16)0)
#define FP16_ONE ((guint16)15360)
#define FP16_MINUS_ONE ((guint16)48128)

static inline guint16
float_to_half_one (const float x)
{
  const guint b = *(guint*)&x+0x00001000; // round-to-nearest-even
  const guint e = (b&0x7F800000)>>23; // exponent
  const guint m = b&0x007FFFFF; // mantissa
  guint n0 = 0;
  guint n1 = 0;
  guint n2 = 0;

  if (e > 112)
    n0 = (((e - 112) << 10) & 0x7C00) | m >> 13;
  if (e < 113 && e > 101)
    n1 = (((0x007FF000 + m) >> (125- e)) + 1) >> 1;
  if (e > 143)
    n2 = 0x7FFF;

  return (b & 0x80000000) >> 16 | n0 | n1 | n2; // sign : normalized : denormalized : saturate
}

void float_to_half4 (const float f[4],
                     guint16     h[4]);

void half_to_float4 (const guint16 h[4],
                     float         f[4]);

void float_to_half (const float *f,
                    guint16     *h,
                    int          n);

void half_to_float (const guint16 *h,
                    float         *f,
                    int            n);

void float_to_half4_f16c (const float f[4],
                          guint16     h[4]);

void half_to_float4_f16c (const guint16 h[4],
                          float         f[4]);

void float_to_half_f16c (const float *f,
                         guint16     *h,
                         int          n);

void half_to_float_f16c (const guint16 *h,
                         float         *f,
                         int            n);

void float_to_half4_c (const float f[4],
                       guint16     h[4]);

void half_to_float4_c (const guint16 h[4],
                       float         f[4]);

void float_to_half_c (const float *f,
                      guint16     *h,
                      int          n);

void half_to_float_c (const guint16 *h,
                      float         *f,
                      int            n);

G_END_DECLS

