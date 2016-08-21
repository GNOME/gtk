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

#include <float.h>

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
