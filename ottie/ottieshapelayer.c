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

#include "ottieshapelayerprivate.h"

#include "ottiefillshapeprivate.h"
#include "ottiegroupshapeprivate.h"
#include "ottieparserprivate.h"
#include "ottiepathshapeprivate.h"
#include "ottieshapeprivate.h"
#include "ottiestrokeshapeprivate.h"
#include "ottietransformprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

struct _OttieShapeLayer
{
  OttieLayer parent;

  OttieShape *shapes;
};

struct _OttieShapeLayerClass
{
  OttieLayerClass parent_class;
};

G_DEFINE_TYPE (OttieShapeLayer, ottie_shape_layer, OTTIE_TYPE_LAYER)

static void
ottie_shape_layer_render (OttieLayer  *layer,
                          OttieRender *render,
                          double       timestamp)
{
  OttieShapeLayer *self = OTTIE_SHAPE_LAYER (layer);

  ottie_shape_render (self->shapes,
                      render,
                      timestamp);
}

static void
ottie_shape_layer_dispose (GObject *object)
{
  OttieShapeLayer *self = OTTIE_SHAPE_LAYER (object);

  g_clear_object (&self->shapes);

  G_OBJECT_CLASS (ottie_shape_layer_parent_class)->dispose (object);
}

static void
ottie_shape_layer_class_init (OttieShapeLayerClass *klass)
{
  OttieLayerClass *layer_class = OTTIE_LAYER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  layer_class->render = ottie_shape_layer_render;

  gobject_class->dispose = ottie_shape_layer_dispose;
}

static void
ottie_shape_layer_init (OttieShapeLayer *self)
{
  self->shapes = ottie_group_shape_new ();
}

static gboolean
ottie_shape_layer_parse_shapes (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  OttieShapeLayer *self = data;

  return ottie_group_shape_parse_shapes (reader, 0, self->shapes);
}

OttieLayer *
ottie_shape_layer_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_LAYER,
    { "shapes", ottie_shape_layer_parse_shapes, 0 },
  };
  OttieShapeLayer *self;

  self = g_object_new (OTTIE_TYPE_SHAPE_LAYER, NULL);

  if (!ottie_parser_parse_object (reader, "shape layer", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_LAYER (self);
}

OttieShape *
ottie_shape_layer_get_shape (OttieShapeLayer *self)
{
  g_return_val_if_fail (OTTIE_IS_SHAPE_LAYER (self), NULL);

  return self->shapes;
}
