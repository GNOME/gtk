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

#include "ottiecompositionlayerprivate.h"

#include "ottiedoublevalueprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

struct _OttieCompositionLayer
{
  OttieLayer parent;

  OttieDoubleValue time_map;
  double width;
  double height;
  char *ref_id;
  OttieComposition *composition;
};

struct _OttieCompositionLayerClass
{
  OttieLayerClass parent_class;
};

G_DEFINE_TYPE (OttieCompositionLayer, ottie_composition_layer, OTTIE_TYPE_LAYER)

static void
ottie_composition_layer_update (OttieLayer *layer,
                                GHashTable *compositions)
{
  OttieCompositionLayer *self = OTTIE_COMPOSITION_LAYER (layer);

  g_clear_object (&self->composition);

  if (self->ref_id)
    self->composition = g_object_ref (g_hash_table_lookup (compositions, self->ref_id));
}

static void
ottie_composition_layer_render (OttieLayer  *layer,
                                OttieRender *render,
                                double       timestamp)
{
  OttieCompositionLayer *self = OTTIE_COMPOSITION_LAYER (layer);
  GskRenderNode *node;
  double time_map;

  if (self->composition == NULL)
    return;

  if (ottie_double_value_is_static (&self->time_map))
    time_map = timestamp;
  else
    time_map = ottie_double_value_get (&self->time_map, timestamp);

  ottie_layer_render (OTTIE_LAYER (self->composition),
                      render,
                      time_map);

  node = ottie_render_get_node (render);
  ottie_render_clear_nodes (render);
  if (node)
    {
      ottie_render_add_node (render,
                             gsk_clip_node_new (node,
                                                &GRAPHENE_RECT_INIT (
                                                  0, 0,
                                                  self->width, self->height
                                                )));
      gsk_render_node_unref (node);
    }
}

static void
ottie_composition_layer_dispose (GObject *object)
{
  OttieCompositionLayer *self = OTTIE_COMPOSITION_LAYER (object);

  g_clear_object (&self->composition);
  g_clear_pointer (&self->ref_id, g_free);
  ottie_double_value_clear (&self->time_map);

  G_OBJECT_CLASS (ottie_composition_layer_parent_class)->dispose (object);
}

static void
ottie_composition_layer_class_init (OttieCompositionLayerClass *klass)
{
  OttieLayerClass *layer_class = OTTIE_LAYER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  layer_class->update = ottie_composition_layer_update;
  layer_class->render = ottie_composition_layer_render;

  gobject_class->dispose = ottie_composition_layer_dispose;
}

static void
ottie_composition_layer_init (OttieCompositionLayer *self)
{
  ottie_double_value_init (&self->time_map, 0);
}

OttieLayer *
ottie_composition_layer_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_LAYER,
    { "refId", ottie_parser_option_string, G_STRUCT_OFFSET (OttieCompositionLayer, ref_id) },
    { "tm", ottie_double_value_parse, 0 },
    { "w", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCompositionLayer, width)  },
    { "h", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCompositionLayer, height)  },
  };
  OttieCompositionLayer *self;

  self = g_object_new (OTTIE_TYPE_COMPOSITION_LAYER, NULL);

  if (!ottie_parser_parse_object (reader, "composition layer", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_LAYER (self);
}

OttieComposition *
ottie_composition_layer_get_composition (OttieCompositionLayer *self)
{
  g_return_val_if_fail (OTTIE_IS_COMPOSITION_LAYER (self), NULL);

  return self->composition;
}
