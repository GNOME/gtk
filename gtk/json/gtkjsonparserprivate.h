/*
 * Copyright © 2021 Benjamin Otte
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


#ifndef __GTK_JSON_PARSER_H__
#define __GTK_JSON_PARSER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  GTK_JSON_NONE,
  GTK_JSON_NULL,
  GTK_JSON_BOOLEAN,
  GTK_JSON_NUMBER,
  GTK_JSON_STRING,
  GTK_JSON_OBJECT,
  GTK_JSON_ARRAY
} GtkJsonNode;

typedef struct _GtkJsonParser GtkJsonParser;

GtkJsonParser *         gtk_json_parser_new_for_bytes           (GBytes                 *bytes);
GtkJsonParser *         gtk_json_parser_new_for_string          (const char             *string,
                                                                 gssize                  size);

void                    gtk_json_parser_free                    (GtkJsonParser          *self);

gboolean                gtk_json_parser_next                    (GtkJsonParser          *self);
GtkJsonNode             gtk_json_parser_get_node                (GtkJsonParser          *self);
const GError *          gtk_json_parser_get_error               (GtkJsonParser          *self) G_GNUC_PURE;
char *                  gtk_json_parser_get_member_name         (GtkJsonParser          *self);
gssize                  gtk_json_parser_select_member           (GtkJsonParser          *self,
                                                                 const char * const     *options);

gboolean                gtk_json_parser_get_boolean             (GtkJsonParser          *self);
double                  gtk_json_parser_get_number              (GtkJsonParser          *self);
int                     gtk_json_parser_get_int                 (GtkJsonParser          *self);
guint                   gtk_json_parser_get_uint                (GtkJsonParser          *self);
char *                  gtk_json_parser_get_string              (GtkJsonParser          *self);

gboolean                gtk_json_parser_start_object            (GtkJsonParser          *self);
gboolean                gtk_json_parser_start_array             (GtkJsonParser          *self);
gboolean                gtk_json_parser_end                     (GtkJsonParser          *self);


G_END_DECLS

#endif /* __GTK_JSON_PARSER_H__ */
