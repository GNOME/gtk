/*
 * Copyright © 2019 Benjamin Otte
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


#ifndef __GTK_CSS_PARSER_H__
#define __GTK_CSS_PARSER_H__

#include "gtkcsstokenizerprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssParser GtkCssParser;

typedef void            (* GtkCssParserErrorFunc)               (GtkCssParser                   *parser,
                                                                 const GtkCssLocation           *location,
                                                                 const GtkCssToken              *token,
                                                                 const GError                   *error,
                                                                 gpointer                        user_data);

GtkCssParser *          gtk_css_parser_new                      (GtkCssParserErrorFunc           error_func,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GtkCssParser *          gtk_css_parser_ref                      (GtkCssParser                   *self);
void                    gtk_css_parser_unref                    (GtkCssParser                   *self);

void                    gtk_css_parser_add_tokenizer            (GtkCssParser                   *self,
                                                                 GtkCssTokenizer                *tokenizer);
void                    gtk_css_parser_add_bytes                (GtkCssParser                   *self,
                                                                 GBytes                         *bytes);

const GtkCssToken *     gtk_css_parser_peek_token               (GtkCssParser                   *self);
const GtkCssToken *     gtk_css_parser_get_token                (GtkCssParser                   *self);
void                    gtk_css_parser_consume_token            (GtkCssParser                   *self);
void                    gtk_css_parser_start_block              (GtkCssParser                   *self); 
void                    gtk_css_parser_end_block                (GtkCssParser                   *self); 
void                    gtk_css_parser_skip                     (GtkCssParser                   *self);
void                    gtk_css_parser_skip_until               (GtkCssParser                   *self,
                                                                 GtkCssTokenType                 token_type);

void                    gtk_css_parser_emit_error               (GtkCssParser                   *self,
                                                                 const GError                   *error);
void                    gtk_css_parser_error_syntax             (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_parser_error_value              (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_parser_warn_syntax              (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);


gboolean                gtk_css_parser_consume_if               (GtkCssParser                   *self,
                                                                 GtkCssTokenType                 token_type);
gboolean                gtk_css_parser_consume_number           (GtkCssParser                   *self,
                                                                 double                         *number);
gboolean                gtk_css_parser_consume_percentage       (GtkCssParser                   *self,
                                                                 double                         *number);
gboolean                gtk_css_parser_consume_function         (GtkCssParser                   *self,
                                                                 guint                           min_args,
                                                                 guint                           max_args,
                                                                 guint (* parse_func) (GtkCssParser *, guint, gpointer),
                                                                 gpointer                        data);

G_END_DECLS

#endif /* __GTK_CSS_PARSER_H__ */
