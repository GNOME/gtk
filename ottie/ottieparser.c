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

#include "ottieparserprivate.h"

#include "ottietransformprivate.h"

#include <gsk/gsk.h>

void
ottie_parser_emit_error (JsonReader    *reader,
                         const GError  *error)
{
  g_printerr ("Ottie is sad: %s\n", error->message);
}

void
ottie_parser_error_syntax (JsonReader *reader,
                           const char *format,
                           ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (JSON_PARSER_ERROR,
                              JSON_PARSER_ERROR_INVALID_DATA,
                              format, args);
  va_end (args);

  ottie_parser_emit_error (reader, error);

  g_error_free (error);
}

void
ottie_parser_error_value (JsonReader *reader,
                          const char *format,
                          ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (JSON_PARSER_ERROR,
                              JSON_PARSER_ERROR_INVALID_DATA,
                              format, args);
  va_end (args);

  ottie_parser_emit_error (reader, error);

  g_error_free (error);
}

void
ottie_parser_error_unsupported (JsonReader *reader,
                                const char *format,
                                ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (JSON_PARSER_ERROR,
                              JSON_PARSER_ERROR_INVALID_DATA,
                              format, args);
  va_end (args);

  ottie_parser_emit_error (reader, error);

  g_error_free (error);
}

gboolean
ottie_parser_parse_array (JsonReader     *reader,
                          const char     *debug_name,
                          guint           min_items,
                          guint           max_items,
                          guint          *out_n_items,
                          gsize           start_offset,
                          gsize           offset_multiplier,
                          OttieParseFunc  func,
                          gpointer        data)
{
  guint i;

  if (!json_reader_is_array (reader))
    {
      if (min_items > 1)
        {
          ottie_parser_error_syntax (reader, "Expected an array when parsing %s", debug_name);
          if (out_n_items)
            *out_n_items = 0;
          return FALSE;
        }
      else
        {
          if (!func (reader, start_offset, data))
            {
              if (out_n_items)
                *out_n_items = 0;
              return FALSE;
            }
          if (out_n_items)
            *out_n_items = 1;
          return TRUE;
        }
    }

  if (json_reader_count_elements (reader) < min_items)
    {
      ottie_parser_error_syntax (reader, "%s needs %u items, but only %u given",
                                 debug_name, min_items, json_reader_count_elements (reader));
      return FALSE;
    }
  max_items = MIN (max_items, json_reader_count_elements (reader));

  for (i = 0; i < max_items; i++)
    {
      if (!json_reader_read_element (reader, i) || 
          !func (reader, start_offset + offset_multiplier * i, data))
        {
          json_reader_end_element (reader);
          if (out_n_items)
            *out_n_items = i;
          return FALSE;
        }

      json_reader_end_element (reader);
    }

  if (out_n_items)
    *out_n_items = i;
  return TRUE;
}

gboolean
ottie_parser_parse_object (JsonReader              *reader,
                           const char              *debug_name,
                           const OttieParserOption *options,
                           gsize                    n_options,
                           gpointer                 data)
{
  if (!json_reader_is_object (reader))
    {
      ottie_parser_error_syntax (reader, "Expected an object when parsing %s", debug_name);
      return FALSE;
    }

  for (int i = 0; ; i++)
    {
      const OttieParserOption *o = NULL;
      const char *name;

      if (!json_reader_read_element (reader, i))
        break;

      name = json_reader_get_member_name (reader);

      for (gsize j = 0; j < n_options; j++)
        {
          o = &options[j];
          if (g_str_equal (o->name, name))
            break;
          o = NULL;
        }

      if (o)
        {
          if (!o->parse_func (reader, o->option_data, data))
            {
              json_reader_end_element (reader);
              return FALSE;
            }
        }
      else
        {
          ottie_parser_error_unsupported (reader, "Unsupported %s property \"%s\"", debug_name, json_reader_get_member_name (reader));
        }

      json_reader_end_element (reader);
    }

  json_reader_end_element (reader);

  return TRUE;
}

gboolean
ottie_parser_option_skip (JsonReader *reader,
                          gsize       offset,
                          gpointer    data)
{
  return TRUE;
}

gboolean
ottie_parser_option_boolean (JsonReader *reader,
                             gsize       offset,
                             gpointer    data)
{
  gboolean b;

  b = json_reader_get_boolean_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      json_reader_end_element (reader);
      return FALSE;
    }
  
  *(gboolean *) ((guint8 *) data + offset) = b;

  return TRUE;
}

gboolean
ottie_parser_option_double (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  double d;

  d = json_reader_get_double_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  *(double *) ((guint8 *) data + offset) = d;

  return TRUE;
}

gboolean
ottie_parser_option_int (JsonReader *reader,
                         gsize       offset,
                         gpointer    data)
{
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  if (i > G_MAXINT || i < G_MININT)
    {
      ottie_parser_error_value (reader, "Integer value %"G_GINT64_FORMAT" out of range", i);
      return FALSE;
    }
  if (i == OTTIE_INT_UNSET)
    {
      ottie_parser_error_unsupported (reader, "The Integer value %d is a magic internal value of Ottie, file a bug", OTTIE_INT_UNSET);
      return FALSE;
    }

  *(int *) ((guint8 *) data + offset) = i;

  return TRUE;
}

gboolean
ottie_parser_option_string (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  char **target;
  const char *s;

  s = json_reader_get_string_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  target = (char **) ((guint8 *) data + offset);

  g_clear_pointer (target, g_free);
  *target = g_strdup (s);

  return TRUE;
}

gboolean
ottie_parser_option_blend_mode (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  GskBlendMode blend_mode;
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  switch (i)
  {
    case 0:
      blend_mode = GSK_BLEND_MODE_DEFAULT;
      break;

    case 1:
      blend_mode = GSK_BLEND_MODE_MULTIPLY;
      break;

    case 2:
      blend_mode = GSK_BLEND_MODE_SCREEN;
      break;

    case 3:
      blend_mode = GSK_BLEND_MODE_OVERLAY;
      break;

    case 4:
      blend_mode = GSK_BLEND_MODE_DARKEN;
      break;

    case 5:
      blend_mode = GSK_BLEND_MODE_LIGHTEN;
      break;

    case 6:
      blend_mode = GSK_BLEND_MODE_COLOR_DODGE;
      break;

    case 7:
      blend_mode = GSK_BLEND_MODE_COLOR_BURN;
      break;

    case 8:
      blend_mode = GSK_BLEND_MODE_HARD_LIGHT;
      break;

    case 9:
      blend_mode = GSK_BLEND_MODE_SOFT_LIGHT;
      break;

    case 10:
      blend_mode = GSK_BLEND_MODE_DIFFERENCE;
      break;

    case 11:
      blend_mode = GSK_BLEND_MODE_EXCLUSION;
      break;

    case 12:
      blend_mode = GSK_BLEND_MODE_HUE;
      break;

    case 13:
      blend_mode = GSK_BLEND_MODE_SATURATION;
      break;

    case 14:
      blend_mode = GSK_BLEND_MODE_COLOR;
      break;

    case 15:
      blend_mode = GSK_BLEND_MODE_LUMINOSITY;
      break;

    default:
      ottie_parser_error_value (reader, "%"G_GINT64_FORMAT" is not a known blend mode", i);
      return FALSE;
  }

  if (blend_mode != GSK_BLEND_MODE_DEFAULT)
    ottie_parser_error_value (reader, "Blend modes are not implemented yet.");
    
  *(GskBlendMode *) ((guint8 *) data + offset) = blend_mode;

  return TRUE;
}

gboolean
ottie_parser_option_3d (JsonReader *reader,
                        gsize       offset,
                        gpointer    data)
{
  double d;

  d = json_reader_get_double_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  if (d != 0)
    {
      ottie_parser_error_value (reader, "3D is not supported.");
    }

  return TRUE;
}

gboolean
ottie_parser_option_direction (JsonReader *reader,
                               gsize       offset,
                               gpointer    data)
{
  OttieDirection direction;
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  switch (i)
  {
    default:
      ottie_parser_error_value (reader, "%"G_GINT64_FORMAT" is not a known direction", i);
      G_GNUC_FALLTHROUGH;
    case 0:
      direction = OTTIE_DIRECTION_FORWARD;
      break;

    case 1:
    case 2:
      direction = OTTIE_DIRECTION_BACKWARD;
      break;
  }

  *(OttieDirection *) ((guint8 *) data + offset) = direction;

  return TRUE;
}

gboolean
ottie_parser_option_line_cap (JsonReader *reader,
                              gsize       offset,
                              gpointer    data)
{
  GskLineCap line_cap;
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  switch (i)
  {
    case 1:
      line_cap = GSK_LINE_CAP_BUTT;
      break;

    case 2:
      line_cap = GSK_LINE_CAP_ROUND;
      break;

    case 3:
      line_cap = GSK_LINE_CAP_SQUARE;
      break;

    default:
      ottie_parser_error_value (reader, "%"G_GINT64_FORMAT" is not a known line cap", i);
      return FALSE;
  }

  *(GskLineCap *) ((guint8 *) data + offset) = line_cap;

  return TRUE;
}

gboolean
ottie_parser_option_line_join (JsonReader *reader,
                               gsize       offset,
                               gpointer    data)
{
  GskLineJoin line_join;
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  switch (i)
  {
    case 1:
      line_join = GSK_LINE_JOIN_MITER;
      break;

    case 2:
      line_join = GSK_LINE_JOIN_ROUND;
      break;

    case 3:
      line_join = GSK_LINE_JOIN_BEVEL;
      break;

    default:
      ottie_parser_error_value (reader, "%"G_GINT64_FORMAT" is not a known line join", i);
      return FALSE;
  }

  *(GskLineJoin *) ((guint8 *) data + offset) = line_join;

  return TRUE;
}

gboolean
ottie_parser_option_fill_rule (JsonReader *reader,
                               gsize       offset,
                               gpointer    data)
{
  GskFillRule fill_rule;
  gint64 i;

  i = json_reader_get_int_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  switch (i)
  {
    case 1:
      fill_rule = GSK_FILL_RULE_WINDING;
      break;

    case 2:
      fill_rule = GSK_FILL_RULE_EVEN_ODD;
      break;

    default:
      ottie_parser_error_value (reader, "%"G_GINT64_FORMAT" is not a known fill rule", i);
      /* XXX: really? */
      fill_rule = GSK_FILL_RULE_EVEN_ODD;
      break;
  }

  *(GskFillRule *) ((guint8 *) data + offset) = fill_rule;

  return TRUE;
}

gboolean
ottie_parser_option_transform (JsonReader *reader,
                               gsize       offset,
                               gpointer    data)
{
  OttieShape **target;
  OttieShape *t;

  t = ottie_transform_parse (reader);
  if (t == NULL)
    return FALSE;
  
  target = (OttieShape **) ((guint8 *) data + offset);

  g_clear_object (target);
  *target = t;

  return TRUE;
}

