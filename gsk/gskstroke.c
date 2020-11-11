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

#include "config.h"

#include "gskstrokeprivate.h"

/**
 * SECTION:gskstroke
 * @Title: Stroke
 * @Short_description: Properties of a stroke operation
 * @See_also: #GskPath, gsk_stroke_node_new()
 *
 * This section describes the #GskStroke structure that is used to
 * describe lines and curves that are more complex than simple rectangles.
 *
 * #GskStroke is an immutable struct. After creation, you cannot change
 * the types it represents. Instead, new #GskStroke have to be created.
 * The #GskStrokeBuilder structure is meant to help in this endeavor.
 */

/**
 * GskStroke:
 *
 * A #GskStroke struct is an opaque struct that should be copied
 * on use.
 */

G_DEFINE_BOXED_TYPE (GskStroke, gsk_stroke,
                     gsk_stroke_copy,
                     gsk_stroke_free)


GskStroke *
gsk_stroke_new (float line_width)
{
  GskStroke *self;

  g_return_val_if_fail (line_width > 0, NULL);

  self = g_new0 (GskStroke, 1);

  self->line_width = line_width;

  return self;
}

GskStroke *
gsk_stroke_copy (const GskStroke *other)
{
  GskStroke *self;

  g_return_val_if_fail (other != NULL, NULL);

  self = g_new (GskStroke, 1);

  gsk_stroke_init_copy (self, other);

  return self;
}

void
gsk_stroke_free (GskStroke *self)
{
  if (self == NULL)
    return;

  gsk_stroke_clear (self);

  g_free (self);
}

gboolean
gsk_stroke_equal (gconstpointer stroke1,
                  gconstpointer stroke2)
{
  const GskStroke *self1 = stroke1;
  const GskStroke *self2 = stroke2;

  return self1->line_width == self2->line_width;
}

void
gsk_stroke_set_line_width (GskStroke *self,
                           float      line_width)
{
  self->line_width = line_width;
}

float
gsk_stroke_get_line_width (const GskStroke *self)
{
  return self->line_width;
}

