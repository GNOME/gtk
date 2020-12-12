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
};

struct _OttieFillShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieFillShape, ottie_fill_shape, OTTIE_TYPE_SHAPE)

static void
ottie_fill_shape_snapshot (OttieShape         *shape,
                           GtkSnapshot        *snapshot,
                           OttieShapeSnapshot *snapshot_data,
                           double              timestamp)
{
  OttieFillShape *self = OTTIE_FILL_SHAPE (shape);
  GskPath *path;
  graphene_rect_t bounds;
  GdkRGBA color;
  double opacity;

  opacity = ottie_double_value_get (&self->opacity, timestamp);
  opacity = CLAMP (opacity, 0, 100);
  ottie_color_value_get (&self->color, timestamp, &color);
  color.alpha = color.alpha * opacity / 100.f;
  if (gdk_rgba_is_clear (&color))
    return;

  path = ottie_shape_snapshot_get_path (snapshot_data);
  gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);

  gsk_path_get_bounds (path, &bounds);
  gtk_snapshot_append_color (snapshot, &color, &bounds);
  
  gtk_snapshot_pop (snapshot);
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
ottie_fill_shape_finalize (GObject *object)
{
  //OttieFillShape *self = OTTIE_FILL_SHAPE (object);

  G_OBJECT_CLASS (ottie_fill_shape_parent_class)->finalize (object);
}

static void
ottie_fill_shape_class_init (OttieFillShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->snapshot = ottie_fill_shape_snapshot;

  gobject_class->finalize = ottie_fill_shape_finalize;
  gobject_class->dispose = ottie_fill_shape_dispose;
}

static void
ottie_fill_shape_init (OttieFillShape *self)
{
  ottie_double_value_init (&self->opacity, 100);
  ottie_color_value_init (&self->color, &(GdkRGBA) { 0, 0, 0, 1 });
}

OttieShape *
ottie_fill_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE
    { "o", ottie_double_value_parse, G_STRUCT_OFFSET (OttieFillShape, opacity) },
    { "c", ottie_color_value_parse, G_STRUCT_OFFSET (OttieFillShape, color) },
    { "bm", ottie_parser_option_blend_mode, G_STRUCT_OFFSET (OttieFillShape, blend_mode) },
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

