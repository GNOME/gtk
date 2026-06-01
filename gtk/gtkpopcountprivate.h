/* gtkpopcountprivate.h: Private implementation of popcount
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if (defined(_MSC_VER) && !_M_ARM && !_M_ARM64) && (__AVX__ || __SSE4_2__ || __POPCNT__)
#include <intrin.h>

static inline unsigned int
gtk_popcount (guint32 value)
{
  return __popcnt (value);
}

static inline unsigned int
gtk_popcount64 (uint64_t value)
{
  return __popcnt64 (value);
}

#elif defined(__GNUC__) || defined(__clang__)

#define gtk_popcount(v) __builtin_popcount (v)
#define gtk_popcount64(v) __builtin_popcountl (v)

#else

static inline unsigned int
gtk_popcount (guint32 value)
{
  /* http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
  return (((value & 0xfff) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f) +
         ((((value & 0xfff000) >> 12) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f) +
         (((value >> 24) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f);
}

static inline unsigned int
gtk_popcount64 (uint64_t value)
{
  unsigned int count = 0;
  for (unsigned int i = 0; i < 64; i++)
    {
      if ((value & (1ull << i)) != 0)
        count++;
    }
  return count;
}

#endif

#if defined(__GNUC__) || defined(__clang__)

#define gtk_ctz(v) __builtin_ctz(v)
#define gtk_ctz64(v) __builtin_ctzl(v)

#else

static inline int
gtk_ctz (guint32 value)
{
  unsigned int count = 0;

  for (unsigned int i = 0; i < 32; i++)
    {
      if ((value & (1 << i)) != 0)
        break;
      count++;
    }

  return count;
}

static inline int
gtk_ctz64 (uint64_t value)
{
  unsigned int count = 0;

  for (unsigned int i = 0; i < 64; i++)
    {
      if ((value & (1ull << i)) != 0)
        break;
      count++;
    }

  return count;
}

#endif
