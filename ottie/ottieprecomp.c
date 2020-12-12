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

#include "ottieprecompprivate.h"

#include "ottieparserprivate.h"
#include "ottieprecomplayerprivate.h"
#include "ottieshapelayerprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

#define GDK_ARRAY_ELEMENT_TYPE OttieLayer *
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_TYPE_NAME OttieLayerList
#define GDK_ARRAY_NAME ottie_layer_list
#define GDK_ARRAY_PREALLOC 4
#include "gdk/gdkarrayimpl.c"

struct _OttiePrecomp
{
  OttieLayer parent;

  OttieLayerList layers;
};

struct _OttiePrecompClass
{
  OttieLayerClass parent_class;
};

G_DEFINE_TYPE (OttiePrecomp, ottie_precomp, OTTIE_TYPE_LAYER)

static void
ottie_precomp_snapshot (OttieLayer  *layer,
                        GtkSnapshot *snapshot,
                        double       timestamp)
{
  OttiePrecomp *self = OTTIE_PRECOMP (layer);

  for (gsize i = 0; i < ottie_layer_list_get_size (&self->layers); i++)
    {
      ottie_layer_snapshot (ottie_layer_list_get (&self->layers, i), snapshot, timestamp);
    }
}

static void
ottie_precomp_dispose (GObject *object)
{
  OttiePrecomp *self = OTTIE_PRECOMP (object);

  ottie_layer_list_clear (&self->layers);

  G_OBJECT_CLASS (ottie_precomp_parent_class)->dispose (object);
}

static void
ottie_precomp_finalize (GObject *object)
{
  //OttiePrecomp *self = OTTIE_PRECOMP (object);

  G_OBJECT_CLASS (ottie_precomp_parent_class)->finalize (object);
}

static void
ottie_precomp_class_init (OttiePrecompClass *klass)
{
  OttieLayerClass *layer_class = OTTIE_LAYER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  layer_class->snapshot = ottie_precomp_snapshot;

  gobject_class->finalize = ottie_precomp_finalize;
  gobject_class->dispose = ottie_precomp_dispose;
}

static void
ottie_precomp_init (OttiePrecomp *self)
{
  ottie_layer_list_init (&self->layers);
}

static gboolean
ottie_precomp_parse_layer (JsonReader *reader,
                           gsize       offset,
                           gpointer    data)
{
  OttiePrecomp *self = data;
  OttieLayer *layer;
  int type;

  if (!json_reader_is_object (reader))
    {
      ottie_parser_error_syntax (reader, "Layer %zu is not an object",
                                 ottie_layer_list_get_size (&self->layers));
      return FALSE;
    }

  if (!json_reader_read_member (reader, "ty"))
    {
      ottie_parser_error_syntax (reader, "Layer %zu has no type",
                                 ottie_layer_list_get_size (&self->layers));
      json_reader_end_member (reader);
      return FALSE;
    }

  type = json_reader_get_int_value (reader);
  json_reader_end_member (reader);

  switch (type)
  {
    case 0:
      layer = ottie_precomp_layer_parse (reader);
      break;

    case 4:
      layer = ottie_shape_layer_parse (reader);
      break;

    default:
      ottie_parser_error_value (reader, "Layer %zu has unknown type %d",
                                ottie_layer_list_get_size (&self->layers),
                                type);
      layer = NULL;
      break;
  }

  if (layer)
    ottie_layer_list_append (&self->layers, layer);

  return TRUE;
}

gboolean
ottie_precomp_parse_layers (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  OttiePrecomp **target = (OttiePrecomp **) ((guint8 *) data + offset);
  OttiePrecomp *self;

  self = g_object_new (OTTIE_TYPE_PRECOMP, NULL);

  if (!ottie_parser_parse_array (reader, "layers",
                                 0, G_MAXUINT, NULL,
                                 0, 0,
                                 ottie_precomp_parse_layer,
                                 self))
    {
      g_object_unref (self);
      return FALSE;
    }

  g_clear_object (target);
  *target = self;

  return TRUE;
}

