/**
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

#include "ottiepoint3dvalueprivate.h"

#include "ottieparserprivate.h"

#include <glib/gi18n-lib.h>

static gboolean
ottie_point3d_value_parse_value (JsonReader *reader,
                                 gsize       offset,
                                 gpointer    data)
{
  double d[3];
  guint n_items;

  if (!ottie_parser_parse_array (reader, "point",
                                 2, 3, &n_items,
                                 0, sizeof (double),
                                 ottie_parser_option_double,
                                 &d))
    return FALSE;

  if (n_items == 2)
    d[2] = NAN; /* We do fixup below */

  *(graphene_point3d_t *) ((guint8 *) data + offset) = GRAPHENE_POINT3D_INIT (d[0], d[1], d[2]);
  return TRUE;
}

#define OTTIE_KEYFRAMES_NAME ottie_point_keyframes
#define OTTIE_KEYFRAMES_TYPE_NAME OttiePointKeyframes
#define OTTIE_KEYFRAMES_ELEMENT_TYPE graphene_point3d_t
#define OTTIE_KEYFRAMES_BY_VALUE 1
#define OTTIE_KEYFRAMES_DIMENSIONS 3
#define OTTIE_KEYFRAMES_PARSE_FUNC ottie_point3d_value_parse_value
#define OTTIE_KEYFRAMES_INTERPOLATE_FUNC graphene_point3d_interpolate
#include "ottiekeyframesimpl.c"

void
ottie_point3d_value_init (OttiePoint3DValue        *self,
                          const graphene_point3d_t *value)
{
  self->is_static = TRUE;
  self->static_value = *value;
}

void
ottie_point3d_value_clear (OttiePoint3DValue *self)
{
  if (!self->is_static)
    g_clear_pointer (&self->keyframes, ottie_point_keyframes_free);
}

void
ottie_point3d_value_get (OttiePoint3DValue  *self,
                         double              timestamp,
                         graphene_point3d_t *value)
{
  if (self->is_static)
    {
      *value = self->static_value;
      return;
    }
  
  ottie_point_keyframes_get (self->keyframes, timestamp, value);
}

gboolean
ottie_point3d_value_parse (JsonReader *reader,
                           float       default_value,
                           gsize       offset,
                           gpointer    data)
{
  OttiePoint3DValue *self = (OttiePoint3DValue *) ((guint8 *) data + offset);

  if (json_reader_read_member (reader, "k"))
    {
      gboolean is_static;

      if (!json_reader_is_array (reader))
        {
          ottie_parser_error_syntax (reader, "Point value needs an array for its value");
          return FALSE;
        }

      if (!json_reader_read_element (reader, 0))
        {
          ottie_parser_emit_error (reader, json_reader_get_error (reader));
          json_reader_end_element (reader);
          return FALSE;
        }

      is_static = !json_reader_is_object (reader);
      json_reader_end_element (reader);

      if (is_static)
        {
          if (!ottie_point3d_value_parse_value (reader, 0, &self->static_value))
            {
              json_reader_end_member (reader);
              return FALSE;
            }
          if (isnan (self->static_value.z))
            self->static_value.z = default_value;
          self->is_static = TRUE;
        }
      else
        {
          OttiePointKeyframes *keyframes = ottie_point_keyframes_parse (reader);
          if (keyframes == NULL)
            {
              json_reader_end_member (reader);
              return FALSE;
            }
          for (int i = 0; i < keyframes->n_items; i++)
            {
              if (isnan (keyframes->items[i].start_value.z))
                keyframes->items[i].start_value.z = default_value;
              if (isnan (keyframes->items[i].end_value.z))
                keyframes->items[i].end_value.z = default_value;
            }
          self->is_static = FALSE;
          self->keyframes = keyframes;
        }
    }
  else
    {
      ottie_parser_error_syntax (reader, "Point value has no value");
    }
  json_reader_end_member (reader);

  return TRUE;
}

