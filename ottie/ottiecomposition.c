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

#include "ottiecompositionprivate.h"

#include "ottieparserprivate.h"
#include "ottiecompositionlayerprivate.h"
#include "ottienulllayerprivate.h"
#include "ottieshapelayerprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

#define GDK_ARRAY_ELEMENT_TYPE OttieLayer *
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_TYPE_NAME OttieLayerList
#define GDK_ARRAY_NAME ottie_layer_list
#define GDK_ARRAY_PREALLOC 4
#include "gdk/gdkarrayimpl.c"

struct _OttieComposition
{
  OttieLayer parent;

  OttieLayerList layers;
  GHashTable *layers_by_index;
};

struct _OttieCompositionClass
{
  OttieLayerClass parent_class;
};

static GType
ottie_composition_get_item_type (GListModel *list)
{
  return OTTIE_TYPE_LAYER;
}

static guint
ottie_composition_get_n_items (GListModel *list)
{
  OttieComposition *self = OTTIE_COMPOSITION (list);

  return ottie_layer_list_get_size (&self->layers);
}

static gpointer
ottie_composition_get_item (GListModel *list,
                             guint       position)
{
  OttieComposition *self = OTTIE_COMPOSITION (list);

  if (position >= ottie_layer_list_get_size (&self->layers))
    return NULL;

  return g_object_ref (ottie_layer_list_get (&self->layers, position));
}

static void
ottie_composition_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = ottie_composition_get_item_type;
  iface->get_n_items = ottie_composition_get_n_items;
  iface->get_item = ottie_composition_get_item;
}

G_DEFINE_TYPE_WITH_CODE (OttieComposition, ottie_composition, OTTIE_TYPE_LAYER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, ottie_composition_list_model_init))

static void
ottie_composition_update (OttieLayer *layer,
                          GHashTable *compositions)
{
  OttieComposition *self = OTTIE_COMPOSITION (layer);

  for (gsize i = ottie_layer_list_get_size (&self->layers); i-- > 0; )
    {
      ottie_layer_update (ottie_layer_list_get (&self->layers, i), compositions);
    }
}

static void
ottie_composition_render (OttieLayer  *layer,
                          OttieRender *render,
                          double       timestamp)
{
  OttieComposition *self = OTTIE_COMPOSITION (layer);
  OttieRender child_render;

  ottie_render_init (&child_render);

  for (gsize i = 0; i < ottie_layer_list_get_size (&self->layers); i++)
    {
      OttieLayer *child = ottie_layer_list_get (&self->layers, i);

      ottie_layer_render (child, &child_render, timestamp);
      /* XXX: Should we clear paths here because they're not needed anymore? */

      /* Use a counter here to avoid inflooping */
      for (gsize j = 0; j < ottie_layer_list_get_size (&self->layers); j++)
        {
          if (child->transform)
            ottie_shape_render (OTTIE_SHAPE (child->transform), &child_render, timestamp);
          if (child->parent_index == OTTIE_INT_UNSET)
            break;
          child = g_hash_table_lookup (self->layers_by_index, GINT_TO_POINTER (child->parent_index));
          if (child == NULL)
            break;
        }

      ottie_render_merge (render, &child_render);
    }

  ottie_render_clear (&child_render);
}

static void
ottie_composition_dispose (GObject *object)
{
  OttieComposition *self = OTTIE_COMPOSITION (object);

  ottie_layer_list_clear (&self->layers);
  g_hash_table_remove_all (self->layers_by_index);

  G_OBJECT_CLASS (ottie_composition_parent_class)->dispose (object);
}

static void
ottie_composition_finalize (GObject *object)
{
  OttieComposition *self = OTTIE_COMPOSITION (object);

  g_hash_table_unref (self->layers_by_index);

  G_OBJECT_CLASS (ottie_composition_parent_class)->finalize (object);
}

static void
ottie_composition_class_init (OttieCompositionClass *klass)
{
  OttieLayerClass *layer_class = OTTIE_LAYER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  layer_class->update = ottie_composition_update;
  layer_class->render = ottie_composition_render;

  gobject_class->dispose = ottie_composition_dispose;
  gobject_class->finalize = ottie_composition_finalize;
}

static void
ottie_composition_init (OttieComposition *self)
{
  ottie_layer_list_init (&self->layers);

  self->layers_by_index = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
ottie_composition_append (OttieComposition *self,
                          OttieLayer       *layer)
{
  ottie_layer_list_append (&self->layers, layer);
  if (layer->index != OTTIE_INT_UNSET)
    g_hash_table_insert (self->layers_by_index, GINT_TO_POINTER (layer->index), layer);
  g_list_model_items_changed (G_LIST_MODEL (self), ottie_layer_list_get_size (&self->layers), 0, 1);
}

static gboolean
ottie_composition_parse_layer (JsonReader *reader,
                               gsize       offset,
                               gpointer    data)
{
  OttieComposition *self = data;
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
      layer = ottie_composition_layer_parse (reader);
      break;

    case 3:
      layer = ottie_null_layer_parse (reader);
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
    ottie_composition_append (self, layer);

  return TRUE;
}

gboolean
ottie_composition_parse_layers (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  OttieComposition **target = (OttieComposition **) ((guint8 *) data + offset);
  OttieComposition *self;

  self = g_object_new (OTTIE_TYPE_COMPOSITION, NULL);

  if (!ottie_parser_parse_array (reader, "layers",
                                 0, G_MAXUINT, NULL,
                                 0, 0,
                                 ottie_composition_parse_layer,
                                 self))
    {
      g_object_unref (self);
      return FALSE;
    }

  g_clear_object (target);
  *target = self;

  return TRUE;
}

