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

#include "ottiecolorvalueprivate.h"

#include "ottieparserprivate.h"

#include <glib/gi18n-lib.h>

static gboolean
ottie_color_value_parse_one (JsonReader *reader,
                             gsize       offset,
                             gpointer    data)
{
  GdkRGBA *rgba = (GdkRGBA *) ((guint8 *) data + offset);
  double d[3];

  if (!ottie_parser_parse_array (reader, "color value",
                                 3, 3, NULL,
                                 0, sizeof (double),
                                 ottie_parser_option_double,
                                 d))
    {
      d[0] = d[1] = d[2] = 0;
    }

  rgba->red = d[0];
  rgba->green = d[1];
  rgba->blue = d[2];
  rgba->alpha = 1;

  return TRUE;
}

static void
ottie_color_value_interpolate (const GdkRGBA *start,
                               const GdkRGBA *end,
                               double         progress,
                               GdkRGBA       *result)
{
  result->red = start->red + progress * (end->red - start->red);
  result->green = start->green + progress * (end->green - start->green);
  result->blue = start->blue + progress * (end->blue - start->blue);
  result->alpha = start->alpha + progress * (end->alpha - start->alpha);
}

#define OTTIE_KEYFRAMES_NAME ottie_color_keyframes
#define OTTIE_KEYFRAMES_TYPE_NAME OttieColorKeyframes
#define OTTIE_KEYFRAMES_ELEMENT_TYPE GdkRGBA
#define OTTIE_KEYFRAMES_BY_VALUE 1
#define OTTIE_KEYFRAMES_DIMENSIONS 4
#define OTTIE_KEYFRAMES_PARSE_FUNC ottie_color_value_parse_one
#define OTTIE_KEYFRAMES_INTERPOLATE_FUNC ottie_color_value_interpolate
#include "ottiekeyframesimpl.c"

void
ottie_color_value_init (OttieColorValue *self,
                        const GdkRGBA   *value)
{
  self->is_static = TRUE;
  self->static_value = *value;
}

void
ottie_color_value_clear (OttieColorValue *self)
{
  if (!self->is_static)
    g_clear_pointer (&self->keyframes, ottie_color_keyframes_free);
}

void
ottie_color_value_get (OttieColorValue *self,
                       double           timestamp,
                       GdkRGBA         *rgba)
{
  if (self->is_static)
    {
      *rgba = self->static_value;
      return;
    }
  
  ottie_color_keyframes_get (self->keyframes, timestamp, rgba);
}

gboolean
ottie_color_value_parse (JsonReader *reader,
                         gsize       offset,
                         gpointer    data)
{
  OttieColorValue *self = (OttieColorValue *) ((guint8 *) data + offset);

  if (json_reader_read_member (reader, "k"))
    {
      gboolean is_static;

      if (!json_reader_is_array (reader))
        is_static = TRUE;
      else
        {
          if (json_reader_read_element (reader, 0))
            is_static = !json_reader_is_object (reader);
          else
            is_static = TRUE;
          json_reader_end_element (reader);
        }

      if (is_static)
        {
          self->is_static = TRUE;
          ottie_color_value_parse_one (reader, 0, &self->static_value);
        }
      else
        {
          self->is_static = FALSE;
          self->keyframes = ottie_color_keyframes_parse (reader);
          if (self->keyframes == NULL)
            {
              json_reader_end_member (reader);
              return FALSE;
            }
        }
    }
  else
    {
      ottie_parser_error_syntax (reader, "Property is not a color value");
    }
  json_reader_end_member (reader);

  return TRUE;
}

