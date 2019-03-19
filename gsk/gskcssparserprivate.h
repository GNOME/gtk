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


#ifndef __GSK_CSS_PARSER_H__
#define __GSK_CSS_PARSER_H__

#include "gskcsstokenizerprivate.h"

G_BEGIN_DECLS

typedef struct _GskCssParser GskCssParser;

typedef void            (* GskCssParserErrorFunc)               (GskCssParser                   *parser,
                                                                 const GskCssLocation           *location,
                                                                 const GskCssToken              *token,
                                                                 const GError                   *error,
                                                                 gpointer                        user_data);

GskCssParser *          gsk_css_parser_new                      (GskCssParserErrorFunc           error_func,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GskCssParser *          gsk_css_parser_ref                      (GskCssParser                   *self);
void                    gsk_css_parser_unref                    (GskCssParser                   *self);

void                    gsk_css_parser_add_tokenizer            (GskCssParser                   *self,
                                                                 GskCssTokenizer                *tokenizer);
void                    gsk_css_parser_add_bytes                (GskCssParser                   *self,
                                                                 GBytes                         *bytes);

const GskCssToken *     gsk_css_parser_peek_token               (GskCssParser                   *self);
const GskCssToken *     gsk_css_parser_get_token                (GskCssParser                   *self);
void                    gsk_css_parser_consume_token            (GskCssParser                   *self);
void                    gsk_css_parser_start_block              (GskCssParser                   *self); 
void                    gsk_css_parser_end_block                (GskCssParser                   *self); 
void                    gsk_css_parser_skip                     (GskCssParser                   *self);
void                    gsk_css_parser_skip_until               (GskCssParser                   *self,
                                                                 GskCssTokenType                 token_type);

void                    gsk_css_parser_emit_error               (GskCssParser                   *self,
                                                                 const GError                   *error);
void                    gsk_css_parser_error_syntax             (GskCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gsk_css_parser_error_value              (GskCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gsk_css_parser_warn_syntax              (GskCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);


gboolean                gsk_css_parser_consume_if               (GskCssParser                   *self,
                                                                 GskCssTokenType                 token_type);
gboolean                gsk_css_parser_consume_number           (GskCssParser                   *self,
                                                                 double                         *number);
gboolean                gsk_css_parser_consume_percentage       (GskCssParser                   *self,
                                                                 double                         *number);
gboolean                gsk_css_parser_consume_function         (GskCssParser                   *self,
                                                                 guint                           min_args,
                                                                 guint                           max_args,
                                                                 guint (* parse_func) (GskCssParser *, guint, gpointer),
                                                                 gpointer                        data);

G_END_DECLS

#endif /* __GSK_CSS_PARSER_H__ */
