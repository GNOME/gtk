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

#include "gtk/gtkcsstypesprivate.h"
#include <gtk/gtkcssprovider.h>

G_BEGIN_DECLS

typedef struct _GtkCssParser GtkCssParser;

typedef void (* GtkCssParserErrorFunc) (GtkCssParser *parser,
                                        const GError *error,
                                        gpointer      user_data);

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
void            _gtk_css_parser_error_full        (GtkCssParser          *parser,
                                                   GtkCssProviderError    code,
                                                   const char            *format,
                                                    ...) G_GNUC_PRINTF (3, 4);

guint           _gtk_css_parser_get_line          (GtkCssParser          *parser);
guint           _gtk_css_parser_get_position      (GtkCssParser          *parser);
GFile *         _gtk_css_parser_get_file          (GtkCssParser          *parser);
GFile *         _gtk_css_parser_get_file_for_path (GtkCssParser          *parser,
                                                   const char            *path);

gboolean        _gtk_css_parser_is_eof            (GtkCssParser          *parser);
gboolean        _gtk_css_parser_begins_with       (GtkCssParser          *parser,
                                                   char                   c);
gboolean        _gtk_css_parser_has_prefix        (GtkCssParser          *parser,
                                                   const char            *prefix);
gboolean        _gtk_css_parser_is_string         (GtkCssParser          *parser);

/* IMPORTANT:
 * _try_foo() functions do not modify the data pointer if they fail, nor do they
 * signal an error. _read_foo() will modify the data pointer and position it at
 * the first token that is broken and emit an error about the failure.
 * So only call _read_foo() when you know that you are reading a foo. _try_foo()
 * however is fine to call if you don’t know yet if the token is a foo or a bar,
 * you can _try_bar() if try_foo() failed.
 */
gboolean        _gtk_css_parser_try               (GtkCssParser          *parser,
                                                   const char            *string,
                                                   gboolean               skip_whitespace);
char *          _gtk_css_parser_try_ident         (GtkCssParser          *parser,
                                                   gboolean               skip_whitespace);
char *          _gtk_css_parser_try_name          (GtkCssParser          *parser,
                                                   gboolean               skip_whitespace);
gboolean        _gtk_css_parser_try_int           (GtkCssParser          *parser,
                                                   int                   *value);
gboolean        _gtk_css_parser_try_uint          (GtkCssParser          *parser,
                                                   guint                 *value);
gboolean        _gtk_css_parser_try_double        (GtkCssParser          *parser,
                                                   gdouble               *value);
gboolean        _gtk_css_parser_try_length        (GtkCssParser          *parser,
                                                   int                   *value);
gboolean        _gtk_css_parser_try_enum          (GtkCssParser          *parser,
                                                   GType                  enum_type,
                                                   int                   *value);
gboolean        _gtk_css_parser_try_hash_color    (GtkCssParser          *parser,
                                                   GdkRGBA               *rgba);

gboolean        _gtk_css_parser_has_number        (GtkCssParser          *parser);
char *          _gtk_css_parser_read_string       (GtkCssParser          *parser);
char *          _gtk_css_parser_read_value        (GtkCssParser          *parser);
GFile *         _gtk_css_parser_read_url          (GtkCssParser          *parser);

void            _gtk_css_parser_skip_whitespace   (GtkCssParser          *parser);
void            _gtk_css_parser_resync            (GtkCssParser          *parser,
                                                   gboolean               sync_at_semicolon,
                                                   char                   terminator);

/* XXX: Find better place to put it? */
void            _gtk_css_print_string             (GString               *str,
                                                   const char            *string);


G_END_DECLS

#endif /* __GTK_CSS_PARSER_PRIVATE_H__ */
