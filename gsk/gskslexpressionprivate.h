/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#ifndef __GSK_SL_EXPRESSION_PRIVATE_H__
#define __GSK_SL_EXPRESSION_PRIVATE_H__

#include "gsk/gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlExpression *       gsk_sl_expression_parse                 (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *stream);
GskSlExpression *       gsk_sl_expression_parse_assignment      (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *stream);
GskSlExpression *       gsk_sl_expression_parse_constant        (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *stream);
GskSlExpression *       gsk_sl_expression_parse_function_call   (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *stream,
                                                                 GskSlFunctionMatcher   *matcher,
                                                                 GskSlFunction          *function);

GskSlExpression *       gsk_sl_expression_ref                   (GskSlExpression        *expression);
void                    gsk_sl_expression_unref                 (GskSlExpression        *expression);

void                    gsk_sl_expression_print                 (const GskSlExpression  *expression,
                                                                 GskSlPrinter           *printer);
GskSlType *             gsk_sl_expression_get_return_type       (const GskSlExpression  *expression);
GskSlValue *            gsk_sl_expression_get_constant          (const GskSlExpression  *expression);

guint32                 gsk_sl_expression_write_spv             (const GskSlExpression  *expression,
                                                                 GskSpvWriter           *writer);

G_END_DECLS

#endif /* __GSK_SL_EXPRESSION_PRIVATE_H__ */
