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

#include "ottielayerprivate.h"

#include <glib/gi18n-lib.h>
#include <json-glib/json-glib.h>

G_DEFINE_TYPE (OttieLayer, ottie_layer, G_TYPE_OBJECT)

static void
ottie_layer_dispose (GObject *object)
{
  OttieLayer *self = OTTIE_LAYER (object);

  g_clear_object (&self->transform);

  G_OBJECT_CLASS (ottie_layer_parent_class)->dispose (object);
}

static void
ottie_layer_finalize (GObject *object)
{
  //OttieLayer *self = OTTIE_LAYER (object);

  G_OBJECT_CLASS (ottie_layer_parent_class)->finalize (object);
}

static void
ottie_layer_class_init (OttieLayerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = ottie_layer_finalize;
  gobject_class->dispose = ottie_layer_dispose;
}

static void
ottie_layer_init (OttieLayer *self)
{
  self->stretch = 1;
  self->blend_mode = GSK_BLEND_MODE_DEFAULT;
}

void
ottie_layer_snapshot (OttieLayer  *self,
                      GtkSnapshot *snapshot,
                      double       timestamp)
{
  if (self->transform)
    {
      GskTransform *transform;

      transform = ottie_transform_get_transform (self->transform, timestamp);
      gtk_snapshot_transform (snapshot, transform);
      gsk_transform_unref (transform);
    }

  OTTIE_LAYER_GET_CLASS (self)->snapshot (self, snapshot, timestamp);
}

