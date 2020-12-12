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

#include "ottiedoublevalueprivate.h"

#include "ottieparserprivate.h"

#include <glib/gi18n-lib.h>

static gboolean
ottie_double_value_parse_value (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  gboolean result, array;

  /* Lottie being Lottie, single values may get dumped into arrays. */
  array = json_reader_is_array (reader);
  if (array)
    json_reader_read_element (reader, 0);

  result = ottie_parser_option_double (reader, offset, data);

  if (array)
    json_reader_end_element (reader);

  return result;
}

static double
ottie_double_value_interpolate (double start,
                                double end,
                                double progress)
{
  return start + (end - start) * progress;
}

#define OTTIE_KEYFRAMES_NAME ottie_double_keyframes
#define OTTIE_KEYFRAMES_TYPE_NAME OttieDoubleKeyframes
#define OTTIE_KEYFRAMES_ELEMENT_TYPE double
#define OTTIE_KEYFRAMES_PARSE_FUNC ottie_double_value_parse_value
#define OTTIE_KEYFRAMES_INTERPOLATE_FUNC ottie_double_value_interpolate
#include "ottiekeyframesimpl.c"

void
ottie_double_value_init (OttieDoubleValue *self,
                         double            value)
{
  self->is_static = TRUE;
  self->static_value = value;
}

void
ottie_double_value_clear (OttieDoubleValue *self)
{
  if (!self->is_static)
    g_clear_pointer (&self->keyframes, ottie_double_keyframes_free);
}

double
ottie_double_value_get (OttieDoubleValue *self,
                        double            timestamp)
{
  if (self->is_static)
    return self->static_value;
  
  return ottie_double_keyframes_get (self->keyframes, timestamp);
}

gboolean
ottie_double_value_parse (JsonReader *reader,
                          gsize       offset,
                          gpointer    data)
{
  OttieDoubleValue *self = (OttieDoubleValue *) ((guint8 *) data + GPOINTER_TO_SIZE (offset));

  if (json_reader_read_member (reader, "k"))
    {
      if (!json_reader_is_array (reader))
        {
          self->is_static = TRUE;
          self->static_value = json_reader_get_double_value (reader);
        }
      else
        {
          self->is_static = FALSE;
          self->keyframes = ottie_double_keyframes_parse (reader);
          if (self->keyframes == NULL)
            {
              json_reader_end_member (reader);
              return FALSE;
            }
        }
    }
  else
    {
      ottie_parser_error_syntax (reader, "Property is not a number");
    }
  json_reader_end_member (reader);

  return TRUE;
}

