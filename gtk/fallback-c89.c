/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
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

#include <math.h>
 
/* Workaround for round() for non-GCC/non-C99 compilers */
#ifndef HAVE_ROUND
static inline double
round (double x)
{
  if (x >= 0)
    return floor (x + 0.5);
  else
    return ceil (x - 0.5);
}
#endif

/* Workaround for rint() for non-GCC/non-C99 compilers */
#ifndef HAVE_RINT
static inline double
rint (double x)
{
  if (ceil (x + 0.5) == floor (x + 0.5))
  {
    int a;
    a = (int) ceil (x);
    if (a % 2 == 0)
      return ceil (x);
    else
      return floor (x);
  }
  else
  {
    if (x >= 0)
      return floor (x + 0.5);
    else
      return ceil (x - 0.5);
  }
}
#endif

#ifndef HAVE_NEARBYINT
/* Workaround for nearbyint() for non-GCC/non-C99 compilers */
/* This is quite similar to rint() in most respects */

static inline double
nearbyint (double x)
{
  return floor (x + 0.5);
}
#endif
