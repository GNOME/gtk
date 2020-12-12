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

#include "ottieshapeprivate.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (OttieShape, ottie_shape, G_TYPE_OBJECT)

static void
ottie_shape_dispose (GObject *object)
{
  OttieShape *self = OTTIE_SHAPE (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->match_name, g_free);

  G_OBJECT_CLASS (ottie_shape_parent_class)->dispose (object);
}

static void
ottie_shape_finalize (GObject *object)
{
  //OttieShape *self = OTTIE_SHAPE (object);

  G_OBJECT_CLASS (ottie_shape_parent_class)->finalize (object);
}

static void
ottie_shape_class_init (OttieShapeClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = ottie_shape_finalize;
  gobject_class->dispose = ottie_shape_dispose;
}

static void
ottie_shape_init (OttieShape *self)
{
}

void
ottie_shape_snapshot (OttieShape         *self,
                      GtkSnapshot        *snapshot,
                      OttieShapeSnapshot *snapshot_data,
                      double              timestamp)
{
  OTTIE_SHAPE_GET_CLASS (self)->snapshot (self, snapshot, snapshot_data, timestamp);
}

void
ottie_shape_snapshot_init (OttieShapeSnapshot *data,
                           OttieShapeSnapshot *copy_from)
{
  if (copy_from)
    {
      data->paths = g_slist_copy_deep (copy_from->paths, (GCopyFunc) gsk_path_ref, NULL);
      if (copy_from->cached_path)
        data->cached_path = gsk_path_ref (copy_from->cached_path);
      else
        data->cached_path = NULL;
    }
  else
    {
      memset (data, 0, sizeof (OttieShapeSnapshot));
    }
}

void
ottie_shape_snapshot_clear (OttieShapeSnapshot *data)
{
  g_slist_free_full (data->paths, (GDestroyNotify) gsk_path_unref);
  data->paths = NULL;

  g_clear_pointer (&data->cached_path, gsk_path_unref);
}

void
ottie_shape_snapshot_add_path (OttieShapeSnapshot *data,
                               GskPath        *path)
{
  g_clear_pointer (&data->cached_path, gsk_path_unref);
  data->paths = g_slist_prepend (data->paths, path);
}

GskPath *
ottie_shape_snapshot_get_path (OttieShapeSnapshot *data)
{
  GskPathBuilder *builder;
  GSList *l;

  if (data->cached_path)
    return data->cached_path;

  builder = gsk_path_builder_new ();
  for (l = data->paths; l; l = l->next)
    {
      gsk_path_builder_add_path (builder, l->data);
    }
  data->cached_path = gsk_path_builder_free_to_path (builder);

  return data->cached_path;
}

