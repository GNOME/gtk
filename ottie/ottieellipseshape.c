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

#include "ottieellipseshapeprivate.h"

#include "ottiedoublevalueprivate.h"
#include "ottiepointvalueprivate.h"
#include "ottieparserprivate.h"
#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>

struct _OttieEllipseShape
{
  OttieShape parent;

  double diellipseion;
  OttiePointValue position;
  OttiePointValue size;
};

struct _OttieEllipseShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieEllipseShape, ottie_ellipse_shape, OTTIE_TYPE_SHAPE)

static void
ottie_ellipse_shape_render (OttieShape  *shape,
                         OttieRender *render,
                         double       timestamp)
{
  OttieEllipseShape *self = OTTIE_ELLIPSE_SHAPE (shape);
  graphene_point_t p, s;
  GskPathBuilder *builder;
  const float weight = sqrt(0.5f);

  ottie_point_value_get (&self->position, timestamp, &p);
  ottie_point_value_get (&self->size, timestamp, &s);
  s.x /= 2;
  s.y /= 2;

  builder = gsk_path_builder_new ();

  gsk_path_builder_move_to (builder,
                            p.x, p.y - s.y);
  gsk_path_builder_conic_to (builder,
                             p.x + s.x, p.y - s.y,
                             p.x + s.x, p.y,
                             weight);
  gsk_path_builder_conic_to (builder,
                             p.x + s.x, p.y + s.y,
                             p.x, p.y + s.y,
                             weight);
  gsk_path_builder_conic_to (builder,
                             p.x - s.x, p.y + s.y,
                             p.x - s.x, p.y,
                             weight);
  gsk_path_builder_conic_to (builder,
                             p.x - s.x, p.y - s.y,
                             p.x, p.y - s.y,
                             weight);
  gsk_path_builder_close (builder);

  ottie_render_add_path (render,
                         gsk_path_builder_free_to_path (builder));
}

static void
ottie_ellipse_shape_dispose (GObject *object)
{
  OttieEllipseShape *self = OTTIE_ELLIPSE_SHAPE (object);

  ottie_point_value_clear (&self->position);
  ottie_point_value_clear (&self->size);

  G_OBJECT_CLASS (ottie_ellipse_shape_parent_class)->dispose (object);
}

static void
ottie_ellipse_shape_class_init (OttieEllipseShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->render = ottie_ellipse_shape_render;

  gobject_class->dispose = ottie_ellipse_shape_dispose;
}

static void
ottie_ellipse_shape_init (OttieEllipseShape *self)
{
  ottie_point_value_init (&self->position, &GRAPHENE_POINT_INIT (0, 0));
  ottie_point_value_init (&self->size, &GRAPHENE_POINT_INIT (0, 0));
}

OttieShape *
ottie_ellipse_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE,
    { "d", ottie_parser_option_double, G_STRUCT_OFFSET (OttieEllipseShape, diellipseion) },
    { "p", ottie_point_value_parse, G_STRUCT_OFFSET (OttieEllipseShape, position) },
    { "s", ottie_point_value_parse, G_STRUCT_OFFSET (OttieEllipseShape, size) },
  };
  OttieEllipseShape *self;

  self = g_object_new (OTTIE_TYPE_ELLIPSE_SHAPE, NULL);

  if (!ottie_parser_parse_object (reader, "ellipse shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_SHAPE (self);
}

