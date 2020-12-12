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

#include "ottiegroupshapeprivate.h"

#include "ottiefillshapeprivate.h"
#include "ottieparserprivate.h"
#include "ottiepathshapeprivate.h"
#include "ottieshapeprivate.h"
#include "ottiestrokeshapeprivate.h"
#include "ottietransformprivate.h"
#include "ottietrimshapeprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

#define GDK_ARRAY_ELEMENT_TYPE OttieShape *
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_TYPE_NAME OttieShapeList
#define GDK_ARRAY_NAME ottie_shape_list
#define GDK_ARRAY_PREALLOC 4
#include "gdk/gdkarrayimpl.c"

struct _OttieGroupShape
{
  OttieShape parent;

  OttieShapeList shapes;
  GskBlendMode blend_mode;
};

struct _OttieGroupShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttieGroupShape, ottie_group_shape, OTTIE_TYPE_SHAPE)

static void
ottie_group_shape_snapshot (OttieShape         *shape,
                            GtkSnapshot        *snapshot,
                            OttieShapeSnapshot *snapshot_data,
                            double              timestamp)
{
  OttieGroupShape *self = OTTIE_GROUP_SHAPE (shape);
  OttieShapeSnapshot group_snapshot;

  gtk_snapshot_save (snapshot);

  ottie_shape_snapshot_init (&group_snapshot, snapshot_data);

  for (gsize i = 0; i < ottie_shape_list_get_size (&self->shapes); i++)
    {
      OttieShape *tr_shape = ottie_shape_list_get (&self->shapes, i);

      if (OTTIE_IS_TRANSFORM (tr_shape))
        {
          GskTransform *transform = ottie_transform_get_transform (OTTIE_TRANSFORM (tr_shape), timestamp);
          gtk_snapshot_transform (snapshot, transform);
          gsk_transform_unref (transform);
          break;
        }
    }

  for (gsize i = 0; i < ottie_shape_list_get_size (&self->shapes); i++)
    {
      ottie_shape_snapshot (ottie_shape_list_get (&self->shapes, i),
                            snapshot,
                            &group_snapshot,
                            timestamp);
    }

  ottie_shape_snapshot_clear (&group_snapshot);

  gtk_snapshot_restore (snapshot);
}

static void
ottie_group_shape_dispose (GObject *object)
{
  OttieGroupShape *self = OTTIE_GROUP_SHAPE (object);

  ottie_shape_list_clear (&self->shapes);

  G_OBJECT_CLASS (ottie_group_shape_parent_class)->dispose (object);
}

static void
ottie_group_shape_finalize (GObject *object)
{
  //OttieGroupShape *self = OTTIE_GROUP_SHAPE (object);

  G_OBJECT_CLASS (ottie_group_shape_parent_class)->finalize (object);
}

static void
ottie_group_shape_class_init (OttieGroupShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->snapshot = ottie_group_shape_snapshot;

  gobject_class->finalize = ottie_group_shape_finalize;
  gobject_class->dispose = ottie_group_shape_dispose;
}

static void
ottie_group_shape_init (OttieGroupShape *self)
{
}

gboolean
ottie_group_shape_parse_shapes (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  OttieGroupShape *self = data;

  if (!json_reader_is_array (reader))
    {
      ottie_parser_error_syntax (reader, "Shapes are not an array.");
      return FALSE;
    }

  for (int i = 0; ; i++)
    {
      OttieShape *shape;
      const char *type;

      if (!json_reader_read_element (reader, i))
        break;

      if (!json_reader_is_object (reader))
        {
          ottie_parser_error_syntax (reader, "Shape %d is not an object", i);
          continue;
        }

      if (!json_reader_read_member (reader, "ty"))
        {
          ottie_parser_error_syntax (reader, "Shape %d has no type", i);
          json_reader_end_member (reader);
          json_reader_end_element (reader);
          continue;
        }

      type = json_reader_get_string_value (reader);
      if (type == NULL || json_reader_get_error (reader))
        {
          ottie_parser_emit_error (reader, json_reader_get_error (reader));
          json_reader_end_member (reader);
          json_reader_end_element (reader);
          continue;
        }
      json_reader_end_member (reader);

      if (g_str_equal (type, "fl"))
        shape = ottie_fill_shape_parse (reader);
      else if (g_str_equal (type, "gr"))
        shape = ottie_group_shape_parse (reader);
      else if (g_str_equal (type, "sh"))
        shape = ottie_path_shape_parse (reader);
      else if (g_str_equal (type, "st"))
        shape = ottie_stroke_shape_parse (reader);
      else if (g_str_equal (type, "tm"))
        shape = ottie_trim_shape_parse (reader);
      else if (g_str_equal (type, "tr"))
        shape = ottie_transform_parse (reader);
      else
        {
          ottie_parser_error_value (reader, "Shape %d has unknown type \"%s\"", i, type);
          shape = NULL;
        }

      if (shape)
        ottie_shape_list_append (&self->shapes, shape);
      json_reader_end_element (reader);
    }

  json_reader_end_element (reader);

  return TRUE;
}

OttieShape *
ottie_group_shape_new (void)
{
  return g_object_new (OTTIE_TYPE_GROUP_SHAPE, NULL);
}

OttieShape *
ottie_group_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE
    { "bm", ottie_parser_option_blend_mode, G_STRUCT_OFFSET (OttieGroupShape, blend_mode) },
    { "np", ottie_parser_option_skip_expression, 0 },
    { "cix", ottie_parser_option_skip_index, 0 },
    { "it", ottie_group_shape_parse_shapes, 0 },
  };
  OttieShape *self;

  self = ottie_group_shape_new ();

  if (!ottie_parser_parse_object (reader, "group shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return self;
}

