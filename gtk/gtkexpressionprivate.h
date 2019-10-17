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


#ifndef __GTK_EXPRESSION_PRIVATE_H__
#define __GTK_EXPRESSION_PRIVATE_H__

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

typedef struct _GtkExpression GtkExpression;

#define GTK_IS_EXPRESSION(expr) ((expr) != NULL)

GtkExpression *         gtk_expression_new_assign               (GObject                        *object,
                                                                 const char                     *property,
                                                                 GtkExpression                  *expr);

GtkExpression *         gtk_expression_ref                      (GtkExpression                  *self);
void                    gtk_expression_unref                    (GtkExpression                  *self);

void                    gtk_expression_print                    (GtkExpression                  *self,
                                                                 GString                        *string);
char *                  gtk_expression_to_string                (GtkExpression                  *self);
GtkExpression *         gtk_expression_parse                    (GtkBuilder                     *scope,
                                                                 const char                     *string,
                                                                 GError                        **error);

GType                   gtk_expression_get_value_type           (GtkExpression                  *self);
gboolean                gtk_expression_evaluate                 (GtkExpression                  *self,
                                                                 GValue                         *value);

char *                  gtk_expression_value_to_string          (const GValue                   *value);
gboolean                gtk_expression_value_to_boolean         (const GValue                   *value);
G_END_DECLS

#endif /* __GTK_EXPRESSION_PRIVATE_H__ */
