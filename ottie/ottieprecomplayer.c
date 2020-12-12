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

#include "ottieprecomplayerprivate.h"

#include "ottiedoublevalueprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

struct _OttiePrecompLayer
{
  OttieLayer parent;

  OttieDoubleValue time_map;

  char *ref_id;
  OttieLayer *ref;
};

struct _OttiePrecompLayerClass
{
  OttieLayerClass parent_class;
};

G_DEFINE_TYPE (OttiePrecompLayer, ottie_precomp_layer, OTTIE_TYPE_LAYER)

static void
ottie_precomp_layer_snapshot (OttieLayer  *layer,
                              GtkSnapshot *snapshot,
                              double       timestamp)
{
  OttiePrecompLayer *self = OTTIE_PRECOMP_LAYER (layer);

  if (self->ref == NULL)
    return;

  ottie_layer_snapshot (self->ref,
                        snapshot,
                        ottie_double_value_get (&self->time_map, timestamp));
}

static void
ottie_precomp_layer_dispose (GObject *object)
{
  OttiePrecompLayer *self = OTTIE_PRECOMP_LAYER (object);

  g_clear_object (&self->ref);
  g_clear_pointer (&self->ref_id, g_free);
  ottie_double_value_clear (&self->time_map);

  G_OBJECT_CLASS (ottie_precomp_layer_parent_class)->dispose (object);
}

static void
ottie_precomp_layer_finalize (GObject *object)
{
  //OttiePrecompLayer *self = OTTIE_PRECOMP_LAYER (object);

  G_OBJECT_CLASS (ottie_precomp_layer_parent_class)->finalize (object);
}

static void
ottie_precomp_layer_class_init (OttiePrecompLayerClass *klass)
{
  OttieLayerClass *layer_class = OTTIE_LAYER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  layer_class->snapshot = ottie_precomp_layer_snapshot;

  gobject_class->finalize = ottie_precomp_layer_finalize;
  gobject_class->dispose = ottie_precomp_layer_dispose;
}

static void
ottie_precomp_layer_init (OttiePrecompLayer *self)
{
  ottie_double_value_init (&self->time_map, 0);
}

OttieLayer *
ottie_precomp_layer_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_LAYER
    { "refId", ottie_parser_option_string, G_STRUCT_OFFSET (OttiePrecompLayer, ref_id) },
    { "tm", ottie_double_value_parse, 0 },
  };
  OttiePrecompLayer *self;

  self = g_object_new (OTTIE_TYPE_PRECOMP_LAYER, NULL);

  if (!ottie_parser_parse_object (reader, "precomp layer", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_LAYER (self);
}

