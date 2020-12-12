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

#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (OttieShape, ottie_shape, OTTIE_TYPE_OBJECT)

static void
ottie_shape_dispose (GObject *object)
{
  //OttieShape *self = OTTIE_SHAPE (object);

  G_OBJECT_CLASS (ottie_shape_parent_class)->dispose (object);
}

static void
ottie_shape_class_init (OttieShapeClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = ottie_shape_dispose;
}

static void
ottie_shape_init (OttieShape *self)
{
}

void
ottie_shape_render (OttieShape  *self,
                    OttieRender *render,
                    double       timestamp)
{
  OTTIE_SHAPE_GET_CLASS (self)->render (self, render, timestamp);
}

