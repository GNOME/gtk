/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "ottietextvalueprivate.h"

#include "ottieparserprivate.h"
#include "ottieprinterprivate.h"

#include <glib/gi18n-lib.h>

static inline void
text_item_copy (const OttieTextItem *source,
                OttieTextItem       *dest)
{
  *dest = *source;
  dest->font = g_strdup (dest->font);
  dest->text = g_strdup (dest->text);
}

static gboolean
ottie_text_value_parse_one (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  OttieTextItem *item = (OttieTextItem *) ((guint8 *) data + offset);
  OttieParserOption options[] = {
    { "f", ottie_parser_option_string, G_STRUCT_OFFSET (OttieTextItem, font) },
    { "t", ottie_parser_option_string, G_STRUCT_OFFSET (OttieTextItem, text) },
    { "s", ottie_parser_option_double, G_STRUCT_OFFSET (OttieTextItem, size) },
    { "fc", ottie_parser_option_color, G_STRUCT_OFFSET (OttieTextItem, color) },
    { "j", ottie_parser_option_text_justify, G_STRUCT_OFFSET (OttieTextItem, justify) },
    { "lh", ottie_parser_option_double, G_STRUCT_OFFSET (OttieTextItem, line_height) },
    { "ls", ottie_parser_option_double, G_STRUCT_OFFSET (OttieTextItem, line_shift) },
    { "tr", ottie_parser_option_double, G_STRUCT_OFFSET (OttieTextItem, tracking) },
  };

  if (!ottie_parser_parse_object (reader, "text value",
                                  options, G_N_ELEMENTS (options),
                                  item))
    {
      g_print ("sorry no text\n");
    }

  return TRUE;
}

static void
ottie_text_value_print_one (OttiePrinter        *printer,
                            const char          *name,
                            const OttieTextItem *text)
{
  ottie_printer_start_object (printer, name);
  ottie_printer_add_string (printer, "f", text->font);
  ottie_printer_add_string (printer, "t", text->text);
  ottie_printer_add_double (printer, "s", text->size);
  g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"fc\" : [ %g, %g, %g ]",
                          text->color.red, text->color.green, text->color.blue);
  ottie_printer_add_int (printer, "j", text->justify);
  ottie_printer_add_double (printer, "lh", text->line_height);
  ottie_printer_add_double (printer, "ls", text->line_shift);
  ottie_printer_add_double (printer, "tr", text->tracking);
  ottie_printer_end_object (printer);
}

static void
ottie_text_value_interpolate (const OttieTextItem *start,
                              const OttieTextItem *end,
                              double               progress,
                              OttieTextItem       *result)
{
  text_item_copy (start, result);
}

#define OTTIE_KEYFRAMES_NAME ottie_text_keyframes
#define OTTIE_KEYFRAMES_TYPE_NAME OttieTextKeyframes
#define OTTIE_KEYFRAMES_ELEMENT_TYPE OttieTextItem
#define OTTIE_KEYFRAMES_BY_VALUE 1
#define OTTIE_KEYFRAMES_PARSE_FUNC ottie_text_value_parse_one
#define OTTIE_KEYFRAMES_INTERPOLATE_FUNC ottie_text_value_interpolate
#define OTTIE_KEYFRAMES_PRINT_FUNC ottie_text_value_print_one
#include "ottiekeyframesimpl.c"

void
ottie_text_value_init (OttieTextValue      *self,
                       const OttieTextItem *value)
{
  self->is_static = TRUE;
  text_item_copy (value, &self->static_value);
}

void
ottie_text_value_clear (OttieTextValue *self)
{
  if (!self->is_static)
    g_clear_pointer (&self->keyframes, ottie_text_keyframes_free);
}

void
ottie_text_value_get (OttieTextValue *self,
                      double          timestamp,
                      OttieTextItem  *text)
{
  if (self->is_static)
    {
      text_item_copy (&self->static_value, text);
      return;
    }

  ottie_text_keyframes_get (self->keyframes, timestamp, text);
}

gboolean
ottie_text_value_parse (JsonReader *reader,
                        gsize       offset,
                        gpointer    data)
{
  OttieTextValue *self = (OttieTextValue *) ((guint8 *) data + offset);

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
          ottie_text_value_parse_one (reader, 0, &self->static_value);
        }
      else
        {
          self->is_static = FALSE;
          self->keyframes = ottie_text_keyframes_parse (reader);
          if (self->keyframes == NULL)
            {
              json_reader_end_member (reader);
              return FALSE;
            }
        }
    }
  else
    {
      ottie_parser_error_syntax (reader, "Property is not a text value");
    }
  json_reader_end_member (reader);

  return TRUE;
}

void
ottie_text_value_print (OttieTextValue *self,
                        const char     *name,
                        OttiePrinter   *printer)
{
  ottie_printer_start_object (printer, name);

  ottie_printer_add_boolean (printer, "a", !self->is_static);
  if (self->is_static)
    ottie_text_value_print_one (printer, "k", &self->static_value);
  else
    ottie_text_keyframes_print (self->keyframes, printer);

  ottie_printer_end_object (printer);
}

