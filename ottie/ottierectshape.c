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

#include "ottierectshapeprivate.h"

#include "ottiedoublevalueprivate.h"
#include "ottiepointvalueprivate.h"
#include "ottieparserprivate.h"
#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>

struct _OttieRectShape
{
  OttieShape parent;

  OttieDirection direction;
  OttiePointValue position;
  OttiePointValue size;
  OttieDoubleValue rounded;
};

struct _OttieRectShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieRectShape, ottie_rect_shape, OTTIE_TYPE_SHAPE)

static void
ottie_rect_shape_render (OttieShape  *shape,
                         OttieRender *render,
                         double       timestamp)
{
  OttieRectShape *self = OTTIE_RECT_SHAPE (shape);
  graphene_point_t p, s;
  double r;
  GskPathBuilder *builder;

  ottie_point_value_get (&self->position, timestamp, &p);
  ottie_point_value_get (&self->size, timestamp, &s);
  r = ottie_double_value_get (&self->rounded, timestamp);
  s.x /= 2;
  s.y /= 2;
  r = MIN (r, MIN (s.x, s.y));

  builder = gsk_path_builder_new ();

  switch (self->direction)
  {
    case OTTIE_DIRECTION_FORWARD:
      if (r <= 0)
        {
          gsk_path_builder_move_to (builder, p.x + s.x, p.y - s.y);
          gsk_path_builder_line_to (builder, p.x - s.x, p.y - s.y);
          gsk_path_builder_line_to (builder, p.x - s.x, p.y + s.y);
          gsk_path_builder_line_to (builder, p.x + s.x, p.y + s.y);
          gsk_path_builder_line_to (builder, p.x + s.x, p.y - s.y);
          gsk_path_builder_close (builder);
        }
      else
        {
          const float weight = sqrt(0.5f);

          gsk_path_builder_move_to (builder,
                                    p.x + s.x, p.y - s.y + r);
          gsk_path_builder_conic_to (builder,
                                     p.x + s.x, p.y - s.y,
                                     p.x + s.x - r, p.y - s.y,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x - s.x + r, p.y - s.y);
          gsk_path_builder_conic_to (builder,
                                     p.x - s.x, p.y - s.y,
                                     p.x - s.x, p.y - s.y + r,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x - s.x, p.y + s.y - r);
          gsk_path_builder_conic_to (builder,
                                     p.x - s.x, p.y + s.y,
                                     p.x - s.x + r, p.y + s.y,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x + s.x - r, p.y + s.y);
          gsk_path_builder_conic_to (builder, 
                                     p.x + s.x, p.y + s.y,
                                     p.x + s.x, p.y + s.y - r,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x + s.x, p.y - s.y + r);
          gsk_path_builder_close (builder);
        }
      break;

    case OTTIE_DIRECTION_BACKWARD:
      if (r <= 0)
        {
          gsk_path_builder_move_to (builder, p.x + s.x, p.y - s.y);
          gsk_path_builder_line_to (builder, p.x + s.x, p.y + s.y);
          gsk_path_builder_line_to (builder, p.x - s.x, p.y + s.y);
          gsk_path_builder_line_to (builder, p.x - s.x, p.y - s.y);
          gsk_path_builder_line_to (builder, p.x + s.x, p.y - s.y);
          gsk_path_builder_close (builder);
        }
      else
        {
          const float weight = sqrt(0.5f);

          gsk_path_builder_move_to (builder,
                                    p.x + s.x, p.y - s.y + r);
          gsk_path_builder_line_to (builder,
                                    p.x + s.x, p.y + s.y - r);
          gsk_path_builder_conic_to (builder, 
                                     p.x + s.x, p.y + s.y,
                                     p.x + s.x - r, p.y + s.y,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x - s.x + r, p.y + s.y);
          gsk_path_builder_conic_to (builder,
                                     p.x - s.x, p.y + s.y,
                                     p.x - s.x, p.y + s.y - r,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x - s.x, p.y - s.y + r);
          gsk_path_builder_conic_to (builder,
                                     p.x - s.x, p.y - s.y,
                                     p.x - s.x + r, p.y - s.y,
                                     weight);
          gsk_path_builder_line_to (builder,
                                    p.x + s.x - r, p.y - s.y);
          gsk_path_builder_conic_to (builder,
                                     p.x + s.x, p.y - s.y,
                                     p.x + s.x, p.y - s.y + r,
                                     weight);
          gsk_path_builder_close (builder);
        }
      break;

    default:
      g_assert_not_reached();
      break;
  }

  ottie_render_add_path (render,
                         gsk_path_builder_free_to_path (builder));
}

static void
ottie_rect_shape_dispose (GObject *object)
{
  OttieRectShape *self = OTTIE_RECT_SHAPE (object);

  ottie_point_value_clear (&self->position);
  ottie_point_value_clear (&self->size);
  ottie_double_value_clear (&self->rounded);

  G_OBJECT_CLASS (ottie_rect_shape_parent_class)->dispose (object);
}

static void
ottie_rect_shape_class_init (OttieRectShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->render = ottie_rect_shape_render;

  gobject_class->dispose = ottie_rect_shape_dispose;
}

static void
ottie_rect_shape_init (OttieRectShape *self)
{
  ottie_point_value_init (&self->position, &GRAPHENE_POINT_INIT (0, 0));
  ottie_point_value_init (&self->size, &GRAPHENE_POINT_INIT (0, 0));
  ottie_double_value_init (&self->rounded, 0);
}

OttieShape *
ottie_rect_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE,
    { "d", ottie_parser_option_direction, G_STRUCT_OFFSET (OttieRectShape, direction) },
    { "p", ottie_point_value_parse, G_STRUCT_OFFSET (OttieRectShape, position) },
    { "s", ottie_point_value_parse, G_STRUCT_OFFSET (OttieRectShape, size) },
    { "r", ottie_double_value_parse, G_STRUCT_OFFSET (OttieRectShape, rounded) },
  };
  OttieRectShape *self;

  self = g_object_new (OTTIE_TYPE_RECT_SHAPE, NULL);

  if (!ottie_parser_parse_object (reader, "rect shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_SHAPE (self);
}

