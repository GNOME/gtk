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

#include "ottiepathshapeprivate.h"

#include "ottiepathvalueprivate.h"
#include "ottieparserprivate.h"
#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>

struct _OttiePathShape
{
  OttieShape parent;

  double direction;
  OttiePathValue path;
};

struct _OttiePathShapeClass
{
  OttieShapeClass parent_class;
};

G_DEFINE_TYPE (OttiePathShape, ottie_path_shape, OTTIE_TYPE_SHAPE)

static void
ottie_path_shape_render (OttieShape  *shape,
                         OttieRender *render,
                         double       timestamp)
{
  OttiePathShape *self = OTTIE_PATH_SHAPE (shape);

  ottie_render_add_path (render,
                         ottie_path_value_get (&self->path,
                                               timestamp,
                                               self->direction));
}

static void
ottie_path_shape_dispose (GObject *object)
{
  OttiePathShape *self = OTTIE_PATH_SHAPE (object);

  ottie_path_value_clear (&self->path);

  G_OBJECT_CLASS (ottie_path_shape_parent_class)->dispose (object);
}

static void
ottie_path_shape_class_init (OttiePathShapeClass *klass)
{
  OttieShapeClass *shape_class = OTTIE_SHAPE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  shape_class->render = ottie_path_shape_render;

  gobject_class->dispose = ottie_path_shape_dispose;
}

static void
ottie_path_shape_init (OttiePathShape *self)
{
  ottie_path_value_init (&self->path);
}

OttieShape *
ottie_path_shape_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_SHAPE,
    { "d", ottie_parser_option_double, G_STRUCT_OFFSET (OttiePathShape, direction) },
    { "ks", ottie_path_value_parse, G_STRUCT_OFFSET (OttiePathShape, path) },
  };
  OttiePathShape *self;

  self = g_object_new (OTTIE_TYPE_PATH_SHAPE, NULL);

  if (!ottie_parser_parse_object (reader, "path shape", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_SHAPE (self);
}

