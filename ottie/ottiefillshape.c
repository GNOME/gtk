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

#include "ottiefillshapeprivate.h"

#include "ottiecolorvalueprivate.h"
#include "ottiedoublevalueprivate.h"
#include "ottieparserprivate.h"
#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

struct _OttieFillShape
{
  OttieShape parent;

  OttieDoubleValue opacity;
  OttieColorValue color;
  GskBlendMode blend_mode;
  GskFillRule fill_rule;
};

struct _OttieFillShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieFillShape, ottie_fill_shape, OTTIE_TYPE_SHAPE)

static void
ottie_fill_shape_render (OttieShape  *shape,
                         OttieRender *render,
                         double       timestamp)
{
  OttieFillShape *self = OTTIE_FILL_SHAPE (shape);
  GskPath *path;
  graphene_rect_t bounds;
  GdkRGBA color;
  double opacity;
  GskRenderNode *color_node;

  opacity = ottie_double_value_get (&self->opacity, timestamp);
  opacity = CLAMP (opacity, 0, 100);
  ottie_color_value_get (&self->color, timestamp, &color);
  color.alpha = color.alpha * opacity / 100.f;
  if (gdk_rgba_is_clear (&color))
    return;

  path = ottie_render_get_path (render);
  if (gsk_path_is_empty (path))
    return;

  gsk_path_get_bounds (path, &bounds);
  color_node = gsk_color_node_new (&color, &bounds);

  ottie_render_add_node (render, gsk_fill_node_new (color_node, path, self->fill_rule));

  gsk_render_node_unref (color_node);
}

static void
ottie_fill_shape_dispose (GObject *object)
{
  OttieFillShape *self = OTTIE_FILL_SHAPE (object);

  ottie_double_value_clear (&self->opacity);
  ottie_color_value_clear (&self->color);

  G_OBJECT_CLASS (ottie_fill_shape_parent_class)->dispose (object);
}

static void
ottie_fill_shape_class_init (OttieFillShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->render = ottie_fill_shape_render;

  gobject_class->dispose = ottie_fill_shape_dispose;
}

static void
ottie_fill_shape_init (OttieFillShape *self)
{
  ottie_double_value_init (&self->opacity, 100);
  ottie_color_value_init (&self->color, &(GdkRGBA) { 0, 0, 0, 1 });
  self->fill_rule = GSK_FILL_RULE_WINDING;
}

OttieShape *
ottie_fill_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE,
    { "o", ottie_double_value_parse, G_STRUCT_OFFSET (OttieFillShape, opacity) },
    { "c", ottie_color_value_parse, G_STRUCT_OFFSET (OttieFillShape, color) },
    { "bm", ottie_parser_option_blend_mode, G_STRUCT_OFFSET (OttieFillShape, blend_mode) },
    { "r", ottie_parser_option_fill_rule, G_STRUCT_OFFSET (OttieFillShape, fill_rule) },
  };
  OttieFillShape *self;

  self = g_object_new (OTTIE_TYPE_FILL_SHAPE, NULL);

  if (!ottie_parser_parse_object (reader, "fill shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_SHAPE (self);
}

