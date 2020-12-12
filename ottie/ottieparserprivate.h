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

#ifndef __OTTIE_PARSER_PRIVATE_H__
#define __OTTIE_PARSER_PRIVATE_H__

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/* for integers where we want to track that nobody has assigned a value to them */
#define OTTIE_INT_UNSET G_MININT

typedef enum
{
  OTTIE_DIRECTION_FORWARD,
  OTTIE_DIRECTION_BACKWARD
} OttieDirection;

typedef struct _OttieParserOption OttieParserOption;

typedef gboolean (* OttieParseFunc) (JsonReader *reader, gsize offset, gpointer data);

struct _OttieParserOption
{
  const char *name;
  OttieParseFunc parse_func;
  gsize option_data;
};

void                    ottie_parser_emit_error                 (JsonReader             *reader,
                                                                 const GError           *error);
void                    ottie_parser_error_syntax               (JsonReader             *reader,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);
void                    ottie_parser_error_value                (JsonReader             *reader,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);
void                    ottie_parser_error_unsupported          (JsonReader             *reader,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);

gboolean                ottie_parser_parse_array               (JsonReader              *reader,
                                                                const char              *debug_name,
                                                                guint                    min_items,
                                                                guint                    max_items,
                                                                guint                   *out_n_items,
                                                                gsize                    start_offset,
                                                                gsize                    offset_multiplier,
                                                                OttieParseFunc           func,
                                                                gpointer                 data);

gboolean                ottie_parser_parse_object              (JsonReader              *reader,
                                                                const char              *debug_name,
                                                                const OttieParserOption *options,
                                                                gsize                    n_options,
                                                                gpointer                 data);
gboolean                ottie_parser_option_skip               (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
#define ottie_parser_option_skip_index ottie_parser_option_skip
#define ottie_parser_option_skip_expression ottie_parser_option_skip
gboolean                ottie_parser_option_boolean            (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_int                (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_double             (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_string             (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_3d                 (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_direction          (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_blend_mode         (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_line_cap           (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_line_join          (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_fill_rule          (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);
gboolean                ottie_parser_option_transform          (JsonReader              *reader,
                                                                gsize                    offset,
                                                                gpointer                 data);

G_END_DECLS

#endif /* __OTTIE_PARSER_PRIVATE_H__ */
