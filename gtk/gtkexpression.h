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


#ifndef __GTK_EXPRESSION_H__
#define __GTK_EXPRESSION_H__

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

typedef struct _GtkExpression GtkExpression;

#define GTK_IS_EXPRESSION(expr) ((expr) != NULL)

#define GTK_TYPE_EXPRESSION (gtk_expression_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gtk_expression_get_type                 (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_expression_ref                      (GtkExpression                  *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_expression_unref                    (GtkExpression                  *self);

GDK_AVAILABLE_IN_ALL
GType                   gtk_expression_get_value_type           (GtkExpression                  *self);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_expression_evaluate                 (GtkExpression                  *self,
                                                                 gpointer                        this_,
                                                                 GValue                         *value);

GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_property_expression_new             (GType                           this_type,
                                                                 const char                     *property_name);
GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_constant_expression_new             (GType                           value_type,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_constant_expression_new_for_value   (const GValue                   *value);                           
GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_object_expression_new               (GObject                        *object);
GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_closure_expression_new              (GType                           value_type,
                                                                 GClosure                       *closure,
                                                                 guint                           n_params,
                                                                 GtkExpression                 **params);
GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_cclosure_expression_new             (GType                           value_type,
                                                                 GClosureMarshal                 marshal,
                                                                 guint                           n_params,
                                                                 GtkExpression                 **params,
                                                                 GCallback                       callback_func,
                                                                 gpointer                        user_data,
                                                                 GClosureNotify                  user_destroy);

G_END_DECLS

#endif /* __GTK_EXPRESSION_H__ */
