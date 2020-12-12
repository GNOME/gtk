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

#include "ottiestrokeshapeprivate.h"

#include "ottiecolorvalueprivate.h"
#include "ottiedoublevalueprivate.h"
#include "ottieparserprivate.h"
#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

struct _OttieStrokeShape
{
  OttieShape parent;

  OttieDoubleValue opacity;
  OttieColorValue color;
  OttieDoubleValue line_width;
  GskLineCap line_cap;
  GskLineJoin line_join;
  double miter_limit;
  GskBlendMode blend_mode;
};

struct _OttieStrokeShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieStrokeShape, ottie_stroke_shape, OTTIE_TYPE_SHAPE)

static void
ottie_stroke_shape_snapshot (OttieShape         *shape,
                             GtkSnapshot        *snapshot,
                             OttieShapeSnapshot *snapshot_data,
                             double              timestamp)
{
  OttieStrokeShape *self = OTTIE_STROKE_SHAPE (shape);
  GskPath *path;
  graphene_rect_t bounds;
  GdkRGBA color;
  GskStroke *stroke;
  double opacity, line_width;

  line_width = ottie_double_value_get (&self->line_width, timestamp);
  if (line_width <= 0)
    return;

  opacity = ottie_double_value_get (&self->opacity, timestamp);
  opacity = CLAMP (opacity, 0, 100);
  ottie_color_value_get (&self->color, timestamp, &color);
  color.alpha = color.alpha * opacity / 100.f;
  if (gdk_rgba_is_clear (&color))
    return;

  path = ottie_shape_snapshot_get_path (snapshot_data);
  stroke = gsk_stroke_new (line_width);
  gsk_stroke_set_line_cap (stroke, self->line_cap);
  gsk_stroke_set_line_join (stroke, self->line_join);
  gsk_stroke_set_miter_limit (stroke, self->miter_limit);
  gtk_snapshot_push_stroke (snapshot, path, stroke);

  gsk_path_get_stroke_bounds (path, stroke, &bounds);
  gtk_snapshot_append_color (snapshot, &color, &bounds);
  
  gsk_stroke_free (stroke);
  gtk_snapshot_pop (snapshot);
}

static void
ottie_stroke_shape_dispose (GObject *object)
{
  OttieStrokeShape *self = OTTIE_STROKE_SHAPE (object);

  ottie_double_value_clear (&self->opacity);
  ottie_color_value_clear (&self->color);
  ottie_double_value_clear (&self->line_width);

  G_OBJECT_CLASS (ottie_stroke_shape_parent_class)->dispose (object);
}

static void
ottie_stroke_shape_finalize (GObject *object)
{
  //OttieStrokeShape *self = OTTIE_STROKE_SHAPE (object);

  G_OBJECT_CLASS (ottie_stroke_shape_parent_class)->finalize (object);
}

static void
ottie_stroke_shape_class_init (OttieStrokeShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->snapshot = ottie_stroke_shape_snapshot;

  gobject_class->finalize = ottie_stroke_shape_finalize;
  gobject_class->dispose = ottie_stroke_shape_dispose;
}

static void
ottie_stroke_shape_init (OttieStrokeShape *self)
{
  ottie_double_value_init (&self->opacity, 100);
  ottie_color_value_init (&self->color, &(GdkRGBA) { 0, 0, 0, 1 });
  ottie_double_value_init (&self->line_width, 1);
}

OttieShape *
ottie_stroke_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE
    { "w", ottie_double_value_parse, G_STRUCT_OFFSET (OttieStrokeShape, line_width) },
    { "o", ottie_double_value_parse, G_STRUCT_OFFSET (OttieStrokeShape, opacity) },
    { "c", ottie_color_value_parse, G_STRUCT_OFFSET (OttieStrokeShape, color) },
    { "lc", ottie_parser_option_line_cap, G_STRUCT_OFFSET (OttieStrokeShape, line_cap) },
    { "lj", ottie_parser_option_line_join, G_STRUCT_OFFSET (OttieStrokeShape, line_join) },
    { "ml", ottie_parser_option_double, G_STRUCT_OFFSET (OttieStrokeShape, miter_limit) },
    { "bm", ottie_parser_option_blend_mode, G_STRUCT_OFFSET (OttieStrokeShape, blend_mode) },
  };
  OttieStrokeShape *self;

  self = g_object_new (OTTIE_TYPE_STROKE_SHAPE, NULL);

  if (!ottie_parser_parse_object (reader, "stroke shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_SHAPE (self);
}

