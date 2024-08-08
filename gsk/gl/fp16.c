/* fp16.c
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

void
float_to_half4_c (const float f[4],
                  guint16     h[4])
{
  h[0] = float_to_half_one (f[0]);
  h[1] = float_to_half_one (f[1]);
  h[2] = float_to_half_one (f[2]);
  h[3] = float_to_half_one (f[3]);
}

void
half_to_float4_c (const guint16 h[4],
                  float         f[4])
{
  f[0] = half_to_float_one (h[0]);
  f[1] = half_to_float_one (h[1]);
  f[2] = half_to_float_one (h[2]);
  f[3] = half_to_float_one (h[3]);
}

void
float_to_half_c (const float *f,
                 guint16     *h,
                 int          n)
{
  for (int i = 0; i < n; i++)
    h[i] = float_to_half_one (f[i]);
}

void
half_to_float_c (const guint16 *h,
                 float         *f,
                 int            n)
{
  for (int i = 0; i < n; i++)
    f[i] = half_to_float_one (h[i]);
}

#ifdef HAVE_F16C

#if defined(_MSC_VER) && !defined(__clang__)
/* based on info from https://walbourn.github.io/directxmath-f16c-and-fma/ */
static gboolean
have_f16c_msvc (void)
{
  static gboolean result = FALSE;
  static gsize inited = 0;

  if (g_once_init_enter (&inited))
    {
      int cpuinfo[4] = { -1 };

      __cpuid (cpuinfo, 0);

      if (cpuinfo[0] > 0)
        {
          __cpuid (cpuinfo, 1);

          if ((cpuinfo[2] & 0x8000000) != 0)
            result = (cpuinfo[2] & 0x20000000) != 0;
        }

      g_once_init_leave (&inited, 1);
    }

  return result;
}

void
float_to_half4 (const float f[4], guint16 h[4])
{
  if (have_f16c_msvc ())
    float_to_half4_f16c (f, h);
  else
    float_to_half4_c (f, h);
}

void
half_to_float4 (const guint16 h[4], float f[4])
{
  if (have_f16c_msvc ())
    half_to_float4_f16c (h, f);
  else
    half_to_float4_c (h, f);
}

void
float_to_half (const float *f, guint16 *h, int n)
{
  if (have_f16c_msvc ())
    float_to_half_f16c (f, h, n);
  else
    float_to_half_c (f, h, n);
}

void
half_to_float (const guint16 *h, float *f, int n)
{
  if (have_f16c_msvc ())
    half_to_float_f16c (h, f, n);
  else
    half_to_float_c (h, f, n);
}

#else

void float_to_half4 (const float f[4], guint16 h[4]) __attribute__((ifunc ("resolve_float_to_half4")));
void half_to_float4 (const guint16 h[4], float f[4]) __attribute__((ifunc ("resolve_half_to_float4")));
void float_to_half (const float *f, guint16 *h, int n) __attribute__((ifunc ("resolve_float_to_half")));
void half_to_float (const guint16 *h, float *f, int n) __attribute__((ifunc ("resolve_half_to_float")));

static void * __attribute__ ((no_sanitize_address)) G_GNUC_UNUSED
resolve_float_to_half4 (void)
{
  __builtin_cpu_init ();
  if (__builtin_cpu_supports ("f16c"))
    return float_to_half4_f16c;
  else
    return float_to_half4_c;
}

static void * __attribute__ ((no_sanitize_address)) G_GNUC_UNUSED
resolve_half_to_float4 (void)
{
  __builtin_cpu_init ();
  if (__builtin_cpu_supports ("f16c"))
    return half_to_float4_f16c;
  else
    return half_to_float4_c;
}

static void * __attribute__ ((no_sanitize_address)) G_GNUC_UNUSED
resolve_float_to_half (void)
{
  __builtin_cpu_init ();
  if (__builtin_cpu_supports ("f16c"))
    return float_to_half_f16c;
  else
    return float_to_half_c;
}

static void * __attribute__ ((no_sanitize_address)) G_GNUC_UNUSED
resolve_half_to_float (void)
{
  __builtin_cpu_init ();
  if (__builtin_cpu_supports ("f16c"))
    return half_to_float_f16c;
  else
    return half_to_float_c;
}

#endif

#else /* ! HAVE_F16C */

#if defined(__APPLE__) || (defined(_MSC_VER) && !defined(__clang__))
// turns out aliases don't work on Darwin nor Visual Studio

void
float_to_half4 (const float f[4],
                guint16     h[4])
{
  float_to_half4_c (f, h);
}

void
half_to_float4 (const guint16 h[4],
                float         f[4])
{
  half_to_float4_c (h, f);
}

void
float_to_half (const float *f,
               guint16     *h,
               int          n)
{
  float_to_half_c (f, h, n);
}

void
half_to_float (const guint16 *h,
               float         *f,
               int            n)
{
  half_to_float_c (h, f, n);
}

#else

void float_to_half4 (const float f[4], guint16 h[4]) __attribute__((alias ("float_to_half4_c")));
void half_to_float4 (const guint16 h[4], float f[4]) __attribute__((alias ("half_to_float4_c")));
void float_to_half (const float *f, guint16 *h, int n) __attribute__((alias ("float_to_half_c")));
void half_to_float (const guint16 *h, float *f, int n) __attribute__((alias ("half_to_float_c")));

#endif

#endif  /* HAVE_F16C */
