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

#ifndef __GSK_SL_PRINTER_PRIVATE_H__
#define __GSK_SL_PRINTER_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlPrinter *          gsk_sl_printer_new                      (void);

GskSlPrinter *          gsk_sl_printer_ref                      (GskSlPrinter          *printer);
void                    gsk_sl_printer_unref                    (GskSlPrinter          *printer);

char *                  gsk_sl_printer_write_to_string          (GskSlPrinter          *printer);

void                    gsk_sl_printer_push_indentation         (GskSlPrinter          *printer);
void                    gsk_sl_printer_pop_indentation          (GskSlPrinter          *printer);

void                    gsk_sl_printer_append                   (GskSlPrinter          *printer,
                                                                 const char            *str);
void                    gsk_sl_printer_append_c                 (GskSlPrinter          *printer,
                                                                 char                   c);
void                    gsk_sl_printer_append_int               (GskSlPrinter          *printer,
                                                                 int                    i);
void                    gsk_sl_printer_append_uint              (GskSlPrinter          *printer,
                                                                 guint                  u);
void                    gsk_sl_printer_append_double            (GskSlPrinter          *printer,
                                                                 double                 d,
                                                                 gboolean               with_dot);
void                    gsk_sl_printer_newline                  (GskSlPrinter          *printer);

G_END_DECLS

#endif /* __GSK_SL_PRINTER_PRIVATE_H__ */
