
/*
 * Copyright © 2020 Red Hat, Inc
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __OTTIE_PRINTER_PRIVATE_H__
#define __OTTIE_PRINTER_PRIVATE_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

typedef struct
{
  GString *str;
  int indent_level;
  int has_member;
} OttiePrinter;

void ottie_printer_init               (OttiePrinter             *printer);
void ottie_printer_indent             (OttiePrinter             *printer);
void ottie_printer_start_object       (OttiePrinter             *printer,
                                       const char               *name);
void ottie_printer_end_object         (OttiePrinter             *printer);
void ottie_printer_start_array        (OttiePrinter             *printer,
                                       const char               *name);
void ottie_printer_end_array          (OttiePrinter             *printer);
void ottie_printer_add_double         (OttiePrinter             *printer,
                                       const char               *name,
                                       double                    value);
void ottie_printer_add_int            (OttiePrinter             *printer,
                                       const char               *name,
                                       int                       value);
void ottie_printer_add_boolean        (OttiePrinter             *printer,
                                       const char               *name,
                                       gboolean                  value);
void ottie_printer_add_string         (OttiePrinter             *printer,
                                       const char               *name,
                                       const char               *value);
void ottie_printer_add_color          (OttiePrinter             *printer,
                                       const char               *name,
                                       const GdkRGBA            *value);
void ottie_printer_add_point          (OttiePrinter             *printer,
                                       const char               *name,
                                       const graphene_point_t   *value);
void ottie_printer_add_point3d        (OttiePrinter             *printer,
                                       const char               *name,
                                       const graphene_point3d_t *value);
void ottie_printer_add_path           (OttiePrinter             *printer,
                                       const char               *name,
                                       gpointer                  value);

G_END_DECLS

#endif /* __OTTIE_PRINTER_PRIVATE_H__ */
