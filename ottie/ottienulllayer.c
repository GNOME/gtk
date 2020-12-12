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

#include "ottienulllayerprivate.h"

#include <glib/gi18n-lib.h>

struct _OttieNullLayer
{
  OttieLayer parent;
};

struct _OttieNullLayerClass
{
  OttieLayerClass parent_class;
};

G_DEFINE_TYPE (OttieNullLayer, ottie_null_layer, OTTIE_TYPE_LAYER)

static void
ottie_null_layer_class_init (OttieNullLayerClass *klass)
{
}

static void
ottie_null_layer_init (OttieNullLayer *self)
{
}

OttieLayer *
ottie_null_layer_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_LAYER,
  };
  OttieNullLayer *self;

  self = g_object_new (OTTIE_TYPE_NULL_LAYER, NULL);

  if (!ottie_parser_parse_object (reader, "null layer", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_LAYER (self);
}

