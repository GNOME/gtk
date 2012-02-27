/* GTK - The GIMP Toolkit
 * gtkprintoperation.h: Print Operation
 * Copyright (C) 2006, Red Hat, Inc.
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

#ifndef __GTK_PRINTER_PRIVATE_H__
#define __GTK_PRINTER_PRIVATE_H__

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>
#include "gtkprinteroptionset.h"

G_BEGIN_DECLS

GtkPrinterOptionSet *_gtk_printer_get_options               (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup,
							     GtkPrintCapabilities capabilities);
gboolean             _gtk_printer_mark_conflicts            (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options);
void                 _gtk_printer_get_settings_from_options (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options,
							     GtkPrintSettings    *settings);
void                 _gtk_printer_prepare_for_print         (GtkPrinter          *printer,
							     GtkPrintJob         *print_job,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup);
cairo_surface_t *    _gtk_printer_create_cairo_surface      (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     gdouble              width,
							     gdouble              height,
							     GIOChannel          *cache_io);
GHashTable *         _gtk_printer_get_custom_widgets        (GtkPrinter          *printer);

/* GtkPrintJob private methods: */
void gtk_print_job_set_status (GtkPrintJob   *job,
			       GtkPrintStatus status);

G_END_DECLS
#endif /* __GTK_PRINT_OPERATION_PRIVATE_H__ */
