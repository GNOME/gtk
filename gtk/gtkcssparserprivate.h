/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CSS_PARSER_PRIVATE_H__
#define __GTK_CSS_PARSER_PRIVATE_H__

#include <gtk/gtkcssprovider.h>

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssParser GtkCssParser;

typedef void (* GtkCssParserErrorFunc) (GtkCssParser *parser,
                                        const GError *error,
                                        gpointer      user_data);

typedef struct _GtkCssParseOption GtkCssParseOption;

struct _GtkCssParseOption
{
  gboolean (* can_parse)  (GtkCssParser *parser,
                           gpointer      option_data,
                           gpointer      user_data);
  gboolean (* parse)      (GtkCssParser *parser,
                           gpointer      option_data,
                           gpointer      user_data);
  gpointer data;
};

GtkCssParser *  _gtk_css_parser_new               (const char            *data,
                                                   GFile                 *file,
                                                   GtkCssParserErrorFunc  error_func,
                                                   gpointer               user_data);
void            _gtk_css_parser_free              (GtkCssParser          *parser);

void            _gtk_css_parser_take_error        (GtkCssParser          *parser,
                                                   GError                *error);
void            _gtk_css_parser_error             (GtkCssParser          *parser,
                                                   const char            *format,
                                                    ...) G_GNUC_PRINTF (2, 3);

guint           _gtk_css_parser_get_line          (GtkCssParser          *parser);
guint           _gtk_css_parser_get_position      (GtkCssParser          *parser);
GFile *         _gtk_css_parser_get_file          (GtkCssParser          *parser);
GFile *         gtk_css_parser_resolve_url        (GtkCssParser          *parser,
                                                   const char            *url);

gboolean        gtk_css_parser_has_token          (GtkCssParser          *parser,
                                                   GtkCssTokenType        token_type);
gboolean        gtk_css_parser_has_ident          (GtkCssParser          *parser,
                                                   const char            *name);
gboolean        gtk_css_parser_has_integer        (GtkCssParser          *parser);
gboolean        gtk_css_parser_has_function       (GtkCssParser          *parser,
                                                   const char            *name);

/* IMPORTANT:
 * _try_foo() functions do not modify the data pointer if they fail, nor do they
 * signal an error. _read_foo() will modify the data pointer and position it at
 * the first token that is broken and emit an error about the failure.
 * So only call _read_foo() when you know that you are reading a foo. _try_foo()
 * however is fine to call if you don’t know yet if the token is a foo or a bar,
 * you can _try_bar() if try_foo() failed.
 */
gboolean        gtk_css_parser_try_ident          (GtkCssParser          *parser,
                                                   const char            *ident);
gboolean        gtk_css_parser_try_delim          (GtkCssParser          *parser,
                                                   gunichar               delim);
gboolean        gtk_css_parser_try_at_keyword     (GtkCssParser          *parser,
                                                   const char            *keyword);
gboolean        gtk_css_parser_try_token          (GtkCssParser          *parser,
                                                   GtkCssTokenType        token_type);
gboolean        _gtk_css_parser_try               (GtkCssParser          *parser,
                                                   const char            *string,
                                                   gboolean               skip_whitespace);
char *          _gtk_css_parser_try_ident         (GtkCssParser          *parser,
                                                   gboolean               skip_whitespace);
char *          _gtk_css_parser_try_name          (GtkCssParser          *parser,
                                                   gboolean               skip_whitespace);
gboolean        _gtk_css_parser_try_hash_color    (GtkCssParser          *parser,
                                                   GdkRGBA               *rgba);

char *          gtk_css_parser_consume_ident      (GtkCssParser          *self);
char *          gtk_css_parser_consume_string     (GtkCssParser          *self);
GFile *         gtk_css_parser_consume_url        (GtkCssParser          *self);
gboolean        gtk_css_parser_consume_number     (GtkCssParser          *self,
                                                   double                *number);
gboolean        gtk_css_parser_consume_integer    (GtkCssParser          *parser,
                                                   int                   *value);
gboolean        gtk_css_parser_consume_function   (GtkCssParser          *self,
                                                   guint                  min_args,
                                                   guint                  max_args,
                                                   guint (* parse_func) (GtkCssParser *, guint, gpointer),
                                                   gpointer               data);
gsize           gtk_css_parser_consume_any        (GtkCssParser          *parser,
                                                   const GtkCssParseOption *options,
                                                   gsize                  n_options,
                                                   gpointer               user_data);

gboolean        _gtk_css_parser_has_number        (GtkCssParser          *parser);

void            _gtk_css_parser_skip_whitespace   (GtkCssParser          *parser);
void            _gtk_css_parser_resync            (GtkCssParser          *parser,
                                                   gboolean               sync_at_semicolon,
                                                   char                   terminator);

/* XXX: Find better place to put it? */
void            _gtk_css_print_string             (GString               *str,
                                                   const char            *string);


G_END_DECLS

#endif /* __GTK_CSS_PARSER_PRIVATE_H__ */
