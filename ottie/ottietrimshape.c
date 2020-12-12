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

#include "ottietrimshapeprivate.h"

#include "ottiedoublevalueprivate.h"
#include "ottieparserprivate.h"
#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

typedef enum
{
  OTTIE_TRIM_TOGETHER,
  OTTIE_TRIM_SEPARATELY
} OttieTrimMode;

struct _OttieTrimShape
{
  OttieShape parent;

  OttieTrimMode mode;
  OttieDoubleValue start;
  OttieDoubleValue end;
  OttieDoubleValue offset;
};

struct _OttieTrimShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieTrimShape, ottie_trim_shape, OTTIE_TYPE_SHAPE)

static void
ottie_trim_shape_snapshot (OttieShape         *shape,
                           GtkSnapshot        *snapshot,
                           OttieShapeSnapshot *snapshot_data,
                           double              timestamp)
{
  OttieTrimShape *self = OTTIE_TRIM_SHAPE (shape);
  GskPathMeasure *measure;
  GskPath *path;
  GskPathBuilder *builder;
  double start, end, offset;

  path = ottie_shape_snapshot_get_path (snapshot_data);
  measure = gsk_path_measure_new (path);
  start = ottie_double_value_get (&self->start, timestamp);
  start = CLAMP (start, 0, 100) / 100.f;
  end = ottie_double_value_get (&self->end, timestamp);
  end = CLAMP (end, 0, 100) / 100.f;

  builder = gsk_path_builder_new ();
  if (start != end)
    {
      if (start > end)
        {
          double swap = end;
          end = start;
          start = swap;
        }
      offset = ottie_double_value_get (&self->offset, timestamp) / 360.f;
      start += offset;
      start = start - floor (start);
      start *= gsk_path_measure_get_length (measure);
      end += offset;
      end = end - floor (end);
      end *= gsk_path_measure_get_length (measure);

      gsk_path_builder_add_segment (builder, measure, start, end);
    }
  path = gsk_path_builder_free_to_path (builder);

  ottie_shape_snapshot_clear (snapshot_data);
  ottie_shape_snapshot_add_path (snapshot_data, path);

  gsk_path_measure_unref (measure);
}

static void
ottie_trim_shape_dispose (GObject *object)
{
  OttieTrimShape *self = OTTIE_TRIM_SHAPE (object);

  ottie_double_value_clear (&self->start);
  ottie_double_value_clear (&self->end);
  ottie_double_value_clear (&self->offset);

  G_OBJECT_CLASS (ottie_trim_shape_parent_class)->dispose (object);
}

static void
ottie_trim_shape_finalize (GObject *object)
{
  //OttieTrimShape *self = OTTIE_TRIM_SHAPE (object);

  G_OBJECT_CLASS (ottie_trim_shape_parent_class)->finalize (object);
}

static void
ottie_trim_shape_class_init (OttieTrimShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->snapshot = ottie_trim_shape_snapshot;

  gobject_class->finalize = ottie_trim_shape_finalize;
  gobject_class->dispose = ottie_trim_shape_dispose;
}

static void
ottie_trim_shape_init (OttieTrimShape *self)
{
  ottie_double_value_init (&self->start, 0);
  ottie_double_value_init (&self->end, 100);
  ottie_double_value_init (&self->offset, 0);
}

static gboolean
ottie_trim_mode_parse (JsonReader *reader,
                       gsize       offset,
                       gpointer    data)
{
  OttieTrimMode trim_mode;
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  switch (i)
  {
    case 1:
      trim_mode = OTTIE_TRIM_TOGETHER;
      break;

    case 2:
      ottie_parser_error_unsupported (reader, "Figure out separate trim mode");
      trim_mode = OTTIE_TRIM_SEPARATELY;
      break;

    default:
      ottie_parser_error_value (reader, "%"G_GINT64_FORMAT" is not a known trim mode", i);
      return FALSE;
  }

  *(OttieTrimMode *) ((guint8 *) data + offset) = trim_mode;

  return TRUE;
}

OttieShape *
ottie_trim_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE
    { "s", ottie_double_value_parse, G_STRUCT_OFFSET (OttieTrimShape, start) },
    { "e", ottie_double_value_parse, G_STRUCT_OFFSET (OttieTrimShape, end) },
    { "o", ottie_double_value_parse, G_STRUCT_OFFSET (OttieTrimShape, offset) },
    { "m", ottie_trim_mode_parse, G_STRUCT_OFFSET (OttieTrimShape, mode) },
  };
  OttieTrimShape *self;

  self = g_object_new (OTTIE_TYPE_TRIM_SHAPE, NULL);

  if (!ottie_parser_parse_object (reader, "trim shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_SHAPE (self);
}

