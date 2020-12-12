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

  double direction;
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
ottie_rect_shape_snapshot (OttieShape         *shape,
                           GtkSnapshot        *snapshot,
                           OttieShapeSnapshot *snapshot_data,
                           double              timestamp)
{
  OttieRectShape *self = OTTIE_RECT_SHAPE (shape);
  graphene_point_t p, s;
  double r;
  graphene_rect_t rect;
  GskPathBuilder *builder;

  ottie_point_value_get (&self->position, timestamp, &p);
  ottie_point_value_get (&self->size, timestamp, &s);
  r = ottie_double_value_get (&self->rounded, timestamp);

  builder = gsk_path_builder_new ();
  rect = GRAPHENE_RECT_INIT (p.x - s.x / 2,
                             p.y - s.y / 2,
                             s.x, s.y);
  if (r <= 0)
    {
      gsk_path_builder_add_rect (builder, &rect);
    }
  else
    {
      GskRoundedRect rounded;

      gsk_rounded_rect_init_from_rect (&rounded, &rect, r);
      gsk_path_builder_add_rounded_rect (builder, &rounded);
    }

  ottie_shape_snapshot_add_path (snapshot_data,
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

  shape_class->snapshot = ottie_rect_shape_snapshot;

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
    OTTIE_PARSE_OPTIONS_SHAPE
    { "d", ottie_parser_option_double, G_STRUCT_OFFSET (OttieRectShape, direction) },
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

