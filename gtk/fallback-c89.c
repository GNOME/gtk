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

#include "config.h"

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

#ifndef HAVE_DECL_ISINF
/* Unfortunately MSVC does not have finite()
 * but it does have _finite() which is the same
 * as finite() except when x is a NaN
 */
static inline gboolean
isinf (double x)
{
  return (!_finite (x) && !_isnan (x));
}
#endif

#ifndef INFINITY
/* define INFINITY for compilers that lack support for it */
# ifdef HUGE_VALF
#  define INFINITY HUGE_VALF
# else
#  define INFINITY (float)HUGE_VAL
# endif
#endif

#ifndef HAVE_LOG2
/* Use a simple implementation for log2() for compilers that lack it */
static inline double
log2 (double x)
{
  return log (x) / log (2.0);
}
#endif

#ifndef HAVE_EXP2
/* Use a simple implementation for exp2() for compilers that lack it */
static inline double
exp2 (double x)
{
  return pow (2.0, x);
}
#endif

#ifndef HAVE_TRUNC
static inline double
trunc (double x)
{
  return (x > 0 ? floor (x) : ceil (x));
}
#endif

#ifndef HAVE_DECL_ISNAN
/* it seems of the supported compilers only
 * MSVC does not have isnan(), but it does
 * have _isnan() which does the same as isnan()
 */
static inline gboolean
isnan (double x)
{
  return _isnan (x);
}
#endif

#ifndef HAVE_FMIN
static inline double
fmin (double x, double y)
{
  return x < y ? x : y;
}
#endif
