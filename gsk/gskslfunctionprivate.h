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

#ifndef __GSK_SL_FUNCTION_PRIVATE_H__
#define __GSK_SL_FUNCTION_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskSlFunctionClass GskSlFunctionClass;

struct _GskSlFunction
{
  const GskSlFunctionClass *class;

  int ref_count;
};

struct _GskSlFunctionClass {
  void                  (* free)                                (GskSlFunction  *function);

  GskSlType *           (* get_return_type)                     (GskSlFunction  *function);
  void                  (* print_name)                          (GskSlFunction  *function,
                                                                 GString        *string);
  gboolean              (* matches)                             (GskSlFunction  *function,
                                                                 GskSlType     **arguments,
                                                                 gsize           n_arguments,
                                                                 GError        **error);
};

GskSlFunction *         gsk_sl_function_new_constructor         (GskSlType      *type);

GskSlFunction *         gsk_sl_function_ref                     (GskSlFunction  *function);
void                    gsk_sl_function_unref                   (GskSlFunction  *function);

void                    gsk_sl_function_print_name              (GskSlFunction  *function,
                                                                 GString        *string);
GskSlType *             gsk_sl_function_get_return_type         (GskSlFunction  *function);
gboolean                gsk_sl_function_matches                 (GskSlFunction  *function,
                                                                 GskSlType     **arguments,
                                                                 gsize           n_arguments,
                                                                 GError        **error);

G_END_DECLS

#endif /* __GSK_SL_FUNCTION_PRIVATE_H__ */
