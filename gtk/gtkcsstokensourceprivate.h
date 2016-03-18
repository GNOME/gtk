/*
 * Copyright © 2016 Red Hat Inc.
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

#ifndef __GTK_CSS_TOKEN_SOURCE_PRIVATE_H__
#define __GTK_CSS_TOKEN_SOURCE_PRIVATE_H__

#include <glib-object.h>
#include "gtk/gtkcsstokenizerprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssTokenSource GtkCssTokenSource;
typedef struct _GtkCssTokenSourceClass GtkCssTokenSourceClass;

struct _GtkCssTokenSource
{
  const GtkCssTokenSourceClass *klass;
  gint ref_count;
  GObject *consumer;
  GSList *blocks; /* of GPOINTER_TO_UINT(GtkCssTokenType) */
};

struct _GtkCssTokenSourceClass
{
  void                  (* finalize)                            (GtkCssTokenSource      *source);
  void                  (* consume_token)                       (GtkCssTokenSource      *source,
                                                                 GObject                *consumer);
  const GtkCssToken *   (* peek_token)                          (GtkCssTokenSource      *source);
  void                  (* error)                               (GtkCssTokenSource      *source,
                                                                 const GError           *error);
};

GtkCssTokenSource *     gtk_css_token_source_new_for_tokenizer  (GtkCssTokenizer        *tokenizer);
GtkCssTokenSource *     gtk_css_token_source_new_for_part       (GtkCssTokenSource      *source,
                                                                 GtkCssTokenType         end_type);

GtkCssTokenSource *     gtk_css_token_source_ref                (GtkCssTokenSource      *source);
void                    gtk_css_token_source_unref              (GtkCssTokenSource      *source);

#define gtk_css_token_source_new(type,klass) ((type *) gtk_css_token_source_alloc (sizeof (type), (klass)))
GtkCssTokenSource *     gtk_css_token_source_alloc              (gsize                   struct_size,
                                                                 const GtkCssTokenSourceClass *klass);

void                    gtk_css_token_source_consume_token      (GtkCssTokenSource      *source);
void                    gtk_css_token_source_consume_token_as   (GtkCssTokenSource      *source,
                                                                 GObject                *consumer);
const GtkCssToken *     gtk_css_token_source_peek_token         (GtkCssTokenSource      *source);
const GtkCssToken *     gtk_css_token_source_get_token          (GtkCssTokenSource      *source);

GtkCssTokenType         gtk_css_token_get_pending_block         (GtkCssTokenSource      *source);

void                    gtk_css_token_source_consume_all        (GtkCssTokenSource      *source);
char *                  gtk_css_token_source_consume_to_string  (GtkCssTokenSource      *source);

gboolean                gtk_css_token_source_consume_function   (GtkCssTokenSource      *source,
                                                                 guint                   min_args,
                                                                 guint                   max_args,
                                                                 gboolean (* parse_func) (GtkCssTokenSource *, guint, gpointer),
                                                                 gpointer                data);
gboolean                gtk_css_token_source_consume_number     (GtkCssTokenSource      *source,
                                                                 double                 *number);

void                    gtk_css_token_source_emit_error         (GtkCssTokenSource      *source,
                                                                 const GError           *error);
void                    gtk_css_token_source_error              (GtkCssTokenSource      *source,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_token_source_unknown            (GtkCssTokenSource      *source,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_token_source_deprecated         (GtkCssTokenSource      *source,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);

GObject *               gtk_css_token_source_get_consumer       (GtkCssTokenSource      *source);
void                    gtk_css_token_source_set_consumer       (GtkCssTokenSource      *source,
                                                                 GObject                *consumer);


G_END_DECLS

#endif /* __GTK_CSS_TOKEN_SOURCE_PRIVATE_H__ */
