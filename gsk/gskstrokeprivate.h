/*
 * Copyright © 2020 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#ifndef __GSK_STROKE_PRIVATE_H__
#define __GSK_STROKE_PRIVATE_H__

#include "gskstroke.h"

G_BEGIN_DECLS

struct _GskStroke
{
  float line_width;
  GskLineCap line_cap;
  GskLineJoin line_join;
};

static inline void
gsk_stroke_init_copy (GskStroke       *stroke,
                      const GskStroke *other)
{
  *stroke = *other;
}

static inline void
gsk_stroke_clear (GskStroke *stroke)
{
}

void                    gsk_stroke_to_cairo                     (const GskStroke        *self,
                                                                 cairo_t                *cr);

G_END_DECLS

#endif /* __GSK_STROKE_PRIVATE_H__ */

