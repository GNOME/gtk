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

#ifndef __FP16_PRIVATE_H__
#define __FP16_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

#define FP16_ZERO ((guint16)0)
#define FP16_ONE ((guint16)15360)
#define FP16_MINUS_ONE ((guint16)48128)

void float_to_half4 (const float f[4],
                     guint16     h[4]);

void half_to_float4 (const guint16 h[4],
                     float         f[4]);

G_END_DECLS

#endif
